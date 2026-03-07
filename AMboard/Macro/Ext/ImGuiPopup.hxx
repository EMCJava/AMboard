//
// Created by LYS on 3/7/2026.
//

#pragma once

class INodeImGuiPupUpExt {
public:
    virtual ~INodeImGuiPupUpExt() = default;

    virtual std::string GetTitle() = 0;
    virtual bool Render() = 0;
};