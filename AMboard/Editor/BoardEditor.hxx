//
// Created by LYS on 2/16/2026.
//

#pragma once

#include "BoardEditor.hxx"

#include <Interface/WindowBase.hxx>

#include <memory>
#include <vector>

#include <glm/vec2.hpp>

class CBoardEditor : public CWindowBase {

    void RenderNodes(const SRenderContext& RenderContext) const;

    [[nodiscard]] glm::vec2 ScreenToWorld(const glm::vec2& ScreenPos) const noexcept;

public:
    CBoardEditor();
    ~CBoardEditor();

    EWindowEventState ProcessEvent() override;

    void RenderBoard(const SRenderContext& RenderContext);

protected:
    bool m_ScreenUniformDirty = true;
    std::unique_ptr<class SSceneUniform> m_SceneUniform;
    wgpu::Buffer m_SceneUniformBuffer;
    wgpu::BindGroup m_UniformBindingGroup;

    std::unique_ptr<class CGridPipline> m_GridPipline;
    std::unique_ptr<class CNodePipline> m_NodePipline;
    std::vector<std::unique_ptr<class CBaseNode>> m_Nodes;

    glm::vec2 m_CameraOffset { };
    float m_CameraZoom = 1;
};
