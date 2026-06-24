#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* dtype constants */
#define TENSOR_F64 0
#define TENSOR_F32 1

/* Extended tensor with dtype support */
typedef struct AuroraTensorV2 {
    int64_t  ndim;
    int64_t* shape;
    void*    data;      /* generic pointer (always valid) */
    double*  data_f64;  /* convenient access for F64 (same as data) */
    float*   data_f32;  /* convenient access for F32 (same as data) */
    int64_t  total_size;
    int      dtype;     /* TENSOR_F64 or TENSOR_F32 */
} AuroraTensorV2;

/* ── Creation / freeing ── */
AuroraTensorV2* tensor_v2_new(int64_t ndim, int64_t* shape, int dtype);
void            tensor_v2_free(AuroraTensorV2* t);

/* ── Conversion ── */
AuroraTensorV2* tensor_v2_to_f32(AuroraTensorV2* t);
AuroraTensorV2* tensor_v2_to_f64(AuroraTensorV2* t);

/* ── FP32-optimized matmul (blocked, cache-aware) ── */
AuroraTensorV2* tensor_v2_matmul_f32(AuroraTensorV2* a, AuroraTensorV2* b);

/* ── Generic matmul (dispatches by dtype) ── */
AuroraTensorV2* tensor_v2_matmul(AuroraTensorV2* a, AuroraTensorV2* b);

/* ── Element-wise ops (dtype-preserving) ── */
AuroraTensorV2* tensor_v2_add(AuroraTensorV2* a, AuroraTensorV2* b);
AuroraTensorV2* tensor_v2_sub(AuroraTensorV2* a, AuroraTensorV2* b);
AuroraTensorV2* tensor_v2_mul(AuroraTensorV2* a, AuroraTensorV2* b);
AuroraTensorV2* tensor_v2_div(AuroraTensorV2* a, AuroraTensorV2* b);

/* ── Activations (in-place, dtype-aware) ── */
void tensor_v2_relu(AuroraTensorV2* t);
void tensor_v2_sigmoid(AuroraTensorV2* t);
void tensor_v2_tanh(AuroraTensorV2* t);
void tensor_v2_softmax(AuroraTensorV2* t);
void tensor_v2_gelu(AuroraTensorV2* t);
void tensor_v2_silu(AuroraTensorV2* t);

/* ── Reduction ── */
double tensor_v2_sum(AuroraTensorV2* t);
double tensor_v2_mean(AuroraTensorV2* t);

/* ── Utility ── */
AuroraTensorV2* tensor_v2_transpose(AuroraTensorV2* t);
AuroraTensorV2* tensor_v2_reshape(AuroraTensorV2* t, int64_t ndim, int64_t* shape);

#ifdef __cplusplus
}
#endif
