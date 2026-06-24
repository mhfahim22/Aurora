#include <cstdio>
#include <cstdlib>
#include <string>
#include <chrono>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

struct BenchResult {
    const char* name;
    double ns_per_op;
    double ops_per_sec;
};

static std::string g_aurorac_path;

static std::string find_aurorac() {
#ifdef _WIN32
    const char* paths[] = {
        "build/Release/aurorac.exe",
        "build/Debug/aurorac.exe",
        "../build/Release/aurorac.exe",
        "../build/Debug/aurorac.exe",
        "aurorac.exe",
    };
#else
    const char* paths[] = {
        "build/Release/aurorac",
        "build/Debug/aurorac",
        "../build/Release/aurorac",
        "../build/Debug/aurorac",
        "aurorac",
    };
#endif
    for (auto p : paths) {
        FILE* f = fopen(p, "rb");
        if (f) { fclose(f); return p; }
    }
    return "";
}

static std::string make_temp_file(const std::string& content) {
    static int counter = 0;
    char tmpfile[1024];
    char tmpdir[1024];
#ifdef _WIN32
    tmpdir[0] = 0;
    DWORD len = GetTempPathA(sizeof(tmpdir), tmpdir);
    if (len == 0 || len >= sizeof(tmpdir)) strcpy(tmpdir, ".");
#else
    const char* td = getenv("TMPDIR");
    if (!td) td = "/tmp";
    snprintf(tmpdir, sizeof(tmpdir), "%s", td);
#endif
    snprintf(tmpfile, sizeof(tmpfile), "%s/aurora_bench_%d.aura", tmpdir, counter++);
    FILE* f = fopen(tmpfile, "w");
    if (f) { fprintf(f, "%s\n", content.c_str()); fclose(f); }
    return tmpfile;
}

static BenchResult run_bench(const char* name, int iterations,
                              const std::string& source) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        std::string tmp = make_temp_file(source);
#ifdef _WIN32
        std::string cmd = "\"" + g_aurorac_path + "\" \"" + tmp + "\" --emit-obj 2>nul";
#else
        std::string cmd = "\"" + g_aurorac_path + "\" \"" + tmp + "\" --emit-obj 2>/dev/null";
#endif
        system(cmd.c_str());
        remove(tmp.c_str());
    }

    auto end = std::chrono::high_resolution_clock::now();
    double ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    BenchResult r;
    r.name = name;
    r.ns_per_op = ns / iterations;
    r.ops_per_sec = (iterations * 1e9) / ns;
    return r;
}

int main() {
    g_aurorac_path = find_aurorac();
    if (g_aurorac_path.empty()) {
        printf("ERROR: aurorac compiler not found\n");
        return 1;
    }

    printf("=== Compiler Performance Benchmarks ===\n");
    printf("  aurorac: %s\n\n", g_aurorac_path.c_str());

    std::vector<BenchResult> results;

    /* Small program */
    results.push_back(run_bench("small program", 100,
        "function add(a, b)\n    return a + b\nx = add(1, 2)\noutput(x)"));

    /* Medium program with classes */
    results.push_back(run_bench("class + inheritance", 100,
        "class Animal\n    function speak()\n        return \"?\"\n"
        "class Dog extends Animal\n    function speak()\n        return \"woof\"\n"
        "d = Dog()\noutput(d.speak())"));

    /* Program with loops and arrays */
    results.push_back(run_bench("loops + arrays", 100,
        "total = 0\nfor i in 100\n    for j in 50\n        total = total + 1\noutput(total)"));

    /* Recursive Fibonacci */
    results.push_back(run_bench("recursive fibonacci", 100,
        "function fib(n)\n    if n <= 1\n        return n\n    return fib(n-1) + fib(n-2)\noutput(fib(20))"));

    /* Large source file */
    {
        std::string large;
        large += "x = 1\n";
        for (int i = 0; i < 50; i++)
            large += "function f" + std::to_string(i) + "()\n    return " + std::to_string(i) + "\n";
        large += "total = 0\nfor i in 50\ntotal = total + f" + std::to_string(0) + "()\noutput(total)";
        results.push_back(run_bench("50 empty functions", 100, large));
    }

    printf("%-35s %12s %16s\n", "Benchmark", "ns/op", "ops/sec");
    printf("%s\n", std::string(65, '-').c_str());
    for (const auto& r : results) {
        printf("%-35s %10.1f ns %14.0f\n", r.name, r.ns_per_op, r.ops_per_sec);
    }

    return 0;
}
