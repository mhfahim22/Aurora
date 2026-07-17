#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include "runtime/string.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

/* ════════════════════════════════════════════════════════════
   Aurora Runtime — AuroraArray (mixed-type dynamic array)
   ════════════════════════════════════════════════════════════

   Each element is a tagged AuroraValue:
     tag 0 = int64
     tag 1 = double
     tag 2 = char* (heap-allocated string)
     tag 3 = AuroraArray* (nested array)
     tag 4 = char[8] SSO string (up to 7 chars)

   Also contains:
     aurora_str_concat — string concatenation via arena allocator
   ════════════════════════════════════════════════════════════ */

/* Forward-declare arena allocator from memory.cpp */
extern "C" void* aurora_alloc(size_t size);
extern "C" void aurora_panic(const char* msg);

// ── Value & Array types ───────────────────────────────────────
struct AuroraValue {
    int64_t tag;   /* 0=int, 1=float, 2=str_aurora, 3=array, 4=str_sso */
    union {
        int64_t  ival;
        double   fval;
        struct AuroraStr* astr;
        void*    aval;
        char     sso_buf[8];
    };
};

struct AuroraArray {
    AuroraValue* data;
    int64_t      len;
    int64_t      cap;
};

static inline AuroraArray* array_alloc(int64_t cap) {
    AuroraArray* a = (AuroraArray*)malloc(sizeof(AuroraArray));
    if (!a) aurora_panic("out of memory");
    a->data = (AuroraValue*)malloc(sizeof(AuroraValue) * (cap > 0 ? cap : 4));
    if (!a->data) aurora_panic("out of memory");
    a->len = 0;
    a->cap = cap > 0 ? cap : 4;
    return a;
}

