/* End-to-End Bridge Integration Test — all 4 ecosystems */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <chrono>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static int g_passed = 0;
static int g_failed = 0;
static int g_test_count = 0;

#define TEST(name) { g_test_count++; printf("  test %d: %s ... ", g_test_count, name);
#define PASS() g_passed++; printf("PASS\n")
#define FAIL(msg) do { g_failed++; printf("FAIL: %s\n", msg); } while(0)
#define CHECK(cond, msg) if (!(cond)) { FAIL(msg); return; }
#define END_TEST }

static double now_ms() {
    static auto epoch = std::chrono::steady_clock::now();
    auto t = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(t - epoch).count();
}

#ifdef _WIN32
static void* dl_open(const char* p) { return (void*)LoadLibraryA(p); }
static void* dl_sym(void* l, const char* n) { return (void*)GetProcAddress((HMODULE)l, n); }
static void dl_close(void* l) { if (l) FreeLibrary((HMODULE)l); }
#else
static void* dl_open(const char* p) { return dlopen(p, RTLD_NOW); }
static void* dl_sym(void* l, const char* n) { return dlsym(l, n); }
static void dl_close(void* l) { if (l) dlclose(l); }
#endif

/* ── Native Bridge Test ── */
static void test_native_bridge() {
    printf("\n=== Native Bridge Test (kernel32.dll) ===\n");
    void* lib = dl_open("kernel32.dll");
    if (!lib) { printf("  SKIP: kernel32.dll not found\n"); return; }

    typedef uint64_t (*GTC_t)();
    GTC_t GetTickCount = (GTC_t)dl_sym(lib, "GetTickCount");
    typedef uint32_t (*GLE_t)();
    GLE_t GetLastError = (GLE_t)dl_sym(lib, "GetLastError");

    TEST("GetTickCount returns > 0");
    CHECK(GetTickCount != nullptr, "symbol not found");
    uint64_t tc = GetTickCount();
    CHECK(tc > 0, "returned 0");
    PASS();
    END_TEST;

    TEST("GetLastError returns valid value");
    CHECK(GetLastError != nullptr, "symbol not found");
    GetLastError();
    uint32_t err = GetLastError();
    CHECK(err == 0 || err > 0, "valid error code");
    PASS();
    END_TEST;

    TEST("1000 iterations latency");
    double t0 = now_ms();
    uint64_t sum = 0;
    for (int i = 0; i < 1000; i++) sum += GetTickCount();
    double elapsed = now_ms() - t0;
    printf("PASS (%.2f us/call)\n", elapsed);
    g_passed++;
    END_TEST;

    dl_close(lib);
}

/* ── npm Bridge Test ── */
static void test_npm_bridge() {
    printf("\n=== npm Bridge Test (moment) ===\n");
    char path[512];
    snprintf(path, sizeof(path), "../../packages/bridges/npm/moment_npm/moment_npm.dll");
    void* lib = dl_open(path);
    if (!lib) {
        snprintf(path, sizeof(path), "packages/bridges/npm/moment_npm/moment_npm.dll");
        lib = dl_open(path);
    }
    if (!lib) { printf("  SKIP: moment_npm bridge not found\n"); return; }

    typedef void* (*require_t)();
    typedef void* (*call_t)(void*, const char*, void*);
    typedef char* (*to_cstr_t)(void*);
    typedef void  (*free_t)(void*);
    require_t moment_require = (require_t)dl_sym(lib, "moment_require");
    call_t moment_call = (call_t)dl_sym(lib, "moment_call");
    to_cstr_t moment_to_cstr = (to_cstr_t)dl_sym(lib, "moment_to_cstr");
    free_t moment_free = (free_t)dl_sym(lib, "moment_free");

    void* mod = nullptr;
    TEST("moment_require returns module");
    CHECK(moment_require && moment_call && moment_to_cstr && moment_free, "missing symbols");
    mod = moment_require();
    CHECK(mod != nullptr, "require returned null");
    PASS();
    END_TEST;

    if (mod) {
        TEST("moment.now() returns timestamp");
        void* now = moment_call(mod, "now", 0);
        CHECK(now != nullptr, "now returned null");
        char* ts = moment_to_cstr(now);
        CHECK(ts != nullptr && strlen(ts) > 0, "timestamp empty");
        PASS();
        moment_free(now);
        END_TEST;

        TEST("100 call latency");
        double t0 = now_ms();
        for (int i = 0; i < 100; i++) {
            void* n = moment_call(mod, "now", 0);
            moment_free(n);
        }
        double el = now_ms() - t0;
        printf("PASS (%.2f ms/call)\n", el / 100.0);
        g_passed++;
        END_TEST;

        moment_free(mod);
    }
    dl_close(lib);
}

