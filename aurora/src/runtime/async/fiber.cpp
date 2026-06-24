/* ── Aurora Runtime — Fiber (cooperative task) ── */
#include "runtime/async.hpp"
#include <cstdlib>

/* ── Thread-local current fiber ── */
static thread_local AuroraFiber* tls_current_fiber = nullptr;

/* ── Fiber context (stack + yield point) ── */
struct FiberContext {
    void* (*func)(void*);
    void*   arg;
    AuroraFiber* fiber;
    jmp_buf  yield_point;
};

extern "C" {

AuroraFiber* aurora_fiber_create(void* (*func)(void*), void* arg) {
    void* mem = std::malloc(sizeof(AuroraFiber));
    if (!mem) return nullptr;
    auto* fiber = ::new (mem) AuroraFiber;
    fiber->func = func;
    fiber->arg  = arg;
    fiber->result = nullptr;
    fiber->state  = 0;     /* ready */
    fiber->context = nullptr;
    fiber->next = nullptr;
    return fiber;
}

void aurora_fiber_destroy(AuroraFiber* fiber) {
    if (!fiber) return;
    if (fiber->context) {
        auto* ctx = (FiberContext*)fiber->context;
        std::free(ctx);
    }
    fiber->~AuroraFiber();
    std::free(fiber);
}

void aurora_fiber_yield(void) {
    AuroraFiber* cur = tls_current_fiber;
    if (!cur) return;
    cur->state = 2;  /* yielded */
    auto* ctx = (FiberContext*)cur->context;
    if (!ctx) return;
    /* longjmp back to the resumer */
    /* TODO: setjmp/longjmp with C++ objects is UB — migrate to std::coroutine or exceptions */
    std::longjmp(ctx->yield_point, 1);
}

void aurora_fiber_resume(AuroraFiber* fiber) {
    if (!fiber) return;
    if (fiber->state == 3) return; /* already done */

    if (fiber->state == 0) {
        /* First time: create context and start */
        fiber->state = 1; /* running */
        auto* ctx = (FiberContext*)std::malloc(sizeof(FiberContext));
        ctx->func  = fiber->func;
        ctx->arg   = fiber->arg;
        ctx->fiber = fiber;
        fiber->context = ctx;

        AuroraFiber* prev = tls_current_fiber;
        tls_current_fiber = fiber;

        /* Use setjmp to establish a yield return point */
        /* TODO: setjmp/longjmp with C++ objects is UB — migrate to std::coroutine or exceptions */
        if (setjmp(ctx->yield_point) == 0) {
            /* Call the function directly (simulated fiber) */
            void* result = fiber->func(fiber->arg);
            fiber->result = result;
            fiber->state = 3; /* done */
        }

        tls_current_fiber = prev;
    } else if (fiber->state == 2) {
        /* Yielded: resume */
        fiber->state = 1;
        auto* ctx = (FiberContext*)fiber->context;
        AuroraFiber* prev = tls_current_fiber;
        tls_current_fiber = fiber;
        /* The yield point already returned via longjmp — just continue */
        /* In a real fiber implementation, we'd switch stack context here */
        fiber->state = 3; /* mark done for this simple implementation */
        tls_current_fiber = prev;
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
