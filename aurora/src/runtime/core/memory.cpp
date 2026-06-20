#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#ifdef _MSC_VER
#pragma intrinsic(_AddressOfReturnAddress)
#endif
#else
#include <pthread.h>
#endif

#include <string.h>

#include "runtime/memory.hpp"
#include "runtime/leak_detector.hpp"

/* ════════════════════════════════════════════════════════════
   Aurora Runtime — Memory Management & RAII (Phase 7)
   ════════════════════════════════════════════════════════════

   Contains:
     - SharedBox   : reference-counted heap wrapper (shared/weak ownership)
     - Arena       : bump-pointer allocator (4 MB blocks, thread-local)
     - RefCounting : aurora_refcount_inc/dec, aurora_weak_lock/release
     - DropGlue    : aurora_drop_glue for RAII cleanup
     - Diagnostics : memory statistics and reporting
     - Exception   : C++ exception-based try/throw (replaces setjmp/longjmp)

   ════════════════════════════════════════════════════════════ */

// ── SharedBox ─────────────────────────────────────────────────
struct SharedBoxInternal {
    uint64_t             magic;     /* 0xDEADB0B1 when alive */
    std::atomic<int64_t> strong_count;
    std::atomic<int64_t> weak_count;
    void*                data;
    void               (*destructor)(void*);
};

// ── Forward declarations ───────────────────────────────────
extern "C" void aurora_panic(const char* msg);
extern "C" void aurora_free(void* ptr);
extern "C" void* aurora_alloc(size_t size);
extern "C" void* aurora_safe_realloc(void* ptr, size_t size) {
    void* p = realloc(ptr, size);
    if (!p && size) {
        char msg[128];
        snprintf(msg, sizeof(msg), "out of memory: realloc(%zu bytes) failed", size);
        aurora_panic(msg);
    }
    return p;
}

extern "C" void* aurora_safe_calloc(size_t nmemb, size_t size) {
    void* p = calloc(nmemb, size);
    if (!p && nmemb && size) {
        char msg[128];
        snprintf(msg, sizeof(msg), "out of memory: calloc(%zu, %zu) failed", nmemb, size);
        aurora_panic(msg);
    }
    return p;
}

// ── Memory Arena Allocator ────────────────────────────────────
#define ARENA_BLOCK_SIZE (1024 * 1024 * 4) // 4 MB per block
struct ArenaBlock {
    ArenaBlock* next;
    size_t      used;
    char        data[ARENA_BLOCK_SIZE];
};

static thread_local ArenaBlock* current_arena = nullptr;

/* ── RAII guard: frees arena blocks automatically on thread exit ── */
static thread_local struct ArenaGuard {
    ~ArenaGuard() { aurora_arena_free(); }
} arena_guard;

// ── Memory Diagnostics ────────────────────────────────────────
static AuroraMemoryStats memory_stats = {};
static std::atomic<bool> diagnostics_enabled(false);

/* Use Windows SRWLOCK (POD - no destructor) instead of std::mutex
   to avoid static destruction order issues during CRT cleanup. */
#ifdef _WIN32
static SRWLOCK stats_lock = SRWLOCK_INIT;
#define LOCK_STATS() AcquireSRWLockExclusive(&stats_lock)
#define UNLOCK_STATS() ReleaseSRWLockExclusive(&stats_lock)
#else
static pthread_mutex_t stats_lock = PTHREAD_MUTEX_INITIALIZER;
#define LOCK_STATS() pthread_mutex_lock(&stats_lock)
#define UNLOCK_STATS() pthread_mutex_unlock(&stats_lock)
#endif

static void update_stats_alloc(size_t size) {
    if (diagnostics_enabled) {
        LOCK_STATS();
        memory_stats.arena_allocated += size;
        memory_stats.heap_allocated++;
        UNLOCK_STATS();
    }
}

static void update_stats_free(void) {
    if (diagnostics_enabled) {
        LOCK_STATS();
        memory_stats.heap_freed++;
        UNLOCK_STATS();
    }
}

// ═══════════════════════════════════════════════════════════════
// Arena Allocator Implementation
// ═══════════════════════════════════════════════════════════════

/* Maximum arena blocks — prevents runaway memory consumption */
#define AURORA_MAX_ARENA_BLOCKS 256  /* 256 × 4 MB = 1 GB max */

