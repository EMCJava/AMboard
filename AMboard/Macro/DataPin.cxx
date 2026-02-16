//
// Created by LYS on 2/15/2026.
//

#include "DataPin.hxx"

CDataPin::CDataPin(CBaseNode* Owner)
    : CPin(Owner)
{
    m_PinType = EPinType::Data;
}