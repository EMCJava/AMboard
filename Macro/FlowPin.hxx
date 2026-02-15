//
// Created by LYS on 2/15/2026.
//

#pragma once

#include "Pin.hxx"

class CFlowPin : public CPin {

    CPin* SetPin(CPin* NewPin) noexcept override;

public:
    ~CFlowPin();
    void BreakPin() override;

    CFlowPin(CBaseNode* Owner);
};
