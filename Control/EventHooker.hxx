//
// Created by LYS on 2/16/2026.
//

#pragma once

#include <array>
#include <functional>
#include <mutex>

struct SMouseEventData {
    uint64_t wParam;
    long x, y;
    unsigned long mouseData;
    unsigned long flags;
    unsigned long time;
    uint64_t dwExtraInfo;
};

struct SKeyboardEventData {
    uint64_t wParam;
    unsigned long vkCode;
    unsigned long scanCode;
    unsigned long flags;
    unsigned long time;
    uint64_t dwExtraInfo;
};

struct SEventHooker {

    SEventHooker();
    ~SEventHooker();

    std::array<bool, 255> PressedKeys { };

    std::unique_lock<std::mutex> HookLock;

    std::function<void(SMouseEventData)> MouseEventCallback;
    std::function<void(SKeyboardEventData)> KeyboardEventCallback;
};
