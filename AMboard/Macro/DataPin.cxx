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

bool CDataPin::Compatible(CPin* NewPin) noexcept
{
    return CPin::Compatible(NewPin) && (m_IsUniversalPin || static_cast<const CDataPin*>(NewPin)->m_IsUniversalPin || m_DataType == static_cast<const CDataPin*>(NewPin)->m_DataType);
}

CDataPin::CDataPin(CBaseNode* Owner, const bool IsInputPin) noexcept
    : CPin(Owner, IsInputPin)
{
    m_PinType = EPinType::Data;
}

std::string_view CDataPin::GetToolTips() const noexcept
{
    if (CPin::GetToolTips().empty()) {
        if (m_IsUniversalPin && m_DataType == "void")
            return "Any";
        return m_DataType;
    }

    return CPin::GetToolTips();
}

void CDataPin::Assign(const CDataPin* Source)
{
    MAKE_SURE(m_IsUniversalPin || Source->m_IsUniversalPin || m_DataType == Source->m_DataType);

    m_DataType = Source->m_DataType;
    m_SharedData = Source->m_SharedData;
}