$content = @'
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cfloat>
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

static const char* aurora_str_ptr(const void* s) {
    return s ? ((const AuroraStr*)s)->ptr : nullptr;
}

static double parse_double(const char* s, int64_t len) {
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

/* Layer types */
#define LAYER_DENSE     0
#define LAYER_CONV      1
#define LAYER_LSTM      2
#define LAYER_GRU       3
#define LAYER_DROPOUT   4
#define LAYER_BATCHNORM 5
#define LAYER_ATTENTION 6
#define LAYER_FLATTEN   7
#define LAYER_MAXPOOL   8

/* Activation functions */
#define ACT_LINEAR     0
#define ACT_RELU       1
#define ACT_SIGMOID    2
#define ACT_TANH       3
#define ACT_LEAKY_RELU 4
#define ACT_SOFTMAX    5

/* Loss functions */
#define LOSS_MSE      0
#define LOSS_CROSS_ENTROPY 1
#define LOSS_BINARY_CROSS_ENTROPY 2
#define LOSS_CATEGORICAL_CROSS_ENTROPY 3

/* Optimizers */
#define OPTIM_SGD      0
#define OPTIM_ADAM     1
#define OPTIM_RMSPROP  2

typedef struct {
    int type;
    int64_t units;
    int activation;
    double dropout_rate;
    int64_t kernel_size;
    int64_t stride;
    int64_t in_channels;
    int64_t out_channels;
    AuroraTensor* w;
    AuroraTensor* b;
    AuroraTensor* cache;
    AuroraTensor* dw;
    AuroraTensor* db;
    AuroraTensor* running_mean;
    AuroraTensor* running_var;
    double momentum;
    double epsilon;
    AuroraTensor* hc_w;
    AuroraTensor* hc_b;
} Layer;

typedef struct {
    int model_type;
    Layer layers[MAX_LAYERS];
    int n_layers;
    double last_loss;
    double last_accuracy;
    int64_t total_params;
    int loss_type;
    int optimizer_type;
    double learning_rate;
    int64_t batch_size;
    int64_t epochs;
    int64_t current_epoch;
    int64_t verbose;
    int64_t early_stop_patience;
    double validation_split;
    int64_t best_epoch;
    double best_val_loss;
    int64_t no_improve_count;
    int64_t checkpoint_interval;
} Model;
'@

$content | Set-Content -Path D:\Downloads\aurora_restructured\aurora\src\runtime\builtins\ai_builtins.cpp -NoNewline
