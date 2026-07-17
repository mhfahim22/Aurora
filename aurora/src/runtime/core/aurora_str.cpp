#include "runtime/string.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>

extern "C" void aurora_panic(const char* msg);

/* ── Allocate a new AuroraStr with given capacity ── */
AuroraStr* aurora_str_new(size_t cap) {
    if (cap < 1) cap = 1;
    AuroraStr* s = (AuroraStr*)malloc(sizeof(AuroraStr));
    if (!s) { aurora_panic("out of memory (AuroraStr)"); return nullptr; }
    s->ptr = (char*)malloc(cap);
    if (!s->ptr) { free(s); aurora_panic("out of memory (AuroraStr data)"); return nullptr; }
    s->ptr[0] = '\0';
    s->len = 0;
    s->cap = cap;
    return s;
}

/* ── Free an AuroraStr (frees both struct and data) ── */
void aurora_str_free(AuroraStr* s) {
    if (!s) return;
    free(s->ptr);
    free(s);
}

/* ── Append string b onto string a, reusing a's buffer with exponential growth ── */
/* Returns a (modified in-place). Does NOT free b — caller owns b's lifetime. */
extern "C" AuroraStr* aurora_str_append(AuroraStr* a, AuroraStr* b) {
    if (!a) return b ? aurora_str_from_cstr(b->ptr) : aurora_str_new(0);
    if (!b) return a;
    size_t new_len = a->len + b->len;
    if (a->cap < new_len + 1) {
        size_t new_cap = a->cap * 2;
        if (new_cap < new_len + 1) new_cap = new_len + 1;
        if (new_cap < 16) new_cap = 16;
        char* new_ptr = (char*)realloc(a->ptr, new_cap);
        if (!new_ptr) { aurora_panic("out of memory (AuroraStr data)"); return a; }
        a->ptr = new_ptr;
        a->cap = new_cap;
    }
    if (b->len > 0) memcpy(a->ptr + a->len, b->ptr, b->len);
    a->ptr[new_len] = '\0';
    a->len = new_len;
    return a;
}

/* ── Repeat source string n times, single allocation ── */
extern "C" AuroraStr* aurora_str_repeat(AuroraStr* src, int64_t n) {
    if (!src || n <= 0) return aurora_str_new(0);
    size_t src_len = src->len;
    size_t total = src_len * (size_t)n;
    size_t cap = total + 1;
    if (cap < 16) cap = 16;
    AuroraStr* r = aurora_str_new(cap);
    r->len = total;
    char* p = r->ptr;
    for (int64_t i = 0; i < n; i++) {
        memcpy(p, src->ptr, src_len);
        p += src_len;
    }
    *p = '\0';
    return r;
}

/* ── Create AuroraStr from a C string (copies data) ── */
AuroraStr* aurora_str_from_cstr(const char* cstr) {
    if (!cstr) cstr = "";
    size_t l = strlen(cstr);
    size_t cap = l + 1;
    if (cap < 16) cap = 16;
    AuroraStr* s = aurora_str_new(cap);
    if (l > 0) memcpy(s->ptr, cstr, l);
    s->ptr[l] = '\0';
    s->len = l;
    return s;
}

/* ── Create AuroraStr from raw parts (takes ownership of ptr) ── */
AuroraStr* aurora_str_from_parts(char* ptr, size_t len, size_t cap) {
    AuroraStr* s = (AuroraStr*)malloc(sizeof(AuroraStr));
    if (!s) { aurora_panic("out of memory (AuroraStr)"); return nullptr; }
    s->ptr  = ptr;
    s->len  = len;
    s->cap  = cap;
    return s;
}

/* ── Convert int64_t to AuroraStr ── */
AuroraStr* aurora_int_to_str(int64_t val) {
    char buf[32];
    int len = 0;
    if (val < 0) {
        buf[len++] = '-';
        if (val == INT64_MIN) {
            buf[len++] = '9';
            int64_t rem = 223372036854775808LL;
            char tmp[20]; int tlen = 0;
            while (rem > 0) { tmp[tlen++] = '0' + (int)(rem % 10); rem /= 10; }
            for (int i = tlen - 1; i >= 0; i--) buf[len++] = tmp[i];
        } else {
            val = -val;
            if (val == 0) { buf[len++] = '0'; }
            else {
                char tmp[20]; int tlen = 0;
                while (val > 0) { tmp[tlen++] = '0' + (int)(val % 10); val /= 10; }
                for (int i = tlen - 1; i >= 0; i--) buf[len++] = tmp[i];
            }
        }
    } else if (val == 0) {
        buf[len++] = '0';
    } else {
        char tmp[20]; int tlen = 0;
        while (val > 0) { tmp[tlen++] = '0' + (int)(val % 10); val /= 10; }
        for (int i = tlen - 1; i >= 0; i--) buf[len++] = tmp[i];
    }
    buf[len] = '\0';
    size_t cap = len + 1;
    if (cap < 16) cap = 16;
    AuroraStr* s = aurora_str_new(cap);
    if (len > 0) memcpy(s->ptr, buf, len);
    s->ptr[len] = '\0';
    s->len = len;
    return s;
}

/* ── Convert double to AuroraStr (decimal format) ── */
AuroraStr* aurora_float_to_str(double val) {
    char buf[64];
#if defined(_MSC_VER)
    int len = _snprintf(buf, sizeof(buf), "%.6g", val);
#else
    int len = snprintf(buf, sizeof(buf), "%.6g", val);
#endif
    if (len < 0) { buf[0] = '\0'; len = 0; }
    if ((size_t)len >= sizeof(buf)) { len = (int)sizeof(buf) - 1; buf[len] = '\0'; }
    size_t cap = (size_t)len + 1;
    if (cap < 16) cap = 16;
    AuroraStr* s = aurora_str_new(cap);
    if (len > 0) memcpy(s->ptr, buf, (size_t)len);
    s->ptr[len] = '\0';
    s->len = (size_t)len;
    return s;
}

/* ── Zero-copy: return internal C string pointer (no allocation) ── */
extern "C" const char* aurora_str_as_cstr(const void* aurora_str_ptr) {
    if (!aurora_str_ptr) return nullptr;
    const AuroraStr* s = static_cast<const AuroraStr*>(aurora_str_ptr);
    if (!s->ptr || s->len == SIZE_MAX) return nullptr;
    return s->ptr;
}

/* ── Ensure capacity (grows if needed, 2x strategy) ── */
void aurora_str_reserve(AuroraStr* s, size_t needed) {
    if (!s || needed <= s->cap) return;
    size_t new_cap = s->cap * 2;
    if (new_cap < 16) new_cap = 16;
    while (new_cap < needed) new_cap *= 2;
    char* new_ptr = (char*)realloc(s->ptr, new_cap);
    if (!new_ptr) { aurora_panic("out of memory (aurora_str_reserve)"); return; }
    s->ptr = new_ptr;
    s->cap = new_cap;
}
