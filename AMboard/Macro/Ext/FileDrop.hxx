//
// Created by LYS on 3/19/2026.
//

#pragma once

#include <string>
#include <vector>

class IFileDrop {

public:
    virtual void OnFileDrop(const std::vector<std::string>& Files) = 0;
};
