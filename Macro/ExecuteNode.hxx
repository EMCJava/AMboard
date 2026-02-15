//
// Created by LYS on 2/15/2026.
//

#pragma once

#include "BaseNode.hxx"

#include <cstdint>

class CExecuteNode : public CBaseNode {

public:
    CExecuteNode();

    void ExecuteNode();

protected:
    void OnPinModified() noexcept override;

    virtual void Execute();

    size_t m_DesiredOutputPin = 0;
    std::vector<CExecuteNode*> m_OutFlowingNode;
};
