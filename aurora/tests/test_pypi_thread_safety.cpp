/* Phase 1: GIL & Thread Safety — release GIL once, never re-save */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

extern "C" {
    int aurora_py_ensure_initialized(void);
    void* aurora_py_get_api(const char*);
}

static void log(const char* m) { fprintf(stderr, "[TEST] %s\n", m); }

static auto GIL_E = (unsigned int(*)(void))nullptr;
static auto GIL_R = (void(*)(unsigned int))nullptr;
static auto PD = (void(*)(void*))nullptr;
static auto PIM = (void*(*)(const char*))nullptr;
static auto PUFS = (void*(*)(const char*))nullptr;
static auto PGAS = (void*(*)(void*,const char*))nullptr;
static auto PCO = (void*(*)(void*,void*))nullptr;
static auto PTN = (void*(*)(int))nullptr;
static auto PTSI = (int(*)(void*,int,void*))nullptr;
static auto POS = (void*(*)(void*))nullptr;
static auto PU8 = (const char*(*)(void*))nullptr;

/* GIL released once at top — main & worker threads use GIL_E/GIL_R */

static int test_basic_gil() {
    unsigned int gs = GIL_E();
    GIL_R(gs);
    return 1;
}

static int test_single_import() {
    unsigned int gs = GIL_E();
    void* mod = PIM("markdown");
    int ok = (mod != nullptr);
    if (mod) PD(mod);
    GIL_R(gs);
    return ok;
}

static int test_mt_import_stress() {
    const int N = 1000;
    std::atomic<int> err{0};
    std::vector<std::thread> thr;
    for (int i = 0; i < 4; i++)
        thr.emplace_back([&]() {
            for (int j = 0; j < N; j++) {
                unsigned int g = GIL_E();
                void* m = PIM("markdown");
                if (!m) err++;
                else PD(m);
                GIL_R(g);
            }
        });
    for (auto& t : thr) t.join();
    return err == 0;
}

static int test_mt_markdown_calls() {
    unsigned int gs = GIL_E();
    void* mod = PIM("markdown");
    GIL_R(gs);
    if (!mod) return 0;

    const int NTHR = 4, NCALLS = 200;
    std::atomic<int> err{0};
    std::vector<std::thread> thr;
    for (int i = 0; i < NTHR; i++)
        thr.emplace_back([&]() {
            for (int j = 0; j < NCALLS; j++) {
                unsigned int g = GIL_E();
                void* f = PGAS(mod, "markdown");
                if (!f) { err++; GIL_R(g); continue; }
                void* s = PUFS("# Hello");
                if (!s) { PD(f); err++; GIL_R(g); continue; }
                void* tup = PTN(1);
                if (!tup) { PD(s); PD(f); err++; GIL_R(g); continue; }
                PTSI(tup, 0, s);
                void* r = PCO(f, tup);
                PD(tup); PD(f);
                if (!r) { err++; GIL_R(g); continue; }
                void* so = POS(r);
                if (!so) { PD(r); err++; GIL_R(g); continue; }
                const char* u = PU8(so);
                if (!u || strstr(u, "<h1>") == nullptr) err++;
                PD(so); PD(r);
                GIL_R(g);
            }
        });
    for (auto& t : thr) t.join();
    gs = GIL_E(); PD(mod); GIL_R(gs);
    return err == 0;
}

/* ── Bridge DLL tests ── */

typedef void* (*ImportFn)();
typedef void* (*Call1Fn)(void*, const char*, void*);
typedef void* (*StrFn)(const char*);
typedef char* (*ToCStrFn)(void*);
typedef void  (*FreeFn)(void*);
typedef void  (*FreeCStrFn)(const char*);

struct Bridge {
    HMODULE lib;
    ImportFn import_fn;
    Call1Fn call1_fn;
    StrFn str_fn;
    ToCStrFn to_cstr_fn;
    FreeFn free_fn;
    FreeCStrFn free_cstr_fn;
};

static bool load_bridge(const char* dll_path, const char* prefix, Bridge* b) {
    memset(b, 0, sizeof(*b));
    b->lib = LoadLibraryA(dll_path);
    if (!b->lib) return false;
    char buf[256];
    snprintf(buf, sizeof(buf), "%s_import", prefix); b->import_fn = (ImportFn)GetProcAddress(b->lib, buf);
    snprintf(buf, sizeof(buf), "%s_call1", prefix);  b->call1_fn = (Call1Fn)GetProcAddress(b->lib, buf);
    snprintf(buf, sizeof(buf), "%s_str", prefix);    b->str_fn = (StrFn)GetProcAddress(b->lib, buf);
    snprintf(buf, sizeof(buf), "%s_to_cstr", prefix); b->to_cstr_fn = (ToCStrFn)GetProcAddress(b->lib, buf);
    snprintf(buf, sizeof(buf), "%s_free", prefix);   b->free_fn = (FreeFn)GetProcAddress(b->lib, buf);
    snprintf(buf, sizeof(buf), "%s_free_cstr", prefix); b->free_cstr_fn = (FreeCStrFn)GetProcAddress(b->lib, buf);
    return b->import_fn && b->call1_fn && b->str_fn && b->to_cstr_fn && b->free_fn && b->free_cstr_fn;
}

static int test_bridge_single(Bridge* br) {
    unsigned int gs = GIL_E();
    void* mod = br->import_fn();
    if (!mod) { GIL_R(gs); return 0; }
    void* s = br->str_fn("# Hello");
    if (!s) { br->free_fn(mod); GIL_R(gs); return 0; }
    void* r = br->call1_fn(mod, "markdown", s);
    if (!r) { br->free_fn(mod); GIL_R(gs); return 0; }
    char* h = br->to_cstr_fn(r);
    int ok = (h && strstr(h, "<h1>") != nullptr);
    if (h) br->free_cstr_fn(h);
    br->free_fn(r);
    br->free_fn(mod);
    GIL_R(gs);
    return ok;
}

