//
// Created by LYS on 2/15/2026.
//

#pragma once

#include <functional>
#include <list>
#include <type_traits>

#include <unordered_set>

#include "MacroDefines.hxx"

enum class EPinType {
    None,
    Data,
    Flow
};

class CBaseNode;
class MACRO_API CPin {

protected:
    virtual void AddPin(CPin* NewPin) noexcept;
    virtual void PreConnectPin(CPin* NewPin) noexcept { }

public:
    CPin(CBaseNode* Owner, bool IsInputPin) noexcept;

    CPin(const CPin&) = delete;
    CPin(CPin&&) = delete;
    CPin& operator=(const CPin&) = delete;
    CPin& operator=(CPin&&) = delete;

    virtual ~CPin();

    virtual bool ConnectPin(CPin* NewPin) noexcept;
    virtual bool DisconnectPin(CPin* TargetPin) noexcept;

    [[nodiscard]] CPin* GetTheOnlyPin() const;

    [[nodiscard]] bool IsConnected(CPin* TargetPin) const noexcept;
    [[nodiscard]] virtual bool Compatible(CPin* NewPin) noexcept;

    [[nodiscard]] bool IsInputPin() const noexcept { return m_IsInputPin; }
    [[nodiscard]] CBaseNode* GetOwner() const noexcept { return m_Owner; }
    [[nodiscard]] operator bool() const noexcept { return !m_ConnectedPins.empty(); } // NOLINT
    [[nodiscard]] operator EPinType() const noexcept { return m_PinType; } // NOLINT

    auto AddOnConnectionChanges(auto&& Func)
    {
        return m_OnConnectionChanges.emplace_back(Func);
    }

    template <typename PinTy>
        requires std::is_base_of_v<CPin, PinTy>
    [[nodiscard]] PinTy* As() noexcept
    {
        return static_cast<PinTy*>(this);
    }

protected:
    bool m_IsInputPin;
    EPinType m_PinType = EPinType::None;

    CBaseNode* m_Owner = nullptr;

    std::unordered_set<CPin*> m_ConnectedPins;

    std::list<std::function<void(CPin* This, CPin* Other, bool IsConnect)>> m_OnConnectionChanges;
};