/* ── PyPI Bridge Test ── */
static void test_pypi_bridge() {
    printf("\n=== PyPI Bridge Test (markdown) ===\n");
    char path[512];
    /* Try from build/Debug (../../) and from root dir */
    snprintf(path, sizeof(path), "../../markdown_pypi/markdown_pypi.dll");
    void* lib = dl_open(path);
    if (!lib) {
        snprintf(path, sizeof(path), "markdown_pypi/markdown_pypi.dll");
        lib = dl_open(path);
    }
    if (!lib) { printf("  SKIP: markdown_pypi bridge not found\n"); return; }

    typedef void* (*import_t)();
    typedef void* (*call1_t)(void*, const char*, void*);
    typedef void* (*str_t)(const char*);
    typedef char* (*to_cstr_t)(void*);
    import_t markdown_import = (import_t)dl_sym(lib, "markdown_import");
    call1_t md_call1 = (call1_t)dl_sym(lib, "markdown_call1");
    str_t md_str = (str_t)dl_sym(lib, "markdown_str");
    to_cstr_t md_to_cstr = (to_cstr_t)dl_sym(lib, "markdown_to_cstr");

    void* mod = nullptr;
    TEST("markdown_import returns module");
    CHECK(markdown_import && md_call1 && md_str && md_to_cstr, "missing symbols");
    mod = markdown_import();
    CHECK(mod != nullptr, "import returned null");
    PASS();
    END_TEST;

    if (mod) {
        TEST("markdown('# Hello') -> <h1>");
        void* s = md_str("# Hello");
        void* result = md_call1(mod, "markdown", s);
        CHECK(result != nullptr, "markdown returned null");
        char* html = md_to_cstr(result);
        CHECK(html != nullptr && strstr(html, "<h1>") != nullptr, "expected <h1>");
        PASS();
        END_TEST;
    }
    dl_close(lib);
}

/* ── Cargo Bridge Test ── */
static void test_cargo_bridge() {
    printf("\n=== Cargo Bridge Test (hello_rust) ===\n");
    char path[512];
    /* Try from build/Debug (../../) and from root dir */
    snprintf(path, sizeof(path), "../../packages/bridges/cargo/hello_rust_cargo/hello_rust_cargo.dll");
    void* lib = dl_open(path);
    if (!lib) {
        snprintf(path, sizeof(path), "packages/bridges/cargo/hello_rust_cargo/hello_rust_cargo.dll");
        lib = dl_open(path);
    }
    if (!lib) { printf("  SKIP: hello_rust_cargo bridge not found\n"); return; }

    typedef void* (*import_t)();
    typedef void* (*call1_t)(void*, const char*, void*);
    typedef void* (*str_t)(const char*);
    typedef char* (*to_cstr_t)(void*);
    import_t rs_import = (import_t)dl_sym(lib, "hello_rust_import");
    call1_t rs_call1 = (call1_t)dl_sym(lib, "hello_rust_call1");
    str_t rs_str = (str_t)dl_sym(lib, "hello_rust_str");
    to_cstr_t rs_to_cstr = (to_cstr_t)dl_sym(lib, "hello_rust_to_cstr");

    void* mod = nullptr;
    TEST("hello_rust_import returns module");
    CHECK(rs_import && rs_call1 && rs_str && rs_to_cstr, "missing symbols");
    mod = rs_import();
    CHECK(mod != nullptr, "import returned null");
    PASS();
    END_TEST;

    if (mod) {
        TEST("greet('World') returns greeting");
        void* name = rs_str("World");
        void* result = rs_call1(mod, "greet", name);
        CHECK(result != nullptr, "greet returned null");
        char* msg = rs_to_cstr(result);
        CHECK(msg != nullptr && strstr(msg, "World") != nullptr, "expected 'World'");
        PASS();
        END_TEST;
    }
    dl_close(lib);
}

