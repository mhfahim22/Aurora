/* Bench helpers — expose tensor ops to Aurora with simple scalar args */
#include "runtime/tensor.hpp"
#include <cstdlib>
#include <cstring>

extern "C" {

void* bench_tensor_new_f32_2d(int n) {
    int64_t shape[2] = { n, n };
    return aurora_tensor_new_f32(2, shape);
}

void bench_tensor_fill_seq(void* t, int n) {
    auto* ten = (AuroraTensor*)t;
    float* d = ten->data_f32;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            d[i * n + j] = (float)((i * 1.0 + j) / n);
}

void bench_tensor_fill_seq_b(void* t, int n) {
    auto* ten = (AuroraTensor*)t;
    float* d = ten->data_f32;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            d[i * n + j] = (float)((j * 2.0 - i) / n);
}

void* bench_tensor_matmul(void* a, void* b) {
    return aurora_tensor_matmul((AuroraTensor*)a, (AuroraTensor*)b);
}

void bench_tensor_free(void* t) {
    aurora_tensor_free((AuroraTensor*)t);
}

} /* extern "C" */
