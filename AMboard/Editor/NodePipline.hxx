//
// Created by LYS on 2/17/2026.
//

#pragma once

#include <Interface/Buffer/DynamicVertexBuffer.hxx>
#include <Interface/Pipline/RenderPipeline.hxx>

struct SNodeBackgroundRenderMeta {

    glm::vec2 Size;
    glm::vec2 Offset;
    uint32_t HeaderColor;
    uint32_t State;
};

class CNodePipline : public CRenderPipeline {

public:
    CNodePipline(CWindowBase* Window);

    template <typename Self>
    auto GetRenderMetas(this Self&& s)
    {
        return s.m_RenderVertexBuffer->template GetView<SNodeBackgroundRenderMeta>();
    }

    template <typename Self>
    decltype(auto) GetVertexBuffer(this Self&& s)
    {
        return *s.m_RenderVertexBuffer;
    }

protected:
    [[nodiscard]] std::vector<SVertexBufferMeta> GetVertexBufferMeta() const override;
    std::unique_ptr<CDynamicVertexBuffer> m_RenderVertexBuffer;
};
