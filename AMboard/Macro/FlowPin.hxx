//
// Created by LYS on 2/15/2026.
//

#pragma once

#include "Pin.hxx"

class MACRO_API CFlowPin : public CPin {
protected:
    void PreConnectPin(CPin* NewPin) noexcept override;

public:
    CFlowPin(CBaseNode* Owner, bool IsInputPin) noexcept;
};
