//
// Created by LYS on 2/15/2026.
//

#include "ExecuteNode.hxx"

#include "ExecutionManager.hxx"

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

    AddInputOutputFlowPin();
}

CExecuteNode::~CExecuteNode()
{
    if (m_Manager)
        m_Manager->UnRegisterNode(this);
}

CExecuteNode* CExecuteNode::ExecuteNode()
{
    m_DesiredOutputPin = 0;
    Execute();

    if (m_DesiredOutputPin < m_OutFlowingPin.size() && *m_OutFlowingPin[m_DesiredOutputPin])
        return static_cast<CExecuteNode*>(m_OutFlowingPin[m_DesiredOutputPin]->GetTheOnlyConnected()->GetOwner());

    return nullptr;
}

void CExecuteNode::SetManager(CExecutionManager* Manager)
{
    m_Manager = Manager;
}

void CExecuteNode::AddInputOutputFlowPin()
{
    EmplacePin<CFlowPin>(true);
    EmplacePin<CFlowPin>(false);
}