static int test_bridge_mt_stress(Bridge* br) {
    const int NTHR = 8, NCALLS = 100;
    std::atomic<int> err{0};
    std::vector<std::thread> thr;
    for (int i = 0; i < NTHR; i++)
        thr.emplace_back([br, &err]() {
            for (int j = 0; j < NCALLS; j++) {
                void* m = br->import_fn();
                if (!m) { err++; continue; }
                void* s = br->str_fn("**bold**");
                if (!s) { br->free_fn(m); err++; continue; }
                void* r = br->call1_fn(m, "markdown", s);
                if (!r) { br->free_fn(m); err++; continue; }
                char* h = br->to_cstr_fn(r);
                if (!h) { br->free_fn(r); br->free_fn(m); err++; continue; }
                if (strstr(h, "<strong>") == nullptr && strstr(h, "<b>") == nullptr) err++;
                br->free_cstr_fn(h);
                br->free_fn(r); br->free_fn(m);  /* s was stolen by call1, don't free it */
            }
        });
    for (auto& t : thr) t.join();
    return err == 0;
}

static int test_bridge_shared_mod(Bridge* br) {
    unsigned int gs = GIL_E();
    void* mod = br->import_fn();
    GIL_R(gs);
    if (!mod) return 0;

    const int NTHR = 4, NCALLS = 200;
    std::atomic<int> err{0};
    std::vector<std::thread> thr;
    for (int i = 0; i < NTHR; i++)
        thr.emplace_back([br, mod, &err]() {
            for (int j = 0; j < NCALLS; j++) {
                void* s = br->str_fn("- item");
                if (!s) { err++; continue; }
                void* r = br->call1_fn(mod, "markdown", s);
                if (!r) { err++; continue; }
                char* h = br->to_cstr_fn(r);
                if (!h) { br->free_fn(r); err++; continue; }
                if (strstr(h, "<li>") == nullptr) err++;
                br->free_cstr_fn(h);
                br->free_fn(r); /* s was stolen by call1, don't free it */
            }
        });
    for (auto& t : thr) t.join();
    gs = GIL_E(); br->free_fn(mod); GIL_R(gs);
    return err == 0;
}

/* ── Main ── */

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    log("========================================");
    log("  Phase 1: GIL & Thread Safety");
    log("========================================");

    if (!aurora_py_ensure_initialized()) {
        log("FATAL: Python init");
        return 1;
    }

    GIL_E = (unsigned int(*)(void))aurora_py_get_api("PyGILState_Ensure");
    GIL_R = (void(*)(unsigned int))aurora_py_get_api("PyGILState_Release");
    PD = (void(*)(void*))aurora_py_get_api("Py_DecRef");
    PIM = (void*(*)(const char*))aurora_py_get_api("PyImport_ImportModule");
    PUFS = (void*(*)(const char*))aurora_py_get_api("PyUnicode_FromString");
    PGAS = (void*(*)(void*,const char*))aurora_py_get_api("PyObject_GetAttrString");
    PCO = (void*(*)(void*,void*))aurora_py_get_api("PyObject_CallObject");
    PTN = (void*(*)(int))aurora_py_get_api("PyTuple_New");
    PTSI = (int(*)(void*,int,void*))aurora_py_get_api("PyTuple_SetItem");
    POS = (void*(*)(void*))aurora_py_get_api("PyObject_Str");
    PU8 = (const char*(*)(void*))aurora_py_get_api("PyUnicode_AsUTF8");

    /* Release GIL once — all threads acquire/release via PyGILState */
    auto save = (void*(*)())aurora_py_get_api("PyEval_SaveThread");
    if (save) save();
    log("GIL released (single release, all threads use GIL_E/GIL_R)");

    int pass = 0, fail = 0;
#define T(fn, name) do { \
    int idx = pass + fail + 1; \
    fprintf(stderr, "[TEST] %2d. %s ... ", idx, name); \
    if (fn()) { pass++; fprintf(stderr, "PASS\n"); } \
    else { fail++; fprintf(stderr, "FAIL\n"); } \
} while(0)

    /* Direct Python C API */
    T(test_basic_gil, "GIL acquire/release (main thread)");
    T(test_single_import, "Import markdown (single thread)");
    T(test_mt_import_stress, "Multi-threaded import stress (4x1000)");
    T(test_mt_markdown_calls, "Multi-threaded markdown calls (4x200)");

    /* Bridge DLL */
    Bridge bridge;
    if (load_bridge("build/Debug/markdown_pypi/markdown_pypi.dll", "markdown", &bridge)) {
        T([&](){ return test_bridge_single(&bridge); }, "Bridge: single-threaded markdown");
        T([&](){ return test_bridge_mt_stress(&bridge); }, "Bridge: multi-threaded stress (8x100)");
        T([&](){ return test_bridge_shared_mod(&bridge); }, "Bridge: shared module (4x200)");
        FreeLibrary(bridge.lib);
    } else {
        log("  SKIP: markdown_pypi.dll not found");
    }

    log("========================================");
    if (fail) { char b[64]; snprintf(b, sizeof(b), "  FAILED: %d/%d tests", fail, pass+fail); log(b); }
    else      { char b[64]; snprintf(b, sizeof(b), "  ALL PASSED: %d/%d", pass, pass+fail); log(b); }
    log("========================================");
    return fail > 0 ? 1 : 0;
}
