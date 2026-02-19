//
// Created by LYS on 2/18/2026.
//

#include "NodeTextRenderPipline.hxx"

#include <Interface/Buffer/DynamicGPUBuffer.hxx>
#include <Interface/Font/Font.hxx>
#include <Interface/WindowBase.hxx>

#include <Util/Assertions.hxx>

constexpr auto NodeTextShaderCode = R"wgsl(
struct VertexInput {
    @location(0) text_bound: vec4<f32>,
    @location(1) uv_bound: vec4<f32>,
    @location(2) text_group_index: u32,
    @location(3) node_index: u32,
    @builtin(vertex_index) vertex_index: u32,
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

struct NodeCommon {
    offset: vec2<f32>,
    state: u32,
    _padding: u32,
}

struct TextGroup {
    color: u32,
}

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(1) @binding(0) var<storage, read> text_commons: array<NodeCommon>;
@group(2) @binding(0) var<storage, read> text_groups: array<TextGroup>;
@group(2) @binding(1) var char_texture: texture_2d<f32>;
@group(2) @binding(2) var char_sampler: sampler;

@vertex
fn vs_main(input: VertexInput) -> VertexOutput {
    // Compute vertex position directly from vertex index
    // 0: (0,0), 1: (1,0), 2: (1,1), 3: (0,1)
    let vertex = vec2<f32>(f32((input.vertex_index >> 1u) & 1u), f32(input.vertex_index & 1u));

    // Unpack color using built-in
    let color = unpack4x8unorm(text_groups[input.text_group_index].color);

    // Compute position
    let pos = vertex * input.text_bound.zw + input.text_bound.xy + text_commons[input.node_index].offset + vec2<f32>(10.0, 10.0);

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

    if (alpha < 1) {
        discard;
    }

    return vec4<f32>(color.rgb, color.a * alpha);
}
)wgsl";

void CNodeTextRenderPipline::CreateCommonBindingGroup()
{
    std::vector<wgpu::BindGroupEntry> bindings(3);
    // Binding 0: Text group SSBO
    bindings[0].binding = 0;
    bindings[0].buffer = *m_TextGroupBuffer;
    bindings[0].offset = 0;
    bindings[0].size = m_TextGroupBuffer->GetBufferByteSize();

    // Binding 1: Texture view
    bindings[1].binding = 1;
    bindings[1].textureView = *m_Font;

    // Binding 2: Sampler
    bindings[2].binding = 2;
    bindings[2].sampler = *m_Font;

    wgpu::BindGroupDescriptor bindGroupDesc { };
    bindGroupDesc.layout = GetBindGroups()[2];
    bindGroupDesc.entryCount = bindings.size();
    bindGroupDesc.entries = bindings.data();
    m_TextGroupSSBOBindingGroup = m_Window->GetDevice().CreateBindGroup(&bindGroupDesc);
}

void CNodeTextRenderPipline::RefreshBuffer(STextGroupHandle& TextGroupHandle)
{
    /// Free old buffer
    if (TextGroupHandle.GroupId != -1) {

        if (TextGroupHandle.VertexSpan.second != 0) {
            m_VertexBufferRangeManager.RemoveRange(TextGroupHandle.VertexSpan.first, TextGroupHandle.VertexSpan.second);
            TextGroupHandle.VertexSpan = { };
        }
    } else {
        /// Assign group
        if (!m_TextGroupBufferFreeList.empty()) {
            TextGroupHandle.GroupId = m_TextGroupBufferFreeList.top();
            m_TextGroupBufferFreeList.pop();
        } else {
            TextGroupHandle.GroupId = m_TextGroupBuffer->GetBufferSize();
            m_TextGroupBuffer->PushUninitialized();
            CreateCommonBindingGroup();
        }
    }
}

