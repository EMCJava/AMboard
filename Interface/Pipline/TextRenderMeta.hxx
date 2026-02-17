//
// Created by LYS on 2/17/2026.
//

#pragma once

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

struct STextRenderVertexMeta {
    glm::vec4 TextBound;
    glm::vec4 UVBound;
    uint32_t TextGroupIndex;
};

struct STextRenderPerGroupMeta {
    glm::vec2 Offset { };
    uint32_t Color { };
    uint8_t Padding[4];
};
