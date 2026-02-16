//
// Created by LYS on 2/16/2026.
//

#include "BoardEditor.hxx"

#include "GridPipline.hxx"
#include "NodePipline.hxx"

#include <AMboard/Macro/BaseNode.hxx>

#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>

void CBoardEditor::RenderNodes(const SRenderContext& RenderContext) const
{
    const auto& NodeVertexBuffer = m_NodePipline->GetVertexBuffer();

    RenderContext.RenderPassEncoder.SetPipeline(*m_NodePipline);
    RenderContext.RenderPassEncoder.SetVertexBuffer(0, NodeVertexBuffer, 0, NodeVertexBuffer.GetBufferByteSize());
    RenderContext.RenderPassEncoder.Draw(4, NodeVertexBuffer.GetBufferSize());
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

    auto& [Size, Offset, HeaderColor] = *m_NodePipline->GetVertexBuffer().PushUninitialized<SNodeBackgroundRenderMeta>();
    Size = { 150, 100 };
    Offset = { 100, 100 };
    HeaderColor = 0xFF00FFFF;

    m_NodePipline->GetVertexBuffer().Upload(0);
}

CBoardEditor::~CBoardEditor() = default;

CWindowBase::EWindowEventState CBoardEditor::ProcessEvent()
{
    return CWindowBase::ProcessEvent();
}

void CBoardEditor::RenderBoard(const SRenderContext& RenderContext)
{
    RenderContext.RenderPassEncoder.SetBindGroup(0, m_UniformBindingGroup, 0, nullptr);
    m_SceneUniform->Projection = glm::ortho(0.0f, (float)GetWindowSize().x, (float)GetWindowSize().y, 0.0f, 0.0f, 1.0f);
    m_SceneUniform->View = glm::translate(glm::mat4 { 1 }, glm::vec3(1920, 1080, 0) / 2.f);
    GetQueue().WriteBuffer(m_SceneUniformBuffer, 0, m_SceneUniform.get(), sizeof(SSceneUniform));

    RenderContext.RenderPassEncoder.SetPipeline(*m_GridPipline);
    RenderContext.RenderPassEncoder.Draw(4);

    RenderNodes(RenderContext);
}