#include "runtime/ai/ai_common.h"
#include "runtime/ai/ai_distributed.h"
#include "runtime/ai/ai_paged_attention.h"

extern "C" {

/* === Model Management === */
int64_t model_create(const void* type_a) {
    const char* type = aurora_str_ptr(type_a);
    Model* m = (Model*)calloc(1, sizeof(Model));
    if (!m) return 0;
    m->model_type = (type && strcmp(type, "linear") == 0) ? 1 : MODEL_TYPE_SEQ;
    m->loss_type = LOSS_MSE; m->optimizer_type = OPTIM_ADAM;
    m->learning_rate = 0.001; m->batch_size = 32; m->epochs = 10;
    m->verbose = 1; m->early_stop_patience = 10;
    m->validation_split = 0.2; m->best_val_loss = 1e18;
    m->moe_aux_weight = 0.01;
    m->dtype = TENSOR_F32; /* Default to FP32 for 2x memory savings and faster matmul */
    return (int64_t)m;
}

int write_tensor_save(FILE* f, AuroraTensor* t) {
    if (!t) { int64_t zero = 0; fwrite(&zero, sizeof(int64_t), 1, f); return 0; }
    fwrite(&t->ndim, sizeof(int64_t), 1, f);
    fwrite(t->shape, sizeof(int64_t), t->ndim, f);
    fwrite(&t->total_size, sizeof(int64_t), 1, f);
    fwrite(t->data, sizeof(double), t->total_size, f);
    return 1;
}

AuroraTensor* read_tensor_load(FILE* f) {
    int64_t nd;
    if (fread(&nd, sizeof(int64_t), 1, f) != 1) return nullptr;
    if (nd <= 0) return nullptr;
    int64_t* sh = (int64_t*)malloc((size_t)nd * sizeof(int64_t));
    if (!sh) return nullptr;
    if (fread(sh, sizeof(int64_t), nd, f) != (size_t)nd) { free(sh); return nullptr; }
    int64_t sz;
    if (fread(&sz, sizeof(int64_t), 1, f) != 1) { free(sh); return nullptr; }
    AuroraTensor* t = aurora_tensor_new(nd, sh);
    free(sh);
    if (!t) return nullptr;
    if (sz > 0) {
        if (fread(t->data, sizeof(double), sz, f) != (size_t)sz) { aurora_tensor_free(t); return nullptr; }
    }
    return t;
}

int64_t model_load(const void* path_a) {
    const char* path = aurora_str_ptr(path_a);
    if (!path) return 0;
    FILE* f = fopen(path, "rb"); if (!f) return 0;
    char hdr[16] = {0};
    if (fread(hdr, 1, 10, f) != 10 || memcmp(hdr, "MRC_MODEL\n", 10) != 0) { fclose(f); return 0; }
    Model* m = (Model*)calloc(1, sizeof(Model));
    if (!m) { fclose(f); return 0; }
    int64_t nl;
    if (fread(&nl, sizeof(int64_t), 1, f) != 1) { free(m); fclose(f); return 0; }
    m->n_layers = (int)nl;
    m->loss_type = LOSS_MSE; m->optimizer_type = OPTIM_ADAM;
    m->learning_rate = 0.001; m->batch_size = 32; m->epochs = 10;
    m->verbose = 1; m->early_stop_patience = 10;
    m->validation_split = 0.2; m->best_val_loss = 1e18;
    for (int i = 0; i < m->n_layers && i < 128; i++) {
        Layer* l = &m->layers[i];
        memset(l, 0, sizeof(Layer));
        int64_t type; int64_t units; int64_t act; int64_t ksize; int64_t stride;
        if (fread(&type, sizeof(int64_t), 1, f) != 1) break;
        if (fread(&units, sizeof(int64_t), 1, f) != 1) break;
        if (fread(&act, sizeof(int64_t), 1, f) != 1) break;
        if (fread(&ksize, sizeof(int64_t), 1, f) != 1) break;
        if (fread(&stride, sizeof(int64_t), 1, f) != 1) break;
        l->type = (int)type; l->units = units; l->activation = (int)act;
        l->kernel_size = ksize; l->stride = stride; l->epsilon = 1e-8;
        l->w = read_tensor_load(f);
        l->b = read_tensor_load(f);
        l->hc_w = read_tensor_load(f);
        l->hc_b = read_tensor_load(f);
    }
    m->total_params = 0;
    for (int i = 0; i < m->n_layers; i++) {
        if (m->layers[i].w) m->total_params += m->layers[i].w->total_size;
        if (m->layers[i].b) m->total_params += m->layers[i].b->total_size;
        if (m->layers[i].hc_w) m->total_params += m->layers[i].hc_w->total_size;
    }
    fclose(f); return (int64_t)m;
}

int64_t model_save(int64_t model_ptr, const void* path_a) {
    Model* m = (Model*)model_ptr; const char* path = aurora_str_ptr(path_a);
    if (!m || !path) return 0;
    FILE* f = fopen(path, "wb"); if (!f) return 0;
    fwrite("MRC_MODEL\n", 1, 10, f);
    int64_t nl = m->n_layers;
    fwrite(&nl, sizeof(int64_t), 1, f);
    for (int i = 0; i < m->n_layers && i < 128; i++) {
        Layer* l = &m->layers[i];
        int64_t type = l->type; int64_t units = l->units; int64_t act = l->activation;
        int64_t ksize = l->kernel_size; int64_t stride = l->stride;
        fwrite(&type, sizeof(int64_t), 1, f);
        fwrite(&units, sizeof(int64_t), 1, f);
        fwrite(&act, sizeof(int64_t), 1, f);
        fwrite(&ksize, sizeof(int64_t), 1, f);
        fwrite(&stride, sizeof(int64_t), 1, f);
        write_tensor_save(f, l->w);
        write_tensor_save(f, l->b);
        write_tensor_save(f, l->hc_w);
        write_tensor_save(f, l->hc_b);
    }
    fclose(f); return 1;
}

int64_t model_copy(int64_t model_ptr) {
    Model* s = (Model*)model_ptr; if (!s) return 0;
    Model* d = (Model*)calloc(1, sizeof(Model)); if (!d) return 0;
    memcpy(d, s, sizeof(Model));
    for (int i = 0; i < s->n_layers && i < 128; i++) {
        auto tc = [](AuroraTensor* t) -> AuroraTensor* {
            if (!t) return nullptr;
            AuroraTensor* r = aurora_tensor_new(t->ndim, t->shape);
            memcpy(r->data, t->data, (size_t)t->total_size * sizeof(double));
            return r;
        };
        d->layers[i].w = tc(s->layers[i].w); d->layers[i].b = tc(s->layers[i].b);
        d->layers[i].hc_w = tc(s->layers[i].hc_w); d->layers[i].hc_b = tc(s->layers[i].hc_b);
        d->layers[i].dw = nullptr; d->layers[i].db = nullptr; d->layers[i].cache = nullptr;
        d->layers[i].hc_c = nullptr; d->layers[i].hc_d = nullptr;
        d->layers[i].flash_m = nullptr; d->layers[i].flash_d = nullptr;
        if (s->layers[i].q_data) {
            d->layers[i].q_data = (int8_t*)malloc((size_t)s->layers[i].q_size);
            memcpy(d->layers[i].q_data, s->layers[i].q_data, (size_t)s->layers[i].q_size);
            d->layers[i].q_size = s->layers[i].q_size;
            d->layers[i].q_scale = s->layers[i].q_scale;
            d->layers[i].q_zero = s->layers[i].q_zero;
        }
    }
    return (int64_t)d;
}


AuroraTensor* model_forward(Model* m, AuroraTensor* input, int training) {
    AuroraTensor* cur = aurora_tensor_new(input->ndim, input->shape);
    memcpy(cur->data, input->data, (size_t)input->total_size * sizeof(double));
    /* Reset per-forward accumulators */
    m->moe_aux_loss = 0.0;
    m->n_checkpoints = 0;
    /* Set active paged manager for mha_forward during inference */
    if (m->paged_mgr) g_active_paged_mgr = (PagedManager*)m->paged_mgr;
    /* gradient checkpointing enforces training mode for cache control */
    int do_ckpt = (training && m->checkpoint_freq > 0);
    for (int i = 0; i < m->n_layers; i++) {
        Layer* l = &m->layers[i];
        /* PP: skip layers not assigned to this stage */
        if (m->parallel_strategy == PARALLEL_PP && (i < (int)m->pp_start || i >= (int)m->pp_end)) continue;
        /* Save checkpoint activation every checkpoint_freq layers */
        if (do_ckpt && i > 0 && (i % m->checkpoint_freq == 0) && m->n_checkpoints < 128) {
            int ci = m->n_checkpoints++;
            m->checkpoint_layers[ci] = i;
            m->checkpoint_acts[ci] = aurora_tensor_new(cur->ndim, cur->shape);
            memcpy(m->checkpoint_acts[ci]->data, cur->data, (size_t)cur->total_size * sizeof(double));
        }
        AuroraTensor* nxt = nullptr;
        switch (l->type) {
            case LAYER_DENSE: nxt = dense_forward(l, cur, training);
                if (m->parallel_strategy == PARALLEL_TP && nxt) tp_allreduce_output(nxt->data, nxt->total_size);
                break;
            case LAYER_CONV: nxt = conv2d_forward(l, cur, training); break;
            case LAYER_LSTM: nxt = lstm_forward(l, cur, training); break;
            case LAYER_GRU: nxt = gru_forward(l, cur, training); break;
            case LAYER_FLATTEN: {
                int64_t batch = cur->shape[0];
                int64_t flat = 1;
                for (int64_t d = 1; d < cur->ndim; d++) flat *= cur->shape[d];
                int64_t fs[2] = { batch, flat };
                nxt = aurora_tensor_new(2, fs);
                int64_t per = flat;
                for (int64_t b = 0; b < batch; b++)
                    for (int64_t j = 0; j < per; j++)
                        nxt->data[b * per + j] = cur->data[b * per + j];
                break;
            }
            case LAYER_DROPOUT:
                nxt = aurora_tensor_new(cur->ndim, cur->shape);
                if (training && l->dropout_rate > 0.0) {
                    double scale = 1.0 / (1.0 - l->dropout_rate);
                    for (int64_t j = 0; j < cur->total_size; j++)
                        nxt->data[j] = (double)rand() / RAND_MAX < l->dropout_rate ? 0.0 : cur->data[j] * scale;
                } else memcpy(nxt->data, cur->data, (size_t)cur->total_size * sizeof(double));
                break;
            case LAYER_ATTENTION: nxt = mha_forward(l, cur, training);
                if (m->parallel_strategy == PARALLEL_TP && nxt) tp_allreduce_output(nxt->data, nxt->total_size);
                /* With gradient checkpointing, free per-layer caches — backward will recompute */
                if (do_ckpt) { aurora_tensor_free(l->cache); l->cache = nullptr; aurora_tensor_free(l->kv_cache_k); l->kv_cache_k = nullptr; l->flash_m = nullptr; l->flash_d = nullptr; }
                break;
            case LAYER_LAYERNORM: nxt = layernorm_forward(l, cur, training); break;
            case LAYER_EMBEDDING: nxt = embedding_forward(l, cur, training); break;
            case LAYER_POS_ENCODING: nxt = pos_encoding_forward(l, cur, training); break;
            case LAYER_ROPE: nxt = rope_forward(l, cur, training); break;
            case LAYER_MUL: nxt = mul_forward(l, cur, training); break;
            case LAYER_MOE: nxt = moe_forward(l, cur, training);
                if (training) m->moe_aux_loss += l->aux_loss;
                if (m->parallel_strategy == PARALLEL_TP && nxt) tp_allreduce_output(nxt->data, nxt->total_size);
                break;
            case LAYER_SWIGLU: nxt = swiglu_forward(l, cur, training);
                if (m->parallel_strategy == PARALLEL_TP && nxt) tp_allreduce_output(nxt->data, nxt->total_size);
                if (do_ckpt) { aurora_tensor_free(l->cache); l->cache = nullptr; aurora_tensor_free(l->kv_cache_k); l->kv_cache_k = nullptr; }
                break;
            case LAYER_QUANT: nxt = aurora_tensor_new(cur->ndim, cur->shape);
                memcpy(nxt->data, cur->data, (size_t)cur->total_size * sizeof(double)); break;
            case LAYER_UNEMBED: nxt = unembed_forward(l, cur, training); break;
            case LAYER_RESIDUAL_SAVE:
                if (l->residual) aurora_tensor_free(l->residual);
                l->residual = aurora_tensor_new(cur->ndim, cur->shape);
                memcpy(l->residual->data, cur->data, (size_t)cur->total_size * sizeof(double));
                nxt = aurora_tensor_new(cur->ndim, cur->shape);
                memcpy(nxt->data, cur->data, (size_t)cur->total_size * sizeof(double));
                break;
            case LAYER_RESIDUAL_ADD: {
                Layer* save = &m->layers[l->residual_layer];
                if (save->residual) {
                    for (int64_t j = 0; j < cur->total_size && j < save->residual->total_size; j++)
                        cur->data[j] += save->residual->data[j];
                }
                nxt = cur; cur = nullptr;
                break;
            }
            case LAYER_BATCHNORM: {
                nxt = aurora_tensor_new(cur->ndim, cur->shape);
                memcpy(nxt->data, cur->data, (size_t)cur->total_size * sizeof(double));
                int64_t nf = cur->shape[1];
                for (int64_t f = 0; f < nf; f++) {
                    double mn = 0.0, vr = 0.0;
                    for (int64_t b = 0; b < cur->shape[0]; b++) mn += cur->data[b * nf + f];
                    mn /= (double)cur->shape[0];
                    for (int64_t b = 0; b < cur->shape[0]; b++) { double d = cur->data[b * nf + f] - mn; vr += d * d; }
                    vr /= (double)cur->shape[0];
                    if (training) {
                        if (!l->running_mean) { l->running_mean = aurora_tensor_new(1, &nf); l->running_var = aurora_tensor_new(1, &nf); }
                        l->running_mean->data[f] = l->momentum * l->running_mean->data[f] + (1.0 - l->momentum) * mn;
                        l->running_var->data[f] = l->momentum * l->running_var->data[f] + (1.0 - l->momentum) * vr;
                    } else {
                        if (l->running_mean) mn = l->running_mean->data[f];
                        if (l->running_var) vr = l->running_var->data[f];
                    }
                    double den = sqrt(vr + l->epsilon);
                    for (int64_t b = 0; b < cur->shape[0]; b++)
                        nxt->data[b * nf + f] = (cur->data[b * nf + f] - mn) / den;
                }
                break;
            }
            default:
                nxt = aurora_tensor_new(cur->ndim, cur->shape);
                memcpy(nxt->data, cur->data, (size_t)cur->total_size * sizeof(double));
                break;
        }
        if (cur != input) aurora_tensor_free(cur);
        cur = nxt;
        if (!cur) return nullptr;
    }
    return cur;
}


} /* extern "C" */

