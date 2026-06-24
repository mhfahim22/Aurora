#include <cstdint>
#include <cmath>

/* ════════════════════════════════════════════════════════════
   Aurora Runtime — Built-in Functions
   ════════════════════════════════════════════════════════════
   Implementation of built-in functions that need runtime support.
   Compiler-side registration is in codegen/ and semantic/.
   ════════════════════════════════════════════════════════════ */

/* Forward declarations from other runtime files */
extern "C" {
    int64_t aurora_array_len(int64_t arr_ptr);
    int64_t aurora_array_get_int(int64_t arr_ptr, int64_t idx);
    double  aurora_array_get_float(int64_t arr_ptr, int64_t idx);
    void    aurora_panic(const char* msg);
}

extern "C" {

/* ── len(arr) / len(str) ── */
int64_t aurora_builtin_len(int64_t value) {
    /* For arrays, aurora_array_len works directly */
    return aurora_array_len(value);
}

/* ── sum(arr) ── */
int64_t aurora_builtin_sum(int64_t arr_ptr) {
    int64_t len = aurora_array_len(arr_ptr);
    int64_t total = 0;
    for (int64_t i = 0; i < len; i++) {
        total += aurora_array_get_int(arr_ptr, i);
    }
    return total;
}

/* ── min(arr) ── */
int64_t aurora_builtin_min(int64_t arr_ptr) {
    int64_t len = aurora_array_len(arr_ptr);
    if (len <= 0) { aurora_panic("min() called on empty array"); return 0; }
    int64_t min_val = aurora_array_get_int(arr_ptr, 0);
    for (int64_t i = 1; i < len; i++) {
        int64_t v = aurora_array_get_int(arr_ptr, i);
        if (v < min_val) min_val = v;
    }
    return min_val;
}

/* ── max(arr) ── */
int64_t aurora_builtin_max(int64_t arr_ptr) {
    int64_t len = aurora_array_len(arr_ptr);
    if (len <= 0) { aurora_panic("max() called on empty array"); return 0; }
    int64_t max_val = aurora_array_get_int(arr_ptr, 0);
    for (int64_t i = 1; i < len; i++) {
        int64_t v = aurora_array_get_int(arr_ptr, i);
        if (v > max_val) max_val = v;
    }
    return max_val;
}

/* ── range(start, end) or range(end) ──
   Returns a new array [start, start+1, ..., end-1] */
int64_t aurora_builtin_range(int64_t start, int64_t end) {
    /* Forward-declare array_new */
    extern int64_t aurora_array_new(int64_t cap);
    extern void aurora_array_push_int(int64_t arr_ptr, int64_t val);

    int64_t arr = aurora_array_new(end - start > 0 ? end - start : 1);
    for (int64_t i = start; i < end; i++) {
        aurora_array_push_int(arr, i);
    }
    return arr;
}

/* ── pow(base, exp) — floating-point power ── */
double aurora_pow(double base, double exp) {
    return std::pow(base, exp);
}

} /* extern "C" */
