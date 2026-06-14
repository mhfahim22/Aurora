// Phase 1: Struct type system for Universal FFI Core
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>

#include "runtime/interop/ffi_abi.hpp"

static int g_pass = 0, g_fail = 0, g_cnt = 0;
static void hdr(const char* n) { printf("\n--- %s ---\n", n); }
static void t(const char* n) { g_cnt++; printf("  test %2d: %s ... ", g_cnt, n); }
#define PASS do { g_pass++; printf("PASS\n"); } while(0)
#define CHECK(c,m) do { if (!(c)) { g_fail++; printf("FAIL: %s\n", m); return; } } while(0)

// ═════════════════════════════════════════════════════════════
// C struct types used as FFI test fixtures
// ═════════════════════════════════════════════════════════════
typedef struct { int32_t a; int32_t b; }     Pair;      // 8 bytes
typedef struct { int64_t a; int64_t b; }      Duo;       // 16 bytes
typedef struct { int64_t a; int64_t b; int64_t c; } Triplet; // 24 bytes
typedef struct { int32_t x; }                 Wrap32;    // 4 bytes
typedef struct { int8_t  a; int32_t b; }      Mixed;     // 8 bytes (with padding 1->3)

extern "C" {

Pair     make_pair(int a, int b)              { Pair p = {a,b}; return p; }
int      sum_pair(Pair p)                     { return p.a + p.b; }
int64_t  pair_as_i64(Pair p)                  { return (int64_t)(uint32_t)p.a | ((int64_t)(uint32_t)p.b << 32); }

Duo      make_duo(int64_t a, int64_t b)       { Duo d = {a,b}; return d; }
int64_t  sum_duo(Duo d)                       { return d.a + d.b; }

Triplet  make_triplet(int64_t a, int64_t b, int64_t c) { Triplet t = {a,b,c}; return t; }
int64_t  sum_triplet(Triplet t)               { return t.a + t.b + t.c; }

Wrap32   wrap_i32(int32_t x)                  { Wrap32 w = {x}; return w; }
int32_t  unwrap_i32(Wrap32 w)                 { return w.x; }

int64_t  sum_mixed(Mixed m)                   { return (int64_t)m.a + m.b; }

void     fill_pair(Pair* p, int a, int b)     { p->a = a; p->b = b; }
Triplet  double_triplet(Triplet t)            { Triplet r = {t.a*2, t.b*2, t.c*2}; return r; }

} // extern "C"

// ═════════════════════════════════════════════════════════════
// ffi_type descriptors for our struct types
// ═════════════════════════════════════════════════════════════

static ffi_type* _pair_elems[] = { &ffi_type_int32, &ffi_type_int32 };
static ffi_type  _pair_type = { FFI_STRUCT, 2, 0, _pair_elems };

static ffi_type* _duo_elems[] = { &ffi_type_int64, &ffi_type_int64 };
static ffi_type  _duo_type = { FFI_STRUCT, 2, 0, _duo_elems };

static ffi_type* _triplet_elems[] = { &ffi_type_int64, &ffi_type_int64, &ffi_type_int64 };
static ffi_type  _triplet_type = { FFI_STRUCT, 3, 0, _triplet_elems };

static ffi_type* _wrap32_elems[] = { &ffi_type_int32 };
static ffi_type  _wrap32_type = { FFI_STRUCT, 1, 0, _wrap32_elems };

static ffi_type* _mixed_elems[] = { &ffi_type_int8, &ffi_type_int32 };
static ffi_type  _mixed_type = { FFI_STRUCT, 2, 0, _mixed_elems };

// Convenience wrapper for ffi_call_type
static void X_type(ffi_type* rt, void* fn, void* ret, int n, ffi_type** at, void** args) {
    ffi_cif2 cif;
    int rc = ffi_cif_prep_type(&cif, rt, n, at);
    if (rc != FFI_OK) { printf("  CIF prep failed: %s\n", ffi_strerror(rc)); return; }
    ffi_call_type(&cif, fn, ret, args);
}

