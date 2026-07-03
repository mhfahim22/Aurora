#include "std/test.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

/* ════════════════════════════════════════════════════════════
   Testing Framework — Unit, Integration, Widget, Bench,
   Snapshot, Coverage
   ════════════════════════════════════════════════════════════ */

static std::mutex g_mtx;

/* ── Test Registry ── */

struct TestCase {
    std::string name;
    void (*fn)(void);
    bool passed;
    std::string error;
};

struct TestSuite {
    std::string name;
    std::vector<TestCase> cases;
};

struct TestState {
    std::vector<TestSuite> suites;
    std::string current_suite;
    int pass_count;
    int fail_count;
    int assert_count;

    void (*setup_fn)(void);
    void (*teardown_fn)(void);

    // bench
    double bench_accum;
    double bench_start;

    // coverage
    bool coverage_on;
    std::map<std::string, int> coverage_hits;
};

static TestState g_state;

/* ── Helpers ── */

#ifdef _WIN32
static double now_sec(void) {
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return double(count.QuadPart) / double(freq.QuadPart);
}
#else
static double now_sec(void) {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return double(tv.tv_sec) + double(tv.tv_usec) / 1e6;
}
#endif

/* ── Test Registration ── */

int aurora_test_describe(const char* suite, void (*fn)(void)) {
    std::lock_guard<std::mutex> lock(g_mtx);
    g_state.current_suite = suite;
    g_state.suites.push_back({suite, {}});
    if (fn) fn();
    return 1;
}

int aurora_test_it(const char* name, void (*fn)(void)) {
    std::lock_guard<std::mutex> lock(g_mtx);
    if (g_state.suites.empty()) return 0;
    g_state.suites.back().cases.push_back({name, fn, true, ""});
    return 1;
}

int aurora_test_run(void) {
    std::lock_guard<std::mutex> lock(g_mtx);
    g_state.pass_count = 0;
    g_state.fail_count = 0;
    g_state.assert_count = 0;

    for (auto& suite : g_state.suites) {
        printf("[suite] %s\n", suite.name.c_str());
        for (auto& tc : suite.cases) {
            if (g_state.setup_fn) g_state.setup_fn();
            tc.passed = true;
            tc.error = "";
            g_state.assert_count = 0;
            try {
                if (tc.fn) tc.fn();
            } catch (const std::exception& e) {
                tc.passed = false;
                tc.error = e.what();
            } catch (...) {
                tc.passed = false;
                tc.error = "unknown exception";
            }
            if (g_state.teardown_fn) g_state.teardown_fn();

            if (tc.passed) {
                g_state.pass_count++;
                printf("  ✓ %s\n", tc.name.c_str());
            } else {
                g_state.fail_count++;
                printf("  ✗ %s: %s\n", tc.name.c_str(), tc.error.c_str());
            }

            if (g_state.coverage_on)
                g_state.coverage_hits[suite.name + "::" + tc.name]++;
        }
    }
    printf("\n%d passed, %d failed out of %d\n",
           g_state.pass_count, g_state.fail_count,
           g_state.pass_count + g_state.fail_count);
    return g_state.fail_count == 0 ? 1 : 0;
}

/* ── Assertions ── */

void aurora_test_assert(int condition, const char* msg) {
    g_state.assert_count++;
    if (!condition) {
        g_state.fail_count++;
        fprintf(stderr, "  ASSERT FAIL: %s\n", msg ? msg : "");
    }
}

void aurora_test_assert_eq_int(int a, int b, const char* msg) {
    g_state.assert_count++;
    if (a != b) {
        g_state.fail_count++;
        fprintf(stderr, "  ASSERT FAIL: %s — expected %d, got %d\n", msg ? msg : "", a, b);
    }
}

void aurora_test_assert_eq_float(double a, double b, double epsilon, const char* msg) {
    g_state.assert_count++;
    if (fabs(a - b) > epsilon) {
        g_state.fail_count++;
        fprintf(stderr, "  ASSERT FAIL: %s — expected %f, got %f (eps %f)\n", msg ? msg : "", a, b, epsilon);
    }
}

void aurora_test_assert_eq_str(const char* a, const char* b, const char* msg) {
    g_state.assert_count++;
    if (strcmp(a ? a : "", b ? b : "") != 0) {
        g_state.fail_count++;
        fprintf(stderr, "  ASSERT FAIL: %s — expected \"%s\", got \"%s\"\n", msg ? msg : "", b ? b : "", a ? a : "");
    }
}

void aurora_test_assert_true(int condition, const char* msg) {
    g_state.assert_count++;
    if (!condition) {
        g_state.fail_count++;
        fprintf(stderr, "  ASSERT FAIL: %s — expected true\n", msg ? msg : "");
    }
}

