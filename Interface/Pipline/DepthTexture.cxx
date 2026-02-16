//
// Created by LYS on 2/16/2026.
//

#include "DepthTexture.hxx"

#include <Interface/WindowBase.hxx>

CDepthTexture::CDepthTexture(const class CWindowBase& Window)
{
    {
        auto DepthTextureFormat = Window.GetDepthFormat();

        wgpu::TextureDescriptor depthTextureDesc;
        depthTextureDesc.dimension = wgpu::TextureDimension::e2D;
        depthTextureDesc.format = DepthTextureFormat;
        depthTextureDesc.mipLevelCount = 1;
        depthTextureDesc.sampleCount = 1;
        depthTextureDesc.size.width = Window.GetWindowSize().x;
        depthTextureDesc.size.height = Window.GetWindowSize().y;
        depthTextureDesc.usage = wgpu::TextureUsage::RenderAttachment;
        depthTextureDesc.viewFormatCount = 1;
        depthTextureDesc.viewFormats = &DepthTextureFormat;
        m_DepthTexture = Window.GetDevice().CreateTexture(&depthTextureDesc);

        wgpu::TextureViewDescriptor depthTextureViewDesc;
        depthTextureViewDesc.aspect = wgpu::TextureAspect::DepthOnly;
        depthTextureViewDesc.baseArrayLayer = 0;
        depthTextureViewDesc.arrayLayerCount = 1;
        depthTextureViewDesc.baseMipLevel = 0;
        depthTextureViewDesc.mipLevelCount = 1;
        depthTextureViewDesc.dimension = wgpu::TextureViewDimension::e2D;
        depthTextureViewDesc.format = DepthTextureFormat;
        m_DepthTextureView = m_DepthTexture.CreateView(&depthTextureViewDesc);
    }
}