#include <Interface/Pipline/DepthTexture.hxx>
#include <Interface/Pipline/RenderPipeline.hxx>

#include <AMboard/Editor/BoardEditor.hxx>

#include <AMboard/Macro/DataPin.hxx>
#include <AMboard/Macro/ExecuteNode.hxx>
#include <AMboard/Macro/FlowPin.hxx>

#include <chrono>
#include <iostream>

#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>

class CPrintingNode : public CExecuteNode {
public:
    CPrintingNode(std::string Msg = "")
        : m_Msg(std::move(Msg))
    {
        AddInputOutputFlowPin();
    }

protected:
    void Execute() override
    {
        std::cout << "Printing Node...  " << m_Msg << std::endl;
    }

    std::string m_Msg;
};

class CBranchingNode : public CExecuteNode {
public:
    CBranchingNode()
    {
        AddInputOutputFlowPin();
        EmplacePin<CDataPin>(true);
        EmplacePin<CFlowPin>(false);
    }

protected:
    void Execute() override
    {
        if (GetFlowOutputPins().size() > 1) [[likely]] {
            if (auto DataPin = GetInputPinsWith<EPinType::Data>();
                !DataPin.empty() && !static_cast<CDataPin*>(DataPin.front().get())->TryGet<bool>())

                m_DesiredOutputPin = 1;
        }
    }
};

class CSequenceNode : public CExecuteNode {
public:
    CSequenceNode()
    {
        AddInputOutputFlowPin();
    }

    void ExecuteNode() override
    {
        for (const auto* Pin : GetFlowOutputPins())
            if (*Pin)
                Pin->GetConnectedOwner()->As<CExecuteNode>()->ExecuteNode();
    }
};

class CGridPipline : public CRenderPipeline {
public:
    CGridPipline()
    {
        static auto Code = R"(
struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) world_pos: vec2<f32>,
}

struct Uniforms {
    view: mat4x4<f32>,
    proj: mat4x4<f32>,
}

@group(0) @binding(0) var<uniform> u: Uniforms;

// Matrix inverse function for 4x4 matrices
fn inverse_mat4(m: mat4x4<f32>) -> mat4x4<f32> {
    let a00 = m[0][0]; let a01 = m[0][1]; let a02 = m[0][2]; let a03 = m[0][3];
    let a10 = m[1][0]; let a11 = m[1][1]; let a12 = m[1][2]; let a13 = m[1][3];
    let a20 = m[2][0]; let a21 = m[2][1]; let a22 = m[2][2]; let a23 = m[2][3];
    let a30 = m[3][0]; let a31 = m[3][1]; let a32 = m[3][2]; let a33 = m[3][3];

    let b00 = a00 * a11 - a01 * a10;
    let b01 = a00 * a12 - a02 * a10;
    let b02 = a00 * a13 - a03 * a10;
    let b03 = a01 * a12 - a02 * a11;
    let b04 = a01 * a13 - a03 * a11;
    let b05 = a02 * a13 - a03 * a12;
    let b06 = a20 * a31 - a21 * a30;
    let b07 = a20 * a32 - a22 * a30;
    let b08 = a20 * a33 - a23 * a30;
    let b09 = a21 * a32 - a22 * a31;
    let b10 = a21 * a33 - a23 * a31;
    let b11 = a22 * a33 - a23 * a32;

    let det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;
    let inv_det = 1.0 / det;

    return mat4x4<f32>(
        vec4<f32>(
            (a11 * b11 - a12 * b10 + a13 * b09) * inv_det,
            (a02 * b10 - a01 * b11 - a03 * b09) * inv_det,
            (a31 * b05 - a32 * b04 + a33 * b03) * inv_det,
            (a22 * b04 - a21 * b05 - a23 * b03) * inv_det
        ),
        vec4<f32>(
            (a12 * b08 - a10 * b11 - a13 * b07) * inv_det,
            (a00 * b11 - a02 * b08 + a03 * b07) * inv_det,
            (a32 * b02 - a30 * b05 - a33 * b01) * inv_det,
            (a20 * b05 - a22 * b02 + a23 * b01) * inv_det
        ),
        vec4<f32>(
            (a10 * b10 - a11 * b08 + a13 * b06) * inv_det,
            (a01 * b08 - a00 * b10 - a03 * b06) * inv_det,
            (a30 * b04 - a31 * b02 + a33 * b00) * inv_det,
            (a21 * b02 - a20 * b04 - a23 * b00) * inv_det
        ),
        vec4<f32>(
            (a11 * b07 - a10 * b09 - a12 * b06) * inv_det,
            (a00 * b09 - a01 * b07 + a02 * b06) * inv_det,
            (a31 * b01 - a30 * b03 - a32 * b00) * inv_det,
            (a20 * b03 - a21 * b01 + a22 * b00) * inv_det
        )
    );
}

