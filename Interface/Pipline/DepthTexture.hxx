//
// Created by LYS on 2/16/2026.
//

#pragma once

#include <dawn/webgpu_cpp.h>

#include <Interface/WindowResizeReactor.hxx>

class CDepthTexture : public CWindowResizeReactor<CDepthTexture> {

    friend CWindowResizeReactor;
    void ResizeCallback(const CWindowBase* Window);

public:
    CDepthTexture(CWindowBase& Window);
    ~CDepthTexture();

    [[nodiscard]] operator const wgpu::TextureView&() const noexcept { return m_DepthTextureView; }

private:
    wgpu::Texture m_DepthTexture; // Created with pipline
    wgpu::TextureView m_DepthTextureView; // Created with pipline

    std::list<std::function<void(CWindowBase* Window)>>::iterator m_CallbackIterator;
};
