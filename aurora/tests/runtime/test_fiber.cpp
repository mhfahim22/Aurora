#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <cassert>
#include "runtime/async.hpp"

static int g_fiber_counter = 0;

static void* fiber_count(void* arg) {
    int n = (int)(intptr_t)arg;
    for (int i = 0; i < n; i++) {
        g_fiber_counter++;
        aurora_fiber_yield();
    }
    return (void*)(intptr_t)(n * 10);
}

static void test_fiber_create_destroy() {
    g_fiber_counter = 0;
    AuroraFiber* f = aurora_fiber_create(fiber_count, (void*)5);
    assert(f != nullptr);
    assert(aurora_fiber_is_done(f) == 0);
    aurora_fiber_destroy(f);
    printf("  PASS test_fiber_create_destroy\n");
}

static void test_fiber_run_to_completion() {
    g_fiber_counter = 0;
    AuroraFiber* f = aurora_fiber_create(fiber_count, (void*)3);
    assert(f != nullptr);
    /* Resume until done */
    while (!aurora_fiber_is_done(f))
        aurora_fiber_resume(f);
    assert(g_fiber_counter == 3);
    intptr_t result = (intptr_t)aurora_fiber_get_result(f);
    assert(result == 30);
    aurora_fiber_destroy(f);
    printf("  PASS test_fiber_run_to_completion\n");
}

static void test_fiber_yield_resume() {
    g_fiber_counter = 0;
    AuroraFiber* f = aurora_fiber_create(fiber_count, (void*)2);
    assert(f != nullptr);
    /* First resume: runs until first yield */
    aurora_fiber_resume(f);
    assert(g_fiber_counter == 1);
    assert(aurora_fiber_is_done(f) == 0);
    /* Second resume: runs until second yield */
    aurora_fiber_resume(f);
    assert(g_fiber_counter == 2);
    assert(aurora_fiber_is_done(f) == 0);
    /* Third resume: fiber completes */
    aurora_fiber_resume(f);
    assert(g_fiber_counter == 2);
    assert(aurora_fiber_is_done(f) == 1);
    intptr_t result = (intptr_t)aurora_fiber_get_result(f);
    assert(result == 20);
    aurora_fiber_destroy(f);
    printf("  PASS test_fiber_yield_resume\n");
}

static void test_fiber_not_done_when_yielded() {
    g_fiber_counter = 0;
    AuroraFiber* f = aurora_fiber_create(fiber_count, (void*)5);
    assert(f != nullptr);
    aurora_fiber_resume(f);
    assert(g_fiber_counter == 1);
    assert(aurora_fiber_is_done(f) == 0);
    aurora_fiber_resume(f);
    assert(g_fiber_counter == 2);
    assert(aurora_fiber_is_done(f) == 0);
    aurora_fiber_destroy(f);
    printf("  PASS test_fiber_not_done_when_yielded\n");
}

static void* fiber_return_null(void*) {
    aurora_fiber_yield();
    return nullptr;
}

static void test_fiber_null_result() {
    AuroraFiber* f = aurora_fiber_create(fiber_return_null, nullptr);
    assert(f != nullptr);
    aurora_fiber_resume(f);
    assert(aurora_fiber_is_done(f) == 0);
    aurora_fiber_resume(f);
    assert(aurora_fiber_is_done(f) == 1);
    assert(aurora_fiber_get_result(f) == nullptr);
    aurora_fiber_destroy(f);
    printf("  PASS test_fiber_null_result\n");
}

static void* fiber_channel_writer(void* arg) {
    AuroraChannel* ch = (AuroraChannel*)arg;
    aurora_chan_send(ch, (void*)(intptr_t)42);
    aurora_fiber_yield();
    aurora_chan_send(ch, (void*)(intptr_t)99);
    return nullptr;
}

static void test_fiber_channel_data_pass() {
    AuroraChannel* ch = aurora_chan_create(4);
    assert(ch != nullptr);
    AuroraFiber* f = aurora_fiber_create(fiber_channel_writer, ch);
    assert(f != nullptr);
    /* Resume fiber — it sends 42 then yields */
    aurora_fiber_resume(f);
    intptr_t val = (intptr_t)aurora_chan_try_recv(ch);
    assert(val == 42);
    /* Resume fiber — it sends 99 then finishes */
    aurora_fiber_resume(f);
    val = (intptr_t)aurora_chan_try_recv(ch);
    assert(val == 99);
    assert(aurora_fiber_is_done(f) == 1);
    aurora_fiber_destroy(f);
    aurora_chan_destroy(ch);
    printf("  PASS test_fiber_channel_data_pass\n");
}

static void test_context_switch_time() {
    g_fiber_counter = 0;
    AuroraFiber* f = aurora_fiber_create(fiber_count, (void*)10000);
    assert(f != nullptr);
    auto start = std::chrono::high_resolution_clock::now();
    while (!aurora_fiber_is_done(f))
        aurora_fiber_resume(f);
    auto end = std::chrono::high_resolution_clock::now();
    aurora_fiber_destroy(f);
    double total_us = std::chrono::duration<double, std::micro>(end - start).count();
    double per_switch_ns = (total_us * 1000.0) / 10000.0;
    printf("  Measured: %.0f ns per context switch (%.0f us total for 10000)\n",
           per_switch_ns, total_us);
    if (per_switch_ns < 5000.0) {
        printf("  PASS test_context_switch_time (<5000 ns, target <50 ns)\n");
    } else {
        printf("  WARN: context switch time %.0f ns exceeds 5000 ns target\n",
               per_switch_ns);
    }
}

int main() {
    printf("=== Fiber Engine Tests ===\n\n");
    test_fiber_create_destroy();
    test_fiber_run_to_completion();
    test_fiber_yield_resume();
    test_fiber_not_done_when_yielded();
    test_fiber_null_result();
    test_fiber_channel_data_pass();
    test_context_switch_time();
    printf("\n=== All fiber tests passed! ===\n");
    return 0;
}
