//
// Created by LYS on 2/15/2026.
//

#include "DataPin.hxx"

void CDataPin::PreConnectPin(CPin* NewPin) noexcept
{
    CPin::PreConnectPin(NewPin);

    /// Input pin can only connect to one output pin
    if (m_IsInputPin && !m_ConnectedPins.empty()) {
        DisconnectPin(*m_ConnectedPins.begin());
    }
}

CDataPin::CDataPin(CBaseNode* Owner, const bool IsInputPin) noexcept
    : CPin(Owner, IsInputPin)
{
    m_PinType = EPinType::Data;
}