static int arena_block_count(void) {
    int n = 0;
    ArenaBlock* b = current_arena;
    while (b) { n++; b = b->next; }
    return n;
}

/* ── Global arena block tracking (for is_arena_ptr and cross-thread safety) ── */
struct ArenaBlockRange {
    void*    start;
    void*    end;
    int      refcount;
};

#ifdef _WIN32
static SRWLOCK arena_blocks_lock = SRWLOCK_INIT;
#define LOCK_ARENA_BLOCKS() AcquireSRWLockExclusive(&arena_blocks_lock)
#define UNLOCK_ARENA_BLOCKS() ReleaseSRWLockExclusive(&arena_blocks_lock)
#else
static pthread_mutex_t arena_blocks_lock = PTHREAD_MUTEX_INITIALIZER;
#define LOCK_ARENA_BLOCKS() pthread_mutex_lock(&arena_blocks_lock)
#define UNLOCK_ARENA_BLOCKS() pthread_mutex_unlock(&arena_blocks_lock)
#endif

static std::vector<ArenaBlockRange>* arena_blocks_global = nullptr;

static void track_arena_block(void* start, size_t size) {
    LOCK_ARENA_BLOCKS();
    if (!arena_blocks_global) arena_blocks_global = new std::vector<ArenaBlockRange>();
    arena_blocks_global->push_back({start, static_cast<char*>(start) + size, 0});
    UNLOCK_ARENA_BLOCKS();
}

static void untrack_arena_block(void* start, size_t size) {
    LOCK_ARENA_BLOCKS();
    if (arena_blocks_global) {
        void* end = static_cast<char*>(start) + size;
        for (auto it = arena_blocks_global->begin(); it != arena_blocks_global->end(); ) {
            if (it->start == start && it->end == end) {
                it = arena_blocks_global->erase(it);
            } else {
                ++it;
            }
        }
    }
    UNLOCK_ARENA_BLOCKS();
}

static bool is_arena_ptr(void* ptr) {
    LOCK_ARENA_BLOCKS();
    if (arena_blocks_global) {
        for (auto& range : *arena_blocks_global) {
            if (ptr >= range.start && ptr < range.end) {
                UNLOCK_ARENA_BLOCKS();
                return true;
            }
        }
    }
    UNLOCK_ARENA_BLOCKS();
    return false;
}

