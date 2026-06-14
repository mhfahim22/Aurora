// leak_detector.cpp — Per-allocation leak detection with backtrace capture
// Phase 8: Memory Safety

#include "runtime/leak_detector.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <ctime>

/* ── Platform backtrace support ── */
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
/* RtlCaptureStackBackTrace is available in ntdll on all Windows versions */
#pragma comment(lib, "ntdll.lib")
extern "C" {
    typedef USHORT (WINAPI *RtlCaptureStackBackTrace_t)(ULONG, ULONG, PVOID*, PULONG);
    static RtlCaptureStackBackTrace_t s_RtlCaptureStackBackTrace = nullptr;
}
static int capture_backtrace(void** frames, int max_depth) {
    if (!s_RtlCaptureStackBackTrace) {
        HMODULE ntdll = GetModuleHandleA("ntdll.dll");
        if (ntdll) s_RtlCaptureStackBackTrace = (RtlCaptureStackBackTrace_t)
            GetProcAddress(ntdll, "RtlCaptureStackBackTrace");
        if (!s_RtlCaptureStackBackTrace) return 0;
    }
    return (int)s_RtlCaptureStackBackTrace(1, max_depth, frames, nullptr);
}
#else
#include <execinfo.h>
static int capture_backtrace(void** frames, int max_depth) {
    return backtrace(frames, max_depth);
}
#endif

/* ── Internal record ── */
struct LeakRecord {
    void*       ptr;
    size_t      size;
    const char* allocator;  /* "heap", "arena", "shared", "gc" */
    uint64_t    id;
    time_t      timestamp;
    void*       backtrace[AURORA_LEAK_BACKTRACE_DEPTH];
    int         backtrace_depth;
};

/* ── State ── */
static struct {
    std::atomic<int>            initialized{0};
    std::atomic<int>            enabled{0};
    LeakRecord*                 records;
    size_t                      capacity;
    std::atomic<size_t>         count{0};
    std::atomic<uint64_t>       next_id{1};
    std::atomic<uint64_t>       total_alloc{0};
    std::atomic<uint64_t>       total_free{0};
    size_t                      min_size;
    int                         auto_report_registered;
#ifdef _WIN32
    CRITICAL_SECTION            lock;
#else
    pthread_mutex_t             lock;
#endif
} g_state;

static void state_lock(void) {
#ifdef _WIN32
    EnterCriticalSection(&g_state.lock);
#else
    pthread_mutex_lock(&g_state.lock);
#endif
}

static void state_unlock(void) {
#ifdef _WIN32
    LeaveCriticalSection(&g_state.lock);
#else
    pthread_mutex_unlock(&g_state.lock);
#endif
}

/* ── FNV-1a hash for pointer → bucket ── */
static size_t hash_ptr(void* ptr) {
    uint64_t h = 0xcbf29ce484222325ULL;
    uint64_t k = (uint64_t)(uintptr_t)ptr;
    for (int i = 0; i < 8; i++) {
        h ^= (k >> (i * 8)) & 0xFF;
        h *= 0x100000001b3ULL;
    }
    return (size_t)(h & (uint64_t)(g_state.capacity - 1));
}

/* ── Init ── */
void aurora_leak_init(void) {
    if (g_state.initialized.load()) return;
#ifdef _WIN32
    InitializeCriticalSection(&g_state.lock);
#else
    pthread_mutex_init(&g_state.lock, nullptr);
#endif
    g_state.capacity = 65536;  /* power of 2 for fast modulo */
    g_state.records = (LeakRecord*)calloc(g_state.capacity, sizeof(LeakRecord));
    if (!g_state.records) {
        fprintf(stderr, "[leak] FATAL: could not allocate %zu records\n", g_state.capacity);
        return;
    }
    g_state.count.store(0);
    g_state.next_id.store(1);
    g_state.total_alloc.store(0);
    g_state.total_free.store(0);
    g_state.min_size = 0;
    g_state.auto_report_registered = 0;
    g_state.initialized.store(1);
}

static void ensure_init(void) {
    if (!g_state.initialized.load()) aurora_leak_init();
}

/* ── Enable / Disable ── */
void aurora_leak_enable(void) {
    ensure_init();
    g_state.enabled.store(1);
}

void aurora_leak_disable(void) {
    g_state.enabled.store(0);
}

int aurora_leak_is_enabled(void) {
    return g_state.enabled.load();
}

