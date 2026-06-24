#include "std/regex.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <regex>

extern "C" {

/* ── Check if pattern matches text (bool as int) ── */
int aurora_regex_match(const char* pattern, const char* text) {
    if (!pattern || !text) return 0;
    try {
        std::regex re(pattern);
        return std::regex_match(text, re) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

/* ── Replace first match in text ── */
char* aurora_regex_replace(const char* pattern, const char* text, const char* replacement) {
    if (!pattern || !text || !replacement) return nullptr;
    try {
        std::regex re(pattern);
        std::string result = std::regex_replace(text, re, replacement,
            std::regex_constants::format_first_only);
        return strdup(result.c_str());
    } catch (...) {
        return nullptr;
    }
}

/* ── Search for pattern, return first match in buffer ── */
int aurora_regex_search(const char* pattern, const char* text, char* buffer, int buffer_size) {
    if (!pattern || !text || !buffer || buffer_size <= 0) return -1;
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
}

/* ── Count matches ── */
int aurora_regex_count(const char* pattern, const char* text) {
    if (!pattern || !text) return 0;
    try {
        std::regex re(pattern);
        auto begin = std::cregex_iterator(text, text + strlen(text), re);
        auto end = std::cregex_iterator();
        return (int)std::distance(begin, end);
    } catch (...) {
        return 0;
    }
}

}
