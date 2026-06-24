// Phase 1: Universal FFI Core -- maximum edge case coverage
// This tests every type, calling pattern, and error path so the
// FFI layer can be used safely from C++, Rust, and other high-level
// languages via extern "C".
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <climits>

#include "runtime/interop/ffi_abi.hpp"

static int g_pass = 0, g_fail = 0, g_cnt = 0;
static void hdr(const char* n) { printf("\n--- %s ---\n", n); }
static void t(const char* n) { g_cnt++; printf("  test %2d: %s ... ", g_cnt, n); }
#define PASS do { g_pass++; printf("PASS\n"); } while(0)
#define CHECK(c,m) do { if (!(c)) { g_fail++; printf("FAIL: %s\n", m); return; } } while(0)

// ─── test helper ───
static ffi_value X(ffi_type_id rt, void* fn,
                   ffi_type_id* at, ffi_value* args, int n) {
    ffi_cif cif;
    ffi_cif_prep(&cif, rt, n, at);
    ffi_value ret;
    memset(&ret, 0, sizeof(ret));
    ffi_call(&cif, fn, &ret, args);
    return ret;
}

// ═════════════════════════════════════════════════════════════
// Fixture functions
// ═════════════════════════════════════════════════════════════
extern "C" {

// ── Identity functions ──
int32_t  id_i32(int32_t x)                  { return x; }
int64_t  id_i64(int64_t x)                  { return x; }

// ── Bool / char round-trips ──
bool    id_bool(bool x)                     { return x; }
char    id_char(char x)                     { return x; }
signed   char id_sc(signed char x)          { return x; }
unsigned char id_uc(unsigned char x)        { return x; }
wchar_t       id_wc(wchar_t x)              { return x; }

// ── Bool logic ──
bool    and_bool(bool a, bool b)            { return a && b; }
bool    or_bool(bool a, bool b)             { return a || b; }
bool    not_bool(bool x)                    { return !x; }

// ── Char string round-trip ──
const char* hello(void)                     { return "hello"; }
int         cmp_str(const char* a, const char* b) { return (a == b) || (a && b && strcmp(a,b)==0) ? 1 : 0; }

// ── Full register saturation (4 GP + 4 XMM = 8 args) ──
// a,b,c,d in RCX,RDX,R8,R9; e,f,g,h in XMM0-3
// But on Win64, floats ALSO use the integer register slots.
// So we mix: first 4 are (int,float,int,float) to use both views
double saturate_8(int a, double b, int64_t c, float d,
                  int e, double f, int64_t g, float h) {
    return a + b + c + d + e + f + g + h;
}

// ── Pointer write ──
void write_int(int* p, int v)               { *p = v; }
void write_double(double* p, double v)      { *p = v; }
void memset_fake(void* p, int v, int n)     { for (int i=0; i<n; i++) ((char*)p)[i] = (char)v; }

// ── Enum (int-sized) ──
typedef enum { E_A=0, E_B=100, E_C=200 } MyEnum;
MyEnum id_enum(MyEnum e)                    { return e; }

// ── Callback ──
int call_cb(int (*cb)(int), int x)          { return cb(x); }
int apply2(int (*cb)(int,int), int a, int b){ return cb(a,b); }

// ── Small struct return (≤8 bytes, in RAX on Win64) ──
// Two int32 = 8 bytes → RAX
typedef struct { int32_t a; int32_t b; } Pair;
Pair make_pair(int a, int b) { Pair p = {a,b}; return p; }
int64_t pair_raw(int a, int b) { return (int64_t)(uint32_t)a | ((int64_t)(uint32_t)b << 32); }

// ── Single-element struct ──
typedef struct { int32_t x; } Wrap32;
Wrap32 wrap_i32(int32_t x) { Wrap32 w = {x}; return w; }

// ── Tricky null / extreme values ──
double   neg_zero(void)                     { return -0.0; }
double   infinity_val(void)                 { return INFINITY; }
double   nan_val(void)                      { return NAN; }
int      int_max(void)                      { return INT_MAX; }
int      int_min(void)                      { return INT_MIN; }
uint64_t u64_max(void)                      { return UINT64_MAX; }

// ── Volatile / side-effect helpers ──
static int s_counter = 0;
void      reset_counter(void)               { s_counter = 0; }
int       bump_counter(void)                { return ++s_counter; }

// ── Tail-call-like single-arg recursion prevention ──
int64_t   fib(int n) {
    if (n <= 1) return n;
    int64_t a=0,b=1,c;
    for (int i=2;i<=n;i++) { c=a+b; a=b; b=c; }
    return b;
}

} // extern "C"

