//
// Created by LYS on 2/19/2026.
//

#include "NodePinPipline.hxx"

#include <Util/Assertions.hxx>

#include <Interface/Buffer/DynamicGPUBuffer.hxx>

constexpr auto PinShaderCode = R"wgsl(
struct VertexInput {
    @location(0) position: vec2<f32>,   // World position of the pin center
    @location(1) radius: f32,           // Size of the pin
    @location(2) color: u32,            // Packed RGBA color
    @location(3) flags: u32,            // Bitfield: Bit 0 = IsExec, Bit 1 = IsConnected
    @location(4) node_id: u32,
    @builtin(vertex_index) vertex_index: u32,
}

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,         // Local coordinates (-1.0 to 1.0)
    @location(1) @interpolate(flat) color: vec4<f32>,
    @location(2) @interpolate(flat) flags: u32,
    @location(3) @interpolate(flat) radius: f32,
}

struct Camera {
    view: mat4x4<f32>,
    projection: mat4x4<f32>,
}

struct NodeCommon {
    offset: vec2<f32>,
    state: u32,
    _padding: u32,
}

@group(0) @binding(0) var<uniform> camera: Camera;
@group(1) @binding(0) var<storage, read> node_commons: array<NodeCommon>;

// Constants
const ARROW_HOLLOW_THICKNESS: f32 = 3.0;
const CIRCLE_HOLLOW_THICKNESS: f32 = 1.0;
const BORDER_WIDTH: f32 = 2.0; // Thickness of the black outline
const HOLLOW_WIDTH: f32 = 2.0; // Thickness of the ring when disconnected

@vertex
fn vs_main(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;

    // Generate Quad (Triangle Strip)
    // We expand the radius slightly to account for AA and borders
    let expansion_factor = 1.5;
    let size = input.radius * expansion_factor;

    let idx = input.vertex_index % 4u;
    var local_pos: vec2<f32>;

    // Generate UVs from -1.0 to 1.0
    switch idx {
        case 0u: { local_pos = vec2<f32>(-1.0, 1.0); } // Bottom-Left
        case 1u: { local_pos = vec2<f32>(1.0, 1.0); }  // Bottom-Right
        case 2u: { local_pos = vec2<f32>(-1.0, -1.0); } // Top-Left
        default: { local_pos = vec2<f32>(1.0, -1.0); }  // Top-Right
    }

    // Calculate world position
    let world_pos = input.position + local_pos * size + node_commons[input.node_id].offset;

    output.position = camera.projection * camera.view * vec4<f32>(world_pos, 0.0, 1.0);
    output.uv = local_pos; // UVs are -1 to 1
    output.color = unpack_color(input.color);
    output.flags = input.flags;
    output.radius = input.radius;

    return output;
}

fn unpack_color(packed: u32) -> vec4<f32> {
    let r = f32((packed >> 24u) & 0xFFu) / 255.0;
    let g = f32((packed >> 16u) & 0xFFu) / 255.0;
    let b = f32((packed >> 8u) & 0xFFu) / 255.0;
    let a = f32(packed & 0xFFu) / 255.0;
    return vec4<f32>(r, g, b, a);
}

fn sd_circle(p: vec2<f32>, r: f32) -> f32 {
    return length(p) - r;
}

fn sd_arrow(p: vec2<f32>, r: f32) -> f32 {
    // Note: We do NOT multiply by r here anymore to keep the distance field
    // uniform (1 unit = 1 unit). We subtract r at the end.

    // Scale factor to match circle visual size
    let s = 0.85;

    // 1. Vertical Walls
    let d_box_y = abs(p.y) - s * r;

    // 2. Back Wall
    let d_box_x = -p.x - s * r;

    // 3. Angled Tip (Normalized)
    let k = vec2<f32>(0.70710678, 0.70710678);
    // Shift tip slightly (-0.1 * r)
    let d_tip = dot(vec2<f32>(p.x - (0.1 * r), abs(p.y)), k) - s * r;

    return max(d_box_x, max(d_box_y, d_tip));
}

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {

    // Unpack flags
    let is_exec = (input.flags & 1u) != 0u;
    let is_connected = (input.flags & 2u) != 0u;

    var dist: f32;

    // The shape radius in UV space.
    // Since UV 1.0 = Radius * 1.5, then Radius = UV 0.666...
    // Let's just use 0.75 to fill the quad nicely.
    let shape_uv_radius = 0.75;

    if (is_exec) {
        dist = sd_arrow(input.uv, shape_uv_radius);
    } else {
        dist = sd_circle(input.uv, shape_uv_radius);
    }

    // --- THICKNESS CALCULATION ---

    // 1. Calculate how many World Units correspond to 1.0 UV Unit
    // In Vertex shader: size = radius * 1.5
    let uv_to_world_scale = input.radius * 1.5;

    // 2. Convert target World Thickness to UV Thickness
    var thickness_uv: f32;
    if (is_exec) {
        thickness_uv = ARROW_HOLLOW_THICKNESS / uv_to_world_scale;
    } else {
        thickness_uv = CIRCLE_HOLLOW_THICKNESS / uv_to_world_scale;
    }

    // 3. Anti-Aliasing (Pixel based for sharpness)
    // We still use fwidth for AA so edges don't look blurry when zoomed in
    let aa_width = 1.5 * fwidth(dist);

    var alpha = 0.0;

    if (is_connected) {
        // --- CONNECTED (FILLED) ---
        alpha = 1.0 - smoothstep(0.0, aa_width, dist);
    } else {
        // --- DISCONNECTED (HOLLOW) ---

        // Outer Edge
        let outer = 1.0 - smoothstep(0.0, aa_width, dist);

        // Inner Edge
        // We subtract the calculated UV thickness
        let inner_edge = -thickness_uv;
        let inner = smoothstep(inner_edge - aa_width, inner_edge, dist);

        alpha = outer * inner;
    }

    if (alpha <= 0.0) {
        discard;
    }

    var final_color = input.color;
    final_color.a *= alpha;

    return final_color;
})wgsl";

