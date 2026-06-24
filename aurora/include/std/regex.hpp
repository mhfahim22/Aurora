#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

int     aurora_regex_match(const char* pattern, const char* text);
char*   aurora_regex_replace(const char* pattern, const char* text, const char* replacement);
int     aurora_regex_search(const char* pattern, const char* text, char* buffer, int buffer_size);
int     aurora_regex_count(const char* pattern, const char* text);

#ifdef __cplusplus
}
#endif