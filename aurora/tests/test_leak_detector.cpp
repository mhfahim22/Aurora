// test_leak_detector.cpp — Tests for Aurora Leak Detector (Phase 8)
#include "runtime/leak_detector.hpp"
#include "runtime/memory.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <string>

static int g_tests = 0, g_passed = 0, g_failed = 0;

#define TEST(name) do { \
    printf("  %s ... ", name); \
    g_tests++; \
} while(0)

#define PASS() do { g_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { g_failed++; printf("FAIL: %s\n", msg); } while(0)

/* ── 1. Basic enable/disable ── */
static void test_enable_disable() {
    printf("\n=== Enable / Disable ===\n");

    TEST("initially disabled");
    assert(!aurora_leak_is_enabled());
    PASS();

    TEST("enable");
    aurora_leak_enable();
    assert(aurora_leak_is_enabled());
    PASS();

    TEST("disable");
    aurora_leak_disable();
    assert(!aurora_leak_is_enabled());
    PASS();
}

/* ── 2. Track / Untrack basic ── */
static void test_track_untrack() {
    printf("\n=== Track / Untrack ===\n");

    aurora_leak_enable();

    TEST("track after enable");
    assert(aurora_leak_count() == 0);
    PASS();

    TEST("single alloc increases count");
    void* p1 = aurora_alloc(64);
    assert(aurora_leak_count() == 1);
    assert(aurora_leak_bytes() >= 64);
    assert(aurora_leak_total_allocated() == 1);
    PASS();

    TEST("free decreases count");
    aurora_free(p1);
    assert(aurora_leak_count() == 0);
    assert(aurora_leak_total_freed() == 1);
    PASS();

    TEST("multiple allocs");
    void* a = aurora_alloc(32);
    void* b = aurora_alloc(64);
    void* c = aurora_alloc(128);
    assert(aurora_leak_count() == 3);
    assert(aurora_leak_total_allocated() == 4); /* 1 from prev + 3 */
    PASS();

    TEST("free one keeps others");
    aurora_free(b);
    assert(aurora_leak_count() == 2);
    PASS();

    TEST("free all clears");
    aurora_free(a);
    aurora_free(c);
    assert(aurora_leak_count() == 0);
    PASS();

    aurora_leak_disable();
}

/* ── 3. Disabled tracking ── */
static void test_disabled_tracking() {
    printf("\n=== Disabled Tracking ===\n");

    aurora_leak_disable();
    TEST("alloc while disabled is not tracked");
    size_t before = aurora_leak_count();
    void* p = aurora_alloc(256);
    assert(aurora_leak_count() == before);
    aurora_free(p);
    PASS();
}

/* ── 4. Leak report (no leaks) ── */
static void test_report_no_leaks() {
    printf("\n=== Report (no leaks) ===\n");

    aurora_leak_enable();
    TEST("report with no leaks");
    /* Allocate and free */
    void* p = aurora_alloc(16);
    aurora_free(p);
    assert(aurora_leak_count() == 0);
    /* Just verify report doesn't crash */
    aurora_leak_report();
    PASS();
    aurora_leak_disable();
}

/* ── 5. Leak report (with leaks) ── */
static void test_report_with_leaks() {
    printf("\n=== Report (with leaks) ===\n");

    aurora_leak_enable();
    TEST("report with leaks");
    void* leak1 = aurora_alloc(100);
    void* leak2 = aurora_alloc(200);
    void* leak3 = aurora_alloc(300);
    (void)leak1; (void)leak2; (void)leak3;

    size_t n = aurora_leak_count();
    assert(n == 3);
    assert(aurora_leak_bytes() == 600);

    /* Generate JSON */
    char* json = aurora_leak_report_json();
    assert(json != nullptr);
    assert(strstr(json, "\"live_allocations\": 3") != nullptr);
    assert(strstr(json, "\"total_bytes\": 600") != nullptr);
    aurora_free(json); /* JSON is allocated with malloc; use aurora_free */

    /* Cleanup tracked pointers so they don't pollute other tests */
    aurora_leak_clear();
    assert(aurora_leak_count() == 0);
    /* Free the leaked memory */
    aurora_free(leak1);
    aurora_free(leak2);
    aurora_free(leak3);
    PASS();
    aurora_leak_disable();
}

