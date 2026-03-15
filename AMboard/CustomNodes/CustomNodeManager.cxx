//
// Created by LYS on 2/26/2026.
//

#include "CustomNodeManager.hxx"

#include <spdlog/spdlog.h>

#include <algorithm>

// ─── Platform-specific dynamic library helpers ──────────────────────────────
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

static void* lib_open(const char* path)
{
    return static_cast<void*>(LoadLibraryA(path));
}
static void* lib_sym(void* handle, const char* sym)
{
    return reinterpret_cast<void*>(
        GetProcAddress(static_cast<HMODULE>(handle), sym));
}
static void lib_close(void* handle)
{
    FreeLibrary(static_cast<HMODULE>(handle));
}
static std::string lib_error()
{
    DWORD err = GetLastError();
    char buf[256];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, err, 0, buf, sizeof(buf), nullptr);
    return std::string(buf);
}

static bool is_shared_lib(const std::filesystem::path& p)
{
    return p.extension() == ".dll";
}
#else
#include <dlfcn.h>

static void* lib_open(const char* path)
{
    return dlopen(path, RTLD_NOW);
}
static void* lib_sym(void* handle, const char* sym)
{
    return dlsym(handle, sym);
}
static void lib_close(void* handle)
{
    dlclose(handle);
}
static std::string lib_error()
{
    const char* e = dlerror();
    return e ? e : "unknown error";
}

static bool is_shared_lib(const std::filesystem::path& p)
{
    auto ext = p.extension();
    return ext == ".so" || ext == ".dylib";
}
#endif

// ─── CCustomNodeHandle ───────────────────────────────────────────────────────────

CCustomNodeHandle::CCustomNodeHandle(const std::filesystem::path& Path, void* ImGuiCtx)
{
    spdlog::info("[CCustomNodeHandle] Loading: {}", Path.string());

    m_LibHandle = lib_open(absolute(Path).string().c_str());
    if (!m_LibHandle) {
        throw std::runtime_error(
            "Failed to load " + Path.string() + ": " + lib_error());
    }

    auto NamesFunc = reinterpret_cast<const char** (*)()>(lib_sym(m_LibHandle, "get_macro_names"));
    if (!NamesFunc) {
        lib_close(m_LibHandle);
        m_LibHandle = nullptr;
        throw std::runtime_error(
            "Plugin " + Path.string() + " missing get_macro_names");
    }

    if (const auto ImGuiCtxFunc = reinterpret_cast<void (*)(void*)>(lib_sym(m_LibHandle, "set_imgui_context")))
        ImGuiCtxFunc(ImGuiCtx);

    for (const char** name = NamesFunc(); *name; ++name) {
        std::string createSym = std::string("create_") + *name;
        std::string destroySym = std::string("destroy_") + *name;

        const auto create = reinterpret_cast<CreateExtFunc>(lib_sym(m_LibHandle, createSym.c_str()));
        const auto destroy = reinterpret_cast<DestroyExtFunc>(lib_sym(m_LibHandle, destroySym.c_str()));

        if (!create || !destroy) {
            spdlog::error("Missing: {}/{}", createSym, destroySym);
            continue;
        }

        m_Allocator.emplace_back(
            std::ranges::fold_left(
                std::string_view(*name)
                    | std::views::chunk_by([](char, const char b) { return !std::isupper(b); })
                    | std::views::drop_while([](auto chunk) {
                          return std::ranges::distance(chunk) == 1 && *chunk.begin() == 'C';
                      }),
                std::string { }, // Initial empty string
                [](std::string acc, auto chunk) {
                    const std::string_view sv(chunk.begin(), chunk.end());
                    return acc.empty() ? std::string(sv) : std::move(acc) + " " + std::string(sv);
                }),
            [=] {
                return std::unique_ptr<CBaseNode, DestroyExtFunc>(create(), destroy);
            });

        spdlog::info("Loaded: {}", *name);
    }

    m_Name = Path.stem().string();
    spdlog::info("[CCustomNodeLoader] Successfully loaded plugin: {}", m_Name);
}

CCustomNodeHandle::~CCustomNodeHandle()
{
    if (m_LibHandle) {
        spdlog::info("[CCustomNodeLoader] Destroying plugin: {}", m_Name);

        lib_close(m_LibHandle);
        m_LibHandle = nullptr;
    }
}

CCustomNodeHandle::CCustomNodeHandle(CCustomNodeHandle&& other) noexcept
    : m_Name(std::move(other.m_Name))
    , m_LibHandle(other.m_LibHandle)
    , m_Allocator(std::move(other.m_Allocator))
{
    other.m_LibHandle = nullptr;
}

CCustomNodeHandle& CCustomNodeHandle::operator=(CCustomNodeHandle&& other) noexcept
{
    if (this != &other) {
        // Clean up current
        if (m_LibHandle)
            lib_close(m_LibHandle);

        m_LibHandle = other.m_LibHandle;
        m_Allocator = std::move(other.m_Allocator);

        other.m_LibHandle = nullptr;
    }
    return *this;
}

// ─── CCustomNodeLoader ───────────────────────────────────────────────────────────

CCustomNodeLoader::CCustomNodeLoader(const std::filesystem::path& NodeExtDir, void* ImGuiCtx)
{
    if (!std::filesystem::exists(NodeExtDir)) {
        spdlog::info("[CCustomNodeLoader] Node ext directory does not exist: {}", NodeExtDir.string());

        return;
    }

    spdlog::info("[CCustomNodeLoader] Scanning for nodes in: {}", NodeExtDir.string());

#ifdef _WIN32
    SetDllDirectoryA(NodeExtDir.string().c_str());
#endif

    for (const auto& entry : std::filesystem::directory_iterator(NodeExtDir)) {
        if (!entry.is_regular_file())
            continue;
        if (!is_shared_lib(entry.path()))
            continue;
        if (!std::wstring_view { entry.path().stem().c_str() }.starts_with(L"Ext"))
            continue;

        try {
            m_Handles.emplace_back(entry.path().stem().string(), CCustomNodeHandle { entry.path(), ImGuiCtx });
            for (const auto& [Name, Allocator] : m_Handles.back().second.m_Allocator) {
                if (!m_NodeExts.insert_or_assign(Name, Allocator).second) {
                    spdlog::warn("[CCustomNodeLoader] Overwriting node: {}", Name);
                }
            }
        } catch (const std::exception& ex) {
            spdlog::error("[CCustomNodeLoader] Error: {}", ex.what());
        }
    }

    spdlog::info("[CCustomNodeLoader] Loaded: {} plugin(s) {} node(s)", m_Handles.size(), m_NodeExts.size());
}