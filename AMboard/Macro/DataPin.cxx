//
// Created by LYS on 2/15/2026.
//

#include "DataPin.hxx"

#include "Util/Assertions.hxx"

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

void CDataPin::Assign(const CDataPin* Source)
{
    MAKE_SURE(m_DataType == "void" || m_DataType == Source->m_DataType);

    m_DataType = Source->m_DataType;
    std::memcpy(m_Data, Source->m_Data, sizeof(m_Data));
}