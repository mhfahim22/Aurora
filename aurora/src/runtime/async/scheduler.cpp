/* ── Aurora Runtime — Thread Pool Scheduler (Phase 28: Work-Stealing) ── */
#include <cstdint>
#include <cstdlib>
#include <thread>
#include <vector>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>

#include "runtime/async.hpp"

/* ════════════════════════════════════════════════════════════
   Work-stealing scheduler
   ════════════════════════════════════════════════════════════ */

struct WorkerQueue {
    std::deque<AuroraTask*> tasks;
    std::mutex              mtx;
};

struct Scheduler {
    std::vector<std::thread> workers;
    std::vector<std::unique_ptr<WorkerQueue>> queues;
    std::atomic<bool>        running{false};
};

static Scheduler g_sched;

static void worker_loop(int id) {
    while (true) {
        AuroraTask* task = nullptr;

        /* Try to pop from own queue first */
        {
            std::lock_guard<std::mutex> lock(g_sched.queues[id]->mtx);
            if (!g_sched.queues[id]->tasks.empty()) {
                task = g_sched.queues[id]->tasks.front();
                g_sched.queues[id]->tasks.pop_front();
            }
        }

        /* Steal from others if own queue was empty */
        if (!task) {
            int n = (int)g_sched.queues.size();
            for (int i = 1; i < n; i++) {
                int victim = (id + i) % n;
                std::lock_guard<std::mutex> lock(g_sched.queues[victim]->mtx);
                if (!g_sched.queues[victim]->tasks.empty()) {
                    task = g_sched.queues[victim]->tasks.back();
                    g_sched.queues[victim]->tasks.pop_back();
                    break;
                }
            }
        }

        if (task) {
            if (task->func) {
                void* result = task->func(task->arg);
                aurora_task_set_result(task, result);
            }
            continue;
        }

        /* No tasks anywhere — check shutdown */
        if (!g_sched.running.load(std::memory_order_acquire)) {
            return;
        }

        /* Spin a bit before yielding (Phase 28: busy-spin for low latency) */
        for (volatile int spin = 0; spin < 100; spin++) {
            if (!g_sched.running.load(std::memory_order_acquire)) return;
            {
                std::lock_guard<std::mutex> lock(g_sched.queues[id]->mtx);
                if (!g_sched.queues[id]->tasks.empty()) goto retry;
            }
        }

        /* Yield to OS */
        std::this_thread::yield();
    retry:;
    }
}

extern "C" {

void aurora_scheduler_init(int32_t thread_count) {
    if (g_sched.running.load(std::memory_order_acquire)) return;
    if (thread_count <= 0) thread_count = 1;

    g_sched.queues.reserve(thread_count);
    for (int32_t i = 0; i < thread_count; i++) {
        g_sched.queues.push_back(std::make_unique<WorkerQueue>());
    }
    g_sched.running.store(true, std::memory_order_release);

    for (int32_t i = 0; i < thread_count; i++) {
        g_sched.workers.emplace_back(worker_loop, i);
    }
}

void aurora_scheduler_shutdown() {
    if (!g_sched.running.load(std::memory_order_acquire)) return;
    g_sched.running.store(false, std::memory_order_release);

    for (auto& w : g_sched.workers) {
        if (w.joinable()) w.join();
    }
    g_sched.workers.clear();
    g_sched.queues.clear();
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

    /* Round-robin add to a worker queue */
    static std::atomic<int> next_q{0};
    int q = next_q.fetch_add(1, std::memory_order_relaxed) % (int)g_sched.queues.size();
    {
        std::lock_guard<std::mutex> lock(g_sched.queues[q]->mtx);
        g_sched.queues[q]->tasks.push_back(task);
    }
}

void aurora_wait(AuroraTask* task) {
    if (!task) return;
    std::unique_lock<std::mutex> lock(task->mtx);
    task->cv.wait(lock, [task] {
        return task->done.load(std::memory_order_acquire);
    });
}

} // extern "C"
