//
// Created by LYS on 2/17/2026.
//

#pragma once

#include <Interface/Buffer/DynamicVertexBuffer.hxx>
#include <Interface/Pipline/TextRenderMeta.hxx>

#include <filesystem>
#include <map>
#include <stack>

#include <dawn/webgpu_cpp.h>

class CTextRenderSystem;
struct STextGroupHandle {

    std::string Text;
    float Scale;

    std::pair<size_t, size_t> VertexSpan { };
    uint32_t GroupId = -1;
};

class CTextRenderSystem {

    void BuildBindGroup();

    void AddVertexRange(int Offset, int Count);
    void RemoveVertexRange(int Offset, int Count);
    void RefreshBuffer(STextGroupHandle& TextGroupHandle);

    void PushVertexBuffer(STextGroupHandle& TextGroupHandle);
    void PushPerGroupBuffer(STextGroupHandle& TextGroupHandle);

public:
    ~CTextRenderSystem();
    CTextRenderSystem(const CWindowBase* Window);
    CTextRenderSystem(const CWindowBase* Window, std::shared_ptr<class CFont> Font);
    CTextRenderSystem(const CWindowBase* Window, const std::filesystem::path& FontPath);

    void Render(const struct SRenderContext& RenderContext);

    std::list<STextGroupHandle>::iterator RegisterTextGroup(std::string Text, float Scale = 1, const STextRenderPerGroupMeta& Meta = { });
    void UpdateTextGroup(const std::list<STextGroupHandle>::iterator& Group);
    void UpdateGroupMeta(const std::list<STextGroupHandle>::iterator& Group);
    void UnregisterTextGroup(const std::list<STextGroupHandle>::iterator& Group);
protected:
    const CWindowBase* m_Window;

    wgpu::BindGroup m_TextBindingGroup;

    std::unique_ptr<CDynamicVertexBuffer> m_TextVertexBuffer;
    std::unique_ptr<CDynamicVertexBuffer> m_TextGroupBuffer;

    std::list<std::pair<size_t, size_t>> m_TextVertexBufferFreeList;
    std::stack<size_t> m_TextGroupBufferFreeList;

    std::list<STextGroupHandle> m_RegisteredTextGroups;

    std::map<int, int> m_DrawCommands;

    std::unique_ptr<class CTextPipline> m_Pipline;
    std::shared_ptr<CFont> m_Font;
};
