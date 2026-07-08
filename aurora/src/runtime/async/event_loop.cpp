#include "runtime/event_loop.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <atomic>
#include <chrono>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <poll.h>
#include <unistd.h>
#endif

/* ═══════════════════════════════════════════════════════════════
   Event Loop (poll/WSAPoll based)
   ═══════════════════════════════════════════════════════════════ */

struct PollFdEntry {
    int fd;
    int events;
    AuroraEventCallback callback;
    void* userdata;
    int active;
};

struct AuroraEventLoop {
    std::vector<PollFdEntry> entries;
    std::mutex mtx;
    int running;
    int capacity;
};

AuroraEventLoop* aurora_event_loop_new(int capacity) {
    AuroraEventLoop* loop = (AuroraEventLoop*)calloc(1, sizeof(AuroraEventLoop));
    if (!loop) return nullptr;
    loop->capacity = capacity > 0 ? capacity : 64;
    loop->running = 0;
    loop->entries.reserve((size_t)loop->capacity);
    return loop;
}

void aurora_event_loop_free(AuroraEventLoop* loop) {
    if (!loop) return;
    loop->running = 0;
    delete loop;
}

int aurora_event_loop_add(AuroraEventLoop* loop, int fd, int events,
                          AuroraEventCallback cb, void* userdata) {
    if (!loop || fd < 0 || !cb) return 0;
    std::lock_guard<std::mutex> lock(loop->mtx);
    for (auto& e : loop->entries) {
        if (e.fd == fd) {
            e.events = events;
            e.callback = cb;
            e.userdata = userdata;
            e.active = 1;
            return 1;
        }
    }
    PollFdEntry entry;
    entry.fd = fd;
    entry.events = events;
    entry.callback = cb;
    entry.userdata = userdata;
    entry.active = 1;
    loop->entries.push_back(entry);
    return 1;
}

int aurora_event_loop_remove(AuroraEventLoop* loop, int fd) {
    if (!loop || fd < 0) return 0;
    std::lock_guard<std::mutex> lock(loop->mtx);
    for (auto& e : loop->entries) {
        if (e.fd == fd) {
            e.active = 0;
            return 1;
        }
    }
    return 0;
}

int aurora_event_loop_wait(AuroraEventLoop* loop, int timeout_ms) {
    if (!loop || !loop->running) return 0;

    std::vector<PollFdEntry> snapshot;
    {
        std::lock_guard<std::mutex> lock(loop->mtx);
        for (auto& e : loop->entries) {
            if (e.active) snapshot.push_back(e);
        }
    }

    if (snapshot.empty()) return 0;

#ifdef _WIN32
    std::vector<WSAPOLLFD> fds;
    fds.reserve(snapshot.size());
    for (auto& e : snapshot) {
        WSAPOLLFD pfd;
        pfd.fd = (SOCKET)e.fd;
        pfd.events = 0;
        if (e.events & AURORA_EV_READ) pfd.events |= POLLIN;
        if (e.events & AURORA_EV_WRITE) pfd.events |= POLLOUT;
        pfd.revents = 0;
        fds.push_back(pfd);
    }
    int ret = WSAPoll(fds.data(), (ULONG)fds.size(), timeout_ms);
    if (ret <= 0) return ret;

    for (size_t i = 0; i < snapshot.size(); i++) {
        if (fds[i].revents && snapshot[i].callback) {
            int revents = 0;
            if (fds[i].revents & POLLIN) revents |= AURORA_EV_READ;
            if (fds[i].revents & POLLOUT) revents |= AURORA_EV_WRITE;
            snapshot[i].callback(snapshot[i].userdata, snapshot[i].fd, revents);
        }
    }
#else
    std::vector<struct pollfd> fds;
    fds.reserve(snapshot.size());
    for (auto& e : snapshot) {
        struct pollfd pfd;
        pfd.fd = e.fd;
        pfd.events = 0;
        if (e.events & AURORA_EV_READ) pfd.events |= POLLIN;
        if (e.events & AURORA_EV_WRITE) pfd.events |= POLLOUT;
        pfd.revents = 0;
        fds.push_back(pfd);
    }
    int ret = poll(fds.data(), (nfds_t)fds.size(), timeout_ms);
    if (ret <= 0) return ret;

    for (size_t i = 0; i < snapshot.size(); i++) {
        if (fds[i].revents && snapshot[i].callback) {
            int revents = 0;
            if (fds[i].revents & POLLIN) revents |= AURORA_EV_READ;
            if (fds[i].revents & POLLOUT) revents |= AURORA_EV_WRITE;
            snapshot[i].callback(snapshot[i].userdata, snapshot[i].fd, revents);
        }
    }
#endif
    return ret;
}

void aurora_event_loop_stop(AuroraEventLoop* loop) {
    if (loop) loop->running = 0;
}

int aurora_event_loop_is_running(AuroraEventLoop* loop) {
    return loop ? loop->running : 0;
}

/* ═══════════════════════════════════════════════════════════════
   Thread Pool
   ═══════════════════════════════════════════════════════════════ */

struct AuroraWorkerPool {
    std::vector<std::thread> workers;
    std::queue<void (*)(void*)> tasks;
    std::queue<void*> task_args;
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic<int> busy_count{0};
    std::atomic<int> running{0};
};

AuroraWorkerPool* aurora_worker_pool_new(int thread_count) {
    AuroraWorkerPool* pool = new AuroraWorkerPool();
    if (!pool) return nullptr;
    if (thread_count <= 0) thread_count = (int)std::thread::hardware_concurrency();
    if (thread_count <= 0) thread_count = 4;
    pool->running.store(1);

    for (int i = 0; i < thread_count; i++) {
        pool->workers.emplace_back([pool]() {
            while (pool->running.load()) {
                void (*task)(void*) = nullptr;
                void* arg = nullptr;
                {
                    std::unique_lock<std::mutex> lock(pool->mtx);
                    if (!pool->cv.wait_for(lock, std::chrono::milliseconds(100),
                                           [pool]() { return !pool->tasks.empty() || !pool->running.load(); })) {
                        continue;
                    }
                    if (!pool->running.load() && pool->tasks.empty()) break;
                    if (pool->tasks.empty()) continue;
                    task = pool->tasks.front();
                    arg = pool->task_args.front();
                    pool->tasks.pop();
                    pool->task_args.pop();
                }
                if (task) {
                    pool->busy_count.fetch_add(1);
                    task(arg);
                    pool->busy_count.fetch_sub(1);
                }
            }
        });
    }
    return pool;
}

void aurora_worker_pool_free(AuroraWorkerPool* pool) {
    if (!pool) return;
    pool->running.store(0);
    pool->cv.notify_all();
    for (auto& t : pool->workers) {
        if (t.joinable()) t.join();
    }
    delete pool;
}

int aurora_worker_pool_enqueue(AuroraWorkerPool* pool, void (*task)(void*), void* arg) {
    if (!pool || !task) return 0;
    {
        std::lock_guard<std::mutex> lock(pool->mtx);
        pool->tasks.push(task);
        pool->task_args.push(arg);
    }
    pool->cv.notify_one();
    return 1;
}

int aurora_worker_pool_busy_count(AuroraWorkerPool* pool) {
    return pool ? pool->busy_count.load() : 0;
}

int aurora_worker_pool_wait_idle(AuroraWorkerPool* pool, int timeout_ms) {
    if (!pool) return 1;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (pool->busy_count.load() == 0 && pool->tasks.empty()) return 1;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return 0;
}
