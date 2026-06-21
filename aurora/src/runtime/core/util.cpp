#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <chrono>
#include <random>
#include "runtime/async.hpp"

/* ── Thread-local yield value for generator/coroutine support ── */
static thread_local int64_t tls_yield_value = 0;

static thread_local std::mt19937 g_rng(std::random_device{}());

extern "C" {

/* ── Sleep for N milliseconds ── */
void aurora_sleep(int64_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

/* ── Get current time in seconds since epoch ── */
int64_t aurora_time() {
    return (int64_t)time(nullptr);
}

/* ── Get random integer between 0 and RAND_MAX ── */
int64_t aurora_random() {
    return (int64_t)g_rng();
}

/* ── Yield — suspends current generator/coroutine with value ── */
void aurora_yield(int64_t val) {
    tls_yield_value = val;
    AuroraFiber* cur = aurora_fiber_current();
    if (cur) {
        aurora_fiber_yield();
    }
}

/* ── Get the last yielded value ── */
int64_t aurora_yielded_value(void) {
    return tls_yield_value;
}

}