extern "C" {

void* aurora_arena_alloc(size_t size) {
    if (!current_arena || current_arena->used + size > ARENA_BLOCK_SIZE) {
        if (arena_block_count() >= AURORA_MAX_ARENA_BLOCKS) {
            aurora_panic("arena memory limit reached (1 GB)");
        }
        ArenaBlock* next_block = (ArenaBlock*)malloc(sizeof(ArenaBlock));
        if (!next_block) {
            aurora_panic("out of memory");
        }
        next_block->next = current_arena;
        next_block->used = 0;
        current_arena = next_block;
        track_arena_block(current_arena->data, ARENA_BLOCK_SIZE);

        if (diagnostics_enabled) {
            LOCK_STATS();
            memory_stats.arena_blocks++;
            UNLOCK_STATS();
        }
    }
    void* ptr = current_arena->data + current_arena->used;
    size_t aligned_size = (size + 7) & ~7; /* 8-byte alignment */
    current_arena->used += aligned_size;

    update_stats_alloc(aligned_size);
    return ptr;
}

void* aurora_arena_alloc_aligned(size_t size, size_t alignment) {
    if (!current_arena || current_arena->used + size > ARENA_BLOCK_SIZE) {
        if (arena_block_count() >= AURORA_MAX_ARENA_BLOCKS) {
            aurora_panic("arena memory limit reached (1 GB)");
        }
        ArenaBlock* next_block = (ArenaBlock*)malloc(sizeof(ArenaBlock));
        if (!next_block) {
            aurora_panic("out of memory");
        }
        next_block->next = current_arena;
        next_block->used = 0;
        current_arena = next_block;
        track_arena_block(current_arena->data, ARENA_BLOCK_SIZE);

        if (diagnostics_enabled) {
            LOCK_STATS();
            memory_stats.arena_blocks++;
            UNLOCK_STATS();
        }
    }

    /* Align the pointer */
    size_t current = (size_t)(current_arena->data + current_arena->used);
    size_t aligned = (current + alignment - 1) & ~(alignment - 1);
    size_t padding = aligned - current;

    void* ptr = current_arena->data + current_arena->used + padding;
    current_arena->used += padding + size;

    update_stats_alloc(padding + size);
    return ptr;
}

void aurora_arena_free(void) {
    ArenaBlock* block = current_arena;
    if (!block) return;
    size_t total_freed = 0;

    /* Free GC arena roots first to prevent dangling refs */
    aurora_gc_clear_arena_roots();

    ArenaBlock* prev = nullptr;
    while (block) {
        ArenaBlock* next = block->next;
        bool can_free = true;
        /* Check refcount — only free blocks with no outstanding GC roots */
        LOCK_ARENA_BLOCKS();
        if (arena_blocks_global) {
            void* data_start = block->data;
            void* data_end = static_cast<char*>(block->data) + ARENA_BLOCK_SIZE;
            for (auto& range : *arena_blocks_global) {
                if (range.start == data_start && range.end == data_end) {
                    if (range.refcount > 0) {
                        can_free = false;
                    }
                    break;
                }
            }
        }
        UNLOCK_ARENA_BLOCKS();

        if (can_free) {
            total_freed += block->used;
            untrack_arena_block(block->data, ARENA_BLOCK_SIZE);
            free(block);
            if (prev) prev->next = next;
            else current_arena = next;
        } else {
            prev = block;
        }
        block = next;
    }
    if (!current_arena) {
        if (diagnostics_enabled) {
            LOCK_STATS();
            memory_stats.arena_freed += total_freed;
            memory_stats.arena_blocks = 0;
            UNLOCK_STATS();
        }
    }
}

size_t aurora_arena_get_used(void) {
    size_t total = 0;
    ArenaBlock* block = current_arena;
    while (block) {
        total += block->used;
        block = block->next;
    }
    return total;
}

size_t aurora_arena_get_block_count(void) {
    size_t count = 0;
    ArenaBlock* block = current_arena;
    while (block) {
        count++;
        block = block->next;
    }
    return count;
}

} /* extern "C" arena */

// ═══════════════════════════════════════════════════════════════
// Reference Counting Implementation
// ═══════════════════════════════════════════════════════════════

