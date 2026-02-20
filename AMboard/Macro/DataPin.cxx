//
// Created by LYS on 2/15/2026.
//

#include "DataPin.hxx"

CDataPin::CDataPin(CBaseNode* Owner, const bool IsInputPin) noexcept
    : CPin(Owner, IsInputPin)
{
    m_PinType = EPinType::Data;
}