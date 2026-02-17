//
// Created by LYS on 2/17/2026.
//

#include "TextPipline.hxx"
#include "TextRenderMeta.hxx"

constexpr auto TextShaderCode = R"wgsl(
struct VertexInput {
    @location(0) text_bound: vec4<f32>,
    @location(1) uv_bound: vec4<f32>,
    @location(2) text_group_index: u32,
}

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) texCoord: vec2<f32>,
    @location(1) color: vec4<f32>,
}

struct Uniforms {
    view: mat4x4<f32>,
    projection: mat4x4<f32>,
}

struct TextGroup {
    offset: vec2<f32>,
    color: u32,
    _padding: u32,
}

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(1) @binding(0) var char_texture: texture_2d<f32>;
@group(1) @binding(1) var char_sampler: sampler;
@group(1) @binding(2) var<storage, read> text_groups: array<TextGroup>;

@vertex
fn vs_main(
    @builtin(vertex_index) vid: u32,
    input: VertexInput
) -> VertexOutput {
    // Compute vertex position directly from vertex index
    // 0: (0,0), 1: (1,0), 2: (1,1), 3: (0,1)
    let vertex = vec2<f32>(f32((vid >> 1u) & 1u), f32(vid & 1u));

    // Unpack color using built-in
    let color = unpack4x8unorm(text_groups[input.text_group_index].color);

    // Compute position
    let pos = vertex * input.text_bound.zw + input.text_bound.xy + text_groups[input.text_group_index].offset;

    return VertexOutput(
        uniforms.projection * uniforms.view * vec4<f32>(pos, 0.0, 1.0),
        vertex * input.uv_bound.zw + input.uv_bound.xy,
        color
    );
}

@fragment
fn fs_main(
    @location(0) texCoord: vec2<f32>,
    @location(1) color: vec4<f32>
) -> @location(0) vec4<f32> {
    let alpha = textureSample(char_texture, char_sampler, texCoord).r;

    if (alpha < 0.003921569) { // 1/255
        discard;
    }

    return vec4<f32>(color.rgb, color.a * alpha);
}
)wgsl";

std::vector<wgpu::BindGroupLayout> CTextPipline::CreateBindingGroupLayout(const wgpu::Device& Device)
{
    auto SceneLayout = CRenderPipeline::CreateBindingGroupLayout(Device);

    std::vector<wgpu::BindGroupLayoutEntry> bindingLayouts(3);

    // Binding 0: Texture
    bindingLayouts[0].binding = 0;
    bindingLayouts[0].visibility = wgpu::ShaderStage::Fragment;
    bindingLayouts[0].texture.sampleType = wgpu::TextureSampleType::Float;
    bindingLayouts[0].texture.viewDimension = wgpu::TextureViewDimension::e2D;

    // Binding 1: Sampler
    bindingLayouts[1].binding = 1;
    bindingLayouts[1].visibility = wgpu::ShaderStage::Fragment;
    bindingLayouts[1].sampler.type = wgpu::SamplerBindingType::Filtering;

    // Binding 2: Text Group SSBO
    bindingLayouts[2].binding = 2;
    bindingLayouts[2].visibility = wgpu::ShaderStage::Vertex;
    bindingLayouts[2].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;

    // Create a bind group layout
    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc { };
    bindGroupLayoutDesc.label = "Font Binding";
    bindGroupLayoutDesc.entryCount = bindingLayouts.size();
    bindGroupLayoutDesc.entries = bindingLayouts.data();

    SceneLayout.emplace_back(Device.CreateBindGroupLayout(&bindGroupLayoutDesc));

    return SceneLayout;
}

std::vector<SVertexBufferMeta> CTextPipline::GetVertexBufferMeta() const
{
    std::vector<SVertexBufferMeta> Result { 1 };

    auto& [Layout, Attributes] = Result.front();

    Attributes.resize(3);

    Attributes[0].shaderLocation = 0;
    Attributes[0].format = wgpu::VertexFormat::Float32x4;
    Attributes[0].offset = offsetof(STextRenderVertexMeta, TextBound);

    Attributes[1].shaderLocation = 1;
    Attributes[1].format = wgpu::VertexFormat::Float32x4;
    Attributes[1].offset = offsetof(STextRenderVertexMeta, UVBound);

    Attributes[2].shaderLocation = 2;
    Attributes[2].format = wgpu::VertexFormat::Uint32;
    Attributes[2].offset = offsetof(STextRenderVertexMeta, TextGroupIndex);

    Layout.arrayStride = sizeof(STextRenderVertexMeta);
    Layout.stepMode = wgpu::VertexStepMode::Instance;

    return Result;
}

CTextPipline::CTextPipline()
{
    SetShaderCode(TextShaderCode);
}