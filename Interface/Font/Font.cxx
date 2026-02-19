//
// Created by LYS on 2/17/2026.
//

#include "Font.hxx"

#include <Interface/WindowBase.hxx>
#include <Util/Assertions.hxx>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <harfbuzz/hb-ft.h>
#include <harfbuzz/hb.h>

static_assert(std::is_same_v<hb_codepoint_t, uint32_t>);
static_assert(std::is_same_v<hb_position_t, int32_t>);

auto GetFreeTypeLibrary()
{
    static auto LibraryInstance = []() static {
        auto* NewLibrary = new FT_Library;
        MAKE_SURE(!FT_Init_FreeType(NewLibrary))

        return std::shared_ptr<FT_Library>(NewLibrary, [](const FT_Library* Library) static {
            FT_Done_FreeType(*Library);
            delete Library;
        });
    }();

    return LibraryInstance;
}

CFont::CFont(const CWindowBase* Window, const std::filesystem::path& FontPath)
    : m_Window(Window)
{
    MAKE_SURE(exists(FontPath))

    static_assert(std::is_same_v<FT_Face, FT_FaceRec_*>);

    m_FontLibrary = GetFreeTypeLibrary();

    FT_Face LocalFace;
    MAKE_SURE(!FT_New_Face(*m_FontLibrary, FontPath.string().c_str(), 0, &LocalFace), "Failed to load font: " + FontPath.string())
    m_FontFace = std::shared_ptr<FT_FaceRec_>(LocalFace, [](FT_FaceRec_* Face) static {
        FT_Done_Face(Face);
    });

    // set size to load glyphs as
    FT_Set_Pixel_Sizes(m_FontFace.get(), 0, 48);

    m_TextShaper = std::shared_ptr<hb_font_t>(hb_ft_font_create(m_FontFace.get(), nullptr), [](hb_font_t* Shaper) static {
        hb_font_destroy(Shaper);
    });

    /*
     *
     * Texture Atlas
     *
     */

    wgpu::TextureDescriptor FontAtlasTextureDesc;
    FontAtlasTextureDesc.dimension = wgpu::TextureDimension::e2D;
    FontAtlasTextureDesc.format = wgpu::TextureFormat::R8Unorm;
    FontAtlasTextureDesc.mipLevelCount = 1;
    FontAtlasTextureDesc.sampleCount = 1;
    FontAtlasTextureDesc.size = wgpu::Extent3D(TextureAtlasSize, TextureAtlasSize, 1);
    FontAtlasTextureDesc.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    m_Texture = m_Window->GetDevice().CreateTexture(&FontAtlasTextureDesc);

    wgpu::TextureViewDescriptor FontTextureViewDesc;
    FontTextureViewDesc.aspect = wgpu::TextureAspect::All;
    FontTextureViewDesc.baseArrayLayer = 0;
    FontTextureViewDesc.arrayLayerCount = 1;
    FontTextureViewDesc.baseMipLevel = 0;
    FontTextureViewDesc.mipLevelCount = 1;
    FontTextureViewDesc.dimension = wgpu::TextureViewDimension::e2D;
    FontTextureViewDesc.format = wgpu::TextureFormat::R8Unorm;
    m_TextureView = m_Texture.CreateView(&FontTextureViewDesc);

    // Create sampler
    wgpu::SamplerDescriptor samplerDesc = { };
    samplerDesc.addressModeU = wgpu::AddressMode::ClampToEdge;
    samplerDesc.addressModeV = wgpu::AddressMode::ClampToEdge;
    samplerDesc.addressModeW = wgpu::AddressMode::ClampToEdge;
    samplerDesc.magFilter = wgpu::FilterMode::Nearest;
    samplerDesc.minFilter = wgpu::FilterMode::Nearest;
    samplerDesc.mipmapFilter = wgpu::MipmapFilterMode::Nearest;
    samplerDesc.lodMinClamp = 0.0f;
    samplerDesc.lodMaxClamp = 1.0f;
    samplerDesc.compare = wgpu::CompareFunction::Undefined;

    m_Sampler = m_Window->GetDevice().CreateSampler(&samplerDesc);
}

