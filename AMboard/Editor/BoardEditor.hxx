//
// Created by LYS on 2/16/2026.
//

#pragma once

#include "BoardEditor.hxx"

#include <Interface/Font/TextRenderSystemHandle.hxx>
#include <Interface/WindowBase.hxx>

#include <filesystem>
#include <memory>
#include <vector>

#include <boost/bimap.hpp>

#include <glm/vec2.hpp>

using NodeStorage = std::unique_ptr<class CBaseNode, void (*)(CBaseNode*)>;
void NodeDefaultDeleter(CBaseNode* Node);

struct SEditorNodeContext {
    NodeStorage Node = { nullptr, nullptr };
    glm::vec2 LogicalPosition; // This is used for node snapping

    /// Mode the logical position and return the display position
    glm::vec2 MoveLogicalPosition(const glm::vec2& Delta, float SnapValue) noexcept;
    glm::vec2 GetDisplayPosition(float SnapValue) const noexcept;
};

class CPin;
class CBoardEditor : public CWindowBase {

    void SetUpImGui() noexcept;

    [[nodiscard]] glm::vec2 ScreenToWorld(const glm::vec2& ScreenPos) const noexcept;

    std::optional<size_t> TryRegisterConnection(CPin* OutputPin, CPin* InputPin);

    size_t CreateNode(const std::string& NodeExtName, const glm::vec2& Position, uint32_t HeaderColor);
    size_t RegisterNode(NodeStorage Node, const std::string& Title, const glm::vec2& Position, uint32_t HeaderColor);
    void UnregisterNode(size_t NodeId);

    void MoveCanvas(const glm::vec2& Delta) noexcept;

    void EndPinDrag() noexcept;

    void SaveCanvas() noexcept;
    void SaveCanvasTo(const std::filesystem::path& Path) noexcept;

public:
    CBoardEditor();
    ~CBoardEditor();

    EWindowEventState ProcessEvent() override;

    void RenderBoard(const SRenderContext& RenderContext);

protected:
    bool m_ScreenUniformDirty = true;
    std::unique_ptr<class SSceneUniform> m_SceneUniform;
    wgpu::Buffer m_SceneUniformBuffer;
    wgpu::BindGroup m_UniformBindingGroup;

    std::unique_ptr<class CGridPipline> m_GridPipline;
    std::unique_ptr<class CNodeRenderer> m_NodeRenderer;

    std::optional<glm::ivec2> MouseStartClickPos;
    std::optional<int> NodeDragThreshold;

    bool m_ControlDraggingCanvas = false;

    bool m_DraggingNode = false;
    std::optional<std::size_t> m_SelectedNode;
    std::optional<std::size_t> m_EntranceNode;

    std::size_t m_VirtualNodeForPinDrag;
    std::size_t m_VirtualConnectionForPinDrag;
    std::optional<std::size_t> m_DraggingPin;
    std::optional<std::size_t> m_LastHoveringPin;

    struct PinPtrPairHash {
        std::size_t operator()(const std::pair<void*, void*>& p) const noexcept;
    };
    std::unordered_map<std::pair<CPin*, CPin*>, size_t, PinPtrPairHash> m_ConnectionIdMapping;
    boost::bimap<CPin*, size_t> m_PinIdMapping;

    std::unique_ptr<class CCustomNodeLoader> m_CustomNodeLoader;

    std::vector<SEditorNodeContext> m_Nodes;

    glm::vec2 m_CameraOffset { };
    float m_CameraZoom = 1;
    float m_NodeSnapValue = 5;

    std::filesystem::path m_CurrentBoardPath;
};
