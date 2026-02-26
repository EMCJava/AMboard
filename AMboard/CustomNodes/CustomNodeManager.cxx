//
// Created by LYS on 2/26/2026.
//

#include "CustomNodeManager.hxx"

#include <spdlog/spdlog.h>

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

CCustomNodeHandle::CCustomNodeHandle(const std::filesystem::path& Path)
{
    spdlog::info("[CCustomNodeHandle] Loading: {}", Path.string());

    m_LibHandle = lib_open(absolute(Path).string().c_str());
    if (!m_LibHandle) {
        throw std::runtime_error(
            "Failed to load " + Path.string() + ": " + lib_error());
    }

    m_CreateFunc = reinterpret_cast<CreateFunc>(lib_sym(m_LibHandle, "create_node_ext"));
    m_DestroyFunc = reinterpret_cast<DestroyFunc>(lib_sym(m_LibHandle, "destroy_node_ext"));

    if (!m_CreateFunc || !m_DestroyFunc) {
        lib_close(m_LibHandle);
        m_LibHandle = nullptr;
        throw std::runtime_error(
            "Plugin " + Path.string() + " missing create_node_ext/destroy_node_ext");
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
    , m_CreateFunc(other.m_CreateFunc)
    , m_DestroyFunc(other.m_DestroyFunc)
{
    other.m_LibHandle = nullptr;
    other.m_CreateFunc = nullptr;
    other.m_DestroyFunc = nullptr;
}

CCustomNodeHandle& CCustomNodeHandle::operator=(CCustomNodeHandle&& other) noexcept
{
    if (this != &other) {
        // Clean up current
        if (m_LibHandle)
            lib_close(m_LibHandle);

        m_LibHandle = other.m_LibHandle;
        m_CreateFunc = other.m_CreateFunc;
        m_DestroyFunc = other.m_DestroyFunc;

        other.m_LibHandle = nullptr;
        other.m_CreateFunc = nullptr;
        other.m_DestroyFunc = nullptr;
    }
    return *this;
}

std::unique_ptr<CBaseNode, CCustomNodeHandle::DestroyFunc> CCustomNodeHandle::Create() const noexcept
{
    return std::unique_ptr<CBaseNode, DestroyFunc>(m_CreateFunc(), m_DestroyFunc);
}

// ─── CCustomNodeLoader ───────────────────────────────────────────────────────────

CCustomNodeLoader::CCustomNodeLoader(const std::filesystem::path& NodeExtDir)
{
    if (!std::filesystem::exists(NodeExtDir)) {
        spdlog::info("[CCustomNodeLoader] Plugin directory does not exist: {}", NodeExtDir.string());

        return;
    }

    spdlog::info("[CCustomNodeLoader] Scanning for plugins in: {}", NodeExtDir.string());

#ifdef _WIN32
    SetDllDirectoryA(NodeExtDir.string().c_str());
#endif

    for (const auto& entry : std::filesystem::directory_iterator(NodeExtDir)) {
        if (!entry.is_regular_file())
            continue;
        if (!is_shared_lib(entry.path()))
            continue;
        if (entry.path().stem() == "MacroSharedLib")
            continue;

        try {
            m_NodeExts.insert({ entry.path().stem().string(), entry.path() });
        } catch (const std::exception& ex) {
            spdlog::error("[CCustomNodeLoader] Error: {}", ex.what());
        }
    }

    spdlog::info("[CCustomNodeLoader] Loaded: {} plugin(s)", m_NodeExts.size());
}