//
// Created by LYS on 2/17/2026.
//

#include "NodePipline.hxx"

const auto NodeRenderShader = R"(
struct VertexInput {
    @location(0) size: vec2<f32>,
    @location(1) offset: vec2<f32>,
    @location(2) header_color: u32,
    @location(3) state: u32,
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

@group(0) @binding(0) var<uniform> camera: Camera;

// Blueprint node constants
const HEADER_HEIGHT: f32 = 30.0;
const CORNER_RADIUS: f32 = 8.0;
const BORDER_WIDTH: f32 = 2.0;
const PICKED_BORDER_WIDTH: f32 = 4.0;
const BODY_COLOR: vec4<f32> = vec4<f32>(0.15, 0.15, 0.2, 1.0);
const BORDER_COLOR: vec4<f32> = vec4<f32>(0.3, 0.5, 0.9, 0.7);
const PICKED_BORDER_COLOR: vec4<f32> = vec4<f32>(1.0, 0.74, 0.0, 1.0);

@vertex
fn vs_main(
    @builtin(vertex_index) vertex_index: u32,
    input: VertexInput,
) -> VertexOutput {
    var output: VertexOutput;

    // Triangle strip vertices (4 vertices for a quad)
    // Order: bottom-left, top-left, bottom-right, top-right
    let strip_index = vertex_index % 4u;
    var local_pos: vec2<f32>;

    switch strip_index {
        case 0u: { local_pos = vec2<f32>(0.0, 1.0); } // bottom-left
        case 1u: { local_pos = vec2<f32>(1.0, 1.0); } // bottom-right
        case 2u: { local_pos = vec2<f32>(0.0, 0.0); } // top-left
        default: { local_pos = vec2<f32>(1.0, 0.0); } // top-right
    }

    // Calculate world position (2D)
    let world_pos_2d = input.offset + local_pos * input.size;
    let world_pos = vec4<f32>(world_pos_2d.x, world_pos_2d.y, 0.0, 1.0);

    // Apply view and projection matrices
    output.position = camera.projection * camera.view * world_pos;

    // Pass through data
    output.uv = local_pos;
    output.node_offset = input.offset;
    output.node_size = input.size;
    output.header_color = input.header_color;
    output.state = input.state;

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

bool SNodeBackgroundRenderMeta::InBound(const glm::vec2 Position) const noexcept
{
    return Position.x >= Offset.x && Position.y >= Offset.y
        && Position.x <= Offset.x + Size.x && Position.y <= Offset.y + Size.y;
}

CNodePipline::CNodePipline(CWindowBase* Window)
{
    SetShaderCode(NodeRenderShader);
    m_RenderVertexBuffer = CDynamicVertexBuffer::Create<SNodeBackgroundRenderMeta>(Window);
}

std::vector<SVertexBufferMeta> CNodePipline::GetVertexBufferMeta() const
{
    std::vector<SVertexBufferMeta> Result { 1 };
    auto& [Layout, Attributes] = Result.front();

    Attributes.resize(4);

    Attributes[0].shaderLocation = 0;
    Attributes[0].format = wgpu::VertexFormat::Float32x2;
    Attributes[0].offset = offsetof(SNodeBackgroundRenderMeta, Size);

    Attributes[1].shaderLocation = 1;
    Attributes[1].format = wgpu::VertexFormat::Float32x2;
    Attributes[1].offset = offsetof(SNodeBackgroundRenderMeta, Offset);

    Attributes[2].shaderLocation = 2;
    Attributes[2].format = wgpu::VertexFormat::Uint32;
    Attributes[2].offset = offsetof(SNodeBackgroundRenderMeta, HeaderColor);

    Attributes[3].shaderLocation = 3;
    Attributes[3].format = wgpu::VertexFormat::Uint32;
    Attributes[3].offset = offsetof(SNodeBackgroundRenderMeta, State);

    Layout.arrayStride = sizeof(SNodeBackgroundRenderMeta);
    Layout.stepMode = wgpu::VertexStepMode::Instance;

    return Result;
}