#include "runtime/tensor.hpp"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <cfloat>
#include <cstdint>
#ifdef _OPENMP
#include <omp.h>
#endif

#if AURORA_CUDA
extern "C" int cuda_matmul_available();
extern "C" int cuda_matmul(double* A, double* B, double* C, int64_t M, int64_t K, int64_t N);
extern "C" int cuda_elewise_available();
extern "C" int cuda_add_f32(const float* a, const float* b, float* out, int64_t n);
extern "C" int cuda_sub_f32(const float* a, const float* b, float* out, int64_t n);
extern "C" int cuda_mul_f32(const float* a, const float* b, float* out, int64_t n);
extern "C" int cuda_div_f32(const float* a, const float* b, float* out, int64_t n);
extern "C" int cuda_relu_f32(float* data, int64_t n);
extern "C" int cuda_sigmoid_f32(float* data, int64_t n);
#elif AURORA_HIP
extern "C" int hip_matmul_available();
extern "C" int hip_matmul(double* A, double* B, double* C, int64_t M, int64_t K, int64_t N);
#endif
/* DirectX Compute (dml_matmul.cpp) — always compiled on WIN32 */
#ifdef _WIN32
extern "C" int dml_available();
extern "C" int dml_matmul(const float* A, const float* B, float* C, int64_t M, int64_t N, int64_t K);
#endif

