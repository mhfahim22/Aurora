/* ── Aurora Runtime — Fiber (cooperative task with true context switching) ──
 *
 * Architecture:
 *   - Windows: Uses CreateFiber / SwitchToFiber / DeleteFiber (Win32 Fiber API)
 *   - POSIX:   Uses getcontext / makecontext / swapcontext (ucontext API)
 *
 * Each fiber gets its own stack (64 KB default).  When a fiber calls yield(),
 * the CPU registers and stack pointer are saved, and control returns to the
 * caller of aurora_fiber_resume().  Resuming the fiber restores its saved
 * context, and execution continues exactly where it stopped.
 *
 * State machine:
 *   0 = ready (never started)
 *   1 = running (currently executing on a thread)
 *   2 = yielded (paused, registers saved, can be resumed)
 *   3 = done   (function returned, fibre is finished)
 * ── */
#include "runtime/async.hpp"
#include <cstdlib>

#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  static constexpr size_t kFiberStackSize = 64 * 1024;  /* 64 KB */
#elif defined(__APPLE__)
  #ifndef _XOPEN_SOURCE
  #define _XOPEN_SOURCE 700
  #endif
  #include <ucontext.h>
  static constexpr size_t kFiberStackSize = 64 * 1024;  /* 64 KB */
#else
  #include <ucontext.h>
  static constexpr size_t kFiberStackSize = 64 * 1024;  /* 64 KB */
#endif

/* ── Thread-local state ── */
static thread_local AuroraFiber* tls_current_fiber = nullptr;

#if defined(_WIN32)
/* On Windows the "main" thread fibre handle is cached once per thread so that
   aurora_fiber_yield() knows where to jump back to. */
static thread_local void*    tls_main_fiber   = nullptr;
static thread_local bool     tls_is_fiber     = false;
#else
/* On POSIX makecontext() only accepts int arguments, so we pass the fibre
   pointer through a thread-local before calling makecontext(). */
static thread_local AuroraFiber* tls_creating_fiber = nullptr;
#endif

/* ── Entry trampolines ── */

#if defined(_WIN32)
static void WINAPI fiber_entry(void* param) {
    AuroraFiber* fiber = static_cast<AuroraFiber*>(param);
    tls_current_fiber = fiber;
    fiber->state = 1;                       /* running */
    fiber->result = fiber->func(fiber->arg);
    fiber->state = 3;                       /* done */
    /* Return to whoever resumed this fiber */
    if (fiber->os_return)
        SwitchToFiber(fiber->os_return);
}
#else
static void fiber_entry(void) {
    AuroraFiber* fiber = tls_creating_fiber;
    tls_creating_fiber = nullptr;
    if (!fiber) return;
    tls_current_fiber = fiber;
    fiber->state = 1;                       /* running */
    fiber->result = fiber->func(fiber->arg);
    fiber->state = 3;                       /* done */
    /* Swap back to the return context saved in aurora_fiber_resume() */
    if (fiber->os_return) {
        ucontext_t* ret_ctx = static_cast<ucontext_t*>(fiber->os_return);
        ucontext_t* cur_ctx = static_cast<ucontext_t*>(fiber->os_handle);
        swapcontext(cur_ctx, ret_ctx);
    }
}
#endif

/* ── Public API ── */

