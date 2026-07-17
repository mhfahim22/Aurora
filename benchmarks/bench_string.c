// C String Concatenation Benchmark
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

int main() {
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);

    int count = 1000000;
    size_t cap = 1024;
    char* result = (char*)malloc(cap);
    result[0] = '\0';
    size_t len = 0;

    QueryPerformanceCounter(&start);
    for (int i = 0; i < count; i++) {
        if (len + 1 >= cap) {
            cap *= 2;
            result = (char*)realloc(result, cap);
        }
        result[len++] = 'x';
        result[len] = '\0';
    }
    QueryPerformanceCounter(&end);

    double ms = (double)(end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart;
    printf("C STRING: %zu chars, %.0f ms, %.0f ns/op\n", len, ms, ms * 1e6 / count);

    free(result);
    return 0;
}
