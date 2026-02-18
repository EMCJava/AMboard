//
// Created by LYS on 2/18/2026.
//

#pragma once

#include <Util/RangeManager.hxx>

#include <Interface/Pipline/RenderPipeline.hxx>
#include <Interface/Pipline/TextRenderMeta.hxx>

#include <Interface/Font/TextRenderSystemHandle.hxx>

#include <stack>

struct SNodeTextPerGroupMeta {
    uint32_t Color { };
};

class CNodeTextRenderPipline : public CRenderPipeline {

    void CreateCommonBindingGroup();
    void RefreshBuffer(STextGroupHandle& TextGroupHandle);

    void PushVertexBuffer(STextGroupHandle& TextGroupHandle);
    void PushPerGroupBuffer(STextGroupHandle& TextGroupHandle);

public:
    CNodeTextRenderPipline(const CWindowBase* Window, std::shared_ptr<class CFont> Font);
    ~CNodeTextRenderPipline();

    std::list<STextGroupHandle>::iterator RegisterTextGroup(size_t NodeId, std::string Text, float Scale = 1, const SNodeTextPerGroupMeta& Meta = { });
    void UpdateTextGroup(const std::list<STextGroupHandle>::iterator& Group);
    void UnregisterTextGroup(const std::list<STextGroupHandle>::iterator& Group);

    template <typename Self>
    auto GetRenderMetas(this Self&& s) noexcept { return s.m_TextVertexBuffer->template GetView<STextRenderVertexMeta>(); }
    template <typename Self>
    auto GetInstanceMetas(this Self&& s) noexcept { return s.m_RenderGroupBuffer->template GetView<SNodeTextPerGroupMeta>(); }
    template <typename Self>
    decltype(auto) GetVertexBuffer(this Self&& s) noexcept { return *s.m_TextVertexBuffer; }
    template <typename Self>
    decltype(auto) GetGroupBuffer(this Self&& s) noexcept { return *s.m_RenderGroupBuffer; }
    template <typename Self>
    decltype(auto) GetBindingGroup(this Self&& s) noexcept { return s.m_TextGroupSSBOBindingGroup; }
    template <typename Self>
    decltype(auto) GetDrawCommands(this Self&& s) noexcept { return s.m_VertexBufferRangeManager; }

protected:
    const CWindowBase* m_Window;

    std::vector<wgpu::BindGroupLayout> CreateBindingGroupLayout(const wgpu::Device& Device) override;
    std::vector<SVertexBufferMeta> GetVertexBufferMeta() const override;

    CRangeManager m_VertexBufferRangeManager;
    std::unique_ptr<class CDynamicGPUBuffer> m_TextVertexBuffer;

    wgpu::BindGroup m_TextGroupSSBOBindingGroup;
    std::stack<size_t> m_TextGroupBufferFreeList;
    std::list<STextGroupHandle> m_RegisteredTextGroup;
    std::unique_ptr<CDynamicGPUBuffer> m_TextGroupBuffer;

    std::shared_ptr<class CFont> m_Font;
};