@vertex
fn vs_main(@builtin(vertex_index) vid: u32) -> VertexOutput {
    // Optimized: compute position directly from vertex_id
    // vid: 0->(-1,-1), 1->(1,-1), 2->(-1,1), 3->(1,1)
    let x = f32((vid & 1u) << 1u) - 1.0;
    let y = f32(vid & 2u) - 1.0;
    let pos = vec2<f32>(x, y);

    var out: VertexOutput;
    out.position = vec4<f32>(pos, 1.0, 1.0);
    out.world_pos = (inverse_mat4(u.proj * u.view) * vec4<f32>(pos, 0.0, 1.0)).xy;
    return out;
}

const BG: vec3<f32> = vec3<f32>(0.15);
const SUB: vec4<f32> = vec4<f32>(0.35, 0.35, 0.35, 0.3);
const MAIN: vec4<f32> = vec4<f32>(0.35, 0.35, 0.35, 1.0);
const AXIS: vec4<f32> = vec4<f32>(0.2, 0.8, 0.2, 1.0);

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    let d = fwidth(in.world_pos);
    let inv_d = 1.0 / d;

    let s = abs(fract(in.world_pos * 0.1 - 0.5) - 0.5) * 10.0;
    let sub = saturate(1.0 - min(s.x, s.y) * inv_d.x);

    let m = abs(fract(in.world_pos * 0.02 - 0.5) - 0.5) * 50.0;
    let main = saturate(1.5 - min(m.x, m.y) * inv_d.x);

    let axis = saturate(2.0 - min(abs(in.world_pos.x), abs(in.world_pos.y)) * inv_d.x);

    var c = vec4<f32>(BG, 1.0);
    c = mix(c, SUB, sub);
    c = mix(c, MAIN, main);
    return mix(c, AXIS, axis);
}
)";
        SetShaderCode(Code);
    }
};

int main()
{
    CBranchingNode BranchingNode;
    if (auto DataPins = BranchingNode.GetInputPinsWith<EPinType::Data>(); !DataPins.empty())
        DataPins.front()->As<CDataPin>()->Set(true);

    CPrintingNode Node1("true"), Node2("false");

    CSequenceNode SequenceNode;
    SequenceNode.EmplacePin<CFlowPin>(false)->ConnectPin(Node1.GetFlowInputPins().front());
    SequenceNode.EmplacePin<CFlowPin>(false)->ConnectPin(Node1.GetFlowInputPins().front());
    SequenceNode.EmplacePin<CFlowPin>(false)->ConnectPin(Node1.GetFlowInputPins().front());
    SequenceNode.EmplacePin<CFlowPin>(false)->ConnectPin(Node1.GetFlowInputPins().front());
    SequenceNode.EmplacePin<CFlowPin>(false)->ConnectPin(Node1.GetFlowInputPins().front());

    auto OutputPins = BranchingNode.GetFlowOutputPins();
    OutputPins[0]->ConnectPin(SequenceNode.GetFlowInputPins().front());
    OutputPins[1]->ConnectPin(Node2.GetFlowInputPins().front());

    BranchingNode.ExecuteNode();

    CBoardEditor Window;

    CDepthTexture DepthTexture { Window };

    CGridPipline GridPipline;
    GridPipline.CreatePipeline(Window);

    SSceneUniform SceneUniform;

    wgpu::Buffer sceneUniformBuffer;
    {
        wgpu::BufferDescriptor bufferDesc;
        bufferDesc.label = "Scene Uniform";
        bufferDesc.size = sizeof(SSceneUniform);
        bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
        sceneUniformBuffer = Window.GetDevice().CreateBuffer(&bufferDesc);
    }

    wgpu::BindGroup uniformBindingGroup;
    {
        std::vector<wgpu::BindGroupEntry> bindings(1);
        bindings[0].binding = 0;
        bindings[0].buffer = sceneUniformBuffer;
        bindings[0].offset = 0;
        bindings[0].size = sizeof(SSceneUniform);

        wgpu::BindGroupDescriptor bindGroupDesc { };
        bindGroupDesc.layout = GridPipline.GetBindGroups()[0];
        bindGroupDesc.entryCount = bindings.size();
        bindGroupDesc.entries = bindings.data();
        uniformBindingGroup = Window.GetDevice().CreateBindGroup(&bindGroupDesc);
    }

    Window.StartFrame();
    while (Window.ProcessEvent()) {

        if (const auto RenderContext = Window.GetRenderContext(DepthTexture)) {
            RenderContext.RenderPassEncoder.SetBindGroup(0, uniformBindingGroup, 0, nullptr);
            SceneUniform.Projection = glm::ortho(0.0f, (float)Window.GetWindowSize().x, (float)Window.GetWindowSize().y, 0.0f, 0.0f, 1.0f);
            SceneUniform.View = glm::translate(glm::mat4 { 1 }, glm::vec3(1920, 1080, 0) / 2.f);
            Window.GetQueue().WriteBuffer(sceneUniformBuffer, 0, &SceneUniform, sizeof(SSceneUniform));

            RenderContext.RenderPassEncoder.SetPipeline(GridPipline);
            RenderContext.RenderPassEncoder.Draw(4);

            Window.RenderBoard(RenderContext);
        }
    }

    return 0;
}
