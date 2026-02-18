//
// Created by LYS on 2/17/2026.
//

#include "TextRenderSystem.hxx"

#include "Font.hxx"

#include <Util/Assertions.hxx>

#include <Interface/Pipline/TextPipline.hxx>
#include <Interface/WindowBase.hxx>

#include <harfbuzz/hb-ft.h>
#include <harfbuzz/hb.h>

void CTextRenderSystem::BuildBindGroup()
{
    MAKE_SURE(m_Font && m_TextGroupBuffer)

    std::vector<wgpu::BindGroupEntry> bindings(3);
    // Binding 0: Texture view
    bindings[0].binding = 0;
    bindings[0].textureView = *m_Font;

    // Binding 1: Sampler
    bindings[1].binding = 1;
    bindings[1].sampler = *m_Font;

    // Binding 2: Text group SSBO
    bindings[2].binding = 2;
    bindings[2].buffer = *m_TextGroupBuffer;
    bindings[2].offset = 0;
    bindings[2].size = m_TextGroupBuffer->GetBufferByteSize();

    wgpu::BindGroupDescriptor bindGroupDesc { };
    bindGroupDesc.layout = m_Pipline->GetBindGroups()[1];
    bindGroupDesc.entryCount = bindings.size();
    bindGroupDesc.entries = bindings.data();
    m_TextBindingGroup = m_Window->GetDevice().CreateBindGroup(&bindGroupDesc);
}
void CTextRenderSystem::AddVertexRange(int Offset, int Count)
{
    auto Right = Offset + Count - 1;

    // 1. Check if we can merge with the RIGHT neighbor (starts at right + 1)
    auto right_it = m_DrawCommands.find(Right + 1);
    if (right_it != m_DrawCommands.end()) {
        Right = right_it->second; // Extend our new right to the neighbor's end
        m_DrawCommands.erase(right_it); // Remove the neighbor (we will re-insert the merged one)
    }

    // 2. Check if we can merge with the LEFT neighbor (ends at left - 1)
    // We look for the interval that starts before or at left.
    auto left_it = m_DrawCommands.upper_bound(Offset);

    if (left_it != m_DrawCommands.begin()) {
        left_it--; // Move back to the potential left neighbor

        // If the left neighbor ends exactly at left - 1, we merge
        if (left_it->second == Offset - 1) {
            left_it->second = Right; // Update the existing left node's end
            return; // Done (we extended the left node to cover the new range and potentially the right neighbor)
        }
    }

    // 3. If no left merge happened, insert the new range
    m_DrawCommands[Offset] = Right;
}

void CTextRenderSystem::RemoveVertexRange(int Offset, int Count)
{
    const auto Right = Offset + Count - 1;

    // Find the ONE interval that contains this range.
    // upper_bound(left) gives the first interval starting > left.
    // The interval containing 'left' must be the one immediately before that.
    auto it = m_DrawCommands.upper_bound(Offset);

    // Per constraints (valid API call), 'it' cannot be begin(),
    // because the range [left, right] must exist within an active slot.
    it--;

    int currentStart = it->first;
    int currentEnd = it->second;

    // We are modifying this node.
    // If we are cutting off the left side (start == left), we must erase the node
    // because we cannot change the Key of a map element in-place.
    // If we are keeping the start (start < left), we can just update the value.

    // 1. Handle the Right side remainder
    // If the removal stops before the end, we need to create a new interval for the remainder.
    if (currentEnd > Right) {
        m_DrawCommands[Right + 1] = currentEnd;
    }

    // 2. Handle the Left side
    if (currentStart < Offset) {
        // We keep the left part, just shorten the end
        it->second = Offset - 1;
    } else {
        // currentStart == left (Exact match on start)
        // We must remove the original node
        m_DrawCommands.erase(it);
    }
}

