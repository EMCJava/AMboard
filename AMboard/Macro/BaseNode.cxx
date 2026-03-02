//
// Created by LYS on 2/15/2026.
//

#include "BaseNode.hxx"

#include "DataPin.hxx"

void CBaseNode::PrepareInputPin() noexcept
{
    for (const auto& IPin : m_InputPins) {
        if (*IPin == EPinType::Data && *IPin) {
            const auto* ConnectedPin = IPin->GetTheOnlyConnected();
            /// If data node, refresh
            if (auto* ConnectedOwner = ConnectedPin->GetOwner(); *ConnectedOwner == ENodeType::Data)
                ConnectedOwner->Evaluate();

            IPin->As<CDataPin>()->Assign(ConnectedPin->As<CDataPin>());
        }
    }
}

CBaseNode::~CBaseNode()
{
    m_IsDestructing = true;

    while (!m_InputPins.empty())
        ErasePin(m_InputPins.front().get());
    while (!m_OutputPins.empty())
        ErasePin(m_OutputPins.front().get());
}

bool CBaseNode::Evaluate() noexcept
{
    PrepareInputPin();
    return true;
}