AuroraTensor* forward_pass(Model* m, AuroraTensor* input) { return model_forward(m, input, 0); }

int64_t* g_vstore = nullptr; int64_t g_vcnt = 0, g_vcap = 0;

extern "C" {
#ifdef _WIN32
static int gpu_check_nvidia() {
    HMODULE h = LoadLibraryA("nvcuda.dll");
    if (!h) return 0;
    typedef int (*cuInit_t)(int);
    cuInit_t cuInit = (cuInit_t)GetProcAddress(h, "cuInit");
    int r = cuInit ? cuInit(0) : -1;
    FreeLibrary(h);
    return r == 0;
}
static int gpu_check_amd() {
    HMODULE h = LoadLibraryA("amdhip64.dll");
    if (!h) return 0;
    typedef int (*hipInit_t)(int);
    hipInit_t hipInit = (hipInit_t)GetProcAddress(h, "hipInit");
    int r = hipInit ? hipInit(0) : -1;
    FreeLibrary(h);
    return r == 0;
}
static int gpu_available() {
    return gpu_check_nvidia() || gpu_check_amd();
}
static int64_t gpu_memory_mb() {
    /* Try NVIDIA CUDA first */
    HMODULE h = LoadLibraryA("nvcuda.dll");
    if (h) {
        typedef int (*cuInit_t)(int);
        typedef int (*cuMemGetInfo_t)(size_t*, size_t*);
        cuInit_t cuInit = (cuInit_t)GetProcAddress(h, "cuInit");
        cuMemGetInfo_t cuMemGetInfo = (cuMemGetInfo_t)GetProcAddress(h, "cuMemGetInfo_v2");
        if (cuInit && cuMemGetInfo && cuInit(0) == 0) {
            size_t free_bytes = 0, total_bytes = 0;
            int r = cuMemGetInfo(&free_bytes, &total_bytes);
            FreeLibrary(h);
            return r == 0 ? (int64_t)(free_bytes / (1024 * 1024)) : 0;
        }
        FreeLibrary(h);
    }
    /* Try AMD ROCm */
    h = LoadLibraryA("amdhip64.dll");
    if (h) {
        typedef int (*hipInit_t)(int);
        typedef int (*hipMemGetInfo_t)(size_t*, size_t*);
        hipInit_t hipInit = (hipInit_t)GetProcAddress(h, "hipInit");
        hipMemGetInfo_t hipMemGetInfo = (hipMemGetInfo_t)GetProcAddress(h, "hipMemGetInfo");
        if (hipInit && hipMemGetInfo && hipInit(0) == 0) {
            size_t free_bytes = 0, total_bytes = 0;
            int r = hipMemGetInfo(&free_bytes, &total_bytes);
            FreeLibrary(h);
            return r == 0 ? (int64_t)(free_bytes / (1024 * 1024)) : 0;
        }
        FreeLibrary(h);
    }
    return 0;
}
#else
static int gpu_check_nvidia() { return 0; }
static int gpu_check_amd() { return 0; }
static int gpu_available() { return 0; }
static int64_t gpu_memory_mb() { return 0; }
#endif

int64_t gpu() {
    if (gpu_check_nvidia()) return 1;
    if (gpu_check_amd()) return 2; /* 2 = AMD */
    return 0;
}
int64_t gpu_mem() { return gpu_memory_mb(); }
char* device() {
    char buf[128];
    if (gpu_check_nvidia())
        snprintf(buf, sizeof(buf), "nvidia gpu");
    else if (gpu_check_amd())
        snprintf(buf, sizeof(buf), "amd gpu");
#ifdef __AVX2__
    else
        snprintf(buf, sizeof(buf), "cpu (AVX2)");
#else
    else
        snprintf(buf, sizeof(buf), "cpu");
#endif
    return AURORA_STRDUP(buf);
}

char* model_info(int64_t model_ptr) {
    Model* m = (Model*)model_ptr; if (!m) return AURORA_STRDUP("no model");
    char buf[1024]; int64_t pos = 0;
    const char* ls = m->loss_type == 0 ? "mse" : (m->loss_type == 2 ? "binary_crossentropy" : (m->loss_type == 3 ? "categorical_crossentropy" : "cross_entropy"));
    const char* os = m->optimizer_type == 0 ? "sgd" : (m->optimizer_type == 1 ? "adam" : "rmsprop");
    pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "Model: %d layers, %lld params | loss=%s | opt=%s | lr=%.6f | batch=%lld | epochs=%lld",
        m->n_layers, (long long)m->total_params, ls, os, m->learning_rate, (long long)m->batch_size, (long long)m->epochs);
    for (int i = 0; i < m->n_layers && i < 20; i++) {
        const char* tn = m->layers[i].type == 0 ? "Dense" : (m->layers[i].type == 1 ? "Conv2D" : (m->layers[i].type == 2 ? "LSTM" : (m->layers[i].type == 3 ? "GRU" : (m->layers[i].type == 8 ? "LayerNorm" : (m->layers[i].type == 9 ? "Embedding" : (m->layers[i].type == 10 ? "PosEnc" : (m->layers[i].type == 6 ? "MHA" : (m->layers[i].type == 11 ? "Unembed" : "Layer"))))))));
        const char* an = m->layers[i].activation == 0 ? "Linear" : (m->layers[i].activation == 1 ? "ReLU" : (m->layers[i].activation == 2 ? "Sigmoid" : (m->layers[i].activation == 3 ? "Tanh" : (m->layers[i].activation == 4 ? "LeakyReLU" : "Softmax"))));
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "\n  [%d] %s(%lld) act=%s", i, tn, (long long)m->layers[i].units, an);
    }
    if (m->last_loss > 0.0 || m->last_accuracy > 0.0)
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "\n  last_loss=%.6f last_acc=%.4f", m->last_loss, m->last_accuracy);
    return AURORA_STRDUP(buf);
}

