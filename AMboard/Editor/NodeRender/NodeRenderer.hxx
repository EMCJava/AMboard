//
// Created by LYS on 2/18/2026.
//

#pragma once

#include "AMboard/Editor/BoardEditor.hxx"

#include <Util/RangeManager.hxx>

#include <glm/vec2.hpp>

#include <dawn/webgpu_cpp.h>

#include <memory>
#include <optional>

struct SCommonNodeSSBO {
    glm::vec2 Position;
    uint32_t State;
    uint8_t Padding[4];
};

struct SNodeAdditionalSourceHandle {
    static constexpr glm::vec2 TitleOffset = { 10, 10 };

    std::optional<std::list<STextGroupHandle>::iterator> TitleText;

    std::vector<size_t> InputPins;
    std::vector<size_t> OutputPins;

    void UnRegister(const CNodeRenderer* Renderer);
};

class CDynamicGPUBuffer;
class CNodeRenderer {

    void CreateCommonBindingGroup();

    size_t NextFreeNode();

public:
    CNodeRenderer(const CWindowBase* Window);
    ~CNodeRenderer();

    void WriteToNode(size_t Id, const std::string& Title, const glm::vec2& Position, uint32_t HeaderColor, std::optional<glm::vec2> NodeSize = std::nullopt);

    uint32_t GetHeaderColor(size_t Id) const noexcept;

    template <typename Self>
    decltype(auto) GetTitle(this Self&& s, size_t Id) noexcept { return s.m_NodeResourcesHandles[Id].TitleText.value()->Text; }

    void Select(size_t Id) const;
    void ToggleSelect(size_t Id) const;

    void ConnectPin(size_t Id) const;
    void DisconnectPin(size_t Id) const;
    void ToggleConnectPin(size_t Id) const;

    void HoverPin(size_t Id) const;
    void ToggleHoverPin(size_t Id) const;

    const glm::vec2& GetNodePosition(size_t Id) const noexcept;
    void SetNodePosition(size_t Id, const glm::vec2& Position) const;
    glm::vec2 MoveNode(size_t Id, const glm::vec2& Delta) const;

    size_t AddInputPin(size_t Id, bool IsExecutionPin);
    size_t AddOutputPin(size_t Id, bool IsExecutionPin);

    size_t LinkVirtualPin(size_t Id1, size_t NodeId);
    size_t LinkPin(size_t Id1, size_t Id2);
    void UnlinkPin(size_t Id) noexcept;

    size_t CreateVirtualNode(const glm::vec2& Position);
    size_t CreateNode(const std::string& Title, const glm::vec2& Position, uint32_t HeaderColor);
    void RemoveNode(size_t Id);

    [[nodiscard]] bool InBound(size_t Id, const glm::vec2& Position) const;

    /// Assuming the position is within bound of the node, find the hovering pin
    [[nodiscard]] std::optional<std::size_t> GetHoveringPin(size_t HoveringNodeId, const glm::vec2& Position) const;

    template <typename Self>
    [[nodiscard]] decltype(auto) GetValidRange(this Self&& s) noexcept { return s.m_ValidIdRange; }

    void Render(const SRenderContext& RenderContext);

protected:
    const CWindowBase* m_Window;

    size_t m_IdCount = 0;
    CRangeManager m_ValidIdRange;
    std::vector<SNodeAdditionalSourceHandle> m_NodeResourcesHandles;

    wgpu::BindGroup m_CommonNodeSSBOBindingGroup;
    std::unique_ptr<CDynamicGPUBuffer> m_CommonNodeSSBOBuffer;

    std::unique_ptr<class CNodeBackgroundPipline> m_NodeBackgroundPipline;
    std::unique_ptr<class CNodeTextRenderPipline> m_NodeTextPipline;
    std::unique_ptr<class CNodePinPipline> m_NodePinPipline;
    std::unique_ptr<class CNodeConnectionPipline> m_NodeConnectionPipline;

    friend SNodeAdditionalSourceHandle;
};
