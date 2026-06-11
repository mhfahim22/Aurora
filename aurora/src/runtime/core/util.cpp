#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <chrono>

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
static int random_seeded = 0;

int64_t aurora_random() {
    if (!random_seeded) {
        srand((unsigned)time(nullptr));
        random_seeded = 1;
    }
    return (int64_t)rand();
}

/* ── Yield placeholder — logs yield event ── */
void aurora_yield(int64_t val) {
    (void)val;
    /* In a full implementation, this would suspend the current
       coroutine/generator and resume later with the value. */
}

}
