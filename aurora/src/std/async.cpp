#include <cstdint>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <queue>
#include <algorithm>

#include "std/async.hpp"

/* ════════════════════════════════════════════════════════════
   Aurora Runtime — Phase 4: Async Runtime
   Event loop with timers, idle callbacks, and dispatcher
   ════════════════════════════════════════════════════════════ */

template<typename T>
static inline int64_t make_handle(T* ptr) {
    return reinterpret_cast<int64_t>(ptr);
}
template<typename T>
static inline T* from_handle(int64_t h) {
    return reinterpret_cast<T*>(h);
}

using TimePoint = std::chrono::steady_clock::time_point;

struct AsyncTimer {
    int64_t id;
    TimePoint expiry;
    int32_t interval_ms;
    bool repeat;
    void (*callback)(void*);
    void* arg;
};

struct AsyncIdle {
    int64_t id;
    void (*callback)(void*);
    void* arg;
};

struct AsyncDispatch {
    void (*callback)(void*);
    void* arg;
};

struct AsyncLoop {
    std::mutex mtx;
    std::condition_variable cv;
    bool running;
    int64_t next_timer_id;
    int64_t next_idle_id;
    std::vector<AsyncTimer> timers;
    std::vector<AsyncIdle> idle_callbacks;
    std::queue<AsyncDispatch> dispatch_queue;
};

extern "C" {

/* ── Event Loop ── */

int64_t aurora_async_create_loop(void) {
    auto* loop = new AsyncLoop();
    loop->running = false;
    loop->next_timer_id = 1;
    loop->next_idle_id = 1;
    return make_handle(loop);
}

void aurora_async_destroy_loop(int64_t handle) {
    delete from_handle<AsyncLoop>(handle);
}

void aurora_async_run(int64_t handle) {
    auto* loop = from_handle<AsyncLoop>(handle);
    {
        std::lock_guard<std::mutex> lock(loop->mtx);
        loop->running = true;
    }
    while (true) {
        {
            std::lock_guard<std::mutex> lock(loop->mtx);
            if (!loop->running) break;
        }
        aurora_async_run_once(handle);
    }
}

void aurora_async_run_once(int64_t handle) {
    auto* loop = from_handle<AsyncLoop>(handle);
    auto now = std::chrono::steady_clock::now();
    bool did_work = false;

    /* Fire expired timers */
    {
        std::lock_guard<std::mutex> lock(loop->mtx);
        if (!loop->running) return;

        for (size_t i = 0; i < loop->timers.size(); ) {
            auto& t = loop->timers[i];
            if (t.expiry <= now) {
                did_work = true;
                if (t.callback) t.callback(t.arg);
                if (t.repeat) {
                    t.expiry = now + std::chrono::milliseconds(t.interval_ms);
                    i++;
                } else {
                    std::swap(loop->timers[i], loop->timers.back());
                    loop->timers.pop_back();
                }
            } else {
                i++;
            }
        }
    }

    /* Process dispatch queue */
    {
        std::lock_guard<std::mutex> lock(loop->mtx);
        if (!loop->running) return;

        while (!loop->dispatch_queue.empty()) {
            did_work = true;
            auto item = loop->dispatch_queue.front();
            loop->dispatch_queue.pop();
            if (item.callback) item.callback(item.arg);
        }
    }

    /* Run idle callbacks (only if no other work) */
    if (!did_work) {
        {
            std::lock_guard<std::mutex> lock(loop->mtx);
            if (!loop->running) return;

            for (auto& idle : loop->idle_callbacks) {
                if (idle.callback) idle.callback(idle.arg);
            }
        }

        /* Brief sleep to avoid busy-wait when idle */
        std::unique_lock<std::mutex> lock(loop->mtx);
        loop->cv.wait_for(lock, std::chrono::milliseconds(10));
    }
}

void aurora_async_stop(int64_t handle) {
    auto* loop = from_handle<AsyncLoop>(handle);
    {
        std::lock_guard<std::mutex> lock(loop->mtx);
        loop->running = false;
    }
    loop->cv.notify_all();
}

/* ── Timers ── */

int64_t aurora_async_set_timeout(int64_t handle, int32_t delay_ms,
                                  void (*callback)(void*), void* arg) {
    auto* loop = from_handle<AsyncLoop>(handle);
    std::lock_guard<std::mutex> lock(loop->mtx);
    AsyncTimer t;
    t.id = loop->next_timer_id++;
    t.expiry = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
    t.interval_ms = 0;
    t.repeat = false;
    t.callback = callback;
    t.arg = arg;
    loop->timers.push_back(t);
    loop->cv.notify_one();
    return t.id;
}

int64_t aurora_async_set_interval(int64_t handle, int32_t interval_ms,
                                   void (*callback)(void*), void* arg) {
    auto* loop = from_handle<AsyncLoop>(handle);
    std::lock_guard<std::mutex> lock(loop->mtx);
    AsyncTimer t;
    t.id = loop->next_timer_id++;
    t.expiry = std::chrono::steady_clock::now() + std::chrono::milliseconds(interval_ms);
    t.interval_ms = interval_ms;
    t.repeat = true;
    t.callback = callback;
    t.arg = arg;
    loop->timers.push_back(t);
    loop->cv.notify_one();
    return t.id;
}

void aurora_async_clear_timer(int64_t handle, int64_t timer_id) {
    auto* loop = from_handle<AsyncLoop>(handle);
    std::lock_guard<std::mutex> lock(loop->mtx);
    for (size_t i = 0; i < loop->timers.size(); i++) {
        if (loop->timers[i].id == timer_id) {
            std::swap(loop->timers[i], loop->timers.back());
            loop->timers.pop_back();
            return;
        }
    }
}

/* ── Idle Callbacks ── */

int64_t aurora_async_idle(int64_t handle, void (*callback)(void*), void* arg) {
    auto* loop = from_handle<AsyncLoop>(handle);
    std::lock_guard<std::mutex> lock(loop->mtx);
    AsyncIdle idle;
    idle.id = loop->next_idle_id++;
    idle.callback = callback;
    idle.arg = arg;
    loop->idle_callbacks.push_back(idle);
    return idle.id;
}

void aurora_async_remove_idle(int64_t handle, int64_t idle_id) {
    auto* loop = from_handle<AsyncLoop>(handle);
    std::lock_guard<std::mutex> lock(loop->mtx);
    for (size_t i = 0; i < loop->idle_callbacks.size(); i++) {
        if (loop->idle_callbacks[i].id == idle_id) {
            std::swap(loop->idle_callbacks[i], loop->idle_callbacks.back());
            loop->idle_callbacks.pop_back();
            return;
        }
    }
}

/* ── Dispatcher ── */

void aurora_async_dispatch(int64_t handle, void (*callback)(void*), void* arg) {
    auto* loop = from_handle<AsyncLoop>(handle);
    {
        std::lock_guard<std::mutex> lock(loop->mtx);
        AsyncDispatch item;
        item.callback = callback;
        item.arg = arg;
        loop->dispatch_queue.push(item);
    }
    loop->cv.notify_one();
}

} // extern "C"