/* ── Track ── */
void aurora_leak_track(void* ptr, size_t size, const char* allocator) {
    if (!ptr || !g_state.enabled.load()) return;
    ensure_init();
    if (size < g_state.min_size) return;

    state_lock();
    if (g_state.count.load() >= AURORA_LEAK_MAX_RECORDS) {
        state_unlock();
        return;
    }

    size_t idx = hash_ptr(ptr);
    /* Linear probing */
    while (g_state.records[idx].ptr != nullptr && g_state.records[idx].ptr != ptr) {
        idx = (idx + 1) & (g_state.capacity - 1);
    }

    if (g_state.records[idx].ptr == ptr) {
        /* Already tracked — update size */
        g_state.records[idx].size = size;
        state_unlock();
        return;
    }

    g_state.records[idx].ptr = ptr;
    g_state.records[idx].size = size;
    g_state.records[idx].allocator = allocator;
    g_state.records[idx].id = g_state.next_id.fetch_add(1);
    g_state.records[idx].timestamp = std::time(nullptr);
    g_state.records[idx].backtrace_depth = capture_backtrace(
        g_state.records[idx].backtrace, AURORA_LEAK_BACKTRACE_DEPTH);

    g_state.count.fetch_add(1);
    g_state.total_alloc.fetch_add(1);
    state_unlock();
}

/* ── Untrack ── */
void aurora_leak_untrack(void* ptr) {
    if (!ptr || !g_state.enabled.load()) return;
    ensure_init();

    state_lock();
    size_t idx = hash_ptr(ptr);
    size_t start = idx;
    while (g_state.records[idx].ptr != nullptr) {
        if (g_state.records[idx].ptr == ptr) {
            g_state.records[idx].ptr = nullptr;
            g_state.records[idx].size = 0;
            g_state.records[idx].allocator = nullptr;
            g_state.records[idx].backtrace_depth = 0;
            g_state.count.fetch_sub(1);
            g_state.total_free.fetch_add(1);
            break;
        }
        idx = (idx + 1) & (g_state.capacity - 1);
        if (idx == start) break;
    }
    state_unlock();
}

/* ── Report (text) ── */
void aurora_leak_report(void) {
    ensure_init();
    state_lock();
    size_t n = g_state.count.load();
    size_t bytes = 0;
    for (size_t i = 0; i < g_state.capacity; i++) {
        if (g_state.records[i].ptr) bytes += g_state.records[i].size;
    }

    fprintf(stderr, "\n");
    fprintf(stderr, "╔═══════════════════════════════════════════════════════════\n");
    fprintf(stderr, "║  Aurora Leak Detector Report\n");
    fprintf(stderr, "╠═══════════════════════════════════════════════════════════\n");
    fprintf(stderr, "║  Live allocations: %llu\n", (unsigned long long)n);
    fprintf(stderr, "║  Total bytes:      %llu\n", (unsigned long long)bytes);
    fprintf(stderr, "║  Total alloc'd:    %llu\n", (unsigned long long)g_state.total_alloc.load());
    fprintf(stderr, "║  Total freed:      %llu\n", (unsigned long long)g_state.total_free.load());
    fprintf(stderr, "╚═══════════════════════════════════════════════════════════\n\n");

    if (n > 100) {
        fprintf(stderr, "[leak] %llu leaks — showing first 100:\n\n",
                (unsigned long long)n);
    }

    int shown = 0;
    for (size_t i = 0; i < g_state.capacity && shown < 100; i++) {
        if (!g_state.records[i].ptr) continue;
        shown++;

        fprintf(stderr, "  #%llu  %p  %llu bytes  [%s]  %s",
                (unsigned long long)g_state.records[i].id,
                g_state.records[i].ptr,
                (unsigned long long)g_state.records[i].size,
                g_state.records[i].allocator ? g_state.records[i].allocator : "?",
                ctime(&g_state.records[i].timestamp));

        /* Print backtrace */
        for (int j = 0; j < g_state.records[i].backtrace_depth && j < 4; j++) {
            fprintf(stderr, "         #%-2d %p\n", j, g_state.records[i].backtrace[j]);
        }
        if (g_state.records[i].backtrace_depth > 4) {
            fprintf(stderr, "         ... (%d more frames)\n",
                    g_state.records[i].backtrace_depth - 4);
        }
        fprintf(stderr, "\n");
    }

    if (n == 0) {
        fprintf(stderr, "  (no leaks detected)\n\n");
    }

    state_unlock();
}

