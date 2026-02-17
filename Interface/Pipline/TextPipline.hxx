//
// Created by LYS on 2/17/2026.
//

#pragma once

#include "RenderPipeline.hxx"

class CTextPipline : public CRenderPipeline {

protected:
    std::vector<wgpu::BindGroupLayout> CreateBindingGroupLayout(const wgpu::Device& Device) override;
    std::vector<SVertexBufferMeta> GetVertexBufferMeta() const override;

public:
    CTextPipline();
};
