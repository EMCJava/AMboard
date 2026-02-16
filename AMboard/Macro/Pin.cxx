//
// Created by LYS on 2/15/2026.
//

#include "Pin.hxx"

#include <utility>
#include <stdexcept>

CPin* CPin::SetPin(CPin* NewPin) noexcept
{
    std::swap(NewPin, m_ConnectedPin);
    return NewPin;
}

CPin* CPin::ReleasePin() noexcept
{
    return SetPin(nullptr);
}

CPin::CPin(CBaseNode* Owner)
    : m_Owner(Owner)
{
}

CPin::~CPin()
{
    CPin::BreakPin();
}

void CPin::BreakPin()
{
    if (auto* OldPin = ReleasePin()) {
        if (OldPin->ReleasePin() != this) {
            throw std::logic_error("CPin::BreakPin() dangling pin");
        }
    }
}

bool CPin::ConnectPin(CPin* NewPin) noexcept
{
    if (!Compatible(NewPin))
        return false;

    SetPin(NewPin);
    NewPin->SetPin(this);

    return true;
}

bool CPin::Compatible(CPin* NewPin) noexcept
{
    return NewPin != nullptr && NewPin->m_PinType == m_PinType;
}

CBaseNode* CPin::GetConnectedOwner() const
{
    if (m_ConnectedPin == nullptr) [[unlikely]] {
        throw std::runtime_error("CPin::GetConnectedOwner() called on a null pin");
    }

    return m_ConnectedPin->m_Owner;
}