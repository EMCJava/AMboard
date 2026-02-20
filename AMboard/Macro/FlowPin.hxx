//
// Created by LYS on 2/15/2026.
//

#pragma once

#include "Pin.hxx"


class CFlowPin : public CPin {
protected:
    void PreConnectPin(CPin* NewPin) noexcept override;

public:
    CFlowPin(CBaseNode* Owner, bool IsInputPin) noexcept;
};
