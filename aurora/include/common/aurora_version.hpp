#pragma once

/* ════════════════════════════════════════════════════════════
   Aurora Universal Memory Safety — Version 1.0
   ════════════════════════════════════════════════════════════

   Phase 10: Production Release

   Features:
     - Compile-time Intelligent Allocation
     - Escape Analysis
     - Lifetime Analysis
     - Ownership & Alias Analysis
     - Allocation Strategy Engine
     - Optimized Code Generation
     - Runtime Infrastructure (Arena, ARC, GC)
     - Memory Profiling & Diagnostics
     - Benchmarking Suite

   ════════════════════════════════════════════════════════════ */

#define AURORA_VERSION_MAJOR  2
#define AURORA_VERSION_MINOR  0
#define AURORA_VERSION_PATCH  0
#define AURORA_VERSION_STRING "2.0.0"

/* Memory Safety Features */
#define AURORA_HAS_ESCAPE_ANALYSIS      1
#define AURORA_HAS_LIFETIME_ANALYSIS    1
#define AURORA_HAS_OWNERSHIP_ANALYSIS   1
#define AURORA_HAS_ALLOCATION_STRATEGY  1
#define AURORA_HAS_ARENA_ALLOCATOR      1
#define AURORA_HAS_ARC_RUNTIME          1
#define AURORA_HAS_GC_RUNTIME           1
#define AURORA_HAS_THREAD_SAFETY        1
#define AURORA_HAS_MEMORY_PROFILING     1
#define AURORA_HAS_BENCHMARKING         1

/* Performance Modes */
#define AURORA_PERFORMANCE_MODE_SUPPORT  1
#define AURORA_GC_DISABLED_IN_PERF       1

/* Version info function */
inline const char* aurora_version() {
    return AURORA_VERSION_STRING;
}

inline const char* aurora_version_full() {
    return "Aurora Universal Memory Safety v" AURORA_VERSION_STRING;
}