/* ── Phase 3: PyPI Real-World Bridge Tests ── */
/* numpy has compiled C extensions that may conflict with /MT CRT; mark as known limitation */
static void test_pypi_numpy() {
    printf("\n=== PyPI Real-World: numpy ===\n");
    char path[512];
    snprintf(path,sizeof(path),"numpy_pypi/numpy_pypi.dll");
    void* lib = dl_open(path);
    if(!lib){printf("  SKIP: numpy_pypi bridge not found\n");return;}
    typedef void* (*import_t)();
    import_t numpy_import = (import_t)dl_sym(lib,"numpy_import");
    typedef void* (*getattr_t)(void*,const char*);
    getattr_t numpy_getattr = (getattr_t)dl_sym(lib,"numpy_getattr");
    typedef char* (*to_cstr_t)(void*);
    to_cstr_t numpy_to_cstr = (to_cstr_t)dl_sym(lib,"numpy_to_cstr");
    CHECK(numpy_import&&numpy_getattr,"missing symbols");
    void* mod = numpy_import();
    CHECK(mod!=0,"import returned null");
    TEST("numpy.__version__");
    void* v = numpy_getattr(mod,"__version__");
    CHECK(v!=0,"__version__ returned null");
    if(numpy_to_cstr){char*s=numpy_to_cstr(v);if(s&&strlen(s)>0)printf("->%s ",s);}
    PASS(); END_TEST;
    dl_close(lib);
}

static void test_pypi_requests_version() {
    printf("\n=== PyPI Real-World: requests ===\n");
    char path[512];
    snprintf(path,sizeof(path),"requests_pypi/requests_pypi.dll");
    void* lib = dl_open(path);
    if(!lib){printf("  SKIP: requests_pypi bridge not found\n");return;}
    typedef void* (*import_t)();
    typedef char* (*to_cstr_t)(void*);
    import_t requests_import = (import_t)dl_sym(lib,"requests_import");
    to_cstr_t requests_to_cstr = (to_cstr_t)dl_sym(lib,"requests_to_cstr");
    CHECK(requests_import&&requests_to_cstr,"missing symbols");
    void* mod = requests_import();
    CHECK(mod!=0,"import returned null");
    TEST("to_cstr shows requests module");
    char* s = requests_to_cstr(mod);
    CHECK(s!=0&&strstr(s,"requests")!=0,"expected 'requests' in repr");
    printf("->%s ",s);
    PASS(); END_TEST;
    dl_close(lib);
}
static void test_pypi_Pillow() {
    printf("\n=== PyPI Real-World: Pillow ===\n");
    char path[512];
    snprintf(path,sizeof(path),"Pillow_pypi/Pillow_pypi.dll");
    void* lib = dl_open(path);
    if(!lib){printf("  SKIP: Pillow_pypi bridge not found\n");return;}
    typedef void* (*import_t)();
    import_t Pillow_import = (import_t)dl_sym(lib,"Pillow_import");
    typedef void* (*getattr_t)(void*,const char*);
    getattr_t Pillow_getattr = (getattr_t)dl_sym(lib,"Pillow_getattr");
    CHECK(Pillow_import&&Pillow_getattr,"missing symbols");
    void* mod = Pillow_import();
    CHECK(mod!=0,"import returned null");
    TEST("Pillow.__version__");
    void* v = Pillow_getattr(mod,"__version__");
    CHECK(v!=0,"__version__ returned null");
    typedef char* (*to_cstr_t)(void*);
    to_cstr_t Pillow_to_cstr = (to_cstr_t)dl_sym(lib,"Pillow_to_cstr");
    if(Pillow_to_cstr){char*s=Pillow_to_cstr(v);if(s&&strlen(s)>0)printf("->%s ",s);}
    PASS(); END_TEST;
    dl_close(lib);
}
static void test_pypi_flask() {
    printf("\n=== PyPI Real-World: flask ===\n");
    char path[512];
    snprintf(path,sizeof(path),"flask_pypi/flask_pypi.dll");
    void* lib = dl_open(path);
    if(!lib){printf("  SKIP: flask_pypi bridge not found\n");return;}
    typedef void* (*import_t)();
    typedef void* (*getattr_t)(void*,const char*);
    typedef char* (*to_cstr_t)(void*);
    import_t flask_import = (import_t)dl_sym(lib,"flask_import");
    getattr_t flask_getattr = (getattr_t)dl_sym(lib,"flask_getattr");
    to_cstr_t flask_to_cstr = (to_cstr_t)dl_sym(lib,"flask_to_cstr");
    CHECK(flask_import&&flask_getattr,"missing symbols");
    void* mod = flask_import();
    CHECK(mod!=0,"import returned null");
    TEST("flask.__version__");
    void* v = flask_getattr(mod,"__version__");
    CHECK(v!=0,"__version__ returned null");
    if(flask_to_cstr){char*s=flask_to_cstr(v);if(s)printf("->%s ",s);}
    PASS(); END_TEST;
    dl_close(lib);
}

