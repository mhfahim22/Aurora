#include "runtime/ai/ai_common.h"
#include <cstring>
#include <cmath>
#include <cstdlib>

extern "C" {

/* ── nanoGPT / llama2.c binary format loader ──
 *
 * Header:  int32_t magic (0xabcd0123)
 *          int32_t version (1)
 *          int32_t dim, hidden_dim, n_layers, n_heads, n_kv_heads, vocab_size, seq_len
 *
 * Weights (all float32 in row-major order):
 *   token_embedding_table  [vocab_size, dim]
 *   rms_final_weight       [dim]
 *   For each layer:
 *     rms_att_weight       [dim]
 *     wq                   [dim, dim]
 *     wk                   [dim, dim]
 *     wv                   [dim, dim]
 *     wo                   [dim, dim]
 *     rms_ffn_weight       [dim]
 *     w1 (gate)            [hidden_dim, dim]
 *     w2 (down)            [dim, hidden_dim]
 *     w3 (up)              [hidden_dim, dim]
 *   output_weight (maybe tied) [vocab_size, dim]
 */
#define LOADER_MAGIC 0xabcd0123

static int64_t loader_config[] = { 0, 0, 0, 0, 0, 0, 0 };

static float* read_weights(FILE* f, int64_t n) {
    float* w = (float*)malloc((size_t)n * sizeof(float));
    if (!w) return nullptr;
    if (fread(w, sizeof(float), (size_t)n, f) != (size_t)n) { free(w); return nullptr; }
    return w;
}

static AuroraTensor* make_tensor_f32(int64_t rows, int64_t cols, const float* src) {
    int64_t shape[2] = { rows, cols };
    AuroraTensor* t = aurora_tensor_new_f32(2, shape);
    if (!t) return nullptr;
    memcpy(t->data_f32, src, (size_t)(rows * cols) * sizeof(float));
    return t;
}

int64_t loader_load_nanogpt(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;

    int32_t magic, ver;
    if (fread(&magic, 4, 1, f) != 1 || magic != LOADER_MAGIC) { fclose(f); return 0; }
    if (fread(&ver, 4, 1, f) != 1) { fclose(f); return 0; }
    int32_t hdr[7];
    if (fread(hdr, 4, 7, f) != 7) { fclose(f); return 0; }

    int64_t dim = hdr[0], hidden_dim = hdr[1], n_layers = hdr[2];
    int64_t n_heads = hdr[3], n_kv_heads = hdr[4];
    int64_t vocab_size = hdr[5], seq_len = hdr[6];

    loader_config[0] = dim; loader_config[1] = hidden_dim; loader_config[2] = n_layers;
    loader_config[3] = n_heads; loader_config[4] = n_kv_heads;
    loader_config[5] = vocab_size; loader_config[6] = seq_len;

    Model* m = (Model*)calloc(1, sizeof(Model));
    if (!m) { fclose(f); return 0; }
    m->model_type = MODEL_TYPE_SEQ;
    m->n_layers = (int)(3 + n_layers * 4); /* embedding + (layers * 4) + final_norm + output */
    if (m->n_layers > MAX_LAYERS) m->n_layers = MAX_LAYERS;
    m->loss_type = LOSS_MSE; m->optimizer_type = OPTIM_ADAM;
    m->learning_rate = 0.001; m->batch_size = 1; m->epochs = 1;
    m->verbose = 0; m->dtype = TENSOR_F32;

    float* tok_emb = read_weights(f, vocab_size * dim);
    if (!tok_emb) { free(m); fclose(f); return 0; }

    float* rms_final = read_weights(f, dim);
    if (!rms_final) { free(tok_emb); free(m); fclose(f); return 0; }

    int layer_idx = 0;

    /* Layer 0: token embedding */
    Layer* emb = &m->layers[layer_idx++];
    emb->type = LAYER_EMBEDDING;
    emb->units = dim;
    emb->vocab_size = vocab_size;
    emb->w = make_tensor_f32(vocab_size, dim, tok_emb);
    free(tok_emb);

    for (int64_t li = 0; li < n_layers && layer_idx < MAX_LAYERS - 4; li++) {
        float* rms_att = read_weights(f, dim);
        float* wq = read_weights(f, dim * dim);
        float* wk = read_weights(f, dim * dim);
        float* wv = read_weights(f, dim * dim);
        float* wo = read_weights(f, dim * dim);
        float* rms_ffn = read_weights(f, dim);
        float* w1 = read_weights(f, hidden_dim * dim);
        float* w2 = read_weights(f, dim * hidden_dim);
        float* w3 = read_weights(f, hidden_dim * dim);
        if (!rms_att || !wq || !wk || !wv || !wo || !rms_ffn || !w1 || !w2 || !w3) {
            free(rms_att); free(wq); free(wk); free(wv); free(wo);
            free(rms_ffn); free(w1); free(w2); free(w3);
            free(rms_final); free(m); fclose(f); return 0;
        }

        /* Pre-attention RMSNorm */
        Layer* ln1 = &m->layers[layer_idx++];
        ln1->type = LAYER_LAYERNORM; ln1->units = dim; ln1->epsilon = 1e-5;
        ln1->w = make_tensor_f32(1, dim, rms_att);
        free(rms_att);

        /* Multi-head attention */
        Layer* attn = &m->layers[layer_idx++];
        attn->type = LAYER_ATTENTION;
        attn->units = dim; attn->num_heads = n_heads;
        attn->embed_dim = dim; attn->max_seq_len = seq_len;
        attn->vocab_size = (int)n_kv_heads;
        /* Store Q, K, V, O weights concatenated. We split at runtime.
           Q: [dim, dim], K: [dim, dim], V: [dim, dim], O: [dim, dim] */
        attn->w = make_tensor_f32(dim * 4, dim, nullptr);
        if (attn->w) {
            memcpy(attn->w->data_f32, wq, (size_t)(dim * dim) * sizeof(float));
            memcpy(attn->w->data_f32 + dim * dim, wk, (size_t)(dim * dim) * sizeof(float));
            memcpy(attn->w->data_f32 + 2 * dim * dim, wv, (size_t)(dim * dim) * sizeof(float));
            memcpy(attn->w->data_f32 + 3 * dim * dim, wo, (size_t)(dim * dim) * sizeof(float));
        }
        int64_t sh_attn_b[] = { dim * 4 };
        attn->b = aurora_tensor_new_f32(1, sh_attn_b);
        if (attn->b) memset(attn->b->data_f32, 0, (size_t)(dim * 4) * sizeof(float));
        free(wq); free(wk); free(wv); free(wo);

        /* Pre-FFN RMSNorm */
        Layer* ln2 = &m->layers[layer_idx++];
        ln2->type = LAYER_LAYERNORM; ln2->units = dim; ln2->epsilon = 1e-5;
        ln2->w = make_tensor_f32(1, dim, rms_ffn);
        free(rms_ffn);

        /* SwiGLU FFN: w1=gate, w3=up (both [hidden_dim, dim]), w2=down ([dim, hidden_dim]) */
        Layer* ffn = &m->layers[layer_idx++];
        ffn->type = LAYER_SWIGLU;
        ffn->units = dim; ffn->embed_dim = hidden_dim;
        ffn->w = make_tensor_f32(hidden_dim, dim, w1);       /* gate */
        ffn->hc_w = make_tensor_f32(hidden_dim, dim, w3);    /* up */
        ffn->hc_b = make_tensor_f32(dim, hidden_dim, w2);    /* down */
        free(w1); free(w2); free(w3);
    }

    /* Final RMSNorm */
    Layer* fn = &m->layers[layer_idx++];
    fn->type = LAYER_LAYERNORM; fn->units = dim; fn->epsilon = 1e-5;
    fn->w = make_tensor_f32(1, dim, rms_final);
    free(rms_final);

    /* Output (unembed) — tied weights with embedding */
    float* output_w = read_weights(f, vocab_size * dim);
    Layer* out = &m->layers[layer_idx++];
    out->type = LAYER_UNEMBED;

    m->n_layers = layer_idx;
    m->total_params = 0;
    for (int i = 0; i < m->n_layers; i++) {
        if (m->layers[i].w) m->total_params += m->layers[i].w->total_size;
        if (m->layers[i].b) m->total_params += m->layers[i].b->total_size;
        if (m->layers[i].hc_w) m->total_params += m->layers[i].hc_w->total_size;
        if (m->layers[i].hc_b) m->total_params += m->layers[i].hc_b->total_size;
    }

    fclose(f);
    return (int64_t)m;
}

int64_t loader_get_config(int64_t idx) {
    if (idx < 0 || idx >= 7) return 0;
    return loader_config[idx];
}

} /* extern "C" */
