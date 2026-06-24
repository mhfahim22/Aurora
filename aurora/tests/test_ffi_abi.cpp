// Phase 1: Universal FFI Core -- ABI abstraction tests
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>

#include "runtime/interop/ffi_abi.hpp"

static int g_pass = 0, g_fail = 0, g_cnt = 0;

static void test_header(const char* name) {
    printf("\n--- %s ---\n", name);
}

static int check(int cond, const char* msg) {
    if (!cond) {
        printf("  test %2d: FAIL: %s\n", g_cnt, msg);
        g_fail++;
        return 0;
    }
    return 1;
}

static void test(const char* name) {
    g_cnt++;
    printf("  test %2d: %s ... ", g_cnt, name);
}

static void pass() {
    g_pass++;
    printf("PASS\n");
}

#define T(n)   do { test(n); } while(0)
#define P      do { pass(); } while(0)
#define C(c,m) do { if (!check((c),(m))) return; } while(0)

// Test fixtures: simple C functions
extern "C" int add_i32(int a, int b) { return a + b; }
extern "C" long long add_i64(long long a, long long b) { return a + b; }
extern "C" double add_f64(double a, double b) { return a + b; }
extern "C" void* id_ptr(void* p) { return p; }
extern "C" int add_4i32(int a, int b, int c, int d) { return a + b + c + d; }
extern "C" double mixed_args(int a, double b, int c) { return a + b + c; }
extern "C" int many_args(int a0,int a1,int a2,int a3,int a4,int a5) {
    return a0 + a1 + a2 + a3 + a4 + a5;
}
extern "C" int no_args(void) { return 42; }

// 1. ffi_type_size
static void test_type_sizes() {
    test_header("Type Sizes");
    T("FFI_VOID");   C(ffi_type_size(FFI_VOID) == 0,   "expected 0"); P;
    T("FFI_INT8");   C(ffi_type_size(FFI_INT8) == 1,   "expected 1"); P;
    T("FFI_INT32");  C(ffi_type_size(FFI_INT32) == 4,  "expected 4"); P;
    T("FFI_INT64");  C(ffi_type_size(FFI_INT64) == 8,  "expected 8"); P;
    T("FFI_FLOAT");  C(ffi_type_size(FFI_FLOAT) == 4,  "expected 4"); P;
    T("FFI_DOUBLE"); C(ffi_type_size(FFI_DOUBLE) == 8, "expected 8"); P;
    T("FFI_PTR");    C(ffi_type_size(FFI_PTR) == 8,    "expected 8"); P;
}

// 2. ffi_cif_prep
static void test_cif_prep() {
    test_header("CIF Prep");

    ffi_type_id types[4] = {FFI_INT32, FFI_INT32};
    ffi_cif cif;

    T("valid CIF");
    int rc = ffi_cif_prep(&cif, FFI_INT32, 2, types);
    C(rc == FFI_OK, "expected OK");
    C(cif.ret_type == FFI_INT32, "ret_type wrong");
    C(cif.arg_count == 2, "arg_count wrong");
    P;

    T("null CIF");
    C(ffi_cif_prep(nullptr, FFI_INT32, 0, nullptr) == FFI_ENULL, "expected ENULL");
    P;

    T("bad return type");
    C(ffi_cif_prep(&cif, (ffi_type_id)99, 0, nullptr) == FFI_EBADTYPE, "expected EBADTYPE");
    P;

    T("too many args");
    C(ffi_cif_prep(&cif, FFI_VOID, 99, nullptr) == FFI_ETOOMANY, "expected ETOOMANY");
    P;
}

// Helper to make a call
static ffi_value call_fn(ffi_type_id ret_type, void* fn,
                         ffi_type_id* arg_types, ffi_value* args, int n) {
    ffi_cif cif;
    ffi_cif_prep(&cif, ret_type, n, arg_types);
    ffi_value ret;
    memset(&ret, 0, sizeof(ret));
    ffi_call(&cif, fn, &ret, args);
    return ret;
}

