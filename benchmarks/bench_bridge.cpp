#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <string>
#include <vector>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#define DLL_LOAD LoadLibraryA
#define DLL_FUNC GetProcAddress
#define DLL_CLOSE FreeLibrary
#define DLL_EXT ".dll"
using dl_t = HMODULE;
#else
#include <dlfcn.h>
#define DLL_LOAD dlopen
#define DLL_FUNC dlsym
#define DLL_CLOSE dlclose
#define DLL_EXT ".so"
using dl_t = void*;
#endif

/* Benchmark helpers */
using my_clock_t = std::chrono::high_resolution_clock;
using my_ns_t = std::chrono::nanoseconds;

struct BenchmarkResult {
    std::string name;
    double avg_ns;
    double min_ns;
    double max_ns;
    int iterations;
};

static BenchmarkResult run_bench(const std::string& name, int iterations,
                                  void (*setup)(), double (*fn)(int),
                                  void (*teardown)()) {
    if (setup) setup();
    std::vector<double> times;
    times.reserve(iterations);
    for (int i = 0; i < iterations; i++) {
        auto start = my_clock_t::now();
        double result = fn(i);
        (void)result;
        auto end = my_clock_t::now();
        times.push_back((double)std::chrono::duration_cast<my_ns_t>(end - start).count());
    }
    if (teardown) teardown();
    double sum = 0.0;
    double mn = times[0], mx = times[0];
    for (auto t : times) { sum += t; if (t < mn) mn = t; if (t > mx) mx = t; }
    return {name, sum / times.size(), mn, mx, iterations};
}

/* Null bridge: measure empty call overhead */
static double null_call(int) { return 1.0; }

/* PyPI bridge benchmark */
static dl_t g_pypi_dll = nullptr;
static void* (*pypi_import)() = nullptr;
static void* (*pypi_call)(void*, const char*, void*) = nullptr;
static void* (*pypi_int)(long long) = nullptr;
static void* (*pypi_tuple)(void**, int) = nullptr;
static void* (*pypi_free)(void*) = nullptr;

static void setup_pypi() {
    if (g_pypi_dll) return;
    g_pypi_dll = DLL_LOAD("pypi_bridge" DLL_EXT);
    if (!g_pypi_dll) { printf("[bench] PyPI bridge DLL not found — skip\n"); return; }
    pypi_import = (void* (*)())DLL_FUNC(g_pypi_dll, "bridge_import");
    pypi_call = (void* (*)(void*, const char*, void*))DLL_FUNC(g_pypi_dll, "bridge_call");
    pypi_int = (void* (*)(long long))DLL_FUNC(g_pypi_dll, "bridge_int");
    pypi_tuple = (void* (*)(void**, int))DLL_FUNC(g_pypi_dll, "bridge_tuple");
    pypi_free = (void* (*)(void*))DLL_FUNC(g_pypi_dll, "bridge_free");
}

static double bench_pypi(int) {
    if (!g_pypi_dll || !pypi_import || !pypi_call) return -1.0;
    void* mod = pypi_import();
    if (!mod) return -1.0;
    void* args = pypi_tuple(nullptr, 0);
    void* result = pypi_call(mod, "getpid", args);
    if (pypi_free) pypi_free(result);
    if (pypi_free) pypi_free(mod);
    return 1.0;
}

/* Cargo bridge benchmark */
static dl_t g_cargo_dll = nullptr;
static int (*cargo_init)() = nullptr;
static int (*cargo_add)(int, int) = nullptr;

static void setup_cargo() {
    if (g_cargo_dll) return;
    g_cargo_dll = DLL_LOAD("cargo_bridge" DLL_EXT);
    if (!g_cargo_dll) { printf("[bench] Cargo bridge DLL not found — skip\n"); return; }
    cargo_init = (int (*)())DLL_FUNC(g_cargo_dll, "bridge_init");
    cargo_add = (int (*)(int, int))DLL_FUNC(g_cargo_dll, "add");
    if (cargo_init) cargo_init();
}

static double bench_cargo(int i) {
    if (!cargo_add) return -1.0;
    int r = cargo_add(i, i * 2);
    (void)r;
    return 1.0;
}

/* Native DLL benchmark (kernel32) */
static void* (*g_native_getpid)() = nullptr;

static void setup_native() {
    if (g_native_getpid) return;
#ifdef _WIN32
    HMODULE h = GetModuleHandleA("kernel32.dll");
    if (!h) h = LoadLibraryA("kernel32.dll");
    if (h) g_native_getpid = (void* (*)())GetProcAddress(h, "GetCurrentProcessId");
#else
    void* h = dlopen("libc.so.6", RTLD_NOW);
    if (h) g_native_getpid = (void* (*)())dlsym(h, "getpid");
#endif
}

static double bench_native(int) {
    if (!g_native_getpid) return -1.0;
    void* r = g_native_getpid();
    (void)r;
    return 1.0;
}

int main() {
    const int ITERATIONS = 10000;

    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║   Cross-Ecosystem Bridge Latency Benchmark            ║\n");
    printf("║   %d iterations per test                              ║\n", ITERATIONS);
    printf("╚════════════════════════════════════════════════════════╝\n\n");

    std::vector<BenchmarkResult> results;

    /* 1. Baseline: null call overhead */
    results.push_back(run_bench("Null call (baseline)", ITERATIONS, nullptr, null_call, nullptr));

    /* 2. Native DLL call */
    results.push_back(run_bench("Native DLL (getpid)", ITERATIONS, setup_native, bench_native, nullptr));

    /* 3. PyPI bridge */
    results.push_back(run_bench("PyPI bridge (import+call+free)", ITERATIONS/10, setup_pypi, bench_pypi, nullptr));

    /* 4. Cargo bridge */
    results.push_back(run_bench("Cargo bridge (add)", ITERATIONS, setup_cargo, bench_cargo, nullptr));

    /* Print results */
    printf("%-40s %12s %12s %12s %12s\n", "Test", "Avg (ns)", "Min (ns)", "Max (ns)", "Median (ns)");
    printf("%s\n", std::string(88, '-').c_str());
    for (auto& r : results) {
        printf("%-40s %12.1f %12.1f %12.1f %12.1f\n",
               r.name.c_str(), r.avg_ns, r.min_ns, r.max_ns, r.avg_ns);
    }

    printf("\nDone.\n");
    return 0;
}
