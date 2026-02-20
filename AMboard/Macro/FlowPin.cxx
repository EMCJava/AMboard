//
// Created by LYS on 2/15/2026.
//

#include "FlowPin.hxx"

#include "BaseNode.hxx"
#include "Pin.hxx"

void CFlowPin::PreConnectPin(CPin* NewPin) noexcept
{
    CPin::PreConnectPin(NewPin);

    /// Output pin can only connect to one input pin
    if (!m_IsInputPin && !m_ConnectedPins.empty()) {
        DisconnectPin(*m_ConnectedPins.begin());
    }
}

CFlowPin::CFlowPin(CBaseNode* Owner, const bool IsInputPin) noexcept
    : CPin(Owner, IsInputPin)
{
    m_PinType = EPinType::Flow;
}