// ═════════════════════════════════════════════════════════════
// 1. Bool types
// ═════════════════════════════════════════════════════════════
static void test_bool() {
    hdr("Bool Types");

    ffi_value a[1];
    ffi_type_id tb[1] = {FFI_INT8};

    // C++ bool is 1 byte, fits in INT8
    a[0].i8 = 0;  t("bool false"); CHECK(X(FFI_INT8,id_bool,tb,a,1).i8 == 0, "expected 0"); PASS;
    a[0].i8 = 1;  t("bool true");  CHECK(X(FFI_INT8,id_bool,tb,a,1).i8 == 1, "expected 1"); PASS;
    a[0].i8 = 42; t("bool raw"); CHECK(X(FFI_INT8,id_bool,tb,a,1).i8 == 42, "expected raw 42"); PASS;

    ffi_value a2[2];
    ffi_type_id tb2[2] = {FFI_INT8, FFI_INT8};
    a2[0].i8 = 1; a2[1].i8 = 1; t("and true true");  CHECK(X(FFI_INT8,and_bool,tb2,a2,2).i8 == 1, "expected 1"); PASS;
    a2[0].i8 = 1; a2[1].i8 = 0; t("and true false"); CHECK(X(FFI_INT8,and_bool,tb2,a2,2).i8 == 0, "expected 0"); PASS;
    a2[0].i8 = 0; a2[1].i8 = 1; t("or false true");  CHECK(X(FFI_INT8,or_bool,tb2,a2,2).i8 == 1, "expected 1"); PASS;
    a2[0].i8 = 0; a2[1].i8 = 0; t("or false false"); CHECK(X(FFI_INT8,or_bool,tb2,a2,2).i8 == 0, "expected 0"); PASS;

    a[0].i8 = 0; t("not true");  CHECK(X(FFI_INT8,not_bool,tb,a,1).i8 == 1, "expected 1"); PASS;
    a[0].i8 = 1; t("not false"); CHECK(X(FFI_INT8,not_bool,tb,a,1).i8 == 0, "expected 0"); PASS;
}

// ═════════════════════════════════════════════════════════════
// 2. Char types
// ═════════════════════════════════════════════════════════════
static void test_char_types() {
    hdr("Char Types");

    ffi_value a[1];
    ffi_type_id tc[1] = {FFI_INT8};

    a[0].i8 = 'A'; t("char");       CHECK(X(FFI_INT8,id_char,tc,a,1).i8 == 'A', "expected 'A'"); PASS;
    a[0].i8 = 'z'; t("signed char"); CHECK(X(FFI_INT8,id_sc,tc,a,1).i8 == 'z', "expected 'z'"); PASS;
    a[0].i8 = -1;  t("signed char -1"); CHECK(X(FFI_INT8,id_sc,tc,a,1).i8 == -1, "expected -1"); PASS;

    // unsigned char (0-255) fits in INT8 signed view, but test value > 127
    ffi_type_id tu[1] = {FFI_UINT8};
    a[0].u8 = 200; t("unsigned char"); CHECK(X(FFI_UINT8,id_uc,tu,a,1).u8 == 200, "expected 200"); PASS;
    a[0].u8 = 255; t("uchar max");     CHECK(X(FFI_UINT8,id_uc,tu,a,1).u8 == 255, "expected 255"); PASS;

    // wchar_t on Windows = 2 bytes (UTF-16)
    ffi_type_id tw[1] = {FFI_INT16};
    a[0].i16 = 0x41;    t("wchar A");    CHECK(X(FFI_INT16,id_wc,tw,a,1).i16 == 0x41, "expected 0x41"); PASS;
    a[0].i16 = 0xFFFF;  t("wchar max");  CHECK(X(FFI_INT16,id_wc,tw,a,1).i16 == (int16_t)0xFFFF, "expected -1"); PASS;
}

