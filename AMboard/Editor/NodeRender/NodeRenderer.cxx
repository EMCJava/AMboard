//
// Created by LYS on 2/18/2026.
//

#include "NodeRenderer.hxx"
#include "NodeBackgroundPipline.hxx"
#include "NodeConnectionPipline.hxx"
#include "NodePinPipline.hxx"
#include "NodeTextRenderPipline.hxx"

#include <Util/Assertions.hxx>

#include <Interface/WindowBase.hxx>

#include <Interface/Buffer/DynamicGPUBuffer.hxx>

#include <Interface/Font/Font.hxx>

#include <glm/gtx/norm.hpp>

void SNodeAdditionalSourceHandle::UnRegister(const CNodeRenderer* Renderer)
{
    if (TitleText.has_value()) {
        Renderer->m_NodeTextPipline->UnregisterTextGroup(*TitleText);
        TitleText.reset();
    }

    for (const auto PinIndex :
        std::views::join(std::array {
            std::views::all(InputPins),
            std::views::all(OutputPins) }))
        Renderer->m_NodePinPipline->RemovePin(PinIndex);
    InputPins.clear();
    OutputPins.clear();
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

size_t CNodeRenderer::NextFreeNode()
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

        m_NodeResourcesHandles.emplace_back();

        m_NodeBackgroundPipline->GetVertexBuffer().PushUninitialized();
    }

    m_ValidIdRange.SetSlot(FreeId);
    return FreeId;
}

CNodeRenderer::CNodeRenderer(const CWindowBase* Window)
    : m_Window(Window)
{
    m_NodeBackgroundPipline = std::make_unique<CNodeBackgroundPipline>(Window);
    m_NodeBackgroundPipline->CreatePipeline(*Window);

    m_NodeTextPipline = std::make_unique<CNodeTextRenderPipline>(Window, std::make_shared<CFont>(Window, "Res/Cubic_11.ttf"));
    m_NodeTextPipline->CreatePipeline(*Window);

    m_NodePinPipline = std::make_unique<CNodePinPipline>(Window);
    m_NodePinPipline->CreatePipeline(*Window);

    m_NodeConnectionPipline = std::make_unique<CNodeConnectionPipline>(Window);
    m_NodeConnectionPipline->CreatePipeline(*Window);

    m_CommonNodeSSBOBuffer = CDynamicGPUBuffer::Create<SCommonNodeSSBO>(Window, wgpu::BufferUsage::Storage);
}

CNodeRenderer::~CNodeRenderer() = default;

constexpr glm::vec2 MinimumNodeSize { 150, 100 };

