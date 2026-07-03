#include <cstdint>
#include <cstdlib>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <semaphore>
#include <atomic>
#include <vector>
#include <queue>

#include "std/thread.hpp"

/* ════════════════════════════════════════════════════════════
   Aurora Runtime — Phase 3: Threading & Concurrency
   ════════════════════════════════════════════════════════════ */

/* ── Helper: opaque handle wrapping a heap-allocated T ── */
template<typename T>
static inline int64_t make_handle(T* ptr) {
    return reinterpret_cast<int64_t>(ptr);
}
template<typename T>
static inline T* from_handle(int64_t h) {
    return reinterpret_cast<T*>(h);
}

/* ════════════════════════════════════════════════════════════
   Thread
   ════════════════════════════════════════════════════════════ */
extern "C" {

int64_t aurora_thread_create(void* (*func)(void*), void* arg) {
    std::thread* t = new std::thread(func, arg);
    return make_handle(t);
}

void aurora_thread_join(int64_t handle) {
    auto* t = from_handle<std::thread>(handle);
    if (t && t->joinable()) t->join();
    delete t;
}

void aurora_thread_detach(int64_t handle) {
    auto* t = from_handle<std::thread>(handle);
    if (t && t->joinable()) t->detach();
    delete t;
}

int64_t aurora_thread_self(void) {
    return static_cast<int64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
}

void aurora_thread_sleep(int32_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

/* ════════════════════════════════════════════════════════════
   Mutex
   ════════════════════════════════════════════════════════════ */

int64_t aurora_mutex_create(void) {
    return make_handle(new std::mutex());
}

void aurora_mutex_destroy(int64_t handle) {
    delete from_handle<std::mutex>(handle);
}

void aurora_mutex_lock(int64_t handle) {
    from_handle<std::mutex>(handle)->lock();
}

void aurora_mutex_unlock(int64_t handle) {
    from_handle<std::mutex>(handle)->unlock();
}

int32_t aurora_mutex_try_lock(int64_t handle) {
    return from_handle<std::mutex>(handle)->try_lock() ? 1 : 0;
}

/* ════════════════════════════════════════════════════════════
   LockGuard (RAII)
   ════════════════════════════════════════════════════════════ */

int64_t aurora_lock_guard_create(int64_t mutex_handle) {
    auto* m = from_handle<std::mutex>(mutex_handle);
    return make_handle(new std::lock_guard<std::mutex>(*m));
}

void aurora_lock_guard_destroy(int64_t handle) {
    delete from_handle<std::lock_guard<std::mutex>>(handle);
}

/* ════════════════════════════════════════════════════════════
   RWLock
   ════════════════════════════════════════════════════════════ */

int64_t aurora_rwlock_create(void) {
    return make_handle(new std::shared_mutex());
}

void aurora_rwlock_destroy(int64_t handle) {
    delete from_handle<std::shared_mutex>(handle);
}

void aurora_rwlock_read_lock(int64_t handle) {
    from_handle<std::shared_mutex>(handle)->lock_shared();
}

void aurora_rwlock_read_unlock(int64_t handle) {
    from_handle<std::shared_mutex>(handle)->unlock_shared();
}

void aurora_rwlock_write_lock(int64_t handle) {
    from_handle<std::shared_mutex>(handle)->lock();
}

void aurora_rwlock_write_unlock(int64_t handle) {
    from_handle<std::shared_mutex>(handle)->unlock();
}

/* ════════════════════════════════════════════════════════════
   Condition Variable
   ════════════════════════════════════════════════════════════ */

int64_t aurora_condvar_create(void) {
    return make_handle(new std::condition_variable());
}

void aurora_condvar_destroy(int64_t handle) {
    delete from_handle<std::condition_variable>(handle);
}

void aurora_condvar_wait(int64_t handle, int64_t mutex_handle) {
    auto* cv = from_handle<std::condition_variable>(handle);
    auto* m = from_handle<std::mutex>(mutex_handle);
    std::unique_lock<std::mutex> lock(*m);
    cv->wait(lock);
}

void aurora_condvar_signal(int64_t handle) {
    from_handle<std::condition_variable>(handle)->notify_one();
}

void aurora_condvar_broadcast(int64_t handle) {
    from_handle<std::condition_variable>(handle)->notify_all();
}

/* ════════════════════════════════════════════════════════════
   Semaphore
   ════════════════════════════════════════════════════════════ */

int64_t aurora_semaphore_create(int32_t initial_count) {
    return make_handle(new std::counting_semaphore<>(initial_count));
}

void aurora_semaphore_destroy(int64_t handle) {
    delete from_handle<std::counting_semaphore<>>(handle);
}

void aurora_semaphore_wait(int64_t handle) {
    from_handle<std::counting_semaphore<>>(handle)->acquire();
}

void aurora_semaphore_post(int64_t handle) {
    from_handle<std::counting_semaphore<>>(handle)->release();
}

int32_t aurora_semaphore_try_wait(int64_t handle) {
    return from_handle<std::counting_semaphore<>>(handle)->try_acquire() ? 1 : 0;
}

/* ════════════════════════════════════════════════════════════
   Atomic (int64_t)
   ════════════════════════════════════════════════════════════ */

int64_t aurora_atomic_create(int64_t initial) {
    return make_handle(new std::atomic<int64_t>(initial));
}

void aurora_atomic_destroy(int64_t handle) {
    delete from_handle<std::atomic<int64_t>>(handle);
}

int64_t aurora_atomic_load(int64_t handle) {
    return from_handle<std::atomic<int64_t>>(handle)->load();
}

void aurora_atomic_store(int64_t handle, int64_t val) {
    from_handle<std::atomic<int64_t>>(handle)->store(val);
}

int64_t aurora_atomic_fetch_add(int64_t handle, int64_t val) {
    return from_handle<std::atomic<int64_t>>(handle)->fetch_add(val);
}

int64_t aurora_atomic_fetch_sub(int64_t handle, int64_t val) {
    return from_handle<std::atomic<int64_t>>(handle)->fetch_sub(val);
}

int64_t aurora_atomic_exchange(int64_t handle, int64_t val) {
    return from_handle<std::atomic<int64_t>>(handle)->exchange(val);
}

int32_t aurora_atomic_compare_exchange(int64_t handle, int64_t expected, int64_t desired) {
    auto* a = from_handle<std::atomic<int64_t>>(handle);
    int64_t exp = expected;
    return a->compare_exchange_strong(exp, desired) ? 1 : 0;
}

/* ════════════════════════════════════════════════════════════
   Future / Promise
   ════════════════════════════════════════════════════════════ */

struct FutureState {
    std::mutex mtx;
    std::condition_variable cv;
    int64_t value;
    bool ready;
    bool exception;
};

int64_t aurora_future_create(void) {
    return make_handle(new FutureState{});
}

void aurora_future_destroy(int64_t handle) {
    delete from_handle<FutureState>(handle);
}

int32_t aurora_future_is_ready(int64_t handle) {
    auto* fs = from_handle<FutureState>(handle);
    return fs->ready ? 1 : 0;
}

int64_t aurora_future_get(int64_t handle) {
    auto* fs = from_handle<FutureState>(handle);
    std::unique_lock<std::mutex> lock(fs->mtx);
    fs->cv.wait(lock, [fs]{ return fs->ready; });
    return fs->value;
}

int64_t aurora_future_try_get(int64_t handle) {
    auto* fs = from_handle<FutureState>(handle);
    std::lock_guard<std::mutex> lock(fs->mtx);
    return fs->ready ? fs->value : 0;
}

void aurora_promise_set_value(int64_t future_handle, int64_t value) {
    auto* fs = from_handle<FutureState>(future_handle);
    {
        std::lock_guard<std::mutex> lock(fs->mtx);
        fs->value = value;
        fs->ready = true;
        fs->exception = false;
    }
    fs->cv.notify_all();
}

void aurora_promise_set_exception(int64_t future_handle) {
    auto* fs = from_handle<FutureState>(future_handle);
    {
        std::lock_guard<std::mutex> lock(fs->mtx);
        fs->value = 0;
        fs->ready = true;
        fs->exception = true;
    }
    fs->cv.notify_all();
}

/* ════════════════════════════════════════════════════════════
   Thread Pool
   ════════════════════════════════════════════════════════════ */

struct ThreadPool {
    std::vector<std::thread> workers;
    std::queue<void* (*)(void*)> funcs;
    std::queue<void*> args;
    std::mutex mtx;
    std::condition_variable cv;
    bool running;
};

int64_t aurora_thread_pool_create(int32_t thread_count) {
    auto* pool = new ThreadPool();
    pool->running = true;
    if (thread_count <= 0) thread_count = 1;

    for (int32_t i = 0; i < thread_count; i++) {
        pool->workers.emplace_back([pool]() {
            while (true) {
                void* (*func)(void*) = nullptr;
                void* arg = nullptr;
                {
                    std::unique_lock<std::mutex> lock(pool->mtx);
                    pool->cv.wait(lock, [pool] {
                        return !pool->running || !pool->funcs.empty();
                    });
                    if (!pool->running && pool->funcs.empty()) return;
                    func = pool->funcs.front();
                    arg = pool->args.front();
                    pool->funcs.pop();
                    pool->args.pop();
                }
                if (func) func(arg);
            }
        });
    }
    return make_handle(pool);
}

void aurora_thread_pool_destroy(int64_t handle) {
    auto* pool = from_handle<ThreadPool>(handle);
    {
        std::lock_guard<std::mutex> lock(pool->mtx);
        pool->running = false;
    }
    pool->cv.notify_all();
    for (auto& w : pool->workers) {
        if (w.joinable()) w.join();
    }
    delete pool;
}

int64_t aurora_thread_pool_enqueue(int64_t handle, void* (*func)(void*), void* arg) {
    auto* pool = from_handle<ThreadPool>(handle);
    {
        std::lock_guard<std::mutex> lock(pool->mtx);
        pool->funcs.push(func);
        pool->args.push(arg);
    }
    pool->cv.notify_one();
    return 1;
}

int32_t aurora_thread_pool_pending(int64_t handle) {
    auto* pool = from_handle<ThreadPool>(handle);
    std::lock_guard<std::mutex> lock(pool->mtx);
    return static_cast<int32_t>(pool->funcs.size());
}

int32_t aurora_thread_pool_worker_count(int64_t handle) {
    auto* pool = from_handle<ThreadPool>(handle);
    return static_cast<int32_t>(pool->workers.size());
}

} // extern "C"
