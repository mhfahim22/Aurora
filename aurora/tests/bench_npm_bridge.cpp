/* npm Bridge Performance Benchmark */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static double now_ms() {
    static auto epoch = std::chrono::steady_clock::now();
    auto t = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(t - epoch).count();
}

#ifdef _WIN32
static void* dl_open(const char* p) { return (void*)LoadLibraryA(p); }
static void* dl_sym(void* l, const char* n) { return (void*)GetProcAddress((HMODULE)l, n); }
#else
static void* dl_open(const char* p) { return dlopen(p, RTLD_NOW); }
static void* dl_sym(void* l, const char* n) { return dlsym(l, n); }
#endif

int main() {
    printf("=== npm Bridge Performance Benchmark ===\n\n");

    char buf[512];
    snprintf(buf, sizeof(buf), "moment_npm_bridge/moment_npm.dll");
    void* lib = dl_open(buf);
    if (!lib) {
        snprintf(buf, sizeof(buf), "../build/moment_npm_bridge/moment_npm.dll");
        lib = dl_open(buf);
    }
    if (!lib) {
        printf("SKIP: moment_npm bridge not found\n");
        return 0;
    }

    typedef void* (*require_t)();
    typedef void* (*call_t)(void*, const char*, void*);
    typedef void  (*free_t)(void*);

    auto moment_require = (require_t)dl_sym(lib, "moment_require");
    auto moment_call = (call_t)dl_sym(lib, "moment_call");
    auto moment_free = (free_t)dl_sym(lib, "moment_free");

    if (!moment_require || !moment_call || !moment_free) {
        printf("FAIL: missing symbols\n");
        return 1;
    }

    void* mod = moment_require();
    if (!mod) { printf("FAIL: require returned null\n"); return 1; }

    /* Phase 1: Single call latency (100 iters) */
    printf("Phase 1: Single call latency (100 iterations)\n");
    double t0 = now_ms();
    for (int i = 0; i < 100; i++) {
        void* r = moment_call(mod, "now", 0);
        moment_free(r);
    }
    double t1 = now_ms();
    double per_call = (t1 - t0) / 100.0;
    printf("  %.3f ms/call  (%.0f calls/sec)\n", per_call, 1000.0 / per_call);

    /* Phase 2: Warm cache (after first call, subsequent should be faster) */
    printf("\nPhase 2: Cached call latency (100 iterations)\n");
    t0 = now_ms();
    for (int i = 0; i < 100; i++) {
        void* r = moment_call(mod, "now", 0);
        moment_free(r);
    }
    t1 = now_ms();
    double cached = (t1 - t0) / 100.0;
    printf("  %.3f ms/call  (%.0f calls/sec)\n", cached, 1000.0 / cached);

    /* Phase 3: Burst throughput (1000 calls) */
    printf("\nPhase 3: Burst throughput (1000 calls)\n");
    t0 = now_ms();
    for (int i = 0; i < 1000; i++) {
        void* r = moment_call(mod, "now", 0);
        moment_free(r);
    }
    t1 = now_ms();
    double burst = (t1 - t0) / 1000.0;
    printf("  %.3f ms/call  (%.0f calls/sec)\n", burst, 1000.0 / burst);

    /* Phase 4: Getattr benchmark */
    printf("\nPhase 4: getattr (version) latency (100 iterations)\n");
    typedef void* (*getattr_t)(void*, const char*);
    auto moment_getattr = (getattr_t)dl_sym(lib, "moment_getattr");
    if (moment_getattr) {
        t0 = now_ms();
        for (int i = 0; i < 100; i++) {
            void* v = moment_getattr(mod, "version");
            moment_free(v);
        }
        t1 = now_ms();
        double attr = (t1 - t0) / 100.0;
        printf("  %.3f ms/call  (%.0f calls/sec)\n", attr, 1000.0 / attr);
    }

    moment_free(mod);
    printf("\n=== Benchmark Complete ===\n");
    return 0;
}