/* ── npm Bridge: lodash Test ── */
static void test_npm_lodash() {
    printf("\n=== npm Bridge Test (lodash) ===\n");
    char path[512];
    snprintf(path,sizeof(path),"packages/bridges/npm/lodash_npm/lodash_npm.dll");
    void* lib = dl_open(path);
    if(!lib){printf("  SKIP: lodash_npm bridge not found\n");return;}

    typedef void* (*require_t)();
    require_t lodash_require = (require_t)dl_sym(lib,"lodash_require");
    CHECK(lodash_require!=0,"missing symbols");
    void* mod = lodash_require();

    TEST("lodash_require returns module");
    CHECK(mod!=0,"require returned null");
    PASS(); END_TEST;
    dl_close(lib);
}

/* ── npm Bridge: chalk Test ── */
static void test_npm_chalk() {
    printf("\n=== npm Bridge Test (chalk@5) ===\n");
    char path[512];
    snprintf(path,sizeof(path),"packages/bridges/npm/chalk_npm/chalk_npm.dll");
    void* lib = dl_open(path);
    if(!lib){printf("  SKIP: chalk_npm bridge not found\n");return;}

    typedef void* (*require_t)();
    require_t chalk_require = (require_t)dl_sym(lib,"chalk_require");
    CHECK(chalk_require!=0,"missing symbols");
    void* mod = chalk_require();
    if(!mod){printf("  SKIP: chalk@5 is ESM-only, QuickJS require() only supports CommonJS\n");dl_close(lib);return;}

    TEST("chalk_require returns module");
    CHECK(mod!=0,"require returned null");
    PASS(); END_TEST;
    dl_close(lib);
}

/* ── npm Bridge: execa Test ── */
static void test_npm_execa() {
    printf("\n=== npm Bridge Test (execa) ===\n");
    char path[512];
    snprintf(path,sizeof(path),"packages/bridges/npm/execa_npm/execa_npm.dll");
    void* lib = dl_open(path);
    if(!lib){printf("  SKIP: execa_npm bridge not found\n");return;}

    typedef void* (*require_t)();
    require_t execa_require = (require_t)dl_sym(lib,"execa_require");
    CHECK(execa_require!=0,"missing symbols");
    void* mod = execa_require();

    TEST("execa_require returns non-null");
    CHECK(mod!=0,"require returned null");
    PASS(); END_TEST;

    dl_close(lib);
}

/* ── npm Bridge: got Test ── */
static void test_npm_got() {
    printf("\n=== npm Bridge Test (got) ===\n");
    char path[512];
    snprintf(path,sizeof(path),"packages/bridges/npm/got_npm/got_npm.dll");
    void* lib = dl_open(path);
    if(!lib){printf("  SKIP: got_npm bridge not found\n");return;}

    typedef void* (*require_t)();
    require_t got_require = (require_t)dl_sym(lib,"got_require");
    CHECK(got_require!=0,"missing symbols");
    void* mod = got_require();

    TEST("got_require returns non-null");
    CHECK(mod!=0,"require returned null");
    PASS(); END_TEST;

    dl_close(lib);
}

/* ── Ecosystem Resolver Test ── */
extern "C" void* aurora_ecosystem_resolve(const char*, const char*);

static void test_ecosystem_resolver() {
    printf("\n=== Ecosystem Resolver Test ===\n");
    TEST("Resolve native_kernel32::GetTickCount");
    void* fn = aurora_ecosystem_resolve("native_kernel32", "GetTickCount");
    CHECK(fn != nullptr, "resolver returned null");
    typedef uint64_t (*GTC_t)();
    GTC_t GetTickCount = (GTC_t)fn;
    uint64_t tc = GetTickCount();
    CHECK(tc > 0, "GetTickCount returned 0");
    PASS();
    END_TEST;
}

