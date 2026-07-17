#pragma once
/* ════════════════════════════════════════════════════════════
   Aurora Runtime — String Type (AuroraStr)
   ════════════════════════════════════════════════════════════
   Clean Rust-style string:  { ptr, len, cap }
   - ptr: heap-allocated char buffer (null-terminated)
   - len: current length (excl. null)
   - cap: allocated capacity (excl. null)
   ════════════════════════════════════════════════════════════ */

#include <cstddef>
#include <cstdint>

struct AuroraStr {
    char*    ptr;
    size_t   len;
    size_t   cap;
};

/* ── Allocate a new AuroraStr with given capacity ── */
extern "C" AuroraStr* aurora_str_new(size_t cap);

/* ── Free an AuroraStr (frees both struct and data) ── */
extern "C" void aurora_str_free(AuroraStr* s);

/* ── Create AuroraStr from a C string (copies data) ── */
extern "C" AuroraStr* aurora_str_from_cstr(const char* cstr);

/* ── Create AuroraStr from raw parts (takes ownership) ── */
extern "C" AuroraStr* aurora_str_from_parts(char* ptr, size_t len, size_t cap);

/* ── Ensure capacity (grows if needed, 2x strategy) ── */
extern "C" void aurora_str_reserve(AuroraStr* s, size_t needed);

/* ── Zero-copy: get C string pointer from AuroraStr (no allocation) ── */
/* Returns s->ptr directly. Only valid while AuroraStr is alive.
   Unlike aurora_str_to_cstr, this does NOT copy or allocate. */
extern "C" const char* aurora_str_as_cstr(const void* aurora_str_ptr);

/* ── Convert int64_t to AuroraStr ── */
extern "C" AuroraStr* aurora_int_to_str(int64_t val);

/* ── Convert double to AuroraStr (decimal format) ── */
extern "C" AuroraStr* aurora_float_to_str(double val);

/* ── Append string b to string a, reusing a's buffer with exponential growth ── */
/* Does NOT free b — caller owns b's lifetime. */
extern "C" AuroraStr* aurora_str_append(AuroraStr* a, AuroraStr* b);

/* ── Repeat source string n times with single allocation ── */
extern "C" AuroraStr* aurora_str_repeat(AuroraStr* src, int64_t n);
