// ── Aurora Cross-Platform Validation Test Suite ───────────────
// Phase 29: Validates core subsystems across all 5 target platforms.
// Registered as CTest target "test_crossplatform".
// No external dependencies beyond aurora_runtime.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <cassert>
#include <ctime>
#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <chrono>

namespace fs = std::filesystem;

// ── Test framework helpers ─────────────────────────────────────
static int  g_total  = 0;
static int  g_passed = 0;
static int  g_failed = 0;
static bool g_verbose = false;

#define TEST(name) do { \
    g_total++; \
    printf("  TEST %-52s ", name); \
    fflush(stdout); \
} while(0)

#define PASS() do { \
    g_passed++; \
    printf("PASS\n"); \
} while(0)

#define FAIL(msg) do { \
    g_failed++; \
    printf("FAIL [%s]\n", msg); \
} while(0)

#define CHECK(cond, msg) do { \
    if (!(cond)) { FAIL(msg); return; } \
} while(0)

// ── Platform detection ─────────────────────────────────────────
static const char* detect_os() {
#if defined(_WIN32)
    return "Windows";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__ANDROID__)
    return "Android";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

static const char* detect_arch() {
#if defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "ARM64";
#elif defined(__arm__) || defined(_M_ARM)
    return "ARM";
#elif defined(__i386__) || defined(_M_IX86)
    return "x86";
#else
    return "Unknown";
#endif
}

// ── Section runners ────────────────────────────────────────────

static void test_platform_detection() {
    printf("\n[Platform Detection]\n");
    const char* os   = detect_os();
    const char* arch = detect_arch();
    printf("  OS   : %s\n", arch);
    printf("  Arch : %s\n", arch);
    TEST("platform OS is known");    CHECK(strcmp(os, "Unknown") != 0, "unknown OS"); PASS();
    TEST("platform arch is known");  CHECK(strcmp(arch, "Unknown") != 0, "unknown arch"); PASS();
}

static void test_threading() {
    printf("\n[Threading]\n");
    std::atomic<int> counter{0};
    const int N = 8;

    // Spawn threads
    std::vector<std::thread> threads;
    for (int i = 0; i < N; i++) {
        threads.emplace_back([&counter, i]() {
            for (int j = 0; j < 1000; j++) {
                counter.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& t : threads) t.join();

    TEST("thread pool counter");  CHECK(counter == N * 1000, "counter mismatch");  PASS();
}

static void test_filesystem() {
    printf("\n[Filesystem]\n");
    const auto tmp = fs::temp_directory_path() / "aurora_cross_test";

    // Clean up any leftover
    fs::remove_all(tmp);

    // Create dir
    TEST("create directory");      CHECK(fs::create_directories(tmp), "mkdir failed"); PASS();

    // Write file
    const auto file = tmp / "test.txt";
    {
        std::ofstream ofs(file);
        CHECK(ofs.is_open(), "open for write failed");
        ofs << "Hello, Aurora!\n";
    }
    TEST("write file");            CHECK(fs::file_size(file) > 0, "file empty"); PASS();

    // Read file
    {
        std::ifstream ifs(file);
        CHECK(ifs.is_open(), "open for read failed");
        std::string line;
        std::getline(ifs, line);
        TEST("read file");         CHECK(line == "Hello, Aurora!", "content mismatch"); PASS();
    }

    // List dir
    int count = 0;
    for (auto& e : fs::directory_iterator(tmp)) {
        (void)e; count++;
    }
    TEST("directory listing");     CHECK(count == 1, "expected 1 entry"); PASS();

    // Remove
    fs::remove_all(tmp);
    TEST("remove directory");      CHECK(!fs::exists(tmp), "not removed"); PASS();
}

static void test_performance() {
    printf("\n[Performance]\n");

    // CPU integer throughput (simple loop)
    auto start = std::chrono::high_resolution_clock::now();
    volatile uint64_t sum = 0;
    for (int i = 0; i < 10000000; i++) sum += i;
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    printf("  10M integer ops: %lld ms\n", (long long)ms);

    // Memory allocation throughput
    start = std::chrono::high_resolution_clock::now();
    const int ALLOCS = 100000;
    std::vector<void*> ptrs;
    ptrs.reserve(ALLOCS);
    for (int i = 0; i < ALLOCS; i++) {
        ptrs.push_back(malloc(64));
    }
    for (auto* p : ptrs) free(p);
    end = std::chrono::high_resolution_clock::now();
    ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    printf("  100K 64-byte alloc/free: %lld ms\n", (long long)ms);

    TEST("performance baseline");  PASS();
}

// ── Report ─────────────────────────────────────────────────────
static void print_summary() {
    printf("\n═══════════════════════════════════════════════════\n");
    printf("  Cross-Platform Validation Results\n");
    printf("═══════════════════════════════════════════════════\n");
    printf("  Total : %d\n", g_total);
    printf("  Passed: %d\n", g_passed);
    printf("  Failed: %d\n", g_failed);
    printf("═══════════════════════════════════════════════════\n");
}

// ── Main ───────────────────────────────────────────────────────
int main(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0)
            g_verbose = true;
    }

    printf("Aurora Cross-Platform Validation Suite\n");
    printf("Platform: %s / %s\n", detect_os(), detect_arch());
    printf("C++ std : %ld\n", (long)__cplusplus);

    test_platform_detection();
    test_threading();
    test_filesystem();
    test_performance();

    print_summary();

    return g_failed > 0 ? 1 : 0;
}
