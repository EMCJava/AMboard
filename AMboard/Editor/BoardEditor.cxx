//
// Created by LYS on 2/16/2026.
//

#include "BoardEditor.hxx"
#include "GridPipline.hxx"
#include "NodeRender/NodeRenderer.hxx"

#include <Util/Assertions.hxx>

#include <Interface/Font/TextRenderSystem.hxx>

#include <AMboard/Macro/BaseNode.hxx>

#include <GLFW/glfw3.h>

#include <glm/gtx/norm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>

glm::vec2 CBoardEditor::ScreenToWorld(const glm::vec2& ScreenPos) const noexcept
{
    return ScreenPos / m_CameraZoom + m_CameraOffset;
}

CBoardEditor::CBoardEditor()
{
    m_GridPipline = std::make_unique<CGridPipline>();
    m_GridPipline->CreatePipeline(*this);

    {
        m_SceneUniform = std::make_unique<SSceneUniform>();

        wgpu::BufferDescriptor bufferDesc;
        bufferDesc.label = "Scene Uniform";
        bufferDesc.size = sizeof(SSceneUniform);
        bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
        m_SceneUniformBuffer = GetDevice().CreateBuffer(&bufferDesc);

        std::vector<wgpu::BindGroupEntry> bindings(1);
        bindings[0].binding = 0;
        bindings[0].buffer = m_SceneUniformBuffer;
        bindings[0].offset = 0;
        bindings[0].size = sizeof(SSceneUniform);

        wgpu::BindGroupDescriptor bindGroupDesc { };
        bindGroupDesc.layout = m_GridPipline->GetBindGroups()[0];
        bindGroupDesc.entryCount = bindings.size();
        bindGroupDesc.entries = bindings.data();
        m_UniformBindingGroup = GetDevice().CreateBindGroup(&bindGroupDesc);
    }

    m_NodeRenderer = std::make_unique<CNodeRenderer>(this);

    {
        const auto NodeId = m_NodeRenderer->CreateNode("Node 1", { 100, 100 }, 0x668DAB88);

        m_NodeRenderer->AddInputPin(NodeId, true);
        m_NodeRenderer->AddInputPin(NodeId, false);
        m_NodeRenderer->AddInputPin(NodeId, false);
        m_NodeRenderer->AddInputPin(NodeId, false);
        m_NodeRenderer->AddInputPin(NodeId, false);
        m_NodeRenderer->AddOutputPin(NodeId, true);
        m_NodeRenderer->AddOutputPin(NodeId, false);
    }

    {
        const auto NodeId = m_NodeRenderer->CreateNode("Node 2", { 300, 100 }, 0x668DAB88);

        m_NodeRenderer->AddOutputPin(NodeId, true);
        m_NodeRenderer->AddOutputPin(NodeId, false);
        m_NodeRenderer->AddOutputPin(NodeId, false);
        m_NodeRenderer->AddOutputPin(NodeId, false);
        m_NodeRenderer->AddOutputPin(NodeId, false);
        m_NodeRenderer->AddInputPin(NodeId, true);
        m_NodeRenderer->AddInputPin(NodeId, false);
    }
}

CBoardEditor::~CBoardEditor() = default;