extern "C" {

/* ── Helper: compute total size from shape ── */
static int64_t tensor_compute_size(int64_t ndim, int64_t* shape) {
    int64_t total = 1;
    for (int64_t i = 0; i < ndim; i++) total *= shape[i];
    return total;
}

/* ── Helper: linear index from multi-dimensional indices ── */
static int64_t tensor_linear_index(AuroraTensor* t, int64_t* indices) {
    int64_t idx = 0;
    int64_t stride = 1;
    for (int64_t i = t->ndim - 1; i >= 0; i--) {
        idx += indices[i] * stride;
        stride *= t->shape[i];
    }
    return idx;
}

/* ── F64 tensor (default, backward compat) ── */
AuroraTensor* aurora_tensor_new(int64_t ndim, int64_t* shape) {
    if (ndim <= 0 || !shape) return nullptr;
    AuroraTensor* t = (AuroraTensor*)calloc(1, sizeof(AuroraTensor));
    if (!t) return nullptr;
    t->ndim = ndim;
    t->shape = (int64_t*)malloc((size_t)ndim * sizeof(int64_t));
    if (!t->shape) { free(t); return nullptr; }
    memcpy(t->shape, shape, (size_t)ndim * sizeof(int64_t));
    t->total_size = tensor_compute_size(ndim, shape);
    t->dtype = TENSOR_F64;
    t->data_ptr = calloc((size_t)t->total_size, sizeof(double));
    if (!t->data_ptr) { free(t->shape); free(t); return nullptr; }
    return t;
}

/* ── F32 tensor (new, half memory) ── */
AuroraTensor* aurora_tensor_new_f32(int64_t ndim, int64_t* shape) {
    if (ndim <= 0 || !shape) return nullptr;
    AuroraTensor* t = (AuroraTensor*)calloc(1, sizeof(AuroraTensor));
    if (!t) return nullptr;
    t->ndim = ndim;
    t->shape = (int64_t*)malloc((size_t)ndim * sizeof(int64_t));
    if (!t->shape) { free(t); return nullptr; }
    memcpy(t->shape, shape, (size_t)ndim * sizeof(int64_t));
    t->total_size = tensor_compute_size(ndim, shape);
    t->dtype = TENSOR_F32;
    t->data_ptr = calloc((size_t)t->total_size, sizeof(float));
    if (!t->data_ptr) { free(t->shape); free(t); return nullptr; }
    return t;
}

/* ── FP16 tensor (half-precision float, 2 bytes per element) ── */
AuroraTensor* aurora_tensor_new_f16(int64_t ndim, int64_t* shape) {
    if (ndim <= 0 || !shape) return nullptr;
    AuroraTensor* t = (AuroraTensor*)calloc(1, sizeof(AuroraTensor));
    if (!t) return nullptr;
    t->ndim = ndim;
    t->shape = (int64_t*)malloc((size_t)ndim * sizeof(int64_t));
    if (!t->shape) { free(t); return nullptr; }
    memcpy(t->shape, shape, (size_t)ndim * sizeof(int64_t));
    t->total_size = tensor_compute_size(ndim, shape);
    t->dtype = TENSOR_F16;
    t->data_ptr = calloc((size_t)t->total_size, sizeof(uint16_t));
    if (!t->data_ptr) { free(t->shape); free(t); return nullptr; }
    return t;
}

/* ── BF16 tensor (brain float 16, 2 bytes per element) ── */
AuroraTensor* aurora_tensor_new_bf16(int64_t ndim, int64_t* shape) {
    if (ndim <= 0 || !shape) return nullptr;
    AuroraTensor* t = (AuroraTensor*)calloc(1, sizeof(AuroraTensor));
    if (!t) return nullptr;
    t->ndim = ndim;
    t->shape = (int64_t*)malloc((size_t)ndim * sizeof(int64_t));
    if (!t->shape) { free(t); return nullptr; }
    memcpy(t->shape, shape, (size_t)ndim * sizeof(int64_t));
    t->total_size = tensor_compute_size(ndim, shape);
    t->dtype = TENSOR_BF16;
    t->data_ptr = calloc((size_t)t->total_size, sizeof(uint16_t));
    if (!t->data_ptr) { free(t->shape); free(t); return nullptr; }
    return t;
}

void aurora_tensor_free(AuroraTensor* t) {
    if (!t) return;
    free(t->shape);
    free(t->data_ptr);
    free(t->grad);
    free(t);
}

int64_t aurora_tensor_ndim(AuroraTensor* t) {
    return t ? t->ndim : 0;
}

int64_t aurora_tensor_shape(AuroraTensor* t, int64_t dim) {
    if (!t || dim < 0 || dim >= t->ndim) return 0;
    return t->shape[dim];
}

double aurora_tensor_get(AuroraTensor* t, int64_t* indices) {
    if (!t || !indices) return 0.0;
    int64_t idx = tensor_linear_index(t, indices);
    if (idx < 0 || idx >= t->total_size) return 0.0;
    switch (t->dtype) {
        case TENSOR_F32: return (double)t->data_f32[idx];
        case TENSOR_F16: return (double)f16_to_f32(((uint16_t*)t->data_ptr)[idx]);
        case TENSOR_BF16: return (double)bf16_to_f32(((uint16_t*)t->data_ptr)[idx]);
        default: return t->data[idx];
    }
}

void aurora_tensor_set(AuroraTensor* t, int64_t* indices, double val) {
    if (!t || !indices) return;
    int64_t idx = tensor_linear_index(t, indices);
    if (idx < 0 || idx >= t->total_size) return;
    switch (t->dtype) {
        case TENSOR_F32: t->data_f32[idx] = (float)val; break;
        case TENSOR_F16: ((uint16_t*)t->data_ptr)[idx] = f32_to_f16((float)val); break;
        case TENSOR_BF16: ((uint16_t*)t->data_ptr)[idx] = f32_to_bf16((float)val); break;
        default: t->data[idx] = val; break;
    }
}

/* ════════════════════════════════════════════════════════════
   Blocked FP32 matmul (cache-friendly)
   ════════════════════════════════════════════════════════════ */

#define BLOCK 64

static void matmul_blocked_f32(int64_t M, int64_t N, int64_t K,
                                const float* A, const float* B, float* C) {
    memset(C, 0, (size_t)(M * N) * sizeof(float));
    #pragma omp parallel for
    for (int64_t i0 = 0; i0 < M; i0 += BLOCK) {
        int64_t imax = (i0 + BLOCK < M) ? i0 + BLOCK : M;
        for (int64_t k0 = 0; k0 < K; k0 += BLOCK) {
            int64_t kmax = (k0 + BLOCK < K) ? k0 + BLOCK : K;
            for (int64_t j0 = 0; j0 < N; j0 += BLOCK) {
                int64_t jmax = (j0 + BLOCK < N) ? j0 + BLOCK : N;
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

/* ════════════════════════════════════════════════════════════
   Conversion
   ════════════════════════════════════════════════════════════ */

AuroraTensor* aurora_tensor_to_f32(AuroraTensor* t) {
    if (!t) return nullptr;
    if (t->dtype == TENSOR_F32) return t;
    AuroraTensor* r = aurora_tensor_new_f32(t->ndim, t->shape);
    int64_t n = t->total_size;
    if (t->dtype == TENSOR_F64) {
        for (int64_t i = 0; i < n; i++) r->data_f32[i] = (float)t->data[i];
    } else if (t->dtype == TENSOR_F16) {
        uint16_t* src = (uint16_t*)t->data_ptr;
        for (int64_t i = 0; i < n; i++) r->data_f32[i] = f16_to_f32(src[i]);
    } else if (t->dtype == TENSOR_BF16) {
        uint16_t* src = (uint16_t*)t->data_ptr;
        for (int64_t i = 0; i < n; i++) r->data_f32[i] = bf16_to_f32(src[i]);
    }
    return r;
}

AuroraTensor* aurora_tensor_to_f64(AuroraTensor* t) {
    if (!t) return nullptr;
    if (t->dtype == TENSOR_F64) return t;
    AuroraTensor* r = aurora_tensor_new(t->ndim, t->shape);
    int64_t n = t->total_size;
    if (t->dtype == TENSOR_F32) {
        for (int64_t i = 0; i < n; i++) r->data[i] = (double)t->data_f32[i];
    } else if (t->dtype == TENSOR_F16) {
        uint16_t* src = (uint16_t*)t->data_ptr;
        for (int64_t i = 0; i < n; i++) r->data[i] = (double)f16_to_f32(src[i]);
    } else if (t->dtype == TENSOR_BF16) {
        uint16_t* src = (uint16_t*)t->data_ptr;
        for (int64_t i = 0; i < n; i++) r->data[i] = (double)bf16_to_f32(src[i]);
    }
    return r;
}

AuroraTensor* aurora_tensor_to_f16(AuroraTensor* t) {
    if (!t) return nullptr;
    if (t->dtype == TENSOR_F16) return t;
    AuroraTensor* r = aurora_tensor_new_f16(t->ndim, t->shape);
    int64_t n = t->total_size;
    uint16_t* dst = (uint16_t*)r->data_ptr;
    if (t->dtype == TENSOR_F32) {
        for (int64_t i = 0; i < n; i++) dst[i] = f32_to_f16(t->data_f32[i]);
    } else if (t->dtype == TENSOR_BF16) {
        uint16_t* src = (uint16_t*)t->data_ptr;
        for (int64_t i = 0; i < n; i++) dst[i] = f32_to_f16(bf16_to_f32(src[i]));
    } else {
        for (int64_t i = 0; i < n; i++) dst[i] = f32_to_f16((float)t->data[i]);
    }
    return r;
}

AuroraTensor* aurora_tensor_to_bf16(AuroraTensor* t) {
    if (!t) return nullptr;
    if (t->dtype == TENSOR_BF16) return t;
    AuroraTensor* r = aurora_tensor_new_bf16(t->ndim, t->shape);
    int64_t n = t->total_size;
    uint16_t* dst = (uint16_t*)r->data_ptr;
    if (t->dtype == TENSOR_F32) {
        for (int64_t i = 0; i < n; i++) dst[i] = f32_to_bf16(t->data_f32[i]);
    } else if (t->dtype == TENSOR_F16) {
        uint16_t* src = (uint16_t*)t->data_ptr;
        for (int64_t i = 0; i < n; i++) dst[i] = f32_to_bf16(f16_to_f32(src[i]));
    } else {
        for (int64_t i = 0; i < n; i++) dst[i] = f32_to_bf16((float)t->data[i]);
    }
    return r;
}

AuroraTensor* aurora_tensor_clone(AuroraTensor* t) {
    if (!t) return nullptr;
    AuroraTensor* r;
    size_t elem_size;
    switch (t->dtype) {
        case TENSOR_F32: r = aurora_tensor_new_f32(t->ndim, t->shape); elem_size = sizeof(float); break;
        case TENSOR_F16: r = aurora_tensor_new_f16(t->ndim, t->shape); elem_size = sizeof(uint16_t); break;
        case TENSOR_BF16: r = aurora_tensor_new_bf16(t->ndim, t->shape); elem_size = sizeof(uint16_t); break;
        default: r = aurora_tensor_new(t->ndim, t->shape); elem_size = sizeof(double); break;
    }
    if (!r || !r->data_ptr || !t->data_ptr) { if (r) aurora_tensor_free(r); return nullptr; }
    memcpy(r->data_ptr, t->data_ptr, (size_t)t->total_size * elem_size);
    return r;
}

/* ════════════════════════════════════════════════════════════
   Matmul — auto-dispatch: F32→blocked, F64→naive/CUDA,
   F16/BF16→promote to F32 first
   ════════════════════════════════════════════════════════════ */

AuroraTensor* aurora_tensor_matmul(AuroraTensor* a, AuroraTensor* b) {
    if (!a || !b) return nullptr;
    if (a->ndim != 2 || b->ndim != 2) return nullptr;
    int64_t M = a->shape[0], K = a->shape[1];
    int64_t Kb = b->shape[0], N = b->shape[1];
    if (K != Kb) return nullptr;

    int dt_a = a->dtype, dt_b = b->dtype;
    int is_low = (dt_a == TENSOR_F16 || dt_a == TENSOR_BF16 || dt_b == TENSOR_F16 || dt_b == TENSOR_BF16);

    /* If either is F16/BF16, promote to F32 first */
    if (is_low || dt_a == TENSOR_F32 || dt_b == TENSOR_F32) {
        /* Convert to F32 if not already */
        AuroraTensor* a2 = (dt_a == TENSOR_F32) ? a : aurora_tensor_to_f32(a);
        AuroraTensor* b2 = (dt_b == TENSOR_F32) ? b : aurora_tensor_to_f32(b);

        int64_t shape[2] = { M, N };
        AuroraTensor* r = aurora_tensor_new_f32(2, shape);

#ifdef _WIN32
        /* Try GPU compute (DirectX 11) first, fall back to CPU OpenMP */
        if (dml_available() && M >= 32 && N >= 32 && K >= 32) {
            if (dml_matmul(a2->data_f32, b2->data_f32, r->data_f32, M, N, K)) {
                if (a2 != a) aurora_tensor_free(a2);
                if (b2 != b) aurora_tensor_free(b2);
                link_grad(r, backward_matmul, a, b, 0, 0);
                return r;
            }
        }
#endif
        matmul_blocked_f32(M, N, K, a2->data_f32, b2->data_f32, r->data_f32);

        if (a2 != a) aurora_tensor_free(a2);
        if (b2 != b) aurora_tensor_free(b2);
        link_grad(r, backward_matmul, a, b, 0, 0);
        return r;
    }

    /* Fallback F64 matmul with CUDA support */
    int64_t shape[2] = { M, N };
    AuroraTensor* r = aurora_tensor_new(2, shape);

#if AURORA_CUDA
    if (cuda_matmul_available()) {
        if (cuda_matmul(a->data, b->data, r->data, M, K, N)) {
            link_grad(r, backward_matmul, a, b, 0, 0);
            return r;
        }
    }
#elif AURORA_HIP
    if (hip_matmul_available()) {
        if (hip_matmul(a->data, b->data, r->data, M, K, N)) {
            link_grad(r, backward_matmul, a, b, 0, 0);
            return r;
        }
    }
#endif

    #pragma omp parallel for
    for (int64_t i = 0; i < M; i++) {
        for (int64_t j = 0; j < N; j++) {
            double sum = 0.0;
            for (int64_t k = 0; k < K; k++) {
                sum += a->data[i * K + k] * b->data[k * N + j];
            }
            r->data[i * N + j] = sum;
        }
    }
    link_grad(r, backward_matmul, a, b, 0, 0);
    return r;
}

/* ════════════════════════════════════════════════════════════
   Element-wise ops (dtype-preserving)
   ════════════════════════════════════════════════════════════ */

static inline double tensor_read_f64(AuroraTensor* t, int64_t i) {
    switch (t->dtype) {
        case TENSOR_F32: return (double)t->data_f32[i];
        case TENSOR_F16: return (double)f16_to_f32(((uint16_t*)t->data_ptr)[i]);
        case TENSOR_BF16: return (double)bf16_to_f32(((uint16_t*)t->data_ptr)[i]);
        default: return t->data[i];
    }
}

static int promote_dtype(AuroraTensor* a, AuroraTensor* b) {
    if (a->dtype == TENSOR_F64 || b->dtype == TENSOR_F64) return TENSOR_F64;
    return TENSOR_F32;
}

AuroraTensor* aurora_tensor_add(AuroraTensor* a, AuroraTensor* b) {
    if (!a || !b) return nullptr;
    if (a->total_size != b->total_size) return nullptr;
    int dt = promote_dtype(a, b);
    AuroraTensor* r = (dt == TENSOR_F32) ?
        aurora_tensor_new_f32(a->ndim, a->shape) : aurora_tensor_new(a->ndim, a->shape);
    int64_t n = r->total_size;
    if (dt == TENSOR_F32) {
#if AURORA_CUDA
        if (a->dtype == TENSOR_F32 && b->dtype == TENSOR_F32 && cuda_elewise_available()) {
            if (cuda_add_f32(a->data_f32, b->data_f32, r->data_f32, n)) {
                link_grad(r, backward_add, a, b, 0, 0);
                return r;
            }
        }
#endif
        for (int64_t i = 0; i < n; i++) r->data_f32[i] = (float)(tensor_read_f64(a, i) + tensor_read_f64(b, i));
    } else {
        for (int64_t i = 0; i < n; i++) r->data[i] = tensor_read_f64(a, i) + tensor_read_f64(b, i);
    }
    link_grad(r, backward_add, a, b, 0, 0);
    return r;
}

AuroraTensor* aurora_tensor_sub(AuroraTensor* a, AuroraTensor* b) {
    if (!a || !b || a->total_size != b->total_size) return nullptr;
    int dt = promote_dtype(a, b);
    AuroraTensor* r = (dt == TENSOR_F32) ?
        aurora_tensor_new_f32(a->ndim, a->shape) : aurora_tensor_new(a->ndim, a->shape);
    int64_t n = r->total_size;
    if (dt == TENSOR_F32) {
#if AURORA_CUDA
        if (a->dtype == TENSOR_F32 && b->dtype == TENSOR_F32 && cuda_elewise_available()) {
            if (cuda_sub_f32(a->data_f32, b->data_f32, r->data_f32, n)) {
                link_grad(r, backward_sub, a, b, 0, 0);
                return r;
            }
        }
#endif
        for (int64_t i = 0; i < n; i++) r->data_f32[i] = (float)(tensor_read_f64(a, i) - tensor_read_f64(b, i));
    } else {
        for (int64_t i = 0; i < n; i++) r->data[i] = tensor_read_f64(a, i) - tensor_read_f64(b, i);
    }
    link_grad(r, backward_sub, a, b, 0, 0);
    return r;
}

AuroraTensor* aurora_tensor_mul(AuroraTensor* a, AuroraTensor* b) {
    if (!a || !b || a->total_size != b->total_size) return nullptr;
    int dt = promote_dtype(a, b);
    AuroraTensor* r = (dt == TENSOR_F32) ?
        aurora_tensor_new_f32(a->ndim, a->shape) : aurora_tensor_new(a->ndim, a->shape);
    int64_t n = r->total_size;
    if (dt == TENSOR_F32) {
#if AURORA_CUDA
        if (a->dtype == TENSOR_F32 && b->dtype == TENSOR_F32 && cuda_elewise_available()) {
            if (cuda_mul_f32(a->data_f32, b->data_f32, r->data_f32, n)) {
                link_grad(r, backward_mul, a, b, 0, 0);
                return r;
            }
        }
#endif
        for (int64_t i = 0; i < n; i++) r->data_f32[i] = (float)(tensor_read_f64(a, i) * tensor_read_f64(b, i));
    } else {
        for (int64_t i = 0; i < n; i++) r->data[i] = tensor_read_f64(a, i) * tensor_read_f64(b, i);
    }
    link_grad(r, backward_mul, a, b, 0, 0);
    return r;
}

AuroraTensor* aurora_tensor_div(AuroraTensor* a, AuroraTensor* b) {
    if (!a || !b || a->total_size != b->total_size) return nullptr;
    int dt = promote_dtype(a, b);
    AuroraTensor* r = (dt == TENSOR_F32) ?
        aurora_tensor_new_f32(a->ndim, a->shape) : aurora_tensor_new(a->ndim, a->shape);
    int64_t n = r->total_size;
    if (dt == TENSOR_F32) {
#if AURORA_CUDA
        if (a->dtype == TENSOR_F32 && b->dtype == TENSOR_F32 && cuda_elewise_available()) {
            if (cuda_div_f32(a->data_f32, b->data_f32, r->data_f32, n)) {
                link_grad(r, backward_div, a, b, 0, 0);
                return r;
            }
        }
#endif
        for (int64_t i = 0; i < n; i++) {
            double bv = tensor_read_f64(b, i);
            r->data_f32[i] = (float)(bv != 0.0 ? tensor_read_f64(a, i) / bv : 0.0);
        }
    } else {
        for (int64_t i = 0; i < n; i++) {
            double bv = tensor_read_f64(b, i);
            r->data[i] = bv != 0.0 ? tensor_read_f64(a, i) / bv : 0.0;
        }
    }
    link_grad(r, backward_div, a, b, 0, 0);
    return r;
}

AuroraTensor* aurora_tensor_reshape(AuroraTensor* t, int64_t ndim, int64_t* shape) {
    if (!t || ndim <= 0 || !shape) return nullptr;
    int64_t new_size = tensor_compute_size(ndim, shape);
    if (new_size != t->total_size) return nullptr;
    AuroraTensor* r;
    size_t elem_size;
    switch (t->dtype) {
        case TENSOR_F32: r = aurora_tensor_new_f32(ndim, shape); elem_size = sizeof(float); break;
        case TENSOR_F16: r = aurora_tensor_new_f16(ndim, shape); elem_size = sizeof(uint16_t); break;
        case TENSOR_BF16: r = aurora_tensor_new_bf16(ndim, shape); elem_size = sizeof(uint16_t); break;
        default: r = aurora_tensor_new(ndim, shape); elem_size = sizeof(double); break;
    }
    memcpy(r->data_ptr, t->data_ptr, (size_t)t->total_size * elem_size);
    return r;
}

AuroraTensor* aurora_tensor_pow(AuroraTensor* a, double exp_val) {
    if (!a) return nullptr;
    AuroraTensor* r = (a->dtype == TENSOR_F32) ?
        aurora_tensor_new_f32(a->ndim, a->shape) : aurora_tensor_new(a->ndim, a->shape);
    int64_t n = r->total_size;
    if (a->dtype == TENSOR_F32) {
        for (int64_t i = 0; i < n; i++) r->data_f32[i] = powf((float)tensor_read_f64(a, i), (float)exp_val);
    } else {
        for (int64_t i = 0; i < n; i++) r->data[i] = pow(tensor_read_f64(a, i), exp_val);
    }
    link_grad(r, backward_pow, a, 0, 0, 0);
    return r;
}

void aurora_tensor_exp(AuroraTensor* t) {
    if (!t) return;
    int64_t n = t->total_size;
    for (int64_t i = 0; i < n; i++) {
        double v = tensor_read_f64(t, i);
        /* in-place: convert to F64 for accuracy, then store back */
        switch (t->dtype) {
            case TENSOR_F32: t->data_f32[i] = expf((float)v); break;
            case TENSOR_F16: ((uint16_t*)t->data_ptr)[i] = f32_to_f16(expf((float)v)); break;
            case TENSOR_BF16: ((uint16_t*)t->data_ptr)[i] = f32_to_bf16(expf((float)v)); break;
            default: t->data[i] = exp(v); break;
        }
    }
}

void aurora_tensor_log(AuroraTensor* t) {
    if (!t) return;
    int64_t n = t->total_size;
    for (int64_t i = 0; i < n; i++) {
        double v = tensor_read_f64(t, i);
        double r = v > 0.0 ? log(v) : 0.0;
        switch (t->dtype) {
            case TENSOR_F32: t->data_f32[i] = (float)r; break;
            case TENSOR_F16: ((uint16_t*)t->data_ptr)[i] = f32_to_f16((float)r); break;
            case TENSOR_BF16: ((uint16_t*)t->data_ptr)[i] = f32_to_bf16((float)r); break;
            default: t->data[i] = r; break;
        }
    }
}

double aurora_tensor_sum(AuroraTensor* t) {
    if (!t) return 0.0;
    int64_t n = t->total_size;
    double s = 0.0;
    for (int64_t i = 0; i < n; i++) s += tensor_read_f64(t, i);
    return s;
}

double aurora_tensor_mean(AuroraTensor* t) {
    if (!t || t->total_size == 0) return 0.0;
    return aurora_tensor_sum(t) / (double)t->total_size;
}

double aurora_tensor_std(AuroraTensor* t) {
    if (!t || t->total_size == 0) return 0.0;
    double m = aurora_tensor_mean(t);
    double v = 0.0;
    for (int64_t i = 0; i < t->total_size; i++) {
        double d = tensor_read_f64(t, i) - m;
        v += d * d;
    }
    return sqrt(v / (double)t->total_size);
}

AuroraTensor* aurora_tensor_transpose(AuroraTensor* t) {
    if (!t || t->ndim != 2) return nullptr;
    int64_t rows = t->shape[0], cols = t->shape[1];
    int64_t shape[2] = { cols, rows };
    AuroraTensor* r = (t->dtype == TENSOR_F32) ?
        aurora_tensor_new_f32(2, shape) : aurora_tensor_new(2, shape);
    if (t->dtype == TENSOR_F32) {
        for (int64_t i = 0; i < rows; i++)
            for (int64_t j = 0; j < cols; j++)
                r->data_f32[j * rows + i] = t->data_f32[i * cols + j];
    } else {
        for (int64_t i = 0; i < rows; i++)
            for (int64_t j = 0; j < cols; j++)
                r->data[j * rows + i] = t->data[i * cols + j];
    }
    return r;
}

static void softmax_inplace_f32(float* data, int64_t n) {
    float maxv = data[0];
    for (int64_t i = 1; i < n; i++) if (data[i] > maxv) maxv = data[i];
    float sum = 0.0f;
    for (int64_t i = 0; i < n; i++) { data[i] = expf(data[i] - maxv); sum += data[i]; }
    if (sum > 0.0f) for (int64_t i = 0; i < n; i++) data[i] /= sum;
}

static void softmax_inplace(AuroraTensor* t, int64_t n) {
    double maxv = tensor_read_f64(t, 0);
    for (int64_t i = 1; i < n; i++) {
        double v = tensor_read_f64(t, i);
        if (v > maxv) maxv = v;
    }
    double sum = 0.0;
    for (int64_t i = 0; i < n; i++) {
        double v = exp(tensor_read_f64(t, i) - maxv);
        switch (t->dtype) {
            case TENSOR_F32: t->data_f32[i] = (float)v; break;
            case TENSOR_F16: ((uint16_t*)t->data_ptr)[i] = f32_to_f16((float)v); break;
            case TENSOR_BF16: ((uint16_t*)t->data_ptr)[i] = f32_to_bf16((float)v); break;
            default: t->data[i] = v; break;
        }
        sum += v;
    }
    if (sum > 0.0) {
        for (int64_t i = 0; i < n; i++) {
            double v = (t->dtype == TENSOR_F32) ? (double)t->data_f32[i] : tensor_read_f64(t, i);
            v /= sum;
            switch (t->dtype) {
                case TENSOR_F32: t->data_f32[i] = (float)v; break;
                case TENSOR_F16: ((uint16_t*)t->data_ptr)[i] = f32_to_f16((float)v); break;
                case TENSOR_BF16: ((uint16_t*)t->data_ptr)[i] = f32_to_bf16((float)v); break;
                default: t->data[i] = v; break;
            }
        }
    }
}

void aurora_tensor_softmax(AuroraTensor* t) {
    if (!t) return;
    if (t->ndim == 2) {
        int64_t cols = t->shape[1];
        for (int64_t r = 0; r < t->shape[0]; r++) {
            /* Create a temporary 1D view for softmax_inplace */
            AuroraTensor view = *t;
            view.total_size = cols;
            view.data_ptr = (void*)((t->dtype == TENSOR_F32) ? (void*)(t->data_f32 + r * cols) :
                            (t->dtype == TENSOR_F64) ? (void*)(t->data + r * cols) :
                            (void*)((uint16_t*)t->data_ptr + r * cols));
            softmax_inplace(&view, cols);
        }
    } else {
        softmax_inplace(t, t->total_size);
    }
}

AuroraTensor* aurora_tensor_concatenate(AuroraTensor* a, AuroraTensor* b, int64_t axis) {
    if (!a || !b) return nullptr;
    if (a->ndim != b->ndim) return nullptr;
    if (axis < 0 || axis >= a->ndim) return nullptr;
    for (int64_t i = 0; i < a->ndim; i++)
        if (i != axis && a->shape[i] != b->shape[i]) return nullptr;

    int dt = promote_dtype(a, b);
    int64_t shape[8];
    for (int64_t i = 0; i < a->ndim; i++)
        shape[i] = a->shape[i] + (i == axis ? b->shape[i] : 0);
    AuroraTensor* r = (dt == TENSOR_F32) ?
        aurora_tensor_new_f32(a->ndim, shape) : aurora_tensor_new(a->ndim, shape);
    if (!r) return nullptr;

    if (a->ndim == 1) {
        for (int64_t i = 0; i < a->total_size; i++) {
            double v = tensor_read_f64(a, i);
            if (r->dtype == TENSOR_F32) r->data_f32[i] = (float)v; else r->data[i] = v;
        }
        for (int64_t i = 0; i < b->total_size; i++) {
            double v = tensor_read_f64(b, i);
            if (r->dtype == TENSOR_F32) r->data_f32[a->total_size + i] = (float)v; else r->data[a->total_size + i] = v;
        }
    } else if (a->ndim == 2 && axis == 0) {
        int64_t cols = a->shape[1];
        for (int64_t i = 0; i < a->shape[0]; i++)
            for (int64_t j = 0; j < cols; j++) {
                double v = tensor_read_f64(a, i * cols + j);
                if (r->dtype == TENSOR_F32) r->data_f32[i * cols + j] = (float)v; else r->data[i * cols + j] = v;
            }
        for (int64_t i = 0; i < b->shape[0]; i++)
            for (int64_t j = 0; j < cols; j++) {
                double v = tensor_read_f64(b, i * cols + j);
                if (r->dtype == TENSOR_F32) r->data_f32[(a->shape[0] + i) * cols + j] = (float)v; else r->data[(a->shape[0] + i) * cols + j] = v;
            }
    } else if (a->ndim == 2 && axis == 1) {
        int64_t rows = a->shape[0];
        int64_t a_cols = a->shape[1], b_cols = b->shape[1];
        for (int64_t i = 0; i < rows; i++) {
            for (int64_t j = 0; j < a_cols; j++) {
                double v = tensor_read_f64(a, i * a_cols + j);
                if (r->dtype == TENSOR_F32) r->data_f32[i * (a_cols + b_cols) + j] = (float)v; else r->data[i * (a_cols + b_cols) + j] = v;
            }
            for (int64_t j = 0; j < b_cols; j++) {
                double v = tensor_read_f64(b, i * b_cols + j);
                if (r->dtype == TENSOR_F32) r->data_f32[i * (a_cols + b_cols) + a_cols + j] = (float)v; else r->data[i * (a_cols + b_cols) + a_cols + j] = v;
            }
        }
    }
    return r;
}

AuroraTensor* aurora_tensor_slice(AuroraTensor* t, int64_t start, int64_t end, int64_t axis) {
    if (!t || axis < 0 || axis >= t->ndim || start < 0 || end > t->shape[axis] || start >= end)
        return nullptr;
    int64_t shape[4];
    AuroraTensor* r;
    if (t->ndim == 1) {
        shape[0] = end - start;
        r = aurora_tensor_new(1, shape);
        if (!r) return nullptr;
        for (int64_t i = start; i < end; i++) r->data[i - start] = tensor_read_f64(t, i);
    } else if (t->ndim == 2 && axis == 0) {
        int64_t cols = t->shape[1];
        shape[0] = end - start; shape[1] = cols;
        r = aurora_tensor_new(2, shape);
        if (!r) return nullptr;
        for (int64_t i = start; i < end; i++)
            for (int64_t j = 0; j < cols; j++)
                r->data[(i - start) * cols + j] = tensor_read_f64(t, i * cols + j);
    } else if (t->ndim == 2 && axis == 1) {
        int64_t rows = t->shape[0];
        shape[0] = rows; shape[1] = end - start;
        r = aurora_tensor_new(2, shape);
        if (!r) return nullptr;
        for (int64_t i = 0; i < rows; i++)
            for (int64_t j = start; j < end; j++)
                r->data[i * (end - start) + (j - start)] = tensor_read_f64(t, i * t->shape[1] + j);
    } else {
        return nullptr;
    }
    return r;
}

/* ── Activation functions (in-place, dtype-aware, autograd-aware) ── */
void aurora_tensor_relu(AuroraTensor* t) {
    if (!t) return;
    int64_t n = t->total_size;
    if (t->dtype == TENSOR_F32) {
#if AURORA_CUDA
        if (cuda_elewise_available()) {
            if (cuda_relu_f32(t->data_f32, n)) {
                if (t->requires_grad) {
                    t->backward_fn = backward_relu;
                    if (t->prev_count == 0) t->prev[t->prev_count++] = t;
                }
                return;
            }
        }
#endif
        for (int64_t i = 0; i < n; i++)
            if (t->data_f32[i] < 0.0f) t->data_f32[i] = 0.0f;
    } else {
        for (int64_t i = 0; i < n; i++) {
            double v = tensor_read_f64(t, i);
            if (v < 0.0) v = 0.0;
            switch (t->dtype) {
                case TENSOR_F16: ((uint16_t*)t->data_ptr)[i] = f32_to_f16((float)v); break;
                case TENSOR_BF16: ((uint16_t*)t->data_ptr)[i] = f32_to_bf16((float)v); break;
                default: t->data[i] = v; break;
            }
        }
    }
    if (t->requires_grad) {
        /* Chain backward: the current backward_fn runs after this activation's backward */
        t->backward_fn = backward_relu;
        /* Ensure t is in its own prev list for topo_sort */
        if (t->prev_count == 0) {
            t->prev[t->prev_count++] = t;
        }
    }
}

void aurora_tensor_sigmoid(AuroraTensor* t) {
    if (!t) return;
    int64_t n = t->total_size;
    if (t->dtype == TENSOR_F32) {
#if AURORA_CUDA
        if (cuda_elewise_available()) {
            if (cuda_sigmoid_f32(t->data_f32, n)) {
                if (t->requires_grad) {
                    t->backward_fn = backward_sigmoid;
                    if (t->prev_count == 0) t->prev[t->prev_count++] = t;
                }
                return;
            }
        }
#endif
        for (int64_t i = 0; i < n; i++)
            t->data_f32[i] = 1.0f / (1.0f + expf(-t->data_f32[i]));
    } else {
        for (int64_t i = 0; i < n; i++) {
            double v = 1.0 / (1.0 + exp(-tensor_read_f64(t, i)));
            switch (t->dtype) {
                case TENSOR_F16: ((uint16_t*)t->data_ptr)[i] = f32_to_f16((float)v); break;
                case TENSOR_BF16: ((uint16_t*)t->data_ptr)[i] = f32_to_bf16((float)v); break;
                default: t->data[i] = v; break;
            }
        }
    }
    if (t->requires_grad) {
        t->backward_fn = backward_sigmoid;
        if (t->prev_count == 0) {
            t->prev[t->prev_count++] = t;
        }
    }
}

void aurora_tensor_tanh(AuroraTensor* t) {
    if (!t) return;
    int64_t n = t->total_size;
    for (int64_t i = 0; i < n; i++) {
        double v = tanh(tensor_read_f64(t, i));
        switch (t->dtype) {
            case TENSOR_F32: t->data_f32[i] = (float)v; break;
            case TENSOR_F16: ((uint16_t*)t->data_ptr)[i] = f32_to_f16((float)v); break;
            case TENSOR_BF16: ((uint16_t*)t->data_ptr)[i] = f32_to_bf16((float)v); break;
            default: t->data[i] = v; break;
        }
    }
    if (t->requires_grad) {
        t->backward_fn = backward_tanh;
        if (t->prev_count == 0) {
            t->prev[t->prev_count++] = t;
        }
    }
}

/* ── Neural network operations ── */
AuroraTensor* aurora_neural_forward(AuroraTensor* input, AuroraTensor* weights, AuroraTensor* bias) {
    if (!input || !weights) return nullptr;
    AuroraTensor* z = aurora_tensor_matmul(input, weights);
    if (bias && z) {
        for (int64_t i = 0; i < z->total_size && i < bias->total_size; i++)
            (z->dtype == TENSOR_F32 ? (void)(z->data_f32[i] += bias->data_f32[i]) : (void)(z->data[i] += bias->data[i]));
    }
    if (z) aurora_tensor_sigmoid(z);
    return z;
}

AuroraTensor* aurora_predict(AuroraTensor* model, AuroraTensor* input) {
    if (!model || !input) return nullptr;
    int64_t n_features = model->shape[1];
    int64_t n_classes = model->shape[0] - 1;
    int dt = model->dtype;

    int64_t w_shape[2] = { n_features, n_classes };
    AuroraTensor* weights = (dt == TENSOR_F32) ?
        aurora_tensor_new_f32(2, w_shape) : aurora_tensor_new(2, w_shape);
    size_t w_bytes = (size_t)(n_features * n_classes) * ((dt == TENSOR_F32) ? sizeof(float) : sizeof(double));
    memcpy(weights->data_ptr, model->data_ptr, w_bytes);

    int64_t b_shape[2] = { 1, n_classes };
    AuroraTensor* bias = (dt == TENSOR_F32) ?
        aurora_tensor_new_f32(2, b_shape) : aurora_tensor_new(2, b_shape);
    size_t b_off = (size_t)(n_features * n_classes);
    size_t b_bytes = (size_t)n_classes * ((dt == TENSOR_F32) ? sizeof(float) : sizeof(double));
    memcpy(bias->data_ptr, (char*)model->data_ptr + b_off * ((dt == TENSOR_F32) ? sizeof(float) : sizeof(double)), b_bytes);

    AuroraTensor* result = aurora_neural_forward(input, weights, bias);
    aurora_tensor_free(weights);
    aurora_tensor_free(bias);
    return result;
}

/* ════════════════════════════════════════════════════════════
   INT8 Quantization Helpers
   ════════════════════════════════════════════════════════════ */

/* Quantize F32 tensor to INT8 with scale and zero_point */
/* Returns a new tensor with dtype indicating 'quantized' */
AuroraTensor* aurora_tensor_quantize_i8(AuroraTensor* t) {
    if (!t) return nullptr;
    /* Work in F32 for quantization */
    AuroraTensor* src = (t->dtype == TENSOR_F32) ? t : aurora_tensor_to_f32(t);

    int64_t n = src->total_size;
    /* Find min/max */
    float min_val = src->data_f32[0], max_val = src->data_f32[0];
    for (int64_t i = 1; i < n; i++) {
        if (src->data_f32[i] < min_val) min_val = src->data_f32[i];
        if (src->data_f32[i] > max_val) max_val = src->data_f32[i];
    }

    /* Compute scale and zero_point for symmetric INT8 */
    float abs_max = (fabsf(min_val) > fabsf(max_val)) ? fabsf(min_val) : fabsf(max_val);
    float scale = (abs_max > 1e-10f) ? abs_max / 127.0f : 1.0f;
    int8_t zero_point = 0;

    /* Allocate quantized tensor — we reuse total_size for element count */
    AuroraTensor* q = (AuroraTensor*)calloc(1, sizeof(AuroraTensor));
    if (!q) { if (src != t) aurora_tensor_free(src); return nullptr; }
    q->ndim = t->ndim;
    q->shape = (int64_t*)malloc((size_t)t->ndim * sizeof(int64_t));
    if (!q->shape) { free(q); if (src != t) aurora_tensor_free(src); return nullptr; }
    memcpy(q->shape, t->shape, (size_t)t->ndim * sizeof(int64_t));
    q->total_size = n;
    q->dtype = 10; /* TENSOR_QINT8 marker */
    q->data_ptr = malloc((size_t)n * sizeof(int8_t));
    if (!q->data_ptr) { free(q->shape); free(q); if (src != t) aurora_tensor_free(src); return nullptr; }

    int8_t* qdata = (int8_t*)q->data_ptr;
    for (int64_t i = 0; i < n; i++) {
        int32_t qv = (int32_t)roundf(src->data_f32[i] / scale);
        if (qv > 127) qv = 127;
        if (qv < -128) qv = -128;
        qdata[i] = (int8_t)qv;
    }

    /* Store scale and zero_point after the data (for dequantize) */
    /* We store them in a small metadata block */
    float* meta = (float*)malloc(2 * sizeof(float));
    meta[0] = scale;
    meta[1] = (float)zero_point;
    q->grad = (double*)meta; /* Reuse grad pointer for metadata storage */

    if (src != t) aurora_tensor_free(src);
    return q;
}

/* Dequantize INT8 tensor back to F64 */
AuroraTensor* aurora_tensor_dequantize(AuroraTensor* q) {
    if (!q || q->dtype != 10 || !q->data_ptr) return nullptr;
    int64_t n = q->total_size;
    float* meta = (float*)q->grad;
    float scale = meta ? meta[0] : 1.0f;
    int8_t zero_point = meta ? (int8_t)meta[1] : 0;
    int8_t* qdata = (int8_t*)q->data_ptr;

    AuroraTensor* r = aurora_tensor_new(q->ndim, q->shape);
    for (int64_t i = 0; i < n; i++)
        r->data[i] = (double)((float)qdata[i] * scale + (float)zero_point);
    return r;
}

} /* extern "C" */
