//
// Created by LYS on 2/26/2026.
//

#pragma once

// ─── DLL export/import macros ───────────────────────────────────────────────

#ifdef _WIN32
#ifdef MACRO_API_EXPORTS
#define MACRO_API __declspec(dllexport)
#elifdef MACRO_API_IMPORTS
#define MACRO_API __declspec(dllimport)
#else
#define MACRO_API
#endif
#define NODE_EXT_EXPORT extern "C" __declspec(dllexport)
#else
#define MACRO_API __attribute__((visibility("default")))
#define NODE_EXT_EXPORT extern "C" __attribute__((visibility("default")))
#endif

#define STRINGIFY(x) #x

#define PARENS ()

#define EXPAND(...) EXPAND4(EXPAND4(EXPAND4(EXPAND4(__VA_ARGS__))))
#define EXPAND4(...) EXPAND3(EXPAND3(EXPAND3(EXPAND3(__VA_ARGS__))))
#define EXPAND3(...) EXPAND2(EXPAND2(EXPAND2(EXPAND2(__VA_ARGS__))))
#define EXPAND2(...) EXPAND1(EXPAND1(EXPAND1(EXPAND1(__VA_ARGS__))))
#define EXPAND1(...) __VA_ARGS__

#define FOR_EACH(macro, ...) \
    __VA_OPT__(EXPAND(FOR_EACH_HELPER(macro, __VA_ARGS__)))

#define FOR_EACH_HELPER(macro, a, ...) \
    macro(a) __VA_OPT__(FOR_EACH_AGAIN PARENS(macro, __VA_ARGS__))

#define FOR_EACH_AGAIN() FOR_EACH_HELPER

// Separate FOR_EACH for comma-separated lists
#define FOR_EACH_COMMA(macro, ...) \
    __VA_OPT__(EXPAND(FOR_EACH_COMMA_HELPER(macro, __VA_ARGS__)))

#define FOR_EACH_COMMA_HELPER(macro, a, ...) \
    macro(a) __VA_OPT__(, FOR_EACH_COMMA_AGAIN PARENS(macro, __VA_ARGS__))

#define FOR_EACH_COMMA_AGAIN() FOR_EACH_COMMA_HELPER

// ─────────────────────────────────────────────────────────────────────────────
// Per-plugin: generate create/destroy functions
// ─────────────────────────────────────────────────────────────────────────────
#define MACRO_FACTORY(Name)                           \
    NODE_EXT_EXPORT CBaseNode* create_##Name()        \
    {                                                 \
        return new Name();                            \
    }                                                 \
    NODE_EXT_EXPORT void destroy_##Name(CBaseNode* p) \
    {                                                 \
        delete p;                                     \
    }

#define MACRO_NAME_ENTRY(Name) STRINGIFY(Name)

// ─────────────────────────────────────────────────────────────────────────────
// REGISTER_MACROS(Foo, Bar, Baz)
//
// Generates:
//   - create_Foo / destroy_Foo
//   - create_Bar / destroy_Bar
//   - create_Baz / destroy_Baz
//   - get_macro_names() -> { "Foo", "Bar", "Baz", nullptr }
// ─────────────────────────────────────────────────────────────────────────────
#define REGISTER_MACROS(...)                               \
    FOR_EACH(MACRO_FACTORY, __VA_ARGS__)                   \
                                                           \
    NODE_EXT_EXPORT const char** get_macro_names()         \
    {                                                      \
        static const char* names[] = {                     \
            FOR_EACH_COMMA(MACRO_NAME_ENTRY, __VA_ARGS__) \
                __VA_OPT__(, ) nullptr                     \
        };                                                 \
        return names;                                      \
    }