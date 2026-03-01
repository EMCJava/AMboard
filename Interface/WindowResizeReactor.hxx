//
// Created by LYS on 3/1/2026.
//

#pragma once

#include <functional>
#include <list>

class CWindowBase;

namespace WRR {
using WindowResizeCallback = std::function<void(CWindowBase* Window)>;
std::list<WindowResizeCallback>::iterator RegisterCallback(CWindowBase*, WindowResizeCallback);
void UnregisterCallback(CWindowBase*, std::list<WindowResizeCallback>::iterator);
}

template <typename Ty>
class CWindowResizeReactor {

public:
    CWindowResizeReactor(CWindowBase* WindowPtr)
        : m_WindowPtr(WindowPtr)
    {
        m_CallbackHandle = WRR::RegisterCallback(m_WindowPtr, [Ptr = static_cast<Ty*>(this)](CWindowBase* Window) { Ptr->ResizeCallback(Window); });
    }

    ~CWindowResizeReactor()
    {
        WRR::UnregisterCallback(m_WindowPtr, m_CallbackHandle);
    }

protected:
    auto GetWindowPtr() const noexcept { return m_WindowPtr; }

private:
    CWindowBase* m_WindowPtr;
    std::list<WRR::WindowResizeCallback>::iterator m_CallbackHandle;
};