int64_t model_size(int64_t model_ptr) { Model* m = (Model*)model_ptr; return m ? m->total_params : 0; }
double model_accuracy(int64_t model_ptr) { Model* m = (Model*)model_ptr; return m ? m->last_accuracy : 0.0; }
double model_loss(int64_t model_ptr) { Model* m = (Model*)model_ptr; return m ? m->last_loss : 0.0; }

int64_t bench_model(int64_t model_ptr) {
    Model* m = (Model*)model_ptr; if (!m || m->n_layers == 0) return 0;
    int64_t nf = m->layers[0].w ? m->layers[0].w->shape[0] : 1;
    int64_t sh[2] = { 1, nf }; AuroraTensor* in = aurora_tensor_new(2, sh);
    for (int64_t i = 0; i < in->total_size; i++) in->data[i] = 0.5;
#ifdef _WIN32
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);
    for (int64_t iter = 0; iter < 500000; iter++) { AuroraTensor* o = forward_pass(m, in); if (o) aurora_tensor_free(o); }
    QueryPerformanceCounter(&t1);
    double sec = (double)(t1.QuadPart - t0.QuadPart) / (double)freq.QuadPart;
    aurora_tensor_free(in);
    return sec > 0.0 ? (int64_t)(500000.0 / sec) : 0;
