//
// Created by LYS on 2/18/2026.
//

#include "NodeRenderer.hxx"
#include "NodeBackgroundPipline.hxx"
#include "NodeTextRenderPipline.hxx"

#include <Util/Assertions.hxx>

#include <Interface/WindowBase.hxx>

#include <Interface/Buffer/DynamicGPUBuffer.hxx>

#include <Interface/Font/Font.hxx>

void SNodeTextHandle::UnRegisterText(CNodeTextRenderPipline* Pipline)
{
    if (TitleText.has_value()) {
        Pipline->UnregisterTextGroup(*TitleText);
    }
}

void CNodeRenderer::CreateCommonBindingGroup()
{
    MAKE_SURE(m_CommonNodeSSBOBuffer)

    std::vector<wgpu::BindGroupEntry> bindings(1);
    // Binding 2: Common node SSBO
    bindings[0].binding = 0;
    bindings[0].buffer = *m_CommonNodeSSBOBuffer;
    bindings[0].offset = 0;
    bindings[0].size = m_CommonNodeSSBOBuffer->GetBufferByteSize();

    wgpu::BindGroupDescriptor bindGroupDesc { };
    bindGroupDesc.layout = m_NodeBackgroundPipline->GetBindGroups()[1]; /// LOL
    bindGroupDesc.entryCount = bindings.size();
    bindGroupDesc.entries = bindings.data();
    m_CommonNodeSSBOBindingGroup = m_Window->GetDevice().CreateBindGroup(&bindGroupDesc);
}

CNodeRenderer::CNodeRenderer(const CWindowBase* Window)
    : m_Window(Window)
{
    m_NodeBackgroundPipline = std::make_unique<CNodeBackgroundPipline>(Window);
    m_NodeBackgroundPipline->CreatePipeline(*Window);

    m_NodeTextPipline = std::make_unique<CNodeTextRenderPipline>(Window, std::make_shared<CFont>(Window, "Res/Cubic_11.ttf"));
    m_NodeTextPipline->CreatePipeline(*Window);

    m_CommonNodeSSBOBuffer = CDynamicGPUBuffer::Create<SCommonNodeSSBO>(Window, wgpu::BufferUsage::Storage);
}

CNodeRenderer::~CNodeRenderer() = default;

void CNodeRenderer::WriteToNode(const size_t Id, const std::string& Title, const glm::vec2& Position, const glm::vec2& Size, const uint32_t HeaderColor)
{
    MAKE_SURE(Id < m_IdCount)

    auto& CommonNode = m_CommonNodeSSBOBuffer->At<SCommonNodeSSBO>(Id);
    CommonNode.Position = Position;
    CommonNode.State = 0;

    auto& BackgroundInstanceBuffer = m_NodeBackgroundPipline->GetVertexBuffer().At<SNodeBackgroundInstanceBuffer>(Id);
    BackgroundInstanceBuffer.HeaderColor = HeaderColor;
    BackgroundInstanceBuffer.Size = Size;

    if (!m_NodeTextHandles[Id].TitleText.has_value()) {
        m_NodeTextHandles[Id].TitleText = m_NodeTextPipline->RegisterTextGroup(Id, Title, 0.4, { .Color = 0xFFFFFFFF });
    } else {
        (*m_NodeTextHandles[Id].TitleText)->Text = Title;
        m_NodeTextPipline->UpdateTextGroup(*m_NodeTextHandles[Id].TitleText);
    }

    m_NodeBackgroundPipline->GetVertexBuffer().Upload(Id);
    m_CommonNodeSSBOBuffer->Upload(Id);
}

void CNodeRenderer::Select(size_t Id) const
{
    MAKE_SURE(Id < m_IdCount)

    m_CommonNodeSSBOBuffer->At<SCommonNodeSSBO>(Id).State |= 1;
    m_CommonNodeSSBOBuffer->Upload(Id);
}

void CNodeRenderer::ToggleSelect(size_t Id) const
{
    MAKE_SURE(Id < m_IdCount)

    m_CommonNodeSSBOBuffer->At<SCommonNodeSSBO>(Id).State ^= 1;
    m_CommonNodeSSBOBuffer->Upload(Id);
}

glm::vec2 CNodeRenderer::MoveNode(size_t Id, const glm::vec2& Delta) const
{
    MAKE_SURE(Id < m_IdCount)

    const auto NewPosition = m_CommonNodeSSBOBuffer->At<SCommonNodeSSBO>(Id).Position += Delta;
    m_CommonNodeSSBOBuffer->Upload(Id);
    return NewPosition;
}

size_t CNodeRenderer::CreateNode(const std::string& Title, const glm::vec2& Position, const glm::vec2& Size, const uint32_t HeaderColor)
{
    const auto FreeId = m_ValidIdRange.GetFirstFreeIndex();

    /// Need to create new Id
    if (FreeId >= m_IdCount) {

        MAKE_SURE(m_IdCount == FreeId)
        ++m_IdCount;

        CHECK(FreeId == m_CommonNodeSSBOBuffer->GetBufferSize());
        m_CommonNodeSSBOBuffer->PushUninitialized();
        /// Common buffer resized, need to recreate the binding group
        CreateCommonBindingGroup();

        m_NodeTextHandles.emplace_back();

        m_NodeBackgroundPipline->GetVertexBuffer().PushUninitialized();
    }

    m_ValidIdRange.SetSlot(FreeId);

    WriteToNode(FreeId, Title, Position, Size, HeaderColor);
    return FreeId;
}

void CNodeRenderer::RemoveNode(const size_t Id)
{
    m_ValidIdRange.RemoveSlot(Id);
    m_NodeTextHandles[Id].UnRegisterText(m_NodeTextPipline.get());
}

bool CNodeRenderer::InBound(const size_t Id, const glm::vec2& Position) const noexcept
{
    const auto& Offset = m_CommonNodeSSBOBuffer->At<SCommonNodeSSBO>(Id).Position;
    const auto& Size = m_NodeBackgroundPipline->GetVertexBuffer().At<SNodeBackgroundInstanceBuffer>(Id).Size;

    return Position.x >= Offset.x && Position.y >= Offset.y && Position.x <= Offset.x + Size.x && Position.y <= Offset.y + Size.y;
}

void CNodeRenderer::Render(const SRenderContext& RenderContext)
{
    if (m_ValidIdRange.GetSlotCount() == 0) {
        return;
    }

    RenderContext.RenderPassEncoder.SetBindGroup(1, m_CommonNodeSSBOBindingGroup, 0, nullptr);

    {
        RenderContext.RenderPassEncoder.SetVertexBuffer(0, m_NodeBackgroundPipline->GetVertexBuffer());
        RenderContext.RenderPassEncoder.SetPipeline(*m_NodeBackgroundPipline);
        for (auto const& [Left, Right] : m_ValidIdRange) {
            RenderContext.RenderPassEncoder.Draw(4, Right - Left + 1, 0, Left);
        }
    }

    {
        RenderContext.RenderPassEncoder.SetBindGroup(2, m_NodeTextPipline->GetBindingGroup(), 0, nullptr);
        RenderContext.RenderPassEncoder.SetVertexBuffer(0, m_NodeTextPipline->GetVertexBuffer());
        RenderContext.RenderPassEncoder.SetPipeline(*m_NodeTextPipline);
        for (auto const& [Left, Right] : m_NodeTextPipline->GetDrawCommands()) {
            RenderContext.RenderPassEncoder.Draw(4, Right - Left + 1, 0, Left);
        }
    }
}
