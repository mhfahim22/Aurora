// C File I/O Benchmark
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

int main() {
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);

    const char* line = "Hello World! This is a benchmark line for Aurora file I/O testing.\n";
    size_t line_len = strlen(line);
    size_t data_len = line_len * 200000;

    char* data = (char*)malloc(data_len + 1);
    for (int i = 0; i < 200000; i++) {
        memcpy(data + i * line_len, line, line_len);
    }
    data[data_len] = '\0';

    QueryPerformanceCounter(&start);

    FILE* f = fopen("bench_file_c.tmp", "wb");
    if (!f) { printf("FAIL: fopen write\n"); free(data); return 1; }
    fwrite(data, 1, data_len, f);
    fclose(f);

    FILE* r = fopen("bench_file_c.tmp", "rb");
    if (!r) { printf("FAIL: fopen read\n"); free(data); return 1; }
    char* buf = (char*)malloc(data_len);
    size_t bytes_read = fread(buf, 1, data_len, r);
    fclose(r);

    remove("bench_file_c.tmp");

    QueryPerformanceCounter(&end);

    double ms = (double)(end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart;
    double mb_per_sec = (double)(data_len + bytes_read) * 1000.0 / (ms * 1024 * 1024);
    printf("C FILE: %.0f ms (wrote %zu bytes, read %zu bytes, %.0f MB/s)\n",
           ms, data_len, bytes_read, mb_per_sec);

    free(data);
    free(buf);
    return 0;
}