extern "C" {

void aurora_drop_glue(void* ptr) {
    if (!ptr) return;
    /* Skip arena-allocated pointers (frees arena memory == UB) */
    if (is_arena_ptr(ptr)) return;
    /* Read magic via memcpy (avoids UB from reinterpret_cast on invalid objects) */
    uint64_t magic = 0;
    memcpy(&magic, ptr, sizeof(magic));
    if (magic != 0xDEADB0B1) {
        free(ptr);
        return;
    }
    SharedBoxInternal* box = static_cast<SharedBoxInternal*>(ptr);
    box->magic = 0;
    /* Call destructor if registered */
    if (box->destructor && box->data) {
        box->destructor(box->data);
        box->data = nullptr;
    }
}

void aurora_refcount_inc(void* ptr) {
    if (!ptr) return;
    SharedBoxInternal* box = reinterpret_cast<SharedBoxInternal*>(ptr);
    box->strong_count.fetch_add(1, std::memory_order_relaxed);

    if (diagnostics_enabled) {
        LOCK_STATS();
        memory_stats.total_refcount_ops++;
        UNLOCK_STATS();
    }
}

void aurora_refcount_dec(void* ptr) {
    if (!ptr) return;
    SharedBoxInternal* box = reinterpret_cast<SharedBoxInternal*>(ptr);
    int64_t prev = box->strong_count.fetch_sub(1, std::memory_order_acq_rel);
    if (prev == 1) {
        if (box->destructor && box->data)
            box->destructor(box->data);
        box->data = nullptr;
        if (box->weak_count.load(std::memory_order_acquire) == 0) {
            aurora_free(box);
        }
    }

    if (diagnostics_enabled) {
        LOCK_STATS();
        memory_stats.total_refcount_ops++;
        UNLOCK_STATS();
    }
}

int64_t aurora_refcount_get(void* ptr) {
    if (!ptr) return 0;
    SharedBoxInternal* box = reinterpret_cast<SharedBoxInternal*>(ptr);
    return box->strong_count.load(std::memory_order_acquire);
}

void* aurora_weak_new(void* shared_ptr) {
    if (!shared_ptr) return nullptr;
    SharedBoxInternal* box = reinterpret_cast<SharedBoxInternal*>(shared_ptr);
    box->weak_count.fetch_add(1, std::memory_order_relaxed);

    if (diagnostics_enabled) {
        LOCK_STATS();
        memory_stats.weak_count++;
        UNLOCK_STATS();
    }

    return shared_ptr;
}

void* aurora_weak_lock(void* ptr) {
    if (!ptr) return nullptr;
    SharedBoxInternal* box = reinterpret_cast<SharedBoxInternal*>(ptr);
    if (box->magic != 0xDEADB0B1) return nullptr;
    int64_t cur = box->strong_count.load(std::memory_order_acquire);
    while (cur > 0) {
        if (box->strong_count.compare_exchange_weak(
                cur, cur + 1,
                std::memory_order_acq_rel,
                std::memory_order_acquire))
            return box;
    }
    return nullptr;
}

void aurora_weak_release(void* ptr) {
    if (!ptr) return;
    SharedBoxInternal* box = reinterpret_cast<SharedBoxInternal*>(ptr);
    int64_t prev = box->weak_count.fetch_sub(1, std::memory_order_acq_rel);
    if (prev == 1) {
        if (box->strong_count.load(std::memory_order_acquire) == 0) {
            aurora_free(box);
        }
    }

    if (diagnostics_enabled) {
        LOCK_STATS();
        memory_stats.weak_count--;
        UNLOCK_STATS();
    }
}

int64_t aurora_weak_count(void* ptr) {
    if (!ptr) return 0;
    SharedBoxInternal* box = reinterpret_cast<SharedBoxInternal*>(ptr);
    return box->weak_count.load(std::memory_order_acquire);
}

void* aurora_shared_new(void* data, void (*destructor)(void*)) {
    SharedBoxInternal* box = (SharedBoxInternal*)aurora_alloc(sizeof(SharedBoxInternal));
    if (!box) {
        aurora_panic("out of memory");
    }
    box->magic = 0xDEADB0B1;
    box->strong_count.store(1, std::memory_order_relaxed);
    box->weak_count.store(0, std::memory_order_relaxed);
    box->data       = data;
    box->destructor = destructor;

    if (diagnostics_enabled) {
        LOCK_STATS();
        memory_stats.shared_count++;
        UNLOCK_STATS();
    }

    return box;
}

void aurora_set_destructor(void* ptr, void (*destructor)(void*)) {
    if (!ptr) return;
    SharedBoxInternal* box = reinterpret_cast<SharedBoxInternal*>(ptr);
    box->destructor = destructor;
}

void* aurora_box_string(const char* str) {
    size_t len = strlen(str);
    char* data = (char*)aurora_alloc(len + 1);
    if (data) {
        memcpy(data, str, len + 1);
    }
    return aurora_shared_new(data, free);
}

} /* extern "C" */

// ═══════════════════════════════════════════════════════════════
// Thread-Safe Operations
// ═══════════════════════════════════════════════════════════════

extern "C" {

void aurora_refcount_inc_atomic(void* ptr) {
    aurora_refcount_inc(ptr);
}

void aurora_refcount_dec_atomic(void* ptr) {
    aurora_refcount_dec(ptr);
}

void* aurora_weak_lock_atomic(void* weak_ptr) {
    return aurora_weak_lock(weak_ptr);
}

} /* extern "C" thread-safe */

// ═══════════════════════════════════════════════════════════════
// Memory Diagnostics
// ═══════════════════════════════════════════════════════════════

