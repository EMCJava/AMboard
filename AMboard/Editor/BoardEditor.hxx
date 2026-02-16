//
// Created by LYS on 2/16/2026.
//

#pragma once

#include <Interface/WindowBase.hxx>

#include <memory>
#include <vector>

#include <glm/vec2.hpp>

class CBoardEditor : public CWindowBase {

    void RenderNodes(const SRenderContext& RenderContext) const;

public:
    CBoardEditor();
    ~CBoardEditor();

    void RenderBoard(const SRenderContext& RenderContext);

protected:
    std::unique_ptr<class CNodePipline> m_NodePipline;
    std::vector<std::unique_ptr<class CBaseNode>> m_Nodes;
};
