#pragma once
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cfloat>
#include "common/platform.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "runtime/string.hpp"
#include "runtime/tensor.hpp"
#include "std/json.hpp"

extern "C" {
    void aurora_panic(const char* msg);
    int64_t aurora_array_len(int64_t arr_ptr);
    int64_t aurora_array_new(int64_t cap);
    void aurora_array_push_int(int64_t arr_ptr, int64_t val);
    void aurora_array_push_float(int64_t arr_ptr, double val);
    void aurora_array_push_str(int64_t arr_ptr, const char* str);
    int64_t aurora_array_get_int(int64_t arr_ptr, int64_t idx);
    double aurora_array_get_float(int64_t arr_ptr, int64_t idx);
    const char* aurora_array_get_str(int64_t arr_ptr, int64_t idx);
    void aurora_array_set_int(int64_t arr_ptr, int64_t idx, int64_t val);
    int64_t aurora_array_contains_int(int64_t arr_ptr, int64_t val);
    int64_t aurora_time();
    double aurora_math_random();
}

static inline const char* aurora_str_ptr(const void* s) { return (const char*)s; }

static inline double parse_double(const char* s, int64_t len) {
    if (!s || len <= 0) return 0.0;
    char* buf = (char*)malloc((size_t)len + 1);
    if (!buf) return 0.0;
    for (int64_t i = 0; i < len; i++) buf[i] = s[i];
    buf[len] = '\0';
    char* end = nullptr;
    double val = strtod(buf, &end);
    free(buf);
    return val;
}

#define MAX_LAYERS 128
#define MODEL_TYPE_SEQ 1
#define LAYER_DENSE 0
#define LAYER_CONV 1
#define LAYER_LSTM 2
#define LAYER_GRU 3
#define LAYER_DROPOUT 4
#define LAYER_BATCHNORM 5
#define LAYER_ATTENTION 6
#define LAYER_FLATTEN 7
#define LAYER_LAYERNORM 8
#define LAYER_EMBEDDING 9
#define LAYER_POS_ENCODING 10
#define LAYER_UNEMBED 11
#define LAYER_ROPE 12
#define LAYER_MUL 13
#define LAYER_MOE 14
#define LAYER_QUANT 15
#define LAYER_TRANSFORMER 16
#define LAYER_RESIDUAL_SAVE 17
#define LAYER_RESIDUAL_ADD 18
#define LAYER_SWIGLU 19
#define ACT_LINEAR 0
#define ACT_RELU 1
#define ACT_SIGMOID 2
#define ACT_TANH 3
#define ACT_LEAKY_RELU 4
#define ACT_SOFTMAX 5
#define ACT_SILU 6
#define ACT_GELU 7
#define LOSS_MSE 0
#define LOSS_CROSS_ENTROPY 1
#define LOSS_BINARY_CROSS_ENTROPY 2
#define LOSS_CATEGORICAL_CROSS_ENTROPY 3
#define OPTIM_SGD 0
#define OPTIM_ADAM 1
#define OPTIM_RMSPROP 2
#define LN_EPS 1e-5

/* Parallelism strategies */
#define PARALLEL_DP 0
#define PARALLEL_TP 1
#define PARALLEL_PP 2

typedef struct {
    int type; int64_t units; int activation; double dropout_rate;
    int64_t kernel_size; int64_t stride;
    AuroraTensor* w; AuroraTensor* b; AuroraTensor* cache;
    AuroraTensor* dw; AuroraTensor* db;
    AuroraTensor* running_mean; AuroraTensor* running_var;
    double momentum; double epsilon;
    AuroraTensor* hc_w; AuroraTensor* hc_b; AuroraTensor* hc_c;
    AuroraTensor* hc_d; /* gradient for hc_b (used by SwiGLU down proj) */
    int64_t num_heads; int64_t embed_dim; int64_t vocab_size; int64_t max_seq_len;
    AuroraTensor* kv_cache_k; AuroraTensor* kv_cache_v; int64_t cache_len;
    /* KV-cache quantization */
    int8_t* kv_qbuf; int64_t kv_qlen; double kv_qscale; double kv_qzero;
    AuroraTensor* tied_w;
    AuroraTensor* residual; int64_t residual_layer;
    AuroraTensor* residual_dout;
    int8_t* q_data; int64_t q_size; double q_scale; double q_zero;
    /* FlashAttention: per-head running max/denom for online softmax */
    double* flash_m; double* flash_d;
    /* MoE auxiliary loss (accumulated per-batch) */
    double aux_loss;
    int dtype; /* TENSOR_F64 or TENSOR_F32 */
    /* Mixed precision: master weights (F64) for optimizer updates */
    AuroraTensor* master_w;
    AuroraTensor* master_b;
    AuroraTensor* master_hc_w;
    AuroraTensor* master_hc_b;
    /* LoRA low-rank adaptation */
    int lora_enabled;
    int lora_r;
    double lora_alpha;
    double lora_dropout;
    AuroraTensor* lora_A;    /* [input_size, r] */
    AuroraTensor* lora_B;    /* [r, units] */
    AuroraTensor* lora_A_grad;
    AuroraTensor* lora_B_grad;
} Layer;

