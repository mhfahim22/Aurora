/* ════════════════════════════════════════════════════════════
   Aurora WASM Browser Runtime (Phase 40)
   Self-contained C runtime compiled with:
     clang --target=wasm32-unknown-unknown -nostdlib -O2

   Provides everything the aurorac codegen links against for a
   browser-native (no WASI, no threads, no filesystem) target:
     - bump allocator (malloc/free/realloc/calloc)
     - memcpy/memset/strlen builtins clang emits calls to
     - i64 division helpers (wasm has no i64 div instruction)
     - AuroraStr string functions (same ABI as runtime/string.hpp)
     - AuroraArray functions (same ABI as runtime/core/array.cpp)
     - aurora_gc_alloc/free/register_root (GC no-op: leak, browser)
     - aurora_arena_alloc (bump alias)
     - output functions -> JS imports (console)
     - aurora_panic -> JS import (throw)

   Undefined `aurora_js_*` symbols become imports from module "env"
   when linked with `wasm-ld --import-undefined`; the JS glue
   (wasm_glue.js) provides them.
   ════════════════════════════════════════════════════════════ */

typedef unsigned int   size_t;
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long  uintptr_t;
typedef unsigned long long uint64_t;

#define NULL ((void*)0)
#define HEAP_BASE_ALIGN 8u

extern unsigned char __heap_base;

/* ── Bump allocator over linear memory ── */
static uint8_t* g_heap_next = 0;
static uint8_t* g_heap_end  = 0;
static const unsigned int MEM_PAGE = 65536u;

static void heap_init(void) {
    uintptr_t base = (uintptr_t)&__heap_base;
    g_heap_next = (uint8_t*)((base + HEAP_BASE_ALIGN - 1u) & ~(uintptr_t)(HEAP_BASE_ALIGN - 1u));
    g_heap_end  = g_heap_next + 8u * MEM_PAGE;
}

void* malloc(size_t n) {
    if (!g_heap_next) heap_init();
    if (n == 0) n = 1;
    n = (n + 7u) & ~7u;
    uint8_t* p = g_heap_next;
    uint8_t* next = p + n;
    if (next > g_heap_end) {
        unsigned int cur = (unsigned int)((uintptr_t)p / MEM_PAGE);
        unsigned int need = (unsigned int)((n + MEM_PAGE - 1u) / MEM_PAGE) + 1u;
        __builtin_wasm_memory_grow(0, need);
        uintptr_t new_end = (uintptr_t)g_heap_end + (uintptr_t)need * MEM_PAGE;
        g_heap_end = (uint8_t*)new_end;
        next = p + n;
        if (next > g_heap_end) return NULL;
    }
    g_heap_next = next;
    return p;
}

void free(void* p) { (void)p; }
void* realloc(void* p, size_t n) {
    if (!p) return malloc(n);
    void* np = malloc(n);
    if (np) {
        /* copy old data (best-effort: we don't know old size, copy n bytes
           from p; callers here are AuroraStr/array which copy within cap) */
        uint8_t* s = (uint8_t*)p;
        uint8_t* d = (uint8_t*)np;
        for (size_t i = 0; i < n; i++) d[i] = s[i];
    }
    return np;
}
void* calloc(size_t n, size_t s) {
    size_t total = n * s;
    uint8_t* p = (uint8_t*)malloc(total);
    if (p) for (size_t i = 0; i < total; i++) p[i] = 0;
    return p;
}