extern "C" {

void aurora_memory_get_stats(AuroraMemoryStats* stats) {
    if (!stats) return;
    LOCK_STATS();
    *stats = memory_stats;
    stats->arena_allocated = aurora_arena_get_used();
    stats->arena_blocks = aurora_arena_get_block_count();
    UNLOCK_STATS();
}

void aurora_memory_print_report(void) {
    AuroraMemoryStats stats;
    aurora_memory_get_stats(&stats);

    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║           Aurora Memory Report                          ║\n");
    printf("╠══════════════════════════════════════════════════════════╣\n");
    printf("║  Arena Allocator:                                       ║\n");
    printf("║    Allocated:   %8zu bytes                        ║\n", stats.arena_allocated);
    printf("║    Freed:       %8zu bytes                        ║\n", stats.arena_freed);
    printf("║    Blocks:      %8zu                               ║\n", stats.arena_blocks);
    printf("╠══════════════════════════════════════════════════════════╣\n");
    printf("║  Heap:                                                  ║\n");
    printf("║    Allocations: %8zu                               ║\n", stats.heap_allocated);
    printf("║    Frees:       %8zu                               ║\n", stats.heap_freed);
    printf("╠══════════════════════════════════════════════════════════╣\n");
    printf("║  Reference Counting:                                    ║\n");
    printf("║    Shared:      %8lld                              ║\n", (long long)stats.shared_count);
    printf("║    Weak:        %8lld                              ║\n", (long long)stats.weak_count);
    printf("║    Operations:  %8lld                              ║\n", (long long)stats.total_refcount_ops);
    printf("╚══════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

void aurora_memory_reset_stats(void) {
    LOCK_STATS();
    memory_stats = {};
    UNLOCK_STATS();
}

void aurora_memory_set_diagnostics(int enabled) {
    diagnostics_enabled.store(enabled != 0);
}

} /* extern "C" diagnostics */

// ═══════════════════════════════════════════════════════════════
// Heap Allocator (aurora_alloc / aurora_free)
// ═══════════════════════════════════════════════════════════════

extern "C" {

void* aurora_alloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr && size) {
        aurora_panic("out of memory");
    }
    aurora_leak_track(ptr, size, "heap");
    return ptr;
}

void aurora_free(void* ptr) {
    aurora_leak_untrack(ptr);
    free(ptr);
}

void* aurora_malloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr && size) {
        aurora_panic("out of memory");
    }
    return ptr;
}

}

// ═══════════════════════════════════════════════════════════════
// GC Runtime — Root-count-based allocation tracking
// ═══════════════════════════════════════════════════════════════

struct GCRecord {
    void* ptr;
    size_t size;
    int root_count;
    bool marked;
};

/* ── Mark helper (C++ linkage, outside extern "C") ── */
static void gc_mark_reachable(GCRecord& obj, std::vector<GCRecord>& objects) {
    if (obj.marked) return;
    obj.marked = true;
    if (obj.size == 0) return;

    char* start = static_cast<char*>(obj.ptr);
    char* end   = start + obj.size;
    for (char* p = start; p + sizeof(void*) <= end; p += sizeof(void*)) {
        void* candidate;
        memcpy(&candidate, p, sizeof(void*));
        if (!candidate) continue;
        for (auto& other : objects) {
            if (&other == &obj) continue;
            if (other.marked) continue;
            char* ostart = static_cast<char*>(other.ptr);
            char* oend   = ostart + other.size;
            if (candidate >= ostart && candidate < oend) {
                gc_mark_reachable(other, objects);
                break;
            }
        }
    }
}

/* Scan a memory range for pointers to GC objects and mark them */
static void gc_scan_and_mark_range(void* start, void* end, std::vector<GCRecord>& objects) {
    if (!start || !end || start >= end) return;
    for (char* p = static_cast<char*>(start); p + static_cast<ptrdiff_t>(sizeof(void*)) <= static_cast<char*>(end); p += sizeof(void*)) {
        void* candidate;
        memcpy(&candidate, p, sizeof(void*));
        if (!candidate) continue;
        for (auto& obj : objects) {
            if (obj.marked) continue;
            if (obj.size == 0) continue;
            char* ostart = static_cast<char*>(obj.ptr);
            char* oend   = ostart + obj.size;
            if (candidate >= ostart && candidate < oend) {
                gc_mark_reachable(obj, objects);
                break;
            }
        }
    }
}

static std::vector<GCRecord>*  gc_objects = nullptr;
static std::vector<void*>*     gc_unknown_roots = nullptr;
static std::atomic<size_t>     gc_live_objects{0};
static std::atomic<size_t>     gc_collected{0};
static std::atomic<size_t>     gc_alloc_since_last{0};
static std::atomic<size_t>     gc_auto_threshold{5000}; /* auto-collect every 5000 allocs */

