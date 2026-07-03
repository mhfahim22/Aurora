#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Thread ── */
int64_t aurora_thread_create(void* (*func)(void*), void* arg);
void    aurora_thread_join(int64_t handle);
void    aurora_thread_detach(int64_t handle);
int64_t aurora_thread_self(void);
void    aurora_thread_sleep(int32_t ms);

/* ── Mutex ── */
int64_t aurora_mutex_create(void);
void    aurora_mutex_destroy(int64_t handle);
void    aurora_mutex_lock(int64_t handle);
void    aurora_mutex_unlock(int64_t handle);
int32_t aurora_mutex_try_lock(int64_t handle);

/* ── LockGuard (RAII) ── */
int64_t aurora_lock_guard_create(int64_t mutex_handle);
void    aurora_lock_guard_destroy(int64_t handle);

/* ── RWLock ── */
int64_t aurora_rwlock_create(void);
void    aurora_rwlock_destroy(int64_t handle);
void    aurora_rwlock_read_lock(int64_t handle);
void    aurora_rwlock_read_unlock(int64_t handle);
void    aurora_rwlock_write_lock(int64_t handle);
void    aurora_rwlock_write_unlock(int64_t handle);

/* ── Condition Variable ── */
int64_t aurora_condvar_create(void);
void    aurora_condvar_destroy(int64_t handle);
void    aurora_condvar_wait(int64_t handle, int64_t mutex_handle);
void    aurora_condvar_signal(int64_t handle);
void    aurora_condvar_broadcast(int64_t handle);

/* ── Semaphore ── */
int64_t aurora_semaphore_create(int32_t initial_count);
void    aurora_semaphore_destroy(int64_t handle);
void    aurora_semaphore_wait(int64_t handle);
void    aurora_semaphore_post(int64_t handle);
int32_t aurora_semaphore_try_wait(int64_t handle);

/* ── Atomic (i64) ── */
int64_t aurora_atomic_create(int64_t initial);
void    aurora_atomic_destroy(int64_t handle);
int64_t aurora_atomic_load(int64_t handle);
void    aurora_atomic_store(int64_t handle, int64_t val);
int64_t aurora_atomic_fetch_add(int64_t handle, int64_t val);
int64_t aurora_atomic_fetch_sub(int64_t handle, int64_t val);
int64_t aurora_atomic_exchange(int64_t handle, int64_t val);
int32_t aurora_atomic_compare_exchange(int64_t handle, int64_t expected, int64_t desired);

/* ── Future / Promise ── */
int64_t aurora_future_create(void);
void    aurora_future_destroy(int64_t handle);
int32_t aurora_future_is_ready(int64_t handle);
int64_t aurora_future_get(int64_t handle);
int64_t aurora_future_try_get(int64_t handle);
void    aurora_promise_set_value(int64_t future_handle, int64_t value);
void    aurora_promise_set_exception(int64_t future_handle);

/* ── Thread Pool ── */
int64_t aurora_thread_pool_create(int32_t thread_count);
void    aurora_thread_pool_destroy(int64_t handle);
int64_t aurora_thread_pool_enqueue(int64_t handle, void* (*func)(void*), void* arg);
int32_t aurora_thread_pool_pending(int64_t handle);
int32_t aurora_thread_pool_worker_count(int64_t handle);

#ifdef __cplusplus
}
#endif