// ═════════════════════════════════════════════════════════════
// 1. Small struct return (<=8 bytes, in RAX)
// ═════════════════════════════════════════════════════════════
static void test_small_struct_ret() {
    hdr("Small Struct Return (<=8 bytes, RAX)");

    int a[2] = {10, 20};
    void* args[2] = {&a[0], &a[1]};
    ffi_type* at[2] = {&ffi_type_int32, &ffi_type_int32};

    Pair result;
    memset(&result, 0, sizeof(result));

    t("make_pair(10,20)");
    X_type(&_pair_type, (void*)make_pair, &result, 2, at, args);
    CHECK(result.a == 10 && result.b == 20, "expected {10,20}"); PASS;

    a[0] = -5; a[1] = 100;
    t("make_pair(-5,100)");
    X_type(&_pair_type, (void*)make_pair, &result, 2, at, args);
    CHECK(result.a == -5 && result.b == 100, "expected {-5,100}"); PASS;
}

// ═════════════════════════════════════════════════════════════
// 2. Small struct arg (<=8 bytes, in register)
// ═════════════════════════════════════════════════════════════
static void test_small_struct_arg() {
    hdr("Small Struct Arg (<=8 bytes, register)");

    Pair p = {3, 7};
    void* args[1] = {&p};
    ffi_type* at[1] = {&_pair_type};

    int result = 0;
    t("sum_pair({3,7})");
    X_type(&ffi_type_int32, (void*)sum_pair, &result, 1, at, args);
    CHECK(result == 10, "expected 10"); PASS;

    p.a = -10; p.b = 25;
    t("sum_pair({-10,25})");
    X_type(&ffi_type_int32, (void*)sum_pair, &result, 1, at, args);
    CHECK(result == 15, "expected 15"); PASS;
}

// ═════════════════════════════════════════════════════════════
// 3. Large struct return (>8 bytes, hidden pointer)
// ═════════════════════════════════════════════════════════════
static void test_large_struct_ret() {
    hdr("Large Struct Return (>8 bytes, hidden ptr)");

    int64_t av[3] = {10, 20, 30};
    void* args[3] = {&av[0], &av[1], &av[2]};
    ffi_type* at[3] = {&ffi_type_int64, &ffi_type_int64, &ffi_type_int64};

    Triplet result;
    memset(&result, 0, sizeof(result));

    t("make_triplet(10,20,30)");
    X_type(&_triplet_type, (void*)make_triplet, &result, 3, at, args);
    CHECK(result.a == 10 && result.b == 20 && result.c == 30, "expected {10,20,30}"); PASS;

    av[0] = 100; av[1] = 200; av[2] = 300;
    t("make_triplet(100,200,300)");
    X_type(&_triplet_type, (void*)make_triplet, &result, 3, at, args);
    CHECK(result.a == 100 && result.b == 200 && result.c == 300, "expected {100,200,300}"); PASS;
}

// ═════════════════════════════════════════════════════════════
// 4. Large struct arg (>8 bytes, by pointer)
// ═════════════════════════════════════════════════════════════
static void test_large_struct_arg() {
    hdr("Large Struct Arg (>8 bytes, by pointer)");

    Triplet tv = {5, 10, 15};
    void* args[1] = {&tv};
    ffi_type* at[1] = {&_triplet_type};

    int64_t result = 0;

    t("sum_triplet({5,10,15})");
    X_type(&ffi_type_int64, (void*)sum_triplet, &result, 1, at, args);
    CHECK(result == 30, "expected 30"); PASS;

    tv.a = 1; tv.b = 2; tv.c = 3;
    t("sum_triplet({1,2,3})");
    X_type(&ffi_type_int64, (void*)sum_triplet, &result, 1, at, args);
    CHECK(result == 6, "expected 6"); PASS;
}

