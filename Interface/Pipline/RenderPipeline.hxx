//
// Created by LYS on 12/18/2025.
//

#pragma once

#include <dawn/webgpu_cpp.h>

#include <glm/mat4x4.hpp>

struct SSceneUniform {
    glm::mat4 View { 1 };
    glm::mat4 Projection { 1 };
};

struct SVertexBufferMeta {
    wgpu::VertexBufferLayout Layout;
    std::vector<wgpu::VertexAttribute> Attributes;
};

class CRenderPipeline {
protected:
    static wgpu::ShaderModule CompileShaderFromCode(const wgpu::Device& Device, const std::string_view& Code);

    void DefineSceneUniforms(wgpu::BindGroupLayoutEntry& Entry);

    virtual wgpu::ShaderModule CompileShader(const wgpu::Device& Device);
    virtual std::vector<wgpu::BindGroupLayout> CreateBindingGroupLayout(const wgpu::Device& Device);
    virtual std::vector<SVertexBufferMeta> GetVertexBufferMeta() const { return { }; }

public:
    CRenderPipeline();
    ~CRenderPipeline();

    void SetShaderCode(const std::string_view Code) noexcept { m_ShaderCode = Code; }
    void CreatePipeline(const class CWindowBase& Window, bool ForceRecreate = true);

    [[nodiscard]] auto& GetBindGroups() const noexcept { return m_BindingGroupLayouts; }
    [[nodiscard]] operator const wgpu::RenderPipeline&() const noexcept { return m_Pipline; }

protected:
    std::vector<wgpu::BindGroupLayout> m_BindingGroupLayouts;

    wgpu::RenderPipeline m_Pipline;

    std::string_view m_ShaderCode;
};
