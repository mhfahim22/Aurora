#include <cstdint>
#include <cstdlib>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "runtime/async.hpp"

/* ════════════════════════════════════════════════════════════
   Aurora Runtime — Thread Pool Scheduler
   ════════════════════════════════════════════════════════════ */

struct Scheduler {
    std::vector<std::thread> workers;
    std::queue<AuroraTask*>  tasks;
    std::mutex               mtx;
    std::condition_variable  cv;
    std::atomic<bool>        running{false};
};

static Scheduler g_sched;

static void worker_loop() {
    while (true) {
        AuroraTask* task = nullptr;
        {
            std::unique_lock<std::mutex> lock(g_sched.mtx);
            g_sched.cv.wait(lock, [] {
                return !g_sched.running.load(std::memory_order_acquire) ||
                       !g_sched.tasks.empty();
            });
            if (!g_sched.running.load(std::memory_order_acquire) &&
                g_sched.tasks.empty()) {
                return;
            }
            task = g_sched.tasks.front();
            g_sched.tasks.pop();
        }
        if (task && task->func) {
            void* result = task->func(task->arg);
            aurora_task_set_result(task, result);
        }
    }
}

extern "C" {

void aurora_scheduler_init(int32_t thread_count) {
    if (g_sched.running.load(std::memory_order_acquire)) return;
    if (thread_count <= 0) thread_count = 1;

    g_sched.running.store(true, std::memory_order_release);

    for (int32_t i = 0; i < thread_count; i++) {
        g_sched.workers.emplace_back(worker_loop);
    }

    std::atexit(aurora_scheduler_shutdown);
}

void aurora_scheduler_shutdown() {
    if (!g_sched.running.load(std::memory_order_acquire)) return;
    {
        std::lock_guard<std::mutex> lock(g_sched.mtx);
        g_sched.running.store(false, std::memory_order_release);
    }
    g_sched.cv.notify_all();

    for (auto& w : g_sched.workers) {
        if (w.joinable()) w.join();
    }
    g_sched.workers.clear();
}

int32_t aurora_scheduler_default_threads() {
    int32_t count = (int32_t)std::thread::hardware_concurrency();
    return count > 0 ? count : 4;
}

void aurora_spawn(AuroraTask* task) {
    if (!task) return;
    static bool init_once = false;
    if (!init_once) {
        aurora_scheduler_init(aurora_scheduler_default_threads());
        init_once = true;
    }
    {
        std::lock_guard<std::mutex> lock(g_sched.mtx);
        g_sched.tasks.push(task);
    }
    g_sched.cv.notify_one();
}

void aurora_wait(AuroraTask* task) {
    if (!task) return;
    std::unique_lock<std::mutex> lock(task->mtx);
    task->cv.wait(lock, [task] {
        return task->done.load(std::memory_order_acquire);
    });
}

} // extern "C"
