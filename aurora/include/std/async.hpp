#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Event Loop ── */
int64_t aurora_async_create_loop(void);
void    aurora_async_destroy_loop(int64_t loop);
void    aurora_async_run(int64_t loop);
void    aurora_async_run_once(int64_t loop);
void    aurora_async_stop(int64_t loop);

/* ── Timers (setTimeout / setInterval) ── */
int64_t aurora_async_set_timeout(int64_t loop, int32_t delay_ms,
                                  void (*callback)(void*), void* arg);
int64_t aurora_async_set_interval(int64_t loop, int32_t interval_ms,
                                   void (*callback)(void*), void* arg);
void    aurora_async_clear_timer(int64_t loop, int64_t timer_id);

/* ── Idle Callbacks ── */
int64_t aurora_async_idle(int64_t loop, void (*callback)(void*), void* arg);
void    aurora_async_remove_idle(int64_t loop, int64_t idle_id);

/* ── Dispatcher (cross-thread safe) ── */
void    aurora_async_dispatch(int64_t loop, void (*callback)(void*), void* arg);

#ifdef __cplusplus
}
#endif
