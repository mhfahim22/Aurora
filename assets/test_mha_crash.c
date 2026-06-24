#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "runtime/ai/ai_common.h"
#include "runtime/runtime_exports.hpp"

int main() {
    printf("=== Test MHA Backward (C) ===\n");
    Model* m = model_create("sequential");
    model_add_dense(m, 2, 4, ACT_RELU);
    /* Manually add MHA layer */
    Layer* lmha = &m->layers[m->n_layers++];
    memset(lmha, 0, sizeof(Layer));
    lmha->type = LAYER_ATTENTION;
    lmha->num_heads = 2;
    lmha->units = 4; lmha->embed_dim = 4;
    model_add_dense(m, 4, 1, ACT_SIGMOID);
    m->loss_type = LOSS_BINARY_CROSS_ENTROPY;

    /* Create small dataset */
    int64_t n = 4, nf = 2;
    int64_t xs[2] = { n, nf };
    int64_t ys[2] = { n, 1 };
    AuroraTensor* X = aurora_tensor_new(2, xs);
    AuroraTensor* y = aurora_tensor_new(2, ys);
    double xd[] = {0.1,0.2, 0.3,0.4, 0.5,0.6, 0.7,0.8};
    double yd[] = {0,0,1,1};
    memcpy(X->data, xd, sizeof(xd));
    memcpy(y->data, yd, sizeof(yd));
    printf("Data created\n");

    /* First forward pass to initialize weights */
    AuroraTensor* out = model_forward(m, X, 1);
    printf("Forward done, out shape: %lld %lld\n", (long long)out->shape[0], (long long)out->shape[1]);
    printf("MHA layer: w=%p hc_w=%p cache=%p kv_cache_k=%p\n",
        (void*)m->layers[1].w, (void*)m->layers[1].hc_w,
        (void*)m->layers[1].cache, (void*)m->layers[1].kv_cache_k);

    /* Compute loss gradient */
    AuroraTensor* dloss = aurora_tensor_new(out->ndim, out->shape);
    double loss = loss_compute(out, y, m->loss_type, dloss);
    printf("Loss: %f\n", loss);

    /* Backward pass for LAST dense */
    AuroraTensor* layer_in = forward_to_layer(m, X, 2);
    printf("layer_in for dense(2): shape %lld %lld\n",
        (long long)layer_in->shape[0], (long long)layer_in->shape[1]);
    AuroraTensor* dp = dense_backward(&m->layers[2], dloss, layer_in);
    printf("dense_backward dp shape: %lld %lld\n",
        (long long)dp->shape[0], (long long)dp->shape[1]);
    aurora_tensor_free(layer_in);

    /* Backward pass for MHA */
    printf("\n=== MHA BACKWARD ===\n");
    printf("MHA layer: cache=%p kv_cache_k=%p\n",
        (void*)m->layers[1].cache, (void*)m->layers[1].kv_cache_k);
    printf("l->cache total_size: %lld\n", (long long)m->layers[1].cache->total_size);
    printf("l->kv_cache_k total_size: %lld\n", (long long)m->layers[1].kv_cache_k->total_size);
    fflush(stdout);

    AuroraTensor* layer_in2 = forward_to_layer(m, X, 1);
    printf("layer_in for mha: shape %lld %lld\n",
        (long long)layer_in2->shape[0], (long long)layer_in2->shape[1]);
    fflush(stdout);

    AuroraTensor* dp2 = mha_backward(&m->layers[1], dp, layer_in2);
    printf("mha_backward done, dp2=%p\n", (void*)dp2);
    fflush(stdout);

    aurora_tensor_free(layer_in2);
    if (dp2) aurora_tensor_free(dp2);
    aurora_tensor_free(dp);
    aurora_tensor_free(dloss);
    aurora_tensor_free(out);
    aurora_tensor_free(X);
    aurora_tensor_free(y);
    model_free(m);
    printf("ALL DONE\n");
    return 0;
}