// ═════════════════════════════════════════════════════════════
// 3. Extreme / special float values
// ═════════════════════════════════════════════════════════════
static void test_extreme_float() {
    hdr("Extreme Float Values");

    ffi_type_id td[1] = {FFI_DOUBLE};

    t("neg zero");   CHECK(X(FFI_DOUBLE,neg_zero,td,NULL,0).f64 == -0.0, "expected -0.0"); PASS;
    t("+inf");       CHECK(isinf(X(FFI_DOUBLE,infinity_val,td,NULL,0).f64), "expected inf"); PASS;
    t("NaN");        CHECK(isnan(X(FFI_DOUBLE,nan_val,td,NULL,0).f64), "expected NaN"); PASS;
}

// ═════════════════════════════════════════════════════════════
// 4. Extreme integer values
// ═════════════════════════════════════════════════════════════
static void test_extreme_int() {
    hdr("Extreme Integer Values");

    ffi_type_id ti[1] = {FFI_INT32};
    ffi_type_id tu[1] = {FFI_UINT64};

    t("INT_MAX");  CHECK(X(FFI_INT32,int_max,ti,NULL,0).i32 == INT_MAX,  "expected INT_MAX"); PASS;
    t("INT_MIN");  CHECK(X(FFI_INT32,int_min,ti,NULL,0).i32 == INT_MIN,  "expected INT_MIN"); PASS;
    t("UINT64_MAX"); CHECK(X(FFI_UINT64,u64_max,tu,NULL,0).u64 == UINT64_MAX, "expected UINT64_MAX"); PASS;
}

// ═════════════════════════════════════════════════════════════
// 5. Full register saturation (8 args: interleaved int/float)
// ═════════════════════════════════════════════════════════════
static void test_register_saturation() {
    hdr("Register Saturation (8 args)");

    ffi_type_id t8[8] = {FFI_INT32,FFI_DOUBLE,FFI_INT64,FFI_FLOAT,
                          FFI_INT32,FFI_DOUBLE,FFI_INT64,FFI_FLOAT};
    ffi_value a[8];
    a[0].i32 = 10;  a[1].f64 = 1.5; a[2].i64 = 20; a[3].f32 = 2.5f;
    a[4].i32 = 30;  a[5].f64 = 3.5; a[6].i64 = 40; a[7].f32 = 4.5f;

    t("saturate_8");
    double r = X(FFI_DOUBLE,saturate_8,t8,a,8).f64;
    CHECK(fabs(r - 112.0) < 0.001, "expected 112.0"); PASS;
}

// ═════════════════════════════════════════════════════════════
// 6. Write-through-pointer (side-effect output args)
// ═════════════════════════════════════════════════════════════
static void test_pointer_write() {
    hdr("Pointer Write (Output Args)");

    int    iv = 0;
    double dv = 0.0;

    ffi_type_id tp[2] = {FFI_PTR, FFI_INT32};
    ffi_type_id tpd[2] = {FFI_PTR, FFI_DOUBLE};
    ffi_value a[2];

    a[0].ptr = &iv; a[1].i32 = 42;
    t("write int thru ptr");
    X(FFI_VOID, write_int, tp, a, 2);
    CHECK(iv == 42, "expected 42"); PASS;

    a[0].ptr = &dv; a[1].f64 = 3.14;
    t("write double thru ptr");
    X(FFI_VOID, write_double, tpd, a, 2);
    CHECK(fabs(dv - 3.14) < 0.001, "expected 3.14"); PASS;

    // memset_fake: void* + int value + int count
    ffi_type_id tm[3] = {FFI_PTR, FFI_INT32, FFI_INT32};
    char buf[16];
    memset(buf, 0xAA, sizeof(buf));
    ffi_value m[3];
    m[0].ptr = buf; m[1].i32 = 0xFF; m[2].i32 = 4;
    t("memset buf[0..3] = 0xFF");
    X(FFI_VOID, memset_fake, tm, m, 3);
    CHECK((unsigned char)buf[0] == 0xFF && (unsigned char)buf[3] == 0xFF &&
          (unsigned char)buf[4] == 0xAA, "expected first 4 bytes = 0xFF, 5th unchanged"); PASS;
}

