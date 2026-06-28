#pragma once

/* ════════════════════════════════════════════════════════════
   DEPRECATED — Tensor v2 API has been merged into v1.
   Use "runtime/tensor.hpp" instead (aurora_tensor_* API).
   This header is retained for compatibility only.
   ════════════════════════════════════════════════════════════ */

#if defined(_MSC_VER)
#pragma message(__FILE__ ": warning: tensor_v2.hpp is deprecated. Include runtime/tensor.hpp instead.")
#elif defined(__GNUC__) || defined(__clang__)
#warning "tensor_v2.hpp is deprecated. Include runtime/tensor.hpp instead."
#endif

#include "runtime/tensor.hpp"
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* dtype constants (re-exported for compatibility) */
#ifndef TENSOR_F64
#define TENSOR_F64 0
#endif
#ifndef TENSOR_F32
#define TENSOR_F32 1
#endif

/* Deprecated alias — use AuroraTensor instead */
typedef AuroraTensor AuroraTensorV2;
    int64_t  ndim;
    int64_t* shape;
    void*    data;      /* generic pointer (always valid) */
    double*  data_f64;  /* convenient access for F64 (same as data) */
    float*   data_f32;  /* convenient access for F32 (same as data) */
    int64_t  total_size;
    int      dtype;     /* TENSOR_F64 or TENSOR_F32 */
} AuroraTensorV2;

/* ── Deprecated compatibility wrappers (forward to v1 API) ── */

static inline AuroraTensor* tensor_v2_new(int64_t ndim, int64_t* shape, int dtype) {
    return aurora_tensor_new_with_dtype(ndim, shape, dtype);
}

static inline void tensor_v2_free(AuroraTensor* t) {
    aurora_tensor_free(t);
}

static inline AuroraTensor* tensor_v2_to_f32(AuroraTensor* t) {
    return aurora_tensor_to_f32(t);
}

static inline AuroraTensor* tensor_v2_to_f64(AuroraTensor* t) {
    return aurora_tensor_to_f64(t);
}

static inline AuroraTensor* tensor_v2_matmul(AuroraTensor* a, AuroraTensor* b) {
    return aurora_tensor_matmul(a, b);
}

static inline AuroraTensor* tensor_v2_add(AuroraTensor* a, AuroraTensor* b) {
    return aurora_tensor_add(a, b);
}

static inline AuroraTensor* tensor_v2_sub(AuroraTensor* a, AuroraTensor* b) {
    return aurora_tensor_sub(a, b);
}

static inline AuroraTensor* tensor_v2_mul(AuroraTensor* a, AuroraTensor* b) {
    return aurora_tensor_mul(a, b);
}

static inline AuroraTensor* tensor_v2_div(AuroraTensor* a, AuroraTensor* b) {
    return aurora_tensor_div(a, b);
}

static inline void tensor_v2_relu(AuroraTensor* t) { aurora_tensor_relu(t); }
static inline void tensor_v2_sigmoid(AuroraTensor* t) { aurora_tensor_sigmoid(t); }
static inline void tensor_v2_tanh(AuroraTensor* t) { aurora_tensor_tanh(t); }
static inline void tensor_v2_softmax(AuroraTensor* t) { aurora_tensor_softmax(t); }
static inline void tensor_v2_gelu(AuroraTensor* t) { aurora_tensor_gelu(t); }
static inline void tensor_v2_silu(AuroraTensor* t) { aurora_tensor_silu(t); }

static inline double tensor_v2_sum(AuroraTensor* t) { return aurora_tensor_sum(t); }
static inline double tensor_v2_mean(AuroraTensor* t) { return aurora_tensor_mean(t); }

static inline AuroraTensor* tensor_v2_transpose(AuroraTensor* t) {
    return aurora_tensor_transpose(t);
}

static inline AuroraTensor* tensor_v2_reshape(AuroraTensor* t, int64_t ndim, int64_t* shape) {
    return aurora_tensor_reshape(t, ndim, shape);
}

#ifdef __cplusplus
}
#endif