void CTextRenderSystem::RefreshBuffer(STextGroupHandle& TextGroupHandle)
{
    /// Free old buffer
    if (TextGroupHandle.GroupId != -1) {

        if (TextGroupHandle.VertexSpan.second != 0) {
            RemoveVertexRange(TextGroupHandle.VertexSpan.first, TextGroupHandle.VertexSpan.second);
            m_TextVertexBufferFreeList.push_back(TextGroupHandle.VertexSpan);
            TextGroupHandle.VertexSpan = { };
        }
    } else {
        /// Assign group
        if (!m_TextGroupBufferFreeList.empty()) {
            TextGroupHandle.GroupId = m_TextGroupBufferFreeList.top();
            m_TextGroupBufferFreeList.pop();
        } else {
            TextGroupHandle.GroupId = m_TextGroupBuffer->GetBufferSize();
            m_TextGroupBuffer->PushUninitialized<STextRenderPerGroupMeta>();
            BuildBindGroup();
        }
    }
}

void CTextRenderSystem::PushVertexBuffer(STextGroupHandle& TextGroupHandle)
{
    RefreshBuffer(TextGroupHandle);

    hb_buffer_t* hb_buffer = hb_buffer_create();
    hb_buffer_add_utf8(hb_buffer, TextGroupHandle.Text.c_str(), -1, 0, -1);
    hb_buffer_guess_segment_properties(hb_buffer);

    hb_shape(*m_Font, hb_buffer, nullptr, 0);

    unsigned int GlyphsCount;
    const hb_glyph_info_t* GlyphInfos = hb_buffer_get_glyph_infos(hb_buffer, &GlyphsCount);
    const hb_glyph_position_t* GlyphPositions = hb_buffer_get_glyph_positions(hb_buffer, &GlyphsCount);

    /// No text push empty draw command
    if (GlyphsCount == 0) {
        return;
    }

    CHECK(TextGroupHandle.VertexSpan.second == 0)
    for (auto it = m_TextVertexBufferFreeList.begin(); it != m_TextVertexBufferFreeList.end(); ++it) {
        if (it->second >= GlyphsCount) {
            TextGroupHandle.VertexSpan = *it;
            m_TextVertexBufferFreeList.erase(it);
            break;
        }
    }

    /// Reallocate is needed
    if (TextGroupHandle.VertexSpan.second == 0) {
        const auto OldVertexCount = m_TextVertexBuffer->GetBufferSize();
        m_TextVertexBuffer->Resize(OldVertexCount + GlyphsCount);
        TextGroupHandle.VertexSpan = { OldVertexCount, GlyphsCount };
    }

    if (TextGroupHandle.VertexSpan.second > GlyphsCount) {
        m_TextVertexBufferFreeList.emplace_back(TextGroupHandle.VertexSpan.first + GlyphsCount, GlyphsCount - TextGroupHandle.VertexSpan.second);
        TextGroupHandle.VertexSpan.second = GlyphsCount;
    }

    VERIFY(TextGroupHandle.VertexSpan.second == GlyphsCount)

    AddVertexRange(TextGroupHandle.VertexSpan.first, TextGroupHandle.VertexSpan.second);
    const auto GlyphsSubspan = m_TextVertexBuffer->GetView<STextRenderVertexMeta>().subspan(TextGroupHandle.VertexSpan.first, GlyphsCount);

    auto XPosition = 0, YPosition = 0;
    float MaxHeight = 0;
    float MaxYOutOfBound = 0;

    for (unsigned int i = 0; i < GlyphsCount; i++) {
        const SCharacter* Character = m_Font->LoadCharacter(GlyphInfos[i].codepoint);
        if (Character == nullptr)
            continue;

        const auto& glyph_pos = GlyphPositions[i];

        GlyphsSubspan[i] = {
            .TextBound = {
                // Convert HarfBuzz units (1/64th of a pixel) to pixels
                XPosition + (Character->Bearing.x + glyph_pos.x_offset / 64.0f) * TextGroupHandle.Scale,
                YPosition - (Character->Bearing.y - glyph_pos.y_offset / 64.0f) * TextGroupHandle.Scale,
                Character->Size.x * TextGroupHandle.Scale,
                Character->Size.y * TextGroupHandle.Scale,
            },
            .UVBound = { Character->UVPosition.x, Character->UVPosition.y, Character->UVSize.x, Character->UVSize.y },
            .TextGroupIndex = TextGroupHandle.GroupId
        };

        MaxYOutOfBound = std::max(MaxYOutOfBound, -GlyphsSubspan[i].TextBound.y);
        MaxHeight = std::max(MaxHeight, GlyphsSubspan[i].TextBound.y + GlyphsSubspan[i].TextBound.w);

        // Convert HarfBuzz units (1/64th of a pixel) to pixels
        XPosition += (glyph_pos.x_advance / 64.0f + 5) * TextGroupHandle.Scale;
        YPosition += (glyph_pos.y_advance / 64.0f) * TextGroupHandle.Scale;
    }

    hb_buffer_destroy(hb_buffer);

    /// Offset back for max bearing so all text start at the desired Y position
    if ((MaxYOutOfBound = std::max(0.0f, MaxYOutOfBound)) > std::numeric_limits<float>::epsilon())
        for (int i = 0; i < GlyphsCount; i++)
            GlyphsSubspan[i].TextBound.y += MaxYOutOfBound;

    (void)m_TextVertexBuffer->Upload(GlyphsSubspan);
}