/* Internal collect — assumes LOCK_GC is held */
static void gc_collect_impl(void) {
    if (!gc_objects || gc_objects->empty()) return;

    /* ── Mark phase ── */
    for (auto& obj : *gc_objects) {
        obj.marked = false;
    }

    /* Mark from registered roots */
    for (auto& obj : *gc_objects) {
        if (obj.root_count > 0) {
            gc_mark_reachable(obj, *gc_objects);
        }
    }

    /* Mark from unknown roots (scan their memory for pointers to GC objects) */
    if (gc_unknown_roots) {
        for (void* root : *gc_unknown_roots) {
            /* Unknown root memory may contain pointers to GC objects */
            /* Scan up to 1 KB (conservative; we don't know allocation size) */
            gc_scan_and_mark_range(root, static_cast<char*>(root) + 1024, *gc_objects);
        }
    }

    /* Mark from stack roots */
    {
#ifdef _WIN32
#if defined(_M_AMD64) || defined(__x86_64__)
#if defined(_MSC_VER)
        void* stack_base   = reinterpret_cast<void*>(__readgsqword(0x08));
        void* stack_limit  = reinterpret_cast<void*>(__readgsqword(0x10));
#else
        void* stack_base;
        void* stack_limit;
        __asm__("movq %%gs:0x08, %0" : "=r"(stack_base));
        __asm__("movq %%gs:0x10, %0" : "=r"(stack_limit));
#endif
#elif defined(_M_ARM64) || defined(__aarch64__)
        void* teb;
#if defined(_MSC_VER)
        teb = reinterpret_cast<void*>(__getReg(ARM64_TPIDRRO_EL0));
#else
        __asm__("mrs %0, tpidrro_el0" : "=r"(teb));
#endif
        void* stack_base  = *reinterpret_cast<void**>(static_cast<char*>(teb) + 0x08);
        void* stack_limit = *reinterpret_cast<void**>(static_cast<char*>(teb) + 0x10);
#else
#error "Unsupported Windows architecture"
#endif
#if defined(_MSC_VER)
        void* current_sp = _AddressOfReturnAddress();
#else
        void* current_sp = __builtin_frame_address(0);
#endif
        /* Scan from current frame toward the stack base */
        if (current_sp >= stack_limit && current_sp < stack_base) {
            gc_scan_and_mark_range(current_sp, stack_base, *gc_objects);
        }
#else
        /* Unix — use pthread_getattr_np to get stack bounds */
        pthread_attr_t attr;
        void* stack_addr = nullptr;
        size_t stack_size = 0;
        if (pthread_getattr_np(pthread_self(), &attr) == 0) {
            if (pthread_attr_getstack(&attr, &stack_addr, &stack_size) == 0) {
                void* stack_end = static_cast<char*>(stack_addr) + stack_size;
                void* sp = __builtin_frame_address(0);
                if (sp >= stack_addr && sp < stack_end) {
                    gc_scan_and_mark_range(sp, stack_end, *gc_objects);
                }
            }
            pthread_attr_destroy(&attr);
        }
#endif
    }

    /* ── Unmark objects with no roots — explicit root_count overrides conservative stack scan ── */
    for (auto& obj : *gc_objects) {
        if (obj.root_count <= 0) {
            obj.marked = false;
        }
    }

    /* ── Sweep phase ── */
    size_t freed = 0;
    for (auto it = gc_objects->begin(); it != gc_objects->end(); ) {
        if (!it->marked) {
            free(it->ptr);
            it = gc_objects->erase(it);
            freed++;
        } else {
            ++it;
        }
    }
    gc_live_objects.store(gc_objects->size(), std::memory_order_relaxed);
    gc_collected.fetch_add(freed, std::memory_order_relaxed);
}

/* POD lock for GC (no destructor issues) */
#ifdef _WIN32
static SRWLOCK gc_lock = SRWLOCK_INIT;
#define LOCK_GC() AcquireSRWLockExclusive(&gc_lock)
#define UNLOCK_GC() ReleaseSRWLockExclusive(&gc_lock)
#else
static pthread_mutex_t gc_lock = PTHREAD_MUTEX_INITIALIZER;
#define LOCK_GC() pthread_mutex_lock(&gc_lock)
#define UNLOCK_GC() pthread_mutex_unlock(&gc_lock)
#endif