CNodePinPipline::~CNodePinPipline() = default;
CNodePinPipline::CNodePinPipline(const CWindowBase* Window)
{
    SetShaderCode(PinShaderCode);

    m_RenderVertexBuffer = CDynamicGPUBuffer::Create<SNodePinInstanceBuffer>(Window);
}

size_t CNodePinPipline::NewPin(const uint32_t NodeId, const glm::vec2& Offset, const float Radius, const uint32_t Color, const bool IsExecution, const bool IsConnected)
{
    const auto OldVertexCount = m_RenderVertexBuffer->GetBufferSize();

    const auto FirstFree = m_VertexBufferRangeManager.GetFirstFreeIndex();
    if (FirstFree >= OldVertexCount) {
        MAKE_SURE(FirstFree == OldVertexCount)
        m_RenderVertexBuffer->PushUninitialized();
    }

    m_VertexBufferRangeManager.SetSlot(FirstFree);
    m_RenderVertexBuffer->At<SNodePinInstanceBuffer>(FirstFree) = {
        .Offset = Offset,
        .Radius = Radius,
        .Color = Color,
        .Flag = (IsConnected ? 0b10u : 0) | (IsExecution ? 0b1u : 0),
        .NodeId = NodeId
    };
    m_RenderVertexBuffer->Upload(FirstFree);

    return FirstFree;
}

void CNodePinPipline::RemovePin(const uint32_t Index) noexcept
{
    m_VertexBufferRangeManager.RemoveSlot(Index);
}

std::vector<SVertexBufferMeta> CNodePinPipline::GetVertexBufferMeta() const
{
    std::vector<SVertexBufferMeta> Result { 1 };
    auto& [Layout, Attributes] = Result.front();

    Attributes.resize(5);

    Attributes[0].shaderLocation = 0;
    Attributes[0].format = wgpu::VertexFormat::Float32x2;
    Attributes[0].offset = offsetof(SNodePinInstanceBuffer, Offset);

    Attributes[1].shaderLocation = 1;
    Attributes[1].format = wgpu::VertexFormat::Float32;
    Attributes[1].offset = offsetof(SNodePinInstanceBuffer, Radius);

    Attributes[2].shaderLocation = 2;
    Attributes[2].format = wgpu::VertexFormat::Uint32;
    Attributes[2].offset = offsetof(SNodePinInstanceBuffer, Color);

    Attributes[3].shaderLocation = 3;
    Attributes[3].format = wgpu::VertexFormat::Uint32;
    Attributes[3].offset = offsetof(SNodePinInstanceBuffer, Flag);

    Attributes[4].shaderLocation = 4;
    Attributes[4].format = wgpu::VertexFormat::Uint32;
    Attributes[4].offset = offsetof(SNodePinInstanceBuffer, NodeId);

    Layout.arrayStride = sizeof(SNodePinInstanceBuffer);
    Layout.stepMode = wgpu::VertexStepMode::Instance;

    return Result;
}

std::vector<wgpu::BindGroupLayout> CNodePinPipline::CreateBindingGroupLayout(const wgpu::Device& Device)
{
    auto SceneLayout = CRenderPipeline::CreateBindingGroupLayout(Device);

    std::vector<wgpu::BindGroupLayoutEntry> bindingLayouts(1);

    // Binding 0: Text common SSBO
    bindingLayouts[0].binding = 0;
    bindingLayouts[0].visibility = wgpu::ShaderStage::Vertex;
    bindingLayouts[0].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;

    // Create a bind group layout
    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc { };
    bindGroupLayoutDesc.label = "Pin Binding";
    bindGroupLayoutDesc.entryCount = bindingLayouts.size();
    bindGroupLayoutDesc.entries = bindingLayouts.data();

    SceneLayout.emplace_back(Device.CreateBindGroupLayout(&bindGroupLayoutDesc));

    return SceneLayout;
}