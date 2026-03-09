//
// Created by LYS on 3/10/2026.
//

#pragma once

#include <glm/vec2.hpp>

struct SNodeTextPerGroupMeta {
    glm::vec2 Offset;
    uint32_t Color { };
    uint8_t Padding[4];
};