#else
    int64_t t0 = aurora_time();
    for (int64_t iter = 0; iter < 50000000; iter++) { AuroraTensor* o = forward_pass(m, in); if (o) aurora_tensor_free(o); }
    int64_t t1 = aurora_time(); aurora_tensor_free(in);
    return t1 - t0 > 0 ? (int64_t)(50000000.0 / (double)(t1 - t0)) : 0;
#endif
}

char* model_metrics(int64_t model_ptr, int64_t data_ptr) {
    Model* m = (Model*)model_ptr; AuroraTensor* d = (AuroraTensor*)data_ptr;
    if (!m || !d) return AURORA_STRDUP("{}");
    int64_t nf = d->shape[1] - 1, nr = d->shape[0], tp = 0, fp = 0, fn = 0, correct = 0;
    for (int64_t r = 0; r < nr; r++) {
        int64_t is[2] = { 1, nf }; AuroraTensor* in = aurora_tensor_new(2, is);
        memcpy(in->data, &d->data[r * d->shape[1]], (size_t)nf * sizeof(double));
        double tgt = d->data[r * d->shape[1] + nf];
        AuroraTensor* out = model_forward(m, in, 0); double pred = out ? out->data[0] : 0.0;
        if (m->loss_type == LOSS_BINARY_CROSS_ENTROPY) pred = pred > 0.5 ? 1.0 : 0.0;
        if (fabs(pred - tgt) < 0.5) correct++;
        if (pred > 0.5 && tgt > 0.5) tp++; else if (pred > 0.5 && tgt < 0.5) fp++; else if (pred < 0.5 && tgt > 0.5) fn++;
        aurora_tensor_free(in); if (out) aurora_tensor_free(out);
    }
    double acc = (double)correct / (double)nr, pr = (tp + fp) > 0 ? (double)tp / (double)(tp + fp) : 0.0;
    double re = (tp + fn) > 0 ? (double)tp / (double)(tp + fn) : 0.0, f1 = (pr + re) > 0 ? 2.0 * pr * re / (pr + re) : 0.0;
    char buf[512]; snprintf(buf, sizeof(buf), "{\"accuracy\":%.4f,\"precision\":%.4f,\"recall\":%.4f,\"f1\":%.4f,\"samples\":%lld}", acc, pr, re, f1, (long long)nr);
    return AURORA_STRDUP(buf);
}

