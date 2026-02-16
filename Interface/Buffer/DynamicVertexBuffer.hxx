//
// Created by LYS on 2/17/2026.
//

#pragma once

#include <dawn/webgpu_cpp.h>

#include <memory>
#include <ranges>

class CDynamicVertexBuffer {

    CDynamicVertexBuffer(class CWindowBase* Window, size_t BytePerElement);

    void* PushVoidUninitialized();

public:
    ~CDynamicVertexBuffer();

    template <typename Ty>
    static auto Create(CWindowBase* Window)
    {
        return std::unique_ptr<CDynamicVertexBuffer> { new CDynamicVertexBuffer { Window, sizeof(Ty) } };
    }

    template <typename Ty, typename Self>
    auto GetView(this Self&& s)
    {
        return std::views::counted(reinterpret_cast<std::conditional_t<std::is_const_v<Self>, const Ty*, Ty*>>(s.m_Data.data()), s.m_EffectiveBufferSize);
    }

    template <typename Ty = void>
    Ty* PushUninitialized()
    {
        return static_cast<Ty*>(PushVoidUninitialized());
    }

    void Upload(std::size_t Offset, std::size_t Count = 1) noexcept;

    [[nodiscard]] auto GetBufferSize() const noexcept { return m_EffectiveBufferSize; }
    [[nodiscard]] auto GetBufferByteSize() const noexcept { return m_EffectiveBufferSize * m_BytePerElement; }
    [[nodiscard]] operator const wgpu::Buffer&() const& noexcept { return m_RenderBuffer; }

protected:
    uint64_t m_BytePerElement;
    uint64_t m_EffectiveBufferSize = 0;
    uint64_t m_LogicalBufferSize = 0;

    wgpu::Buffer m_RenderBuffer;
    std::vector<uint8_t> m_Data;

    CWindowBase* m_Window;
};
