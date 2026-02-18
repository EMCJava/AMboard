//
// Created by LYS on 2/18/2026.
//

#include "NodeRenderer.hxx"
#include "NodeBackgroundPipline.hxx"

#include <Util/Assertions.hxx>

#include <Interface/WindowBase.hxx>

#include <Interface/Buffer/DynamicGPUBuffer.hxx>

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

    m_CommonNodeSSBOBuffer = CDynamicGPUBuffer::Create<SCommonNodeSSBO>(Window, wgpu::BufferUsage::Storage);
}

CNodeRenderer::~CNodeRenderer() = default;

void CNodeRenderer::WriteToNode(const size_t Id, const glm::vec2& Position, const glm::vec2& Size, const uint32_t HeaderColor) const
{
    MAKE_SURE(Id < m_IdCount)

    m_CommonNodeSSBOBuffer->At<SCommonNodeSSBO>(Id).Position = Position;

    auto& BackgroundInstanceBuffer = m_NodeBackgroundPipline->GetVertexBuffer().At<SNodeBackgroundInstanceBuffer>(Id);
    BackgroundInstanceBuffer.HeaderColor = HeaderColor;
    BackgroundInstanceBuffer.Size = Size;

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

size_t CNodeRenderer::CreateNode(const glm::vec2& Position, const glm::vec2& Size, const uint32_t HeaderColor)
{
    const auto FreeId = m_ValidIdRange.GetFirstFreeIndex();

    /// Need to create new Id
    if (FreeId >= m_IdCount) {

        MAKE_SURE(m_IdCount == FreeId)
        ++m_IdCount;

        CHECK(FreeId == m_CommonNodeSSBOBuffer->GetBufferSize());
        m_CommonNodeSSBOBuffer->PushUninitialized<SCommonNodeSSBO>();
        /// Common buffer resized, need to recreate the binding group
        CreateCommonBindingGroup();

        m_NodeBackgroundPipline->GetVertexBuffer().PushUninitialized();
    }

    m_ValidIdRange.SetSlot(FreeId);

    WriteToNode(FreeId, Position, Size, HeaderColor);
    return FreeId;
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

    RenderContext.RenderPassEncoder.SetVertexBuffer(0, m_NodeBackgroundPipline->GetVertexBuffer());
    RenderContext.RenderPassEncoder.SetPipeline(*m_NodeBackgroundPipline);

    for (auto const& [Left, Right] : m_ValidIdRange) {
        RenderContext.RenderPassEncoder.Draw(4, Right - Left + 1, 0, Left);
    }
}