// ═════════════════════════════════════════════════════════════
// 5. Large struct return + large struct arg (chained)
// ═════════════════════════════════════════════════════════════
static void test_large_struct_chain() {
    hdr("Large Struct Chain (ret + arg)");

    // double_triplet takes Triplet, returns Triplet*2
    // Both arg and ret are large (>8 bytes)
    Triplet in = {3, 6, 9};
    void* args[1] = {&in};
    ffi_type* at[1] = {&_triplet_type};

    Triplet result;
    memset(&result, 0, sizeof(result));

    t("double_triplet({3,6,9})");
    X_type(&_triplet_type, (void*)double_triplet, &result, 1, at, args);
    CHECK(result.a == 6 && result.b == 12 && result.c == 18, "expected {6,12,18}"); PASS;

    in.a = 10; in.b = 20; in.c = 30;
    t("double_triplet({10,20,30})");
    X_type(&_triplet_type, (void*)double_triplet, &result, 1, at, args);
    CHECK(result.a == 20 && result.b == 40 && result.c == 60, "expected {20,40,60}"); PASS;
}

// ═════════════════════════════════════════════════════════════
// 6. Mix: both args are small structs
// ═════════════════════════════════════════════════════════════
static void test_mixed_small_struct() {
    hdr("Mixed Small Struct");

    Mixed m;
    m.a = 7;
    m.b = 42;
    void* args[1] = {&m};
    ffi_type* at[1] = {&_mixed_type};

    int64_t result = 0;
    t("sum_mixed({7,42})");
    X_type(&ffi_type_int64, (void*)sum_mixed, &result, 1, at, args);
    CHECK(result == 49, "expected 49"); PASS;

    m.a = -3; m.b = 10;
    t("sum_mixed({-3,10})");
    X_type(&ffi_type_int64, (void*)sum_mixed, &result, 1, at, args);
    CHECK(result == 7, "expected 7"); PASS;
}

// ═════════════════════════════════════════════════════════════
// 7. 16-byte struct return (Duo: 2 x int64, in two regs?)
// Actually on Win64, a 16-byte struct exceeds 8 bytes → hidden ptr
// ═════════════════════════════════════════════════════════════
static void test_duo() {
    hdr("16-byte Struct (Duo = 2 x int64, hidden ptr)");

    int64_t av[2] = {100, 200};
    void* args[2] = {&av[0], &av[1]};
    ffi_type* at[2] = {&ffi_type_int64, &ffi_type_int64};

    Duo result;
    memset(&result, 0, sizeof(result));

    t("make_duo(100,200)");
    X_type(&_duo_type, (void*)make_duo, &result, 2, at, args);
    CHECK(result.a == 100 && result.b == 200, "expected {100,200}"); PASS;

    // sum_duo takes Duo by value (16 bytes → hidden ptr)
    Duo d = {7, 8};
    args[0] = &d;
    ffi_type* at2[1] = {&_duo_type};

    int64_t sresult = 0;
    t("sum_duo({7,8})");
    X_type(&ffi_type_int64, (void*)sum_duo, &sresult, 1, at2, args);
    CHECK(sresult == 15, "expected 15"); PASS;
}

