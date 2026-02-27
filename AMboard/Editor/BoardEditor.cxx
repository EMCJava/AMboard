//
// Created by LYS on 2/16/2026.
//

#include "BoardEditor.hxx"
#include "GridPipline.hxx"
#include "NodeRender/NodeRenderer.hxx"

#include <AMboard/CustomNodes/CustomNodeManager.hxx>
#include <AMboard/Macro/DataPin.hxx>
#include <AMboard/Macro/ExecuteNode.hxx>
#include <AMboard/Macro/FlowPin.hxx>

#include <Util/Assertions.hxx>

#include <Interface/Font/TextRenderSystem.hxx>

#include <AMboard/Macro/BaseNode.hxx>

#include <GLFW/glfw3.h>

#include <glm/gtx/norm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>

#include <yaml-cpp/yaml.h>

#include <nfd.h>

#include <fstream>
#include <random>

void NodeDefaultDeleter(CBaseNode* Node)
{
    if (Node != nullptr) {
        delete Node;
    }
}

glm::vec2 SEditorNodeContext::MoveLogicalPosition(const glm::vec2& Delta, const float SnapValue) noexcept
{
    LogicalPosition += Delta;
    return GetDisplayPosition(SnapValue);
}

glm::vec2 SEditorNodeContext::GetDisplayPosition(const float SnapValue) const noexcept
{
    return round(LogicalPosition / SnapValue) * SnapValue;
}

glm::vec2 CBoardEditor::ScreenToWorld(const glm::vec2& ScreenPos) const noexcept
{
    return ScreenPos / m_CameraZoom + m_CameraOffset;
}

std::optional<size_t> CBoardEditor::TryRegisterConnection(CPin* OutputPin, CPin* InputPin)
{
    CHECK(OutputPin != nullptr && InputPin != nullptr)

    if (!OutputPin->IsConnected(InputPin)) [[unlikely]] {
        if (!OutputPin->ConnectPin(InputPin)) [[unlikely]] {
            return std::nullopt;
        }
    }

    return m_ConnectionIdMapping.at({ OutputPin, InputPin });
}

size_t CBoardEditor::CreateNode(const std::string& NodeExtName, const glm::vec2& Position, const uint32_t HeaderColor)
{
    return RegisterNode(m_CustomNodeLoader->CreateNodeExt(NodeExtName), NodeExtName, Position, HeaderColor);
}

