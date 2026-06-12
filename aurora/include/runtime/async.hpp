#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <csetjmp>

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

/* ── Channel (async bounded queue) ── */
typedef struct AuroraChannel {
    std::mutex              mtx;
    std::condition_variable cv;
    void**                  buf;        /* circular buffer */
    int32_t                 capacity;
    int32_t                 head;
    int32_t                 tail;
    int32_t                 count;
} AuroraChannel;

/* Create a channel with given capacity */
AuroraChannel* aurora_chan_create(int32_t capacity);

/* Destroy a channel */
void aurora_chan_destroy(AuroraChannel* ch);

/* Send a value (blocks if full) */
void aurora_chan_send(AuroraChannel* ch, void* val);

/* Receive a value (blocks if empty) */
void* aurora_chan_recv(AuroraChannel* ch);

/* Try send (non-blocking) — returns 1 on success */
int32_t aurora_chan_try_send(AuroraChannel* ch, void* val);

/* Try recv (non-blocking) — returns value or null */
void* aurora_chan_try_recv(AuroraChannel* ch);

/* ── Event Bus (Signal/emit runtime) ── */

/* Initialize the global event bus */
void aurora_event_bus_init(void);

/* Register a handler for an event name */
void aurora_event_on(const char* event_name, void (*handler)(void*), void* user_data);

/* Unregister a handler for an event name */
void aurora_event_off(const char* event_name, void (*handler)(void*));

/* Trigger an event — calls all registered handlers */
void aurora_event_emit(const char* event_name, void* arg);

/* Shutdown the event bus and clean up */
void aurora_event_bus_shutdown(void);

/* ── Fiber (cooperative task) ── */
typedef struct AuroraFiber {
    void*             (*func)(void*);
    void*              arg;
    void*              result;
    int                state;       /* 0=ready, 1=running, 2=yielded, 3=done */
    void*              context;     /* platform-specific fiber handle / jmp_buf */
    struct AuroraFiber* next;       /* for scheduler queue */
} AuroraFiber;

/* Create a fiber */
AuroraFiber* aurora_fiber_create(void* (*func)(void*), void* arg);

/* Destroy a fiber */
void aurora_fiber_destroy(AuroraFiber* fiber);

/* Yield — return control to the scheduler */
void aurora_fiber_yield(void);

/* Resume a fiber (must be in yielded state) */
void aurora_fiber_resume(AuroraFiber* fiber);

/* Check if fiber is done */
int32_t aurora_fiber_is_done(AuroraFiber* fiber);

/* Get fiber result */
void* aurora_fiber_get_result(AuroraFiber* fiber);

/* Get current fiber (null if not in fiber context) */
AuroraFiber* aurora_fiber_current(void);

}
