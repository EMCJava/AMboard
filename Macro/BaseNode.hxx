//
// Created by LYS on 2/15/2026.
//

#pragma once

#include <memory>
#include <vector>

#include "Pin.hxx"

enum class ENodeType {
    Data,
    Execution
};

class CBaseNode {
public:
    virtual ~CBaseNode() = default;

    template <typename PinTy>
    auto* EmplacePin(const bool IsInput)
    {
        if (IsInput) {
            auto Result = m_InputPins.emplace_back(std::make_unique<PinTy>(this)).get();
            OnPinModified();
            return Result;
        } else { // NOLINT
            auto Result = m_OutputPins.emplace_back(std::make_unique<PinTy>(this)).get();
            OnPinModified();
            return Result;
        }
    }

    const auto& GetInputPins() const noexcept { return m_InputPins; }
    const auto& GetOutputPins() const noexcept { return m_OutputPins; }

    virtual void OnPinModified() noexcept { }

    [[nodiscard]] operator ENodeType() const noexcept { return m_NodeType; } // NOLINT

protected:
    ENodeType m_NodeType = ENodeType::Data;

    std::vector<std::unique_ptr<CPin>> m_InputPins;
    std::vector<std::unique_ptr<CPin>> m_OutputPins;
};
