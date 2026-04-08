//
// Created by LYS on 2/16/2026.
//

#pragma once

#include <functional>
#include <future>
#include <mutex>

// =============================================================================
// 1. CROSS-PLATFORM DATA STRUCTURES
// =============================================================================

enum class EInputType {
    KeyDown,
    KeyUp,
    MouseMove,
    MouseDown,
    MouseUp
};

struct SInputEvent {
    EInputType type;
    int keyCode;
    int x, y;

    // Returns a human-readable string representation of the event
    std::string ToString() const;
};

// =============================================================================
// 2. THE OPTIMIZED INPUT SERVICE
// =============================================================================

class CInputService {
public:
    using InputCallback = std::function<void(const SInputEvent&)>;
    using SubscriptionID = uint64_t;

    static CInputService& Get();

    // Automatically starts OS listening if this is the first subscriber
    SubscriptionID Subscribe(InputCallback callback);

    // Automatically stops OS listening if this was the last subscriber
    void Unsubscribe(SubscriptionID id);

    void DispatchEvent(const SInputEvent& event);

private:
    CInputService() = default;
    ~CInputService() { StopInternal(); }

    std::mutex m_mutex;
    std::unordered_map<SubscriptionID, InputCallback> m_subscribers;
    SubscriptionID m_nextId = 0;

    std::atomic<bool> m_running { false };
    std::thread m_hookThread;

    void StartInternal();
    void StopInternal();
    void JoinThreadInternal();
    void RunPlatformLoop(std::promise<void>* initialized);
    void StopPlatformLoop();

#ifdef _WIN32
    std::atomic<uint32_t> m_threadId { 0 };
#elif defined(__APPLE__)
    std::atomic<void*> m_runLoop { nullptr };
#endif
};
