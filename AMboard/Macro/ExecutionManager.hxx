//
// Created by LYS on 3/9/2026.
//

#pragma once

#include "MacroDefines.hxx"

#include <thread>

class CExecuteNode;
class MACRO_API CExecutionManager {

protected:
    void ExecutionThread(CExecuteNode* Target);

public:
    ~CExecutionManager();

    bool StartExecuteThread(CExecuteNode* Target);

    void Execute(CExecuteNode* Target);

    [[nodiscard]] CExecuteNode* GetActiveNode() const noexcept { return const_cast<CExecuteNode*>(m_ActiveNode); }
    void SetActiveNode(CExecuteNode* Node) noexcept;

    void UnRegisterNode(CExecuteNode* Node) noexcept;

protected:
    std::atomic_flag m_TerminationFlag;
    std::unique_ptr<std::thread> m_ExecutionThread;

    volatile CExecuteNode* m_ActiveNode = nullptr;
};