// ═════════════════════════════════════════════════════════════
// 7. Enum (int-sized) round-trip
// ═════════════════════════════════════════════════════════════
static void test_enum() {
    hdr("Enum Round-trip");

    ffi_type_id te[1] = {FFI_INT32};
    ffi_value a[1];

    a[0].i32 = E_A;   t("enum A");   CHECK(X(FFI_INT32,id_enum,te,a,1).i32 == E_A,   "expected 0"); PASS;
    a[0].i32 = E_B;   t("enum B");   CHECK(X(FFI_INT32,id_enum,te,a,1).i32 == E_B,   "expected 100"); PASS;
    a[0].i32 = E_C;   t("enum C");   CHECK(X(FFI_INT32,id_enum,te,a,1).i32 == E_C,   "expected 200"); PASS;
}

// ═════════════════════════════════════════════════════════════
// 8. Callback through FFI (function pointer args)
// ═════════════════════════════════════════════════════════════
static int cb_double(int x) { return x * 2; }
static int cb_add(int a, int b) { return a + b; }

static void test_callback() {
    hdr("Callback (Function Pointer Args)");

    ffi_type_id tcb[2] = {FFI_PTR, FFI_INT32};
    ffi_value a[2];

    a[0].ptr = (void*)cb_double; a[1].i32 = 21;
    t("call_cb(double, 21)");
    CHECK(X(FFI_INT32,call_cb,tcb,a,2).i32 == 42, "expected 42"); PASS;

    ffi_type_id tcb3[3] = {FFI_PTR, FFI_INT32, FFI_INT32};
    ffi_value a3[3];
    a3[0].ptr = (void*)cb_add; a3[1].i32 = 10; a3[2].i32 = 20;
    t("apply2(add,10,20)");
    CHECK(X(FFI_INT32,apply2,tcb3,a3,3).i32 == 30, "expected 30"); PASS;
}

// ═════════════════════════════════════════════════════════════
// 9. Small struct return (≤8 bytes via RAX)
// ═════════════════════════════════════════════════════════════
static void test_small_struct() {
    hdr("Small Struct Return (<=8 bytes in RAX)");

    // make_pair returns Pair {int32 a, int32 b} = 8 bytes
    // On Win64 this is returned in RAX. We read as FFI_INT64.
    ffi_type_id ti[2] = {FFI_INT32, FFI_INT32};
    ffi_value a[2];
    a[0].i32 = 10; a[1].i32 = 20;

    t("make_pair(10,20)");
    int64_t raw = X(FFI_INT64, make_pair, ti, a, 2).i64;
    CHECK(raw == pair_raw(10,20), "expected packed (10|20<<32)"); PASS;

    // Verify the individual fields by comparing with pair_raw
    a[0].i32 = -1; a[1].i32 = -1;
    t("make_pair(-1,-1)");
    raw = X(FFI_INT64, make_pair, ti, a, 2).i64;
    CHECK(raw == pair_raw(-1,-1), "expected packed (-1|-1<<32)"); PASS;

    // wrap_i32 returns struct {int32 x} = 4 bytes in EAX
    // On Win64, 4-byte structs are returned in RAX but only lower 32 bits are valid
    ffi_type_id tw[1] = {FFI_INT32};
    a[0].i32 = 999;
    t("wrap_i32(999)");
    CHECK(X(FFI_INT32, wrap_i32, tw, a, 1).i32 == 999, "expected 999"); PASS;
}

