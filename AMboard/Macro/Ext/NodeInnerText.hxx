//
// Created by LYS on 3/9/2026.
//

#pragma once

#include <functional>
#include <string>

class INodeInnerText {

public:
    [[nodiscard]] const auto& GetInnerText() const noexcept { return m_InnerText; }

    void SetInnerText(std::string Str)
    {
        m_InnerText = std::move(Str);
        if (OnUpdate)
            OnUpdate();
    }

    void SetOnUpdate(auto&& Callback) { OnUpdate = Callback; }

protected:
    std::function<void()> OnUpdate;
    std::string m_InnerText;
};