float CNodeTextRenderPipline::PushVertexBuffer(STextGroupHandle& TextGroupHandle)
{
    RefreshBuffer(TextGroupHandle);

    unsigned int GlyphsCount = 0;
    float TextWidth = m_Font->BuildVertex(TextGroupHandle.Text, TextGroupHandle.Scale, [&](const size_t GlyphsToAllocate) {
        GlyphsCount = GlyphsToAllocate;

        CHECK(TextGroupHandle.VertexSpan.second == 0)
        const auto OldVertexCount = m_TextVertexBuffer->GetBufferSize();
        const auto FirstFit = m_VertexBufferRangeManager.FirstFit(GlyphsToAllocate);
        CHECK(FirstFit <= OldVertexCount)

        /// Reallocate is needed
        if (FirstFit + GlyphsToAllocate > OldVertexCount) {
            m_TextVertexBuffer->Resize(FirstFit + GlyphsToAllocate);
        }

        TextGroupHandle.VertexSpan = { FirstFit, GlyphsToAllocate };
        m_VertexBufferRangeManager.SetRange(FirstFit, GlyphsToAllocate);

        static constexpr size_t ArchetypeByteOffset = 0;
        static_assert(offsetof(STextVertexArchetype, TextBound) == offsetof(STextRenderVertexMeta, TextBound) + ArchetypeByteOffset);
        static_assert(offsetof(STextVertexArchetype, UVBound) == offsetof(STextRenderVertexMeta, UVBound) + ArchetypeByteOffset);

        return reinterpret_cast<STextVertexArchetype*>(reinterpret_cast<std::byte*>(m_TextVertexBuffer->Begin<STextRenderVertexMeta>() + TextGroupHandle.VertexSpan.first) + ArchetypeByteOffset); }, sizeof(STextRenderVertexMeta));

    if (GlyphsCount == 0) {
        return TextWidth;
    }

    const auto GlyphsSubspan = m_TextVertexBuffer->GetView<STextRenderVertexMeta>().subspan(TextGroupHandle.VertexSpan.first, GlyphsCount);
    for (unsigned int i = 0; i < GlyphsCount; i++) {
        GlyphsSubspan[i].TextGroupIndex = TextGroupHandle.GroupId;
        GlyphsSubspan[i].NodeIndex = TextGroupHandle.NodeId;
    }

    (void)m_TextVertexBuffer->Upload(GlyphsSubspan);
    return TextWidth;
}

void CNodeTextRenderPipline::PushPerGroupBuffer(STextGroupHandle& TextGroupHandle)
{
    MAKE_SURE(TextGroupHandle.GroupId != -1)
    m_TextGroupBuffer->Upload(TextGroupHandle.GroupId);
}

CNodeTextRenderPipline::CNodeTextRenderPipline(const CWindowBase* Window, std::shared_ptr<class CFont> Font)
    : m_Window(Window)
    , m_Font(std::move(Font))
{
    SetShaderCode(NodeTextShaderCode);

    m_TextVertexBuffer = CDynamicGPUBuffer::Create<STextRenderVertexMeta>(Window);
    m_TextGroupBuffer = CDynamicGPUBuffer::Create<SNodeTextPerGroupMeta>(Window, wgpu::BufferUsage::Storage);
}

CNodeTextRenderPipline::~CNodeTextRenderPipline() = default;

std::list<STextGroupHandle>::iterator CNodeTextRenderPipline::RegisterTextGroup(size_t NodeId, std::string Text, float Scale, const SNodeTextPerGroupMeta& Meta)
{
    auto Result = m_RegisteredTextGroup.emplace(m_RegisteredTextGroup.end(), std::move(Text), Scale);
    Result->NodeId = NodeId;

    RefreshBuffer(*Result);
    MAKE_SURE(Result->GroupId != -1)
    m_TextGroupBuffer->At<SNodeTextPerGroupMeta>(Result->GroupId) = Meta;

    UpdateTextGroup(Result);
    return Result; // NOLINT
}

void CNodeTextRenderPipline::UpdateTextGroup(const std::list<STextGroupHandle>::iterator& Group)
{
    Group->TextWidth = PushVertexBuffer(*Group);
    PushPerGroupBuffer(*Group);
}

