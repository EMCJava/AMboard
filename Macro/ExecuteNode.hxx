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

    const auto& GetFlowInputPins() const noexcept { return m_InFlowingPin; }
    const auto& GetFlowOutputPins() const noexcept { return m_OutFlowingPin; }

protected:
    void AddInputOutputFlowPin();

    void OnPinModified() noexcept override;

    virtual void Execute();

    std::vector<CPin*> m_InFlowingPin;
    size_t m_DesiredOutputPin = 0;
    std::vector<CPin*> m_OutFlowingPin;
};
