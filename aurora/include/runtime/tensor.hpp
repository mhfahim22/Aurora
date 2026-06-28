#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif

/* FP16 (IEEE 754 half-precision) encode/decode */
static inline uint16_t f32_to_f16(float v) {
    uint32_t bits;
    memcpy(&bits, &v, sizeof(bits));
    uint16_t sign = (uint16_t)((bits >> 16) & 0x8000);
    int32_t exp = (int32_t)((bits >> 23) & 0xFF) - 127 + 15;
    uint32_t mant = bits & 0x007FFFFF;
    if (exp >= 31) return (uint16_t)(sign | 0x7C00);
    if (exp <= 0) {
        mant = (mant | 0x00800000) >> (1 - exp);
        exp = 0;
    }
    return (uint16_t)(sign | ((uint16_t)exp << 10) | (uint16_t)(mant >> 13));
}

static inline float f16_to_f32(uint16_t h) {
    uint32_t sign = ((uint32_t)h & 0x8000) << 16;
    int32_t exp = ((int32_t)(h >> 10) & 0x1F) - 15 + 127;
    uint32_t mant = (uint32_t)(h & 0x03FF) << 13;
    if (((h >> 10) & 0x1F) == 0) {
        if (mant != 0) {
            while ((mant & 0x00800000) == 0) { mant <<= 1; exp--; }
            exp++;
        }
    }
    if (((h >> 10) & 0x1F) == 31) exp = 255;
    uint32_t bits = sign | ((uint32_t)(exp & 0xFF) << 23) | (mant & 0x007FFFFF);
    float r;
    memcpy(&r, &bits, sizeof(r));
    return r;
}

/* BF16 encode/decode: truncate upper 16 bits of F32 */
static inline uint16_t f32_to_bf16(float v) {
    uint32_t bits;
    memcpy(&bits, &v, sizeof(bits));
    return (uint16_t)(bits >> 16);
}

static inline float bf16_to_f32(uint16_t h) {
    uint32_t bits = (uint32_t)h << 16;
    float r;
    memcpy(&r, &bits, sizeof(r));
    return r;
}

/* dtype constants */
#define TENSOR_F64 0
#define TENSOR_F32 1
#define TENSOR_F16 2
#define TENSOR_BF16 3

/* ── Tensor structure ── */
typedef struct AuroraTensor {
    int64_t  ndim;
    int64_t* shape;
    union {
        double* data;    /* F64 access (same as data_ptr) */
        float*  data_f32; /* F32 access */
        void*   data_ptr; /* generic */
    };
    int64_t  total_size;
    int      dtype;      /* TENSOR_F64 or TENSOR_F32 */

    /* Autograd fields (zero-initialized → disabled by default) */
    double*  grad;
    int      requires_grad;
    void   (*backward_fn)(struct AuroraTensor* self);
    struct AuroraTensor* prev[8];
    int      prev_count;
} AuroraTensor;