/* ── Compiler builtins clang emits for wasm32 ── */
void* memcpy(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}
void* memmove(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    if (d < s) { for (size_t i = 0; i < n; i++) d[i] = s[i]; }
    else if (d > s) { for (size_t i = n; i > 0; i--) d[i-1] = s[i-1]; }
    return dst;
}
void* memset(void* dst, int c, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    for (size_t i = 0; i < n; i++) d[i] = (uint8_t)c;
    return dst;
}
int memcmp(const void* a, const void* b, size_t n) {
    const uint8_t* x = (const uint8_t*)a;
    const uint8_t* y = (const uint8_t*)b;
    for (size_t i = 0; i < n; i++) { if (x[i] != y[i]) return (int)x[i] - (int)y[i]; }
    return 0;
}
size_t strlen(const char* s) {
    size_t n = 0; while (s[n]) n++; return n;
}
int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}
int strncmp(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return (int)(unsigned char)a[i] - (int)(unsigned char)b[i];
        if (!a[i]) return 0;
    }
    return 0;
}
int snprintf(char* buf, size_t n, const char* fmt, ...) {
    (void)fmt;
    if (n > 0) buf[0] = '\0';
    return 0;
}
void abort(void) {
    /* JS import wired by glue */
    extern void aurora_js_abort(void);
    aurora_js_abort();
}

/* i64 division helpers (wasm has no i64.div) */
static uint64_t udivmod(uint64_t a, uint64_t b, uint64_t* rem) {
    if (b == 0) { *rem = 0; return 0; }
    uint64_t q = 0, r = 0;
    for (int i = 63; i >= 0; i--) {
        r = (r << 1) | ((a >> i) & 1u);
        if (r >= b) { r -= b; q |= (1ull << i); }
    }
    *rem = r;
    return q;
}
long long __udivdi3(unsigned long long a, unsigned long long b) {
    unsigned long long rem;
    return (long long)udivmod(a, b, &rem);
}
long long __umoddi3(unsigned long long a, unsigned long long b) {
    unsigned long long rem;
    udivmod(a, b, &rem);
    return (long long)rem;
}
long long __divdi3(long long a, long long b) {
    int neg = 0;
    unsigned long long ua = (unsigned long long)a, ub = (unsigned long long)b;
    if (a < 0) { ua = (unsigned long long)(-(a + 1)) + 1; neg ^= 1; }
    if (b < 0) { ub = (unsigned long long)(-(b + 1)) + 1; neg ^= 1; }
    unsigned long long rem;
    unsigned long long q = udivmod(ua, ub, &rem);
    return neg ? -(long long)q : (long long)q;
}
long long __moddi3(long long a, long long b) {
    int neg = 0;
    unsigned long long ua = (unsigned long long)a, ub = (unsigned long long)b;
    if (a < 0) { ua = (unsigned long long)(-(a + 1)) + 1; neg = 1; }
    if (b < 0) { ub = (unsigned long long)(-(b + 1)) + 1; }
    unsigned long long rem;
    udivmod(ua, ub, &rem);
    return neg ? -(long long)rem : (long long)rem;
}

/* ── JS imports (module "env") ── */
extern void aurora_js_console_log_str(const char* s);
extern void aurora_js_console_log_int(long long v);
extern void aurora_js_console_log_float(double v);
extern void aurora_js_console_log_bool(long long v);
extern void aurora_js_console_log_raw(void);
extern void aurora_js_abort(void);

/* ── AuroraStr ABI (must match runtime/string.hpp) ── */
typedef struct AuroraStr {
    char*   ptr;
    size_t  len;
    size_t  cap;
    int     shared;   /* 1 = shared/immutable (cached literal) — append must clone */
} AuroraStr;

static size_t astr_cap_for(size_t len) {
    size_t cap = len + 1;
    if (cap < 16) cap = 16;
    return cap;
}

