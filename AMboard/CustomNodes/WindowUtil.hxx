//
// Created by LYS on 3/15/2026.
//

#pragma once

#include <windows.h>

struct SDpiSetup {
    DPI_AWARENESS_CONTEXT OldSpiContext;
    SDpiSetup() { OldSpiContext = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2); }
    ~SDpiSetup() { SetThreadDpiAwarenessContext(OldSpiContext); }
};