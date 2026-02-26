//
// Created by LYS on 2/26/2026.
//

#pragma once

// ─── DLL export/import macros ───────────────────────────────────────────────
#ifdef _WIN32
    #ifdef MACRO_API_EXPORTS
        #define MACRO_API __declspec(dllexport)
    #else
        #define MACRO_API __declspec(dllimport)
    #endif
    #define NODE_EXT_EXPORT extern "C" __declspec(dllexport)
#else
    #define MACRO_API __attribute__((visibility("default")))
    #define PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#endif