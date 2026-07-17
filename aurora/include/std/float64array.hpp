#pragma once
#include <cstdint>

struct Float64Array {
    double* data;
    int64_t len;
};

extern "C" {
void*  f64array_new(int64_t n);
void   f64array_free(void* arr);
double f64array_get(void* arr, int64_t i);
void   f64array_set(void* arr, int64_t i, double v);
int64_t f64array_len(void* arr);
void   f64array_fill(void* arr, double v);
void   f64array_copy(void* dst, void* src);
void   f64array_matmul(void* a, void* b, void* c, int32_t n);
double f64array_sum(void* arr);
void   f64array_scale(void* arr, double f);
void   f64array_add(void* dst, void* src);
}