void CNodeRenderer::WriteToNode(const size_t Id, const std::string& Title, const glm::vec2& Position, const uint32_t HeaderColor, std::optional<glm::vec2> NodeSize)
{
    MAKE_SURE(Id < m_IdCount)

    auto& CommonNode = m_CommonNodeSSBOBuffer->At<SCommonNodeSSBO>(Id);
    CommonNode.Position = Position;
    CommonNode.State = 0;

    auto& BackgroundInstanceBuffer = m_NodeBackgroundPipline->GetVertexBuffer().At<SNodeBackgroundInstanceBuffer>(Id);
    BackgroundInstanceBuffer.HeaderColor = HeaderColor;
    BackgroundInstanceBuffer.Size = NodeSize.value_or(MinimumNodeSize);

    if (!m_NodeResourcesHandles[Id].TitleText.has_value()) {
        m_NodeResourcesHandles[Id].TitleText = m_NodeTextPipline->RegisterTextGroup(Id, Title, 0.4, { .Color = 0xFFFFFFFF });
    } else {
        (*m_NodeResourcesHandles[Id].TitleText)->Text = Title;
        m_NodeTextPipline->UpdateTextGroup(*m_NodeResourcesHandles[Id].TitleText);
    }

    if (!Title.empty() && !NodeSize.has_value()) {
        BackgroundInstanceBuffer.Size = glm::max(BackgroundInstanceBuffer.Size, { (*m_NodeResourcesHandles[Id].TitleText)->TextWidth + 20, 0 });
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

void CNodeRenderer::ConnectPin(size_t Id) const
{
    MAKE_SURE(Id < m_NodePinPipline->GetVertexBuffer().GetBufferSize())

    m_NodePinPipline->GetVertexBuffer().At<SNodePinInstanceBuffer>(Id).Flag |= SNodePinInstanceBuffer::ConnectedFlag;
    m_NodePinPipline->GetVertexBuffer().Upload(Id);
}

void CNodeRenderer::DisconnectPin(const size_t Id) const
{
    MAKE_SURE(Id < m_NodePinPipline->GetVertexBuffer().GetBufferSize())

    m_NodePinPipline->GetVertexBuffer().At<SNodePinInstanceBuffer>(Id).Flag &= ~SNodePinInstanceBuffer::ConnectedFlag;
    m_NodePinPipline->GetVertexBuffer().Upload(Id);
}

void CNodeRenderer::ToggleConnectPin(size_t Id) const
{
    MAKE_SURE(Id < m_NodePinPipline->GetVertexBuffer().GetBufferSize())

    m_NodePinPipline->GetVertexBuffer().At<SNodePinInstanceBuffer>(Id).Flag ^= SNodePinInstanceBuffer::ConnectedFlag;
    m_NodePinPipline->GetVertexBuffer().Upload(Id);
}

void CNodeRenderer::HoverPin(const size_t Id) const
{
    MAKE_SURE(Id < m_NodePinPipline->GetVertexBuffer().GetBufferSize())

    m_NodePinPipline->GetVertexBuffer().At<SNodePinInstanceBuffer>(Id).Flag |= SNodePinInstanceBuffer::HoveringFlag;
    m_NodePinPipline->GetVertexBuffer().Upload(Id);
}

void CNodeRenderer::ToggleHoverPin(const size_t Id) const
{
    MAKE_SURE(Id < m_NodePinPipline->GetVertexBuffer().GetBufferSize())

    m_NodePinPipline->GetVertexBuffer().At<SNodePinInstanceBuffer>(Id).Flag ^= SNodePinInstanceBuffer::HoveringFlag;
    m_NodePinPipline->GetVertexBuffer().Upload(Id);
}

void CNodeRenderer::SetNodePosition(size_t Id, const glm::vec2& Position) const
{
    MAKE_SURE(Id < m_IdCount)

    const auto NewPosition = m_CommonNodeSSBOBuffer->At<SCommonNodeSSBO>(Id).Position = Position;
    m_CommonNodeSSBOBuffer->Upload(Id);
}

glm::vec2 CNodeRenderer::MoveNode(size_t Id, const glm::vec2& Delta) const
{
    MAKE_SURE(Id < m_IdCount)

    const auto NewPosition = m_CommonNodeSSBOBuffer->At<SCommonNodeSSBO>(Id).Position += Delta;
    m_CommonNodeSSBOBuffer->Upload(Id);
    return NewPosition;
}

static constexpr glm::vec2 NodePinStartPosition { 10.0f, 30.0f };
static constexpr float NodeRadius { 8.0f };

size_t CNodeRenderer::AddInputPin(size_t Id, bool IsExecutionPin)
{
    MAKE_SURE(Id < m_IdCount)

    const auto PinCount = m_NodeResourcesHandles[Id].InputPins.size();
    const glm::vec2 InternalNodeOffset = { NodeRadius, PinCount * 3 * NodeRadius };
    const glm::vec2 NodeOffset = NodePinStartPosition + glm::vec2 { 0, NodeRadius * 2 } + InternalNodeOffset;

    if (m_NodeResourcesHandles[Id].InputPins.size() >= m_NodeResourcesHandles[Id].OutputPins.size()) {
        auto& NodeInstance = m_NodeBackgroundPipline->GetVertexBuffer().At<SNodeBackgroundInstanceBuffer>(Id);
        NodeInstance.Size = glm::max(NodeInstance.Size, { 0, NodeOffset.y + NodeRadius * 2 });
        m_NodeBackgroundPipline->GetVertexBuffer().Upload(Id);
    }

    return m_NodeResourcesHandles[Id].InputPins.emplace_back(m_NodePinPipline->NewPin(Id, NodeOffset, IsExecutionPin ? NodeRadius : NodeRadius * 0.6f, 0xFFFFFFFF, IsExecutionPin, false));
}

size_t CNodeRenderer::AddOutputPin(size_t Id, bool IsExecutionPin)
{
    MAKE_SURE(Id < m_IdCount)

    const auto PinCount = m_NodeResourcesHandles[Id].OutputPins.size();
    auto& NodeInstance = m_NodeBackgroundPipline->GetVertexBuffer().At<SNodeBackgroundInstanceBuffer>(Id);

    const glm::vec2 InternalNodeOffset = { NodeInstance.Size.x - NodeRadius * 3.5f, PinCount * 3 * NodeRadius };
    const glm::vec2 NodeOffset = NodePinStartPosition + glm::vec2 { 0, NodeRadius * 2 } + InternalNodeOffset;

    if (m_NodeResourcesHandles[Id].OutputPins.size() >= m_NodeResourcesHandles[Id].InputPins.size()) {
        NodeInstance.Size = glm::max(NodeInstance.Size, { 0, NodeOffset.y + NodeRadius * 2 });
        m_NodeBackgroundPipline->GetVertexBuffer().Upload(Id);
    }

    return m_NodeResourcesHandles[Id].OutputPins.emplace_back(m_NodePinPipline->NewPin(Id, NodeOffset, IsExecutionPin ? NodeRadius : NodeRadius * 0.6f, 0xFFFFFFFF, IsExecutionPin, false));
}

size_t CNodeRenderer::LinkVirtualPin(size_t Id1, size_t NodeId)
{
    return m_NodeConnectionPipline->AddConnection(
        { .StartInnerOffset = m_NodePinPipline->GetRenderMetas()[Id1].Offset,
            .EndInnerOffset = { },
            .StartNodeId = m_NodePinPipline->GetRenderMetas()[Id1].NodeId,
            .EndNodeId = static_cast<uint32_t>(NodeId) });
}

size_t CNodeRenderer::LinkPin(size_t Id1, size_t Id2)
{
    return m_NodeConnectionPipline->AddConnection(
        { .StartInnerOffset = m_NodePinPipline->GetRenderMetas()[Id1].Offset,
            .EndInnerOffset = m_NodePinPipline->GetRenderMetas()[Id2].Offset,
            .StartNodeId = m_NodePinPipline->GetRenderMetas()[Id1].NodeId,
            .EndNodeId = m_NodePinPipline->GetRenderMetas()[Id2].NodeId });
}

void CNodeRenderer::UnlinkPin(const size_t Id) noexcept
{
    m_NodeConnectionPipline->RemoveConnection(Id);
}

size_t CNodeRenderer::CreateVirtualNode(const glm::vec2& Position)
{
    const auto FreeId = NextFreeNode();
    WriteToNode(FreeId, "", Position, 0, glm::vec2 { 0 });
    return FreeId;
}

size_t CNodeRenderer::CreateNode(const std::string& Title, const glm::vec2& Position, const uint32_t HeaderColor)
{
    const auto FreeId = NextFreeNode();
    WriteToNode(FreeId, Title, Position, HeaderColor);
    return FreeId;
}

void CNodeRenderer::RemoveNode(const size_t Id)
{
    m_ValidIdRange.RemoveSlot(Id);
    m_NodeResourcesHandles[Id].UnRegister(this);
}

bool CNodeRenderer::InBound(const size_t Id, const glm::vec2& Position) const
{
    MAKE_SURE(Id < m_IdCount)

    const auto& Offset = m_CommonNodeSSBOBuffer->At<SCommonNodeSSBO>(Id).Position;
    const auto& Size = m_NodeBackgroundPipline->GetVertexBuffer().At<SNodeBackgroundInstanceBuffer>(Id).Size;

    return Position.x >= Offset.x && Position.y >= Offset.y && Position.x <= Offset.x + Size.x && Position.y <= Offset.y + Size.y;
}

std::optional<std::size_t> CNodeRenderer::GetHoveringPin(const size_t HoveringNodeId, const glm::vec2& Position) const
{
    MAKE_SURE(HoveringNodeId < m_IdCount)

    const auto& Offset = m_CommonNodeSSBOBuffer->At<SCommonNodeSSBO>(HoveringNodeId).Position;
    const auto InNodeOffset = Position - Offset;

    for (const auto PinId : std::views::join(std::array { std::views::all(m_NodeResourcesHandles[HoveringNodeId].InputPins), std::views::all(m_NodeResourcesHandles[HoveringNodeId].OutputPins) })) {
        const auto& NodeMeta = m_NodePinPipline->GetVertexBuffer().At<SNodePinInstanceBuffer>(PinId);
        if (glm::length2(InNodeOffset - NodeMeta.Offset) <= std::pow(NodeMeta.Radius + NodeRadius / 2, 2)) {
            return PinId;
        }
    }

    return std::nullopt;
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

    {
        RenderContext.RenderPassEncoder.SetVertexBuffer(0, m_NodePinPipline->GetVertexBuffer());
        RenderContext.RenderPassEncoder.SetPipeline(*m_NodePinPipline);
        for (auto const& [Left, Right] : m_NodePinPipline->GetDrawCommands()) {
            RenderContext.RenderPassEncoder.Draw(4, Right - Left + 1, 0, Left);
        }
    }

    {
        RenderContext.RenderPassEncoder.SetVertexBuffer(0, m_NodeConnectionPipline->GetVertexBuffer());
        RenderContext.RenderPassEncoder.SetPipeline(*m_NodeConnectionPipline);
        for (auto const& [Left, Right] : m_NodeConnectionPipline->GetDrawCommands()) {
            RenderContext.RenderPassEncoder.Draw((64 + 1) * 2, Right - Left + 1, 0, Left);
        }
    }
}
