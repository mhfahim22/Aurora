#pragma once
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>
#include <functional>

/* ════════════════════════════════════════════════════════════
   utils.hpp — Common Utility Functions
   ════════════════════════════════════════════════════════════ */

/* ── String trimming ── */
inline std::string trim(const std::string& s) {
    auto front = std::find_if_not(s.begin(), s.end(),
                                  [](unsigned char c) { return std::isspace(c); });
    auto back  = std::find_if_not(s.rbegin(), s.rend(),
                                  [](unsigned char c) { return std::isspace(c); }).base();
    return (front < back) ? std::string(front, back) : "";
}

/* ── String split ── */
inline std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> parts;
    std::istringstream stream(s);
    std::string part;
    while (std::getline(stream, part, delim))
        parts.push_back(part);
    return parts;
}

/* ── String join ── */
inline std::string join(const std::vector<std::string>& parts, const std::string& sep) {
    std::ostringstream oss;
    for (size_t i = 0; i < parts.size(); i++) {
        if (i > 0) oss << sep;
        oss << parts[i];
    }
    return oss.str();
}

/* ── Case-insensitive comparison ── */
inline bool iequals(const std::string& a, const std::string& b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
                      [](unsigned char ca, unsigned char cb) {
                          return std::tolower(ca) == std::tolower(cb);
                      });
}

/* ── Hash combine (for custom hash types) ── */
inline void hash_combine(std::size_t& seed) {}

template <typename T, typename... Rest>
inline void hash_combine(std::size_t& seed, const T& v, Rest... rest) {
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    hash_combine(seed, rest...);
}