size_t CBoardEditor::RegisterNode(NodeStorage Node, const std::string& Title, const glm::vec2& Position, const uint32_t HeaderColor)
{
    if (!Node)
        return -1;

    const auto NodeId = m_NodeRenderer->CreateNode(Title, Position, HeaderColor);
    if (NodeId >= m_Nodes.size())
        m_Nodes.resize(NodeId + 1);
    m_Nodes[NodeId] = { .Node = std::move(Node), .LogicalPosition = Position };
    m_NodeRenderer->SetNodePosition(NodeId, m_Nodes[NodeId].GetDisplayPosition(m_NodeSnapValue));

    for (const auto& Pin : m_Nodes[NodeId].Node->GetInputPins()) {
        MAKE_SURE(m_PinIdMapping.insert({ Pin.get(), m_NodeRenderer->AddInputPin(NodeId, *Pin == EPinType::Flow) }).second);

        Pin->AddOnConnectionChanges([this](CPin* This, CPin* Other, const bool IsConnect) {
            if (IsConnect) {
                const auto Key = This->IsInputPin() ? std::pair { Other, This } : std::pair { This, Other };
                if (const auto It = m_ConnectionIdMapping.find(Key); It == m_ConnectionIdMapping.end()) {
                    m_ConnectionIdMapping
                        .insert(It, { Key, m_NodeRenderer->LinkPin(m_PinIdMapping.left.at(Key.first), m_PinIdMapping.left.at(Key.second)) });
                }

                m_NodeRenderer->ConnectPin(m_PinIdMapping.left.at(This));
            } else {
                const auto Key = This->IsInputPin() ? std::pair { Other, This } : std::pair { This, Other };
                if (const auto It = m_ConnectionIdMapping.find(Key); It != m_ConnectionIdMapping.end()) {
                    m_NodeRenderer->UnlinkPin(It->second);
                    m_ConnectionIdMapping.erase(It);
                }

                if (!*This) {
                    m_NodeRenderer->DisconnectPin(m_PinIdMapping.left.at(This));
                }
            }
        });
    }
    for (const auto& Pin : m_Nodes[NodeId].Node->GetOutputPins()) {
        MAKE_SURE(m_PinIdMapping.insert({ Pin.get(), m_NodeRenderer->AddOutputPin(NodeId, *Pin == EPinType::Flow) }).second);

        Pin->AddOnConnectionChanges([this](CPin* This, CPin* Other, const bool IsConnect) {
            if (IsConnect) {
                const auto Key = This->IsInputPin() ? std::pair { Other, This } : std::pair { This, Other };
                if (const auto It = m_ConnectionIdMapping.find(Key); It == m_ConnectionIdMapping.end()) {
                    m_ConnectionIdMapping
                        .insert(It, { Key, m_NodeRenderer->LinkPin(m_PinIdMapping.left.at(Key.first), m_PinIdMapping.left.at(Key.second)) });
                }

                m_NodeRenderer->ConnectPin(m_PinIdMapping.left.at(This));
            } else {
                const auto Key = This->IsInputPin() ? std::pair { Other, This } : std::pair { This, Other };
                if (const auto It = m_ConnectionIdMapping.find(Key); It != m_ConnectionIdMapping.end()) {
                    m_NodeRenderer->UnlinkPin(It->second);
                    m_ConnectionIdMapping.erase(It);
                }

                if (!*This) {
                    m_NodeRenderer->DisconnectPin(m_PinIdMapping.left.at(This));
                }
            }
        });
    }

    return NodeId;
}

void CBoardEditor::UnregisterNode(const size_t NodeId)
{
    m_NodeRenderer->RemoveNode(NodeId);
    m_Nodes[NodeId] = { { nullptr, NodeDefaultDeleter } };
}

void CBoardEditor::MoveCanvas(const glm::vec2& Delta) noexcept
{
    m_CameraOffset += Delta;
    m_ScreenUniformDirty = true;
}

void CBoardEditor::SaveCanvas() noexcept
{
    NFD_Init();

    nfdchar_t* SavePath;

    // prepare filters for the dialog
    static constexpr nfdfilteritem_t FilterItem[] = { { "Board", "yaml" } };
    if (NFD_SaveDialog(&SavePath, FilterItem, 1, std::filesystem::current_path().string().c_str(), "Board.yaml") == NFD_OKAY) {
        SaveCanvasTo(m_CurrentBoardPath = SavePath);
        NFD_FreePath(SavePath);
    }

    NFD_Quit();
}

