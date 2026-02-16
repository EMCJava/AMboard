//
// Created by LYS on 2/16/2026.
//

#include "BoardEditor.hxx"

#include "GridPipline.hxx"
#include "NodePipline.hxx"

#include <AMboard/Macro/BaseNode.hxx>
#include <GLFW/glfw3.h>
#include <iostream>

#include <glm/gtx/norm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>

void CBoardEditor::RenderNodes(const SRenderContext& RenderContext) const
{
    const auto& NodeVertexBuffer = m_NodePipline->GetVertexBuffer();

    RenderContext.RenderPassEncoder.SetPipeline(*m_NodePipline);
    RenderContext.RenderPassEncoder.SetVertexBuffer(0, NodeVertexBuffer, 0, NodeVertexBuffer.GetBufferByteSize());
    RenderContext.RenderPassEncoder.Draw(4, NodeVertexBuffer.GetBufferSize());
}

glm::vec2 CBoardEditor::ScreenToWorld(const glm::vec2& ScreenPos) const noexcept
{
    return ScreenPos / m_CameraZoom + m_CameraOffset;
}

CBoardEditor::CBoardEditor()
{
    m_GridPipline = std::make_unique<CGridPipline>();
    m_GridPipline->CreatePipeline(*this);

    m_NodePipline = std::make_unique<CNodePipline>(this);
    m_NodePipline->CreatePipeline(*this);

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

    {
        auto& [Size, Offset, HeaderColor, State] = *m_NodePipline->GetVertexBuffer().PushUninitialized<SNodeBackgroundRenderMeta>();
        Size = { 150, 100 };
        Offset = { 100, 100 };
        HeaderColor = 0xFF00FFFF;
        State = 0;
    }

    {
        auto& [Size, Offset, HeaderColor, State] = *m_NodePipline->GetVertexBuffer().PushUninitialized<SNodeBackgroundRenderMeta>();
        Size = { 450, 150 };
        Offset = { 250, 50 };
        HeaderColor = 0xFFFF00FF;
        State = 0;
    }

    m_NodePipline->GetVertexBuffer().Upload(0, 2);
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

    if (GetInputManager().GetMouseButtons().ConsumeEvent({ GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE })) {

        const auto MouseReleasePos = GetInputManager().GetCursorPosition();
        if (glm::length2(glm::vec2 { MouseReleasePos - *MouseStartClickPos }) < 3 * 3) {
            const glm::vec2 ClickPosition = ScreenToWorld(GetInputManager().GetCursorPosition());

            const auto RenderMetas = m_NodePipline->GetRenderMetas();
            for (auto& RenderMeta : RenderMetas) {

                if (ClickPosition.x >= RenderMeta.Offset.x && ClickPosition.y >= RenderMeta.Offset.y
                    && ClickPosition.x <= RenderMeta.Offset.x + RenderMeta.Size.x && ClickPosition.y <= RenderMeta.Offset.y + RenderMeta.Size.y) {

                    const auto CurrentClickingNode = std::distance(RenderMetas.data(), &RenderMeta);
                    if (m_SelectedNode.has_value()) { /// Deselect

                        if (*m_SelectedNode == CurrentClickingNode) {
                            RenderMeta.State ^= 1;
                            m_NodePipline->GetVertexBuffer().Upload(CurrentClickingNode);

                            m_SelectedNode.reset();
                            break;
                        }

                        RenderMetas[*m_SelectedNode].State ^= 1;
                        m_NodePipline->GetVertexBuffer().Upload(*m_SelectedNode);
                    }

                    m_SelectedNode = CurrentClickingNode;
                    RenderMeta.State |= 1;
                    m_NodePipline->GetVertexBuffer().Upload(CurrentClickingNode);

                    break;
                }
            }
        }

        MouseStartClickPos.reset();
    }

    /// Click
    if (GetInputManager().GetMouseButtons().ConsumeHoldEvent(this, GLFW_MOUSE_BUTTON_LEFT)) {
        if (!MouseStartClickPos.has_value())
            MouseStartClickPos = GetInputManager().GetCursorPosition();
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

    RenderNodes(RenderContext);
}