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

int64_t aurora_strcmp(const char* a, const char* b) {
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    while (*a && *b) {
        if (*a != *b) return (int64_t)(unsigned char)*a - (int64_t)(unsigned char)*b;
        a++;
        b++;
    }
    return (int64_t)(unsigned char)*a - (int64_t)(unsigned char)*b;
}

AuroraStr* aurora_substr(const char* str, int64_t start, int64_t len) {
    if (!str || start < 0 || len <= 0) {
        return aurora_str_new(0);
    }
    int64_t slen = 0;
    while (str[slen]) slen++;
    if (start >= slen) {
        return aurora_str_new(0);
    }
    if (start + len > slen) len = slen - start;
    AuroraStr* result = aurora_str_new((size_t)len + 1);
    for (int64_t i = 0; i < len; i++) result->ptr[i] = str[start + i];
    result->ptr[len] = '\0';
    result->len = (size_t)len;
    return result;
}

int64_t aurora_str_index(const char* str, const char* sub) {
    if (!str || !sub || !*sub) return -1;
    int64_t slen = 0;
    while (str[slen]) slen++;
    int64_t sublen = 0;
    while (sub[sublen]) sublen++;
    for (int64_t i = 0; i <= slen - sublen; i++) {
        int64_t j = 0;
        while (j < sublen && str[i + j] == sub[j]) j++;
        if (j == sublen) return i;
    }
    return -1;
}

int64_t aurora_str_contains(const char* str, const char* sub) {
    return aurora_str_index(str, sub) >= 0 ? 1 : 0;
}

AuroraStr* aurora_str_upper(const char* str) {
    if (!str) return aurora_str_new(0);
    int64_t len = 0;
    while (str[len]) len++;
    AuroraStr* result = aurora_str_new((size_t)len + 1);
    for (int64_t i = 0; i < len; i++) {
        char c = str[i];
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        result->ptr[i] = c;
    }
    result->ptr[len] = '\0';
    result->len = (size_t)len;
    return result;
}

AuroraStr* aurora_str_lower(const char* str) {
    if (!str) return aurora_str_new(0);
    int64_t len = 0;
    while (str[len]) len++;
    AuroraStr* result = aurora_str_new((size_t)len + 1);
    for (int64_t i = 0; i < len; i++) {
        char c = str[i];
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        result->ptr[i] = c;
    }
    result->ptr[len] = '\0';
    result->len = (size_t)len;
    return result;
}

} /* extern "C" */