void CBoardEditor::SaveCanvasTo(const std::filesystem::path& Path) noexcept
{
    YAML::Node Root;

    std::random_device rd;
    std::mt19937_64 Salt(rd());

    std::unordered_map<CBaseNode*, std::size_t> NodeSaltMap;
    std::unordered_map<const CPin*, std::size_t> PinHashMap;

    /// Scan all node and pin to generate hash
    for (const auto& [Left, Right] : m_NodeRenderer->GetValidRange()) {
        for (auto i = Left; i <= Right; ++i) {
            const uint64_t NodeSalt = NodeSaltMap[m_Nodes[i].Node.get()] = Salt();

            std::mt19937 rng(NodeSalt);
            std::uniform_int_distribution<uint64_t> dist;

            /// Lets just assume no collision
            for (const auto& Pin : m_Nodes[i].Node->GetInputPins())
                MAKE_SURE(PinHashMap.insert({ Pin.get(), dist(rng) }).second);
            for (const auto& Pin : m_Nodes[i].Node->GetOutputPins())
                MAKE_SURE(PinHashMap.insert({ Pin.get(), dist(rng) }).second);
        }
    }

    /// Write node
    for (const auto& [Left, Right] : m_NodeRenderer->GetValidRange()) {
        for (auto i = Left; i <= Right; ++i) {

            YAML::Node Node;

            Node["ID"] = m_NodeRenderer->GetTitle(i);

            const auto Position = m_NodeRenderer->GetNodePosition(i);
            Node["pos"].SetStyle(YAML::EmitterStyle::Flow);
            Node["pos"].push_back(Position.x);
            Node["pos"].push_back(Position.y);

            Node["header_color"] = m_NodeRenderer->GetHeaderColor(i);

            Node["salt"] = NodeSaltMap[m_Nodes[i].Node.get()];

            Root["Nodes"].push_back(Node);
        }
    }

    /// Write connections
    for (const auto& [Left, Right] : m_NodeRenderer->GetValidRange()) {
        for (auto i = Left; i <= Right; ++i) {
            for (const auto& Pin : m_Nodes[i].Node->GetOutputPins()) {
                for (const auto* OtherPin : Pin->GetConnections()) {
                    YAML::Node Link;
                    Link.SetStyle(YAML::EmitterStyle::Flow);
                    Link.push_back(PinHashMap.at(Pin.get()));
                    Link.push_back(PinHashMap.at(OtherPin));

                    Root["Links"].push_back(Link);
                }
            }
        }
    }

    std::ofstream fout(Path);
    fout << Root;
}

CBoardEditor::CBoardEditor()
{
    m_GridPipline = std::make_unique<CGridPipline>();
    m_GridPipline->CreatePipeline(*this);

    {
        m_SceneUniform = std::make_unique<SSceneUniform>();

        wgpu::BufferDescriptor bufferDesc;
        bufferDesc.label = "Scene Uniform";
        bufferDesc.size = sizeof(SSceneUniform);
        bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
        m_SceneUniformBuffer = GetDevice().CreateBuffer(&bufferDesc);

        std::vector<wgpu::BindGroupEntry> bindings(1);
        bindings[0].binding = 0;
        bindings[0].buffer = m_SceneUniformBuffer;
        bindings[0].offset = 0;
        bindings[0].size = sizeof(SSceneUniform);

        wgpu::BindGroupDescriptor bindGroupDesc { };
        bindGroupDesc.layout = m_GridPipline->GetBindGroups()[0];
        bindGroupDesc.entryCount = bindings.size();
        bindGroupDesc.entries = bindings.data();
        m_UniformBindingGroup = GetDevice().CreateBindGroup(&bindGroupDesc);
    }

    m_NodeRenderer = std::make_unique<CNodeRenderer>(this);

    m_CustomNodeLoader = std::make_unique<CCustomNodeLoader>("NodeExts");

    {
        YAML::Node Graph = YAML::LoadFile("graph.yaml");

        std::unordered_map<uint64_t, CPin*> PinHashMap;

        for (const auto& Node : Graph["Nodes"]) {
            const auto Name = Node["ID"].as<std::string>();
            const auto Position = Node["pos"].IsDefined() ? glm::vec2 { Node["pos"][0].as<float>(), Node["pos"][1].as<float>() } : glm::vec2(0.0f, 0.0f);
            const auto HeaderColor = Node["header_color"].as<uint32_t>(0xAAAAAA88);

            const auto NodeId = CreateNode(Name, Position, HeaderColor);

            if (Name == "Entrance Node") [[unlikely]] {
                m_EntranceNode = NodeId;
            }

            std::mt19937 rng(Node["salt"].as<uint64_t>());
            std::uniform_int_distribution<uint64_t> dist;

            for (const auto& Pin : m_Nodes[NodeId].Node->GetInputPins())
                MAKE_SURE(PinHashMap.insert({ dist(rng), Pin.get() }).second);
            for (const auto& Pin : m_Nodes[NodeId].Node->GetOutputPins())
                MAKE_SURE(PinHashMap.insert({ dist(rng), Pin.get() }).second);
        }

        for (const auto& Link : Graph["Links"]) {
            const auto OutputId = Link[0].as<uint64_t>();
            const auto InputId = Link[1].as<uint64_t>();

            const auto OutputIt = PinHashMap.find(OutputId);
            const auto InputIt = PinHashMap.find(InputId);

            if (OutputIt == PinHashMap.end()) [[unlikely]] {
                spdlog::error("Pin #{} missing", OutputId);
                continue;
            }
            if (InputIt == PinHashMap.end()) [[unlikely]] {
                spdlog::error("Pin #{} missing", InputId);
                continue;
            }

            OutputIt->second->ConnectPin(InputIt->second);
        }
    }
}