extern "C" {

int64_t aurora_array_new(int64_t cap) {
    return (int64_t)(uintptr_t)array_alloc(cap);
}

void aurora_array_reserve(int64_t arr_ptr, int64_t new_cap) {
    AuroraArray* a = (AuroraArray*)(uintptr_t)arr_ptr;
    if (!a) return;
    if (new_cap > a->cap) {
        AuroraValue* new_data = (AuroraValue*)realloc(a->data, sizeof(AuroraValue) * new_cap);
        if (!new_data) aurora_panic("out of memory");
        a->data = new_data;
        a->cap = new_cap;
    }
}

void aurora_array_push_int(int64_t arr_ptr, int64_t val) {
    AuroraArray* a = (AuroraArray*)(uintptr_t)arr_ptr;
    if (!a) return;
    if (a->len >= a->cap) { AuroraValue* nd = (AuroraValue*)realloc(a->data, sizeof(AuroraValue) * a->cap * 2); if (!nd) aurora_panic("out of memory"); a->data = nd; a->cap *= 2; }
    a->data[a->len].tag  = 0;
    a->data[a->len].ival = val;
    a->len++;
}

void aurora_array_push_float(int64_t arr_ptr, double val) {
    AuroraArray* a = (AuroraArray*)(uintptr_t)arr_ptr;
    if (!a) return;
    if (a->len >= a->cap) { AuroraValue* nd = (AuroraValue*)realloc(a->data, sizeof(AuroraValue) * a->cap * 2); if (!nd) aurora_panic("out of memory"); a->data = nd; a->cap *= 2; }
    a->data[a->len].tag  = 1;
    a->data[a->len].fval = val;
    a->len++;
}

void aurora_array_push_str(int64_t arr_ptr, const char* str) {
    AuroraArray* a = (AuroraArray*)(uintptr_t)arr_ptr;
    if (!a) return;
    if (a->len >= a->cap) { AuroraValue* nd = (AuroraValue*)realloc(a->data, sizeof(AuroraValue) * a->cap * 2); if (!nd) aurora_panic("out of memory"); a->data = nd; a->cap *= 2; }
    int slen = 0; while (str[slen]) slen++;
    if (slen < 8) {
        a->data[a->len].tag = 4;
        for (int i = 0; i <= slen; i++) a->data[a->len].sso_buf[i] = str[i];
    } else {
        a->data[a->len].tag  = 2;
        a->data[a->len].astr = aurora_str_from_cstr(str);
    }
    a->len++;
}

void aurora_array_push_array(int64_t arr_ptr, int64_t nested_ptr) {
    AuroraArray* a = (AuroraArray*)(uintptr_t)arr_ptr;
    if (!a) return;
    if (a->len >= a->cap) { AuroraValue* nd = (AuroraValue*)realloc(a->data, sizeof(AuroraValue) * a->cap * 2); if (!nd) aurora_panic("out of memory"); a->data = nd; a->cap *= 2; }
    a->data[a->len].tag  = 3;
    a->data[a->len].aval = (void*)(uintptr_t)nested_ptr;
    a->len++;
}

int64_t aurora_array_get_int(int64_t arr_ptr, int64_t idx) {
    AuroraArray* a = (AuroraArray*)(uintptr_t)arr_ptr;
    if (!a) aurora_panic("null array access");
    if (idx < 0 || idx >= a->len) aurora_panic("array index out of bounds");
    AuroraValue& v = a->data[idx];
    if (v.tag == 0) return v.ival;
    if (v.tag == 1) return (int64_t)v.fval;
    return 0;
}

double aurora_array_get_float(int64_t arr_ptr, int64_t idx) {
    AuroraArray* a = (AuroraArray*)(uintptr_t)arr_ptr;
    if (!a) aurora_panic("null array access");
    if (idx < 0 || idx >= a->len) aurora_panic("array index out of bounds");
    AuroraValue& v = a->data[idx];
    if (v.tag == 1) return v.fval;
    if (v.tag == 0) return (double)v.ival;
    return 0.0;
}

const char* aurora_array_get_str(int64_t arr_ptr, int64_t idx) {
    AuroraArray* a = (AuroraArray*)(uintptr_t)arr_ptr;
    if (!a) aurora_panic("null array access");
    if (idx < 0 || idx >= a->len) aurora_panic("array index out of bounds");
    AuroraValue& v = a->data[idx];
    if (v.tag == 2) return v.astr->ptr;
    if (v.tag == 4) return v.sso_buf;
    return "";
}

int64_t aurora_array_get_tag(int64_t arr_ptr, int64_t idx) {
    AuroraArray* a = (AuroraArray*)(uintptr_t)arr_ptr;
    if (!a) aurora_panic("null array access");
    if (idx < 0 || idx >= a->len) aurora_panic("array index out of bounds");
    return a->data[idx].tag;
}

static void aurora_array_grow(AuroraArray* a, int64_t idx) {
    if (idx < a->cap) return;
    int64_t new_cap = a->cap * 2;
    while (new_cap <= idx) new_cap *= 2;
    AuroraValue* nd = (AuroraValue*)realloc(a->data, sizeof(AuroraValue) * new_cap);
    if (!nd) aurora_panic("out of memory");
    a->data = nd;
    a->cap  = new_cap;
}

static void aurora_array_fill_gap(AuroraArray* a, int64_t idx) {
    if (idx < a->len) return;
    aurora_array_grow(a, idx);
    for (int64_t i = a->len; i < idx; i++) {
        a->data[i].tag  = 0;
        a->data[i].ival = 0;
    }
    a->len = idx + 1;
}

void aurora_array_set_int(int64_t arr_ptr, int64_t idx, int64_t val) {
    AuroraArray* a = (AuroraArray*)(uintptr_t)arr_ptr;
    if (!a) aurora_panic("null array access");
    if (idx < 0) aurora_panic("array index out of bounds");
    aurora_array_fill_gap(a, idx);
    a->data[idx].tag  = 0;
    a->data[idx].ival = val;
}

void aurora_array_set_float(int64_t arr_ptr, int64_t idx, double val) {
    AuroraArray* a = (AuroraArray*)(uintptr_t)arr_ptr;
    if (!a) aurora_panic("null array access");
    if (idx < 0) aurora_panic("array index out of bounds");
    aurora_array_fill_gap(a, idx);
    a->data[idx].tag  = 1;
    a->data[idx].fval = val;
}

void aurora_array_set_str(int64_t arr_ptr, int64_t idx, const char* str) {
    AuroraArray* a = (AuroraArray*)(uintptr_t)arr_ptr;
    if (!a) aurora_panic("null array access");
    if (idx < 0) aurora_panic("array index out of bounds");
    aurora_array_fill_gap(a, idx);
    int slen = 0; while (str[slen]) slen++;
    if (slen < 8) {
        a->data[idx].tag = 4;
        for (int i = 0; i <= slen; i++) a->data[idx].sso_buf[i] = str[i];
    } else {
        a->data[idx].tag  = 2;
        a->data[idx].astr = aurora_str_from_cstr(str);
    }
}

int64_t aurora_array_len(int64_t arr_ptr) {
    AuroraArray* a = (AuroraArray*)(uintptr_t)arr_ptr;
    return a ? a->len : 0;
}

void aurora_array_print(int64_t arr_ptr);
static void aurora_array_print_inner(int64_t arr_ptr) {
    AuroraArray* a = (AuroraArray*)(uintptr_t)arr_ptr;
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD w;
    WriteFile(hOut, "[", 1, &w, nullptr);
#else
    printf("[");
#endif
    if (a) {
        for (int64_t i = 0; i < a->len; i++) {
            AuroraValue& v = a->data[i];
            if (v.tag == 0) {
                char buf[32]; int len = 0;
                int64_t val = v.ival;
                if (val < 0) { buf[len++] = '-'; val = -val; }
                if (val == 0) buf[len++] = '0';
                else { char t[20]; int tl=0; while(val>0){t[tl++]='0'+(int)(val%10);val/=10;}
                       for(int j=tl-1;j>=0;j--) buf[len++]=t[j]; }
#ifdef _WIN32
                WriteFile(hOut, buf, len, &w, nullptr);
#else
                printf("%.*s", len, buf);
#endif
            } else if (v.tag == 1) {
                char buf[32]; int len = 0;
                double fv = v.fval;
                if (fv < 0) { buf[len++]='-'; fv=-fv; }
                int64_t ip=(int64_t)fv; double fr=fv-(double)ip;
                if (ip==0) buf[len++]='0';
                else { char t[20];int tl=0;while(ip>0){t[tl++]='0'+(int)(ip%10);ip/=10;}
                       for(int j=tl-1;j>=0;j--) buf[len++]=t[j]; }
                buf[len++]='.';
                for(int k=0;k<2;k++){fr*=10;int d=(int)fr;buf[len++]='0'+d;fr-=d;}
#ifdef _WIN32
                WriteFile(hOut, buf, len, &w, nullptr);
#else
                printf("%.*s", len, buf);
#endif
            } else if (v.tag == 2 && v.astr && v.astr->ptr) {
#ifdef _WIN32
                WriteFile(hOut, "\"", 1, &w, nullptr);
                WriteFile(hOut, v.astr->ptr, (DWORD)v.astr->len, &w, nullptr);
                WriteFile(hOut, "\"", 1, &w, nullptr);
#else
                printf("\"%s\"", v.astr->ptr);
#endif
            } else if (v.tag == 4) {
#ifdef _WIN32
                WriteFile(hOut, "\"", 1, &w, nullptr);
                int sl=0; while(v.sso_buf[sl]) sl++;
                WriteFile(hOut, v.sso_buf, sl, &w, nullptr);
                WriteFile(hOut, "\"", 1, &w, nullptr);
#else
                printf("\"%s\"", v.sso_buf);
#endif
            } else if (v.tag == 3 && v.aval) {
                aurora_array_print_inner((int64_t)(uintptr_t)v.aval);
            }
            if (i + 1 < a->len) {
#ifdef _WIN32
                WriteFile(hOut, ", ", 2, &w, nullptr);
#else
                printf(", ");
#endif
            }
        }
    }
#ifdef _WIN32
    WriteFile(hOut, "]", 1, &w, nullptr);
#else
    printf("]");
#endif
}

void aurora_array_print(int64_t arr_ptr) {
    aurora_array_print_inner(arr_ptr);
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD w;
    WriteFile(hOut, "\n", 1, &w, nullptr);
#else
    printf("\n");
#endif
}


void aurora_array_free(int64_t arr_ptr) {
    if (arr_ptr == 0) return;
    AuroraArray* a = (AuroraArray*)(uintptr_t)arr_ptr;
    if (!a->data) return;
    /* Recursively free nested arrays */
    for (int64_t i = 0; i < a->len; i++) {
        if (a->data[i].tag == 3 && a->data[i].aval) {
            aurora_array_free((int64_t)(uintptr_t)a->data[i].aval);
            a->data[i].aval = 0;
        }
        /* Free AuroraStr strings */
        if (a->data[i].tag == 2 && a->data[i].astr) {
            aurora_str_free(a->data[i].astr);
            a->data[i].astr = 0;
        }
    }
    free(a->data);
    a->data = nullptr;
    a->len = 0;
    free(a);
}

/* ════════════════════════════════════════════════════════════
   String concatenation — AuroraStr-based
   ════════════════════════════════════════════════════════════
   Uses AuroraStr{ptr, len, cap} with exponential growth,
   turning repeated `s = s + x` from O(n²) into O(n).
   ════════════════════════════════════════════════════════════ */

AuroraStr* aurora_str_concat(AuroraStr* a, AuroraStr* b) {
    if (!a) return b ? aurora_str_from_cstr(b->ptr) : aurora_str_new(0);
    if (!b) return a;
    aurora_str_append(a, b);
    aurora_str_free(b);
    return a;
}

/* ── Contains / Copy (unchanged logic) ── */

int64_t aurora_array_contains_int(int64_t arr_ptr, int64_t val) {
    AuroraArray* a = (AuroraArray*)(uintptr_t)arr_ptr;
    if (!a) return 0;
    for (int64_t i = 0; i < a->len; i++) {
        if (a->data[i].tag == 0 && a->data[i].ival == val)
            return 1;
    }
    return 0;
}

int64_t aurora_array_copy(int64_t src_ptr) {
    AuroraArray* src = (AuroraArray*)(uintptr_t)src_ptr;
    if (!src) return 0;
    int64_t dst_ptr = aurora_array_new(src->cap > 0 ? src->cap : 1);
    AuroraArray* dst = (AuroraArray*)(uintptr_t)dst_ptr;
    for (int64_t i = 0; i < src->len; i++) {
        AuroraValue el = src->data[i];
        if (el.tag == 2) {
            /* Deep copy AuroraStr — push_str will re-create from raw ptr */
            aurora_array_push_str(dst_ptr, el.astr->ptr);
        } else if (el.tag == 3) {
            int64_t nested = aurora_array_copy((int64_t)(uintptr_t)el.aval);
            dst->data[dst->len].tag  = 3;
            dst->data[dst->len].aval = (void*)(uintptr_t)nested;
            dst->len++;
        } else if (el.tag == 1) {
            aurora_array_push_float(dst_ptr, el.fval);
        } else {
            aurora_array_push_int(dst_ptr, el.ival);
        }
    }
    return dst_ptr;
}

} /* extern "C" */
