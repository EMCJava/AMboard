//
// Created by LYS on 2/16/2026.
//

#include "EventHooker.hxx"

#include <mutex>

#include <windows.h>

inline std::mutex g_HookMutex;
inline SEventHooker* g_HookOwner = nullptr;

inline HHOOK g_MouseHook = nullptr;
inline HHOOK g_KeyboardHook = nullptr;

static LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (g_HookOwner != nullptr && g_HookOwner->MouseEventCallback && nCode >= 0 && wParam != WM_MOUSEMOVE) {
        const auto* pMouseStruct = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);

        g_HookOwner->MouseEventCallback(SMouseEventData {
            .wParam = wParam,
            .x = pMouseStruct->pt.x,
            .y = pMouseStruct->pt.y,
            .mouseData = pMouseStruct->mouseData,
            .flags = pMouseStruct->flags,
            .time = pMouseStruct->time,
            .dwExtraInfo = pMouseStruct->dwExtraInfo,
        });
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

static LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (g_HookOwner != nullptr && g_HookOwner->KeyboardEventCallback && nCode >= 0) {
        const auto* pKeyboardStruct = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

        bool Valid = false;

        if (wParam == WM_KEYUP) {
            g_HookOwner->PressedKeys[pKeyboardStruct->vkCode] = false;
            Valid = true;
        } else if (wParam == WM_KEYDOWN && !g_HookOwner->PressedKeys[pKeyboardStruct->vkCode]) {
            g_HookOwner->PressedKeys[pKeyboardStruct->vkCode] = true;
            Valid = true;
        }

        if (Valid) {
            g_HookOwner->KeyboardEventCallback(SKeyboardEventData {
                .wParam = wParam,
                .vkCode = pKeyboardStruct->vkCode,
                .scanCode = pKeyboardStruct->scanCode,
                .flags = pKeyboardStruct->flags,
                .time = pKeyboardStruct->time,
                .dwExtraInfo = pKeyboardStruct->dwExtraInfo,
            });
        }
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

SEventHooker::SEventHooker()
{
    HookLock = std::unique_lock { g_HookMutex };
    g_HookOwner = this;
    g_MouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseHookProc, GetModuleHandle(nullptr), 0);
    g_KeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookProc, GetModuleHandle(nullptr), 0);
}

SEventHooker::~SEventHooker()
{
    UnhookWindowsHookEx(g_MouseHook);
    UnhookWindowsHookEx(g_KeyboardHook);

    g_MouseHook = g_KeyboardHook = nullptr;
}