CBoardEditor::~CBoardEditor() = default;

CWindowBase::EWindowEventState CBoardEditor::ProcessEvent()
{
    if (const auto EventStage = CWindowBase::ProcessEvent();
        EventStage != EWindowEventState::Normal) [[unlikely]] {
        return EventStage;
    }

    const auto MouseCurrentPos = GetInputManager().GetCursorPosition();
    const auto MouseDeltaPos = GetInputManager().GetDeltaCursor();
    const glm::vec2 MouseWorldPos = ScreenToWorld(MouseCurrentPos);

    std::optional<std::size_t> CursorHoveringNode;
    std::optional<std::size_t> CursorHoveringPin;

    /// Node interaction pre-compute
    for (const auto& [Left, Right] : m_NodeRenderer->GetValidRange()) {
        for (auto i = Left; i <= Right; ++i) {
            if (m_NodeRenderer->InBound(i, MouseWorldPos)) [[unlikely]] {
                CursorHoveringNode = i;

                CursorHoveringPin = m_NodeRenderer->GetHoveringPin(i, MouseWorldPos);

                if (m_LastHoveringPin != CursorHoveringPin) {
                    if (m_LastHoveringPin.has_value()) /// Deselect
                        m_NodeRenderer->ToggleHoverPin(*m_LastHoveringPin);
                    if (CursorHoveringPin.has_value()) /// Select
                        m_NodeRenderer->HoverPin(*CursorHoveringPin);
                }

                m_LastHoveringPin = CursorHoveringPin;
                break;
            }
        }
    }

    /// Save
    {
        if (GetInputManager().GetKeyboardButtons().ConsumeEvent({ GLFW_KEY_S, GLFW_PRESS, GLFW_MOD_SHIFT | GLFW_MOD_CONTROL })) {
            SaveCanvas();
        }

        if (GetInputManager().GetKeyboardButtons().ConsumeEvent({ GLFW_KEY_S, GLFW_PRESS, GLFW_MOD_CONTROL })) {
            if (exists(m_CurrentBoardPath)) {
                SaveCanvasTo(m_CurrentBoardPath);
            } else {
                SaveCanvas();
            }
        }
    }

    /// Draging virtual node
    if (m_DraggingPin.has_value()) {
        m_NodeRenderer->SetNodePosition(m_VirtualNodeForPinDrag, MouseWorldPos);
    }

    /// Drag canvas
    if (GetInputManager().GetMouseButtons().ConsumeHoldEvent(this, GLFW_MOUSE_BUTTON_MIDDLE)) {
        MoveCanvas(-glm::vec2 { MouseDeltaPos } / m_CameraZoom);
    }

    /// Zoom canvas
    if (const auto Scroll = GetInputManager().GetDeltaScroll(); Scroll != 0) {
        const float ZoomFactor = 1.0f + (Scroll * 0.1f);
        const float NewZoom = glm::clamp(m_CameraZoom * ZoomFactor, 0.1f, 5.0f);

        // Zoom towards mouse position
        const glm::vec2 WorldPosBefore = MouseWorldPos;
        m_CameraZoom = NewZoom;
        const glm::vec2 WorldPosAfter = ScreenToWorld(MouseCurrentPos);
        m_CameraOffset += (WorldPosBefore - WorldPosAfter);

        m_ScreenUniformDirty = true;
    }

    /// Execute Node
    if (m_EntranceNode.has_value() && GetInputManager().GetKeyboardButtons().ConsumeEvent(GLFW_KEY_R)) {
        if (auto* ENode = dynamic_cast<CExecuteNode*>(m_Nodes[*m_EntranceNode].Node.get())) {
            ENode->ExecuteNode();
        }
    }

    /// Remove Node
    if (GetInputManager().GetKeyboardButtons().ConsumeEvent(GLFW_KEY_DELETE)) {
        if (m_SelectedNode.has_value()) {
            m_Nodes[*m_SelectedNode].Node.reset();
            m_NodeRenderer->RemoveNode(*m_SelectedNode);
            if (m_SelectedNode == m_EntranceNode) [[unlikely]] {
                m_EntranceNode.reset();
            }
            m_SelectedNode.reset();
        }
    }

    /// Create Node
    if (GetInputManager().GetMouseButtons().ConsumeEvent(GLFW_MOUSE_BUTTON_RIGHT)) {
        auto Node = NodeStorage { new CExecuteNode, NodeDefaultDeleter };
        Node->EmplacePin<CFlowPin>(true);
        Node->EmplacePin<CFlowPin>(false);
        Node->EmplacePin<CDataPin>(true);
        Node->EmplacePin<CDataPin>(false);

        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<uint32_t> distrib(0, 0xFFFFFF);

        RegisterNode(std::move(Node), "Identity", MouseWorldPos, distrib(gen) << 16 | 0x88);
    }

    /// Select Node (release mouse)
    if (GetInputManager().GetMouseButtons().ConsumeEvent({ GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE })) {

        /// Click, not drag
        if (glm::length2(glm::vec2 { MouseCurrentPos - *MouseStartClickPos }) < 3 * 3) {

            if (!m_DraggingPin.has_value() && CursorHoveringNode.has_value()) [[unlikely]] {

                if (m_SelectedNode.has_value()) {
                    m_NodeRenderer->ToggleSelect(*m_SelectedNode);
                }

                if (m_SelectedNode.has_value() && *m_SelectedNode == *CursorHoveringNode) { /// Deselect
                    m_SelectedNode.reset();
                } else {
                    m_SelectedNode = *CursorHoveringNode;
                    m_NodeRenderer->Select(*CursorHoveringNode);
                }
            }
        }

        /// End of pin drag
        if (m_DraggingPin.has_value()) {
            bool IsPinConnected = false;

            /// End of drag pin
            if (CursorHoveringPin.has_value() && *m_DraggingPin != *CursorHoveringPin) {
                std::pair Pins = { m_PinIdMapping.right.at(*m_DraggingPin), m_PinIdMapping.right.at(*CursorHoveringPin) };
                if (Pins.first->IsInputPin())
                    std::swap(Pins.first, Pins.second);
                IsPinConnected = TryRegisterConnection(Pins.first, Pins.second).has_value();
            }

            /// No new connection is created, maybe there exist an old one?
            if (!IsPinConnected) {
                /// Not connected
                if (!*m_PinIdMapping.right.at(*m_DraggingPin)) {
                    m_NodeRenderer->DisconnectPin(*m_DraggingPin);
                }
            }

            /// Clear pin selection
            m_DraggingPin.reset();

            /// Remove virtual node for drag visualization
            m_NodeRenderer->UnlinkPin(m_VirtualConnectionForPinDrag);
            m_NodeRenderer->RemoveNode(m_VirtualNodeForPinDrag);
        }

        MouseStartClickPos.reset();
    }

    /// Hold primary mouse
    if (GetInputManager().GetMouseButtons().ConsumeHoldEvent(this, GLFW_MOUSE_BUTTON_LEFT)) {

        /// First frame clicking
        if (!MouseStartClickPos.has_value()) {
            MouseStartClickPos = MouseCurrentPos;

            bool FirstClickHasUsed = false;

            /// Dragging takes priority
            m_ControlDraggingCanvas = false;
            if (GetInputManager().GetKeyboardButtons().IsKeyDown(GLFW_KEY_LEFT_CONTROL)) [[unlikely]] {
                m_ControlDraggingCanvas = true;
                FirstClickHasUsed = true;
            }

            /// Pin selecting takes priority
            if (!FirstClickHasUsed && CursorHoveringPin.has_value()) {
                CHECK(!m_DraggingPin.has_value())

                m_DraggingPin = CursorHoveringPin;
                m_NodeRenderer->ConnectPin(*CursorHoveringPin);

                m_VirtualNodeForPinDrag = m_NodeRenderer->CreateVirtualNode(MouseWorldPos);
                m_VirtualConnectionForPinDrag = m_NodeRenderer->LinkVirtualPin(*m_DraggingPin, m_VirtualNodeForPinDrag);

                FirstClickHasUsed = true;
            }

            /// Drag selected pin
            m_DraggingNode = false;
            if (!FirstClickHasUsed && m_SelectedNode.has_value()) {
                if (m_SelectedNode == CursorHoveringNode) {
                    m_DraggingNode = true;
                    NodeDragThreshold = 3 * 3;
                }
            }

        } else if (m_ControlDraggingCanvas) {
            MoveCanvas(-glm::vec2 { MouseDeltaPos } / m_CameraZoom);
        } else if (m_DraggingNode) {

            if (NodeDragThreshold.has_value()) {
                if (glm::length2(glm::vec2 { MouseCurrentPos - *MouseStartClickPos }) > *NodeDragThreshold) {
                    NodeDragThreshold.reset();
                }
            }

            if (!NodeDragThreshold.has_value()) {
                if (MouseDeltaPos.x || MouseDeltaPos.y) {
                    m_NodeRenderer->SetNodePosition(*m_SelectedNode, m_Nodes[*m_SelectedNode].MoveLogicalPosition(glm::vec2 { MouseDeltaPos } / m_CameraZoom, m_NodeSnapValue));
                }
            }
        }
    }

    return EWindowEventState::Normal;
}