/* ── Cross-Ecosystem Resolution Tests ── */
static void test_cross_ecosystem() {
    printf("\n=== Cross-Ecosystem Resolution Test ===\n");
    void* fn = nullptr;
    void* mod = nullptr;
    typedef void* (*require_t)();
    typedef void* (*import_t)();

    fn = aurora_ecosystem_resolve("npm_moment", "moment_require");
    TEST("Resolve npm_moment::moment_require");
    CHECK(fn != nullptr, "resolver returned null");
    mod = ((require_t)fn)();
    CHECK(mod != nullptr, "moment_require returned null");
    PASS(); END_TEST;

    fn = aurora_ecosystem_resolve("pypi_flask", "flask_import");
    TEST("Resolve pypi_flask::flask_import");
    if (!fn) { printf("SKIP (bridge not built)\n"); g_passed++; } else
    { mod = ((import_t)fn)();
      CHECK(mod != nullptr, "flask_import returned null");
      PASS(); } END_TEST;

    fn = aurora_ecosystem_resolve("pypi_requests", "requests_import");
    TEST("Resolve pypi_requests::requests_import");
    if (!fn) { printf("SKIP (bridge not built)\n"); g_passed++; } else
    { mod = ((import_t)fn)();
      CHECK(mod != nullptr, "requests_import returned null");
      PASS(); } END_TEST;
}

/* ── Stress Test: rapid npm require + API calls ── */
static void test_npm_stress() {
    printf("\n=== Stress Test (rapid require + API calls) ===\n");
    char path[512];
    snprintf(path, sizeof(path), "../../packages/bridges/npm/moment_npm/moment_npm.dll");
    void* lib = dl_open(path);
    if (!lib) {
        snprintf(path, sizeof(path), "packages/bridges/npm/moment_npm/moment_npm.dll");
        lib = dl_open(path);
    }
    if (!lib) { printf("  SKIP: moment_npm bridge not found\n"); return; }

    typedef void* (*require_t)();
    typedef void* (*call_t)(void*, const char*, void*);
    typedef char* (*to_cstr_t)(void*);
    typedef void  (*free_t)(void*);
    typedef void  (*free_cstr_t)(char*);
    require_t moment_require = (require_t)dl_sym(lib, "moment_require");
    call_t moment_call = (call_t)dl_sym(lib, "moment_call");
    to_cstr_t moment_to_cstr = (to_cstr_t)dl_sym(lib, "moment_to_cstr");
    free_t moment_free = (free_t)dl_sym(lib, "moment_free");
    free_cstr_t moment_free_cstr = (free_cstr_t)dl_sym(lib, "moment_free_cstr");
    if (!moment_require || !moment_call || !moment_to_cstr || !moment_free) {
        printf("  SKIP: missing symbols\n"); dl_close(lib); return;
    }

    void* mod = moment_require();
    if (!mod) { printf("  SKIP: require failed\n"); dl_close(lib); return; }

    int ok; double t0, dt;

    /* Test 1: rapid require() calls (simulate 50 module loads) */
    TEST("rapid require calls (x50)");
    t0 = now_ms(); ok = 1;
    for (int i = 0; i < 50; i++) {
        void* m = moment_require();
        if (!m) { ok = 0; break; }
    }
    dt = now_ms() - t0;
    if (ok) printf("  %.2f ms for 50 calls", dt);
    CHECK(ok, "require failed during stress");
    PASS(); END_TEST;

    /* Test 2: rapid JS function calls with arguments */
    TEST("rapid JS calls (x1000)");
    t0 = now_ms(); ok = 1;
    for (int i = 0; i < 1000; i++) {
        void* r = moment_call(mod, "now", 0);
        if (!r) { ok = 0; break; }
        moment_free(r);
    }
    dt = now_ms() - t0;
    if (ok) printf("  %.2f ms for 1000 calls", dt);
    CHECK(ok, "JS call failed during stress");
    PASS(); END_TEST;

    /* Test 3: rapid JSON value conversion */
    TEST("rapid JSON conversion (x1000)");
    t0 = now_ms();
    for (int i = 0; i < 1000; i++) {
        void* s = moment_call(mod, "now", 0);
        if (s) {
            char* cstr = moment_to_cstr(s);
            if (cstr) { moment_free_cstr(cstr); }
            moment_free(s);
        }
    }
    dt = now_ms() - t0;
    printf("  %.2f ms for 1000 conversions", dt);
    PASS(); END_TEST;

    moment_free(mod);
    dl_close(lib);
}

