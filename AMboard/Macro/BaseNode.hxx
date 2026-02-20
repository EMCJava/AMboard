//
// Created by LYS on 2/15/2026.
//

#pragma once

#include <memory>
#include <ranges>
#include <vector>

#include "Pin.hxx"

enum class ENodeType {
    Data,
    Execution
};

class CBaseNode {
public:
    virtual ~CBaseNode();

    template <typename PinTy>
    PinTy* EmplacePin(const bool IsInput)
    {
        auto* Result = static_cast<PinTy*>((IsInput ? m_InputPins : m_OutputPins).emplace_back(std::make_unique<PinTy>(this, IsInput)).get());
        OnPinModified();
        return Result;
    }

    const auto& GetInputPins() const noexcept { return m_InputPins; }
    const auto& GetOutputPins() const noexcept { return m_OutputPins; }

    template <EPinType PinTy>
    auto GetInputPinsWith() const noexcept
    {
        return m_InputPins | std::views::filter([](const auto& Pin) static { return *Pin == PinTy; });
    }

    virtual void OnPinModified() noexcept { }

    [[nodiscard]] operator ENodeType() const noexcept { return m_NodeType; } // NOLINT

    template <typename NodeTy>
        requires std::is_base_of_v<CBaseNode, NodeTy>
    [[nodiscard]] NodeTy* As() noexcept
    {
        return static_cast<NodeTy*>(this);
    }

protected:
    ENodeType m_NodeType = ENodeType::Data;

    std::vector<std::unique_ptr<CPin>> m_InputPins;
    std::vector<std::unique_ptr<CPin>> m_OutputPins;

    bool m_IsDestructing = false;
};
