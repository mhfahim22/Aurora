#include <cstdint>
#include <cstdlib>
#include "runtime/string.hpp"

/* ════════════════════════════════════════════════════════════
   Aurora Runtime — String Module
   ════════════════════════════════════════════════════════════
   Basic string utility functions.
   String concatenation lives in array.cpp (aurora_str_concat).
   ════════════════════════════════════════════════════════════ */

extern "C" void* aurora_alloc(size_t size);

extern "C" {

int64_t aurora_strlen(const char* str) {
    if (!str) return 0;
    /* Called from LLVM with AuroraStr* — use header len
       WARNING: MUST receive an AuroraStr*, not a raw C string.
       Casts str to AuroraStr* and reads the len field. */
    AuroraStr* s = (AuroraStr*)str;
    return (int64_t)s->len;
}

/* Helper: extract raw C string from AuroraStr* (passed as const char*) */
static const char* aurora_cstr(const void* s) {
    return s ? ((const AuroraStr*)s)->ptr : nullptr;
}
static int64_t aurora_cstr_len(const void* s) {
    return s ? (int64_t)((const AuroraStr*)s)->len : 0;
}

int64_t aurora_strcmp(const char* a, const char* b) {
    const char* sa = aurora_cstr(a);
    const char* sb = aurora_cstr(b);
    if (!sa && !sb) return 0;
    if (!sa) return -1;
    if (!sb) return 1;
    while (*sa && *sb) {
        if (*sa != *sb) return (int64_t)(unsigned char)*sa - (int64_t)(unsigned char)*sb;
        sa++;
        sb++;
    }
    return (int64_t)(unsigned char)*sa - (int64_t)(unsigned char)*sb;
}

AuroraStr* aurora_substr(const char* str, int64_t start, int64_t len) {
    const char* s = aurora_cstr(str);
    if (!s || start < 0 || len <= 0) {
        return aurora_str_new(0);
    }
    int64_t slen = aurora_cstr_len(str);
    if (start >= slen) {
        return aurora_str_new(0);
    }
    if (start + len > slen) len = slen - start;
    AuroraStr* result = aurora_str_new((size_t)len + 1);
    for (int64_t i = 0; i < len; i++) result->ptr[i] = s[start + i];
    result->ptr[len] = '\0';
    result->len = (size_t)len;
    return result;
}

int64_t aurora_str_index(const char* str, const char* sub) {
    const char* s = aurora_cstr(str);
    const char* ss = aurora_cstr(sub);
    if (!s || !ss) return -1;
    int64_t slen = aurora_cstr_len(str);
    int64_t sublen = aurora_cstr_len(sub);
    if (sublen == 0) return 0;
    if (sublen > slen) return -1;
    for (int64_t i = 0; i <= slen - sublen; i++) {
        int64_t j = 0;
        while (j < sublen && s[i + j] == ss[j]) j++;
        if (j == sublen) return i;
    }
    return -1;
}

int64_t aurora_str_contains(const char* str, const char* sub) {
    return aurora_str_index(str, sub) >= 0 ? 1 : 0;
}

AuroraStr* aurora_str_upper(const char* str) {
    const char* s = aurora_cstr(str);
    if (!s) return aurora_str_new(0);
    int64_t len = aurora_cstr_len(str);
    AuroraStr* result = aurora_str_new((size_t)len + 1);
    for (int64_t i = 0; i < len; i++) {
        char c = s[i];
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        result->ptr[i] = c;
    }
    result->ptr[len] = '\0';
    result->len = (size_t)len;
    return result;
}

AuroraStr* aurora_str_lower(const char* str) {
    const char* s = aurora_cstr(str);
    if (!s) return aurora_str_new(0);
    int64_t len = aurora_cstr_len(str);
    AuroraStr* result = aurora_str_new((size_t)len + 1);
    for (int64_t i = 0; i < len; i++) {
        char c = s[i];
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        result->ptr[i] = c;
    }
    result->ptr[len] = '\0';
    result->len = (size_t)len;
    return result;
}

int64_t aurora_str_index_c(const char* str, const char* sub) {
    /* Raw C-string version for internal use */
    if (!str || !sub || !*sub) return -1;
    int64_t slen = 0, sublen = 0;
    while (str[slen]) slen++;
    while (sub[sublen]) sublen++;
    if (sublen > slen) return -1;
    for (int64_t i = 0; i <= slen - sublen; i++) {
        int64_t j = 0;
        while (j < sublen && str[i + j] == sub[j]) j++;
        if (j == sublen) return i;
    }
    return -1;
}

} /* extern "C" */
