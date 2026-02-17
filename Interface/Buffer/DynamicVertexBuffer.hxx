//
// Created by LYS on 2/17/2026.
//

#pragma once

#include <dawn/webgpu_cpp.h>

#include <memory>
#include <ranges>
#include <stdexcept>

class CDynamicVertexBuffer {

    CDynamicVertexBuffer(const class CWindowBase* Window, size_t BytePerElement, wgpu::BufferUsage ExtraUsage);

    void* PushVoidUninitialized();

public:
    ~CDynamicVertexBuffer();

    template <typename Ty>
    static auto Create(const CWindowBase* Window, const wgpu::BufferUsage ExtraUsage = wgpu::BufferUsage::Vertex)
    {
        return std::unique_ptr<CDynamicVertexBuffer> { new CDynamicVertexBuffer { Window, sizeof(Ty), ExtraUsage } };
    }

    template <typename Ty, typename Self>
    decltype(auto) At(this Self&& s, auto Offset)
    {
        return *(reinterpret_cast<std::conditional_t<std::is_const_v<Self>, const Ty*, Ty*>>(s.m_Data.data()) + Offset);
    }

    template <typename Ty, typename Self>
    auto Begin(this Self&& s)
    {
        return reinterpret_cast<std::conditional_t<std::is_const_v<Self>, const Ty*, Ty*>>(s.m_Data.data());
    }

    template <typename Ty, typename Self>
    auto GetView(this Self&& s)
    {
        return std::views::counted(s.template Begin<Ty>(), s.m_EffectiveBufferSize);
    }

    template <typename Ty = void>
    Ty* PushUninitialized()
    {
        return static_cast<Ty*>(PushVoidUninitialized());
    }

    void Resize(std::size_t Count);

    void Upload(std::size_t Offset, std::size_t Count = 1) const noexcept;

    template <typename Ty>
    auto Upload(Ty* Value) const noexcept
    {
#if !defined(NDEBUG)
        if (sizeof(Ty) != m_BytePerElement) [[unlikely]] {
            throw std::runtime_error { "Upload data with different size." };
        }
#endif

        auto Offset = std::distance(reinterpret_cast<std::add_const_t<Ty>*>(m_Data.data()), static_cast<std::add_const_t<Ty>*>(Value));
        Upload(Offset);
        return Offset;
    }

    template <typename Ty>
    auto Upload(const std::span<Ty>& Span) const noexcept
    {
#if !defined(NDEBUG)
        if (sizeof(Ty) != m_BytePerElement) [[unlikely]] {
            throw std::runtime_error { "Upload data with different size." };
        }
#endif

        auto Offset = std::distance(reinterpret_cast<std::add_const_t<Ty>*>(m_Data.data()), static_cast<std::add_const_t<Ty>*>(Span.data()));
        Upload(Offset, Span.size());
        return Offset;
    }

    [[nodiscard]] auto GetBufferSize() const noexcept { return m_EffectiveBufferSize; }
    [[nodiscard]] auto GetBufferByteSize() const noexcept { return m_EffectiveBufferSize * m_BytePerElement; }
    [[nodiscard]] operator const wgpu::Buffer&() const& noexcept { return m_RenderBuffer; }

protected:
    wgpu::BufferUsage m_BufferUsage = wgpu::BufferUsage::CopyDst;

    uint64_t m_BytePerElement;
    uint64_t m_EffectiveBufferSize = 0;
    uint64_t m_LogicalBufferSize = 0;

    wgpu::Buffer m_RenderBuffer;
    std::vector<uint8_t> m_Data;

    const CWindowBase* m_Window;
};