void* aurora_str_new(size_t cap) {
    if (cap < 1) cap = 1;
    AuroraStr* s = (AuroraStr*)malloc(sizeof(AuroraStr));
    s->ptr = (char*)malloc(cap);
    s->ptr[0] = '\0';
    s->len = 0;
    s->cap = cap;
    s->shared = 0;
    return s;
}
void aurora_str_free(void* p) {
    AuroraStr* s = (AuroraStr*)p;
    if (!s) return;
    free(s->ptr);
    free(s);
}
void* aurora_str_from_cstr(const char* cstr) {
    if (!cstr) cstr = "";
    size_t l = strlen(cstr);
    size_t cap = astr_cap_for(l);
    AuroraStr* s = (AuroraStr*)aurora_str_new(cap);
    for (size_t i = 0; i < l; i++) s->ptr[i] = cstr[i];
    s->ptr[l] = '\0';
    s->len = l;
    return s;
}
void* aurora_str_literal(const char* cstr) {
    AuroraStr* s = (AuroraStr*)aurora_str_from_cstr(cstr);
    if (s) s->shared = 1;
    return s;
}
void* aurora_str_from_parts(char* ptr, size_t len, size_t cap) {
    AuroraStr* s = (AuroraStr*)malloc(sizeof(AuroraStr));
    s->ptr = ptr; s->len = len; s->cap = cap; s->shared = 0;
    return s;
}
const char* aurora_str_as_cstr(const void* p) {
    if (!p) return NULL;
    const AuroraStr* s = (const AuroraStr*)p;
    if (!s->ptr) return NULL;
    return s->ptr;
}
void aurora_str_reserve(void* p, size_t needed) {
    AuroraStr* s = (AuroraStr*)p;
    if (!s || needed <= s->cap) return;
    size_t new_cap = s->cap * 2;
    if (new_cap < 16) new_cap = 16;
    while (new_cap < needed) new_cap *= 2;
    char* np = (char*)realloc(s->ptr, new_cap);
    s->ptr = np;
    s->cap = new_cap;
}
void* aurora_str_append(void* a, void* b) {
    AuroraStr* sa = (AuroraStr*)a;
    AuroraStr* sb = (AuroraStr*)b;
    if (!sa) return sb ? aurora_str_from_cstr(sb->ptr) : aurora_str_new(0);
    if (!sb) return sa;
    if (sa->shared) {
        AuroraStr* clone = (AuroraStr*)aurora_str_from_cstr(sa->ptr);
        if (!clone) return sa;
        return aurora_str_append(clone, sb);
    }
    size_t new_len = sa->len + sb->len;
    aurora_str_reserve(sa, new_len + 1);
    for (size_t i = 0; i < sb->len; i++) sa->ptr[sa->len + i] = sb->ptr[i];
    sa->ptr[new_len] = '\0';
    sa->len = new_len;
    return sa;
}
void* aurora_str_concat(void* a, void* b) {
    AuroraStr* sa = (AuroraStr*)a;
    AuroraStr* sb = (AuroraStr*)b;
    if (!sa && !sb) return aurora_str_new(0);
    if (!sa) return aurora_str_from_cstr(sb->ptr);
    if (!sb) return aurora_str_from_cstr(sa->ptr);
    size_t total = sa->len + sb->len;
    size_t cap = astr_cap_for(total);
    AuroraStr* r = (AuroraStr*)aurora_str_new(cap);
    for (size_t i = 0; i < sa->len; i++) r->ptr[i] = sa->ptr[i];
    for (size_t i = 0; i < sb->len; i++) r->ptr[sa->len + i] = sb->ptr[i];
    r->ptr[total] = '\0';
    r->len = total;
    return r;
}
void* aurora_str_repeat(void* p, long long n) {
    AuroraStr* src = (AuroraStr*)p;
    if (!src || n <= 0) return aurora_str_new(0);
    size_t total = src->len * (size_t)n;
    size_t cap = astr_cap_for(total);
    AuroraStr* r = (AuroraStr*)aurora_str_new(cap);
    for (long long i = 0; i < n; i++)
        for (size_t j = 0; j < src->len; j++) r->ptr[i * src->len + j] = src->ptr[j];
    r->ptr[total] = '\0';
    r->len = total;
    return r;
}
void* aurora_substr(void* p, long long start, long long len) {
    AuroraStr* s = (AuroraStr*)p;
    if (!s) return aurora_str_new(0);
    if (start < 0) start = 0;
    if ((unsigned long long)start >= (unsigned long long)s->len) return aurora_str_new(0);
    long long avail = (long long)s->len - start;
    if (len < 0 || len > avail) len = avail;
    size_t cap = astr_cap_for((size_t)len);
    AuroraStr* r = (AuroraStr*)aurora_str_new(cap);
    for (long long i = 0; i < len; i++) r->ptr[i] = s->ptr[start + i];
    r->ptr[len] = '\0';
    r->len = (size_t)len;
    return r;
}
long long aurora_str_index(void* a, void* b) {
    AuroraStr* sa = (AuroraStr*)a;
    AuroraStr* sb = (AuroraStr*)b;
    if (!sa || !sb || sb->len == 0) return -1;
    if (sb->len > sa->len) return -1;
    for (size_t i = 0; i + sb->len <= sa->len; i++) {
        size_t j = 0;
        while (j < sb->len && sa->ptr[i + j] == sb->ptr[j]) j++;
        if (j == sb->len) return (long long)i;
    }
    return -1;
}
long long aurora_str_equal(void* a, void* b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    AuroraStr* sa = (AuroraStr*)a;
    AuroraStr* sb = (AuroraStr*)b;
    if (sa->len != sb->len) return 0;
    for (size_t i = 0; i < sa->len; i++) if (sa->ptr[i] != sb->ptr[i]) return 0;
    return 1;
}

