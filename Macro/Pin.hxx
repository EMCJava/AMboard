//
// Created by LYS on 2/15/2026.
//

#pragma once

enum class EPinType {
    Data,
    Flow
};

class CBaseNode;
class CPin {

    CPin* ReleasePin() noexcept;

protected:
    virtual CPin* SetPin(CPin* NewPin) noexcept;

public:
    CPin(CBaseNode* Owner);
    virtual ~CPin();

    void BreakPin();
    bool ConnectPin(CPin* NewPin) noexcept;

    [[nodiscard]] virtual bool Compatible(CPin* NewPin) noexcept;

    [[nodiscard]] CBaseNode* GetOwner() const noexcept { return m_Owner; }
    [[nodiscard]] CBaseNode* GetConnectedOwner() const;
    [[nodiscard]] operator bool() const noexcept { return m_ConnectedPin != nullptr; } // NOLINT
    [[nodiscard]] operator EPinType() const noexcept { return m_PinType; } // NOLINT

protected:
    EPinType m_PinType = EPinType::Data;

    CBaseNode* m_Owner = nullptr;
    CPin* m_ConnectedPin = nullptr;
};