extern "C" {

void aurora_gc_init(void) {
    LOCK_GC();
    if (!gc_objects) {
        gc_objects = new std::vector<GCRecord>();
    }
    if (!gc_unknown_roots) {
        gc_unknown_roots = new std::vector<void*>();
    }
    UNLOCK_GC();
}

void* aurora_gc_alloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr && size) {
        aurora_panic("aurora_gc_alloc: out of memory");
    }
    LOCK_GC();
    if (!gc_objects) gc_objects = new std::vector<GCRecord>();
    gc_objects->push_back({ptr, size, 0, false});
    gc_live_objects.store(gc_objects->size(), std::memory_order_relaxed);

    /* Auto-collect when threshold exceeded — inside the lock to prevent races */
    size_t since_last = gc_alloc_since_last.fetch_add(1, std::memory_order_relaxed) + 1;
    if (since_last >= gc_auto_threshold.load(std::memory_order_relaxed)) {
        gc_alloc_since_last.store(0, std::memory_order_relaxed);
        gc_collect_impl();
    }
    UNLOCK_GC();
    return ptr;
}

void aurora_gc_collect(void) {
    LOCK_GC();
    gc_collect_impl();
    UNLOCK_GC();
}

void aurora_gc_register_root(void* ptr) {
    if (!ptr) return;
    LOCK_GC();
    if (!gc_objects) gc_objects = new std::vector<GCRecord>();
    for (auto& obj : *gc_objects) {
        if (obj.ptr == ptr) {
            obj.root_count++;
            UNLOCK_GC();
            return;
        }
    }
    /* Unknown pointer — track in separate unknown_roots vector */
    if (!gc_unknown_roots) gc_unknown_roots = new std::vector<void*>();
    gc_unknown_roots->push_back(ptr);
    UNLOCK_GC();
}

void aurora_gc_unregister_root(void* ptr) {
    if (!ptr) return;
    LOCK_GC();
    if (gc_objects) {
        for (auto& obj : *gc_objects) {
            if (obj.ptr == ptr) {
                obj.root_count--;
                UNLOCK_GC();
                return;
            }
        }
    }
    if (gc_unknown_roots) {
        for (auto it = gc_unknown_roots->begin(); it != gc_unknown_roots->end(); ++it) {
            if (*it == ptr) {
                gc_unknown_roots->erase(it);
                UNLOCK_GC();
                return;
            }
        }
    }
    UNLOCK_GC();
}

void aurora_gc_get_stats(size_t* live_objects, size_t* collected) {
    if (live_objects) *live_objects = gc_live_objects.load(std::memory_order_relaxed);
    if (collected) *collected = gc_collected.load(std::memory_order_relaxed);
}

void aurora_gc_set_auto_collect_threshold(size_t alloc_count) {
    gc_auto_threshold.store(alloc_count, std::memory_order_relaxed);
}

void aurora_gc_trigger_check(void) {
    LOCK_GC();
    size_t since_last = gc_alloc_since_last.load(std::memory_order_relaxed);
    size_t threshold = gc_auto_threshold.load(std::memory_order_relaxed);
    if (since_last >= threshold) {
        gc_alloc_since_last.store(0, std::memory_order_relaxed);
        gc_collect_impl();
    }
    UNLOCK_GC();
}

} /* extern "C" GC */

// ═══════════════════════════════════════════════════════════════
// Exception Handling (return-based for JIT compatibility)
// C++ exceptions cannot propagate through JIT-compiled frames
// because ORC JIT does not emit unwind tables.  Instead, we use
// a thread-local flag: aurora_throw sets the flag and returns
// normally.  The try/catch in gen_throw generates a `ret` after
// the throw call (dead code path), so execution unwinds via
// normal function returns.  aurora_try_exec checks the flag
// after try_fn returns and runs the catch handler if set.
// ═══════════════════════════════════════════════════════════════

struct AuroraThrowState {
    bool    thrown;
    int64_t val;
};

static thread_local AuroraThrowState g_throw_state = {false, 0};

extern "C" {

void aurora_try_exec(AuroraTryFn try_fn, AuroraCatchFn catch_fn, int64_t* ctx,
                     AuroraFinallyFn finally_fn) {
    AuroraThrowState prev = g_throw_state;
    g_throw_state = {false, 0};

    try_fn(ctx);

    if (g_throw_state.thrown) {
        int64_t val = g_throw_state.val;
        g_throw_state = {false, 0};
        if (catch_fn) catch_fn(ctx, val);
    }

    if (finally_fn) finally_fn(ctx);

    g_throw_state = prev;
}

void aurora_throw(int64_t val, int64_t /*has*/) {
    g_throw_state = {true, val};
}

}

