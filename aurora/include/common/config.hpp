#pragma once

/* ════════════════════════════════════════════════════════════
   config.hpp — Aurora Compiler Configuration
   ════════════════════════════════════════════════════════════
   Compile-time flags that control compiler behaviour.
   ════════════════════════════════════════════════════════════ */

/* ── Debug / Diagnostics ── */
#ifndef AURORA_DEBUG
    #ifdef _DEBUG
        #define AURORA_DEBUG 1
    #else
        #define AURORA_DEBUG 0
    #endif
#endif

/* ── Assert (always-on internal check, not disabled by NDEBUG) ── */
#if AURORA_DEBUG
    #include <cassert>
    #define AURORA_ASSERT(cond, msg) assert((cond) && (msg))
#else
    #define AURORA_ASSERT(cond, msg) ((void)0)
#endif

/* ── Feature toggles ── */
#ifndef AURORA_ENABLE_PROFILER
    #define AURORA_ENABLE_PROFILER 1
#endif

#ifndef AURORA_ENABLE_BENCHMARKS
    #define AURORA_ENABLE_BENCHMARKS 1
#endif

#ifndef AURORA_ENABLE_GC
    #define AURORA_ENABLE_GC 1
#endif

#ifndef AURORA_MAX_OPTIMIZER_ITERATIONS
    #define AURORA_MAX_OPTIMIZER_ITERATIONS 5
#endif

#ifndef AURORA_DEFAULT_ARRAY_CAPACITY
    #define AURORA_DEFAULT_ARRAY_CAPACITY 16
#endif

#ifndef AURORA_ARENA_BLOCK_SIZE
    #define AURORA_ARENA_BLOCK_SIZE (4 * 1024 * 1024)  /* 4 MB */
#endif

#ifndef AURORA_GC_COLLECT_THRESHOLD
    #define AURORA_GC_COLLECT_THRESHOLD 1024
#endif
