#include "runtime/ai/ai_common.h"
#include "runtime/tensor.hpp"
#include <cstdio>
#include <chrono>
#include <cstdlib>
#include <cmath>

/* Forward decls from ai_builtins.cpp / ai_train.cpp */
extern "C" {
    double train_batch(Model* m, AuroraTensor* X, AuroraTensor* y, OptimState* opt);
    double loss_compute(AuroraTensor* pred, AuroraTensor* target, int loss_type, AuroraTensor* dloss);
    double metric_accuracy(AuroraTensor* pred, AuroraTensor* target);
    int model_init_layer_weights(Model* m, int idx, int64_t input_size);
    void clip_gradients(Model* m, double max_norm);
}

static double now_sec() {
    auto t = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration<double>(t).count();
}

/* Generate synthetic MNIST batch: 784 features, 10 classes */
static void gen_batch(AuroraTensor* X, AuroraTensor* y, int64_t batch_size, int64_t epoch_seed) {
    srand((unsigned)(epoch_seed + 42));
    for (int64_t i = 0; i < batch_size; i++) {
        int label = rand() % 10;
        for (int64_t j = 0; j < 784; j++) {
            double val;
            if ((j / 78) == label) /* rough pattern per class */
                val = 0.8 + ((double)rand() / RAND_MAX) * 0.2;
            else
                val = ((double)rand() / RAND_MAX) * 0.3;
            X->data[i * 784 + j] = val;
        }
        for (int k = 0; k < 10; k++)
            y->data[i * 10 + k] = (k == label) ? 1.0 : 0.0;
    }
}

int main() {
    printf("=== Aurora AI Benchmark ===\n\n");

    int64_t input_size = 784, hidden_size = 128, output_size = 10;
    int64_t batch_size = 32, n_epochs = 5;

    /* ── Build model ── */
    Model m;
    memset(&m, 0, sizeof(m));
    m.model_type = MODEL_TYPE_SEQ;
    m.n_layers = 2;
    m.loss_type = LOSS_CATEGORICAL_CROSS_ENTROPY;
    m.verbose = 0;
    m.dtype = TENSOR_F64;

    /* Layer 0: Dense(128, relu) */
    Layer* l0 = &m.layers[0];
    l0->type = LAYER_DENSE;
    l0->units = hidden_size;
    l0->activation = ACT_RELU;
    l0->dtype = TENSOR_F64;

    /* Layer 1: Dense(10, softmax) */
    Layer* l1 = &m.layers[1];
    l1->type = LAYER_DENSE;
    l1->units = output_size;
    l1->activation = ACT_SOFTMAX;
    l1->dtype = TENSOR_F64;

    /* Init weights */
    model_init_layer_weights(&m, 0, input_size);
    model_init_layer_weights(&m, 1, hidden_size);

    /* ── Optimizer ── */
    OptimState opt;
    memset(&opt, 0, sizeof(opt));
    opt.type = OPTIM_ADAM;
    opt.lr = 0.001;
    opt.beta1 = 0.9;
    opt.beta2 = 0.999;
    opt.epsilon = 1e-8;
    opt.t = 1;
    /* Count params */
    opt.param_count = 0;
    for (int i = 0; i < m.n_layers; i++) {
        if (m.layers[i].w) opt.param_count += m.layers[i].w->total_size;
        if (m.layers[i].b) opt.param_count += m.layers[i].b->total_size;
    }
    opt.m = (double*)calloc((size_t)opt.param_count, sizeof(double));
    opt.v = (double*)calloc((size_t)opt.param_count, sizeof(double));

    /* ── Data tensors ── */
    int64_t x_shape[2] = { batch_size, input_size };
    int64_t y_shape[2] = { batch_size, output_size };
    AuroraTensor* X = aurora_tensor_new(2, x_shape);
    AuroraTensor* y = aurora_tensor_new(2, y_shape);

    /* ── Benchmark epochs ── */
    printf("Model: 784 -> Dense(128,ReLU) -> Dense(10,Softmax)\n");
    printf("Batch: %lld, Epochs: %lld\n\n", (long long)batch_size, (long long)n_epochs);
    printf("Epoch   Loss    Acc     Time(s)\n");
    printf("-----   ----    ---     ------\n");

    double total_time = 0.0;
    for (int ep = 0; ep < n_epochs; ep++) {
        gen_batch(X, y, batch_size, ep);

        double t0 = now_sec();
        double loss = train_batch(&m, X, y, &opt);
        double t1 = now_sec();

        AuroraTensor* pred = model_forward(&m, X, 0);
        double acc = metric_accuracy(pred, y);

        double elapsed = t1 - t0;
        total_time += elapsed;

        printf("%3d     %.4f  %.4f  %.4f\n", ep + 1, loss, acc, elapsed);

        if (pred) aurora_tensor_free(pred);
    }

    printf("\nTotal: %.4f s, Avg: %.4f s/epoch\n", total_time, total_time / n_epochs);

    /* ── Cleanup ── */
    aurora_tensor_free(X);
    aurora_tensor_free(y);
    for (int i = 0; i < m.n_layers; i++) {
        if (m.layers[i].w) aurora_tensor_free(m.layers[i].w);
        if (m.layers[i].b) aurora_tensor_free(m.layers[i].b);
        if (m.layers[i].dw) aurora_tensor_free(m.layers[i].dw);
        if (m.layers[i].db) aurora_tensor_free(m.layers[i].db);
    }
    free(opt.m);
    free(opt.v);

    printf("\n=== Benchmark Complete ===\n");
    return 0;
}