void CNodeTextRenderPipline::UnregisterTextGroup(const std::list<STextGroupHandle>::iterator& Group)
{
    if (Group->GroupId != -1) {
        RefreshBuffer(*Group);
        m_TextGroupBufferFreeList.push(Group->GroupId);
    }
}

std::vector<wgpu::BindGroupLayout> CNodeTextRenderPipline::CreateBindingGroupLayout(const wgpu::Device& Device)
{
    auto SceneLayout = CRenderPipeline::CreateBindingGroupLayout(Device);

    std::vector<wgpu::BindGroupLayoutEntry> NodeBindingLayouts(1);

    // Binding 0: Text common SSBO
    NodeBindingLayouts[0].binding = 0;
    NodeBindingLayouts[0].visibility = wgpu::ShaderStage::Vertex;
    NodeBindingLayouts[0].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;

    wgpu::BindGroupLayoutDescriptor NodeBindGroupLayoutDesc { };
    NodeBindGroupLayoutDesc.label = "Node Binding";
    NodeBindGroupLayoutDesc.entryCount = NodeBindingLayouts.size();
    NodeBindGroupLayoutDesc.entries = NodeBindingLayouts.data();
    SceneLayout.emplace_back(Device.CreateBindGroupLayout(&NodeBindGroupLayoutDesc));

    std::vector<wgpu::BindGroupLayoutEntry> TextBindingLayouts(3);

    // Binding 0: Text Group SSBO
    TextBindingLayouts[0].binding = 0;
    TextBindingLayouts[0].visibility = wgpu::ShaderStage::Vertex;
    TextBindingLayouts[0].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;

    // Binding 1: Texture
    TextBindingLayouts[1].binding = 1;
    TextBindingLayouts[1].visibility = wgpu::ShaderStage::Fragment;
    TextBindingLayouts[1].texture.sampleType = wgpu::TextureSampleType::Float;
    TextBindingLayouts[1].texture.viewDimension = wgpu::TextureViewDimension::e2D;

    // Binding 2: Sampler
    TextBindingLayouts[2].binding = 2;
    TextBindingLayouts[2].visibility = wgpu::ShaderStage::Fragment;
    TextBindingLayouts[2].sampler.type = wgpu::SamplerBindingType::Filtering;

    // Create a bind group layout
    wgpu::BindGroupLayoutDescriptor TextBindGroupLayoutDesc { };
    TextBindGroupLayoutDesc.label = "Font Binding";
    TextBindGroupLayoutDesc.entryCount = TextBindingLayouts.size();
    TextBindGroupLayoutDesc.entries = TextBindingLayouts.data();

    SceneLayout.emplace_back(Device.CreateBindGroupLayout(&TextBindGroupLayoutDesc));

    return SceneLayout;
}

std::vector<SVertexBufferMeta> CNodeTextRenderPipline::GetVertexBufferMeta() const
{
    std::vector<SVertexBufferMeta> Result { 1 };

    auto& [Layout, Attributes] = Result.front();

    Attributes.resize(4);

    Attributes[0].shaderLocation = 0;
    Attributes[0].format = wgpu::VertexFormat::Float32x4;
    Attributes[0].offset = offsetof(STextRenderVertexMeta, TextBound);

    Attributes[1].shaderLocation = 1;
    Attributes[1].format = wgpu::VertexFormat::Float32x4;
    Attributes[1].offset = offsetof(STextRenderVertexMeta, UVBound);

    Attributes[2].shaderLocation = 2;
    Attributes[2].format = wgpu::VertexFormat::Uint32;
    Attributes[2].offset = offsetof(STextRenderVertexMeta, TextGroupIndex);

    Attributes[3].shaderLocation = 3;
    Attributes[3].format = wgpu::VertexFormat::Uint32;
    Attributes[3].offset = offsetof(STextRenderVertexMeta, NodeIndex);

    Layout.arrayStride = sizeof(STextRenderVertexMeta);
    Layout.stepMode = wgpu::VertexStepMode::Instance;

    return Result;
}