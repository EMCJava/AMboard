//
// Created by LYS on 2/15/2026.
//

#include "FlowPin.hxx"

#include "BaseNode.hxx"
#include "Pin.hxx"

CPin* CFlowPin::SetPin(CPin* NewPin) noexcept
{
    auto* Result = CPin::SetPin(NewPin);
    GetOwner()->OnPinModified();
    return Result;
}

CFlowPin::CFlowPin(CBaseNode* Owner)
    : CPin(Owner)
{
    m_PinType = EPinType::Flow;
}