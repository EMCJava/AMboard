//
// Created by LYS on 2/17/2026.
//

#pragma once

#include <filesystem>
#include <unordered_map>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <dawn/webgpu_cpp.h>

struct FT_LibraryRec_;
struct FT_FaceRec_;
struct hb_font_t;

struct SCharacter {
    glm::ivec2 Size;
    glm::ivec2 Bearing;
    glm::vec2 UVPosition;
    glm::vec2 UVSize;
};

struct STextVertexArchetype {
    glm::vec4 TextBound;
    glm::vec4 UVBound;
};

class CFont {
public:
    CFont(const class CWindowBase* Window, const std::filesystem::path& FontPath);

    SCharacter* LoadCharacter(uint32_t Codepoint);

    float BuildVertex(const std::string& Text, float Scale, const std::function<STextVertexArchetype*(size_t)>& Allocator, size_t Stride = sizeof(STextVertexArchetype));

    operator hb_font_t*() const noexcept { return m_TextShaper.get(); }
    operator const wgpu::TextureView&() const noexcept { return m_TextureView; }
    operator const wgpu::Sampler&() const noexcept { return m_Sampler; }

private:
    const CWindowBase* m_Window;

    /// Hold a reference to avoid getting released
    std::shared_ptr<FT_LibraryRec_*> m_FontLibrary;

    std::shared_ptr<FT_FaceRec_> m_FontFace;
    std::shared_ptr<hb_font_t> m_TextShaper;

    // Cache for rendered glyphs
    std::unordered_map<uint32_t /* Codepoint */, SCharacter> m_CharacterCache;

    static constexpr auto TextureAtlasSize = 1024;
    wgpu::Texture m_Texture;
    wgpu::TextureView m_TextureView;
    wgpu::Sampler m_Sampler;

    // Atlas packing state
    uint32_t m_FontAtlasPenX = 0;
    uint32_t m_FontAtlasPenY = 0;
    int m_FontAtlasMaxRowHeight = 0; // Current max height of glyphs in the current row, we will jump over it next time
};