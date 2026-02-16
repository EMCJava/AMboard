//
// Created by LYS on 12/20/2025.
//

#pragma once

#include <queue>
#include <memory>

#include <glm/vec2.hpp>

namespace spdlog {
class logger;
}

struct SButtonInputEvent final {

    SButtonInputEvent(int Key = 0, int Action = /* GLFW_PRESS */ 1, int Mods = -1);

    // The keyboard key that was pressed or released
    int Key;
    // GLFW_PRESS, GLFW_RELEASE or GLFW_REPEAT.
    int Action;
    // Bit field describing which modifier keys were held down
    int Mods;

    bool operator==(const SButtonInputEvent& Other) const noexcept
    {
        return Key == Other.Key && Action == Other.Action && (Mods == Other.Mods || Mods == -1 || Other.Mods == -1);
    }
};

struct SButtonStage final {

    // The keyboard key has held down
    bool IsDown = false;

    // The keyboard key detected double click at this frame
    uint64_t LastDoubleClickFrame = 0;

    // The frame the last click event start technically the same with @HoldLockHolderFrame
    uint64_t LastClickFrame = 0;

    // Object holding the "hold" event lock
    void* HoldLockHolder = nullptr;

    // The frame the "hold" event start (clicking is holding)
    uint64_t HoldLockHolderFrame = 0;
};

struct SButtonSet {
    SButtonSet(const int MaxButtonCode);

    /**
     * Accumulated button events pre frame
     */
    std::vector<SButtonInputEvent> ButtonEvents;

    /**
     * Button stage at any time
     */
    std::vector<SButtonStage> KeyStages;

    /**
     *
     * Pop button event in a LIFO order
     *
     * @return The first key event left
     */
    SButtonInputEvent PopKeyEvent();

    /**
     *
     * Find a target event in queue, pop when found
     *
     * @param Target The event to search for
     * @return true if found
     */
    bool ConsumeEvent(SButtonInputEvent Target);

    /**
     *
     * Find a press event on @Key in queue, pop when found.
     * This will remain true for the same @ContextObject until a release event is found on the @Key.
     * If an object want to keep the exclusive lock, this function should be called every frame,
     * otherwise other object can steal the event
     *
     * @param ContextObject The object holding the hold event lock
     * @param GLFWKeyCode The key to search for
     * @return true if found, and remain true until release event
     */
    bool ConsumeHoldEvent(void* ContextObject, int GLFWKeyCode);

    /**
     * @return true if it has button event left
     */
    [[nodiscard]]
    bool HasKeyEvent() const noexcept;

    /**
     * Check if a key is being held down
     * @param GLFWKeyCode A key code defined by glfw
     * @return true if the key is being held down
     */
    [[nodiscard]]
    bool IsKeyDown(int GLFWKeyCode) const noexcept;

    /**
     * Check if a key is being double-clicked this frame
     * @param GLFWKeyCode A key code defined by glfw
     * @return true if the key is being double-clicked this frame
     */
    [[nodiscard]]
    bool IsKeyDoubleClick(int GLFWKeyCode) const noexcept;

    void ClearEvents()
    {
        ButtonEvents.clear();
    }
};

class CInputManager {

public:
    CInputManager(struct GLFWwindow* WindowHandle);

    /**
     * Advance a frame (event poll)
     */
    void AdvanceFrame();

    /**
     * Clear input records before advancing a frame (event poll)
     */
    void ClearEvents();

protected:
    static constexpr int MaxKeyCode = 348;

    SButtonSet KeyboardButtons { MaxKeyCode };

    /**
     * Accumulated char events in UTF-32
     */
    std::queue<uint32_t> CharEvents;

public:
    /**
     *
     * Return the internal keyboard key stages
     *
     * @return The first char event left in UTF-32
     */
    [[nodiscard]] const auto& GetKeyboardButtons() const noexcept
    {
        return KeyboardButtons;
    }