/* ── Tensor operations ── */
AuroraTensor* aurora_tensor_new(int64_t ndim, int64_t* shape);
AuroraTensor* aurora_tensor_new_f32(int64_t ndim, int64_t* shape);
AuroraTensor* aurora_tensor_new_f16(int64_t ndim, int64_t* shape);
AuroraTensor* aurora_tensor_new_bf16(int64_t ndim, int64_t* shape);
AuroraTensor* aurora_tensor_new_with_dtype(int64_t ndim, int64_t* shape, int dtype);
void          aurora_tensor_free(AuroraTensor* t);
int64_t       aurora_tensor_ndim(AuroraTensor* t);
int64_t       aurora_tensor_shape(AuroraTensor* t, int64_t dim);
double        aurora_tensor_get(AuroraTensor* t, int64_t* indices);
void          aurora_tensor_set(AuroraTensor* t, int64_t* indices, double val);
AuroraTensor* aurora_tensor_add(AuroraTensor* a, AuroraTensor* b);
AuroraTensor* aurora_tensor_sub(AuroraTensor* a, AuroraTensor* b);
AuroraTensor* aurora_tensor_mul(AuroraTensor* a, AuroraTensor* b);
AuroraTensor* aurora_tensor_matmul(AuroraTensor* a, AuroraTensor* b);
AuroraTensor* aurora_tensor_reshape(AuroraTensor* t, int64_t ndim, int64_t* shape);
AuroraTensor* aurora_tensor_div(AuroraTensor* a, AuroraTensor* b);
AuroraTensor* aurora_tensor_pow(AuroraTensor* a, double exp_val);
void          aurora_tensor_exp(AuroraTensor* t);
void          aurora_tensor_log(AuroraTensor* t);
double        aurora_tensor_sum(AuroraTensor* t);
double        aurora_tensor_mean(AuroraTensor* t);
double        aurora_tensor_std(AuroraTensor* t);
AuroraTensor* aurora_tensor_transpose(AuroraTensor* t);
void          aurora_tensor_softmax(AuroraTensor* t);
AuroraTensor* aurora_tensor_concatenate(AuroraTensor* a, AuroraTensor* b, int64_t axis);
AuroraTensor* aurora_tensor_slice(AuroraTensor* t, int64_t start, int64_t end, int64_t axis);
AuroraTensor* aurora_tensor_to_f32(AuroraTensor* t);
AuroraTensor* aurora_tensor_to_f64(AuroraTensor* t);
AuroraTensor* aurora_tensor_to_f16(AuroraTensor* t);
AuroraTensor* aurora_tensor_to_bf16(AuroraTensor* t);
AuroraTensor* aurora_tensor_clone(AuroraTensor* t);

/* ── Neural network operations ── */
AuroraTensor* aurora_neural_forward(AuroraTensor* input, AuroraTensor* weights, AuroraTensor* bias);
AuroraTensor* aurora_predict(AuroraTensor* model, AuroraTensor* input);

/* ── Activation functions ── */
void aurora_tensor_relu(AuroraTensor* t);
void aurora_tensor_sigmoid(AuroraTensor* t);
void aurora_tensor_tanh(AuroraTensor* t);
void aurora_tensor_gelu(AuroraTensor* t);
void aurora_tensor_silu(AuroraTensor* t);

/* ── Autograd (computation graph & gradient-based optimisation) ── */

/* Enable / disable gradient tracking on a tensor */
void aurora_tensor_set_requires_grad(AuroraTensor* t, int flag);

/* Reset gradients of a tensor to zero (call between training steps) */
void aurora_tensor_zero_grad(AuroraTensor* t);

/* Run the backward pass — computes gradients for all ancestors via chain rule */
void aurora_tensor_backward(AuroraTensor* t);

/* Apply one step of SGD: param = param - lr * param->grad */
void aurora_tensor_sgd_step(AuroraTensor* param, double lr);

/* Backward function type — stored in AuroraTensor::backward_fn */
typedef void (*autograd_backward_fn)(AuroraTensor*);

/* Per-operation backward implementations (used by tensor.cpp link_grad calls) */
void backward_add(AuroraTensor* self);
void backward_sub(AuroraTensor* self);
void backward_mul(AuroraTensor* self);
void backward_div(AuroraTensor* self);
void backward_pow(AuroraTensor* self);
void backward_matmul(AuroraTensor* self);
void backward_relu(AuroraTensor* self);
void backward_sigmoid(AuroraTensor* self);
void backward_tanh(AuroraTensor* self);

/* Graph construction helper — wires result into the autograd graph */
AuroraTensor* link_grad(AuroraTensor* result, autograd_backward_fn backward,
                         AuroraTensor* input1, AuroraTensor* input2,
                         AuroraTensor* input3, AuroraTensor* input4);

#ifdef __cplusplus
}
#endif
