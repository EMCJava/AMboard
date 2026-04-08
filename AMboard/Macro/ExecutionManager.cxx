//
// Created by LYS on 3/9/2026.
//

#include "ExecutionManager.hxx"

#include "ExecuteNode.hxx"

CExecutionManager::~CExecutionManager()
{
    m_TerminationFlag.test_and_set();
    if (m_AsyncThread)
        if (m_AsyncThread->joinable())
            m_AsyncThread->join();
}

bool CExecutionManager::StartExecuteAsync(CExecuteNode* Target, std::function<void()> OnComplete)
{
    // Bounded reentry: reject if an async execution is already in flight
    if (m_AsyncRunning.test_and_set(std::memory_order_acq_rel)) {
        return false;
    }

    // Join any previously completed async thread before launching a new one
    if (m_AsyncThread && m_AsyncThread->joinable()) {
        m_AsyncThread->join();
    }

    m_AsyncThread = std::make_unique<std::thread>([this, Target, Cb = std::move(OnComplete)]() {
        Execute(Target);
        m_AsyncRunning.clear(std::memory_order_release);
        if (Cb) {
            Cb();
        }
    });

    return true;
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