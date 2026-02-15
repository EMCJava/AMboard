//
// Created by LYS on 2/15/2026.
//

#include "ExecuteNode.hxx"

#include <cassert>

CExecuteNode::CExecuteNode()
{
    m_NodeType = ENodeType::Execution;
}

void CExecuteNode::ExecuteNode()
{
    m_DesiredOutputPin = 0;
    Execute();

    if (m_DesiredOutputPin < m_OutFlowingNode.size())
        m_OutFlowingNode[m_DesiredOutputPin]->ExecuteNode();
}

void CExecuteNode::Execute()
{
}

void CExecuteNode::OnPinModified() noexcept
{
    CBaseNode::OnPinModified();

    m_OutFlowingNode.clear();
    for (const auto& Pin : m_OutputPins) {
        if (*Pin && *Pin == EPinType::Flow) {
            if (auto* PinOwner = Pin->GetConnectedOwner()) {
                if (*PinOwner == ENodeType::Execution) {
                    assert(dynamic_cast<CExecuteNode*>(PinOwner) != nullptr);
                    m_OutFlowingNode.push_back(static_cast<CExecuteNode*>(PinOwner));
                }
            }
        }
    }
}