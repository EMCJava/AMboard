//
// Created by LYS on 2/19/2026.
//

#pragma once

#include <Util/RangeManager.hxx>

#include <Interface/Pipline/RenderPipeline.hxx>

struct SNodePinInstanceBuffer {
    glm::vec2 Offset;
    float Radius;
    uint32_t Color;
    uint32_t Flag;
    uint32_t NodeId;

    enum {
        EExecutionFlag = 0b1,
        ConnectedFlag = 0b10,
        HoveringFlag = 0b100,
    };
};

class CNodePinPipline final : public CRenderPipeline {

public:
    CNodePinPipline(CWindowBase* Window);
    ~CNodePinPipline();

    template <typename Self>
    auto GetRenderMetas(this Self&& s) { return s.m_RenderVertexBuffer->template GetView<SNodePinInstanceBuffer>(); }
    template <typename Self>
    decltype(auto) GetVertexBuffer(this Self&& s) { return *s.m_RenderVertexBuffer; }
    template <typename Self>
    decltype(auto) GetDrawCommands(this Self&& s) noexcept { return s.m_VertexBufferRangeManager; }

    size_t NewPin(uint32_t NodeId, const glm::vec2& Offset, float Radius, uint32_t Color, bool IsExecution, bool IsConnected);
    void RemovePin(uint32_t Index) noexcept;

protected:
    [[nodiscard]] std::vector<SVertexBufferMeta> GetVertexBufferMeta() const override;
    [[nodiscard]] std::vector<wgpu::BindGroupLayout> CreateBindingGroupLayout(const wgpu::Device& Device) override;

    CRangeManager m_VertexBufferRangeManager;
    std::unique_ptr<class CDynamicGPUBuffer> m_RenderVertexBuffer;
};