/* int/float to string (decimal, mirrors runtime) */
static void reverse(char* b, int n) {
    for (int i = 0, j = n - 1; i < j; i++, j--) { char t = b[i]; b[i] = b[j]; b[j] = t; }
}
static void* mk_str(const char* buf, int len) {
    size_t cap = astr_cap_for((size_t)len);
    AuroraStr* r = (AuroraStr*)aurora_str_new(cap);
    for (int i = 0; i < len; i++) r->ptr[i] = buf[i];
    r->ptr[len] = '\0';
    r->len = (size_t)len;
    return r;
}
void* aurora_int_to_str(long long val) {
    char buf[32];
    int len = 0;
    if (val == 0) { buf[len++] = '0'; }
    else {
        if (val < 0) { buf[len++] = '-'; val = -(val + 1) + 1; }
        char tmp[24]; int tlen = 0;
        while (val > 0) { tmp[tlen++] = (char)('0' + (val % 10)); val /= 10; }
        while (tlen > 0) buf[len++] = tmp[--tlen];
    }
    buf[len] = '\0';
    return mk_str(buf, len);
}
void* aurora_float_to_str(double v) {
    /* minimal %.6g equivalent */
    char buf[64];
    int len = 0;
    if (v != v) { return mk_str("nan", 3); }
    if (v == 1.0/0.0) { return mk_str("inf", 3); }
    if (v == -1.0/0.0) { return mk_str("-inf", 4); }
    if (v < 0) { buf[len++] = '-'; v = -v; }
    if (v == 0) { buf[len++] = '0'; }
    else if (v >= 1e15) {
        /* integer part too big: print as integer */
        long long whole = (long long)v;
        char tmp[24]; int tlen = 0;
        while (whole > 0) { tmp[tlen++] = (char)('0' + (whole % 10)); whole /= 10; }
        while (tlen > 0) buf[len++] = tmp[--tlen];
    } else {
        long long whole = (long long)v;
        char tmp[24]; int tlen = 0;
        if (whole == 0) tmp[tlen++] = '0';
        while (whole > 0) { tmp[tlen++] = (char)('0' + (whole % 10)); whole /= 10; }
        while (tlen > 0) buf[len++] = tmp[--tlen];
        double frac = v - (double)(long long)v;
        if (frac > 0) {
            buf[len++] = '.';
            int digits = 0;
            while (frac > 1e-6 && digits < 6) {
                frac *= 10.0;
                int d = (int)frac;
                buf[len++] = (char)('0' + d);
                frac -= (double)d;
                digits++;
            }
            while (len > 0 && buf[len-1] == '0') len--;
            if (buf[len-1] == '.') len--;
        }
    }
    buf[len] = '\0';
    return mk_str(buf, len);
}

/* ── AuroraArray ABI (must match runtime/core/array.cpp) ── */
typedef struct AuroraValue {
    long long tag;
    union {
        long long ival;
        double    fval;
        struct AuroraStr* astr;
        void*     aval;
        char      sso_buf[8];
    };
} AuroraValue;
typedef struct AuroraArray {
    AuroraValue* data;
    long long    len;
    long long    cap;
} AuroraArray;

