//
// Created by LYS on 3/9/2026.
//

#include "InputDispatcher.hxx"

#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>

void CInputDispatcher::InjectEvent(const SInputEvent& ev)
{
    INPUT input = { 0 };

    if (ev.type == EInputType::KeyDown || ev.type == EInputType::KeyUp) {
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = ev.keyCode;
        if (ev.type == EInputType::KeyUp) {
            input.ki.dwFlags = KEYEVENTF_KEYUP;
        }
        SendInput(1, &input, sizeof(INPUT));
    } else {
        input.type = INPUT_MOUSE;

        // If x and y are not -1, we want to move the cursor to the recorded location
        if (ev.x != -1 && ev.y != -1) {
            int screenWidth = GetSystemMetrics(SM_CXSCREEN);
            int screenHeight = GetSystemMetrics(SM_CYSCREEN);

            // Windows requires normalized absolute coordinates (0 to 65535)
            input.mi.dx = (ev.x * 65535) / (screenWidth - 1);
            input.mi.dy = (ev.y * 65535) / (screenHeight - 1);

            // Force the cursor to teleport to this location
            input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
        }

        // Append the click flags (if any) to the move flags
        if (ev.type == EInputType::MouseDown) {
            input.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
        } else if (ev.type == EInputType::MouseUp) {
            input.mi.dwFlags |= MOUSEEVENTF_LEFTUP;
        }

        // Send the combined Move + Click event in a single OS call
        SendInput(1, &input, sizeof(INPUT));
    }
}

#elif defined(__APPLE__)
#include <ApplicationServices/ApplicationServices.h>

void CInputDispatcher::InjectEvent(const SInputEvent& ev)
{
    CGEventRef cgEvent = nullptr;

    if (ev.type == EInputType::KeyDown || ev.type == EInputType::KeyUp) {
        cgEvent = CGEventCreateKeyboardEvent(nullptr, ev.keyCode, ev.type == EInputType::KeyDown);
    } else {
        CGPoint loc = CGPointMake(ev.x, ev.y);

        // If x and y are -1, fetch the current physical mouse position
        if (ev.x == -1 && ev.y == -1) {
            CGEventRef tempEvent = CGEventCreate(nullptr);
            loc = CGEventGetLocation(tempEvent);
            CFRelease(tempEvent);
        }

        CGEventType cgType;
        if (ev.type == EInputType::MouseMove)
            cgType = kCGEventMouseMoved;
        else if (ev.type == EInputType::MouseDown)
            cgType = kCGEventLeftMouseDown;
        else if (ev.type == EInputType::MouseUp)
            cgType = kCGEventLeftMouseUp;

        // macOS natively moves the cursor to 'loc' before performing the click action
        cgEvent = CGEventCreateMouseEvent(nullptr, cgType, loc, kCGMouseButtonLeft);
    }

    if (cgEvent) {
        CGEventPost(kCGHIDEventTap, cgEvent);
        CFRelease(cgEvent);
    }
}
#endif

// =============================================================================
// 4. PLAYBACK LOOP (WITH INTERRUPTIBLE SLEEP)
// =============================================================================

CInputDispatcher& CInputDispatcher::Get()
{
    static CInputDispatcher instance;
    return instance;
}

void CInputDispatcher::Play(const std::vector<SPlaybackEvent>& sequence)
{
    Stop(); // Ensure any currently playing sequence is stopped

    m_playing = true;
    m_playbackThread = std::thread(&CInputDispatcher::PlaybackLoop, this, sequence);
}

void CInputDispatcher::Stop()
{
    if (m_playing.exchange(false)) {
        m_cv.notify_all(); // Wake up the thread if it's sleeping
    }

    if (m_playbackThread.joinable()) {
        m_playbackThread.join();
    }
}

bool CInputDispatcher::IsPlaying() const
{
    return m_playing.load();
}

void CInputDispatcher::PlaybackLoop(std::vector<SPlaybackEvent> sequence)
{
    const auto StartTimestamp = std::chrono::steady_clock::now();
    for (const auto& item : sequence) {
        if (!m_playing)
            break;

        // Interruptible sleep: Waits for delayMs, but wakes up instantly if Stop() is called
        std::unique_lock lock(m_cvMutex);
        if (m_cv.wait_until(lock, StartTimestamp + std::chrono::milliseconds(item.delayMs), [this] { return !m_playing.load(); })) {
            break; // Stop() was called during the sleep
        }

        if (!m_playing)
            break;

        InjectEvent(item.event);
    }

    m_playing = false;
    spdlog::info("[Dispatcher] Playback finished");
}