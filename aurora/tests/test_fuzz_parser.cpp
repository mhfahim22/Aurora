#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <chrono>
#include <cassert>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

static int g_total = 0;
static int g_crashes = 0;
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

static std::string generate_random_script() {
    static const char* bodies[] = {
        "x = 1",
        "x = 3.14",
        "x = \"hello\"",
        "if x == 1\n    y = 2",
        "while x < 10\n    x = x + 1",
        "for i in 10\n    total = total + i",
        "function foo()\n    return 1",
        "output(42)",
        "arr = [1, 2, 3]",
        "class Foo\n    pass",
        "match x\n    _ -> 1",
        "x = y",
        "x = 1 + 2",
        "x = 1 * 2 + 3",
        "obj.foo()",
        "arr[0]",
        "return 1",
        "break",
        "continue",
        "x = 1 + 2 * 3 - 4 / 5",
    };
    static const char* prefixes[] = {
        "", "@stack\n", "@arena\n", "@raii\n", "@gc\n",
    };

    std::string result;
    int lines = (rand() % 8) + 1;
    for (int l = 0; l < lines; l++) {
        if (l > 0) result += "\n";
        int choice = rand() % 5;
        if (choice == 0)
            result += prefixes[rand() % 5];
        result += bodies[rand() % (sizeof(bodies) / sizeof(bodies[0]))];
    }
    return result;
}

static int compile_and_check(const std::string& src) {
    /* Generate unique temp file path */
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
    snprintf(tmpfile, sizeof(tmpfile), "%s/aurora_fuzz_%d.aura", tmpdir, rand());

    FILE* f = fopen(tmpfile, "w");
    if (!f) return -1;
    fprintf(f, "%s\n", src.c_str());
    fclose(f);

    /* Run aurorac on it */
#ifdef _WIN32
    std::string cmd = "\"" + g_aurorac_path + "\" \"" + tmpfile + "\" --emit-obj 2>nul";
#else
    std::string cmd = "\"" + g_aurorac_path + "\" \"" + tmpfile + "\" --emit-obj 2>/dev/null";
#endif
    int rc = system(cmd.c_str());

    remove(tmpfile);
    return rc;
}

static void fuzz_once() {
    std::string src = generate_random_script();
    int rc = compile_and_check(src);
    if (rc == -1073741819 || rc == -1073741571 || rc == 0xC0000005) {
        g_crashes++;
        printf("  CRASH (rc=%d) on input:\n---\n%s\n---\n", rc, src.c_str());
    }
}

int main(int argc, char** argv) {
    int iterations = 5000;
    if (argc > 1) iterations = atoi(argv[1]);
    if (iterations < 100) iterations = 100;

    g_aurorac_path = find_aurorac();
    if (g_aurorac_path.empty()) {
        printf("ERROR: aurorac not found\n");
        return 1;
    }

    printf("=== Parser Fuzz Test ===\n");
    printf("  aurorac:     %s\n", g_aurorac_path.c_str());
    printf("  Iterations:  %d\n", iterations);
    printf("  Seed:        42\n");
    printf("\n");

    srand(42);
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        fuzz_once();
        g_total++;
        if (g_total % 1000 == 0)
            printf("  %d / %d ...\n", g_total, iterations);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    printf("\nResults:\n");
    printf("  Total:     %d\n", g_total);
    printf("  Crashes:   %d\n", g_crashes);
    printf("  Time:      %lld ms\n", (long long)elapsed_ms);
    printf("  Speed:     %d inputs/sec\n", (int)(g_total * 1000.0 / (elapsed_ms + 1)));
    printf("  Verdict:   %s\n", (g_crashes == 0) ? "PASS" : "FAIL");

    return (g_crashes == 0) ? 0 : 1;
}
