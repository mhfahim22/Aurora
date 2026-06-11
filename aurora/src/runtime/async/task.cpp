#include <cstdint>
#include <cstdlib>
#include <new>

#include "runtime/async.hpp"

/* ════════════════════════════════════════════════════════════
   Aurora Runtime — Async Task
   ════════════════════════════════════════════════════════════ */

extern "C" {

AuroraTask* aurora_task_create(void* (*func)(void*), void* arg) {
    void* mem = std::malloc(sizeof(AuroraTask));
    if (!mem) return nullptr;
    AuroraTask* task = ::new (mem) AuroraTask;
    task->func   = func;
    task->arg    = arg;
    task->result = nullptr;
    task->done.store(false, std::memory_order_release);
    return task;
}

void aurora_task_destroy(AuroraTask* task) {
    if (!task) return;
    task->~AuroraTask();
    std::free(task);
}

int32_t aurora_task_is_done(AuroraTask* task) {
    if (!task) return 1;
    return task->done.load(std::memory_order_acquire) ? 1 : 0;
}

void* aurora_task_get_result(AuroraTask* task) {
    if (!task) return nullptr;
    return task->result;
}

void aurora_task_set_result(AuroraTask* task, void* result) {
    if (!task) return;
    {
        std::lock_guard<std::mutex> lock(task->mtx);
        task->result = result;
        task->done.store(true, std::memory_order_release);
    }
    task->cv.notify_one();
}

}
