//
// Created by LYS on 2/19/2026.
//

#include "NodeConnectionPipline.hxx"

#include <Util/Assertions.hxx>

#include <Interface/Buffer/DynamicGPUBuffer.hxx>

constexpr auto LineShaderNode = R"wgsl(
struct VertexInput {
    @location(0) start_inner_offset: vec2<f32>,
    @location(1) end_inner_offset: vec2<f32>,
    @location(2) start_node_id: u32,
    @location(3) end_node_id: u32,
    @builtin(vertex_index) vertex_index: u32,
};

struct Camera {
    view: mat4x4<f32>,
    projection: mat4x4<f32>,
}

struct NodeCommon {
    offset: vec2<f32>,
    state: u32,
    _padding: u32,
}

const SEGMENTS: f32 = 64.0;
const THICKNESS: f32 = 4.0;
const FEATHER: f32 = 2.0;

@group(0) @binding(0) var<uniform> camera: Camera;
@group(1) @binding(0) var<storage, read> node_commons: array<NodeCommon>;

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2<f32>,
};

fn bezier(t: f32, p0: vec2<f32>, p1: vec2<f32>, p2: vec2<f32>, p3: vec2<f32>) -> vec2<f32> {
    let one_minus_t = 1.0 - t;
    return
        (one_minus_t * one_minus_t * one_minus_t) * p0 +
        (3.0 * one_minus_t * one_minus_t * t)     * p1 +
        (3.0 * one_minus_t * t * t)               * p2 +
        (t * t * t)                               * p3;
}

@vertex
fn vs_main(input: VertexInput) -> VertexOutput {

    // 1. Triangle Strip Logic
    let step_index = f32(input.vertex_index / 2u);
    let side = select(-1.0, 1.0, (input.vertex_index % 2u) == 1u);
    let t = clamp(step_index / SEGMENTS, 0.0, 1.0);

    let line_start = input.start_inner_offset + node_commons[input.start_node_id].offset + vec2<f32>(4, 0);
    let line_end = input.end_inner_offset + node_commons[input.end_node_id].offset -  vec2<f32>(4, 0);

    // 2. Bezier Control Points (World Space)
    let dist = distance(line_start, line_end);
    let control_dist = max(dist * 0.5, 30.0); // Adjust this constant based on your world scale

    let p0 = line_start;
    let p1 = line_start + vec2<f32>(control_dist, 0.0);
    let p2 = line_end   - vec2<f32>(control_dist, 0.0);
    let p3 = line_end;

    // 3. Calculate Position & Tangent (World Space)
    let current_world = bezier(t, p0, p1, p2, p3);

    // Look ahead/behind
    let t_next = select(t + 0.01, t - 0.01, t >= 1.0);
    let next_world = bezier(t_next, p0, p1, p2, p3);

    // 4. Calculate Normal (World Space)
    // Note: We do NOT project to screen space first.
    var dir = next_world - current_world;
    if (t >= 1.0) { dir = -dir; }

    let len = length(dir);
    var normal = vec2<f32>(0.0);
    if (len > 0.001) {
        normal = normalize(vec2<f32>(-dir.y, dir.x));
    }

    // 5. Extrude (World Space)
    // We simply move the point by half the THICKNESS along the normal
    let offset = normal * (THICKNESS * 0.5) * side;
    let final_world_pos = current_world + offset;

    // 6. Project to Clip Space
    var out: VertexOutput;
    out.position =  camera.projection * camera.view * vec4f(final_world_pos, 0.0, 1.0);
    out.uv = vec2<f32>(t, side);

    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    // Anti-Aliasing Logic
    let dist = abs(in.uv.y); // 0.0 (center) to 1.0 (edge)

    // fwidth() calculates the rate of change of a value between neighboring pixels.
    // This tells us "how much UV space does 1 pixel cover?" at the current zoom level.
    let dist_change_per_pixel = fwidth(dist);

    // We want the fade to happen over 'FEATHER' pixels.
    let feather_width = dist_change_per_pixel * FEATHER;

    // Calculate alpha
    let alpha = 1.0 - smoothstep(1.0 - feather_width, 1.0, dist);

    return vec4f(1.0, 1.0, 1.0, alpha);
})wgsl";

CNodeConnectionPipline::CNodeConnectionPipline(const CWindowBase* Window)
{
    SetShaderCode(LineShaderNode);
    m_RenderVertexBuffer = CDynamicGPUBuffer::Create<SNodeConnectionInstanceBuffer>(Window);
}

CNodeConnectionPipline::~CNodeConnectionPipline() = default;

size_t CNodeConnectionPipline::AddConnection(const SNodeConnectionInstanceBuffer& Connection)
{
    const auto FreeIndex = m_VertexBufferRangeManager.GetFirstFreeIndex();
    if (FreeIndex >= m_RenderVertexBuffer->GetBufferSize()) {
        CHECK(FreeIndex == m_RenderVertexBuffer->GetBufferSize())
        m_RenderVertexBuffer->PushUninitialized();
    }

    m_VertexBufferRangeManager.SetSlot(FreeIndex);
    GetVertexBuffer().At<SNodeConnectionInstanceBuffer>(FreeIndex) = Connection;
    GetVertexBuffer().Upload(FreeIndex);

    return FreeIndex;
}

void CNodeConnectionPipline::RemoveConnection(const size_t Id) noexcept
{
    m_VertexBufferRangeManager.RemoveSlot(Id);
}

std::vector<SVertexBufferMeta> CNodeConnectionPipline::GetVertexBufferMeta() const
{
    std::vector<SVertexBufferMeta> Result { 1 };
    auto& [Layout, Attributes] = Result.front();

    Attributes.resize(4);

    Attributes[0].shaderLocation = 0;
    Attributes[0].format = wgpu::VertexFormat::Float32x2;
    Attributes[0].offset = offsetof(SNodeConnectionInstanceBuffer, StartInnerOffset);

    Attributes[1].shaderLocation = 1;
    Attributes[1].format = wgpu::VertexFormat::Float32x2;
    Attributes[1].offset = offsetof(SNodeConnectionInstanceBuffer, EndInnerOffset);

    Attributes[2].shaderLocation = 2;
    Attributes[2].format = wgpu::VertexFormat::Uint32;
    Attributes[2].offset = offsetof(SNodeConnectionInstanceBuffer, StartNodeId);

    Attributes[3].shaderLocation = 3;
    Attributes[3].format = wgpu::VertexFormat::Uint32;
    Attributes[3].offset = offsetof(SNodeConnectionInstanceBuffer, EndNodeId);

    Layout.arrayStride = sizeof(SNodeConnectionInstanceBuffer);
    Layout.stepMode = wgpu::VertexStepMode::Instance;

    return Result;
}

std::vector<wgpu::BindGroupLayout> CNodeConnectionPipline::CreateBindingGroupLayout(const wgpu::Device& Device)
{
    auto SceneLayout = CRenderPipeline::CreateBindingGroupLayout(Device);

    std::vector<wgpu::BindGroupLayoutEntry> bindingLayouts(1);

    // Binding 0: Text common SSBO
    bindingLayouts[0].binding = 0;
    bindingLayouts[0].visibility = wgpu::ShaderStage::Vertex;
    bindingLayouts[0].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;

    // Create a bind group layout
    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc { };
    bindGroupLayoutDesc.label = "Node Binding";
    bindGroupLayoutDesc.entryCount = bindingLayouts.size();
    bindGroupLayoutDesc.entries = bindingLayouts.data();

    SceneLayout.emplace_back(Device.CreateBindGroupLayout(&bindGroupLayoutDesc));

    return SceneLayout;
}
