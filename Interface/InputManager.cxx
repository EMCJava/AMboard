//
// Created by LYS on 12/20/2025.
//

#include "InputManager.hxx"
#include "Interface.hxx"

#include <GLFW/glfw3.h>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <cassert>
#include <ranges>

constexpr int DoubleClickingFrameInterval = 40;

SButtonInputEvent::SButtonInputEvent(int Key, int Action, int Mods)
{
    this->Key = Key;
    this->Action = Action;
    this->Mods = Mods;
}

SButtonSet::SButtonSet(const int MaxButtonCode)
{
    KeyStages.resize(MaxButtonCode + 1);
}

SButtonInputEvent
SButtonSet::PopKeyEvent()
{
    if (ButtonEvents.empty()) [[unlikely]]
        return { };

    const SButtonInputEvent Result = ButtonEvents.back();
    ButtonEvents.pop_back();
    return Result;
}

bool SButtonSet::ConsumeEvent(SButtonInputEvent Target)
{
    const auto RemoveIt = std::ranges::find(ButtonEvents, Target);
    if (RemoveIt == ButtonEvents.end())
        return false;
    ButtonEvents.erase(RemoveIt);
    return true;
}

bool SButtonSet::ConsumeHoldEvent(void* ContextObject, int GLFWKeyCode)
{
    auto& Key = KeyStages[GLFWKeyCode];

    if (Key.IsDown) {
        /// No need to check event queue if already done this frame
        if (Key.HoldLockHolderFrame < GlobalFrameCounter) {
            if (/* No one is checking */ Key.HoldLockHolder == nullptr || /* Owned */ Key.HoldLockHolder == ContextObject || /* Steal */ Key.HoldLockHolderFrame < GlobalFrameCounter - 1) {
                Key.HoldLockHolder = ContextObject;
                Key.HoldLockHolderFrame = GlobalFrameCounter;

                /// Remove click event
                for (int i = 0; i < ButtonEvents.size(); ++i) {
                    if (ButtonEvents.at(i).Key == GLFWKeyCode && ButtonEvents.at(i).Action == GLFW_PRESS) {
                        ButtonEvents.erase(ButtonEvents.begin() + i);
                        break;
                    }
                }

                return true;
            }
        } else {
            return /* Owned */ Key.HoldLockHolder == ContextObject;
        }
    }

    /// Event is locked by other context object, or not holding
    return false;
}

bool SButtonSet::HasKeyEvent() const noexcept
{
    return !ButtonEvents.empty();
}

bool SButtonSet::IsKeyDown(int GLFWKeyCode) const noexcept
{
    return KeyStages[GLFWKeyCode].IsDown;
}

bool SButtonSet::IsKeyDoubleClick(int GLFWKeyCode) const noexcept
{
    return KeyStages[GLFWKeyCode].LastDoubleClickFrame == GlobalFrameCounter;
}

CInputManager::CInputManager(GLFWwindow* WindowHandle)
    : WindowHandle(WindowHandle)
{

    try {
        InputLogger = spdlog::stderr_color_mt("InputManager");
    } catch (...) {
        InputLogger = spdlog::get("InputManager");
        InputLogger->warn("Using existing logger for InputManager");
    }
}

void CInputManager::AdvanceFrame()
{
    CursorDelta = CursorPos - PrevCursorPos;
    PrevCursorPos = CursorPos;
}

void CInputManager::ClearEvents()
{
    MouseButtons.ClearEvents();
    KeyboardButtons.ClearEvents();
    CharEvents = { };
    bHasMouseEvent = false;
    CursorScrollDelta = 0;
}

void CInputManager::RegisterKeyInput(int key, [[maybe_unused]] int, int action, int mods)
{
    if (key < 0 || key >= GLFW_KEY_LAST) [[unlikely]] {
        InputLogger->warn("Invalid key code %d ignored!", key);
        return;
    }

    static_assert(GLFW_KEY_LAST == MaxKeyCode);
    KeyboardButtons.ButtonEvents.emplace_back(key, action, mods);

    KeyboardButtons.KeyStages[key].IsDown = action != GLFW_RELEASE;
    if (action == GLFW_PRESS) {
        if (KeyboardButtons.KeyStages[key].LastClickFrame + DoubleClickingFrameInterval >= GlobalFrameCounter) {
            KeyboardButtons.KeyStages[key].LastDoubleClickFrame = GlobalFrameCounter;
        }

        KeyboardButtons.KeyStages[key].LastClickFrame = GlobalFrameCounter;
    }
}

void CInputManager::RegisterMouseInput(int key, int action, int mods)
{
    if (key < 0 || key >= GLFW_MOUSE_BUTTON_LAST) [[unlikely]] {
        InputLogger->warn("Invalid key code %d ignored!", key);
        return;
    }

    static_assert(GLFW_MOUSE_BUTTON_LAST == MaxMouseCode);
    MouseButtons.ButtonEvents.emplace_back(key, action, mods);

    MouseButtons.KeyStages[key].IsDown = action != GLFW_RELEASE;
    if (action == GLFW_PRESS) {
        if (MouseButtons.KeyStages[key].LastClickFrame + DoubleClickingFrameInterval >= GlobalFrameCounter) {
            MouseButtons.KeyStages[key].LastDoubleClickFrame = GlobalFrameCounter;
        }

        MouseButtons.KeyStages[key].LastClickFrame = GlobalFrameCounter;
    }

    bHasMouseEvent = true;
}

void CInputManager::RegisterCharInput(uint32_t CodePoint)
{
    CharEvents.push(CodePoint);
}

uint32_t
CInputManager::PopCharEvent()
{
    if (CharEvents.empty()) [[unlikely]]
        return 0;

    const auto Result = CharEvents.front();
    CharEvents.pop();
    return Result;
}

bool CInputManager::HasCharEvent() const noexcept
{
    return !CharEvents.empty();
}

bool CInputManager::IsCharacterDown(char CharacterAToZ) const noexcept
{
    assert(CharacterAToZ >= 'A' && CharacterAToZ <= 'Z');
    return KeyboardButtons.KeyStages[CharacterAToZ + (GLFW_KEY_A - 'A')].IsDown;
}

void CInputManager::SetLockMouse(bool Lock) noexcept
{
    if ((MouseLocked = Lock)) {
        glfwSetInputMode(WindowHandle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        ResetCursorPosition();
    } else {
        glfwSetInputMode(WindowHandle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

bool CInputManager::IsMouseLocked() const noexcept
{
    return MouseLocked;
}

void CInputManager::ResetCursorPosition() noexcept
{
    double XPos, YPos;
    glfwGetCursorPos(WindowHandle, &XPos, &YPos);
    PrevCursorPos = CursorPos = { XPos, YPos };
    CursorDelta = { 0, 0 };

    bHasMouseEvent = true;
}

void CInputManager::RegisterCursorPosition(int cursorX, int cursorY)
{
    CursorPos = { cursorX, cursorY };

    bHasMouseEvent = true;
}

void CInputManager::RegisterCursorScroll(int delta)
{
    CursorScrollDelta += delta;
}

glm::ivec2
CInputManager::GetDeltaCursor() const noexcept
{
    return CursorDelta;
}

int CInputManager::GetDeltaScroll() const noexcept
{
    return CursorScrollDelta;
}

glm::ivec2
CInputManager::GetCursorPosition() const noexcept
{
    return CursorPos;
}
