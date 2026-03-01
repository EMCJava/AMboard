//
// Created by LYS on 3/1/2026.
//

#include "NodeContextMenu.hxx"

#include "imgui.h"

#include <algorithm>
#include <bit>

// Helper for case-insensitive search (Safe for all char types)
bool CaseInsensitiveContains(const std::string& str, const std::string& search)
{
    return std::ranges::search(
               str, search,
               [](auto&& ch1, auto&& ch2) static {
                   return std::toupper(ch1) == std::toupper(ch2);
               })
               .begin()
        != str.end();
}

void CNodeContextMenu::UpdateFilter()
{
    m_VisibleItems.clear();
    std::string searchStr(m_SearchBuffer);
    bool isSearching = !searchStr.empty();

    for (const auto& kvp : m_NodesByCategory) {
        const std::string& categoryName = kvp.first;
        const auto& nodes = kvp.second;

        bool categoryMatches = isSearching && CaseInsensitiveContains(categoryName, searchStr);
        std::vector<const SNodeNameMeta*> matchedNodes;

        for (const auto* node : nodes) {
            if (!isSearching || categoryMatches || CaseInsensitiveContains(node->Name, searchStr)) {
                matchedNodes.push_back(node);
            }
        }

        if (!matchedNodes.empty()) {
            // Add Category Header
            m_VisibleItems.push_back({ ENodeMenuItemType::Category, categoryName, nullptr });

            bool isExpanded = isSearching ? true : m_CategoryExpandedState[categoryName];

            // Add Children
            if (isExpanded) {
                for (const auto* node : matchedNodes) {
                    m_VisibleItems.push_back({ ENodeMenuItemType::Node, node->Name, node });
                }
            }
        }
    }

    // Clamp selection
    if (m_VisibleItems.empty()) {
        m_SelectedIndex = 0;
    } else if (m_SelectedIndex >= (int)m_VisibleItems.size()) {
        m_SelectedIndex = (int)m_VisibleItems.size() - 1;
    }
}
int CNodeContextMenu::InputTextCallback(ImGuiInputTextCallbackData* data)
{
    CNodeContextMenu* menu = (CNodeContextMenu*)data->UserData;

    if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
        if (data->EventKey == ImGuiKey_UpArrow) {
            menu->m_SelectedIndex--;
            if (menu->m_SelectedIndex < 0)
                menu->m_SelectedIndex = 0;
            menu->m_ScrollToSelection = true;
        } else if (data->EventKey == ImGuiKey_DownArrow) {
            menu->m_SelectedIndex++;
            if (menu->m_VisibleItems.empty()) {
                menu->m_SelectedIndex = 0;
            } else if (menu->m_SelectedIndex >= (int)menu->m_VisibleItems.size()) {
                menu->m_SelectedIndex = (int)menu->m_VisibleItems.size() - 1;
            }
            menu->m_ScrollToSelection = true;
        }
    }
    return 0;
}

void CNodeContextMenu::ExecuteSelectedItem()
{
    if (m_VisibleItems.empty())
        return;

    const SVisibleMenuItem& selected = m_VisibleItems[m_SelectedIndex];

    if (selected.Type == ENodeMenuItemType::Node) {
        m_PickedItem = selected.NodePtr->Name;
        ImGui::CloseCurrentPopup();
    } else if (selected.Type == ENodeMenuItemType::Category) {
        m_CategoryExpandedState[selected.DisplayName] = !m_CategoryExpandedState[selected.DisplayName];
        UpdateFilter();
        m_ReclaimFocus = true; // Keep user typing after expanding/collapsing
    }
}

void CNodeContextMenu::Initialize(std::vector<SNodeNameMeta> nodes)
{
    m_AllNodes = std::move(nodes);
    m_NodesByCategory.clear();
    m_CategoryExpandedState.clear();

    for (const auto& node : m_AllNodes) {
        m_NodesByCategory[node.Category].push_back(&node);
        m_CategoryExpandedState[node.Category] = false;
    }
}

std::optional<std::string> CNodeContextMenu::Draw()
{
    if (!m_ShouldOpen && !ImGui::IsPopupOpen("NCM", ImGuiPopupFlags_AnyPopupId)) {
        return std::nullopt;
    }

    // We move it way off-screen so it doesn't interfere with anything.
    ImGui::SetNextWindowPos(ImVec2(-10000, -10000));
    ImGui::Begin("##HiddenContextMenuHost", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);

    if (m_ShouldOpen) {
        ImGui::OpenPopup("NCM");
        ImGui::SetNextWindowPos(reinterpret_cast<const ImVec2&>(m_PopupPos), ImGuiCond_Appearing);
        m_SearchBuffer[0] = '\0';
        UpdateFilter();
        m_PickedItem.reset();
        m_ShouldOpen = false;
    }

    if (ImGui::BeginPopup("NCM")) {

        // Focus management
        if (ImGui::IsWindowAppearing() || m_ReclaimFocus) {
            ImGui::SetKeyboardFocusHere();
            m_ReclaimFocus = false;
        }

        ImGui::PushItemWidth(250);
        ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_CallbackHistory;

        if (ImGui::InputText("##Search", m_SearchBuffer, IM_ARRAYSIZE(m_SearchBuffer), inputFlags, InputTextCallback, this)) {
            m_SelectedIndex = 0;
            UpdateFilter();
        }
        ImGui::PopItemWidth();

        // Handle Enter Key
        if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
            ExecuteSelectedItem();
        }

        ImGui::Separator();

        // Node List
        ImGui::BeginChild("NodeList", ImVec2(250, 300), false, ImGuiWindowFlags_HorizontalScrollbar);

        if (m_VisibleItems.empty()) {
            ImGui::TextDisabled("No results found.");
        } else {
            for (int i = 0; i < (int)m_VisibleItems.size(); i++) {
                const SVisibleMenuItem& item = m_VisibleItems[i];
                bool isSelected = (m_SelectedIndex == i);

                if (item.Type == ENodeMenuItemType::Category) {
                    bool isExpanded = (m_SearchBuffer[0] != '\0') ? true : m_CategoryExpandedState[item.DisplayName];
                    std::string prefix = isExpanded ? "v " : "> ";
                    std::string label = prefix + item.DisplayName;

                    // Make categories stand out slightly
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                    if (ImGui::Selectable(label.c_str(), isSelected)) {
                        m_SelectedIndex = i;
                        ExecuteSelectedItem();
                    }
                    ImGui::PopStyleColor();
                } else {
                    // Draw Node (Indented)
                    ImGui::Indent(15.0f);
                    if (ImGui::Selectable(item.DisplayName.c_str(), isSelected)) {
                        m_SelectedIndex = i;
                        ExecuteSelectedItem();
                    }
                    ImGui::Unindent(15.0f);
                }

                // Handle auto-scrolling
                if (isSelected && m_ScrollToSelection) {
                    ImGui::SetScrollHereY(0.5f);
                    m_ScrollToSelection = false;
                }
            }
        }

        ImGui::EndChild();
        ImGui::EndPopup();
    }

    ImGui::End();

    return std::move(m_PickedItem);
}

void CNodeContextMenu::OpenPopup()
{
    m_ShouldOpen = true;
    m_PopupPos = std::bit_cast<std::pair<float, float>>(ImGui::GetMousePos());
}