void aurora_test_assert_false(int condition, const char* msg) {
    g_state.assert_count++;
    if (condition) {
        g_state.fail_count++;
        fprintf(stderr, "  ASSERT FAIL: %s — expected false\n", msg ? msg : "");
    }
}

void aurora_test_assert_null(const void* ptr, const char* msg) {
    g_state.assert_count++;
    if (ptr != nullptr) {
        g_state.fail_count++;
        fprintf(stderr, "  ASSERT FAIL: %s — expected null\n", msg ? msg : "");
    }
}

/* ── Integration Tests ── */

void aurora_test_setup(void (*fn)(void)) {
    g_state.setup_fn = fn;
}

void aurora_test_teardown(void (*fn)(void)) {
    g_state.teardown_fn = fn;
}

int aurora_test_integration(const char* name, void (*fn)(void)) {
    return aurora_test_it(name, fn);
}

/* ── Widget Tests ── */

struct WidgetEntry {
    std::string id;
    void* ptr;
};
static std::vector<WidgetEntry> g_widgets;

int aurora_test_widget(const char* name, void (*fn)(void)) {
    return aurora_test_it(name, fn);
}

void* aurora_test_find_widget(const char* id) {
    std::lock_guard<std::mutex> lock(g_mtx);
    for (auto& w : g_widgets) {
        if (w.id == id) return w.ptr;
    }
    return nullptr;
}

int aurora_test_click(const char* id) {
    std::lock_guard<std::mutex> lock(g_mtx);
    for (auto& w : g_widgets) {
        if (w.id == id) return 1;
    }
    return 0;
}

/* ── Benchmark Tests ── */

int aurora_test_bench(const char* name, void (*fn)(void), int iterations) {
    if (!fn) return 0;
    double start = now_sec();
    for (int i = 0; i < iterations; i++) fn();
    double elapsed = now_sec() - start;
    double ops = elapsed > 0 ? iterations / elapsed : 0;
    printf("[bench] %s: %d iterations in %.3fs — %.0f ops/sec\n",
           name, iterations, elapsed, ops);
    return 1;
}

void aurora_test_bench_start(void) {
    g_state.bench_start = now_sec();
}

void aurora_test_bench_end(void) {
    g_state.bench_accum += now_sec() - g_state.bench_start;
}

double aurora_test_bench_result(void) {
    return g_state.bench_accum;
}

/* ── Snapshot Tests ── */

static std::string snap_dir = ".snapshots";

static std::string snap_path(const char* name) {
    return snap_dir + "/" + name + ".snap";
}

int aurora_test_snapshot(const char* name, const char* value) {
    std::string path = snap_path(name);
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        // First run — create snapshot
        f = fopen(path.c_str(), "wb");
        if (!f) return 0;
        fwrite(value, 1, strlen(value), f);
        fclose(f);
        printf("[snap] created %s\n", name);
        return 1;
    }
    // Read existing
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string existing((size_t)len, '\0');
    fread(&existing[0], 1, (size_t)len, f);
    fclose(f);
    // Compare
    if (existing == value) {
        printf("[snap] ✓ %s\n", name);
        return 1;
    }
    printf("[snap] ✗ %s — snapshot mismatch\n", name);
    g_state.fail_count++;
    return 0;
}

int aurora_test_snapshot_update(const char* name, const char* value) {
    std::string path = snap_path(name);
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return 0;
    fwrite(value, 1, strlen(value), f);
    fclose(f);
    printf("[snap] updated %s\n", name);
    return 1;
}

int aurora_test_snapshot_delete(const char* name) {
    std::string path = snap_path(name);
    if (remove(path.c_str()) == 0) {
        printf("[snap] deleted %s\n", name);
        return 1;
    }
    return 0;
}

/* ── Coverage ── */

int aurora_test_coverage_start(void) {
    g_state.coverage_on = true;
    g_state.coverage_hits.clear();
    return 1;
}

int aurora_test_coverage_stop(void) {
    g_state.coverage_on = false;
    return 1;
}

const char* aurora_test_coverage_report(void) {
    static std::string report;
    std::ostringstream oss;
    oss << "Coverage Report:\n";
    for (auto& [test, hits] : g_state.coverage_hits) {
        oss << "  " << test << ": " << hits << " runs\n";
    }
    report = oss.str();
    return report.c_str();
}

/* ── Results ── */

int aurora_test_pass_count(void) {
    return g_state.pass_count;
}

int aurora_test_fail_count(void) {
    return g_state.fail_count;
}

int aurora_test_total_count(void) {
    return g_state.pass_count + g_state.fail_count;
}

const char* aurora_test_results(void) {
    static std::string result;
    std::ostringstream oss;
    oss << g_state.pass_count << " passed, "
        << g_state.fail_count << " failed out of "
        << (g_state.pass_count + g_state.fail_count);
    result = oss.str();
    return result.c_str();
}
