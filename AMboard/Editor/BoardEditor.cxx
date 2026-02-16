//
// Created by LYS on 2/16/2026.
//

#include "BoardEditor.hxx"
#include "NodePipline.hxx"

#include <AMboard/Macro/BaseNode.hxx>

void CBoardEditor::RenderNodes(const SRenderContext& RenderContext) const
{
    const auto& NodeVertexBuffer = m_NodePipline->GetVertexBuffer();

    RenderContext.RenderPassEncoder.SetPipeline(*m_NodePipline);
    RenderContext.RenderPassEncoder.SetVertexBuffer(0, NodeVertexBuffer, 0, NodeVertexBuffer.GetBufferByteSize());
    RenderContext.RenderPassEncoder.Draw(4, NodeVertexBuffer.GetBufferSize());
}

CBoardEditor::CBoardEditor()
{
    m_NodePipline = std::make_unique<CNodePipline>(this);
    m_NodePipline->CreatePipeline(*this);

    auto& [Size, Offset, HeaderColor] = *m_NodePipline->GetVertexBuffer().PushUninitialized<SNodeBackgroundRenderMeta>();
    Size = { 150, 100 };
    Offset = { 100, 100 };
    HeaderColor = 0xFF00FFFF;

    m_NodePipline->GetVertexBuffer().Upload(0);
}

CBoardEditor::~CBoardEditor() = default;

void CBoardEditor::RenderBoard(const SRenderContext& RenderContext)
{
    RenderNodes(RenderContext);
}