#pragma once

/* ════════════════════════════════════════════════════════════
   platform.hpp — Platform & Compiler Detection
   ════════════════════════════════════════════════════════════
   Provides macros for platform-specific code paths,
   compiler attributes, and export/import declarations.
   ════════════════════════════════════════════════════════════ */

/* ── Compiler Detection ── */
#if defined(_MSC_VER)
    #define AURORA_COMPILER_MSVC   1
    #define AURORA_COMPILER_GCC    0
    #define AURORA_COMPILER_CLANG  0
#elif defined(__clang__)
    #define AURORA_COMPILER_MSVC   0
    #define AURORA_COMPILER_GCC    0
    #define AURORA_COMPILER_CLANG  1
#elif defined(__GNUC__)
    #define AURORA_COMPILER_MSVC   0
    #define AURORA_COMPILER_GCC    1
    #define AURORA_COMPILER_CLANG  0
#else
    #define AURORA_COMPILER_MSVC   0
    #define AURORA_COMPILER_GCC    0
    #define AURORA_COMPILER_CLANG  0
#endif

/* ── Platform Detection ── */
#if defined(_WIN32) || defined(_WIN64)
    #define AURORA_PLATFORM_WINDOWS 1
    #define AURORA_PLATFORM_LINUX   0
    #define AURORA_PLATFORM_MACOS   0
    #define AURORA_PLATFORM_ANDROID 0
    #define AURORA_PLATFORM_IOS     0
#elif defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_IPHONE
        #define AURORA_PLATFORM_WINDOWS 0
        #define AURORA_PLATFORM_LINUX   0
        #define AURORA_PLATFORM_MACOS   0
        #define AURORA_PLATFORM_ANDROID 0
        #define AURORA_PLATFORM_IOS     1
    #else
        #define AURORA_PLATFORM_WINDOWS 0
        #define AURORA_PLATFORM_LINUX   0
        #define AURORA_PLATFORM_MACOS   1
        #define AURORA_PLATFORM_ANDROID 0
        #define AURORA_PLATFORM_IOS     0
    #endif
#elif defined(__ANDROID__)
    #define AURORA_PLATFORM_WINDOWS 0
    #define AURORA_PLATFORM_LINUX   0
    #define AURORA_PLATFORM_MACOS   0
    #define AURORA_PLATFORM_ANDROID 1
    #define AURORA_PLATFORM_IOS     0
#elif defined(__linux__)
    #define AURORA_PLATFORM_WINDOWS 0
    #define AURORA_PLATFORM_LINUX   1
    #define AURORA_PLATFORM_MACOS   0
    #define AURORA_PLATFORM_ANDROID 0
    #define AURORA_PLATFORM_IOS     0
#else
    #define AURORA_PLATFORM_WINDOWS 0
    #define AURORA_PLATFORM_LINUX   0
    #define AURORA_PLATFORM_MACOS   0
    #define AURORA_PLATFORM_ANDROID 0
    #define AURORA_PLATFORM_IOS     0
#endif

/* ── Architecture Detection ── */
#if defined(_M_AMD64) || defined(__x86_64__)
    #define AURORA_ARCH_X86_64 1
    #define AURORA_ARCH_AARCH64 0
#elif defined(_M_ARM64) || defined(__aarch64__)
    #define AURORA_ARCH_X86_64 0
    #define AURORA_ARCH_AARCH64 1
#else
    #define AURORA_ARCH_X86_64 0
    #define AURORA_ARCH_AARCH64 0
#endif

/* ── DLL Export / Import ── */
#if AURORA_COMPILER_MSVC
    #define AURORA_EXPORT __declspec(dllexport)
    #define AURORA_IMPORT __declspec(dllimport)
#elif AURORA_COMPILER_GCC || AURORA_COMPILER_CLANG
    #define AURORA_EXPORT __attribute__((visibility("default")))
    #define AURORA_IMPORT __attribute__((visibility("default")))
#else
    #define AURORA_EXPORT
    #define AURORA_IMPORT
#endif

/* ── Inline hints ── */
#if AURORA_COMPILER_MSVC
    #define AURORA_FORCEINLINE __forceinline
#elif AURORA_COMPILER_GCC || AURORA_COMPILER_CLANG
    #define AURORA_FORCEINLINE __attribute__((always_inline)) inline
#else
    #define AURORA_FORCEINLINE inline
#endif

/* ── Likely / Unlikely (PGO hints) ── */
#if AURORA_COMPILER_GCC || AURORA_COMPILER_CLANG
    #define AURORA_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define AURORA_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define AURORA_LIKELY(x)   (x)
#define AURORA_UNLIKELY(x) (x)
#endif

/* ── Portable strtok (strtok_s on MSVC, strtok_r elsewhere) ── */
#if AURORA_COMPILER_MSVC
    #define AURORA_STRTOK(str, delim, ctx) strtok_s(str, delim, ctx)
#else
    #define AURORA_STRTOK(str, delim, ctx) strtok_r(str, delim, ctx)
#endif

/* ── Portable strdup (MSVC: _strdup, POSIX: strdup) ── */
#if AURORA_PLATFORM_WINDOWS && AURORA_COMPILER_MSVC
    #define AURORA_STRDUP(s) _strdup(s)
#else
    #define AURORA_STRDUP(s) strdup(s)
#endif
