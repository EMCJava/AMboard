//
// Created by LYS on 12/18/2025.
//

#include "RenderPipeline.hxx"

#include <Interface/WindowBase.hxx>

#include <dawn/webgpu_cpp.h>

CRenderPipeline::CRenderPipeline() = default;
CRenderPipeline::~CRenderPipeline() = default;

void CRenderPipeline::CreatePipeline(const CWindowBase& Window, bool ForceRecreate)
{
    wgpu::RenderPipelineDescriptor pipelineDesc;

    const auto ShaderModule = CompileShader(Window.GetDevice());

    pipelineDesc.vertex.module = ShaderModule;
    pipelineDesc.vertex.entryPoint = "vs_main";
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants = nullptr;

    pipelineDesc.vertex.bufferCount = 0;
    pipelineDesc.vertex.buffers = nullptr;

    const auto VertexMeta = GetVertexBufferMeta();

    std::vector<wgpu::VertexBufferLayout> VertexLayouts;
    if (!VertexMeta.empty()) {
        VertexLayouts.resize(VertexMeta.size());
        for (int i = 0; i < VertexMeta.size(); ++i) {
            VertexLayouts[i] = VertexMeta[i].Layout;
            if ((VertexLayouts[i].attributeCount = VertexMeta[i].Attributes.size()) != 0) {
                VertexLayouts[i].attributes = VertexMeta[i].Attributes.data();
            }
        }

        pipelineDesc.vertex.bufferCount = VertexLayouts.size();
        pipelineDesc.vertex.buffers = VertexLayouts.data();
    }

    wgpu::BlendState blendState;
    blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blendState.color.operation = wgpu::BlendOperation::Add;
    blendState.alpha.srcFactor = wgpu::BlendFactor::Zero;
    blendState.alpha.dstFactor = wgpu::BlendFactor::One;
    blendState.alpha.operation = wgpu::BlendOperation::Add;

    wgpu::ColorTargetState colorTarget;
    colorTarget.format = Window.GetSurfaceFormat();
    colorTarget.blend = &blendState;
    colorTarget.writeMask = wgpu::ColorWriteMask::All;

    wgpu::FragmentState fragmentState;
    fragmentState.module = ShaderModule;
    fragmentState.entryPoint = "fs_main";
    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    pipelineDesc.fragment = &fragmentState;

    pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleStrip;
    pipelineDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
    pipelineDesc.primitive.frontFace = wgpu::FrontFace::CCW;
    pipelineDesc.primitive.cullMode = wgpu::CullMode::Back;

    if (ForceRecreate || m_BindingGroupLayouts.empty()) {
        m_BindingGroupLayouts = CreateBindingGroupLayout(Window.GetDevice());
    }

    if (!m_BindingGroupLayouts.empty()) {
        // Create the pipeline layout
        wgpu::PipelineLayoutDescriptor layoutDesc { };
        layoutDesc.bindGroupLayoutCount = m_BindingGroupLayouts.size();
        layoutDesc.bindGroupLayouts = m_BindingGroupLayouts.data();
        pipelineDesc.layout = Window.GetDevice().CreatePipelineLayout(&layoutDesc);
    }

    wgpu::DepthStencilState depthStencilState;
    depthStencilState.depthCompare = wgpu::CompareFunction::LessEqual;
    depthStencilState.depthWriteEnabled = true;
    depthStencilState.format = Window.GetDepthFormat();
    depthStencilState.stencilReadMask = 0;
    depthStencilState.stencilWriteMask = 0;
    pipelineDesc.depthStencil = &depthStencilState;

    // pipelineDesc.multisample;

    m_Pipline = Window.GetDevice().CreateRenderPipeline(&pipelineDesc);
}

wgpu::ShaderModule
CRenderPipeline::CompileShaderFromCode(const wgpu::Device& Device, const std::string_view& Code)
{
    wgpu::ShaderSourceWGSL shaderCodeDesc;
    shaderCodeDesc.code = Code;

    const wgpu::ShaderModuleDescriptor shaderDesc { .nextInChain = &shaderCodeDesc };
    return Device.CreateShaderModule(&shaderDesc);
}

wgpu::ShaderModule
CRenderPipeline::CompileShader(const wgpu::Device& Device)
{
    return CompileShaderFromCode(Device, m_ShaderCode);
}

std::vector<wgpu::BindGroupLayout>
CRenderPipeline::CreateBindingGroupLayout(const wgpu::Device& Device)
{
    std::vector<wgpu::BindGroupLayoutEntry> bindingLayouts(1);
    bindingLayouts[0].binding = 0;
    DefineSceneUniforms(bindingLayouts[0]);

    // Create a bind group layout
    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc { };
    bindGroupLayoutDesc.entryCount = bindingLayouts.size();
    bindGroupLayoutDesc.entries = bindingLayouts.data();
    return { Device.CreateBindGroupLayout(&bindGroupLayoutDesc) };
}

void CRenderPipeline::DefineSceneUniforms(wgpu::BindGroupLayoutEntry& Entry)
{
    Entry.visibility = wgpu::ShaderStage::Vertex;
    Entry.buffer.type = wgpu::BufferBindingType::Uniform;
    Entry.buffer.minBindingSize = sizeof(SSceneUniform);
}