// ═════════════════════════════════════════════════════════════
// 8. ffi_type_size_full verification
// ═════════════════════════════════════════════════════════════
static void test_type_sizes() {
    hdr("ffi_type_size_full");

    t("void");     CHECK(ffi_type_size_full(&ffi_type_void) == 0,     "0"); PASS;
    t("int8");     CHECK(ffi_type_size_full(&ffi_type_int8) == 1,     "1"); PASS;
    t("int32");    CHECK(ffi_type_size_full(&ffi_type_int32) == 4,    "4"); PASS;
    t("int64");    CHECK(ffi_type_size_full(&ffi_type_int64) == 8,    "8"); PASS;
    t("float");    CHECK(ffi_type_size_full(&ffi_type_float) == 4,    "4"); PASS;
    t("double");   CHECK(ffi_type_size_full(&ffi_type_double) == 8,   "8"); PASS;
    t("pointer");  CHECK(ffi_type_size_full(&ffi_type_pointer) == 8,  "8"); PASS;

    // Struct sizes (auto-computed from elements)
    t("Pair (2xi32)");        CHECK(ffi_type_size_full(&_pair_type) == 8,     "8"); PASS;
    t("Duo (2xi64)");         CHECK(ffi_type_size_full(&_duo_type) == 16,    "16"); PASS;
    t("Triplet (3xi64)");     CHECK(ffi_type_size_full(&_triplet_type) == 24,"24"); PASS;
    t("Wrap32 (1xi32)");      CHECK(ffi_type_size_full(&_wrap32_type) == 4,   "4"); PASS;
    t("Mixed (i8+i32)");      CHECK(ffi_type_size_full(&_mixed_type) == 8,    "8 (padded)"); PASS;
}

// ═════════════════════════════════════════════════════════════
// 9. ffi_cif_prep_type validation
// ═════════════════════════════════════════════════════════════
static void test_cif_prep_type() {
    hdr("ffi_cif_prep_type Validation");

    ffi_cif2 cif;
    ffi_type* at[1] = {&ffi_type_int32};

    t("null cif");
    CHECK(ffi_cif_prep_type(NULL, &ffi_type_int32, 0, NULL) == FFI_ENULL, "expected ENULL"); PASS;

    t("null ret_type");
    CHECK(ffi_cif_prep_type(&cif, NULL, 0, NULL) == FFI_EBADTYPE, "expected EBADTYPE"); PASS;

    t("null arg_types with count");
    CHECK(ffi_cif_prep_type(&cif, &ffi_type_int32, 1, NULL) == FFI_ENULL, "expected ENULL"); PASS;

    t("hidden ptr flag");
    ffi_cif_prep_type(&cif, &_triplet_type, 0, NULL);
    CHECK(cif.flags & FFI_F_HIDDEN_RET, "expected HIDDEN_RET flag"); PASS;

    t("no hidden ptr for small");
    ffi_cif_prep_type(&cif, &_pair_type, 0, NULL);
    CHECK(!(cif.flags & FFI_F_HIDDEN_RET), "expected no flag"); PASS;
}

// ═════════════════════════════════════════════════════════════
// 10. Pair return via old-style API (backward compat)
// ═════════════════════════════════════════════════════════════
static void test_pair_old_api() {
    hdr("Pair via Old-Style API");

    // Pair is 8 bytes, returned in RAX. Old API sees it as FFI_INT64.
    ffi_cif cif;
    ffi_type_id at[2] = {FFI_INT32, FFI_INT32};
    ffi_cif_prep(&cif, FFI_INT64, 2, at);

    ffi_value args[2];
    args[0].i32 = 10;
    args[1].i32 = 20;

    ffi_value ret;
    memset(&ret, 0, sizeof(ret));
    ffi_call(&cif, (void*)make_pair, &ret, args);

    t("make_pair via old API");;
    int64_t raw = ret.i64;
    Pair* p = (Pair*)&raw;
    CHECK(p->a == 10 && p->b == 20, "expected {10,20}"); PASS;
}

int main() {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    printf("[Phase 1: Struct FFI Tests]\n");

    test_type_sizes();
    test_cif_prep_type();
    test_small_struct_ret();
    test_small_struct_arg();
    test_large_struct_ret();
    test_large_struct_arg();
    test_large_struct_chain();
    test_mixed_small_struct();
    test_duo();
    test_pair_old_api();

    printf("\n[Results: %d/%d passed%s]\n",
           g_pass, g_pass + g_fail, g_fail ? " (FAIL)" : "");
    return g_fail > 0 ? 1 : 0;
}
