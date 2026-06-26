/* ── Aurora Runtime — Fiber (cooperative task) ── */
#include "runtime/async.hpp"

/* ── Thread-local current fiber ── */
static thread_local AuroraFiber* tls_current_fiber = nullptr;

extern "C" {

AuroraFiber* aurora_fiber_create(void* (*func)(void*), void* arg) {
    auto* fiber = new AuroraFiber();
    fiber->func = func;
    fiber->arg  = arg;
    fiber->result = nullptr;
    fiber->state  = 0;     /* ready */
    fiber->next = nullptr;
    return fiber;
}

void aurora_fiber_destroy(AuroraFiber* fiber) {
    delete fiber;
}

void aurora_fiber_yield(void) {
    AuroraFiber* cur = tls_current_fiber;
    if (!cur) return;
    cur->state = 2;          /* yielded */
    cur->yielded = 1;
}

void aurora_fiber_resume(AuroraFiber* fiber) {
    if (!fiber) return;
    if (fiber->state == 3) return; /* already done */

    if (fiber->state == 0 || fiber->state == 2) {
        AuroraFiber* prev = tls_current_fiber;
        tls_current_fiber = fiber;

        fiber->yielded = 0;
        void* result = fiber->func(fiber->arg);
        fiber->result = result;

        tls_current_fiber = prev;

        if (fiber->yielded) {
            fiber->state = 2; /* yielded */
        } else {
            fiber->state = 3; /* done */
        }
    }
}

int32_t aurora_fiber_is_done(AuroraFiber* fiber) {
    return fiber ? (fiber->state == 3 ? 1 : 0) : 1;
}

void* aurora_fiber_get_result(AuroraFiber* fiber) {
    return fiber ? fiber->result : nullptr;
}

AuroraFiber* aurora_fiber_current(void) {
    return tls_current_fiber;
}

}
