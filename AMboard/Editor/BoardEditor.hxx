//
// Created by LYS on 2/16/2026.
//

#pragma once

#include "BoardEditor.hxx"

#include <Interface/Font/TextRenderSystemHandle.hxx>
#include <Interface/WindowBase.hxx>

#include <boost/bimap.hpp>
#include <memory>
#include <vector>

#include <glm/vec2.hpp>

struct SEditorNodeContext {
    std::unique_ptr<class CBaseNode> Node;
};

class CPin;
class CBoardEditor : public CWindowBase {

    [[nodiscard]] glm::vec2 ScreenToWorld(const glm::vec2& ScreenPos) const noexcept;

    std::optional<size_t> TryRegisterConnection(CPin* OutputPin, CPin* InputPin);

    size_t RegisterNode(std::unique_ptr<CBaseNode> Node, const std::string& Title, const glm::vec2& Position, uint32_t HeaderColor);
    void UnregisterNode(size_t NodeId);

    void MoveCanvas(const glm::vec2& Delta) noexcept;

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

    std::size_t m_VirtualNodeForPinDrag;
    std::size_t m_VirtualConnectionForPinDrag;
    std::optional<std::size_t> m_DraggingPin;
    std::optional<std::size_t> m_LastHoveringPin;

    struct PinPtrPairHash {
        std::size_t operator()(const std::pair<void*, void*>& p) const noexcept;
    };
    std::unordered_map<std::pair<CPin*, CPin*>, size_t, PinPtrPairHash> m_ConnectionIdMapping;
    boost::bimap<CPin*, size_t> m_PinIdMapping;

    std::vector<SEditorNodeContext> m_Nodes;

    glm::vec2 m_CameraOffset { };
    float m_CameraZoom = 1;
};
