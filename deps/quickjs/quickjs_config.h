/* QuickJS compatibility config for Windows (MSVC or Zig cc/MinGW) */
/* This file is force-included first via -include /FI */

/* Always available: CONFIG_VERSION */
#define CONFIG_VERSION "2021-03-27"

/* Always available: inttypes.h for PRId64 etc */
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

/* On Windows, disable pthread-based atomics (no pthread.h available) */
#if defined(_WIN32) || defined(_WIN64)
#undef CONFIG_ATOMICS
#endif

#ifdef _MSC_VER
#pragma warning(disable: 4005) /* macro redefinition */

/* Suppress unsafe function deprecation warnings */
#define _CRT_SECURE_NO_WARNINGS 1
#define _CRT_NONSTDC_NO_WARNINGS 1

/* Define POSIX S_ISREG/S_ISDIR for MSVC CRT */
#ifndef S_ISREG
#include <sys/stat.h>
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#endif
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif

/* MSVC needs malloc.h for alloca() */
#include <malloc.h>

/* Provide gettimeofday for Windows (MSVC doesn't have it) */
#include <windows.h>
struct timeval { long tv_sec; long tv_usec; };
static __forceinline int gettimeofday(struct timeval *tv, void *tz) {
    static int init = 0; static LARGE_INTEGER freq;
    if (!init) { QueryPerformanceFrequency(&freq); init = 1; }
    LARGE_INTEGER pc; QueryPerformanceCounter(&pc);
    tv->tv_sec = (long)(pc.QuadPart / freq.QuadPart);
    tv->tv_usec = (long)((pc.QuadPart % freq.QuadPart) * 1000000 / freq.QuadPart);
    return 0;
}

/* __attribute__ not supported by MSVC */
#define __attribute__(x) /* nothing */

/* force_inline / no_inline / maybe_unused for MSVC */
#define force_inline __forceinline
#define no_inline __declspec(noinline)
#define __maybe_unused /* nothing */

/* js_force_inline / printf format / warn_unused_result for MSVC */
#define js_force_inline __forceinline
#define __js_printf_like(f, a) /* nothing */
#define __exception /* nothing */

/* Disable source_location on MSVC (uses __builtin_FILE/LINE) */
#define js_source_location __FILE__, __LINE__

/* __builtin_ctz / __builtin_ctzll are GCC/Clang builtins — provide MSVC fallback */
#include <intrin.h>
#ifdef __cplusplus
extern "C" {
#endif
static __forceinline int __builtin_ctz(unsigned int x) {
    unsigned long idx; _BitScanForward(&idx, x); return (int)idx;
}
static __forceinline int __builtin_ctzll(unsigned long long x) {
    unsigned long idx; _BitScanForward64(&idx, x); return (int)idx;
}
#ifdef __cplusplus
}
#endif

#endif /* _MSC_VER */