void CTextRenderSystem::PushPerGroupBuffer(STextGroupHandle& TextGroupHandle)
{
    MAKE_SURE(TextGroupHandle.GroupId != -1)
    m_TextGroupBuffer->Upload(TextGroupHandle.GroupId);
}

CTextRenderSystem::~CTextRenderSystem() = default;

CTextRenderSystem::CTextRenderSystem(const CWindowBase* Window)
    : m_Window(Window)
    , m_TextGroupBuffer(CDynamicVertexBuffer::Create<STextRenderPerGroupMeta>(Window, wgpu::BufferUsage::Storage))
    , m_TextVertexBuffer(CDynamicVertexBuffer::Create<STextRenderVertexMeta>(Window))
{
    m_Pipline = std::make_unique<CTextPipline>();
    m_Pipline->CreatePipeline(*Window);
}

CTextRenderSystem::CTextRenderSystem(const CWindowBase* Window, std::shared_ptr<CFont> Font)
    : CTextRenderSystem(Window)
{
    m_Font = std::move(Font);
}

CTextRenderSystem::CTextRenderSystem(const CWindowBase* Window, const std::filesystem::path& FontPath)
    : CTextRenderSystem(Window, std::make_shared<CFont>(Window, FontPath))
{
}

void CTextRenderSystem::SetTextPosition(const std::list<STextGroupHandle>::iterator& Group, const glm::vec2& Position)
{
    MAKE_SURE(Group->GroupId != -1)
    m_TextGroupBuffer->At<STextRenderPerGroupMeta>(Group->GroupId).Offset = Position;
    PushPerGroupBuffer(*Group);
}

void CTextRenderSystem::Render(const SRenderContext& RenderContext)
{
    if (m_DrawCommands.empty()) {
        return;
    }

    RenderContext.RenderPassEncoder.SetBindGroup(1, m_TextBindingGroup, 0, nullptr);
    RenderContext.RenderPassEncoder.SetVertexBuffer(0, *m_TextVertexBuffer);
    RenderContext.RenderPassEncoder.SetPipeline(*m_Pipline);

    for (auto const& [Left, Right] : m_DrawCommands) {
        RenderContext.RenderPassEncoder.Draw(4, Right - Left + 1, 0, Left);
    }
}

std::list<STextGroupHandle>::iterator CTextRenderSystem::RegisterTextGroup(std::string Text, const float Scale, const STextRenderPerGroupMeta& Meta)
{
    auto Result = m_RegisteredTextGroups.emplace(m_RegisteredTextGroups.end(), std::move(Text), Scale);

    RefreshBuffer(*Result);
    MAKE_SURE(Result->GroupId != -1)
    m_TextGroupBuffer->At<STextRenderPerGroupMeta>(Result->GroupId) = Meta;

    UpdateTextGroup(Result);
    return Result; // NOLINT
}

void CTextRenderSystem::UpdateTextGroup(const std::list<STextGroupHandle>::iterator& Group)
{
    PushVertexBuffer(*Group);
    PushPerGroupBuffer(*Group);
}

void CTextRenderSystem::UpdateGroupMeta(const std::list<STextGroupHandle>::iterator& Group)
{
    PushPerGroupBuffer(*Group);
}

void CTextRenderSystem::UnregisterTextGroup(const std::list<STextGroupHandle>::iterator& Group)
{
    if (Group->GroupId != -1) {
        RefreshBuffer(*Group);
        m_TextGroupBufferFreeList.push(Group->GroupId);
    }
}