static AuroraArray* array_alloc(long long cap) {
    AuroraArray* a = (AuroraArray*)malloc(sizeof(AuroraArray));
    a->data = (AuroraValue*)malloc(sizeof(AuroraValue) * (cap > 0 ? cap : 4));
    a->len = 0;
    a->cap = cap > 0 ? cap : 4;
    return a;
}
static void array_grow(AuroraArray* a) {
    long long nc = a->cap * 2;
    AuroraValue* nd = (AuroraValue*)realloc(a->data, sizeof(AuroraValue) * nc);
    a->data = nd;
    a->cap = nc;
}
long long aurora_array_new(long long cap) {
    return (long long)(size_t)array_alloc(cap);
}
void aurora_array_reserve(long long arr, long long nc) {
    AuroraArray* a = (AuroraArray*)(size_t)arr;
    if (!a || nc <= a->cap) return;
    AuroraValue* nd = (AuroraValue*)realloc(a->data, sizeof(AuroraValue) * nc);
    a->data = nd;
    a->cap = nc;
}
void aurora_array_push_int(long long arr, long long val) {
    AuroraArray* a = (AuroraArray*)(size_t)arr;
    if (!a) return;
    if (a->len >= a->cap) array_grow(a);
    a->data[a->len].tag = 0; a->data[a->len].ival = val; a->len++;
}
void aurora_array_push_float(long long arr, double val) {
    AuroraArray* a = (AuroraArray*)(size_t)arr;
    if (!a) return;
    if (a->len >= a->cap) array_grow(a);
    a->data[a->len].tag = 1; a->data[a->len].fval = val; a->len++;
}
void aurora_array_push_str(long long arr, void* s) {
    AuroraArray* a = (AuroraArray*)(size_t)arr;
    if (!a) return;
    if (a->len >= a->cap) array_grow(a);
    a->data[a->len].tag = 2; a->data[a->len].astr = (AuroraStr*)s; a->len++;
}
void aurora_array_push_array(long long arr, long long nested) {
    AuroraArray* a = (AuroraArray*)(size_t)arr;
    if (!a) return;
    if (a->len >= a->cap) array_grow(a);
    a->data[a->len].tag = 3; a->data[a->len].aval = (void*)(size_t)nested; a->len++;
}
long long aurora_array_get_int(long long arr, long long idx) {
    AuroraArray* a = (AuroraArray*)(size_t)arr;
    if (!a || idx < 0 || idx >= a->len) return 0;
    return a->data[idx].ival;
}
double aurora_array_get_float(long long arr, long long idx) {
    AuroraArray* a = (AuroraArray*)(size_t)arr;
    if (!a || idx < 0 || idx >= a->len) return 0;
    return a->data[idx].fval;
}
void* aurora_array_get_str(long long arr, long long idx) {
    AuroraArray* a = (AuroraArray*)(size_t)arr;
    if (!a || idx < 0 || idx >= a->len) return NULL;
    return a->data[idx].astr;
}
long long aurora_array_get_tag(long long arr, long long idx) {
    AuroraArray* a = (AuroraArray*)(size_t)arr;
    if (!a || idx < 0 || idx >= a->len) return -1;
    return a->data[idx].tag;
}
long long aurora_array_get_array(long long arr, long long idx) {
    AuroraArray* a = (AuroraArray*)(size_t)arr;
    if (!a || idx < 0 || idx >= a->len) return 0;
    return (long long)(size_t)a->data[idx].aval;
}
long long aurora_array_length(long long arr) {
    AuroraArray* a = (AuroraArray*)(size_t)arr;
    return a ? a->len : 0;
}
long long aurora_array_capacity(long long arr) {
    AuroraArray* a = (AuroraArray*)(size_t)arr;
    return a ? a->cap : 0;
}
void aurora_array_clear(long long arr) {
    AuroraArray* a = (AuroraArray*)(size_t)arr;
    if (a) a->len = 0;
}

