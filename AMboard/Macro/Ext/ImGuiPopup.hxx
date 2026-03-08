//
// Created by LYS on 3/7/2026.
//

#pragma once

class INodeImGuiPupUpExt {
public:
    virtual ~INodeImGuiPupUpExt() = default;

    virtual std::string GetTitle() = 0;

    virtual void OnStartPopup() { }
    virtual void OnEndPopup() { }
    virtual bool Render() = 0;
};