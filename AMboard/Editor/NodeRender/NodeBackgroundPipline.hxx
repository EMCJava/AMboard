//
// Created by LYS on 2/17/2026.
//

#pragma once

#include <Interface/Buffer/DynamicGPUBuffer.hxx>
#include <Interface/Pipline/RenderPipeline.hxx>

struct SNodeBackgroundInstanceBuffer {
    glm::vec2 Size;
    uint32_t HeaderColor;
};

class CNodeBackgroundPipline : public CRenderPipeline {

public:
    CNodeBackgroundPipline(CWindowBase* Window);

    template <typename Self>
    auto GetRenderMetas(this Self&& s)
    {
        return s.m_RenderVertexBuffer->template GetView<SNodeBackgroundInstanceBuffer>();
    }

    template <typename Self>
    decltype(auto) GetVertexBuffer(this Self&& s)
    {
        return *s.m_RenderVertexBuffer;
    }

protected:
    [[nodiscard]] std::vector<SVertexBufferMeta> GetVertexBufferMeta() const override;
    [[nodiscard]] std::vector<wgpu::BindGroupLayout> CreateBindingGroupLayout(const wgpu::Device& Device) override;

    std::unique_ptr<CDynamicGPUBuffer> m_RenderVertexBuffer;
};
