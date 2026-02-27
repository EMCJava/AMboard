//
// Created by LYS on 2/15/2026.
//

#include "ExecuteNode.hxx"

#include <cassert>

CExecuteNode::CExecuteNode()
{
    m_NodeType = ENodeType::Execution;

    AddOnPinChanges([this](CPin*, bool) {
        if (m_IsDestructing)
            return;

        m_InFlowingPin.clear();
        std::ranges::copy(GetInputPins() | FlowPinFilter | FlowPinTransform, std::back_inserter(m_InFlowingPin));
        m_OutFlowingPin.clear();
        std::ranges::copy(GetOutputPins() | FlowPinFilter | FlowPinTransform, std::back_inserter(m_OutFlowingPin));
    });
}

void CExecuteNode::ExecuteNode()
{
    m_DesiredOutputPin = 0;
    Execute();

    if (m_DesiredOutputPin < m_OutFlowingPin.size() && *m_OutFlowingPin[m_DesiredOutputPin])
        static_cast<CExecuteNode*>(m_OutFlowingPin[m_DesiredOutputPin]->GetTheOnlyPin()->GetOwner())->ExecuteNode();
}

void CExecuteNode::AddInputOutputFlowPin()
{
    EmplacePin<CFlowPin>(true);
    EmplacePin<CFlowPin>(false);
}