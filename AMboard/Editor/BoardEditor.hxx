//
// Created by LYS on 2/16/2026.
//

#pragma once

#include "BoardEditor.hxx"
#include "Utils.hxx"

#include <AMboard/Editor/NodeRender/NodeTextPerGroupMeta.hxx>
#include <AMboard/Editor/NodeRender/NodeTextType.hxx>

#include <Interface/Font/TextRenderSystemHandle.hxx>
#include <Interface/WindowBase.hxx>

#include <filesystem>
#include <memory>
#include <mutex>
#include <vector>

#include <boost/bimap.hpp>

#include <glm/vec2.hpp>

using NodeStorage = std::unique_ptr<class CBaseNode, void (*)(CBaseNode*)>;
void NodeDefaultDeleter(CBaseNode* Node);

struct SEditorNodeContext {
    NodeStorage Node = { nullptr, nullptr };
    bool SupportFileDrop = false;
    glm::vec2 LogicalPosition; // This is used for node snapping

    /// Mode the logical position and return the display position
    glm::vec2 MoveLogicalPosition(const glm::vec2& Delta, float SnapValue) noexcept;
    glm::vec2 GetDisplayPosition(float SnapValue) const noexcept;

    bool operator==(const CBaseNode* Ptr) const noexcept { return Node.get() == Ptr; }
};

class CPin;
class CBoardEditor : public CWindowBase {

    void SetUpImGui() noexcept;

    void RenderImGuiMenu();

    [[nodiscard]] glm::vec2 WorldToScreen(const glm::vec2& WorldPos) const noexcept;
    [[nodiscard]] glm::vec2 ScreenToWorld(const glm::vec2& ScreenPos) const noexcept;
    [[nodiscard]] std::optional<std::size_t> WorldAboveNode(const glm::vec2& WorldPos) const noexcept;

    std::optional<size_t> TryRegisterConnection(CPin* OutputPin, CPin* InputPin);

    size_t CreateNode(const std::string& NodeExtName, const glm::vec2& Position, uint32_t HeaderColor);
    size_t RegisterNode(NodeStorage Node, const std::string& Title, const glm::vec2& Position, uint32_t HeaderColor);
    void UnregisterNode(size_t NodeId);

    void MoveCanvas(const glm::vec2& Delta) noexcept;

    void EndPinDrag() noexcept;

    void LoadCanvas();
    void LoadCanvas(const std::filesystem::path& Canvas);
    void SaveCanvas() noexcept;
    void SaveCanvasTo(const std::filesystem::path& Path) noexcept;

    void FlushPendingNodeTextUpdate();

    void SetCancelOnHoldAction(auto&& Func);

    void UpdateDragSelection(bool ResetSelection = false);

public:
    CBoardEditor();
    ~CBoardEditor();

    void RecreateSurface() noexcept override;

    EWindowEventState ProcessEvent() override;

    void RenderBoard(const SRenderContext& RenderContext);

    bool OnFileDrag(int WindowX, int WindowY) override;
    void OnFileDragLeave() override;
    void OnFileDragDrop(int WindowX, int WindowY, const std::vector<std::string>& Files) override;

protected:
    bool m_ScreenUniformDirty = true;
    std::unique_ptr<class SSceneUniform> m_SceneUniform;
    wgpu::Buffer m_SceneUniformBuffer;
    wgpu::BindGroup m_UniformBindingGroup;

    std::unique_ptr<class CGridPipline> m_GridPipline;
    std::unique_ptr<class CNodeRenderer> m_NodeRenderer;
    std::unique_ptr<class CNodeContextMenu> m_NodeContextMenu;

    std::string_view m_CurrentToolTips;

    std::optional<glm::ivec2> MouseStartClickPos;
    glm::vec2 m_MouseStartClickWorldPos; // If MouseStartClickPos has value, this is valid
    std::optional<int> NodeDragThreshold;

    bool m_ControlDraggingCanvas = false;
    bool m_ControlDraggingNode = false;
    bool m_ControlDraggingSelection = false;

    std::vector<size_t> m_SelectedNodes;
    std::vector<size_t> m_LiveSelectedNodes;
    std::vector<size_t> m_LiveSelectedNodesBuffer;

    bool m_LiveSelectNodeToggle = false;
    std::vector<size_t> m_LiveBoundedNodesBuffer;

    std::optional<std::size_t> m_EntranceNode;

    std::optional<std::size_t> m_VirtualNodeForPinDrag;
    std::size_t m_VirtualConnectionForPinDrag;
    std::optional<std::size_t> m_DraggingPin;
    std::optional<std::size_t> m_LastHoveringPin;

    std::optional<std::size_t> m_LastFileDragNode;

    std::function<void()> m_CancelOnHoldAction;

    std::unordered_map<std::pair<CPin*, CPin*>, size_t, SPairHash<CPin*, CPin*>> m_ConnectionIdMapping;
    boost::bimap<CPin*, size_t> m_PinIdMapping;

    std::unique_ptr<class CCustomNodeLoader> m_CustomNodeLoader;

    class INodeImGuiPupUpExt* m_PopupNode = nullptr;
    std::string m_PopupTitle;
    bool m_TriggerPopup = true;

    std::mutex m_PendingNodeTextUpdateMutex;
    std::unordered_map<std::pair<size_t, ENodeTextType>, std::pair<class INodeInnerText*, STextUpdateData>, SPairHash<size_t, ENodeTextType>> m_PendingNodeTextUpdate;

    class CExecuteNode* m_LastExecutedNode = nullptr;
    std::unique_ptr<class CExecutionManager> m_ExecutionManager;

    std::vector<SEditorNodeContext> m_Nodes;

    glm::vec2 m_CameraOffset { };
    float m_CameraZoom = 1;
    float m_NodeSnapValue = 5;

    std::filesystem::path m_CurrentBoardPath;
};
