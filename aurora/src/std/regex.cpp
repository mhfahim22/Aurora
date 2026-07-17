#include "std/regex.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>

#if defined(_WIN32)
#include <regex>
#else
#include <regex.h>
#endif

extern "C" {

int aurora_regex_match(const char* pattern, const char* text) {
    if (!pattern || !text) return 0;
#if defined(_WIN32)
    try {
        std::regex re(pattern);
        return std::regex_match(text, re) ? 1 : 0;
    } catch (...) {
        return 0;
    }
#else
    regex_t re;
    if (regcomp(&re, pattern, REG_EXTENDED | REG_NOSUB) != 0)
        return 0;
    int ret = regexec(&re, text, 0, nullptr, 0);
    regfree(&re);
    return ret == 0 ? 1 : 0;
#endif
}

char* aurora_regex_replace(const char* pattern, const char* text, const char* replacement) {
    if (!pattern || !text || !replacement) return nullptr;
#if defined(_WIN32)
    try {
        std::regex re(pattern);
        std::string result = std::regex_replace(text, re, replacement,
            std::regex_constants::format_first_only);
        return strdup(result.c_str());
    } catch (...) {
        return nullptr;
    }
#else
    regex_t re;
    if (regcomp(&re, pattern, REG_EXTENDED) != 0)
        return nullptr;
    regmatch_t match;
    std::string result;
    const char* p = text;
    if (regexec(&re, p, 1, &match, 0) == 0) {
        result.append(p, match.rm_so);
        result.append(replacement);
        p += match.rm_eo;
    }
    result.append(p);
    regfree(&re);
    return strdup(result.c_str());
#endif
}

int aurora_regex_search(const char* pattern, const char* text, char* buffer, int buffer_size) {
    if (!pattern || !text || !buffer || buffer_size <= 0) return -1;
#if defined(_WIN32)
    try {
        std::regex re(pattern);
        std::cmatch m;
        if (std::regex_search(text, m, re)) {
            size_t len = m[0].length();
            if (len >= (size_t)buffer_size) len = (size_t)buffer_size - 1;
            memcpy(buffer, m[0].first, len);
            buffer[len] = '\0';
            return (int)len;
        }
        return 0;
    } catch (...) {
        return -1;
    }
#else
    regex_t re;
    if (regcomp(&re, pattern, REG_EXTENDED) != 0)
        return -1;
    regmatch_t match;
    int ret = regexec(&re, text, 1, &match, 0);
    regfree(&re);
    if (ret != 0) return 0;
    size_t len = (size_t)(match.rm_eo - match.rm_so);
    if (len >= (size_t)buffer_size) len = (size_t)buffer_size - 1;
    memcpy(buffer, text + match.rm_so, len);
    buffer[len] = '\0';
    return (int)len;
#endif
}

int aurora_regex_count(const char* pattern, const char* text) {
    if (!pattern || !text) return 0;
#if defined(_WIN32)
    try {
        std::regex re(pattern);
        auto begin = std::cregex_iterator(text, text + strlen(text), re);
        auto end = std::cregex_iterator();
        return (int)std::distance(begin, end);
    } catch (...) {
        return 0;
    }
#else
    regex_t re;
    if (regcomp(&re, pattern, REG_EXTENDED) != 0)
        return 0;
    int count = 0;
    const char* p = text;
    regmatch_t match;
    while (regexec(&re, p, 1, &match, 0) == 0) {
        count++;
        p += match.rm_eo;
        if (match.rm_so == match.rm_eo) break;
    }
    regfree(&re);
    return count;
#endif
}

}
