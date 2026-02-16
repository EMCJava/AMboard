//
// Created by LYS on 2/17/2026.
//

#include <utility>

#include "DynamicVertexBuffer.hxx"

#include <Interface/WindowBase.hxx>

CDynamicVertexBuffer::CDynamicVertexBuffer(CWindowBase* Window, const size_t BytePerElement)
    : m_BytePerElement(BytePerElement)
    , m_Window(Window)
{
}

void* CDynamicVertexBuffer::PushVoidUninitialized()
{
    if (++m_EffectiveBufferSize > m_LogicalBufferSize) {
        m_LogicalBufferSize = m_EffectiveBufferSize;

        wgpu::BufferDescriptor bufferDesc;

        bufferDesc.label = "DynamicVertexBuffer";
        bufferDesc.size = m_LogicalBufferSize * m_BytePerElement;
        bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex;

        m_RenderBuffer = m_Window->GetDevice().CreateBuffer(&bufferDesc);
        m_Data.resize(bufferDesc.size);
    }

    return m_Data.data() + (m_EffectiveBufferSize - 1) * m_BytePerElement;
}

CDynamicVertexBuffer::~CDynamicVertexBuffer()
{
    auto a = 0;
}

void CDynamicVertexBuffer::Upload(const std::size_t Offset, const std::size_t Count) noexcept
{
    m_Window->GetQueue().WriteBuffer(m_RenderBuffer, Offset * m_BytePerElement, m_Data.data() + Offset * m_BytePerElement, Count * m_BytePerElement);
}