CWindowBase::EWindowEventState CBoardEditor::ProcessEvent()
{
    if (const auto EventStage = CWindowBase::ProcessEvent();
        EventStage != EWindowEventState::Normal) [[unlikely]] {
        return EventStage;
    }

    const auto MouseCurrentPos = GetInputManager().GetCursorPosition();
    const auto MouseDeltaPos = GetInputManager().GetDeltaCursor();
    const glm::vec2 MouseWorldPos = ScreenToWorld(MouseCurrentPos);

    std::optional<std::size_t> CursorHoveringNode;
    std::optional<std::size_t> CursorHoveringPin;

    /// Node interaction pre-compute
    for (const auto& [Left, Right] : m_NodeRenderer->GetValidRange()) {
        for (auto i = Left; i <= Right; ++i) {
            if (m_NodeRenderer->InBound(i, MouseWorldPos)) [[unlikely]] {
                CursorHoveringNode = i;

                CursorHoveringPin = m_NodeRenderer->GetHoveringPin(i, MouseWorldPos);

                if (m_LastHoveringPin != CursorHoveringPin) {
                    if (m_LastHoveringPin.has_value()) /// Deselect
                        m_NodeRenderer->ToggleHoverPin(*m_LastHoveringPin);
                    if (CursorHoveringPin.has_value()) /// Select
                        m_NodeRenderer->HoverPin(*CursorHoveringPin);
                }

                m_LastHoveringPin = CursorHoveringPin;
                break;
            }
        }
    }

    /// Draging virtual node
    if (m_SelectedPin.has_value()) {
        m_NodeRenderer->SetNodePosition(m_VirtualNodeForPinDrag, MouseWorldPos);
    }

    /// Drag canvas
    if (GetInputManager().GetMouseButtons().ConsumeHoldEvent(this, GLFW_MOUSE_BUTTON_MIDDLE)) {
        m_CameraOffset -= glm::vec2 { MouseDeltaPos } / m_CameraZoom;
        m_ScreenUniformDirty = true;
    }

    /// Zoom canvas
    if (const auto Scroll = GetInputManager().GetDeltaScroll(); Scroll != 0) {
        const float ZoomFactor = 1.0f + (Scroll * 0.1f);
        const float NewZoom = glm::clamp(m_CameraZoom * ZoomFactor, 0.1f, 5.0f);

        // Zoom towards mouse position
        const glm::vec2 WorldPosBefore = MouseWorldPos;
        m_CameraZoom = NewZoom;
        const glm::vec2 WorldPosAfter = ScreenToWorld(MouseCurrentPos);
        m_CameraOffset += (WorldPosBefore - WorldPosAfter);

        m_ScreenUniformDirty = true;
    }

    /// Remove Node
    if (GetInputManager().GetKeyboardButtons().ConsumeEvent(GLFW_KEY_DELETE)) {
        if (m_SelectedNode.has_value()) {
            m_NodeRenderer->RemoveNode(*m_SelectedNode);
            m_SelectedNode.reset();
        }
    }

    /// Create Node
    if (GetInputManager().GetMouseButtons().ConsumeEvent(GLFW_MOUSE_BUTTON_RIGHT)) {
        m_NodeRenderer->CreateNode("User Node", MouseWorldPos, ((rand() << 16) ^ rand()) & 0xFFFFFF00 | 0x88);
    }

    /// Select Node
    if (GetInputManager().GetMouseButtons().ConsumeEvent({ GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE })) {

        /// Click not drag
        if (glm::length2(glm::vec2 { MouseCurrentPos - *MouseStartClickPos }) < 3 * 3) {

            if (!m_SelectedPin.has_value() && CursorHoveringNode.has_value()) [[unlikely]] {

                if (m_SelectedNode.has_value()) {
                    m_NodeRenderer->ToggleSelect(*m_SelectedNode);
                }

                if (m_SelectedNode.has_value() && *m_SelectedNode == *CursorHoveringNode) { /// Deselect
                    m_SelectedNode.reset();
                } else {
                    m_SelectedNode = *CursorHoveringNode;
                    m_NodeRenderer->Select(*CursorHoveringNode);
                }
            }
        }

        if (m_SelectedPin.has_value() && CursorHoveringPin.has_value() && *m_SelectedPin != *CursorHoveringPin) {
            m_NodeRenderer->LinkPin(*m_SelectedPin, *CursorHoveringPin);
        }

        /// Clear pin selection
        if (m_SelectedPin.has_value()) {
            m_NodeRenderer->ToggleConnectPin(*m_SelectedPin);
            m_SelectedPin.reset();
            m_NodeRenderer->RemoveNode(m_VirtualNodeForPinDrag);
            m_NodeRenderer->UnlinkPin(m_VirtualConnectionForPinDrag);
        }

        MouseStartClickPos.reset();
    }

    /// Click
    if (GetInputManager().GetMouseButtons().ConsumeHoldEvent(this, GLFW_MOUSE_BUTTON_LEFT)) {
        if (!MouseStartClickPos.has_value()) {
            MouseStartClickPos = MouseCurrentPos;

            /// Pin selecting takes priority
            if (CursorHoveringPin.has_value()) {
                CHECK(!m_SelectedPin.has_value())

                m_SelectedPin = CursorHoveringPin;
                m_NodeRenderer->ConnectPin(*CursorHoveringPin);

                m_VirtualNodeForPinDrag = m_NodeRenderer->CreateVirtualNode(MouseWorldPos);
                m_VirtualConnectionForPinDrag = m_NodeRenderer->LinkVirtualPin(*m_SelectedPin, m_VirtualNodeForPinDrag);
            }

            /// Drag selected pin
            m_DraggingNode = false;
            if (m_SelectedNode.has_value()) {
                if (!m_SelectedPin.has_value() && m_SelectedNode == CursorHoveringNode) {
                    m_DraggingNode = true;
                    NodeDragThreshold = 3 * 3;
                }
            }

        } else if (m_DraggingNode) {

            if (NodeDragThreshold.has_value()) {
                if (glm::length2(glm::vec2 { MouseCurrentPos - *MouseStartClickPos }) > *NodeDragThreshold) {
                    NodeDragThreshold.reset();
                }
            }

            if (!NodeDragThreshold.has_value()) {
                if (MouseDeltaPos.x || MouseDeltaPos.y) {
                    m_NodeRenderer->MoveNode(*m_SelectedNode, glm::vec2 { MouseDeltaPos } / m_CameraZoom);
                }
            }
        }
    }

    return EWindowEventState::Normal;
}

void CBoardEditor::RenderBoard(const SRenderContext& RenderContext)
{
    RenderContext.RenderPassEncoder.SetBindGroup(0, m_UniformBindingGroup, 0, nullptr);

    if (m_ScreenUniformDirty) {
        m_SceneUniform->Projection = glm::ortho(0.0f, (float)GetWindowSize().x, (float)GetWindowSize().y, 0.0f, 0.0f, 1.0f);
        m_SceneUniform->View = glm::translate(glm::scale(glm::mat4 { 1 }, glm::vec3(m_CameraZoom, m_CameraZoom, 1.0f)), glm::vec3(-m_CameraOffset, 0));
        GetQueue().WriteBuffer(m_SceneUniformBuffer, 0, m_SceneUniform.get(), sizeof(SSceneUniform));
        m_ScreenUniformDirty = false;
    }

    RenderContext.RenderPassEncoder.SetPipeline(*m_GridPipline);
    RenderContext.RenderPassEncoder.Draw(4);

    m_NodeRenderer->Render(RenderContext);
}