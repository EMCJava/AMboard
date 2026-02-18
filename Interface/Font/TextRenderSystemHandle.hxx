//
// Created by LYS on 2/18/2026.
//

#pragma once

#include <string>
#include <utility>

struct STextGroupHandle {

    std::string Text;
    float Scale;

    std::pair<size_t, size_t> VertexSpan { };
    uint32_t GroupId = -1;
};