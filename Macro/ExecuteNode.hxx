//
// Created by LYS on 2/15/2026.
//

#pragma once

#include "BaseNode.hxx"
#include "FlowPin.hxx"

#include <ranges>

static constexpr auto FlowPinFilter = std::views::filter([](const auto& Pin) static { return *Pin == EPinType::Flow; });
static constexpr auto FlowPinTransform = std::views::transform([](const auto& Pin) static { return static_cast<CFlowPin*>(Pin.get()); });

class CExecuteNode : public CBaseNode {

public:
    CExecuteNode();

    void ExecuteNode();

    auto GetFlowInputPins() const noexcept { return GetInputPins() | FlowPinFilter | FlowPinTransform; }
    auto GetFlowOutputPins() const noexcept { return GetOutputPins() | FlowPinFilter | FlowPinTransform; }

protected:
    void AddInputOutputFlowPin();

    void OnPinModified() noexcept override;

    virtual void Execute();

    size_t m_DesiredOutputPin = 0;
    std::vector<CExecuteNode*> m_OutFlowingNode;
};