/* ═══════════════════════════════════════════════════════════════
   Phase 3: GC / Arena Coordination
   ═══════════════════════════════════════════════════════════════ */

/* GC root set for arena-scanned objects */
/* POD lock (no destructor) to avoid static destruction order issues */
#ifdef _WIN32
static SRWLOCK arena_gc_lock = SRWLOCK_INIT;
#define LOCK_ARENA_GC() AcquireSRWLockExclusive(&arena_gc_lock)
#define UNLOCK_ARENA_GC() ReleaseSRWLockExclusive(&arena_gc_lock)
#else
static pthread_mutex_t arena_gc_lock = PTHREAD_MUTEX_INITIALIZER;
#define LOCK_ARENA_GC() pthread_mutex_lock(&arena_gc_lock)
#define UNLOCK_ARENA_GC() pthread_mutex_unlock(&arena_gc_lock)
#endif
static std::vector<void*>* arena_gc_roots = nullptr;

extern "C" {

/* Register an arena-allocated object as a GC root (so GC can see it) */
void aurora_arena_register_gc_root(void* ptr) {
    if (!ptr) return;
    LOCK_ARENA_GC();
    if (!arena_gc_roots) arena_gc_roots = new std::vector<void*>();
    arena_gc_roots->push_back(ptr);
    /* Increment arena block refcount to prevent cross-thread UAF */
    LOCK_ARENA_BLOCKS();
    if (arena_blocks_global) {
        for (auto& range : *arena_blocks_global) {
            if (ptr >= range.start && ptr < range.end) {
                range.refcount++;
                break;
            }
        }
    }
    UNLOCK_ARENA_BLOCKS();
    UNLOCK_ARENA_GC();
}

/* Unregister an arena-allocated object from GC roots */
void aurora_arena_unregister_gc_root(void* ptr) {
    if (!ptr || !arena_gc_roots) return;
    LOCK_ARENA_GC();
    for (auto it = arena_gc_roots->begin(); it != arena_gc_roots->end(); ++it) {
        if (*it == ptr) {
            arena_gc_roots->erase(it);
            /* Decrement arena block refcount */
            LOCK_ARENA_BLOCKS();
            if (arena_blocks_global) {
                for (auto& range : *arena_blocks_global) {
                    if (ptr >= range.start && ptr < range.end) {
                        if (range.refcount > 0) range.refcount--;
                        break;
                    }
                }
            }
            UNLOCK_ARENA_BLOCKS();
            UNLOCK_ARENA_GC();
            return;
        }
    }
    UNLOCK_ARENA_GC();
}

/* Tell GC to scan arena roots as well */
void aurora_gc_scan_arena_roots(void) {
    if (!arena_gc_roots || !gc_objects) return;
    LOCK_GC();

    for (void* arena_ptr : *arena_gc_roots) {
        for (auto& obj : *gc_objects) {
            if (obj.ptr == arena_ptr && obj.root_count == 0) {
                obj.root_count = 1;
            }
        }
    }
    UNLOCK_GC();
}

/* Memory pressure: check if arena usage exceeds threshold and trigger GC */
int aurora_memory_is_pressure_high(void) {
    size_t used = aurora_arena_get_used();
    /* Consider memory pressure high if arena usage > 100 MB */
    return (used > 100 * 1024 * 1024) ? 1 : 0;
}

/* Hybrid: tell GC it can free arena objects by zeroing their root counts */
void aurora_gc_clear_arena_roots(void) {
    if (!arena_gc_roots || !gc_objects) return;
    LOCK_GC();
    for (void* arena_ptr : *arena_gc_roots) {
        for (auto& obj : *gc_objects) {
            if (obj.ptr == arena_ptr) {
                obj.root_count = 0;
            }
        }
    }
    UNLOCK_GC();
}

/* Get total memory usage across all allocators */
size_t aurora_memory_total_usage(void) {
    size_t arena_used = aurora_arena_get_used();
    /* Add heap usage from diagnostics */
    AuroraMemoryStats stats;
    aurora_memory_get_stats(&stats);
    /* Rough estimate: each heap alloc ~64 bytes avg */
    return arena_used + (stats.heap_allocated * 64);
}

} /* extern "C" coordination */
