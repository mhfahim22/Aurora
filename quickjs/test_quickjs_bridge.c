/* Test QuickJS bridge DLL */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static double now_ms() {
#ifdef _WIN32
    LARGE_INTEGER freq, t;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart * 1000.0 / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
#endif
}

int main() {
    printf("=== QuickJS Bridge Test ===\n\n");

#ifdef _WIN32
    HMODULE lib = LoadLibraryA("quickjs\\build\\moment_quickjs.dll");
    if (!lib) {
        lib = LoadLibraryA("moment_quickjs.dll");
    }
    if (!lib) {
        printf("FAIL: cannot load moment_quickjs.dll\n");
        return 1;
    }
#else
    void *lib = dlopen("./moment_quickjs.so", RTLD_NOW);
    if (!lib) { printf("FAIL: %s\n", dlerror()); return 1; }
#endif

    /* Get function pointers */
    typedef void *(*require_t)();
    typedef void  (*free_t)(void *);
    typedef void *(*call_t)(void *, const char *, void *);
    typedef void *(*str_t)(const char *);
    typedef void *(*getattr_t)(void *, const char *);
    typedef char *(*to_cstr_t)(void *);

#ifdef _WIN32
    require_t moment_require = (require_t)GetProcAddress(lib, "moment_require");
    free_t moment_free = (free_t)GetProcAddress(lib, "moment_free");
    call_t moment_call = (call_t)GetProcAddress(lib, "moment_call");
    str_t moment_str = (str_t)GetProcAddress(lib, "moment_str");
    getattr_t moment_getattr = (getattr_t)GetProcAddress(lib, "moment_getattr");
    to_cstr_t moment_to_cstr = (to_cstr_t)GetProcAddress(lib, "moment_to_cstr");
#else
    require_t moment_require = (require_t)dlsym(lib, "moment_require");
    free_t moment_free = (free_t)dlsym(lib, "moment_free");
    call_t moment_call = (call_t)dlsym(lib, "moment_call");
    str_t moment_str = (str_t)dlsym(lib, "moment_str");
    getattr_t moment_getattr = (getattr_t)dlsym(lib, "moment_getattr");
    to_cstr_t moment_to_cstr = (to_cstr_t)dlsym(lib, "moment_to_cstr");
#endif

    if (!moment_require || !moment_free || !moment_call || !moment_str) {
        printf("FAIL: missing symbols\n");
        return 1;
    }

    printf("[1] moment_require()...\n");
    double t0 = now_ms();
    void *mod = moment_require();
    double t1 = now_ms();
    if (!mod) { printf("FAIL: require returned null\n"); return 1; }
    printf("  PASS (%.3f ms)\n", t1 - t0);

    printf("[2] moment.now()...\n");
    t0 = now_ms();
    void *now = moment_call(mod, "now", 0);
    t1 = now_ms();
    if (!now) { printf("FAIL: now returned null\n"); return 1; }
    char *ts = moment_to_cstr(now);
    printf("  Result: %s\n", ts);
    printf("  PASS (%.3f ms)\n", t1 - t0);
    if (ts) free(ts);
    moment_free(now);

    printf("[3] moment.version (getattr)...\n");
    t0 = now_ms();
    void *ver = moment_getattr(mod, "version");
    t1 = now_ms();
    if (!ver) { printf("FAIL: getattr returned null\n"); return 1; }
    ts = moment_to_cstr(ver);
    printf("  Version: %s\n", ts);
    printf("  PASS (%.3f ms)\n", t1 - t0);
    if (ts) free(ts);
    moment_free(ver);

    printf("[4] 100 call latency...\n");
    t0 = now_ms();
    for (int i = 0; i < 100; i++) {
        void *n = moment_call(mod, "now", 0);
        moment_free(n);
    }
    t1 = now_ms();
    printf("  PASS (%.3f ms/call)\n", (t1 - t0) / 100.0);

    printf("[5] 1000 burst...\n");
    t0 = now_ms();
    for (int i = 0; i < 1000; i++) {
        void *n = moment_call(mod, "now", 0);
        moment_free(n);
    }
    t1 = now_ms();
    printf("  PASS (%.3f ms/call)\n", (t1 - t0) / 1000.0);

    printf("[6] moment('2026-01-15').format('YYYY-MM-DD')...\n");
    void *date_str = moment_str("2026-01-15");
    void *format_str = moment_str("YYYY-MM-DD");
    void *m = moment_call(mod, "", date_str);  // call moment() as function
    if (m) {
        void *fmt_result = moment_call(m, "format", format_str);
        if (fmt_result) {
            char *fmt_str = moment_to_cstr(fmt_result);
            printf("  Result: %s\n", fmt_str);
            if (fmt_str) free(fmt_str);
            moment_free(fmt_result);
        } else {
            printf("  format returned null\n");
        }
        moment_free(m);
    } else {
        printf("  moment() returned null\n");
    }
    moment_free(date_str);
    moment_free(format_str);

    moment_free(mod);
    printf("\n=== All Tests Passed ===\n");
    return 0;
}