/* === Model Ops === */
int64_t compile(int64_t model_ptr) {
    Model* m = (Model*)model_ptr; if (!m) return 0;
    m->total_params = 0;
    for (int i = 0; i < m->n_layers; i++) { if (m->layers[i].w) m->total_params += m->layers[i].w->total_size; if (m->layers[i].b) m->total_params += m->layers[i].b->total_size; }
    return 1;
}
int64_t onnx(int64_t model_ptr, const void* path_a) {
    Model* m = (Model*)model_ptr;
    const char* path = aurora_str_ptr(path_a);
    if (!m || !path) return 0;
    FILE* f = fopen(path, "w");
    if (!f) return 0;

    /* ONNX-style JSON export */
    fprintf(f, "{\n  \"ir_version\": 7,\n  \"producer_name\": \"Aurora/MrCode\",\n");
    fprintf(f, "  \"graph\": {\n    \"name\": \"model\",\n    \"node\": [\n");
    fprintf(f, "      {\"input\": [\"input\"], \"output\": [\"l0\"], \"op_type\": \"Identity\", \"name\": \"input\"},\n");
    for (int i = 0; i < m->n_layers; i++) {
        Layer* l = &m->layers[i];
        const char* op = "Identity";
        if (l->type == LAYER_DENSE) op = "Gemm";
        else if (l->type == LAYER_CONV) op = "Conv";
        else if (l->type == LAYER_LSTM) op = "LSTM";
        else if (l->type == LAYER_GRU) op = "GRU";
        else if (l->type == LAYER_ATTENTION) op = "Attention";
        else if (l->type == LAYER_LAYERNORM) op = "LayerNormalization";
        else if (l->type == LAYER_EMBEDDING) op = "Gather";
        else if (l->type == LAYER_POS_ENCODING) op = "Add";
        else if (l->type == LAYER_ROPE) op = "RoPE";
        else if (l->type == LAYER_MOE) op = "MoE";
        else if (l->type == LAYER_UNEMBED) op = "MatMul";
        fprintf(f, "      {\"input\": [\"l%d\"], \"output\": [\"l%d\"], \"op_type\": \"%s\", \"name\": \"layer_%d\"}%s\n",
            i, i + 1, op, i, (i < m->n_layers - 1) ? "," : "");
    }
    fprintf(f, "    ],\n    \"initializer\": [\n");
    int init_count = 0;
    for (int i = 0; i < m->n_layers; i++) {
        Layer* l = &m->layers[i];
        AuroraTensor* tensors[] = { l->w, l->b, l->hc_w, l->hc_b, l->running_mean, l->running_var };
        const char* names[] = { "w", "b", "hc_w", "hc_b", "rm", "rv" };
        for (int t = 0; t < 6; t++) {
            if (!tensors[t]) continue;
            fprintf(f, "      {\"name\": \"l%d_%s\", \"dims\": [", i, names[t]);
            for (int d = 0; d < tensors[t]->ndim; d++)
                fprintf(f, "%lld%s", (long long)tensors[t]->shape[d], (d < tensors[t]->ndim - 1) ? "," : "");
            fprintf(f, "], \"data_type\": 1}%s\n", (init_count < 100) ? "," : "");
            init_count++;
        }
    }
    fprintf(f, "    ],\n    \"input\": [{\"name\": \"input\", \"type\": {\"tensor_type\": {\"elem_type\": 1, \"shape\": [\"batch\", \"features\"]}}}],\n");
    fprintf(f, "    \"output\": [{\"name\": \"output\", \"type\": {\"tensor_type\": {\"elem_type\": 1, \"shape\": [\"batch\", \"out\"]}}}]\n");
    fprintf(f, "  }\n}\n");
    fclose(f);
    return 1;
}

int64_t set_dtype(int64_t model_ptr, const void* dtype_a) {
    Model* m = (Model*)model_ptr; if (!m) return 0;
    const char* dtype = aurora_str_ptr(dtype_a);
    if (!dtype) return 0;
    if (strcmp(dtype, "f32") == 0 || strcmp(dtype, "float32") == 0) {
        m->dtype = TENSOR_F32;
        for (int i = 0; i < m->n_layers; i++) m->layers[i].dtype = TENSOR_F32;
    } else if (strcmp(dtype, "f64") == 0 || strcmp(dtype, "float64") == 0) {
        m->dtype = TENSOR_F64;
        for (int i = 0; i < m->n_layers; i++) m->layers[i].dtype = TENSOR_F64;
    } else {
        return 0;
    }
    return 1;
}

} /* extern "C" */