    /**
     *
     * Return the internal keyboard key stages
     *
     * @return The first char event left in UTF-32
     */
    [[nodiscard]] auto& GetKeyboardButtons() noexcept
    {
        return KeyboardButtons;
    }

    /**
     *
     * Pop char event in a FIFO order
     *
     * @return The first char event left in UTF-32
     */
    uint32_t PopCharEvent();

    /**
     * @return true if has char event left
     */
    [[nodiscard]]
    bool HasCharEvent() const noexcept;

    /**
     * Check if a character key (A - Z in capital) is being held down
     * @return true if the key is being held down
     */
    [[nodiscard]]
    bool IsCharacterDown(char CharacterAToZ) const noexcept;

protected:
    bool MouseLocked = false;
    bool bHasMouseEvent = false;

    static constexpr int MaxMouseCode = 7;

    SButtonSet MouseButtons { MaxMouseCode };

    // Most recent cursor position
    glm::ivec2 CursorPos;

    // Cursor position since last event poll
    glm::ivec2 PrevCursorPos;

    // Cursor delta position since last event poll
    glm::ivec2 CursorDelta;

    // Cursor delta scroll since last event poll
    int CursorScrollDelta;

public:
    /**
     *
     * Return the internal mouse key stages
     *
     * @return The first char event left in UTF-32
     */
    [[nodiscard]] const auto& GetMouseButtons() const noexcept
    {
        return MouseButtons;
    }

    /**
     *
     * Return the internal mouse key stages
     *
     * @return The first char event left in UTF-32
     */
    [[nodiscard]] auto& GetMouseButtons() noexcept
    {
        return MouseButtons;
    }

    /**
     * @return true if it has any mouse event since last reset/update
     */
    [[nodiscard]]
    bool HasMouseEvent() const noexcept
    {
        return bHasMouseEvent;
    }

    /**
     * Set input mode of the mouse
     * @param Lock if true mouse is locked at the middle, else mouse will be free to move
     */
    void SetLockMouse(bool Lock = true) noexcept;

    /**
     * Get input mode of the mouse
     * @return true if mouse is locked
     */
    [[nodiscard]]
    bool IsMouseLocked() const noexcept;

    /**
     * Reset mouse history data
     */
    void ResetCursorPosition() noexcept;

    [[nodiscard]]
    glm::ivec2 GetDeltaCursor() const noexcept;

    [[nodiscard]]
    int GetDeltaScroll() const noexcept;

    [[nodiscard]]
    glm::ivec2 GetCursorPosition() const noexcept;

public:
    /**
     * Should be called for every GLFW cursor callback
     *
     * @param cursorX New cursor x position
     * @param cursorY New cursor Y position
     */
    void RegisterCursorPosition(int cursorX, int cursorY);

    /**
     * Should be called for every GLFW cursor callback
     *
     * @param delta Cursor accumulated delta scroll
     */
    void RegisterCursorScroll(int delta);

    /**
     * Should be called for every GLFW key callback
     *
     * @param key The keyboard key that was pressed or released
     * @param scancode The system-specific scancode of the key
     * @param action GLFW_PRESS, GLFW_RELEASE or GLFW_REPEAT.
     * @param mods Bit field describing which modifier keys were held down
     */
    void RegisterKeyInput(int key, int scancode, int action, int mods);

    /**
     * Should be called for every GLFW key callback
     *
     * @param key The mouse button that was pressed or released
     * @param action GLFW_PRESS, GLFW_RELEASE or GLFW_REPEAT.
     * @param mods Bit field describing which modifier keys were held down
     */
    void RegisterMouseInput(int key, int action, int mods);

    /**
     * Should be called for every GLFW char callback
     *
     * @param CodePoint The Unicode code point of the character in UTF-32.
     */
    void RegisterCharInput(uint32_t CodePoint);

protected:
    std::shared_ptr<spdlog::logger> InputLogger;

    GLFWwindow* WindowHandle = nullptr;
};
