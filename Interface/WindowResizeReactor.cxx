//
// Created by LYS on 3/1/2026.
//

#include "WindowResizeReactor.hxx"

#include "WindowBase.hxx"

namespace WRR {
std::list<WindowResizeCallback>::iterator RegisterCallback(CWindowBase* Window, WindowResizeCallback Callback)
{
    return Window->AddOnWindowResizes(std::move(Callback));
}

void UnregisterCallback(CWindowBase* Window, std::list<WindowResizeCallback>::iterator Iter)
{
    Window->EraseOnWindowResizes(Iter);
}
}