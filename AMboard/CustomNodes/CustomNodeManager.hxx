//
// Created by LYS on 2/26/2026.
//

#pragma once

#include <filesystem>
#include <ranges>
#include <unordered_map>

class CBaseNode;
class CCustomNodeHandle {

    using CreateFunc = CBaseNode* (*)();
    using DestroyFunc = void (*)(CBaseNode*);

public:
    CCustomNodeHandle(const std::filesystem::path& Path);
    ~CCustomNodeHandle();

    CCustomNodeHandle(const CCustomNodeHandle&) = delete;
    CCustomNodeHandle& operator=(const CCustomNodeHandle&) = delete;
    CCustomNodeHandle(CCustomNodeHandle&&) noexcept;
    CCustomNodeHandle& operator=(CCustomNodeHandle&&) noexcept;

    std::unique_ptr<CBaseNode, DestroyFunc> Create() const noexcept;

private:
    std::string m_Name;

    void* m_LibHandle = nullptr;

    CreateFunc m_CreateFunc = nullptr;
    DestroyFunc m_DestroyFunc = nullptr;
};

class CCustomNodeLoader {
public:
    /// @param NodeExtDir Directory to scan for .dll / .so / .dylib files.
    explicit CCustomNodeLoader(const std::filesystem::path& NodeExtDir);

    /// Returns all successfully loaded plugins.
    decltype(auto) GetNodeExts() const noexcept { return std::views::keys(m_NodeExts); }
    auto CreateNodeExt(auto&& Str) const noexcept -> decltype(static_cast<CCustomNodeHandle*>(nullptr)->Create())
    {
        if (const auto It = m_NodeExts.find(Str); It != m_NodeExts.end()) {
            return It->second.Create();
        }

        return { nullptr, nullptr };
    }

private:
    std::unordered_map<std::string, CCustomNodeHandle> m_NodeExts;
};