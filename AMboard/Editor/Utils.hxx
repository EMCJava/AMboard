//
// Created by LYS on 3/10/2026.
//

#pragma once

#include <utility>

template <typename Ty1, typename Ty2>
struct SPairHash {
    std::size_t operator()(const std::pair<Ty1, Ty2>& p) const noexcept
    {
        const auto h1 = std::hash<Ty1> { }(p.first);
        const auto h2 = std::hash<Ty2> { }(p.second);

        // The "Magic Number" 0x9e3779b9 is the golden ratio,
        // which spreads bits randomly to avoid collisions.
        return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};
