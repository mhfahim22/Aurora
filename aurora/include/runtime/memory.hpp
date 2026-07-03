#pragma once
// memory.hpp — Runtime memory management API
// Phase 7: Runtime Infrastructure

#include <cstdint>
#include <cstddef>
#include <atomic>

/* ════════════════════════════════════════════════════════════
   Aurora Runtime — Memory Management API
   ════════════════════════════════════════════════════════════

   This header exposes the runtime memory management functions
   for the Aurora compiler and runtime.

   Memory Strategies:
     Stack    — compiler-managed alloca (fastest)
     Arena    — bump pointer allocator (fast, bulk free)
     RAII     — deterministic destruction
     ARC      — reference counting (shared ownership)
     GC       — garbage collection (future)

   ════════════════════════════════════════════════════════════ */

#ifdef __cplusplus
extern "C" {
#endif

/* ── SharedBox structure ── */
#ifdef __cplusplus
}
#endif
struct SharedBox {
    uint64_t             magic;
    std::atomic<int64_t> strong_count;
    std::atomic<int64_t> weak_count;
    void*                data;
    void               (*destructor)(void*);
};

/* Phase 28 — Pool Allocator (small object cache) */

/* Pool buckets for common sizes: 8, 16, 32, 64, 128 bytes */
#define POOL_BUCKET_COUNT 5

#ifdef __cplusplus
extern "C" {
#endif

void* aurora_pool_alloc(size_t size);
void  aurora_pool_free(void* ptr, size_t size);
void  aurora_pool_cleanup(void);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ════════════════════════════════════════════════════════════
   Arena Allocator API
   ════════════════════════════════════════════════════════════ */

/* Allocate memory from the arena (bump pointer) */
void* aurora_arena_alloc(size_t size);

/* Allocate aligned memory from the arena */
void* aurora_arena_alloc_aligned(size_t size, size_t alignment);

/* Free all arena memory (bulk free) */
void aurora_arena_free(void);

/* Get arena statistics */
size_t aurora_arena_get_used(void);
size_t aurora_arena_get_block_count(void);

/* ════════════════════════════════════════════════════════════
   Reference Counting API (ARC)
   ════════════════════════════════════════════════════════════ */

/* Create a new shared box with reference counting */
void* aurora_shared_new(void* data, void (*destructor)(void*));

/* Increment strong reference count */
void aurora_refcount_inc(void* ptr);

/* Decrement strong reference count (drop if zero) */
void aurora_refcount_dec(void* ptr);

/* Get current strong reference count */
int64_t aurora_refcount_get(void* ptr);

/* ════════════════════════════════════════════════════════════
   Weak Reference API
   ════════════════════════════════════════════════════════════ */

/* Create a weak reference */
void* aurora_weak_new(void* shared_ptr);

/* Lock weak reference (returns null if dead) */
void* aurora_weak_lock(void* weak_ptr);

/* Release weak reference */
void aurora_weak_release(void* weak_ptr);

/* Get weak reference count */
int64_t aurora_weak_count(void* ptr);

/* ════════════════════════════════════════════════════════════
   Drop Glue API (RAII)
   ════════════════════════════════════════════════════════════ */

/* Call destructor and cleanup */
void aurora_drop_glue(void* ptr);

/* Register destructor for a pointer */
void aurora_set_destructor(void* ptr, void (*destructor)(void*));

/* ════════════════════════════════════════════════════════════
   Thread-Safe Operations
   ════════════════════════════════════════════════════════════ */

/* Thread-safe reference count increment */
void aurora_refcount_inc_atomic(void* ptr);

/* Thread-safe reference count decrement */
void aurora_refcount_dec_atomic(void* ptr);

/* Thread-safe weak lock */
void* aurora_weak_lock_atomic(void* weak_ptr);

/* ════════════════════════════════════════════════════════════
   Memory Diagnostics
   ════════════════════════════════════════════════════════════ */

/* Memory diagnostics structure */
struct AuroraMemoryStats {
    size_t arena_allocated;      /* total bytes allocated in arena */
    size_t arena_freed;          /* total bytes freed */
    size_t arena_blocks;         /* number of arena blocks */
    size_t heap_allocated;       /* total heap allocations */
    size_t heap_freed;           /* total heap frees */
    int64_t shared_count;        /* number of shared boxes */
    int64_t weak_count;          /* number of weak references */
    int64_t total_refcount_ops;  /* total refcount operations */
};

/* Get current memory statistics */
void aurora_memory_get_stats(AuroraMemoryStats* stats);

/* Print memory report to stdout */
void aurora_memory_print_report(void);

/* Reset memory statistics */
void aurora_memory_reset_stats(void);

/* Enable/disable memory diagnostics */
void aurora_memory_set_diagnostics(int enabled);

/* ════════════════════════════════════════════════════════════
   Heap Allocator API
   ════════════════════════════════════════════════════════════ */

/* Allocate memory from the heap (malloc wrapper) */
void* aurora_alloc(size_t size);

/* Free heap memory */
void aurora_free(void* ptr);

/* ════════════════════════════════════════════════════════════
   GC Runtime API
   ════════════════════════════════════════════════════════════ */

/* Allocate memory tracked by the GC */
void* aurora_gc_alloc(size_t size);

/* Initialize GC */
void aurora_gc_init(void);

/* Run garbage collection — frees objects with no remaining roots */
void aurora_gc_collect(void);

/* Register root for GC — increments root count for ptr */
void aurora_gc_register_root(void* ptr);

/* Unregister root from GC — decrements root count for ptr */
void aurora_gc_unregister_root(void* ptr);

/* Get GC statistics */
void aurora_gc_get_stats(size_t* live_objects, size_t* collected);

/* Set auto-collect threshold (number of allocations between collections, default 5000) */
void aurora_gc_set_auto_collect_threshold(size_t alloc_count);

/* Trigger a GC check — runs collect if threshold exceeded */
void aurora_gc_trigger_check(void);

/* ════════════════════════════════════════════════════════════
    Phase 3: GC / Arena Coordination API
    ════════════════════════════════════════════════════════════ */

/* Register arena pointer as GC root (hybrid mode) */
void aurora_arena_register_gc_root(void* ptr);
void aurora_arena_unregister_gc_root(void* ptr);

/* Tell GC to scan arena roots during collection */
void aurora_gc_scan_arena_roots(void);

/* Check if memory pressure is high */
int aurora_memory_is_pressure_high(void);

/* Clear arena root references from GC (before arena free) */
void aurora_gc_clear_arena_roots(void);

/* Get total memory usage across all allocators */
size_t aurora_memory_total_usage(void);

/* ════════════════════════════════════════════════════════════
    Exception Handling API
    ════════════════════════════════════════════════════════════ */

/* Exception handling function types */
typedef void (*AuroraTryFn)(int64_t*);
typedef void (*AuroraCatchFn)(int64_t*, int64_t);
typedef void (*AuroraFinallyFn)(int64_t*);

/* C++ exception-based try/catch — runs try_fn; if it throws, runs catch_fn.
   finally_fn (if non-null) runs after try/catch in all cases. */
void aurora_try_exec(AuroraTryFn try_fn, AuroraCatchFn catch_fn, int64_t* ctx,
                     AuroraFinallyFn finally_fn);

/* Throw exception */
void aurora_throw(int64_t val, int64_t has);

/* Safe realloc wrapper — panics on OOM */
void* aurora_safe_realloc(void* ptr, size_t size);

/* Safe calloc wrapper — panics on OOM */
void* aurora_safe_calloc(size_t nmemb, size_t size);

#ifdef __cplusplus
}
#endif
