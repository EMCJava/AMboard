//
// Created by LYS on 2/15/2026.
//

#include "Pin.hxx"

#include <Util/Assertions.hxx>

#include <stdexcept>
#include <utility>

#include <spdlog/spdlog.h>

void CPin::AddPin(CPin* NewPin) noexcept
{
    if (const auto It = m_ConnectedPins.find(NewPin); It == m_ConnectedPins.end()) {
        m_ConnectedPins.insert(It, NewPin);
    }
}

CPin::CPin(CBaseNode* Owner, const bool IsInputPin) noexcept
    : m_Owner(Owner)
    , m_IsInputPin(IsInputPin)
{
}

CPin::~CPin()
{
    DisconnectPins();
}

bool CPin::ConnectPin(CPin* NewPin) noexcept
{
    if (!Compatible(NewPin))
        return false;

    PreConnectPin(NewPin);
    NewPin->PreConnectPin(this);

    AddPin(NewPin);
    NewPin->AddPin(this);

    for (const auto& Func : m_OnConnectionChanges)
        Func(this, NewPin, true);
    for (const auto& Func : NewPin->m_OnConnectionChanges)
        Func(NewPin, this, true);

    return true;
}

bool CPin::DisconnectPin(CPin* TargetPin) noexcept
{
    if (const auto It = m_ConnectedPins.find(TargetPin); It != m_ConnectedPins.end()) {
        m_ConnectedPins.erase(It);
        TargetPin->DisconnectPin(this); /// Make sure to do it second to avoid infinite looping

        for (const auto& Func : m_OnConnectionChanges)
            Func(this, TargetPin, false);
        for (const auto& Func : TargetPin->m_OnConnectionChanges)
            Func(TargetPin, this, false);

        return true;
    }

    return false;
}

void CPin::DisconnectPins() noexcept
{
    /// FIXME: This is O(n log n), not good
    while (!m_ConnectedPins.empty()) {
        CPin::DisconnectPin(*m_ConnectedPins.begin());
    }
}

CPin* CPin::GetTheOnlyPin() const
{
    MAKE_SURE(m_ConnectedPins.size() == 1)
    return *m_ConnectedPins.begin();
}

bool CPin::IsConnected(CPin* TargetPin) const noexcept
{
    return m_ConnectedPins.contains(TargetPin);
}

bool CPin::Compatible(CPin* NewPin) noexcept
{
    return NewPin != nullptr && NewPin->m_PinType == m_PinType && NewPin->m_IsInputPin != m_IsInputPin && !IsConnected(NewPin);
}