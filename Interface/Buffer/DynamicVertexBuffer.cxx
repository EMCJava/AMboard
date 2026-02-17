//
// Created by LYS on 2/17/2026.
//

#include <utility>

#include "DynamicVertexBuffer.hxx"

#include <Interface/WindowBase.hxx>

CDynamicVertexBuffer::CDynamicVertexBuffer(const CWindowBase* Window, const size_t BytePerElement, wgpu::BufferUsage ExtraUsage)
    : m_BytePerElement(BytePerElement)
    , m_Window(Window)
{
    m_BufferUsage |= ExtraUsage;
}

void* CDynamicVertexBuffer::PushVoidUninitialized()
{
    if (m_EffectiveBufferSize + 1 > m_LogicalBufferSize)
        Resize(m_EffectiveBufferSize + 1);

    return m_Data.data() + (m_EffectiveBufferSize - 1) * m_BytePerElement;
}

CDynamicVertexBuffer::~CDynamicVertexBuffer()
{
}

void CDynamicVertexBuffer::Resize(std::size_t Count)
{
    m_LogicalBufferSize = Count;

    wgpu::BufferDescriptor bufferDesc;

    bufferDesc.label = "DynamicVertexBuffer";
    bufferDesc.size = m_LogicalBufferSize * m_BytePerElement;
    bufferDesc.usage = m_BufferUsage;

    m_RenderBuffer = m_Window->GetDevice().CreateBuffer(&bufferDesc);
    m_Data.resize(bufferDesc.size);

    Upload(0, m_EffectiveBufferSize);
    m_EffectiveBufferSize = Count;
}

void CDynamicVertexBuffer::Upload(const std::size_t Offset, const std::size_t Count) const noexcept
{
    m_Window->GetQueue().WriteBuffer(m_RenderBuffer, Offset * m_BytePerElement, m_Data.data() + Offset * m_BytePerElement, Count * m_BytePerElement);
}