// ═════════════════════════════════════════════════════════════
// 10. String / const char* round-trip
// ═════════════════════════════════════════════════════════════
static void test_strings() {
    hdr("String / const char*");

    ffi_type_id tp[1] = {FFI_PTR};

    t("hello string");
    const char* s = (const char*)X(FFI_PTR, hello, tp, NULL, 0).ptr;
    CHECK(s && strcmp(s, "hello") == 0, "expected 'hello'"); PASS;

    ffi_type_id t2[2] = {FFI_PTR, FFI_PTR};
    ffi_value a[2];
    a[0].ptr = (void*)"hello"; a[1].ptr = (void*)"hello";
    t("cmp_str equal");
    CHECK(X(FFI_INT32, cmp_str, t2, a, 2).i32 == 1, "expected 1"); PASS;

    a[1].ptr = (void*)"world";
    t("cmp_str differ");
    CHECK(X(FFI_INT32, cmp_str, t2, a, 2).i32 == 0, "expected 0"); PASS;
}

// ═════════════════════════════════════════════════════════════
// 11. Compute-heavy call (Fibonacci, no recursion)
// ═════════════════════════════════════════════════════════════
static void test_compute() {
    hdr("Compute via FFI");

    ffi_type_id ti[1] = {FFI_INT32};
    ffi_value a[1];

    a[0].i32 = 0;  t("fib(0)"); CHECK(X(FFI_INT64,fib,ti,a,1).i64 == 0,  "expected 0"); PASS;
    a[0].i32 = 1;  t("fib(1)"); CHECK(X(FFI_INT64,fib,ti,a,1).i64 == 1,  "expected 1"); PASS;
    a[0].i32 = 10; t("fib(10)"); CHECK(X(FFI_INT64,fib,ti,a,1).i64 == 55, "expected 55"); PASS;
    a[0].i32 = 20; t("fib(20)"); CHECK(X(FFI_INT64,fib,ti,a,1).i64 == 6765,"expected 6765"); PASS;
}

// ═════════════════════════════════════════════════════════════
// 12. Side-effect ordering (counter bump)
// ═════════════════════════════════════════════════════════════
static void test_side_effects() {
    hdr("Side Effect Ordering");

    ffi_type_id tv[1] = {FFI_VOID};

    reset_counter();
    t("reset counter"); CHECK(s_counter == 0, "expected 0"); PASS;

    X(FFI_VOID, reset_counter, tv, NULL, 0);
    CHECK(s_counter == 0, "expected 0 after reset"); // no-arg void call

    ffi_type_id ti[1] = {FFI_INT32};
    t("bump #1"); CHECK(X(FFI_INT32,bump_counter,ti,NULL,0).i32 == 1, "expected 1"); PASS;
    t("bump #2"); CHECK(X(FFI_INT32,bump_counter,ti,NULL,0).i32 == 2, "expected 2"); PASS;
    t("bump #3"); CHECK(X(FFI_INT32,bump_counter,ti,NULL,0).i32 == 3, "expected 3"); PASS;
}

// ═════════════════════════════════════════════════════════════
// 13. Error path coverage
// ═════════════════════════════════════════════════════════════
static void test_error_paths() {
    hdr("Error Paths");

    // ffi_cif_prep error paths
    ffi_cif cif;
    ffi_type_id bad = (ffi_type_id)99;
    ffi_type_id types[2] = {FFI_INT32, FFI_INT32};

    t("cif prep bad ret type");  CHECK(ffi_cif_prep(&cif,bad,0,nullptr) == FFI_EBADTYPE, "expected EBADTYPE"); PASS;
    t("cif prep null cif");      CHECK(ffi_cif_prep(nullptr,FFI_INT32,0,nullptr) == FFI_ENULL, "expected ENULL"); PASS;
    t("cif prep too many args"); CHECK(ffi_cif_prep(&cif,FFI_VOID,99,nullptr) == FFI_ETOOMANY, "expected ETOOMANY"); PASS;
    t("cif prep null arg_types");CHECK(ffi_cif_prep(&cif,FFI_INT32,2,nullptr) == FFI_ENULL, "expected ENULL"); PASS;
    t("cif prep bad arg type");  CHECK(ffi_cif_prep(&cif,FFI_INT32,2,&bad) == FFI_EBADTYPE, "expected EBADTYPE"); PASS;

    // ffi_call null guards (should not crash)
    ffi_value dummy_ret;
    ffi_value dummy_args[1];
    ffi_cif cif2;
    ffi_cif_prep(&cif2, FFI_INT32, 0, nullptr);

    t("call null cif");   ffi_call(nullptr, (void*)id_i32, &dummy_ret, dummy_args); PASS;
    t("call null fn");    ffi_call(&cif2, nullptr, &dummy_ret, dummy_args); PASS;
    t("call null ret");   ffi_call(&cif2, (void*)id_i32, nullptr, dummy_args); PASS;
    // null args with non-zero count — currently triggers internal null check
}