// 3. Basic FFI calls
static void test_basic_calls() {
    test_header("Basic FFI Calls");

    ffi_type_id two_i32[2] = {FFI_INT32, FFI_INT32};
    ffi_type_id ret_f64[1] = {FFI_DOUBLE};
    ffi_type_id ret_ptr[1] = {FFI_PTR};
    ffi_type_id ret_i32[1] = {FFI_INT32};
    ffi_type_id ret_i64[1] = {FFI_INT64};

    // add_i32(3, 5) = 8
    {
        ffi_value args[2];
        args[0].i32 = 3; args[1].i32 = 5;
        ffi_value r = call_fn(FFI_INT32, (void*)add_i32, two_i32, args, 2);
        T("add_i32(3,5)");
        C(r.i32 == 8, "expected 8"); P;
    }

    // add_i64(10000000000LL, 20000000000LL)
    {
        ffi_value args[2];
        args[0].i64 = 10000000000LL; args[1].i64 = 20000000000LL;
        ffi_value r = call_fn(FFI_INT64, (void*)add_i64, ret_i64, args, 2);
        T("add_i64(10B,20B)");
        C(r.i64 == 30000000000LL, "expected 30B"); P;
    }

    // add_f64(3.14, 2.86)
    {
        ffi_value args[2];
        args[0].f64 = 3.14; args[1].f64 = 2.86;
        ffi_value r = call_fn(FFI_DOUBLE, (void*)add_f64, ret_f64, args, 2);
        T("add_f64(3.14,2.86)");
        C(fabs(r.f64 - 6.0) < 0.001, "expected ~6.0"); P;
    }

    // id_ptr((void*)0x1234)
    {
        ffi_value args[1];
        args[0].ptr = (void*)(intptr_t)0x1234;
        ffi_value r = call_fn(FFI_PTR, (void*)id_ptr, ret_ptr, args, 1);
        T("id_ptr(0x1234)");
        C(r.ptr == (void*)(intptr_t)0x1234, "expected 0x1234"); P;
    }

    // no_args()
    {
        ffi_value r = call_fn(FFI_INT32, (void*)no_args, ret_i32, nullptr, 0);
        T("no_args()");
        C(r.i32 == 42, "expected 42"); P;
    }
}

// 4. Mixed argument types
static void test_mixed_args() {
    test_header("Mixed Argument Types");

    ffi_type_id types[3] = {FFI_INT32, FFI_DOUBLE, FFI_INT32};
    ffi_value args[3];
    args[0].i32 = 10;
    args[1].f64 = 20.5;
    args[2].i32 = 30;

    ffi_value r = call_fn(FFI_DOUBLE, (void*)mixed_args, types, args, 3);
    T("mixed_args(10, 20.5, 30)");
    C(fabs(r.f64 - 60.5) < 0.001, "expected 60.5"); P;
}

// 5. Multi-arg calls (4+ args)
static void test_multi_args() {
    test_header("Multi-argument Calls");

    // add_4i32(1,2,3,4)
    {
        ffi_type_id types[4] = {FFI_INT32, FFI_INT32, FFI_INT32, FFI_INT32};
        ffi_value args[4];
        args[0].i32 = 1; args[1].i32 = 2; args[2].i32 = 3; args[3].i32 = 4;
        ffi_value r = call_fn(FFI_INT32, (void*)add_4i32, types, args, 4);
        T("add_4i32(1,2,3,4)");
        C(r.i32 == 10, "expected 10"); P;
    }

    // many_args(1,2,3,4,5,6) -- uses stack args on Win64
    {
        ffi_type_id types[6] = {FFI_INT32,FFI_INT32,FFI_INT32,FFI_INT32,FFI_INT32,FFI_INT32};
        ffi_value args[6];
        for (int i = 0; i < 6; i++) args[i].i32 = i + 1;
        ffi_value r = call_fn(FFI_INT32, (void*)many_args, types, args, 6);
        T("many_args(1..6)");
        C(r.i32 == 21, "expected 21"); P;
    }
}

// 6. Platform ABI detection
static void test_platform() {
    test_header("Platform Detection");

    int abi = ffi_abi_platform();
    T("platform ABI");
#if defined(_WIN64)
    C(abi == FFI_ABI_WIN64, "expected WIN64");
#else
    C(abi == FFI_ABI_SYSV, "expected SYSV");
#endif
    P;

    T("ffi_strerror");
    C(strcmp(ffi_strerror(FFI_OK), "Success") == 0, "expected Success");
    C(strcmp(ffi_strerror(FFI_EBADTYPE), "Invalid type in FFI CIF") == 0, "expected msg");
    P;
}

int main() {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    printf("[Phase 1: Universal FFI Core]\n");

    test_type_sizes();
    test_cif_prep();
    test_basic_calls();
    test_mixed_args();
    test_multi_args();
    test_platform();

    printf("\n[Results: %d/%d passed%s]\n",
           g_pass, g_pass + g_fail, g_fail ? " (FAIL)" : "");
    return g_fail > 0 ? 1 : 0;
}
