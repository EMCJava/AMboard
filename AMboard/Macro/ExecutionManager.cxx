//
// Created by LYS on 3/9/2026.
//

#include "ExecutionManager.hxx"

#include "ExecuteNode.hxx"

void CExecutionManager::ExecutionThread(CExecuteNode* Target)
{
    Execute(Target);
}

CExecutionManager::~CExecutionManager()
{
    m_TerminationFlag.test_and_set();
    if (m_ExecutionThread)
        if (m_ExecutionThread->joinable())
            m_ExecutionThread->join();
}

bool CExecutionManager::StartExecuteThread(CExecuteNode* Target)
{
    if (m_ExecutionThread) {
        if (!m_ExecutionThread->joinable())
            return false;

        m_ExecutionThread->join();
        m_ExecutionThread.reset();
    }

    m_ExecutionThread = std::make_unique<std::thread>(&CExecutionManager::ExecutionThread, this, Target);
}

void CExecutionManager::Execute(CExecuteNode* Target)
{
    while (Target != nullptr && !m_TerminationFlag.test()) {
        SetActiveNode(Target);
        Target = Target->ExecuteNode();
    }
    SetActiveNode(nullptr);
}

void CExecutionManager::SetActiveNode(CExecuteNode* Node) noexcept
{
    m_ActiveNode = Node;
}

void CExecutionManager::UnRegisterNode(CExecuteNode* Node) noexcept
{
    if (m_ActiveNode == Node)
        m_ActiveNode = nullptr;
}