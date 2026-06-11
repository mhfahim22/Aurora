#include "runtime/tensor_v2.hpp"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <cfloat>

/*
 * TensorV2 — dtype-aware tensor engine with FP32/FP64 support
 * and a cache-friendly blocked matmul for FP32.
 */

extern "C" {

/* ════════════════════════════════════════════════════════════
   Helpers
   ════════════════════════════════════════════════════════════ */

static int64_t compute_size(int64_t ndim, int64_t* shape) {
    int64_t total = 1;
    for (int64_t i = 0; i < ndim; i++) total *= shape[i];
    return total;
}

static inline float f32_from_f64(double v) { return (float)v; }

/* ════════════════════════════════════════════════════════════
   Creation / Freeing
   ════════════════════════════════════════════════════════════ */

AuroraTensorV2* tensor_v2_new(int64_t ndim, int64_t* shape, int dtype) {
    if (ndim <= 0 || !shape) return nullptr;
    int64_t total = compute_size(ndim, shape);
    size_t elem_size = (dtype == TENSOR_F32) ? sizeof(float) : sizeof(double);
    AuroraTensorV2* t = (AuroraTensorV2*)calloc(1, sizeof(AuroraTensorV2));
    t->ndim = ndim;
    t->shape = (int64_t*)malloc((size_t)ndim * sizeof(int64_t));
    memcpy(t->shape, shape, (size_t)ndim * sizeof(int64_t));
    t->total_size = total;
    t->dtype = dtype;
    t->data = calloc((size_t)total, elem_size);
    t->data_f64 = (double*)t->data;
    t->data_f32 = (float*)t->data;
    return t;
}

void tensor_v2_free(AuroraTensorV2* t) {
    if (!t) return;
    free(t->shape);
    free(t->data);
    free(t);
}

/* ════════════════════════════════════════════════════════════
   Conversion
   ════════════════════════════════════════════════════════════ */

AuroraTensorV2* tensor_v2_to_f32(AuroraTensorV2* t) {
    if (!t) return nullptr;
    if (t->dtype == TENSOR_F32) return t; /* no copy needed */
    AuroraTensorV2* r = tensor_v2_new(t->ndim, t->shape, TENSOR_F32);
    int64_t n = t->total_size;
    for (int64_t i = 0; i < n; i++) r->data_f32[i] = f32_from_f64(t->data_f64[i]);
    return r;
}

AuroraTensorV2* tensor_v2_to_f64(AuroraTensorV2* t) {
    if (!t) return nullptr;
    if (t->dtype == TENSOR_F64) return t;
    AuroraTensorV2* r = tensor_v2_new(t->ndim, t->shape, TENSOR_F64);
    int64_t n = t->total_size;
    for (int64_t i = 0; i < n; i++) r->data_f64[i] = (double)t->data_f32[i];
    return r;
}

/* ════════════════════════════════════════════════════════════
   FP32-optimized matmul (blocked, cache-aware)
   Block size = 64 — tuned for modern CPU L1/L2 cache
   ════════════════════════════════════════════════════════════ */

#define BLOCK 64

static void matmul_blocked_f32(int64_t M, int64_t N, int64_t K,
                                const float* A, const float* B, float* C) {
    memset(C, 0, (size_t)(M * N) * sizeof(float));

    for (int64_t i0 = 0; i0 < M; i0 += BLOCK) {
        int64_t imax = (i0 + BLOCK < M) ? i0 + BLOCK : M;
        for (int64_t j0 = 0; j0 < N; j0 += BLOCK) {
            int64_t jmax = (j0 + BLOCK < N) ? j0 + BLOCK : N;
            for (int64_t k0 = 0; k0 < K; k0 += BLOCK) {
                int64_t kmax = (k0 + BLOCK < K) ? k0 + BLOCK : K;

                /* Accumulate C[i][j] += A[i][k] * B[k][j] within blocks */
                for (int64_t i = i0; i < imax; i++) {
                    for (int64_t k = k0; k < kmax; k++) {
                        float aik = A[i * K + k];
                        int64_t base = k * N;
                        for (int64_t j = j0; j < jmax; j++) {
                            C[i * N + j] += aik * B[base + j];
                        }
                    }
                }
            }
        }
    }
}

AuroraTensorV2* tensor_v2_matmul_f32(AuroraTensorV2* a, AuroraTensorV2* b) {
    if (!a || !b) return nullptr;
    if (a->ndim != 2 || b->ndim != 2) return nullptr;
    int64_t M = a->shape[0], K = a->shape[1];
    int64_t Kb = b->shape[0], N = b->shape[1];
    if (K != Kb) return nullptr;

    /* If either is F64, convert to F32 */
    AuroraTensorV2* a_f32 = (a->dtype == TENSOR_F32) ? a : tensor_v2_to_f32(a);
    AuroraTensorV2* b_f32 = (b->dtype == TENSOR_F32) ? b : tensor_v2_to_f32(b);

    int64_t shape[2] = { M, N };
    AuroraTensorV2* r = tensor_v2_new(2, shape, TENSOR_F32);

    matmul_blocked_f32(M, N, K, a_f32->data_f32, b_f32->data_f32, r->data_f32);

    if (a_f32 != a) tensor_v2_free(a_f32);
    if (b_f32 != b) tensor_v2_free(b_f32);
    return r;
}

/* ════════════════════════════════════════════════════════════
   Generic matmul (dispatches by dtype)
   ════════════════════════════════════════════════════════════ */

AuroraTensorV2* tensor_v2_matmul(AuroraTensorV2* a, AuroraTensorV2* b) {
    if (!a || !b) return nullptr;
    if (a->ndim != 2 || b->ndim != 2) return nullptr;
    int64_t M = a->shape[0], K = a->shape[1];
    int64_t Kb = b->shape[0], N = b->shape[1];
    if (K != Kb) return nullptr;

    /* Use blocked FP32 for both types (it's faster even for F64 input via conversion) */
    return tensor_v2_matmul_f32(a, b);
}

/* ════════════════════════════════════════════════════════════
   Element-wise ops (dtype-preserving)
   ════════════════════════════════════════════════════════════ */

AuroraTensorV2* tensor_v2_add(AuroraTensorV2* a, AuroraTensorV2* b) {
    if (!a || !b || a->total_size != b->total_size) return nullptr;
    int dtype = (a->dtype == TENSOR_F32 && b->dtype == TENSOR_F32) ? TENSOR_F32 : TENSOR_F64;
    AuroraTensorV2* r = tensor_v2_new(a->ndim, a->shape, dtype);
    int64_t n = r->total_size;
    if (dtype == TENSOR_F32) {
        for (int64_t i = 0; i < n; i++) r->data_f32[i] = a->data_f32[i] + b->data_f32[i];
    } else {
        for (int64_t i = 0; i < n; i++) r->data_f64[i] = a->data_f64[i] + b->data_f64[i];
    }
    return r;
}

AuroraTensorV2* tensor_v2_sub(AuroraTensorV2* a, AuroraTensorV2* b) {
    if (!a || !b || a->total_size != b->total_size) return nullptr;
    int dtype = (a->dtype == TENSOR_F32 && b->dtype == TENSOR_F32) ? TENSOR_F32 : TENSOR_F64;
    AuroraTensorV2* r = tensor_v2_new(a->ndim, a->shape, dtype);
    int64_t n = r->total_size;
    if (dtype == TENSOR_F32) {
        for (int64_t i = 0; i < n; i++) r->data_f32[i] = a->data_f32[i] - b->data_f32[i];
    } else {
        for (int64_t i = 0; i < n; i++) r->data_f64[i] = a->data_f64[i] - b->data_f64[i];
    }
    return r;
}

AuroraTensorV2* tensor_v2_mul(AuroraTensorV2* a, AuroraTensorV2* b) {
    if (!a || !b || a->total_size != b->total_size) return nullptr;
    int dtype = (a->dtype == TENSOR_F32 && b->dtype == TENSOR_F32) ? TENSOR_F32 : TENSOR_F64;
    AuroraTensorV2* r = tensor_v2_new(a->ndim, a->shape, dtype);
    int64_t n = r->total_size;
    if (dtype == TENSOR_F32) {
        for (int64_t i = 0; i < n; i++) r->data_f32[i] = a->data_f32[i] * b->data_f32[i];
    } else {
        for (int64_t i = 0; i < n; i++) r->data_f64[i] = a->data_f64[i] * b->data_f64[i];
    }
    return r;
}

AuroraTensorV2* tensor_v2_div(AuroraTensorV2* a, AuroraTensorV2* b) {
    if (!a || !b || a->total_size != b->total_size) return nullptr;
    int dtype = (a->dtype == TENSOR_F32 && b->dtype == TENSOR_F32) ? TENSOR_F32 : TENSOR_F64;
    AuroraTensorV2* r = tensor_v2_new(a->ndim, a->shape, dtype);
    int64_t n = r->total_size;
    if (dtype == TENSOR_F32) {
        for (int64_t i = 0; i < n; i++)
            r->data_f32[i] = b->data_f32[i] != 0.0f ? a->data_f32[i] / b->data_f32[i] : 0.0f;
    } else {
        for (int64_t i = 0; i < n; i++)
            r->data_f64[i] = b->data_f64[i] != 0.0 ? a->data_f64[i] / b->data_f64[i] : 0.0;
    }
    return r;
}

/* ════════════════════════════════════════════════════════════
   Activations (in-place, dtype-aware)
   ════════════════════════════════════════════════════════════ */

void tensor_v2_relu(AuroraTensorV2* t) {
    if (!t) return;
    int64_t n = t->total_size;
    if (t->dtype == TENSOR_F32) {
        for (int64_t i = 0; i < n; i++)
            if (t->data_f32[i] < 0.0f) t->data_f32[i] = 0.0f;
    } else {
        for (int64_t i = 0; i < n; i++)
            if (t->data_f64[i] < 0.0) t->data_f64[i] = 0.0;
    }
}

void tensor_v2_sigmoid(AuroraTensorV2* t) {
    if (!t) return;
    int64_t n = t->total_size;
    if (t->dtype == TENSOR_F32) {
        for (int64_t i = 0; i < n; i++)
            t->data_f32[i] = 1.0f / (1.0f + expf(-t->data_f32[i]));
    } else {
        for (int64_t i = 0; i < n; i++)
            t->data_f64[i] = 1.0 / (1.0 + exp(-t->data_f64[i]));
    }
}

void tensor_v2_tanh(AuroraTensorV2* t) {
    if (!t) return;
    int64_t n = t->total_size;
    if (t->dtype == TENSOR_F32) {
        for (int64_t i = 0; i < n; i++)
            t->data_f32[i] = tanhf(t->data_f32[i]);
    } else {
        for (int64_t i = 0; i < n; i++)
            t->data_f64[i] = tanh(t->data_f64[i]);
    }
}

static void softmax_f32(float* data, int64_t n) {
    float maxv = data[0];
    for (int64_t i = 1; i < n; i++) if (data[i] > maxv) maxv = data[i];
    float sum = 0.0f;
    for (int64_t i = 0; i < n; i++) { data[i] = expf(data[i] - maxv); sum += data[i]; }
    if (sum > 0.0f) for (int64_t i = 0; i < n; i++) data[i] /= sum;
}

void tensor_v2_softmax(AuroraTensorV2* t) {
    if (!t) return;
    int64_t n = t->total_size;
    if (t->dtype == TENSOR_F32) {
        if (t->ndim == 2) {
            int64_t cols = t->shape[1], rows = t->shape[0];
            for (int64_t r = 0; r < rows; r++)
                softmax_f32(t->data_f32 + r * cols, cols);
        } else {
            softmax_f32(t->data_f32, n);
        }
    } else {
        /* F64 softmax (row-wise if 2D) */
        if (t->ndim == 2) {
            int64_t cols = t->shape[1], rows = t->shape[0];
            for (int64_t r = 0; r < rows; r++) {
                double* row = t->data_f64 + r * cols;
                double maxv = row[0];
                for (int64_t j = 1; j < cols; j++) if (row[j] > maxv) maxv = row[j];
                double sum = 0.0;
                for (int64_t j = 0; j < cols; j++) { row[j] = exp(row[j] - maxv); sum += row[j]; }
                if (sum > 0.0) for (int64_t j = 0; j < cols; j++) row[j] /= sum;
            }
        }
    }
}

void tensor_v2_gelu(AuroraTensorV2* t) {
    if (!t) return;
    int64_t n = t->total_size;
    if (t->dtype == TENSOR_F32) {
        for (int64_t i = 0; i < n; i++) {
            float x = t->data_f32[i];
            t->data_f32[i] = 0.5f * x * (1.0f + tanhf(0.7978845608f * (x + 0.044715f * x * x * x)));
        }
    } else {
        for (int64_t i = 0; i < n; i++) {
            double x = t->data_f64[i];
            t->data_f64[i] = 0.5 * x * (1.0 + tanh(0.7978845608 * (x + 0.044715 * x * x * x)));
        }
    }
}

void tensor_v2_silu(AuroraTensorV2* t) {
    if (!t) return;
    int64_t n = t->total_size;
    if (t->dtype == TENSOR_F32) {
        for (int64_t i = 0; i < n; i++) {
            float x = t->data_f32[i];
            t->data_f32[i] = x / (1.0f + expf(-x));
        }
    } else {
        for (int64_t i = 0; i < n; i++) {
            double x = t->data_f64[i];
            t->data_f64[i] = x / (1.0 + exp(-x));
        }
    }
}

/* ════════════════════════════════════════════════════════════
   Reduction
   ════════════════════════════════════════════════════════════ */

double tensor_v2_sum(AuroraTensorV2* t) {
    if (!t) return 0.0;
    int64_t n = t->total_size;
    double s = 0.0;
    if (t->dtype == TENSOR_F32) {
        for (int64_t i = 0; i < n; i++) s += (double)t->data_f32[i];
    } else {
        for (int64_t i = 0; i < n; i++) s += t->data_f64[i];
    }
    return s;
}

double tensor_v2_mean(AuroraTensorV2* t) {
    if (!t || t->total_size == 0) return 0.0;
    return tensor_v2_sum(t) / (double)t->total_size;
}

/* ════════════════════════════════════════════════════════════
   Utility
   ════════════════════════════════════════════════════════════ */

AuroraTensorV2* tensor_v2_transpose(AuroraTensorV2* t) {
    if (!t || t->ndim != 2) return nullptr;
    int64_t rows = t->shape[0], cols = t->shape[1];
    int64_t shape[2] = { cols, rows };
    AuroraTensorV2* r = tensor_v2_new(2, shape, t->dtype);
    int64_t n = rows * cols;
    if (t->dtype == TENSOR_F32) {
        for (int64_t i = 0; i < rows; i++)
            for (int64_t j = 0; j < cols; j++)
                r->data_f32[j * rows + i] = t->data_f32[i * cols + j];
    } else {
        for (int64_t i = 0; i < rows; i++)
            for (int64_t j = 0; j < cols; j++)
                r->data_f64[j * rows + i] = t->data_f64[i * cols + j];
    }
    return r;
}

AuroraTensorV2* tensor_v2_reshape(AuroraTensorV2* t, int64_t ndim, int64_t* shape) {
    if (!t || ndim <= 0 || !shape) return nullptr;
    int64_t new_size = compute_size(ndim, shape);
    if (new_size != t->total_size) return nullptr;
    AuroraTensorV2* r = tensor_v2_new(ndim, shape, t->dtype);
    size_t bytes = (size_t)t->total_size * ((t->dtype == TENSOR_F32) ? sizeof(float) : sizeof(double));
    memcpy(r->data, t->data, bytes);
    return r;
}

} /* extern "C" */
