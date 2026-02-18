//
// Created by LYS on 2/17/2026.
//

#include "NodeBackgroundPipline.hxx"

const auto NodeRenderShader = R"(
struct VertexInput {
    @location(0) size: vec2<f32>,
    @location(1) header_color: u32,
    @builtin(vertex_index) vertex_index: u32,
    @builtin(instance_index) inst_id: u32,
}

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
    @location(1) node_offset: vec2<f32>,
    @location(2) node_size: vec2<f32>,
    @location(3) @interpolate(flat) header_color: u32,
    @location(4) @interpolate(flat) state: u32,
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
@group(1) @binding(0) var<storage, read> text_commons: array<NodeCommon>;

// Blueprint node constants
const HEADER_HEIGHT: f32 = 30.0;
const CORNER_RADIUS: f32 = 8.0;
const BORDER_WIDTH: f32 = 2.0;
const PICKED_BORDER_WIDTH: f32 = 4.0;
const BODY_COLOR: vec4<f32> = vec4<f32>(0.15, 0.15, 0.2, 1.0);
const BORDER_COLOR: vec4<f32> = vec4<f32>(0.3, 0.5, 0.9, 0.7);
const PICKED_BORDER_COLOR: vec4<f32> = vec4<f32>(1.0, 0.74, 0.0, 1.0);

@vertex
fn vs_main(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;

    let text_common = text_commons[input.inst_id];

    // Triangle strip vertices (4 vertices for a quad)    // Order: bottom-left, top-left, bottom-right, top-right
    let strip_index = input.vertex_index % 4u;
    var local_pos: vec2<f32>;

    switch strip_index {
        case 0u: { local_pos = vec2<f32>(0.0, 1.0); } // bottom-left
        case 1u: { local_pos = vec2<f32>(1.0, 1.0); } // bottom-right
        case 2u: { local_pos = vec2<f32>(0.0, 0.0); } // top-left
        default: { local_pos = vec2<f32>(1.0, 0.0); } // top-right
    }

    // Calculate world position (2D)
    let world_pos_2d = text_common.offset + local_pos * input.size;
    let world_pos = vec4<f32>(world_pos_2d.x, world_pos_2d.y, 0.0, 1.0);

    // Apply view and projection matrices
    output.position = camera.projection * camera.view * world_pos;

    // Pass through data
    output.uv = local_pos;
    output.node_offset = text_common.offset;
    output.node_size = input.size;
    output.header_color = input.header_color;
    output.state = text_common.state;

    return output;
}

// Unpack 8-bit RGBA color to vec4<f32>
fn unpack_color(packed: u32) -> vec4<f32> {
    let r = f32((packed >> 24u) & 0xFFu) / 255.0;
    let g = f32((packed >> 16u) & 0xFFu) / 255.0;
    let b = f32((packed >> 8u) & 0xFFu) / 255.0;
    let a = f32(packed & 0xFFu) / 255.0;
    return vec4<f32>(r, g, b, a);
}

// Signed distance function for rounded rectangle
fn sd_rounded_box(p: vec2<f32>, b: vec2<f32>, r: f32) -> f32 {
    let q = abs(p) - b + vec2<f32>(r);
    return length(max(q, vec2<f32>(0.0))) + min(max(q.x, q.y), 0.0) - r;
}

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {

    // Calculate pixel position within the node
    let pixel_pos = input.node_offset + input.uv * input.node_size;

    // Calculate node bounds
    let node_center = input.node_offset + input.node_size * 0.5;
    let half_size = input.node_size * 0.5;

    // Position relative to node center
    let p = pixel_pos - node_center;

    // Calculate distance to node boundary
    let node_dist = sd_rounded_box(p, half_size, CORNER_RADIUS);

    // Unpack header color
    let header_color = unpack_color(input.header_color);

    // Determine which part we're rendering
    var color = vec4<f32>(0.0);

    // Check if we're inside the node
    if (node_dist < 0.0) {
        // Check if we're in the header (top 30 units)
        if (p.y < -half_size.y + HEADER_HEIGHT) {
            color = header_color;
        } else {
            color = BODY_COLOR;
        }

        // Add border
        if((input.state & 1) == 0){
            if (node_dist > -BORDER_WIDTH) {
                color = BORDER_COLOR;
            }
        } else {
            if (node_dist > -PICKED_BORDER_WIDTH) {
                color = PICKED_BORDER_COLOR;
            }
        }
    } else {
        discard; // Discard pixels outside the node
    }

    // Anti-aliasing
    let edge_softness = 1.0;
    let alpha = 1.0 - smoothstep(-edge_softness, edge_softness, node_dist);
    color.a *= alpha;

    return color;
})";

CNodeBackgroundPipline::CNodeBackgroundPipline(const CWindowBase* Window)
{
    SetShaderCode(NodeRenderShader);

    m_RenderVertexBuffer = CDynamicGPUBuffer::Create<SNodeBackgroundInstanceBuffer>(Window);
}

std::vector<SVertexBufferMeta> CNodeBackgroundPipline::GetVertexBufferMeta() const
{
    std::vector<SVertexBufferMeta> Result { 1 };
    auto& [Layout, Attributes] = Result.front();

    Attributes.resize(2);

    Attributes[0].shaderLocation = 0;
    Attributes[0].format = wgpu::VertexFormat::Float32x2;
    Attributes[0].offset = offsetof(SNodeBackgroundInstanceBuffer, Size);

    Attributes[1].shaderLocation = 1;
    Attributes[1].format = wgpu::VertexFormat::Uint32;
    Attributes[1].offset = offsetof(SNodeBackgroundInstanceBuffer, HeaderColor);

    Layout.arrayStride = sizeof(SNodeBackgroundInstanceBuffer);
    Layout.stepMode = wgpu::VertexStepMode::Instance;

    return Result;
}

std::vector<wgpu::BindGroupLayout> CNodeBackgroundPipline::CreateBindingGroupLayout(const wgpu::Device& Device)
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