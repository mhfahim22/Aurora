// C Matrix Multiplication Benchmark (naive O(n^3))
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

int main() {
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);

    int N = 128;
    int size = N * N;

    double* a = (double*)malloc(size * sizeof(double));
    double* b = (double*)malloc(size * sizeof(double));
    double* c = (double*)calloc(size, sizeof(double));

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            a[i * N + j] = (i * 1.0 + j) / N;
            b[i * N + j] = (j * 2.0 - i) / N;
        }
    }

    QueryPerformanceCounter(&start);

    for (int i = 0; i < N; i++) {
        for (int k = 0; k < N; k++) {
            double aik = a[i * N + k];
            for (int j = 0; j < N; j++) {
                c[i * N + j] += aik * b[k * N + j];
            }
        }
    }

    QueryPerformanceCounter(&end);

    double ms = (double)(end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart;
    long long flops = 2LL * N * N * N;
    double mflops = (double)flops * 1000.0 / (ms * 1e6);
    printf("C MATMUL: %.0f ms (N=%d, %.0f MFLOPS)\n", ms, N, mflops);

    free(a);
    free(b);
    free(c);
    return 0;
}