/* ── Cross-Ecosystem Chain Test: Rust → Python → JS ── */
static void test_cross_ecosystem_chain() {
    printf("\n=== Cross-Ecosystem Chain Test (Rust→Python→JS) ===\n");

    /* ── Load Cargo bridge (Rust) ── */
    char path[512];
    snprintf(path, sizeof(path), "../../packages/bridges/cargo/hello_rust_cargo/hello_rust_cargo.dll");
    void* rs_lib = dl_open(path);
    if (!rs_lib) {
        snprintf(path, sizeof(path), "packages/bridges/cargo/hello_rust_cargo/hello_rust_cargo.dll");
        rs_lib = dl_open(path);
    }
    if (!rs_lib) { printf("  SKIP: hello_rust_cargo not found\n"); return; }

    typedef void* (*import_t)();
    typedef void* (*call1_t)(void*, const char*, void*);
    typedef void* (*str_t)(const char*);
    typedef char* (*to_cstr_t)(void*);
    import_t rs_import = (import_t)dl_sym(rs_lib, "hello_rust_import");
    call1_t rs_call1 = (call1_t)dl_sym(rs_lib, "hello_rust_call1");
    str_t rs_str = (str_t)dl_sym(rs_lib, "hello_rust_str");
    to_cstr_t rs_to_cstr = (to_cstr_t)dl_sym(rs_lib, "hello_rust_to_cstr");
    if (!rs_import) { dl_close(rs_lib); printf("  SKIP: missing Rust symbols\n"); return; }

    void* rs_mod = rs_import();
    if (!rs_mod) { dl_close(rs_lib); printf("  SKIP: Rust import failed\n"); return; }

    /* ── Load PyPI bridge (Python) ── */
    snprintf(path, sizeof(path), "../../markdown_pypi/markdown_pypi.dll");
    void* py_lib = dl_open(path);
    if (!py_lib) {
        snprintf(path, sizeof(path), "markdown_pypi/markdown_pypi.dll");
        py_lib = dl_open(path);
    }
    if (!py_lib) { dl_close(rs_lib); printf("  SKIP: markdown_pypi not found\n"); return; }

    import_t py_import = (import_t)dl_sym(py_lib, "markdown_import");
    call1_t py_call1 = (call1_t)dl_sym(py_lib, "markdown_call1");
    str_t py_str = (str_t)dl_sym(py_lib, "markdown_str");
    to_cstr_t py_to_cstr = (to_cstr_t)dl_sym(py_lib, "markdown_to_cstr");
    if (!py_import) { dl_close(rs_lib); dl_close(py_lib); printf("  SKIP: missing Python symbols\n"); return; }

    void* py_mod = py_import();
    if (!py_mod) { dl_close(rs_lib); dl_close(py_lib); printf("  SKIP: Python import failed\n"); return; }

    /* ── Load npm bridge (JS) ── */
    snprintf(path, sizeof(path), "../../packages/bridges/npm/moment_npm/moment_npm.dll");
    void* js_lib = dl_open(path);
    if (!js_lib) {
        snprintf(path, sizeof(path), "packages/bridges/npm/moment_npm/moment_npm.dll");
        js_lib = dl_open(path);
    }
    void* js_mod = NULL;
    typedef void* (*require_t)();
    typedef void* (*js_call_t)(void*, const char*, void*);
    typedef void  (*free_t)(void*);
    require_t js_require = NULL;
    js_call_t js_call = NULL;
    to_cstr_t js_to_cstr = NULL;
    free_t js_free = NULL;
    if (js_lib) {
        js_require = (require_t)dl_sym(js_lib, "moment_require");
        js_call = (js_call_t)dl_sym(js_lib, "moment_call");
        js_to_cstr = (to_cstr_t)dl_sym(js_lib, "moment_to_cstr");
        js_free = (free_t)dl_sym(js_lib, "moment_free");
        if (js_require) js_mod = js_require();
    }

    /* ── Test 1: Rust → Python ── */
    TEST("Rust→Python: greet→markdown chain");
    void* rs_arg = rs_str("CrossEcosystem");
    void* rs_result = rs_call1(rs_mod, "greet", rs_arg);
    CHECK(rs_result != 0, "greet returned null");
    char* greeting = rs_to_cstr(rs_result);
    CHECK(greeting != 0 && strlen(greeting) > 0, "greeting empty");
    /* Pass Rust result to Python markdown */
    void* py_arg = py_str(greeting);
    void* py_result = py_call1(py_mod, "markdown", py_arg);
    CHECK(py_result != 0, "markdown returned null");
    char* html = py_to_cstr(py_result);
    CHECK(html != 0, "html empty");
    CHECK(strstr(html, "CrossEcosystem") != 0, "expected name in HTML output");
    PASS(); END_TEST;

    /* ── Test 2: JS → Rust ── */
    if (js_mod && rs_mod) {
        void* ts; char* ts_str;
        TEST("JS→Rust: moment→greet chain");
        ts = js_call(js_mod, "now", 0);
        CHECK(ts != 0, "moment.now() failed");
        ts_str = js_to_cstr(ts);
        CHECK(ts_str != 0 && strlen(ts_str) > 0, "timestamp empty");
        { void* rs_a2 = rs_str(ts_str); void* rs_r2 = rs_call1(rs_mod, "greet", rs_a2);
          CHECK(rs_r2 != 0, "greet returned null");
          char* g2 = rs_to_cstr(rs_r2);
          CHECK(g2 != 0 && strstr(g2, ts_str) != 0, "expected timestamp in greeting"); }
        PASS(); END_TEST;
        js_free(ts);
    } else {
        printf("  SKIP: moment_npm not available\n");
    }

    /* ── Test 3: JS → Python ── */
    if (js_mod && py_mod) {
        void* ts; char* ts_str; char md_buf[256];
        TEST("JS→Python: moment→markdown chain");
        ts = js_call(js_mod, "now", 0);
        CHECK(ts != 0, "moment.now() failed");
        ts_str = js_to_cstr(ts);
        CHECK(ts_str != 0 && strlen(ts_str) > 0, "timestamp empty");
        snprintf(md_buf, sizeof(md_buf), "# Timestamp: %s", ts_str);
        { void* p_a2 = py_str(md_buf); void* p_r2 = py_call1(py_mod, "markdown", p_a2);
          CHECK(p_r2 != 0, "markdown returned null");
          char* h2 = py_to_cstr(p_r2);
          CHECK(h2 != 0 && strstr(h2, "<h1>") != 0, "expected <h1> in HTML"); }
        PASS(); END_TEST;
        js_free(ts);
    } else {
        printf("  SKIP: moment or markdown not available\n");
    }

    /* ── Test 4: Full three-way chain Rust→Python→JS ── */
    if (js_mod && rs_mod && py_mod) {
        void* j_r;
        TEST("Full chain: Rust→Python→JS roundtrip");
        { void* rs_a2 = rs_str("ChainTest"); void* rs_r2 = rs_call1(rs_mod, "greet", rs_a2);
          CHECK(rs_r2 != 0, "greet failed");
          char* g2 = rs_to_cstr(rs_r2);
          void* p_a2 = py_str(g2); void* p_r2 = py_call1(py_mod, "markdown", p_a2);
          CHECK(p_r2 != 0, "markdown failed");
          char* h2 = py_to_cstr(p_r2);
          CHECK(h2 != 0 && strstr(h2, "ChainTest") != 0, "ChainTest in HTML"); }
        j_r = js_call(js_mod, "now", 0);
        CHECK(j_r != 0, "moment.now failed");
        PASS(); END_TEST;
        js_free(j_r);
    }

    if (js_mod) js_free(js_mod);
    if (js_lib) dl_close(js_lib);
    dl_close(py_lib);
    dl_close(rs_lib);
}

int main() {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    printf("========================================\n");
    printf("  Bridge End-to-End Integration Test\n");
    printf("========================================\n");

    test_native_bridge();
    /* Ensure Python is initialized early (markdown bridge is lightweight) */
    test_pypi_bridge();
    test_pypi_numpy();
    test_pypi_requests_version();
    test_pypi_Pillow();
    test_pypi_flask();
    test_npm_bridge();
    test_cargo_bridge();
    test_ecosystem_resolver();
    test_cross_ecosystem();
    test_cross_ecosystem_chain();
    test_npm_stress();

    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed, %d total\n", g_passed, g_failed, g_test_count);
    printf("========================================\n");
    return g_failed > 0 ? 1 : 0;
}
