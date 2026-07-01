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

/* ── Get high-resolution time in nanoseconds ── */
int64_t aurora_time_ns() {
    return (int64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

/* ── Format time (epoch secs) as string via strftime ── */
int aurora_time_format(int64_t t, const char* fmt, char* buf, int size) {
    time_t tt = (time_t)t;
    struct tm local;
#ifdef _WIN32
    if (localtime_s(&local, &tt) != 0) return 0;
#else
    if (!localtime_r(&tt, &local)) return 0;
#endif
    return (int)strftime(buf, (size_t)size, fmt, &local);
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
