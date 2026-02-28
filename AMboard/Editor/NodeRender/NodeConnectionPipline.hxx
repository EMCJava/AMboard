//
// Created by LYS on 2/19/2026.
//

#pragma once

#include <Util/RangeManager.hxx>

#include <Interface/Pipline/RenderPipeline.hxx>

#include <glm/vec2.hpp>

struct SNodeConnectionInstanceBuffer {
    glm::vec2 StartInnerOffset;
    glm::vec2 EndInnerOffset;
    uint32_t StartNodeId;
    uint32_t EndNodeId;
};

class CNodeConnectionPipline : public CRenderPipeline {
public:
    CNodeConnectionPipline(const CWindowBase* Window);
    ~CNodeConnectionPipline();

    size_t AddConnection(const SNodeConnectionInstanceBuffer& Connection);
    void UpdateConnection(size_t Id, const SNodeConnectionInstanceBuffer& Connection);
    void RemoveConnection(size_t Id) noexcept;

    template <typename Self>
    auto GetRenderMetas(this Self&& s) { return s.m_RenderVertexBuffer->template GetView<SNodeConnectionInstanceBuffer>(); }
    template <typename Self>
    decltype(auto) GetVertexBuffer(this Self&& s) { return *s.m_RenderVertexBuffer; }
    template <typename Self>
    decltype(auto) GetDrawCommands(this Self&& s) noexcept { return s.m_VertexBufferRangeManager; }

protected:
    [[nodiscard]] std::vector<SVertexBufferMeta> GetVertexBufferMeta() const override;
    [[nodiscard]] std::vector<wgpu::BindGroupLayout> CreateBindingGroupLayout(const wgpu::Device& Device) override;

    CRangeManager m_VertexBufferRangeManager;
    std::unique_ptr<class CDynamicGPUBuffer> m_RenderVertexBuffer;
};
