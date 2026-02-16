//
// Created by LYS on 2/16/2026.
//

#pragma once

#include <dawn/webgpu_cpp.h>

class CDepthTexture {

public:
    CDepthTexture(const class CWindowBase& Window);

    [[nodiscard]] operator const wgpu::TextureView&() const noexcept { return m_DepthTextureView; }

private:
    wgpu::Texture m_DepthTexture; // Created with pipline
    wgpu::TextureView m_DepthTextureView; // Created with pipline
};