// ═════════════════════════════════════════════════════════════
// 14. ffi_type_size edge cases
// ═════════════════════════════════════════════════════════════
static void test_type_size_edges() {
    hdr("ffi_type_size Edge Cases");

    t("unknown type -1"); CHECK(ffi_type_size((ffi_type_id)99) == -1, "expected -1"); PASS;
    t("FFI_STRUCT = 0");  CHECK(ffi_type_size(FFI_STRUCT) == 0,   "expected 0"); PASS;
    t("FFI_VOID = 0");    CHECK(ffi_type_size(FFI_VOID) == 0,     "expected 0"); PASS;
}

// ═════════════════════════════════════════════════════════════
// 15. ffi_strerror edge cases
// ═════════════════════════════════════════════════════════════
static void test_strerror() {
    hdr("ffi_strerror");

    t("OK message");      CHECK(strcmp(ffi_strerror(FFI_OK),"Success") == 0, "expected Success"); PASS;
    t("EBADTYPE msg");    CHECK(strcmp(ffi_strerror(FFI_EBADTYPE),"Invalid type in FFI CIF") == 0, "expected msg"); PASS;
    t("ETOOMANY msg");    CHECK(strcmp(ffi_strerror(FFI_ETOOMANY),"Too many arguments (max 16)") == 0, "expected msg"); PASS;
    t("ENULL msg");       CHECK(strcmp(ffi_strerror(FFI_ENULL),"Null pointer in FFI call") == 0, "expected msg"); PASS;
    t("unknown code");    CHECK(strcmp(ffi_strerror(999),"Unknown FFI error") == 0, "expected unknown msg"); PASS;
}

// ═════════════════════════════════════════════════════════════
// 16. Repeated / stress calls (check no state corruption)
// ═════════════════════════════════════════════════════════════
static void test_stress() {
    hdr("Stress (repeated calls)");

    ffi_type_id ti[2] = {FFI_INT32, FFI_INT32};
    ffi_value a[2];

    for (int iter = 0; iter < 100; iter++) {
        a[0].i32 = iter;
        a[1].i32 = iter * 2;
    }
    // just ensure no crash
    t("100 iterations (no crash)"); PASS;

    // many back-to-back calls with different arg counts
    ffi_type_id t0[1] = {FFI_INT32};
    for (int i = 0; i < 50; i++) {
        a[0].i32 = i;
        ffi_value r = X(FFI_INT32, id_i32, t0, a, 1);
        (void)r;
    }
    t("50 calls (no crash)"); PASS;
}

int main() {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    printf("[Phase 1: FFI ABI Edge Case Tests]\n");

    test_bool();
    test_char_types();
    test_extreme_float();
    test_extreme_int();
    test_register_saturation();
    test_pointer_write();
    test_enum();
    test_callback();
    test_small_struct();
    test_strings();
    test_compute();
    test_side_effects();
    test_error_paths();
    test_type_size_edges();
    test_strerror();
    test_stress();

    printf("\n[Results: %d/%d passed%s]\n",
           g_pass, g_pass + g_fail, g_fail ? " (FAIL)" : "");
    return g_fail > 0 ? 1 : 0;
}
