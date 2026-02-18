//
// Created by LYS on 2/16/2026.
//

#include "BoardEditor.hxx"

#include "GridPipline.hxx"

#include "NodeRender/NodeRenderer.hxx"

#include <Interface/Font/TextRenderSystem.hxx>

#include <AMboard/Macro/BaseNode.hxx>
#include <GLFW/glfw3.h>

#include <glm/gtx/norm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>

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

    m_NodeRenderer->CreateNode("Node", { 100, 100 }, { 150, 100 }, 0x668DAB88);
    m_NodeRenderer->CreateNode("Node", { 250, 50 }, { 450, 150 }, 0x668DAB88);
}

CBoardEditor::~CBoardEditor() = default;

CWindowBase::EWindowEventState CBoardEditor::ProcessEvent()
{
    if (const auto EventStage = CWindowBase::ProcessEvent();
        EventStage != EWindowEventState::Normal) [[unlikely]] {
        return EventStage;
    }

    /// Drag canvas
    if (GetInputManager().GetMouseButtons().ConsumeHoldEvent(this, GLFW_MOUSE_BUTTON_MIDDLE)) {
        m_CameraOffset -= glm::vec2 { GetInputManager().GetDeltaCursor() } / m_CameraZoom;
        m_ScreenUniformDirty = true;
    }

    /// Zoom canvas
    if (const auto Scroll = GetInputManager().GetDeltaScroll(); Scroll != 0) {
        const float ZoomFactor = 1.0f + (Scroll * 0.1f);
        const float NewZoom = glm::clamp(m_CameraZoom * ZoomFactor, 0.1f, 5.0f);

        // Zoom towards mouse position
        const glm::vec2 WorldPosBefore = ScreenToWorld(GetInputManager().GetCursorPosition());
        m_CameraZoom = NewZoom;
        const glm::vec2 WorldPosAfter = ScreenToWorld(GetInputManager().GetCursorPosition());
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
        m_NodeRenderer->CreateNode("User Node", ScreenToWorld(GetInputManager().GetCursorPosition()), { 150 + rand() % 100, 100 + rand() % 20 }, ((rand() << 16) ^ rand()) & 0xFFFFFF00 | 0x88);
    }

    /// Select Node
    if (GetInputManager().GetMouseButtons().ConsumeEvent({ GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE })) {

        const auto MouseReleasePos = GetInputManager().GetCursorPosition();
        if (glm::length2(glm::vec2 { MouseReleasePos - *MouseStartClickPos }) < 3 * 3) {
            const glm::vec2 ClickPosition = ScreenToWorld(MouseReleasePos);

            for (auto [Left, Right] : m_NodeRenderer->GetValidRange()) {
                for (int i = Left; i <= Right; ++i) {

                    if (m_NodeRenderer->InBound(i, ClickPosition)) [[unlikely]] {

                        if (m_SelectedNode.has_value()) { /// Deselect

                            if (*m_SelectedNode == i) {
                                m_NodeRenderer->ToggleSelect(i);
                                m_SelectedNode.reset();
                                break;
                            }

                            m_NodeRenderer->ToggleSelect(*m_SelectedNode);
                        }

                        m_SelectedNode = i;
                        m_NodeRenderer->Select(i);

                        break;
                    }
                }
            }
        }

        MouseStartClickPos.reset();
    }

    /// Click
    if (GetInputManager().GetMouseButtons().ConsumeHoldEvent(this, GLFW_MOUSE_BUTTON_LEFT)) {

        const auto CurrentMousePosition = GetInputManager().GetCursorPosition();
        const auto WorldMousePosition = ScreenToWorld(CurrentMousePosition);

        if (!MouseStartClickPos.has_value()) {
            MouseStartClickPos = GetInputManager().GetCursorPosition();

            m_DraggingNode = false;
            if (m_SelectedNode.has_value()) {
                if (m_NodeRenderer->InBound(*m_SelectedNode, WorldMousePosition)) {
                    m_DraggingNode = true;
                    NodeDragThreshold = 3 * 3;
                }
            }

        } else if (m_DraggingNode) {

            if (NodeDragThreshold.has_value()) {
                if (glm::length2(glm::vec2 { CurrentMousePosition - *MouseStartClickPos }) > *NodeDragThreshold) {
                    NodeDragThreshold.reset();
                }
            }

            if (!NodeDragThreshold.has_value()) {
                if (const auto DeltaCursor = GetInputManager().GetDeltaCursor(); DeltaCursor.x || DeltaCursor.y) {
                    m_NodeRenderer->MoveNode(*m_SelectedNode, glm::vec2 { GetInputManager().GetDeltaCursor() } / m_CameraZoom);
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