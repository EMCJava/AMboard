//
// Created by LYS on 3/1/2026.
//

#pragma once

#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct SNodeNameMeta {
    std::string Name;
    std::string Category;
};

enum class ENodeMenuItemType {
    Category,
    Node
};

struct SVisibleMenuItem {
    ENodeMenuItemType Type;
    std::string DisplayName;
    const SNodeNameMeta* NodePtr; // Null if this is a Category
};

class CNodeContextMenu {

    void UpdateFilter();

    static int InputTextCallback(struct ImGuiInputTextCallbackData* data);

    void ExecuteSelectedItem();

public:
    // Allow passing nodes in dynamically
    void Initialize(std::vector<SNodeNameMeta> nodes);

    std::optional<std::string> Draw();

    void OpenPopup();
    auto GetPopupLocation() const noexcept { return m_PopupPos; }

private:
    std::vector<SNodeNameMeta> m_AllNodes;
    std::map<std::string, std::vector<const SNodeNameMeta*>> m_NodesByCategory;

    std::vector<SVisibleMenuItem> m_VisibleItems;
    std::unordered_map<std::string, bool> m_CategoryExpandedState;

    char m_SearchBuffer[256] = "";
    int m_SelectedIndex = 0;
    std::optional<std::string> m_PickedItem;
    bool m_ScrollToSelection = false;
    bool m_ReclaimFocus = false; // Used to keep focus on search bar after clicking a category

    bool m_ShouldOpen = false;
    std::pair<float, float> m_PopupPos;
};