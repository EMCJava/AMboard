//
// Created by LYS on 2/18/2026.
//

#pragma once

#include <string>
#include <utility>

struct STextGroupHandle {

    std::string Text;
    float Scale;

    float TextWidth = 0;

    std::pair<size_t, size_t> VertexSpan { };
    uint32_t GroupId = -1;
    uint32_t NodeId = -1;
};