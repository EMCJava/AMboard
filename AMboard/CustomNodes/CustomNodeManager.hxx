//
// Created by LYS on 2/26/2026.
//

#pragma once

#include <filesystem>
#include <functional>
#include <ranges>
#include <unordered_map>

class CBaseNode;
using CreateExtFunc = CBaseNode* (*)();
using DestroyExtFunc = void (*)(CBaseNode*);

class CCustomNodeHandle {

public:
    CCustomNodeHandle(const std::filesystem::path& Path);
    ~CCustomNodeHandle();

    CCustomNodeHandle(const CCustomNodeHandle&) = delete;
    CCustomNodeHandle& operator=(const CCustomNodeHandle&) = delete;
    CCustomNodeHandle(CCustomNodeHandle&&) noexcept;
    CCustomNodeHandle& operator=(CCustomNodeHandle&&) noexcept;

private:
    std::string m_Name;

    void* m_LibHandle = nullptr;

    std::vector<std::pair<std::string, std::function<std::unique_ptr<CBaseNode, DestroyExtFunc>()>>> m_Allocator;

    friend class CCustomNodeLoader;
};

class CCustomNodeLoader {
public:
    /// @param NodeExtDir Directory to scan for .dll / .so / .dylib files.
    explicit CCustomNodeLoader(const std::filesystem::path& NodeExtDir);

    /// Returns all successfully loaded plugins.
    [[nodiscard]] decltype(auto) GetNodeExts() const noexcept { return std::views::keys(m_NodeExts); }
    [[nodiscard]] std::unique_ptr<CBaseNode, DestroyExtFunc> CreateNodeExt(auto&& Str) const noexcept
    {
        if (const auto It = m_NodeExts.find(Str); It != m_NodeExts.end()) {
            return It->second();
        }

        return { nullptr, nullptr };
    }

private:
    std::vector<std::pair<std::string, CCustomNodeHandle>> m_Handles;
    std::unordered_map<std::string, std::function<std::unique_ptr<CBaseNode, DestroyExtFunc>()>> m_NodeExts;

};