/* ── GC / arena (browser: leak-only, no collection) ──
   NOTE: Aurora `int` is i64 in LLVM IR, so size params must be 64-bit
   to match the codegen-emitted signatures (i64->i32). */
void* aurora_gc_alloc(long long size) { return malloc((size_t)size); }
void* aurora_arena_alloc(long long size) { return malloc((size_t)size); }
void* aurora_arena_alloc_aligned(long long size, long long alignment) {
    void* p = malloc((size_t)size + (size_t)alignment);
    size_t addr = (size_t)p;
    size_t aligned = (addr + (size_t)alignment - 1) & ~((size_t)alignment - 1);
    return (void*)aligned;
}
void aurora_gc_free(void* p) { free(p); }
void aurora_arena_free(void) { (void)0; }
void aurora_gc_register_root(void* p) { (void)p; }
void aurora_gc_unregister_root(void* p) { (void)p; }
void aurora_gc_register_root_sized(void* p, long long s) { (void)p; (void)s; }
void aurora_gc_clear_arena_roots(void) { (void)0; }
void aurora_gc_collect(void) { (void)0; }

/* ── Output ── */
void aurora_outputln_int(long long v) { aurora_js_console_log_int(v); }
void aurora_outputln_float(double v) { aurora_js_console_log_float(v); }
void aurora_outputln_str(const char* s) { aurora_js_console_log_str(s); }
void aurora_outputln_bool(long long v) { aurora_js_console_log_bool(v); }
void aurora_outputN(void) { aurora_js_console_log_raw(); }
void aurora_print_str(const char* s) { aurora_js_console_log_str(s); }
void aurora_print_int(long long v) { aurora_js_console_log_int(v); }
void aurora_print_float(double v) { aurora_js_console_log_float(v); }
void aurora_print_bool(long long v) { aurora_js_console_log_bool(v); }

/* ── Panic ── */
void aurora_panic(const char* msg) {
    aurora_js_console_log_str(msg);
    aurora_js_abort();
}

/* exported helpers used by JS glue */
void* aurora_wasm_alloc_str(size_t len) {
    size_t cap = astr_cap_for(len);
    AuroraStr* s = (AuroraStr*)aurora_str_new(cap);
    return s;
}
void aurora_wasm_import_str(void* dst_astr, const char* src) {
    AuroraStr* s = (AuroraStr*)dst_astr;
    size_t l = strlen(src);
    if (s->cap < l + 1) { free(s->ptr); s->ptr = (char*)malloc(l + 1); s->cap = l + 1; }
    for (size_t i = 0; i < l; i++) s->ptr[i] = src[i];
    s->ptr[l] = '\0';
    s->len = l;
}
void aurora_wasm_export_str(void* src_astr, char* dst) {
    AuroraStr* s = (AuroraStr*)src_astr;
    for (size_t i = 0; i < s->len; i++) dst[i] = s->ptr[i];
    dst[s->len] = '\0';
}

/* string accessors for JS glue */
char* aurora_wasm_str_buf(void* astr) {
    AuroraStr* s = (AuroraStr*)astr;
    return s ? s->ptr : NULL;
}
long long aurora_wasm_str_len(void* astr) {
    AuroraStr* s = (AuroraStr*)astr;
    return s ? (long long)s->len : 0;
}
void aurora_wasm_str_set_len(void* astr, long long len) {
    AuroraStr* s = (AuroraStr*)astr;
    if (s) {
        s->len = (size_t)len;
        s->ptr[len] = '\0';
    }
}
char* aurora_wasm_import_buf(void* astr) {
    return aurora_wasm_str_buf(astr);
}
void aurora_wasm_import_setlen(void* astr, long long len) {
    aurora_wasm_str_set_len(astr, len);
}

/* single global Aurora event callback (index into wasm function table) */
static long long g_wasm_event_cb = 0;
void aurora_wasm_set_cb(long long fn) { g_wasm_event_cb = fn; }
long long aurora_wasm_get_cb(void) { return g_wasm_event_cb; }