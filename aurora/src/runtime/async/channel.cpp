/* ── Aurora Runtime — Async Channel (bounded queue) ── */
#include "runtime/async.hpp"
#include <cstdlib>
#include <cstring>

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

void aurora_chan_send(AuroraChannel* ch, void* val) {
    if (!ch) return;
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
    std::lock_guard<std::mutex> lock(ch->mtx);
    if (ch->count <= 0) return nullptr;
    void* val = ch->buf[ch->head];
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    ch->cv.notify_one();
    return val;
}

}
