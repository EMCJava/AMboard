//
// Created by LYS on 3/9/2026.
//

#pragma once

#include "MacroDefines.hxx"

#include <functional>
#include <thread>

class CExecuteNode;
class MACRO_API CExecutionManager {

public:
    ~CExecutionManager();

    // Starts execution on a dedicated async thread, returning immediately.
    // Returns false if an async execution is already in flight (bounded reentry).
    // The optional callback is invoked on the async thread after execution completes.
    bool StartExecuteAsync(CExecuteNode* Target, std::function<void()> OnComplete = nullptr);

    // Returns true if an async trigger execution is currently in flight.
    [[nodiscard]] bool IsAsyncRunning() const noexcept { return m_AsyncRunning.test(std::memory_order_acquire); }

    void Execute(CExecuteNode* Target);

    [[nodiscard]] CExecuteNode* GetActiveNode() const noexcept { return const_cast<CExecuteNode*>(m_ActiveNode); }
    void SetActiveNode(CExecuteNode* Node) noexcept;

    void UnRegisterNode(CExecuteNode* Node) noexcept;

    operator bool() const noexcept { return !m_TerminationFlag.test(); }

protected:
    std::atomic_flag m_TerminationFlag;
    std::atomic_flag m_AsyncRunning {};
    std::unique_ptr<std::thread> m_AsyncThread;

    volatile CExecuteNode* m_ActiveNode = nullptr;
};