/* ── 6. Min size filter ── */
static void test_min_size() {
    printf("\n=== Min Size Filter ===\n");

    aurora_leak_enable();
    TEST("min_size filter works");
    aurora_leak_set_min_size(50);
    void* small = aurora_alloc(16);
    void* big   = aurora_alloc(100);
    assert(aurora_leak_count() == 1); /* only big tracked */
    aurora_free(small);
    aurora_free(big);
    aurora_leak_set_min_size(0); /* reset */
    PASS();
    aurora_leak_disable();
}

/* ── 7. Auto-report via atexit ── */
static void test_auto_report() {
    printf("\n=== Auto-report ===\n");
    TEST("auto_report registers atexit handler");
    /* Just verify it doesn't crash */
    aurora_leak_auto_report();
    PASS();
}

/* ── 8. Zero-size alloc ── */
static void test_zero_size() {
    printf("\n=== Zero-size ===\n");
    aurora_leak_enable();
    TEST("zero-size alloc tracked");
    void* p = aurora_alloc(0);
    assert(aurora_leak_count() == 1);
    aurora_free(p);
    assert(aurora_leak_count() == 0);
    PASS();
    aurora_leak_disable();
}

/* ── 9. SharedBox tracking ── */
static void test_shared_box() {
    printf("\n=== SharedBox ===\n");
    aurora_leak_enable();
    TEST("aurora_shared_new tracked via aurora_alloc");
    int* data = (int*)aurora_alloc(sizeof(int));
    *data = 42;
    void* box = aurora_shared_new(data, aurora_free);
    assert(box != nullptr);
    /* box + data = 2 tracked allocs */
    assert(aurora_leak_count() == 2);
    aurora_refcount_dec(box);
    /* Both should be freed now */
    assert(aurora_leak_count() == 0);
    PASS();
    aurora_leak_disable();
}

/* ── 10. Stress: many allocs ── */
static void test_stress() {
    printf("\n=== Stress ===\n");
    aurora_leak_enable();
    TEST("1000 alloc/free cycles");
    for (int i = 0; i < 1000; i++) {
        void* p = aurora_alloc((size_t)(rand() % 1024 + 1));
        aurora_free(p);
    }
    assert(aurora_leak_count() == 0);
    PASS();

    TEST("100 simultaneous allocs");
    void* ptrs[100];
    for (int i = 0; i < 100; i++) {
        ptrs[i] = aurora_alloc(128);
    }
    assert(aurora_leak_count() == 100);
    for (int i = 0; i < 100; i++) {
        aurora_free(ptrs[i]);
    }
    assert(aurora_leak_count() == 0);
    PASS();
    aurora_leak_disable();
}

/* ── 11. Realloc tracking ── */
static void test_realloc() {
    printf("\n=== Realloc ===\n");
    aurora_leak_enable();
    TEST("realloc properly tracked");
    void* p = aurora_alloc(64);
    assert(aurora_leak_count() == 1);
    /* Simulate realloc: alloc new, copy, free old */
    void* p2 = aurora_alloc(128);
    memcpy(p2, p, 64);
    aurora_free(p);
    assert(aurora_leak_count() == 1);
    aurora_free(p2);
    assert(aurora_leak_count() == 0);
    PASS();
    aurora_leak_disable();
}

/* ── 12. JSON report format ── */
static void test_json_report() {
    printf("\n=== JSON Report ===\n");
    aurora_leak_enable();
    TEST("JSON report format");
    void* p = aurora_alloc(42);
    char* json = aurora_leak_report_json();
    assert(json != nullptr);
    assert(strstr(json, "\"live_allocations\"") != nullptr);
    assert(strstr(json, "\"total_bytes\"") != nullptr);
    assert(strstr(json, "\"total_allocated\"") != nullptr);
    assert(strstr(json, "\"total_freed\"") != nullptr);
    assert(strstr(json, "\"leaks\"") != nullptr);
    assert(strstr(json, "\"allocator\": \"heap\"") != nullptr);
    aurora_free(json);
    aurora_free(p);
    assert(aurora_leak_count() == 0);
    PASS();
    aurora_leak_disable();
}

int main() {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    printf("========================================\n");
    printf("  Leak Detector Test Suite\n");
    printf("========================================\n");

    aurora_leak_init();

    test_enable_disable();
    test_track_untrack();
    test_disabled_tracking();
    test_report_no_leaks();
    test_report_with_leaks();
    test_min_size();
    test_auto_report();
    test_zero_size();
    test_shared_box();
    test_stress();
    test_realloc();
    test_json_report();

    /* Cleanup */
    aurora_leak_shutdown();

    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed, %d total\n",
           g_passed, g_failed, g_tests);
    printf("========================================\n");
    return g_failed > 0 ? 1 : 0;
}