/* ── Report (JSON) ── */
char* aurora_leak_report_json(void) {
    ensure_init();
    state_lock();
    size_t n = g_state.count.load();
    size_t bytes = 0;
    for (size_t i = 0; i < g_state.capacity; i++) {
        if (g_state.records[i].ptr) bytes += g_state.records[i].size;
    }

    /* Estimate buffer size */
    size_t buf_size = 4096 + n * 256;
    char* buf = (char*)malloc(buf_size);
    if (!buf) { state_unlock(); return nullptr; }
    size_t pos = 0;

    pos += snprintf(buf + pos, buf_size - pos, "{\n");
    pos += snprintf(buf + pos, buf_size - pos, "  \"live_allocations\": %llu,\n", (unsigned long long)n);
    pos += snprintf(buf + pos, buf_size - pos, "  \"total_bytes\": %llu,\n", (unsigned long long)bytes);
    pos += snprintf(buf + pos, buf_size - pos, "  \"total_allocated\": %llu,\n", (unsigned long long)g_state.total_alloc.load());
    pos += snprintf(buf + pos, buf_size - pos, "  \"total_freed\": %llu,\n", (unsigned long long)g_state.total_free.load());
    pos += snprintf(buf + pos, buf_size - pos, "  \"leaks\": [\n");

    int first = 1;
    for (size_t i = 0; i < g_state.capacity; i++) {
        if (!g_state.records[i].ptr) continue;
        if (!first) { pos += snprintf(buf + pos, buf_size - pos, ",\n"); }
        first = 0;

        pos += snprintf(buf + pos, buf_size - pos,
            "    { \"id\": %llu, \"ptr\": \"%p\", \"size\": %llu, "
            "\"allocator\": \"%s\", \"timestamp\": %lld }",
            (unsigned long long)g_state.records[i].id,
            g_state.records[i].ptr,
            (unsigned long long)g_state.records[i].size,
            g_state.records[i].allocator ? g_state.records[i].allocator : "?",
            (long long)g_state.records[i].timestamp);
    }

    pos += snprintf(buf + pos, buf_size - pos, "\n  ]\n}\n");
    state_unlock();
    return buf;
}

/* ── Clear ── */
void aurora_leak_clear(void) {
    ensure_init();
    state_lock();
    for (size_t i = 0; i < g_state.capacity; i++) {
        g_state.records[i].ptr = nullptr;
        g_state.records[i].size = 0;
        g_state.records[i].allocator = nullptr;
        g_state.records[i].backtrace_depth = 0;
    }
    g_state.count.store(0);
    g_state.total_alloc.store(0);
    g_state.total_free.store(0);
    state_unlock();
}

/* ── Accessors ── */
size_t aurora_leak_count(void) {
    return g_state.count.load();
}

size_t aurora_leak_bytes(void) {
    ensure_init();
    state_lock();
    size_t bytes = 0;
    for (size_t i = 0; i < g_state.capacity; i++) {
        if (g_state.records[i].ptr) bytes += g_state.records[i].size;
    }
    state_unlock();
    return bytes;
}

uint64_t aurora_leak_total_allocated(void) {
    return g_state.total_alloc.load();
}

uint64_t aurora_leak_total_freed(void) {
    return g_state.total_free.load();
}

void aurora_leak_set_min_size(size_t bytes) {
    g_state.min_size = bytes;
}

void aurora_leak_set_max_records(size_t max) {
    /* Not implemented — uses fixed array */
    (void)max;
}

static void auto_report_fn(void) {
    if (g_state.enabled.load()) {
        fprintf(stderr, "[leak] exit-time report:\n");
        aurora_leak_report();
    }
}

void aurora_leak_auto_report(void) {
    ensure_init();
    if (!g_state.auto_report_registered) {
        atexit(auto_report_fn);
        g_state.auto_report_registered = 1;
    }
}

/* ── Shutdown ── */
void aurora_leak_shutdown(void) {
    if (!g_state.initialized.load()) return;
    g_state.enabled.store(0);
    state_lock();
    free(g_state.records);
    g_state.records = nullptr;
    g_state.capacity = 0;
    g_state.count.store(0);
    state_unlock();
    g_state.initialized.store(0);
}
