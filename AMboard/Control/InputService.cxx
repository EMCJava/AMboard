//
// Created by LYS on 2/16/2026.
//

#include "InputService.hxx"

#include <spdlog/spdlog.h>

#include <ranges>

std::string SInputEvent::ToString() const
{
    std::string typeStr;
    switch (type) {
    case EInputType::KeyDown:
        typeStr = "KeyDown";
        break;
    case EInputType::KeyUp:
        typeStr = "KeyUp";
        break;
    case EInputType::MouseMove:
        typeStr = "MouseMove";
        break;
    case EInputType::MouseDown:
        typeStr = "MouseDown";
        break;
    case EInputType::MouseUp:
        typeStr = "MouseUp";
        break;
    default:
        typeStr = "Unknown";
        break;
    }

    // Format output based on whether it's a keyboard or mouse event
    if (type == EInputType::KeyDown || type == EInputType::KeyUp) {
        return typeStr + " [KeyCode: " + std::to_string(keyCode) + "]";
    } else {
        return typeStr + " [X: " + std::to_string(x) + ", Y: " + std::to_string(y) + "]";
    }
}

CInputService& CInputService::Get()
{
    static CInputService instance;
    return instance;
}

CInputService::SubscriptionID CInputService::Subscribe(InputCallback callback)
{
    std::lock_guard lock(m_mutex);
    SubscriptionID id = ++m_nextId;
    m_subscribers[id] = std::move(callback);

    if (m_subscribers.size() == 1) {
        spdlog::info("[Service] First subscriber added. Starting OS hooks...");
        StartInternal();
    }
    return id;
}

void CInputService::Unsubscribe(SubscriptionID id)
{
    bool shouldStop = false;
    {
        std::lock_guard lock(m_mutex);
        if (m_subscribers.erase(id) > 0) {
            if (m_subscribers.empty()) {
                shouldStop = true;
            }
        }
    }

    if (shouldStop) {
        spdlog::info("[Service] Last subscriber removed. Stopping OS hooks...");
        StopInternal();
    }
}

void CInputService::DispatchEvent(const SInputEvent& event)
{
    std::vector<InputCallback> callbacks;
    {
        // Copy callbacks to avoid deadlocks if a consumer unsubscribes inside their callback
        std::lock_guard lock(m_mutex);
        for (const auto& cb : m_subscribers | std::views::values) {
            callbacks.push_back(cb);
        }
    }
    for (const auto& cb : callbacks) {
        cb(event);
    }
}

// =============================================================================
// 3. PLATFORM SPECIFIC IMPLEMENTATIONS
// =============================================================================

#ifdef _WIN32
#include <windows.h>

LRESULT CALLBACK WinMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0) {
        MSLLHOOKSTRUCT* ms = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        SInputEvent ev { EInputType::MouseMove, 0, ms->pt.x, ms->pt.y };
        if (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN)
            ev.type = EInputType::MouseDown;
        else if (wParam == WM_LBUTTONUP || wParam == WM_RBUTTONUP)
            ev.type = EInputType::MouseUp;
        CInputService::Get().DispatchEvent(ev);
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK WinKbdProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* ks = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        SInputEvent ev { EInputType::KeyDown, static_cast<int>(ks->vkCode), 0, 0 };
        if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
            ev.type = EInputType::KeyUp;
        CInputService::Get().DispatchEvent(ev);
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

void CInputService::RunPlatformLoop(std::promise<void>* initialized)
{
    m_threadId = GetCurrentThreadId();

    // Hooks are now local to the thread, preventing global state leaks
    HHOOK mouseHook = SetWindowsHookEx(WH_MOUSE_LL, WinMouseProc, nullptr, 0);
    HHOOK kbdHook = SetWindowsHookEx(WH_KEYBOARD_LL, WinKbdProc, nullptr, 0);

    // Force message queue creation before signaling readiness
    MSG msg;
    PeekMessage(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

    initialized->set_value(); // Signal StartInternal() that we are ready

    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(mouseHook);
    UnhookWindowsHookEx(kbdHook);
    m_threadId = 0;
}

void CInputService::StopPlatformLoop()
{
    if (m_threadId != 0) {
        PostThreadMessage(m_threadId, WM_QUIT, 0, 0);
    }
}

#elif defined(__APPLE__)
#include <ApplicationServices/ApplicationServices.h>

CGEventRef MacEventCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void* refcon)
{
    SInputEvent ev { };
    CGPoint loc = CGEventGetLocation(event);
    ev.x = static_cast<int>(loc.x);
    ev.y = static_cast<int>(loc.y);

    switch (type) {
    case kCGEventKeyDown:
        ev.type = EInputType::KeyDown;
        ev.keyCode = CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
        break;
    case kCGEventKeyUp:
        ev.type = EInputType::KeyUp;
        ev.keyCode = CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
        break;
    case kCGEventMouseMoved:
        ev.type = EInputType::MouseMove;
        break;
    case kCGEventLeftMouseDown:
    case kCGEventRightMouseDown:
        ev.type = EInputType::MouseDown;
        break;
    case kCGEventLeftMouseUp:
    case kCGEventRightMouseUp:
        ev.type = EInputType::MouseUp;
        break;
    default:
        return event;
    }
    CInputService::Get().DispatchEvent(ev);
    return event;
}

void CInputService::RunPlatformLoop(std::promise<void>* initialized)
{
    CGEventMask eventMask = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp) | CGEventMaskBit(kCGEventMouseMoved) | CGEventMaskBit(kCGEventLeftMouseDown) | CGEventMaskBit(kCGEventLeftMouseUp) | CGEventMaskBit(kCGEventRightMouseDown) | CGEventMaskBit(kCGEventRightMouseUp);

    CFMachPortRef eventTap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault, eventMask, MacEventCallback, nullptr);
    if (!eventTap) {
        spdlog::error("Failed to create Mac Event Tap! (Check Accessibility permissions)");
        initialized->set_value();
        return;
    }

    CFRunLoopSourceRef runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopCommonModes);
    CGEventTapEnable(eventTap, true);

    m_runLoop = CFRunLoopGetCurrent();
    initialized->set_value(); // Signal StartInternal() that we are ready

    CFRunLoopRun(); // Blocks until CFRunLoopStop is called

    CFRelease(runLoopSource);
    CFRelease(eventTap);
    m_runLoop = nullptr;
}

void CInputService::StopPlatformLoop()
{
    void* loop = m_runLoop.load();
    if (loop) {
        CFRunLoopStop(static_cast<CFRunLoopRef>(loop));
    }
}
#endif

// =============================================================================
// 4. INTERNAL LIFECYCLE MANAGEMENT
// =============================================================================

void CInputService::StartInternal()
{
    if (m_running.exchange(true))
        return;

    std::promise<void> initialized;
    m_hookThread = std::thread([this, &initialized]() {
        this->RunPlatformLoop(&initialized);
    });

    // Block until the OS loop is fully initialized to prevent race conditions
    initialized.get_future().wait();
}

void CInputService::StopInternal()
{
    if (!m_running.exchange(false))
        return;

    StopPlatformLoop();

    if (m_hookThread.joinable()) {
        // Safe-guard: If a consumer unsubscribes from inside their own callback,
        // we cannot join() the thread from itself. We detach it and let it die naturally.
        if (std::this_thread::get_id() == m_hookThread.get_id()) {
            m_hookThread.detach();
        } else {
            m_hookThread.join();
        }
    }
}