SCharacter*
CFont::LoadCharacter(uint32_t Codepoint)
{
    // Check if glyph is already cached
    if (const auto CharacterCacheIt = m_CharacterCache.find(Codepoint); CharacterCacheIt == m_CharacterCache.end()) {
        // Not cached, so render and cache it
        VERIFY(!FT_Load_Glyph(m_FontFace.get(), Codepoint, FT_LOAD_RENDER), return nullptr)

        // If the glyph doesn't fit on the current row, move to the next one
        if (m_FontAtlasPenX + m_FontFace->glyph->bitmap.width >= TextureAtlasSize) {
            m_FontAtlasPenX = 0;
            m_FontAtlasPenY += m_FontAtlasMaxRowHeight + 1;
            m_FontAtlasMaxRowHeight = 0;
        }

        VERIFY(m_FontAtlasPenY + m_FontFace->glyph->bitmap.rows < TextureAtlasSize, return nullptr)

        // 4. Upload the glyph bitmap to the GPU texture atlas
        const wgpu::TexelCopyTextureInfo Destination = {
            .texture = m_Texture,
            .mipLevel = 0,
            .origin = { m_FontAtlasPenX, m_FontAtlasPenY, 0 },
            .aspect = wgpu::TextureAspect::All
        };

        const wgpu::TexelCopyBufferLayout DataLayout = {
            .offset = 0,
            .bytesPerRow = m_FontFace->glyph->bitmap.width,
            .rowsPerImage = m_FontFace->glyph->bitmap.rows
        };

        const wgpu::Extent3D WriteSize = { m_FontFace->glyph->bitmap.width, m_FontFace->glyph->bitmap.rows, 1 };

        // Write texture data
        m_Window->GetQueue().WriteTexture(
            &Destination,
            m_FontFace->glyph->bitmap.buffer,
            m_FontFace->glyph->bitmap.width * m_FontFace->glyph->bitmap.rows,
            &DataLayout,
            &WriteSize);

        // clang-format off
        auto* CharacterResult = &m_CharacterCache.emplace_hint(
            CharacterCacheIt, Codepoint,
            SCharacter {
                {                                         m_FontFace->glyph->bitmap.width,                                          m_FontFace->glyph->bitmap.rows},
                {                                          m_FontFace->glyph->bitmap_left,                                           m_FontFace->glyph->bitmap_top},
                {                static_cast<float>( m_FontAtlasPenX ) / TextureAtlasSize,                static_cast<float>( m_FontAtlasPenY ) / TextureAtlasSize},
                {static_cast<float>( m_FontFace->glyph->bitmap.width ) / TextureAtlasSize, static_cast<float>( m_FontFace->glyph->bitmap.rows ) / TextureAtlasSize}
        } )->second;
        // clang-format on

        m_FontAtlasPenX += m_FontFace->glyph->bitmap.width + 1;
        m_FontAtlasMaxRowHeight = std::max<int>(m_FontAtlasMaxRowHeight, m_FontFace->glyph->bitmap.rows);

        return CharacterResult;
    } else {
        return &CharacterCacheIt->second;
    }
}

float CFont::BuildVertex(const std::string& Text, float Scale, const std::function<STextVertexArchetype*(size_t)>& Allocator, size_t Stride)
{
    hb_buffer_t* hb_buffer = hb_buffer_create();
    hb_buffer_add_utf8(hb_buffer, Text.c_str(), -1, 0, -1);
    hb_buffer_guess_segment_properties(hb_buffer);

    hb_shape(m_TextShaper.get(), hb_buffer, nullptr, 0);

    unsigned int GlyphsCount;
    const hb_glyph_info_t* GlyphInfos = hb_buffer_get_glyph_infos(hb_buffer, &GlyphsCount);
    const hb_glyph_position_t* GlyphPositions = hb_buffer_get_glyph_positions(hb_buffer, &GlyphsCount);

    /// No text push empty draw command
    if (GlyphsCount == 0) {
        return 0;
    }

    auto* VertexArchetypeMemory = reinterpret_cast<std::byte*>(Allocator(GlyphsCount));

    float XPosition = 0, YPosition = 0;
    float MaxHeight = 0;
    float MaxYOutOfBound = 0;

    constexpr float Spacing = 5;

    for (unsigned int i = 0; i < GlyphsCount; i++) {
        const SCharacter* Character = LoadCharacter(GlyphInfos[i].codepoint);
        if (Character == nullptr)
            continue;

        const auto& glyph_pos = GlyphPositions[i];

        auto& VertexArchetype = *reinterpret_cast<STextVertexArchetype*>(VertexArchetypeMemory + Stride * i);

        VertexArchetype = {
            .TextBound = {
                // Convert HarfBuzz units (1/64th of a pixel) to pixels
                XPosition + (Character->Bearing.x + glyph_pos.x_offset / 64.0f) * Scale,
                YPosition - (Character->Bearing.y - glyph_pos.y_offset / 64.0f) * Scale,
                Character->Size.x * Scale,
                Character->Size.y * Scale,
            },
            .UVBound = { Character->UVPosition.x, Character->UVPosition.y, Character->UVSize.x, Character->UVSize.y }
        };

        MaxYOutOfBound = std::max(MaxYOutOfBound, -VertexArchetype.TextBound.y);
        MaxHeight = std::max(MaxHeight, VertexArchetype.TextBound.y + VertexArchetype.TextBound.w);

        // Convert HarfBuzz units (1/64th of a pixel) to pixels
        XPosition += (glyph_pos.x_advance / 64.0f + Spacing) * Scale;
        YPosition += (glyph_pos.y_advance / 64.0f) * Scale;
    }

    hb_buffer_destroy(hb_buffer);

    /// Offset back for max bearing so all text start at the desired Y position
    if ((MaxYOutOfBound = std::max(0.0f, MaxYOutOfBound)) > std::numeric_limits<float>::epsilon())
        for (int i = 0; i < GlyphsCount; i++)
            reinterpret_cast<STextVertexArchetype*>(VertexArchetypeMemory + Stride * i)->TextBound.y += MaxYOutOfBound;

    return std::max(0.f, XPosition - Spacing * Scale);
}