typedef struct {
    int model_type; Layer layers[128]; int n_layers;
    double last_loss; double last_accuracy; int64_t total_params;
    int loss_type; int optimizer_type; double learning_rate;
    int64_t batch_size; int64_t epochs; int64_t current_epoch;
    int64_t verbose; int64_t early_stop_patience;
    double validation_split; int64_t best_epoch; double best_val_loss;
    int64_t no_improve_count; int64_t checkpoint_interval;
    int64_t augment; double lr_factor; double min_lr; double gradient_clip;
    int dtype; /* TENSOR_F64 or TENSOR_F32 — precision for weights */
    /* Distributed / TP / PP fields */
    int parallel_strategy;
    int pp_stage_rank; int pp_group_size; int64_t pp_start; int64_t pp_end;
    int tp_group_rank; int tp_group_size;
    /* MoE auxiliary loss weight */
    double moe_aux_loss; double moe_aux_weight;
    /* PagedAttention / continuous batching */
    void* paged_mgr;
    /* Mixed precision (TENSOR_F16 or TENSOR_BF16) */
    int mixed_precision;
    /* Gradient checkpointing */
    int checkpoint_freq;
    AuroraTensor* checkpoint_acts[128];
    int checkpoint_layers[128];
    int n_checkpoints;
} Model;

/* Active model + tokenizer for generation pipeline */
extern "C" {
    extern Model* g_active_model;
    extern int64_t g_active_tokenizer;
}

typedef struct {
    int type; double lr; double beta1, beta2; double epsilon;
    double decay; double momentum; int64_t t; int64_t param_count;
    double* m; double* v;
} OptimState;

#define MAX_TOOLS 32
typedef struct { char* name; char* desc; } ToolDef;
typedef struct { int64_t id; int64_t reward; int64_t episodes; int nt; ToolDef tools[32]; int active; } Agent;
#define CHAT_MAX 128
#define MSG_LEN 4096
typedef struct { char role[32]; char content[4096]; } Msg;
typedef struct { Msg hist[128]; int n; char system_prompt[4096]; } ChatS;

static inline double rand_uniform() { return ((double)rand() / RAND_MAX) * 2.0 - 1.0; }
static inline double randn() {
    double u1 = (double)rand() / RAND_MAX;
    if (u1 < 1e-10) u1 = 1e-10;
    return sqrt(-2.0 * log(u1)) * cos(2.0 * 3.14159265358979 * ((double)rand() / RAND_MAX));
}

static inline AuroraTensor* l_tensor(Layer* l, int64_t ndim, int64_t* shape) {
    return (l->dtype == TENSOR_F32) ? aurora_tensor_new_f32(ndim, shape) : aurora_tensor_new(ndim, shape);
}

static inline AuroraTensor* l_grad_tensor(Layer* l, int64_t ndim, int64_t* shape) {
    (void)l; return aurora_tensor_new(ndim, shape);
}

