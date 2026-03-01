//
// Created by LYS on 2/15/2026.
//

#pragma once

#include <memory>
#include <ranges>
#include <vector>

#include "MacroDefines.hxx"
#include "Pin.hxx"

#include <algorithm>
#include <string>

enum class ENodeType {
    Data,
    Execution
};

class MACRO_API CBaseNode {
public:
    CBaseNode() = default;

    CBaseNode(const CBaseNode&) = delete;
    CBaseNode(CBaseNode&&) = delete;
    CBaseNode& operator=(const CBaseNode&) = delete;
    CBaseNode& operator=(CBaseNode&&) = delete;

    virtual ~CBaseNode();

    virtual std::string_view GetCategory() noexcept
    {
        return "Default";
    }

    template <typename PinTy>
    PinTy* EmplacePin(const bool IsInput)
    {
        auto* Result = static_cast<PinTy*>((IsInput ? m_InputPins : m_OutputPins).emplace_back(std::make_unique<PinTy>(this, IsInput)).get());
        for (const auto& Func : m_OnPinChanges)
            Func(Result, true);
        return Result;
    }

    template <typename PinTy>
        requires std::is_base_of_v<CPin, PinTy>
    bool ErasePin(const PinTy* PinPtr) noexcept
    {
        if (PinPtr == nullptr) [[unlikely]]
            return false;

        auto& Pins = PinPtr->IsInputPin() ? m_InputPins : m_OutputPins;
        if (auto It = std::ranges::find_if(Pins, [PinPtr](auto&& Pin) { return Pin.get() == static_cast<const CPin*>(PinPtr); }); It != Pins.end()) {

            const auto PinExtracted = std::move(*It);
            Pins.erase(It);

            for (const auto& Func : m_OnPinChanges)
                Func(PinExtracted.get(), false);

            return true;
        }

        return false;
    }

    const auto& GetInputPins() const noexcept { return m_InputPins; }
    const auto& GetOutputPins() const noexcept { return m_OutputPins; }

    template <EPinType PinTy>
    auto GetInputPinsWith() const noexcept
    {
        return m_InputPins | std::views::filter([](const auto& Pin) static { return *Pin == PinTy; });
    }

    template <EPinType PinTy>
    auto GetOutputPinsWith() const noexcept
    {
        return m_OutputPins | std::views::filter([](const auto& Pin) static { return *Pin == PinTy; });
    }

    auto AddOnPinChanges(auto&& Func) { return m_OnPinChanges.emplace_back(Func); }

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

    std::list<std::function<void(CPin* TargetPin, bool NewPin)>> m_OnPinChanges;

    bool m_IsDestructing = false;
};