extern "C" {

AuroraFiber* aurora_fiber_create(void* (*func)(void*), void* arg) {
    auto* fiber = new AuroraFiber();
    if (!fiber) return nullptr;
    fiber->func      = func;
    fiber->arg       = arg;
    fiber->result    = nullptr;
    fiber->state     = 0;          /* ready */
    fiber->os_handle = nullptr;
    fiber->os_return = nullptr;
    fiber->stack_mem = nullptr;

#if defined(_WIN32)
    /* Ensure this thread is converted to a fiber (once per thread) */
    if (!tls_is_fiber) {
        tls_main_fiber = ConvertThreadToFiber(nullptr);
        if (!tls_main_fiber)          /* Already a fiber */
            tls_main_fiber = GetCurrentFiber();
        tls_is_fiber = true;
    }
    fiber->os_handle = CreateFiber(kFiberStackSize, fiber_entry, fiber);
    if (!fiber->os_handle) {
        delete fiber;
        return nullptr;
    }
    /* The "return" target for this fibre is always the current thread's main
       fibre so that yield() goes back to the thread, not to a specific nested
       resumer.  The resumer does NOT need to track its own context on Windows
       because SwitchToFiber is symmetric – whoever calls SwitchToFiber becomes
       the "current" fibre, and yielding returns to tls_main_fiber. */
    fiber->os_return = tls_main_fiber;
#else
    /* Allocate stack for the fibre */
    void* stack = std::malloc(kFiberStackSize);
    if (!stack) {
        delete fiber;
        return nullptr;
    }
    fiber->stack_mem = stack;

    /* Allocate ucontext_t for the fibre */
    auto* ctx = new ucontext_t();
    if (!ctx) {
        std::free(stack);
        delete fiber;
        return nullptr;
    }
    fiber->os_handle = ctx;

    /* Set up the context using makecontext */
    getcontext(ctx);
    ctx->uc_stack.ss_sp    = stack;
    ctx->uc_stack.ss_size  = kFiberStackSize;
    ctx->uc_link           = nullptr;          /* handled by trampoline */

    tls_creating_fiber = fiber;
    makecontext(ctx, fiber_entry, 0);

    /* Allocate the return context lazily on first resume */
    fiber->os_return = nullptr;
#endif

    return fiber;
}

void aurora_fiber_destroy(AuroraFiber* fiber) {
    if (!fiber) return;

#if defined(_WIN32)
    if (fiber->os_handle)
        DeleteFiber(fiber->os_handle);
#else
    if (fiber->stack_mem)
        std::free(fiber->stack_mem);
    if (fiber->os_handle)
        delete static_cast<ucontext_t*>(fiber->os_handle);
    if (fiber->os_return)
        delete static_cast<ucontext_t*>(fiber->os_return);
#endif

    delete fiber;
}

void aurora_fiber_yield(void) {
    AuroraFiber* cur = tls_current_fiber;
    if (!cur) return;                     /* not inside a fiber – no-op */
    if (cur->state != 1) return;          /* not running – no-op */

    cur->state = 2;                       /* yielded */

#if defined(_WIN32)
    /* Save our state and jump back to the thread's main fiber.  Next time
       someone calls SwitchToFiber(cur->os_handle) we will resume here. */
    tls_current_fiber = nullptr;
    SwitchToFiber(cur->os_return);
#else
    ucontext_t* cur_ctx  = static_cast<ucontext_t*>(cur->os_handle);
    ucontext_t* ret_ctx  = static_cast<ucontext_t*>(cur->os_return);
    if (cur_ctx && ret_ctx) {
        tls_current_fiber = nullptr;
        swapcontext(cur_ctx, ret_ctx);
    }
#endif

    /* Execution resumes here when the fiber is resumed */
    tls_current_fiber = cur;
}

void aurora_fiber_resume(AuroraFiber* fiber) {
    if (!fiber) return;
    if (fiber->state == 3) return;        /* already done */
    if (fiber->state == 1) return;        /* already running – ignore */

    AuroraFiber* prev = tls_current_fiber;

#if defined(_WIN32)
    fiber->os_return = tls_main_fiber;
    tls_current_fiber = fiber;
    fiber->state = 1;                     /* running */
    SwitchToFiber(fiber->os_handle);
    /* When we get back here the fibre either yielded or completed */
    tls_current_fiber = prev;
#else
    /* Lazily allocate the return context */
    if (!fiber->os_return) {
        fiber->os_return = new ucontext_t();
    }
    ucontext_t* fiber_ctx = static_cast<ucontext_t*>(fiber->os_handle);
    ucontext_t* ret_ctx   = static_cast<ucontext_t*>(fiber->os_return);
    if (!fiber_ctx) return;

    tls_current_fiber = fiber;
    fiber->state = 1;                     /* running */
    swapcontext(ret_ctx, fiber_ctx);
    /* When we get back here the fibre either yielded or completed */
    tls_current_fiber = prev;
#endif
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

} // extern "C"
