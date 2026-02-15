//
// Created by LYS on 2/15/2026.
//

#pragma once
#include <type_traits>

enum class EPinType {
    None,
    Data,
    Flow
};

class CBaseNode;
class CPin {

protected:
    CPin* ReleasePin() noexcept;

    virtual CPin* SetPin(CPin* NewPin) noexcept;

public:
    CPin(CBaseNode* Owner);
    virtual ~CPin();

    virtual void BreakPin();
    bool ConnectPin(CPin* NewPin) noexcept;

    [[nodiscard]] virtual bool Compatible(CPin* NewPin) noexcept;

    [[nodiscard]] CBaseNode* GetOwner() const noexcept { return m_Owner; }
    [[nodiscard]] CBaseNode* GetConnectedOwner() const;
    [[nodiscard]] operator bool() const noexcept { return m_ConnectedPin != nullptr; } // NOLINT
    [[nodiscard]] operator EPinType() const noexcept { return m_PinType; } // NOLINT

    template <typename PinTy>
        requires std::is_base_of_v<CPin, PinTy>
    [[nodiscard]] PinTy* As() noexcept
    {
        return static_cast<PinTy*>(this);
    }

protected:
    EPinType m_PinType = EPinType::None;

    CBaseNode* m_Owner = nullptr;
    CPin* m_ConnectedPin = nullptr;
};