/* Layer forward/backward */
extern "C" {
AuroraTensor* dense_forward(Layer* l, AuroraTensor* input, int training);
AuroraTensor* dense_backward(Layer* l, AuroraTensor* dout, AuroraTensor* input);
AuroraTensor* conv2d_forward(Layer* l, AuroraTensor* input, int training);
AuroraTensor* conv2d_backward(Layer* l, AuroraTensor* dout, AuroraTensor* input);
AuroraTensor* lstm_forward(Layer* l, AuroraTensor* input, int training);
AuroraTensor* lstm_backward(Layer* l, AuroraTensor* dout, AuroraTensor* input);
AuroraTensor* gru_forward(Layer* l, AuroraTensor* input, int training);
AuroraTensor* gru_backward(Layer* l, AuroraTensor* dout, AuroraTensor* input);
AuroraTensor* layernorm_forward(Layer* l, AuroraTensor* input, int training);
AuroraTensor* layernorm_backward(Layer* l, AuroraTensor* dout, AuroraTensor* input);
AuroraTensor* embedding_forward(Layer* l, AuroraTensor* input, int training);
AuroraTensor* embedding_backward(Layer* l, AuroraTensor* dout, AuroraTensor* input);
AuroraTensor* pos_encoding_forward(Layer* l, AuroraTensor* input, int training);
AuroraTensor* pos_encoding_backward(Layer* l, AuroraTensor* dout, AuroraTensor* input);
AuroraTensor* rope_forward(Layer* l, AuroraTensor* input, int training);
AuroraTensor* rope_backward(Layer* l, AuroraTensor* dout, AuroraTensor* input);
AuroraTensor* mul_forward(Layer* l, AuroraTensor* input, int training);
AuroraTensor* mul_backward(Layer* l, AuroraTensor* dout, AuroraTensor* input);
AuroraTensor* moe_forward(Layer* l, AuroraTensor* input, int training);
AuroraTensor* moe_backward(Layer* l, AuroraTensor* dout, AuroraTensor* input);
int64_t quantize_weights(Model* m);
AuroraTensor* mha_forward(Layer* l, AuroraTensor* input, int training);
AuroraTensor* mha_backward(Layer* l, AuroraTensor* dout, AuroraTensor* input);
AuroraTensor* swiglu_forward(Layer* l, AuroraTensor* input, int training);
AuroraTensor* swiglu_backward(Layer* l, AuroraTensor* dout, AuroraTensor* input);
AuroraTensor* unembed_forward(Layer* l, AuroraTensor* input, int training);
AuroraTensor* unembed_backward(Layer* l, AuroraTensor* dout, AuroraTensor* input);
void activation_forward(AuroraTensor* t, int act);
void activation_backward(AuroraTensor* out, AuroraTensor* dout, int act);
int model_init_layer_weights(Model* m, int idx, int64_t input_size);
void im2col(const double* input, int64_t batch, int64_t h, int64_t w, int64_t c, int64_t kh, int64_t kw, int64_t sh, int64_t sw, double* col, int64_t oh, int64_t ow);
void col2im(const double* col, int64_t batch, int64_t h, int64_t w, int64_t c, int64_t kh, int64_t kw, int64_t sh, int64_t sw, double* input, int64_t oh, int64_t ow);

/* Optimizer (defined in ai_train.cpp) */
void optim_update_offset(OptimState* o, double* params, double* grads, int64_t offset, int64_t n);

/* Distributed (defined in ai_train.cpp) */
void distributed_sync_gradients(Model* m);
void distributed_broadcast_weights(Model* m, int root);

/* Model (defined in ai_model.cpp) */
AuroraTensor* model_forward(Model* m, AuroraTensor* input, int training);

/* Paged attention (defined in ai_layers.cpp) */
struct PagedManager;
void mha_inference_cached(struct PagedManager* pm, int64_t seq_id, Layer* l, AuroraTensor* q, AuroraTensor* k, AuroraTensor* v, AuroraTensor* out, int64_t h, int64_t n, int64_t d, int64_t d_h);

/* Generation (defined in ai_gen.cpp) */
int64_t generate_step(Model* m, int64_t token, int64_t* cache_len_ptr);
char* generate_text(Model* m, int64_t start_token, int64_t max_toks);
void reset_kv_cache(Model* m);
/* Chat (defined in ai_gen.cpp) */
int64_t chat_new();
int64_t chat_system(int64_t session, const char* prompt);
char* chat_send(int64_t session, const char* prompt);
int64_t chat_reset(int64_t session);
/* Full generate: tokenize → run model → decode */
char* model_generate(Model* m, int64_t tokenizer_ptr, const char* text, int64_t max_toks);

/* Speculative decoding (defined in ai_speculative.cpp) */
char* speculative_decode(Model* target, Model* draft, int64_t start_token, int64_t max_tokens, int64_t K);

/* Enhanced quantization (defined in ai_layers.cpp) */
int64_t quantize_groupwise(Model* m, int64_t group_size);
int64_t quantize_kv_cache(Model* m);

/* ONNX export (defined in ai_onnx.cpp) */
int model_export_onnx(Model* m, const char* path);

/* Weight loaders (defined in ai_loaders.cpp) */
int64_t loader_load_nanogpt(const char* path);
int64_t loader_get_config(int64_t idx);

/* Optimizer checkpointing (defined in ai_train.cpp) */
int optim_save(OptimState* o, const char* path);
int optim_load(OptimState* o, const char* path);

/* Mixed precision helpers (defined in ai_train.cpp) */
int model_cast_to_lowp(Model* m, int low_dtype);
int model_sync_weights_from_master(Model* m);
int model_sync_weights_to_master(Model* m);

/* LoRA initialization (defined in ai_train.cpp) */
int model_add_lora(Model* m, int rank, double alpha, double dropout);
} /* extern "C" */