void CBoardEditor::RenderBoard(const SRenderContext& RenderContext)
{
    RenderContext.RenderPassEncoder.SetBindGroup(0, m_UniformBindingGroup, 0, nullptr);

    if (m_ScreenUniformDirty) {
        m_SceneUniform->Projection = glm::ortho(0.0f, (float)GetWindowSize().x, (float)GetWindowSize().y, 0.0f, 0.0f, 1.0f);
        m_SceneUniform->View = glm::translate(glm::scale(glm::mat4 { 1 }, glm::vec3(m_CameraZoom, m_CameraZoom, 1.0f)), glm::vec3(-m_CameraOffset, 0));
        GetQueue().WriteBuffer(m_SceneUniformBuffer, 0, m_SceneUniform.get(), sizeof(SSceneUniform));
        m_ScreenUniformDirty = false;
    }

    RenderContext.RenderPassEncoder.SetPipeline(*m_GridPipline);
    RenderContext.RenderPassEncoder.Draw(4);

    m_NodeRenderer->Render(RenderContext);
}

std::size_t CBoardEditor::PinPtrPairHash::operator()(const std::pair<void*, void*>& p) const noexcept
{
    const auto h1 = std::hash<void*> { }(p.first);
    const auto h2 = std::hash<void*> { }(p.second);

    // The "Magic Number" 0x9e3779b9 is the golden ratio,
    // which spreads bits randomly to avoid collisions.
    return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
}