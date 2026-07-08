#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* Event flags */
#define AURORA_EV_READ  1
#define AURORA_EV_WRITE 2

/* Callback: userdata, fd, events */
typedef void (*AuroraEventCallback)(void* userdata, int fd, int events);

/* Opaque event loop handle */
typedef struct AuroraEventLoop AuroraEventLoop;

AuroraEventLoop* aurora_event_loop_new(int capacity);
void             aurora_event_loop_free(AuroraEventLoop* loop);
int              aurora_event_loop_add(AuroraEventLoop* loop, int fd, int events,
                                       AuroraEventCallback cb, void* userdata);
int              aurora_event_loop_remove(AuroraEventLoop* loop, int fd);
int              aurora_event_loop_wait(AuroraEventLoop* loop, int timeout_ms);
void             aurora_event_loop_stop(AuroraEventLoop* loop);
int              aurora_event_loop_is_running(AuroraEventLoop* loop);

/* ── Worker thread pool (used by server) ── */
typedef struct AuroraWorkerPool AuroraWorkerPool;

AuroraWorkerPool* aurora_worker_pool_new(int thread_count);
void              aurora_worker_pool_free(AuroraWorkerPool* pool);
int               aurora_worker_pool_enqueue(AuroraWorkerPool* pool,
                                             void (*task)(void*), void* arg);
int               aurora_worker_pool_busy_count(AuroraWorkerPool* pool);
int               aurora_worker_pool_wait_idle(AuroraWorkerPool* pool, int timeout_ms);

#ifdef __cplusplus
}
#endif
