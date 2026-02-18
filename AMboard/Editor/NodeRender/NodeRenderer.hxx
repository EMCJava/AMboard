//
// Created by LYS on 2/18/2026.
//

#pragma once

#include "AMboard/Editor/BoardEditor.hxx"

#include <Util/RangeManager.hxx>

#include <glm/vec2.hpp>

#include <dawn/webgpu_cpp.h>

#include <memory>

struct SCommonNodeSSBO {
    glm::vec2 Position;
    uint32_t State;
    uint8_t Padding[4];
};

class CDynamicGPUBuffer;
class CNodeRenderer {

    void CreateCommonBindingGroup();

public:
    CNodeRenderer(const CWindowBase* Window);
    ~CNodeRenderer();

    void WriteToNode(size_t Id, const glm::vec2& Position, const glm::vec2& Size, uint32_t HeaderColor) const;

    void Select(size_t Id) const;
    void ToggleSelect(size_t Id) const;

    glm::vec2 MoveNode(size_t Id, const glm::vec2& Delta) const;

    size_t CreateNode(const glm::vec2& Position, const glm::vec2& Size, uint32_t HeaderColor);

    [[nodiscard]] bool InBound(size_t Id, const glm::vec2& Position) const noexcept;

    template <typename Self>
    [[nodiscard]] decltype(auto) GetValidRange(this Self&& s) noexcept { return s.m_ValidIdRange; }

    void Render(const SRenderContext& RenderContext);

protected:
    const CWindowBase* m_Window;

    size_t m_IdCount = 0;
    CRangeManager m_ValidIdRange;

    wgpu::BindGroup m_CommonNodeSSBOBindingGroup;
    std::unique_ptr<CDynamicGPUBuffer> m_CommonNodeSSBOBuffer;

    std::unique_ptr<class CNodeBackgroundPipline> m_NodeBackgroundPipline;
};
