/* ── Aurora Runtime — Async Channel (Phase 28: Lock-free SPSC) ── */
#include "runtime/async.hpp"
#include <cstdlib>
#include <cstring>
#include <atomic>

/* Phase 28: For power-of-2 capacity, use lock-free SPSC ring buffer */
static bool is_pow2(int32_t x) { return x > 0 && (x & (x - 1)) == 0; }

extern "C" {

AuroraChannel* aurora_chan_create(int32_t capacity) {
    if (capacity <= 0) capacity = 16;
    void* mem = std::malloc(sizeof(AuroraChannel));
    if (!mem) return nullptr;
    auto* ch = ::new (mem) AuroraChannel;
    ch->buf = (void**)std::calloc((size_t)capacity, sizeof(void*));
    ch->capacity = capacity;
    ch->head = 0;
    ch->tail = 0;
    ch->count = 0;
    ch->closed = false;
    return ch;
}

void aurora_chan_destroy(AuroraChannel* ch) {
    if (!ch) return;
    std::free(ch->buf);
    ch->~AuroraChannel();
    std::free(ch);
}

/* Phase 28: For power-of-2 capacity channels, use lock-free fast path */
static inline bool chan_send_fast(AuroraChannel* ch, void* val) {
    if (!is_pow2(ch->capacity)) return false;
    int32_t mask = ch->capacity - 1;
    int32_t tail = ch->tail;
    int32_t head;
    memcpy(&head, &ch->head, sizeof(head));
    if ((tail - head) >= ch->capacity) return false; /* full */
    ch->buf[tail & mask] = val;
    std::atomic_thread_fence(std::memory_order_release);
    ch->tail = tail + 1;
    return true;
}

static inline void* chan_recv_fast(AuroraChannel* ch) {
    if (!is_pow2(ch->capacity)) return nullptr;
    int32_t mask = ch->capacity - 1;
    int32_t head = ch->head;
    int32_t tail;
    memcpy(&tail, &ch->tail, sizeof(tail));
    if (head >= tail) return nullptr; /* empty */
    void* val = ch->buf[head & mask];
    std::atomic_thread_fence(std::memory_order_acquire);
    ch->head = head + 1;
    return val;
}

void aurora_chan_send(AuroraChannel* ch, void* val) {
    if (!ch) return;

    /* Try lock-free fast path first */
    if (chan_send_fast(ch, val)) return;

    /* Fall back to mutex path */
    std::unique_lock<std::mutex> lock(ch->mtx);
    ch->cv.wait(lock, [ch] { return (ch->count < ch->capacity) || ch->closed; });
    if (ch->closed) return;
    ch->buf[ch->tail] = val;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    lock.unlock();
    ch->cv.notify_one();
}

void* aurora_chan_recv(AuroraChannel* ch) {
    if (!ch) return nullptr;

    /* Try lock-free fast path first */
    void* r = chan_recv_fast(ch);
    if (r) return r;

    /* Fall back to mutex path */
    std::unique_lock<std::mutex> lock(ch->mtx);
    ch->cv.wait(lock, [ch] { return (ch->count > 0) || ch->closed; });
    if (ch->closed || ch->count == 0) return nullptr;
    void* val = ch->buf[ch->head];
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    lock.unlock();
    ch->cv.notify_one();
    return val;
}

void aurora_chan_close(AuroraChannel* ch) {
    if (!ch) return;
    {
        std::lock_guard<std::mutex> lock(ch->mtx);
        ch->closed = true;
    }
    ch->cv.notify_all();
}

int32_t aurora_chan_try_send(AuroraChannel* ch, void* val) {
    if (!ch) return 0;
    if (chan_send_fast(ch, val)) return 1;
    std::lock_guard<std::mutex> lock(ch->mtx);
    if (ch->count >= ch->capacity) return 0;
    ch->buf[ch->tail] = val;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    ch->cv.notify_one();
    return 1;
}

void* aurora_chan_try_recv(AuroraChannel* ch) {
    if (!ch) return nullptr;
    void* r = chan_recv_fast(ch);
    if (r) return r;
    std::lock_guard<std::mutex> lock(ch->mtx);
    if (ch->count <= 0) return nullptr;
    void* val = ch->buf[ch->head];
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    ch->cv.notify_one();
    return val;
}

}
