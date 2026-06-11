#pragma once
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <mutex>
#include <condition_variable>

extern "C" {

/* ── Task handle ── */
typedef struct AuroraTask {
    void*            (*func)(void*);
    void*             arg;
    void*             result;
    std::atomic<bool> done;
    std::mutex        mtx;
    std::condition_variable cv;
} AuroraTask;

/* Create a new task */
AuroraTask* aurora_task_create(void* (*func)(void*), void* arg);

/* Destroy a task (must not be in-flight) */
void aurora_task_destroy(AuroraTask* task);

/* Check if task is done */
int32_t aurora_task_is_done(AuroraTask* task);

/* Get task result (does not wait) */
void* aurora_task_get_result(AuroraTask* task);

/* Set task result (called by worker) */
void aurora_task_set_result(AuroraTask* task, void* result);

/* ── Scheduler / Thread Pool ── */

/* Initialize the global thread pool with N workers */
void aurora_scheduler_init(int32_t thread_count);

/* Shutdown the global thread pool (drains all pending tasks) */
void aurora_scheduler_shutdown(void);

/* Get the default thread count (hardware concurrency) */
int32_t aurora_scheduler_default_threads(void);

/* Spawn a task onto the global thread pool */
void aurora_spawn(AuroraTask* task);

/* Wait for a task to complete (blocks caller) */
void aurora_wait(AuroraTask* task);

}
