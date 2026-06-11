#include "runtime/ai/ai_common.h"
#include "runtime/ai/ai_paged_attention.h"

/* Global active paged manager — set by model_forward during inference */
PagedManager* g_active_paged_mgr = nullptr;

extern "C" {

/* ── Helper: create a tensor respecting layer dtype ── */
static inline AuroraTensor* l_tensor(Layer* l, int64_t ndim, int64_t* shape) {
    return (l->dtype == TENSOR_F32) ? aurora_tensor_new_f32(ndim, shape) : aurora_tensor_new(ndim, shape);
}

/* ── Helper: create a gradient tensor (always F64 for precise gradients) ── */
static inline AuroraTensor* l_grad_tensor(Layer* l, int64_t ndim, int64_t* shape) {
    (void)l; return aurora_tensor_new(ndim, shape);
}

/* === Activation Functions === */
void activation_forward(AuroraTensor* t, int act) {
    if (!t) return;
    int64_t n = t->total_size;
    int is_f32 = (t->dtype == TENSOR_F32);
    switch (act) {
        case ACT_RELU:
            if (is_f32) {
                for (int64_t i = 0; i < n; i++)
                    if (t->data_f32[i] < 0.0f) t->data_f32[i] = 0.0f;
            } else {
                for (int64_t i = 0; i < n; i++)
                    if (t->data[i] < 0.0) t->data[i] = 0.0;
            }
            break;
        case ACT_SIGMOID:
            if (is_f32) {
                for (int64_t i = 0; i < n; i++)
                    t->data_f32[i] = 1.0f / (1.0f + expf(-t->data_f32[i]));
            } else {
                for (int64_t i = 0; i < n; i++)
                    t->data[i] = 1.0 / (1.0 + exp(-t->data[i]));
            }
            break;
        case ACT_TANH:
            if (is_f32) {
                for (int64_t i = 0; i < n; i++)
                    t->data_f32[i] = tanhf(t->data_f32[i]);
            } else {
                for (int64_t i = 0; i < n; i++)
                    t->data[i] = tanh(t->data[i]);
            }
            break;
        case ACT_LEAKY_RELU:
            if (is_f32) {
                for (int64_t i = 0; i < n; i++)
                    if (t->data_f32[i] < 0.0f) t->data_f32[i] *= 0.01f;
            } else {
                for (int64_t i = 0; i < n; i++)
                    if (t->data[i] < 0.0) t->data[i] *= 0.01;
            }
            break;
        case ACT_SOFTMAX: {
            aurora_tensor_softmax(t);
            break;
        }
        case ACT_SILU:
            if (is_f32) {
                for (int64_t i = 0; i < n; i++) {
                    float x = t->data_f32[i];
                    t->data_f32[i] = x / (1.0f + expf(-x));
                }
            } else {
                for (int64_t i = 0; i < n; i++) {
                    double x = t->data[i];
                    t->data[i] = x / (1.0 + exp(-x));
                }
            }
            break;
        case ACT_GELU:
            if (is_f32) {
                for (int64_t i = 0; i < n; i++) {
                    float x = t->data_f32[i];
                    t->data_f32[i] = 0.5f * x * (1.0f + tanhf(0.7978845608f * (x + 0.044715f * x * x * x)));
                }
            } else {
                for (int64_t i = 0; i < n; i++) {
                    double x = t->data[i];
                    t->data[i] = 0.5 * x * (1.0 + tanh(0.7978845608 * (x + 0.044715 * x * x * x)));
                }
            }
            break;
        case ACT_LINEAR:
        default:
            break;
    }
}

void activation_backward(AuroraTensor* out, AuroraTensor* dout, int act) {
    if (!out || !dout) return;
    int64_t n = out->total_size;
    int is_f32 = (out->dtype == TENSOR_F32 && dout->dtype == TENSOR_F32);
    if (is_f32) {
        switch (act) {
            case ACT_RELU:
                for (int64_t i = 0; i < n; i++)
                    dout->data_f32[i] *= (out->data_f32[i] > 0.0f) ? 1.0f : 0.0f;
                break;
            case ACT_LEAKY_RELU:
                for (int64_t i = 0; i < n; i++)
                    dout->data_f32[i] *= (out->data_f32[i] > 0.0f) ? 1.0f : 0.01f;
                break;
            case ACT_SIGMOID:
                for (int64_t i = 0; i < n; i++) {
                    float v = out->data_f32[i];
                    dout->data_f32[i] *= v * (1.0f - v);
                }
                break;
            case ACT_TANH:
                for (int64_t i = 0; i < n; i++) {
                    float v = out->data_f32[i];
                    dout->data_f32[i] *= 1.0f - v * v;
                }
                break;
            case ACT_SILU: {
                for (int64_t i = 0; i < n; i++) {
                    float v = out->data_f32[i];
                    float sig = 1.0f / (1.0f + expf(-v));
                    dout->data_f32[i] *= sig * (1.0f + v * (1.0f - sig));
                }
                break;
            }
            case ACT_GELU: {
                for (int64_t i = 0; i < n; i++) {
                    float x = out->data_f32[i];
                    float tanh_in = 0.7978845608f * (x + 0.044715f * x * x * x);
                    float sech2 = 1.0f - tanhf(tanh_in) * tanhf(tanh_in);
                    float dgelu = 0.5f * (1.0f + tanhf(tanh_in)) + 0.5f * x * sech2 * 0.7978845608f * (1.0f + 3.0f * 0.044715f * x * x);
                    dout->data_f32[i] *= dgelu;
                }
                break;
            }
            default: break;
        }
    } else {
        for (int64_t i = 0; i < n; i++) {
            double v = out->data[i];
            switch (act) {
                case ACT_RELU: dout->data[i] *= (v > 0.0) ? 1.0 : 0.0; break;
                case ACT_LEAKY_RELU: dout->data[i] *= (v > 0.0) ? 1.0 : 0.01; break;
                case ACT_SIGMOID: dout->data[i] *= v * (1.0 - v); break;
                case ACT_TANH: dout->data[i] *= 1.0 - v * v; break;
                case ACT_SILU: {
                    double sig = 1.0 / (1.0 + exp(-v));
                    dout->data[i] *= sig * (1.0 + v * (1.0 - sig));
                    break;
                }
                case ACT_GELU: {
                    double x = v;
                    double tanh_in = 0.7978845608 * (x + 0.044715 * x * x * x);
                    double sech2 = 1.0 - tanh(tanh_in) * tanh(tanh_in);
                    double dgelu = 0.5 * (1.0 + tanh(tanh_in)) + 0.5 * x * sech2 * 0.7978845608 * (1.0 + 3.0 * 0.044715 * x * x);
                    dout->data[i] *= dgelu;
                    break;
                }
                default: break;
            }
        }
    }
}

/* === LayerNorm Layer === */
#define LN_EPS 1e-5
AuroraTensor* layernorm_forward(Layer* l, AuroraTensor* input, int training) {
    (void)training;
    if (!input) return nullptr;
    int64_t nf = input->shape[1], nr = input->shape[0];
    if (!l->w) {
        int64_t s[1] = { nf };
        l->w = l_tensor(l, 1, s);
        if (l->dtype == TENSOR_F32) {
            for (int64_t i = 0; i < nf; i++) l->w->data_f32[i] = 1.0f;
        } else {
            for (int64_t i = 0; i < nf; i++) l->w->data[i] = 1.0;
        }
        l->b = l_tensor(l, 1, s);
        memset(l->b->data_ptr, 0, (size_t)nf * ((l->dtype == TENSOR_F32) ? sizeof(float) : sizeof(double)));
        l->dw = nullptr; l->db = nullptr;
    }
    AuroraTensor* out = l_tensor(l, input->ndim, input->shape);
    if (l->cache) aurora_tensor_free(l->cache);
    l->cache = aurora_tensor_new(1, &nr);
    if (l->running_mean) aurora_tensor_free(l->running_mean);
    l->running_mean = aurora_tensor_new(1, &nr);
    if (l->dtype == TENSOR_F32) {
        for (int64_t r = 0; r < nr; r++) {
            float mn = 0.0f, vr = 0.0f;
            for (int64_t j = 0; j < nf; j++) mn += input->data_f32[r * nf + j];
            mn /= (float)nf;
            for (int64_t j = 0; j < nf; j++) { float d = input->data_f32[r * nf + j] - mn; vr += d * d; }
            vr /= (float)nf;
            float inv_std = 1.0f / sqrtf(vr + 1e-5f);
            l->cache->data_f32[r] = mn;
            l->running_mean->data_f32[r] = inv_std;
            for (int64_t j = 0; j < nf; j++)
                out->data_f32[r * nf + j] = (input->data_f32[r * nf + j] - mn) * inv_std
                    * l->w->data_f32[j] + l->b->data_f32[j];
        }
    } else {
        for (int64_t r = 0; r < nr; r++) {
            double mn = 0.0, vr = 0.0;
            for (int64_t j = 0; j < nf; j++) mn += input->data[r * nf + j];
            mn /= (double)nf;
            for (int64_t j = 0; j < nf; j++) { double d = input->data[r * nf + j] - mn; vr += d * d; }
            vr /= (double)nf;
            double inv_std = 1.0 / sqrt(vr + 1e-5);
            l->cache->data[r] = mn;
            l->running_mean->data[r] = inv_std;
            for (int64_t j = 0; j < nf; j++)
                out->data[r * nf + j] = (input->data[r * nf + j] - mn) * inv_std
                    * l->w->data[j] + l->b->data[j];
        }
    }
    return out;
}

AuroraTensor* layernorm_backward(Layer* l, AuroraTensor* dout, AuroraTensor* input) {
    if (!l->w || !dout || !input) return nullptr;
    if (!l->dw) { l->dw = aurora_tensor_new(1, l->w->shape); l->db = aurora_tensor_new(1, l->b->shape); }
    memset(l->dw->data, 0, (size_t)l->dw->total_size * sizeof(double));
    memset(l->db->data, 0, (size_t)l->db->total_size * sizeof(double));
    int64_t nf = input->shape[1], nr = input->shape[0];
    AuroraTensor* dx = aurora_tensor_new(input->ndim, input->shape);
    for (int64_t r = 0; r < nr; r++) {
        double mn = l->cache ? l->cache->data[r] : 0.0;
        double vr = l->running_mean ? l->running_mean->data[r] : 1.0;
        double den = 1.0 / sqrt(vr + LN_EPS);
        for (int64_t j = 0; j < nf; j++) {
            double x = input->data[r * nf + j];
            double d = dout->data[r * nf + j];
            double xnorm = (x - mn) * den;
            /* gamma grad */
            l->dw->data[j] += d * xnorm;
            /* beta grad */
            l->db->data[j] += d;
            /* dx: sum across features of gradient contributions */
            dx->data[r * nf + j] = d * l->w->data[j] * den;
            double sum_d = 0.0, sum_dx = 0.0;
            for (int64_t k = 0; k < nf; k++) {
                double xk = input->data[r * nf + k];
                double dk = dout->data[r * nf + k];
                sum_d += dk * l->w->data[k];
                sum_dx += dk * l->w->data[k] * (xk - mn);
            }
            dx->data[r * nf + j] -= (sum_d / (double)nf + (x - mn) * sum_dx / ((double)nf * (vr + LN_EPS)));
        }
    }
    for (int64_t j = 0; j < nf; j++) { l->dw->data[j] /= (double)nr; l->db->data[j] /= (double)nr; }
    return dx;
}

/* === Embedding Layer === */
AuroraTensor* embedding_forward(Layer* l, AuroraTensor* input, int training) {
    (void)training;
    if (!input || !l->w) return nullptr;
    int64_t batch = input->shape[0], seq = input->shape[1], d = l->w->shape[1];
    int64_t os[2] = { batch * seq, d };
    AuroraTensor* out = aurora_tensor_new(2, os);
    for (int64_t b = 0; b < batch; b++)
        for (int64_t s = 0; s < seq; s++) {
            int64_t tok = (int64_t)input->data[b * seq + s];
            if (tok < 0 || tok >= l->w->shape[0]) tok = 0;
            memcpy(&out->data[(b * seq + s) * d], &l->w->data[tok * d], (size_t)d * sizeof(double));
        }
    return out;
}

AuroraTensor* embedding_backward(Layer* l, AuroraTensor* dout, AuroraTensor* input) {
    if (!l->w || !dout || !input) return nullptr;
    if (!l->dw) { l->dw = aurora_tensor_new(l->w->ndim, l->w->shape); memset(l->dw->data, 0, (size_t)l->dw->total_size * sizeof(double)); }
    int64_t batch = input->shape[0], seq = input->shape[1], d = l->w->shape[1];
    for (int64_t b = 0; b < batch; b++)
        for (int64_t s = 0; s < seq; s++) {
            int64_t tok = (int64_t)input->data[b * seq + s];
            if (tok < 0 || tok >= l->w->shape[0]) continue;
            for (int64_t j = 0; j < d; j++)
                l->dw->data[tok * d + j] += dout->data[(b * seq + s) * d + j];
        }
    return nullptr;
}

/* === Positional Encoding Layer (sinusoidal, non-learned) === */
AuroraTensor* pos_encoding_forward(Layer* l, AuroraTensor* input, int training) {
    (void)training;
    if (!input) return nullptr;
    int64_t seq = input->shape[0] > 0 ? input->shape[0] : 1;
    int64_t d = input->shape[1];
    AuroraTensor* out = aurora_tensor_new(input->ndim, input->shape);
    memcpy(out->data, input->data, (size_t)input->total_size * sizeof(double));
    for (int64_t p = 0; p < seq; p++) {
        for (int64_t i = 0; i < d; i++) {
            double val = (i % 2 == 0) ? sin(p / pow(10000.0, (double)i / (double)d))
                                      : cos(p / pow(10000.0, (double)(i - 1) / (double)d));
            out->data[p * d + i] += val;
        }
    }
    return out;
}

AuroraTensor* pos_encoding_backward(Layer* l, AuroraTensor* dout, AuroraTensor* input) {
    (void)l; (void)input;
    if (!dout) return nullptr;
    AuroraTensor* dx = aurora_tensor_new(dout->ndim, dout->shape);
    memcpy(dx->data, dout->data, (size_t)dout->total_size * sizeof(double));
    return dx;
}

/* === RoPE (Rotary Position Embedding) Layer === */
static void apply_rope_to_tensor(double* data, int64_t seq, int64_t dim, double* out) {
    for (int64_t p = 0; p < seq; p++) {
        for (int64_t i = 0; i < dim; i += 2) {
            double theta = (double)p / pow(10000.0, (double)i / (double)dim);
            double cos_t = cos(theta);
            double sin_t = sin(theta);
            int64_t i2 = (i + 1 < dim) ? i + 1 : i;
            double x0 = data[p * dim + i];
            double x1 = data[p * dim + i2];
            out[p * dim + i] = x0 * cos_t - x1 * sin_t;
            if (i2 != i) out[p * dim + i2] = x0 * sin_t + x1 * cos_t;
        }
    }
}

AuroraTensor* rope_forward(Layer* l, AuroraTensor* input, int training) {
    (void)l; (void)training;
    if (!input || input->ndim < 2) return nullptr;
    int64_t seq = input->shape[0];
    int64_t dim = input->shape[1];
    int64_t total = input->total_size;
    AuroraTensor* out = aurora_tensor_new(input->ndim, input->shape);
    /* Handle batch: treat as flat [total, dim] or [batch * seq, dim] */
    if (input->ndim == 2) {
        apply_rope_to_tensor(input->data, seq, dim, out->data);
    } else if (input->ndim == 3) {
        int64_t batch = input->shape[0];
        int64_t seq_len = input->shape[1];
        int64_t d = input->shape[2];
        for (int64_t b = 0; b < batch; b++)
            apply_rope_to_tensor(input->data + b * seq_len * d, seq_len, d, out->data + b * seq_len * d);
    } else {
        memcpy(out->data, input->data, (size_t)total * sizeof(double));
    }
    return out;
}

AuroraTensor* rope_backward(Layer* l, AuroraTensor* dout, AuroraTensor* input) {
    (void)l; (void)input;
    if (!dout) return nullptr;
    /* RoPE is orthogonal rotation; gradient is rotated by same angle */
    AuroraTensor* dx = aurora_tensor_new(dout->ndim, dout->shape);
    int64_t seq = dout->shape[0];
    int64_t dim = dout->shape[1];
    if (dout->ndim == 2) {
        apply_rope_to_tensor(dout->data, seq, dim, dx->data);
    } else if (dout->ndim == 3) {
        int64_t batch = dout->shape[0];
        int64_t seq_len = dout->shape[1];
        int64_t d = dout->shape[2];
        for (int64_t b = 0; b < batch; b++)
            apply_rope_to_tensor(dout->data + b * seq_len * d, seq_len, d, dx->data + b * seq_len * d);
    } else {
        memcpy(dx->data, dout->data, (size_t)dout->total_size * sizeof(double));
    }
    return dx;
}

/* === Element-wise Multiply Layer (for SwiGLU gating) === */
AuroraTensor* mul_forward(Layer* l, AuroraTensor* input, int training) {
    (void)training;
    /* input[0] = first operand, input[1] = second operand (concatenated) */
    /* We cache the split point in l->units */
    if (!input) return nullptr;
    int64_t half = input->total_size / 2;
    AuroraTensor* out = aurora_tensor_new(input->ndim, input->shape);
    if (input->ndim == 2) {
        int64_t n = input->shape[0], d = input->shape[1] / 2;
        for (int64_t i = 0; i < n; i++) {
            for (int64_t j = 0; j < d; j++) {
                double a = input->data[i * 2 * d + j];
                double b = input->data[i * 2 * d + d + j];
                out->data[i * d + j] = a * b;
            }
        }
        out->shape[1] = d;
        out->total_size = n * d;
    } else {
        for (int64_t i = 0; i < half; i++)
            out->data[i] = input->data[i] * input->data[half + i];
        out->total_size = half;
    }
    if (training) {
        if (l->cache) aurora_tensor_free(l->cache);
        l->cache = aurora_tensor_new(input->ndim, input->shape);
        memcpy(l->cache->data, input->data, (size_t)input->total_size * sizeof(double));
    }
    return out;
}

AuroraTensor* mul_backward(Layer* l, AuroraTensor* dout, AuroraTensor* input) {
    (void)input;
    if (!dout || !l->cache) return nullptr;
    int64_t half = l->cache->total_size / 2;
    AuroraTensor* dx = aurora_tensor_new(l->cache->ndim, l->cache->shape);
    memcpy(dx->data, l->cache->data, (size_t)l->cache->total_size * sizeof(double));
    if (l->cache->ndim == 2) {
        int64_t n = l->cache->shape[0], d = l->cache->shape[1] / 2;
        for (int64_t i = 0; i < n; i++) {
            for (int64_t j = 0; j < d; j++) {
                double a = l->cache->data[i * 2 * d + j];
                double b = l->cache->data[i * 2 * d + d + j];
                double grad = dout->data[i * d + j];
                dx->data[i * 2 * d + j] = grad * b;
                dx->data[i * 2 * d + d + j] = grad * a;
            }
        }
    } else {
        for (int64_t i = 0; i < half; i++) {
            double a = l->cache->data[i];
            double b = l->cache->data[half + i];
            dx->data[i] = dout->data[i] * b;
            dx->data[half + i] = dout->data[i] * a;
        }
    }
    return dx;
}

/* === Multi-Head Self-Attention Layer === */
/*
 * ════════════════════════════════════════════════════════════
 * mha_forward — FlashAttention v2 (tiled Q + K/V, online softmax)
 *
 * Phases 2-5 integrated features:
 *  - Tiled forward: process Q in blocks of B_r, K/V in blocks of B_c
 *    Memory: O(B_r × d_h) per tile, NOT O(n²)
 *  - Causal masking built into tiling
 *  - dtype-aware (F32 weights via l_tensor, F32 matmul auto-dispatch)
 *  - Inference: PagedManager path with proper seq_id, causal mask
 *  - Inference: Legacy KV cache path with online softmax
 * ════════════════════════════════════════════════════════════
 */

#define FLASH_BLOCK 64 /* block size for tiling */

AuroraTensor* mha_forward(Layer* l, AuroraTensor* input, int training) {
    if (!input || input->ndim < 2) return nullptr;
    int64_t n = input->shape[0], d = input->shape[1];
    int64_t h = l->num_heads > 0 ? l->num_heads : 4;
    int64_t d_h = d / h;
    if (d_h < 1) d_h = 1;
    int dt = l->dtype;

    /* Lazy weight init */
    if (!l->w) {
        double scale = sqrt(2.0 / (double)(d + 3 * d));
        int64_t ws[2] = { d, 3 * d };
        l->w = l_tensor(l, 2, ws);
        if (dt == TENSOR_F32) {
            for (int64_t i = 0; i < l->w->total_size; i++) l->w->data_f32[i] = (float)(rand_uniform() * scale);
        } else {
            for (int64_t i = 0; i < l->w->total_size; i++) l->w->data[i] = rand_uniform() * scale;
        }
        int64_t bs[1] = { 3 * d };
        l->b = l_tensor(l, 1, bs);
        memset(l->b->data_ptr, 0, (size_t)3 * d * ((dt == TENSOR_F32) ? sizeof(float) : sizeof(double)));
        int64_t os[2] = { d, d };
        l->hc_w = l_tensor(l, 2, os);
        if (dt == TENSOR_F32) {
            for (int64_t i = 0; i < l->hc_w->total_size; i++) l->hc_w->data_f32[i] = (float)(rand_uniform() * scale);
        } else {
            for (int64_t i = 0; i < l->hc_w->total_size; i++) l->hc_w->data[i] = rand_uniform() * scale;
        }
        int64_t obs[1] = { d };
        l->hc_b = l_tensor(l, 1, obs);
        memset(l->hc_b->data_ptr, 0, (size_t)d * ((dt == TENSOR_F32) ? sizeof(float) : sizeof(double)));
    }

    /* QKV = input @ Wqkv + bias */
    AuroraTensor* qkv = aurora_tensor_matmul(input, l->w);
    if (!qkv) return nullptr;
    if (qkv->dtype == TENSOR_F32) {
        for (int64_t i = 0; i < qkv->total_size; i++)
            qkv->data_f32[i] += l->b->data_f32[i % (3 * d)];
    } else {
        for (int64_t i = 0; i < qkv->total_size; i++)
            qkv->data[i] += l->b->data[i % (3 * d)];
    }

    /* ════════════════════════════════════════════════════════
       INFERENCE PATH
       ════════════════════════════════════════════════════════ */
    if (!training) {
        int64_t new_n = n;

        /* ── PagedManager path (multi-sequence continuous batching) ── */
        if (g_active_paged_mgr) {
            PagedManager* pm = g_active_paged_mgr;
            int64_t seq_id = pm->current_seq_id;
            AuroraTensor* out = (dt == TENSOR_F32)
                ? aurora_tensor_new_f32(input->ndim, input->shape)
                : aurora_tensor_new(input->ndim, input->shape);
            if (!out) { aurora_tensor_free(qkv); return nullptr; }
            double scale = 1.0 / sqrt((double)d_h);
            /* Merge append+attention for correct causal masking: each token attends
               only to itself and previously appended tokens in the KV cache. */
            for (int64_t ri = 0; ri < new_n; ri++) {
                double* k_ptr = (qkv->dtype == TENSOR_F32)
                    ? (double*)(qkv->data_f32 + ri * 3 * d_h * h + d_h * h)
                    : qkv->data + ri * 3 * d + d;
                double* v_ptr = (qkv->dtype == TENSOR_F32)
                    ? (double*)(qkv->data_f32 + ri * 3 * d_h * h + 2 * d_h * h)
                    : qkv->data + ri * 3 * d + 2 * d;
                paged_cache_kv_for_layer(pm, seq_id, 0, k_ptr, v_ptr, h, d_h);
                double* q_ptr = (qkv->dtype == TENSOR_F32)
                    ? (double*)(qkv->data_f32 + ri * 3 * d_h * h)
                    : qkv->data + ri * 3 * d;
                double* o_ptr = (out->dtype == TENSOR_F32)
                    ? (double*)(out->data_f32 + ri * d)
                    : out->data + ri * d;
                paged_attention_forward(pm, seq_id, q_ptr, o_ptr, h, d_h, scale);
            }
            AuroraTensor* final = aurora_tensor_matmul(out, l->hc_w);
            if (final && final->dtype == TENSOR_F32) {
                for (int64_t i = 0; i < final->total_size; i++)
                    final->data_f32[i] += l->hc_b->data_f32[i % d];
            } else if (final) {
                for (int64_t i = 0; i < final->total_size; i++)
                    final->data[i] += l->hc_b->data[i % d];
            }
            aurora_tensor_free(qkv);
            aurora_tensor_free(out);
            return final;
        }

        /* ── Legacy per-layer KV cache path (single sequence) ── */
        int64_t cache_n = (l->kv_cache_k) ? l->kv_cache_k->shape[0] : 0;
        int64_t total_n = cache_n + new_n;

        int64_t cache_shape[2] = { total_n, d };
        AuroraTensor* new_k = l_tensor(l, 2, cache_shape);
        AuroraTensor* new_v = l_tensor(l, 2, cache_shape);
        if (!new_k || !new_v) { aurora_tensor_free(qkv); aurora_tensor_free(new_k); aurora_tensor_free(new_v); return nullptr; }

        if (l->kv_cache_k && l->kv_cache_v && cache_n > 0) {
            size_t sz = (size_t)cache_n * d * ((dt == TENSOR_F32) ? sizeof(float) : sizeof(double));
            memcpy(new_k->data_ptr, l->kv_cache_k->data_ptr, sz);
            memcpy(new_v->data_ptr, l->kv_cache_v->data_ptr, sz);
        }
        /* Copy new K,V */
        if (dt == TENSOR_F32) {
            for (int64_t i = 0; i < new_n; i++) {
                memcpy(new_k->data_f32 + (cache_n + i) * d, qkv->data_f32 + i * 3 * d + d, (size_t)d * sizeof(float));
                memcpy(new_v->data_f32 + (cache_n + i) * d, qkv->data_f32 + i * 3 * d + 2 * d, (size_t)d * sizeof(float));
            }
        } else {
            for (int64_t i = 0; i < new_n; i++) {
                memcpy(new_k->data + (cache_n + i) * d, qkv->data + i * 3 * d + d, (size_t)d * sizeof(double));
                memcpy(new_v->data + (cache_n + i) * d, qkv->data + i * 3 * d + 2 * d, (size_t)d * sizeof(double));
            }
        }
        aurora_tensor_free(l->kv_cache_k); l->kv_cache_k = new_k;
        aurora_tensor_free(l->kv_cache_v); l->kv_cache_v = new_v;
        l->cache_len = total_n;

        /* Per-head attention using cached K,V — online softmax with causal mask */
        AuroraTensor* out = l_tensor(l, input->ndim, input->shape);
        if (!out) { aurora_tensor_free(qkv); return nullptr; }

        double inv_scale_d = 1.0 / sqrt((double)d_h);
        float  inv_scale_f = 1.0f / sqrtf((float)d_h);

        for (int64_t hi = 0; hi < h; hi++) {
            int64_t bo = hi * d_h;
            for (int64_t ri = 0; ri < new_n; ri++) {
                if (dt == TENSOR_F32) {
                    float* q_h = qkv->data_f32 + (ri * 3 * d + bo);
                    float* o_h = out->data_f32 + ri * d + bo;
                    memset(o_h, 0, (size_t)d_h * sizeof(float));
                    float m_val = -1e18f, d_val = 0.0f;
                    for (int64_t ci = 0; ci < total_n; ci++) {
                        if (ci > cache_n + ri) break; /* causal */
                        float* k_h = l->kv_cache_k->data_f32 + ci * d + bo;
                        float* v_h = l->kv_cache_v->data_f32 + ci * d + bo;
                        float score = 0.0f;
                        for (int64_t di = 0; di < d_h; di++) score += q_h[di] * k_h[di];
                        score *= inv_scale_f;
                        float m_new = (score > m_val) ? score : m_val;
                        float exp_old = expf(m_val - m_new);
                        float exp_cur = expf(score - m_new);
                        d_val = d_val * exp_old + exp_cur;
                        for (int64_t di = 0; di < d_h; di++)
                            o_h[di] = o_h[di] * exp_old + exp_cur * v_h[di];
                        m_val = m_new;
                    }
                    if (d_val > 1e-15f)
                        for (int64_t di = 0; di < d_h; di++) o_h[di] /= d_val;
                } else {
                    double* q_h = qkv->data + (ri * 3 * d + bo);
                    double* o_h = out->data + ri * d + bo;
                    memset(o_h, 0, (size_t)d_h * sizeof(double));
                    double m_val = -1e18, d_val = 0.0;
                    for (int64_t ci = 0; ci < total_n; ci++) {
                        if (ci > cache_n + ri) break;
                        double* k_h = l->kv_cache_k->data + ci * d + bo;
                        double* v_h = l->kv_cache_v->data + ci * d + bo;
                        double score = 0.0;
                        for (int64_t di = 0; di < d_h; di++) score += q_h[di] * k_h[di];
                        score *= inv_scale_d;
                        double m_new = (score > m_val) ? score : m_val;
                        double exp_old = exp(m_val - m_new);
                        double exp_cur = exp(score - m_new);
                        d_val = d_val * exp_old + exp_cur;
                        for (int64_t di = 0; di < d_h; di++)
                            o_h[di] = o_h[di] * exp_old + exp_cur * v_h[di];
                        m_val = m_new;
                    }
                    if (d_val > 1e-15)
                        for (int64_t di = 0; di < d_h; di++) o_h[di] /= d_val;
                }
            }
        }

        AuroraTensor* final = aurora_tensor_matmul(out, l->hc_w);
        if (final && final->dtype == TENSOR_F32) {
            for (int64_t i = 0; i < final->total_size; i++)
                final->data_f32[i] += l->hc_b->data_f32[i % d];
        } else if (final) {
            for (int64_t i = 0; i < final->total_size; i++)
                final->data[i] += l->hc_b->data[i % d];
        }
        aurora_tensor_free(qkv);
        aurora_tensor_free(out);
        return final;
    }

    /* ════════════════════════════════════════════════════════
       TRAINING PATH — FlashAttention v2 (fully tiled)
       Both Q and K/V are processed in blocks of FLASH_BLOCK.
       Memory: O(B_r × d_h + B_c × d_h) per tile.
       No O(n²) attention matrix materialized.
       ════════════════════════════════════════════════════════ */

    double inv_scale_d = 1.0 / sqrt((double)d_h);
    float  inv_scale_f = 1.0f / sqrtf((float)d_h);

    if (!l->flash_m) l->flash_m = (double*)calloc((size_t)(h * n), sizeof(double));
    if (!l->flash_d) l->flash_d = (double*)calloc((size_t)(h * n), sizeof(double));

    AuroraTensor* out = l_tensor(l, input->ndim, input->shape);
    if (!out) { aurora_tensor_free(qkv); return nullptr; }
    memset(out->data_ptr, 0, (size_t)out->total_size * ((dt == TENSOR_F32) ? sizeof(float) : sizeof(double)));

    int64_t B_c = FLASH_BLOCK; /* K/V block size */
    int64_t B_r = FLASH_BLOCK; /* Q block size */

    for (int64_t hi = 0; hi < h; hi++) {
        int64_t ho = hi * d_h;
        int64_t base = hi * n;

        /* Init per-row online stats */
        for (int64_t i = 0; i < n; i++) {
            l->flash_m[base + i] = -1e18;
            l->flash_d[base + i] = 0.0;
        }

        /* Outer loop: iterate over K/V blocks */
        for (int64_t j = 0; j < n; j += B_c) {
            int64_t j_end = (j + B_c < n) ? j + B_c : n;

            /* Inner loop: iterate over Q blocks (only those affected by causal mask) */
            int64_t q_start = j; /* q_start = j ensures causal: q only attends to k up to its index */
            for (int64_t qi = q_start; qi < n; qi += B_r) {
                int64_t qi_end = (qi + B_r < n) ? qi + B_r : n;

                /* For each q in this block, compute scores against K[j..j_end] with causal masking */
                if (dt == TENSOR_F32) {
                    /* === F32 tiled attention === */
                    for (int64_t i = qi; i < qi_end; i++) {
                        if (j > i) continue; /* causal: no K before Q can attend */
                        float* q_h = qkv->data_f32 + i * 3 * d + ho;
                        float* o_h = out->data_f32 + i * d + ho;

                        float m_local = -1e18f;
                        float P_local[FLASH_BLOCK];
                        int64_t k_end = (j_end < i + 1) ? j_end : (i + 1);
                        int64_t n_cols = k_end - j;
                        if (n_cols <= 0) continue;

                        for (int64_t cj = 0; cj < n_cols; cj++) {
                            float* k_h = qkv->data_f32 + (j + cj) * 3 * d + d + ho;
                            float score = 0.0f;
                            for (int64_t dd = 0; dd < d_h; dd++)
                                score += q_h[dd] * k_h[dd];
                            score *= inv_scale_f;
                            if (cj == 0 || score > m_local) m_local = score;
                            P_local[cj] = score;
                        }

                        float d_local = 0.0f;
                        for (int64_t cj = 0; cj < n_cols; cj++) {
                            P_local[cj] = expf(P_local[cj] - m_local);
                            d_local += P_local[cj];
                        }

                        float m_old = (float)l->flash_m[base + i];
                        float d_old = (float)l->flash_d[base + i];
                        float m_new = (m_old > m_local) ? m_old : m_local;
                        float exp_old = expf(m_old - m_new);
                        float exp_local = expf(m_local - m_new);

                        float P_times_V[FLASH_BLOCK];
                        memset(P_times_V, 0, (size_t)d_h * sizeof(float));
                        for (int64_t cj = 0; cj < n_cols; cj++) {
                            float* v_h = qkv->data_f32 + (j + cj) * 3 * d + 2 * d + ho;
                            float p = P_local[cj];
                            for (int64_t dd = 0; dd < d_h; dd++)
                                P_times_V[dd] += p * v_h[dd];
                        }
                        for (int64_t dd = 0; dd < d_h; dd++)
                            o_h[dd] = o_h[dd] * exp_old + P_times_V[dd] * exp_local;

                        l->flash_d[base + i] = (double)(d_old * exp_old + d_local * exp_local);
                        l->flash_m[base + i] = (double)m_new;
                    }
                } else {
                    /* === F64 tiled attention === */
                    for (int64_t i = qi; i < qi_end; i++) {
                        if (j > i) continue;
                        double* q_h = qkv->data + i * 3 * d + ho;
                        double* o_h = out->data + i * d + ho;

                        double m_local = -1e18;
                        double P_local[FLASH_BLOCK];
                        int64_t k_end = (j_end < i + 1) ? j_end : (i + 1);
                        int64_t n_cols = k_end - j;
                        if (n_cols <= 0) continue;

                        for (int64_t cj = 0; cj < n_cols; cj++) {
                            double* k_h = qkv->data + (j + cj) * 3 * d + d + ho;
                            double score = 0.0;
                            for (int64_t dd = 0; dd < d_h; dd++)
                                score += q_h[dd] * k_h[dd];
                            score *= inv_scale_d;
                            if (cj == 0 || score > m_local) m_local = score;
                            P_local[cj] = score;
                        }

                        double d_local = 0.0;
                        for (int64_t cj = 0; cj < n_cols; cj++) {
                            P_local[cj] = exp(P_local[cj] - m_local);
                            d_local += P_local[cj];
                        }

                        double m_old = l->flash_m[base + i];
                        double d_old = l->flash_d[base + i];
                        double m_new = (m_old > m_local) ? m_old : m_local;
                        double exp_old = exp(m_old - m_new);
                        double exp_local = exp(m_local - m_new);

                        double P_times_V[FLASH_BLOCK];
                        memset(P_times_V, 0, (size_t)d_h * sizeof(double));
                        for (int64_t cj = 0; cj < n_cols; cj++) {
                            double* v_h = qkv->data + (j + cj) * 3 * d + 2 * d + ho;
                            double p = P_local[cj];
                            for (int64_t dd = 0; dd < d_h; dd++)
                                P_times_V[dd] += p * v_h[dd];
                        }
                        for (int64_t dd = 0; dd < d_h; dd++)
                            o_h[dd] = o_h[dd] * exp_old + P_times_V[dd] * exp_local;

                        l->flash_d[base + i] = d_old * exp_old + d_local * exp_local;
                        l->flash_m[base + i] = m_new;
                    }
                }
            }
        }

        /* Normalize */
        if (dt == TENSOR_F32) {
            for (int64_t qi = 0; qi < n; qi++) {
                float d_val = (float)l->flash_d[base + qi];
                if (d_val > 1e-15f) {
                    float* o_h = out->data_f32 + qi * d + ho;
                    for (int64_t dd = 0; dd < d_h; dd++) o_h[dd] /= d_val;
                }
            }
        } else {
            for (int64_t qi = 0; qi < n; qi++) {
                double d_val = l->flash_d[base + qi];
                if (d_val > 1e-15) {
                    double* o_h = out->data + qi * d + ho;
                    for (int64_t dd = 0; dd < d_h; dd++) o_h[dd] /= d_val;
                }
            }
        }
    }

    /* Output projection */
    AuroraTensor* final = aurora_tensor_matmul(out, l->hc_w);
    if (final && final->dtype == TENSOR_F32) {
        for (int64_t i = 0; i < final->total_size; i++)
            final->data_f32[i] += l->hc_b->data_f32[i % d];
    } else if (final) {
        for (int64_t i = 0; i < final->total_size; i++)
            final->data[i] += l->hc_b->data[i % d];
    }

    /* Cache for backward */
    if (training) {
        if (l->cache) aurora_tensor_free(l->cache);
        l->cache = qkv; qkv = nullptr;
        if (l->kv_cache_k) aurora_tensor_free(l->kv_cache_k);
        l->kv_cache_k = out; out = nullptr;
    }
    if (qkv) aurora_tensor_free(qkv);
    if (out) aurora_tensor_free(out);
    return final;
}

/* ════════════════════════════════════════════════════════════
   mha_backward — tiled FlashAttention backward pass
   No O(n²) attention matrix materialized.
   Recomputed P in tiles for dV, dQ, dK.
   ════════════════════════════════════════════════════════════ */

AuroraTensor* mha_backward(Layer* l, AuroraTensor* dout, AuroraTensor* input) {
    if (!l->w || !dout || !input) return nullptr;
    if (!l->dw) {
        l->dw = l_grad_tensor(l, l->w->ndim, l->w->shape);
        l->db = l_grad_tensor(l, l->b->ndim, l->b->shape);
    }
    if (!l->dw || !l->db) return nullptr;
    int64_t n = dout->shape[0], d = dout->shape[1];
    if (n != input->shape[0] || d != input->shape[1]) return nullptr;
    if (!l->cache) return nullptr;
    int64_t nh = l->num_heads > 0 ? l->num_heads : 1, hd = d / nh;
    if (hd < 1 || d % nh != 0) return nullptr;

    AuroraTensor* qkv = l->cache;
    AuroraTensor* attn_out = l->kv_cache_k;
    double inv_scale = 1.0 / sqrt((double)hd);

    /* 1. Output projection gradient hc_c (d L / d W_o) */
    if (!l->hc_c) { int64_t ws[2] = { d, d }; l->hc_c = aurora_tensor_new(2, ws); }
    memset(l->hc_c->data, 0, (size_t)d * d * sizeof(double));
    for (int64_t i = 0; i < d; i++)
        for (int64_t j = 0; j < d; j++) {
            double sum = 0.0;
            for (int64_t k = 0; k < n; k++)
                sum += attn_out->data[k * d + i] * dout->data[k * d + j];
            l->hc_c->data[i * d + j] = sum / (double)n;
        }

    /* 2. d_attn = dout @ hc_w^T  (gradient w.r.t attention output) */
    double* d_attn = (double*)malloc((size_t)n * d * sizeof(double));
    if (!d_attn) return nullptr;
    for (int64_t i = 0; i < n; i++)
        for (int64_t j = 0; j < d; j++) {
            double sum = 0.0;
            for (int64_t k = 0; k < d; k++)
                sum += dout->data[i * d + k] * l->hc_w->data[j * d + k];
            d_attn[i * d + j] = sum;
        }

    /* 3. Tiled attention backward
       Recompute P in tiles, compute dV, dQ, dK per tile
       Memory: O(B_c × hd) per tile — not O(n²) */

    double* dqkv = (double*)calloc((size_t)n * 3 * d, sizeof(double));

    for (int64_t h = 0; h < nh; h++) {
        int64_t ho = h * hd;

        /* ── dV: accumulate over query positions (tiled over Q) ── */
        /* dV[t] = Σ_b P[b][t] * d_attn[b] */
        for (int64_t b = 0; b < n; b++) {
            /* Compute row P[b][*] for this query b */
            double* Qb = &qkv->data[b * 3 * d + ho];

            /* Find the softmax denominator (need max and sum) */
            double row_max = -1e18;
            double row_sum = 0.0;
            double scores[FLASH_BLOCK]; /* score row buffer */
            int64_t cols_this_row = b + 1; /* causal: b attends to 0..b */

            for (int64_t t = 0; t <= b; t++) {
                double* Kt = &qkv->data[t * 3 * d + d + ho];
                double dot = 0.0;
                for (int64_t k = 0; k < hd; k++) dot += Qb[k] * Kt[k];
                double s = dot * inv_scale;
                scores[t] = s;
                if (t == 0 || s > row_max) row_max = s;
            }
            for (int64_t t = 0; t <= b; t++) {
                scores[t] = exp(scores[t] - row_max);
                row_sum += scores[t];
            }
            if (row_sum > 1e-15) {
                for (int64_t t = 0; t <= b; t++) {
                    double p = scores[t] / row_sum;
                    /* dV[t] += P[b][t] * d_attn[b] */
                    for (int64_t k = 0; k < hd; k++)
                        dqkv[t * 3 * d + 2 * d + ho + k] += p * d_attn[b * d + ho + k];
                }
            }
        }

        /* ── dQ and dK: tiled approach using recomputed softmax ── */
        /* dS[b][t] = P[b][t] * (dV[b][t] - Σ_k P[b][k] * dV[b][k]) */
        /* where dV[b][t] = Σ_i d_attn[b][i] * V[t][i] */

        for (int64_t b = 0; b < n; b++) {
            double* Qb = &qkv->data[b * 3 * d + ho];

            /* Recompute softmax for row b */
            double row_max = -1e18;
            double row_sum = 0.0;
            double scores[FLASH_BLOCK];
            for (int64_t t = 0; t <= b; t++) {
                double* Kt = &qkv->data[t * 3 * d + d + ho];
                double dot = 0.0;
                for (int64_t k = 0; k < hd; k++) dot += Qb[k] * Kt[k];
                scores[t] = dot * inv_scale;
                if (t == 0 || scores[t] > row_max) row_max = scores[t];
            }
            for (int64_t t = 0; t <= b; t++) {
                scores[t] = exp(scores[t] - row_max);
                row_sum += scores[t];
            }
            double inv_sum = (row_sum > 1e-15) ? 1.0 / row_sum : 0.0;
            for (int64_t t = 0; t <= b; t++) scores[t] *= inv_sum; /* now P[b][t] */

            /* Compute dP[b][t] = Σ_k d_attn[b][k] * V[t][k] */
            double dP[FLASH_BLOCK];
            memset(dP, 0, (size_t)(b + 1) * sizeof(double));
            for (int64_t t = 0; t <= b; t++) {
                double* Vt = &qkv->data[t * 3 * d + 2 * d + ho];
                double sum = 0.0;
                for (int64_t k = 0; k < hd; k++)
                    sum += d_attn[b * d + ho + k] * Vt[k];
                dP[t] = sum;
            }

            /* dot = Σ_t P[b][t] * dP[t] */
            double dot_soft = 0.0;
            for (int64_t t = 0; t <= b; t++) dot_soft += scores[t] * dP[t];

            /* dS[b][t] = P[b][t] * (dP[t] - dot) */
            for (int64_t t = 0; t <= b; t++) {
                double ds = scores[t] * (dP[t] - dot_soft);
                /* dQ[b] += ds * K[t] * scale */
                double* Kt = &qkv->data[t * 3 * d + d + ho];
                for (int64_t k = 0; k < hd; k++)
                    dqkv[b * 3 * d + ho + k] += ds * Kt[k] * inv_scale;
                /* dK[t] += ds * Q[b] * scale */
                for (int64_t k = 0; k < hd; k++)
                    dqkv[t * 3 * d + d + ho + k] += ds * Qb[k] * inv_scale;
            }
        }
    }

    /* 4. QKV projection gradients */
    memset(l->dw->data, 0, (size_t)l->dw->total_size * sizeof(double));
    memset(l->db->data, 0, (size_t)l->db->total_size * sizeof(double));
    for (int64_t i = 0; i < d; i++)
        for (int64_t j = 0; j < 3 * d; j++) {
            double sum = 0.0;
            for (int64_t k = 0; k < n; k++)
                sum += input->data[k * d + i] * dqkv[k * 3 * d + j];
            l->dw->data[i * 3 * d + j] = sum / (double)n;
        }
    for (int64_t j = 0; j < 3 * d; j++) {
        double sum = 0.0;
        for (int64_t k = 0; k < n; k++) sum += dqkv[k * 3 * d + j];
        l->db->data[j] = sum / (double)n;
    }

    /* 5. Input gradient: dx = dqkv @ w^T */
    AuroraTensor* dx = aurora_tensor_new(input->ndim, input->shape);
    if (!dx) { free(d_attn); free(dqkv); return nullptr; }
    for (int64_t i = 0; i < n; i++)
        for (int64_t j = 0; j < d; j++) {
            double sum = 0.0;
            for (int64_t k = 0; k < 3 * d; k++)
                sum += dqkv[i * 3 * d + k] * l->w->data[j * 3 * d + k];
            dx->data[i * d + j] = sum;
        }

    free(d_attn); free(dqkv);
    return dx;
}



/* === Transformer Block: constructs sub-layers in model === */
int64_t transformer_block(Model* m, int64_t embed_dim, int64_t num_heads, int64_t ff_dim) {
    if (m->n_layers + 7 > MAX_LAYERS) return 0;
    int start = m->n_layers;
    /* LAYER_RESIDUAL_SAVE — saves input for first residual add */
    Layer* save1 = &m->layers[m->n_layers++];
    memset(save1, 0, sizeof(Layer)); save1->type = LAYER_RESIDUAL_SAVE;
    /* LayerNorm 1 */
    Layer* ln1 = &m->layers[m->n_layers++];
    memset(ln1, 0, sizeof(Layer)); ln1->type = LAYER_LAYERNORM; ln1->units = embed_dim;
    /* MHA */
    Layer* attn = &m->layers[m->n_layers++];
    memset(attn, 0, sizeof(Layer)); attn->type = LAYER_ATTENTION; attn->units = embed_dim;
    attn->num_heads = num_heads; attn->embed_dim = embed_dim;
    /* LAYER_RESIDUAL_ADD — adds saved input (from save1) to MHA output */
    Layer* add1 = &m->layers[m->n_layers++];
    memset(add1, 0, sizeof(Layer)); add1->type = LAYER_RESIDUAL_ADD;
    add1->residual_layer = start; /* points to save1 */
    /* LAYER_RESIDUAL_SAVE — saves input for second residual add */
    Layer* save2 = &m->layers[m->n_layers++];
    memset(save2, 0, sizeof(Layer)); save2->type = LAYER_RESIDUAL_SAVE;
    /* LayerNorm 2 */
    Layer* ln2 = &m->layers[m->n_layers++];
    memset(ln2, 0, sizeof(Layer)); ln2->type = LAYER_LAYERNORM; ln2->units = embed_dim;
    /* FFN: Dense(ff_dim, relu) + Dense(embed_dim, linear) */
    Layer* ff1 = &m->layers[m->n_layers++];
    memset(ff1, 0, sizeof(Layer)); ff1->type = LAYER_DENSE; ff1->units = ff_dim; ff1->activation = ACT_RELU;
    Layer* ff2 = &m->layers[m->n_layers++];
    memset(ff2, 0, sizeof(Layer)); ff2->type = LAYER_DENSE; ff2->units = embed_dim; ff2->activation = ACT_LINEAR;
    /* LAYER_RESIDUAL_ADD — adds saved input (from save2) to FFN output */
    Layer* add2 = &m->layers[m->n_layers++];
    memset(add2, 0, sizeof(Layer)); add2->type = LAYER_RESIDUAL_ADD;
    add2->residual_layer = start + 4; /* points to save2 */
    return start;
}

/* ── SwiGLU FFN ── */
/* gate(x) = silu(x @ w + b),  up(x) = x @ hc_w,  out = (gate * up) @ hc_b */
AuroraTensor* swiglu_forward(Layer* l, AuroraTensor* input, int training) {
    if (!input || !l->w) return nullptr;
    int64_t n = input->shape[0], d = input->shape[1];
    int64_t hidden = l->units;

    /* Gate: x @ w + b */
    AuroraTensor* gate = aurora_tensor_matmul(input, l->w);
    if (!gate) return nullptr;
    for (int64_t i = 0; i < gate->total_size; i++)
        gate->data[i] += l->b->data[i % l->b->total_size];

    /* Up: x @ hc_w */
    AuroraTensor* up = aurora_tensor_matmul(input, l->hc_w);
    if (!up) { aurora_tensor_free(gate); return nullptr; }

    /* silu(gate) */
    activation_forward(gate, ACT_SILU);

    /* gate * up */
    int64_t nh_2[2] = { n, hidden };
    AuroraTensor* gated = aurora_tensor_new(2, nh_2);
    if (!gated) { aurora_tensor_free(gate); aurora_tensor_free(up); return nullptr; }
    for (int64_t i = 0; i < n * hidden; i++)
        gated->data[i] = gate->data[i] * up->data[i];

    /* Down: gated @ hc_b */
    AuroraTensor* out = aurora_tensor_matmul(gated, l->hc_b);
    if (!out) { aurora_tensor_free(gate); aurora_tensor_free(up); aurora_tensor_free(gated); return nullptr; }

    if (training) {
        if (l->cache) aurora_tensor_free(l->cache);
        l->cache = gate; gate = nullptr;    /* gate post-silu for silu backward */
        if (l->kv_cache_k) aurora_tensor_free(l->kv_cache_k);
        l->kv_cache_k = gated; gated = nullptr; /* gated result for down proj grad */
    } else {
        aurora_tensor_free(gate);
    }
    aurora_tensor_free(up);
    if (gated) aurora_tensor_free(gated);
    return out;
}

AuroraTensor* swiglu_backward(Layer* l, AuroraTensor* dout, AuroraTensor* input) {
    if (!l->w || !dout || !input) return nullptr;
    AuroraTensor* gate = l->cache;       /* post-silu gate */
    AuroraTensor* gated = l->kv_cache_k; /* gate * up (pre-down-proj) */
    if (!gate || !gated) return nullptr;

    int64_t n = input->shape[0], d = input->shape[1];
    int64_t hidden = l->units;

    /* ── Down projection gradient ── hc_b = gated^T @ d_out */
    if (!l->hc_d) l->hc_d = aurora_tensor_new(l->hc_b->ndim, l->hc_b->shape);
    if (!l->hc_d) return nullptr;
    memset(l->hc_d->data, 0, (size_t)l->hc_d->total_size * sizeof(double));
    /* hc_b shape: [hidden, output_dim] */
    for (int64_t i = 0; i < hidden; i++) {
        for (int64_t j = 0; j < l->hc_b->shape[1]; j++) {
            double sum = 0.0;
            for (int64_t b = 0; b < n; b++)
                sum += gated->data[b * hidden + i] * dout->data[b * l->hc_b->shape[1] + j];
            l->hc_d->data[i * l->hc_b->shape[1] + j] = sum / (double)n;
        }
    }

    /* d_gated = d_out @ hc_b^T */
    int64_t ts[2] = { l->hc_b->shape[1], hidden };
    AuroraTensor* hc_b_t = aurora_tensor_new(2, ts);
    for (int64_t i = 0; i < hidden; i++)
        for (int64_t j = 0; j < l->hc_b->shape[1]; j++)
            hc_b_t->data[j * hidden + i] = l->hc_b->data[i * l->hc_b->shape[1] + j];
    AuroraTensor* d_gated = aurora_tensor_matmul(dout, hc_b_t);
    aurora_tensor_free(hc_b_t);
    if (!d_gated) return nullptr;

    /* ── Compute up gradient from gated ── */
    int64_t nh_3[2] = { n, hidden };
    AuroraTensor* d_up = aurora_tensor_new(2, nh_3);
    memset(d_up->data, 0, (size_t)n * hidden * sizeof(double));
    for (int64_t i = 0; i < n * hidden; i++)
        d_up->data[i] = d_gated->data[i] * gate->data[i]; /* gate is post-silu */

    /* ── Compute gate pre-silu gradient ── */
    /* Recover pre-silu gate by storing in cache (gate IS cache, overwrite) */
    /* Actually we need pre-silu for activation_backward, but we stored post-silu.
       We'll compute d_gate_pre = d_gated * up, then do silu backward in-place on d_gate_pre.
       But we need the post-silu output (gate) for silu backward. activation_backward
       takes (output, dout, act) and modifies dout to be gradient w.r.t. pre-activation.
       dout = d_gated * up, output = gate (post-silu). */
    for (int64_t i = 0; i < n * hidden; i++)
        d_gated->data[i] *= (gated->data[i] / (gate->data[i] + 1e-15)); /* d_gated * up (since gated = gate * up) */
    /* Now d_gated holds d(gate*up)/d(gate) * dL/d(gate*up) = up * dL/d(gate*up) = dL/d(gate_post) */
    /* Apply silu backward to get dL/d(gate_pre) */
    activation_backward(gate, d_gated, ACT_SILU);
    /* d_gated now holds dL/d(gate_pre) */

    /* ── Gate weight/bias gradients ── */
    if (!l->dw) { l->dw = aurora_tensor_new(2, l->w->shape); l->db = aurora_tensor_new(2, l->b->shape); }
    memset(l->dw->data, 0, (size_t)l->dw->total_size * sizeof(double));
    memset(l->db->data, 0, (size_t)l->db->total_size * sizeof(double));
    for (int64_t j = 0; j < hidden; j++) {
        double bsum = 0.0;
        for (int64_t b = 0; b < n; b++) bsum += d_gated->data[b * hidden + j];
        l->db->data[j] = bsum / (double)n;
        for (int64_t i = 0; i < d; i++) {
            double sum = 0.0;
            for (int64_t b = 0; b < n; b++)
                sum += input->data[b * d + i] * d_gated->data[b * hidden + j];
            l->dw->data[i * hidden + j] = sum / (double)n;
        }
    }

    /* ── Up weight gradient ── */
    if (!l->hc_c || l->hc_c->total_size != l->hc_w->total_size) {
        if (l->hc_c) aurora_tensor_free(l->hc_c);
        l->hc_c = aurora_tensor_new(2, l->hc_w->shape);
    }
    memset(l->hc_c->data, 0, (size_t)l->hc_c->total_size * sizeof(double));
    for (int64_t j = 0; j < hidden; j++) {
        for (int64_t i = 0; i < d; i++) {
            double sum = 0.0;
            for (int64_t b = 0; b < n; b++)
                sum += input->data[b * d + i] * d_up->data[b * hidden + j];
            l->hc_c->data[i * hidden + j] = sum / (double)n;
        }
    }

    /* ── Input gradient ── */
    /* d_gate_pre @ w^T + d_up @ hc_w^T */
    int64_t hd_2[2] = { hidden, d };
    AuroraTensor* w_t = aurora_tensor_new(2, hd_2);
    for (int64_t i = 0; i < d; i++)
        for (int64_t j = 0; j < hidden; j++)
            w_t->data[j * d + i] = l->w->data[i * hidden + j];
    AuroraTensor* dx_gate = aurora_tensor_matmul(d_gated, w_t);
    aurora_tensor_free(w_t);

    AuroraTensor* hc_w_t = aurora_tensor_new(2, hd_2);
    for (int64_t i = 0; i < d; i++)
        for (int64_t j = 0; j < hidden; j++)
            hc_w_t->data[j * d + i] = l->hc_w->data[i * hidden + j];
    AuroraTensor* dx_up = aurora_tensor_matmul(d_up, hc_w_t);
    aurora_tensor_free(hc_w_t);

    if (!dx_gate || !dx_up) { aurora_tensor_free(d_gated); aurora_tensor_free(d_up); aurora_tensor_free(dx_gate); aurora_tensor_free(dx_up); return nullptr; }
    for (int64_t i = 0; i < dx_gate->total_size; i++)
        dx_gate->data[i] += dx_up->data[i];
    aurora_tensor_free(dx_up);

    aurora_tensor_free(d_gated);
    aurora_tensor_free(d_up);
    return dx_gate;
}
AuroraTensor* unembed_forward(Layer* l, AuroraTensor* input, int training) {
    (void)training;
    if (!input || !l->w) return nullptr;
    /* l->w is embedding weight [vocab, embed_dim]. Need input @ w^T => [n, vocab] */
    int64_t n = input->shape[0], emb = input->shape[1], voc = l->w->shape[0];
    int64_t os[2] = { n, voc };
    AuroraTensor* out = aurora_tensor_new(2, os);
    for (int64_t ri = 0; ri < n; ri++)
        for (int64_t ci = 0; ci < voc; ci++) {
            double sum = 0.0;
            for (int64_t k = 0; k < emb; k++)
                sum += input->data[ri * emb + k] * l->w->data[ci * emb + k];
            out->data[ri * voc + ci] = sum;
        }
    return out;
}

AuroraTensor* unembed_backward(Layer* l, AuroraTensor* dout, AuroraTensor* input) {
    if (!l->w || !dout || !input) return nullptr;
    if (!l->dw) { l->dw = aurora_tensor_new(l->w->ndim, l->w->shape); memset(l->dw->data, 0, (size_t)l->dw->total_size * sizeof(double)); }
    /* dW = input^T @ dout (partial, tied weight handled separately) */
    int64_t batch_seq = input->shape[0], emb = input->shape[1], voc = dout->shape[1];
    for (int64_t v = 0; v < voc; v++)
        for (int64_t e = 0; e < emb; e++) {
            double s = 0.0;
            for (int64_t b = 0; b < batch_seq; b++)
                s += input->data[b * emb + e] * dout->data[b * voc + v];
            l->dw->data[v * emb + e] = s / (double)batch_seq;
        }
    return nullptr;
}

/* === Dense Layer === */
int model_init_layer_weights(Model* m, int idx, int64_t input_size) {
    Layer* l = &m->layers[idx];
    if (l->type == LAYER_DENSE) {
        double scale = sqrt(2.0 / (double)(input_size + l->units));
        int64_t ws[2] = { input_size, l->units };
        l->w = aurora_tensor_new(2, ws);
        for (int64_t i = 0; i < l->w->total_size; i++) l->w->data[i] = rand_uniform() * scale;
        int64_t bs[2] = { 1, l->units };
        l->b = aurora_tensor_new(2, bs);
        memset(l->b->data, 0, (size_t)l->b->total_size * sizeof(double));
        m->total_params += l->w->total_size + l->b->total_size;
    } else if (l->type == LAYER_LSTM) {
        int64_t u = l->units;
        double scale = sqrt(2.0 / (double)(input_size + 4 * u));
        int64_t ws[2] = { input_size, 4 * u };
        l->w = aurora_tensor_new(2, ws);
        for (int64_t i = 0; i < l->w->total_size; i++) l->w->data[i] = rand_uniform() * scale;
        int64_t hws[2] = { u, 4 * u };
        l->hc_w = aurora_tensor_new(2, hws);
        double hscale = sqrt(2.0 / (double)(u + 4 * u));
        for (int64_t i = 0; i < l->hc_w->total_size; i++) l->hc_w->data[i] = rand_uniform() * hscale;
        int64_t bs[2] = { 1, 4 * u };
        l->b = aurora_tensor_new(2, bs);
        memset(l->b->data, 0, (size_t)l->b->total_size * sizeof(double));
        m->total_params += l->w->total_size + l->hc_w->total_size + l->b->total_size;
    } else if (l->type == LAYER_GRU) {
        int64_t u = l->units;
        double scale = sqrt(2.0 / (double)(input_size + 3 * u));
        int64_t ws[2] = { input_size, 3 * u };
        l->w = aurora_tensor_new(2, ws);
        for (int64_t i = 0; i < l->w->total_size; i++) l->w->data[i] = rand_uniform() * scale;
        int64_t hws[2] = { u, 3 * u };
        l->hc_w = aurora_tensor_new(2, hws);
        double hscale = sqrt(2.0 / (double)(u + 3 * u));
        for (int64_t i = 0; i < l->hc_w->total_size; i++) l->hc_w->data[i] = rand_uniform() * hscale;
        int64_t bs[2] = { 1, 3 * u };
        l->b = aurora_tensor_new(2, bs);
        memset(l->b->data, 0, (size_t)l->b->total_size * sizeof(double));
        m->total_params += l->w->total_size + l->hc_w->total_size + l->b->total_size;
    } else if (l->type == LAYER_ATTENTION) {
        int64_t d = input_size;
        l->units = d; /* MHA output dim = input dim */
        double scale = sqrt(2.0 / (double)(d + 3 * d));
        int64_t ws[2] = { d, 3 * d };
        l->w = aurora_tensor_new(2, ws);
        for (int64_t i = 0; i < l->w->total_size; i++) l->w->data[i] = rand_uniform() * scale;
        int64_t bs[1] = { 3 * d };
        l->b = aurora_tensor_new(1, bs);
        memset(l->b->data, 0, (size_t)3 * d * sizeof(double));
        int64_t os[2] = { d, d };
        l->hc_w = aurora_tensor_new(2, os);
        for (int64_t i = 0; i < l->hc_w->total_size; i++) l->hc_w->data[i] = rand_uniform() * scale;
        int64_t obs[1] = { d };
        l->hc_b = aurora_tensor_new(1, obs);
        memset(l->hc_b->data, 0, (size_t)d * sizeof(double));
        m->total_params += l->w->total_size + l->b->total_size + l->hc_w->total_size + l->hc_b->total_size;
    } else if (l->type == LAYER_MOE) {
        int64_t d = input_size, num_experts = l->num_heads, hidden = l->units;
        double scale = sqrt(2.0 / (double)(d + num_experts));
        int64_t rs[2] = { d, num_experts };
        l->w = aurora_tensor_new(2, rs);
        for (int64_t i = 0; i < l->w->total_size; i++) l->w->data[i] = rand_uniform() * scale;
        int64_t es[2] = { num_experts * d, hidden };
        l->hc_w = aurora_tensor_new(2, es);
        for (int64_t i = 0; i < l->hc_w->total_size; i++) l->hc_w->data[i] = rand_uniform() * scale;
        m->total_params += l->w->total_size + l->hc_w->total_size;
    } else if (l->type == LAYER_SWIGLU) {
        int64_t d = input_size, hidden = l->units;
        double scale = sqrt(2.0 / (double)(d + hidden));
        /* Gate: [d, hidden] */
        int64_t ws[2] = { d, hidden };
        l->w = aurora_tensor_new(2, ws);
        for (int64_t i = 0; i < l->w->total_size; i++) l->w->data[i] = rand_uniform() * scale;
        int64_t bs[2] = { 1, hidden };
        l->b = aurora_tensor_new(2, bs);
        memset(l->b->data, 0, (size_t)l->b->total_size * sizeof(double));
        /* Up: [d, hidden] */
        l->hc_w = aurora_tensor_new(2, ws);
        for (int64_t i = 0; i < l->hc_w->total_size; i++) l->hc_w->data[i] = rand_uniform() * scale;
        /* Down: [hidden, output_units] */
        int64_t ds[2] = { hidden, l->embed_dim > 0 ? l->embed_dim : d };
        l->hc_b = aurora_tensor_new(2, ds);
        for (int64_t i = 0; i < l->hc_b->total_size; i++) l->hc_b->data[i] = rand_uniform() * sqrt(2.0 / (double)(hidden + ds[1]));
        m->total_params += l->w->total_size + l->b->total_size + l->hc_w->total_size + l->hc_b->total_size;
    }
    return 1;
}


AuroraTensor* dense_forward(Layer* l, AuroraTensor* input, int training) {
    if (!input) return nullptr;
    if (!l->w) {
        int64_t in_dim = input->shape[1];
        double scale = sqrt(2.0 / (double)(in_dim + l->units));
        int64_t ws[2] = { in_dim, l->units };
        l->w = l_tensor(l, 2, ws);
        if (l->dtype == TENSOR_F32) {
            for (int64_t i = 0; i < l->w->total_size; i++)
                l->w->data_f32[i] = (float)(rand_uniform() * scale);
        } else {
            for (int64_t i = 0; i < l->w->total_size; i++)
                l->w->data[i] = rand_uniform() * scale;
        }
        int64_t bs[2] = { 1, l->units };
        l->b = l_tensor(l, 2, bs);
        memset(l->b->data_ptr, 0, (size_t)l->b->total_size *
               ((l->dtype == TENSOR_F32) ? sizeof(float) : sizeof(double)));
    }
    if (!l->w) return nullptr;

    AuroraTensor* w_use = l->w;
    AuroraTensor* temp_w = nullptr;

    if (l->q_data && !training) {
        int64_t sz = l->w->total_size;
        temp_w = l_tensor(l, l->w->ndim, l->w->shape);
        if (l->dtype == TENSOR_F32) {
            for (int64_t j = 0; j < sz; j++)
                temp_w->data_f32[j] = (float)(l->q_zero + l->q_scale * ((double)l->q_data[j] + 128.0));
        } else {
            for (int64_t j = 0; j < sz; j++)
                temp_w->data[j] = l->q_zero + l->q_scale * ((double)l->q_data[j] + 128.0);
        }
        w_use = temp_w;
    }

    AuroraTensor* z = aurora_tensor_matmul(input, w_use);
    if (temp_w) aurora_tensor_free(temp_w);
    if (!z) return nullptr;

    /* LoRA: z += (input @ lora_A) @ lora_B * (alpha / r) */
    if (l->lora_enabled && l->lora_A && l->lora_B && l->lora_r > 0) {
        double lora_scale = l->lora_alpha / (double)l->lora_r;
        AuroraTensor* xA = aurora_tensor_matmul(input, l->lora_A);
        if (xA) {
            AuroraTensor* lora_out = aurora_tensor_matmul(xA, l->lora_B);
            if (lora_out) {
                /* Apply dropout to LoRA output during training */
                if (training && l->lora_dropout > 0.0) {
                    double scale_d = 1.0 / (1.0 - l->lora_dropout);
                    if (lora_out->dtype == TENSOR_F32) {
                        for (int64_t ii = 0; ii < lora_out->total_size; ii++)
                            if ((double)rand() / RAND_MAX < l->lora_dropout) lora_out->data_f32[ii] = 0.0f;
                            else lora_out->data_f32[ii] *= (float)(lora_scale * scale_d);
                    } else {
                        for (int64_t ii = 0; ii < lora_out->total_size; ii++)
                            if ((double)rand() / RAND_MAX < l->lora_dropout) lora_out->data[ii] = 0.0;
                            else lora_out->data[ii] *= lora_scale * scale_d;
                    }
                } else {
                    if (lora_out->dtype == TENSOR_F32) {
                        for (int64_t ii = 0; ii < lora_out->total_size; ii++)
                            lora_out->data_f32[ii] *= (float)lora_scale;
                    } else {
                        for (int64_t ii = 0; ii < lora_out->total_size; ii++)
                            lora_out->data[ii] *= lora_scale;
                    }
                }
                /* z += lora_out */
                if (z->dtype == TENSOR_F32 && lora_out->dtype == TENSOR_F32) {
                    for (int64_t ii = 0; ii < z->total_size; ii++)
                        z->data_f32[ii] += lora_out->data_f32[ii];
                } else {
                    AuroraTensor* z_f64 = (z->dtype == TENSOR_F64) ? z : aurora_tensor_to_f64(z);
                    AuroraTensor* lo_f64 = (lora_out->dtype == TENSOR_F64) ? lora_out : aurora_tensor_to_f64(lora_out);
                    if (z_f64 != z) { aurora_tensor_free(z); z = z_f64; }
                    for (int64_t ii = 0; ii < z->total_size; ii++)
                        z->data[ii] += lo_f64->data[ii];
                    if (lo_f64 != lora_out) aurora_tensor_free(lo_f64);
                }
                aurora_tensor_free(lora_out);
            }
            aurora_tensor_free(xA);
        }
    }

    /* Bias addition — dtype-aware */
    if (l->dtype == TENSOR_F32 && z->dtype == TENSOR_F32) {
        for (int64_t i = 0; i < z->total_size; i++)
            z->data_f32[i] += l->b->data_f32[i % l->b->total_size];
    } else {
        /* Ensure z is F64 for the fallback path */
        AuroraTensor* z_f64 = (z->dtype == TENSOR_F64) ? z : aurora_tensor_to_f64(z);
        if (z_f64 != z) { aurora_tensor_free(z); z = z_f64; }
        for (int64_t i = 0; i < z->total_size; i++)
            z->data[i] += l->b->data[i % l->b->total_size];
    }
    if (training && l->dropout_rate > 0.0 && l->dropout_rate < 1.0) {
        double scale = 1.0 / (1.0 - l->dropout_rate);
        if (z->dtype == TENSOR_F32) {
            for (int64_t i = 0; i < z->total_size; i++)
                if ((double)rand() / RAND_MAX < l->dropout_rate) z->data_f32[i] = 0.0f;
                else z->data_f32[i] *= (float)scale;
        } else {
            for (int64_t i = 0; i < z->total_size; i++)
                if ((double)rand() / RAND_MAX < l->dropout_rate) z->data[i] = 0.0;
                else z->data[i] *= scale;
        }
    }
    activation_forward(z, l->activation);
    if (training) {
        if (l->cache) aurora_tensor_free(l->cache);
        l->cache = l_tensor(l, z->ndim, z->shape);
        if (z->dtype == TENSOR_F32) {
            memcpy(l->cache->data_f32, z->data_f32, (size_t)z->total_size * sizeof(float));
        } else {
            memcpy(l->cache->data, z->data, (size_t)z->total_size * sizeof(double));
        }
    }
    return z;
}


AuroraTensor* dense_backward(Layer* l, AuroraTensor* dout, AuroraTensor* input) {
    if (!l->w || !dout || !input) return nullptr;
    if (l->cache) activation_backward(l->cache, dout, l->activation);
    int64_t batch = input->shape[0], in_dim = input->shape[1], units = l->w->shape[1];
    if (!l->dw) { l->dw = l_grad_tensor(l, 2, l->w->shape); l->db = l_grad_tensor(l, 2, l->b->shape); }
    memset(l->dw->data, 0, (size_t)l->dw->total_size * sizeof(double));
    memset(l->db->data, 0, (size_t)l->db->total_size * sizeof(double));
    /* Convert dout to F64 if needed for gradient computation */
    AuroraTensor* dout_f64 = (dout->dtype == TENSOR_F64) ? dout : aurora_tensor_to_f64(dout);
    AuroraTensor* input_f64 = (input->dtype == TENSOR_F64) ? input : aurora_tensor_to_f64(input);
    for (int64_t j = 0; j < units; j++) {
        double bsum = 0.0;
        for (int64_t b = 0; b < batch; b++) bsum += dout_f64->data[b * units + j];
        l->db->data[j] = bsum / (double)batch;
        for (int64_t i = 0; i < in_dim; i++) {
            double sum = 0.0;
            for (int64_t b = 0; b < batch; b++)
                sum += input_f64->data[b * in_dim + i] * dout_f64->data[b * units + j];
            l->dw->data[i * units + j] = sum / (double)batch;
        }
    }
    if (dout_f64 != dout) aurora_tensor_free(dout_f64);
    if (input_f64 != input) aurora_tensor_free(input_f64);
    int64_t ts[2] = { units, in_dim };
    AuroraTensor* wt;
    if (l->dtype == TENSOR_F32) {
        wt = aurora_tensor_new_f32(2, ts);
        for (int64_t i = 0; i < in_dim; i++)
            for (int64_t j = 0; j < units; j++)
                wt->data_f32[j * in_dim + i] = l->w->data_f32[i * units + j];
    } else {
        wt = aurora_tensor_new(2, ts);
        for (int64_t i = 0; i < in_dim; i++)
            for (int64_t j = 0; j < units; j++)
                wt->data[j * in_dim + i] = l->w->data[i * units + j];
    }
    AuroraTensor* dx = aurora_tensor_matmul(dout, wt);
    aurora_tensor_free(wt);

    /* ── LoRA backward: gradients for A and B ── */
    if (l->lora_enabled && l->lora_A && l->lora_B && l->lora_r > 0 && l->lora_A_grad && l->lora_B_grad) {
        double lora_scale = l->lora_alpha / (double)l->lora_r;
        AuroraTensor* dout_f64_2 = (dout->dtype == TENSOR_F64) ? dout : aurora_tensor_to_f64(dout);
        AuroraTensor* input_f64_2 = (input->dtype == TENSOR_F64) ? input : aurora_tensor_to_f64(input);

        /* xA = input @ A  (for B gradient) */
        AuroraTensor* xA = aurora_tensor_matmul(input_f64_2, l->lora_A);

        /* dB = (xA)^T @ dout * scale / batch */
        int64_t dB_shape[2] = { l->lora_r, l->w->shape[1] };
        memset(l->lora_B_grad->data, 0, (size_t)l->lora_B_grad->total_size * sizeof(double));
        for (int64_t ri = 0; ri < l->lora_r; ri++) {
            for (int64_t j = 0; j < l->w->shape[1]; j++) {
                double sum = 0.0;
                for (int64_t b = 0; b < batch; b++)
                    sum += xA->data[b * l->lora_r + ri] * dout_f64_2->data[b * l->w->shape[1] + j];
                l->lora_B_grad->data[ri * l->w->shape[1] + j] = sum / (double)batch * lora_scale;
            }
        }

        /* dA = input^T @ (dout @ B^T) * scale / batch */
        memset(l->lora_A_grad->data, 0, (size_t)l->lora_A_grad->total_size * sizeof(double));
        for (int64_t i = 0; i < input->shape[1]; i++) {
            for (int64_t ri = 0; ri < l->lora_r; ri++) {
                double sum = 0.0;
                for (int64_t b = 0; b < batch; b++) {
                    double db_contrib = 0.0;
                    for (int64_t j = 0; j < l->w->shape[1]; j++)
                        db_contrib += dout_f64_2->data[b * l->w->shape[1] + j] * l->lora_B->data[ri * l->w->shape[1] + j];
                    sum += input_f64_2->data[b * input->shape[1] + i] * db_contrib;
                }
                l->lora_A_grad->data[i * l->lora_r + ri] = sum / (double)batch * lora_scale;
            }
        }

        /* Add LoRA contribution to input gradient: dout @ B^T @ A^T * scale */
        int64_t Bt_shape[2] = { l->w->shape[1], l->lora_r };
        AuroraTensor* Bt = aurora_tensor_new(2, Bt_shape);
        for (int64_t ri = 0; ri < l->lora_r; ri++)
            for (int64_t j = 0; j < l->w->shape[1]; j++)
                Bt->data[j * l->lora_r + ri] = l->lora_B->data[ri * l->w->shape[1] + j];
        AuroraTensor* dout_Bt = aurora_tensor_matmul(dout_f64_2, Bt);
        aurora_tensor_free(Bt);

        int64_t At_shape[2] = { l->lora_r, input->shape[1] };
        AuroraTensor* At = aurora_tensor_new(2, At_shape);
        for (int64_t i = 0; i < input->shape[1]; i++)
            for (int64_t ri = 0; ri < l->lora_r; ri++)
                At->data[ri * input->shape[1] + i] = l->lora_A->data[i * l->lora_r + ri];
        AuroraTensor* lora_dx = aurora_tensor_matmul(dout_Bt, At);
        aurora_tensor_free(At);
        aurora_tensor_free(dout_Bt);

        if (lora_dx) {
            for (int64_t i = 0; i < dx->total_size && i < lora_dx->total_size; i++)
                dx->data[i] += lora_dx->data[i] * lora_scale;
            aurora_tensor_free(lora_dx);
        }

        aurora_tensor_free(xA);
        if (dout_f64_2 != dout) aurora_tensor_free(dout_f64_2);
        if (input_f64_2 != input) aurora_tensor_free(input_f64_2);
    }

    return dx;
}


/* ── im2col ── */
void im2col(const double* input, int64_t batch, int64_t h, int64_t w, int64_t c,
                    int64_t kh, int64_t kw, int64_t sh, int64_t sw,
                    double* col, int64_t oh, int64_t ow) {
    for (int64_t b = 0; b < batch; b++)
        for (int64_t oy = 0; oy < oh; oy++)
            for (int64_t ox = 0; ox < ow; ox++)
                for (int64_t ky = 0; ky < kh; ky++)
                    for (int64_t kx = 0; kx < kw; kx++)
                        for (int64_t cc = 0; cc < c; cc++) {
                            int64_t iy = oy * sh + ky;
                            int64_t ix = ox * sw + kx;
                            int64_t col_idx = ((b * oh + oy) * ow + ox) * (kh * kw * c) + ((ky * kw + kx) * c + cc);
                            int64_t in_idx = ((b * h + iy) * w + ix) * c + cc;
                            col[col_idx] = input[in_idx];
                        }
}

/* ── col2im ── */
void col2im(const double* col, int64_t batch, int64_t h, int64_t w, int64_t c,
                    int64_t kh, int64_t kw, int64_t sh, int64_t sw,
                    double* input, int64_t oh, int64_t ow) {
    memset(input, 0, (size_t)(batch * h * w * c) * sizeof(double));
    if (batch <= 0 || h <= 0 || w <= 0 || c <= 0) return;
    for (int64_t b = 0; b < batch; b++)
        for (int64_t oy = 0; oy < oh; oy++)
            for (int64_t ox = 0; ox < ow; ox++)
                for (int64_t ky = 0; ky < kh; ky++)
                    for (int64_t kx = 0; kx < kw; kx++)
                        for (int64_t cc = 0; cc < c; cc++) {
                            int64_t iy = oy * sh + ky;
                            int64_t ix = ox * sw + kx;
                            if (iy < 0 || iy >= h || ix < 0 || ix >= w) continue;
                            int64_t col_idx = ((b * oh + oy) * ow + ox) * (kh * kw * c) + ((ky * kw + kx) * c + cc);
                            int64_t in_idx = ((b * h + iy) * w + ix) * c + cc;
                            input[in_idx] += col[col_idx];
                        }
}

/* ── Conv2D Layer ── */
AuroraTensor* conv2d_forward(Layer* l, AuroraTensor* input, int training) {
    if (!input || input->ndim != 4) return nullptr;
    int64_t batch = input->shape[0], h = input->shape[1], w = input->shape[2], c = input->shape[3];
    int64_t kh = l->kernel_size, kw = l->kernel_size;
    int64_t sh = l->stride, sw = l->stride;
    int64_t oh = (h - kh) / sh + 1, ow = (w - kw) / sw + 1;
    int64_t filters = l->units;
    if (oh <= 0 || ow <= 0) return nullptr;
    if (!l->w) {
        double scale = sqrt(2.0 / (double)(kh * kw * c + filters));
        int64_t ws[2] = { kh * kw * c, filters };
        l->w = aurora_tensor_new(2, ws);
        for (int64_t i = 0; i < l->w->total_size; i++) l->w->data[i] = rand_uniform() * scale;
        int64_t bs[2] = { 1, filters };
        l->b = aurora_tensor_new(2, bs);
        memset(l->b->data, 0, (size_t)l->b->total_size * sizeof(double));
    }
    int64_t col_h = batch * oh * ow, col_w = kh * kw * c;
    int64_t col_shape[2] = { col_h, col_w };
    AuroraTensor* col = aurora_tensor_new(2, col_shape);
    im2col(input->data, batch, h, w, c, kh, kw, sh, sw, col->data, oh, ow);
    AuroraTensor* out = aurora_tensor_matmul(col, l->w);
    for (int64_t i = 0; i < out->total_size; i++)
        out->data[i] += l->b->data[i % l->b->total_size];
    int64_t os[4] = { batch, oh, ow, filters };
    AuroraTensor* reshaped = aurora_tensor_reshape(out, 4, os);
    if (out != reshaped) aurora_tensor_free(out);
    if (l->cache) aurora_tensor_free(l->cache);
    l->cache = col;
    activation_forward(reshaped, l->activation);
    return reshaped;
}

AuroraTensor* conv2d_backward(Layer* l, AuroraTensor* dout, AuroraTensor* input) {
    if (!l->w || !dout || !input) return nullptr;
    int64_t batch = input->shape[0], h = input->shape[1], w = input->shape[2], c = input->shape[3];
    int64_t kh = l->kernel_size, kw = l->kernel_size;
    int64_t sh = l->stride, sw = l->stride;
    int64_t oh = (h - kh) / sh + 1, ow = (w - kw) / sw + 1;
    int64_t filters = l->units;
    if (!l->dw) { l->dw = aurora_tensor_new(2, l->w->shape); l->db = aurora_tensor_new(2, l->b->shape); }
    memset(l->dw->data, 0, (size_t)l->dw->total_size * sizeof(double));
    memset(l->db->data, 0, (size_t)l->db->total_size * sizeof(double));
    int64_t col_h = batch * oh * ow, col_w = kh * kw * c;
    int64_t col_shape[2] = { col_h, col_w };
    AuroraTensor* col = aurora_tensor_new(2, col_shape);
    im2col(input->data, batch, h, w, c, kh, kw, sh, sw, col->data, oh, ow);
    for (int64_t j = 0; j < filters; j++) {
        double bsum = 0.0;
        for (int64_t i = 0; i < col_h; i++) bsum += dout->data[i * filters + j];
        l->db->data[j] = bsum / (double)batch;
        for (int64_t i = 0; i < col_w; i++) {
            double sum = 0.0;
            for (int64_t k = 0; k < col_h; k++)
                sum += col->data[k * col_w + i] * dout->data[k * filters + j];
            l->dw->data[i * filters + j] = sum / (double)batch;
        }
    }
    int64_t wts[2] = { filters, col_w };
    AuroraTensor* wt = aurora_tensor_new(2, wts);
    for (int64_t i = 0; i < col_w; i++)
        for (int64_t j = 0; j < filters; j++)
            wt->data[j * col_w + i] = l->w->data[i * filters + j];
    AuroraTensor* dx_col = aurora_tensor_matmul(dout, wt);
    aurora_tensor_free(wt);
    int64_t in_shape[4] = { batch, h, w, c };
    AuroraTensor* dx = aurora_tensor_new(4, in_shape);
    col2im(dx_col->data, batch, h, w, c, kh, kw, sh, sw, dx->data, oh, ow);
    aurora_tensor_free(dx_col);
    aurora_tensor_free(col);
    return dx;
}

/* ── LSTM Layer ── */
AuroraTensor* lstm_forward(Layer* l, AuroraTensor* input, int training) {
    if (!input || input->ndim < 2) return nullptr;
    int64_t batch = input->shape[0], input_size = input->shape[1];
    int64_t units = l->units;
    if (!l->w) {
        double scale = sqrt(2.0 / (double)(input_size + 4 * units));
        int64_t ws[2] = { input_size, 4 * units };
        l->w = aurora_tensor_new(2, ws);
        for (int64_t i = 0; i < l->w->total_size; i++) l->w->data[i] = rand_uniform() * scale;
        int64_t hws[2] = { units, 4 * units };
        l->hc_w = aurora_tensor_new(2, hws);
        double hscale = sqrt(2.0 / (double)(units + 4 * units));
        for (int64_t i = 0; i < l->hc_w->total_size; i++) l->hc_w->data[i] = rand_uniform() * hscale;
        int64_t bs[2] = { 1, 4 * units };
        l->b = aurora_tensor_new(2, bs);
        memset(l->b->data, 0, (size_t)l->b->total_size * sizeof(double));
    }
    AuroraTensor* gates = aurora_tensor_matmul(input, l->w);

    /* Cache h_prev before overwriting l->cache */
    AuroraTensor* h_prev = nullptr;
    if (training) {
        if (l->cache) {
            h_prev = aurora_tensor_new(l->cache->ndim, l->cache->shape);
            memcpy(h_prev->data, l->cache->data, (size_t)l->cache->total_size * sizeof(double));
        }
    }

    if (!l->cache) {
        int64_t hs[2] = { batch, units };
        l->cache = aurora_tensor_new(2, hs);
        memset(l->cache->data, 0, (size_t)l->cache->total_size * sizeof(double));
        l->running_mean = aurora_tensor_new(2, hs);
        memset(l->running_mean->data, 0, (size_t)l->running_mean->total_size * sizeof(double));
    } else if (l->cache->shape[0] != batch) {
        aurora_tensor_free(l->cache);
        int64_t hs[2] = { batch, units };
        l->cache = aurora_tensor_new(2, hs);
        memset(l->cache->data, 0, (size_t)l->cache->total_size * sizeof(double));
    }
    if (l->cache) {
        AuroraTensor* h_gates = aurora_tensor_matmul(l->cache, l->hc_w);
        for (int64_t i = 0; i < gates->total_size; i++)
            gates->data[i] += h_gates->data[i];
        aurora_tensor_free(h_gates);
    }
    for (int64_t i = 0; i < gates->total_size; i++)
        gates->data[i] += l->b->data[i % l->b->total_size];

    /* Cache pre-activation gates for backward */
    if (training) {
        if (l->kv_cache_k) aurora_tensor_free(l->kv_cache_k);
        l->kv_cache_k = aurora_tensor_new(gates->ndim, gates->shape);
        memcpy(l->kv_cache_k->data, gates->data, (size_t)gates->total_size * sizeof(double));
        if (l->kv_cache_v) aurora_tensor_free(l->kv_cache_v);
        l->kv_cache_v = h_prev; /* take ownership; may be nullptr */
        h_prev = nullptr;
    }

    int64_t os2[2] = { batch, units };
    AuroraTensor* out = aurora_tensor_new(2, os2);
    double* c_data = l->running_mean ? l->running_mean->data : nullptr;
    for (int64_t b = 0; b < batch; b++) {
        int64_t bo = b * 4 * units;
        double* g = &gates->data[bo];
        double* hp = l->cache ? &l->cache->data[b * units] : nullptr;
        double* cp = c_data ? &c_data[b * units] : nullptr;
        double* ho = &out->data[b * units];
        for (int64_t j = 0; j < units; j++) {
            double ig = 1.0 / (1.0 + exp(-g[j]));
            double fg = 1.0 / (1.0 + exp(-g[units + j]));
            double og = 1.0 / (1.0 + exp(-g[2 * units + j]));
            double gg = tanh(g[3 * units + j]);
            double cn = fg * (cp ? cp[j] : 0.0) + ig * gg;
            double hn = og * tanh(cn);
            ho[j] = hn;
            if (cp) cp[j] = cn;
        }
    }
    if (l->cache)
        memcpy(l->cache->data, out->data, (size_t)out->total_size * sizeof(double));
    if (h_prev) aurora_tensor_free(h_prev);
    aurora_tensor_free(gates);
    return out;
}

AuroraTensor* lstm_backward(Layer* l, AuroraTensor* dout, AuroraTensor* input) {
    if (!l->w || !l->hc_w || !dout || !input) return nullptr;
    int64_t batch = dout->shape[0], units = l->units;
    int64_t input_size = input->shape[1];
    double* gates_pre = l->kv_cache_k ? l->kv_cache_k->data : nullptr;
    double* h_prev_data = l->kv_cache_v ? l->kv_cache_v->data : nullptr;
    if (!gates_pre) return nullptr;

    if (!l->dw) {
        l->dw = aurora_tensor_new(2, l->w->shape);
        l->db = aurora_tensor_new(2, l->b->shape);
    }
    if (!l->hc_c) l->hc_c = aurora_tensor_new(2, l->hc_w->shape);
    if (!l->dw || !l->db || !l->hc_c) return nullptr;

    memset(l->dw->data, 0, (size_t)l->dw->total_size * sizeof(double));
    memset(l->db->data, 0, (size_t)l->db->total_size * sizeof(double));
    memset(l->hc_c->data, 0, (size_t)l->hc_c->total_size * sizeof(double));

    /* Pre-compute entire dl/d(gates_pre) then do weight gradient at end */
    AuroraTensor* dgates = aurora_tensor_new(l->kv_cache_k->ndim, l->kv_cache_k->shape);
    memset(dgates->data, 0, (size_t)dgates->total_size * sizeof(double));

    double* c_data = l->running_mean ? l->running_mean->data : nullptr;

    for (int64_t b = 0; b < batch; b++) {
        int64_t bo = b * 4 * units;
        int64_t in_off = b * input_size;
        double* g = &gates_pre[bo];
        double* dg = &dgates->data[bo];
        double* hp = h_prev_data ? &h_prev_data[b * units] : nullptr;
        double* cp = c_data ? &c_data[b * units] : nullptr;
        double* dout_row = &dout->data[b * units];

        for (int64_t j = 0; j < units; j++) {
            /* Reconstruct forward activations */
            double i_pre = g[j];
            double f_pre = g[units + j];
            double o_pre = g[2 * units + j];
            double g_pre = g[3 * units + j];

            double i = 1.0 / (1.0 + exp(-i_pre));
            double f = 1.0 / (1.0 + exp(-f_pre));
            double o = 1.0 / (1.0 + exp(-o_pre));
            double g_candidate = tanh(g_pre);

            double c_prev_val = cp ? cp[j] : 0.0;
            double c_new = f * c_prev_val + i * g_candidate;
            double tanh_cn = tanh(c_new);

            /* dL/d(h_new) = dout */
            double dh = dout_row[j];

            /* dL/d(o) = dh * tanh_cn */
            double do_val = dh * tanh_cn;
            /* dL/d(o_pre) = do_val * sigmoid_deriv(o) */
            dg[2 * units + j] = do_val * o * (1.0 - o);

            /* dL/d(tanh_cn) = dh * o */
            double dtanh = dh * o;
            /* dL/d(c_new) = dtanh * (1 - tanh_cn^2) */
            double dc = dtanh * (1.0 - tanh_cn * tanh_cn);

            /* dL/d(i) = dc * g_candidate */
            double di = dc * g_candidate;
            dg[j] = di * i * (1.0 - i);  /* sigmoid derivative */

            /* dL/d(g_candidate) = dc * i */
            double dg_candidate = dc * i;
            dg[3 * units + j] = dg_candidate * (1.0 - g_candidate * g_candidate);  /* tanh derivative */

            /* dL/d(f) = dc * c_prev_val */
            double df = dc * c_prev_val;
            dg[units + j] = df * f * (1.0 - f);  /* sigmoid derivative */
        }
    }

    /* Weight gradients: dw = input^T @ dgates (averaged over batch) */
    for (int64_t j = 0; j < 4 * units; j++) {
        for (int64_t h = 0; h < input_size; h++) {
            double sum = 0.0;
            for (int64_t b = 0; b < batch; b++)
                sum += input->data[b * input_size + h] * dgates->data[b * 4 * units + j];
            l->dw->data[h * 4 * units + j] = sum / (double)batch;
        }
        double bsum = 0.0;
        for (int64_t b = 0; b < batch; b++)
            bsum += dgates->data[b * 4 * units + j];
        l->db->data[j] = bsum / (double)batch;
    }

    /* Hidden weight gradients: hc_c = h_prev^T @ dgates (if h_prev exists) */
    if (h_prev_data) {
        for (int64_t j = 0; j < 4 * units; j++) {
            for (int64_t h = 0; h < units; h++) {
                double sum = 0.0;
                for (int64_t b = 0; b < batch; b++)
                    sum += h_prev_data[b * units + h] * dgates->data[b * 4 * units + j];
                l->hc_c->data[h * 4 * units + j] = sum / (double)batch;
            }
        }
    }

    /* Input gradient: dx = dgates @ w^T */
    AuroraTensor* dx = aurora_tensor_new(input->ndim, input->shape);
    memset(dx->data, 0, (size_t)dx->total_size * sizeof(double));
    for (int64_t h = 0; h < input_size; h++) {
        for (int64_t b = 0; b < batch; b++) {
            double sum = 0.0;
            for (int64_t j = 0; j < 4 * units; j++)
                sum += l->w->data[h * 4 * units + j] * dgates->data[b * 4 * units + j];
            dx->data[b * input_size + h] = sum;
        }
    }

    aurora_tensor_free(dgates);

    /* Free caches */
    if (l->kv_cache_k) { aurora_tensor_free(l->kv_cache_k); l->kv_cache_k = nullptr; }
    if (l->kv_cache_v) { aurora_tensor_free(l->kv_cache_v); l->kv_cache_v = nullptr; }

    return dx;
}

/* ── GRU Layer ── */
AuroraTensor* gru_forward(Layer* l, AuroraTensor* input, int training) {
    if (!input || input->ndim < 2) return nullptr;
    int64_t batch = input->shape[0], input_size = input->shape[1];
    int64_t units = l->units;
    if (!l->w) {
        double scale = sqrt(2.0 / (double)(input_size + 3 * units));
        int64_t ws[2] = { input_size, 3 * units };
        l->w = aurora_tensor_new(2, ws);
        for (int64_t i = 0; i < l->w->total_size; i++) l->w->data[i] = rand_uniform() * scale;
        int64_t hws[2] = { units, 3 * units };
        l->hc_w = aurora_tensor_new(2, hws);
        double hscale = sqrt(2.0 / (double)(units + 3 * units));
        for (int64_t i = 0; i < l->hc_w->total_size; i++) l->hc_w->data[i] = rand_uniform() * hscale;
        int64_t bs[2] = { 1, 3 * units };
        l->b = aurora_tensor_new(2, bs);
        memset(l->b->data, 0, (size_t)l->b->total_size * sizeof(double));
    }
    AuroraTensor* gates = aurora_tensor_matmul(input, l->w);

    /* Cache h_prev before overwriting l->cache */
    AuroraTensor* h_prev = nullptr;
    if (training) {
        if (l->cache) {
            h_prev = aurora_tensor_new(l->cache->ndim, l->cache->shape);
            memcpy(h_prev->data, l->cache->data, (size_t)l->cache->total_size * sizeof(double));
        }
    }

    if (!l->cache) {
        int64_t hs[2] = { batch, units };
        l->cache = aurora_tensor_new(2, hs);
        memset(l->cache->data, 0, (size_t)l->cache->total_size * sizeof(double));
    } else if (l->cache->shape[0] != batch) {
        aurora_tensor_free(l->cache);
        int64_t hs[2] = { batch, units };
        l->cache = aurora_tensor_new(2, hs);
        memset(l->cache->data, 0, (size_t)l->cache->total_size * sizeof(double));
    }
    if (l->cache) {
        AuroraTensor* h_gates = aurora_tensor_matmul(l->cache, l->hc_w);
        for (int64_t i = 0; i < gates->total_size; i++)
            gates->data[i] += h_gates->data[i];
        aurora_tensor_free(h_gates);
    }
    for (int64_t i = 0; i < gates->total_size; i++)
        gates->data[i] += l->b->data[i % l->b->total_size];

    /* Cache pre-activation gates for backward */
    if (training) {
        if (l->kv_cache_k) aurora_tensor_free(l->kv_cache_k);
        l->kv_cache_k = aurora_tensor_new(gates->ndim, gates->shape);
        memcpy(l->kv_cache_k->data, gates->data, (size_t)gates->total_size * sizeof(double));
        if (l->kv_cache_v) aurora_tensor_free(l->kv_cache_v);
        l->kv_cache_v = h_prev;
        h_prev = nullptr;
    }

    int64_t os3[2] = { batch, units };
    AuroraTensor* out = aurora_tensor_new(2, os3);
    for (int64_t b = 0; b < batch; b++) {
        int64_t bo = b * 3 * units;
        double* g = &gates->data[bo];
        double* hp = l->cache ? &l->cache->data[b * units] : nullptr;
        double* ho = &out->data[b * units];
        for (int64_t j = 0; j < units; j++) {
            double r = 1.0 / (1.0 + exp(-g[j]));
            double z = 1.0 / (1.0 + exp(-g[units + j]));
            double n = tanh(g[2 * units + j] + r * (hp ? hp[j] : 0.0));
            ho[j] = (1.0 - z) * n + z * (hp ? hp[j] : 0.0);
        }
    }
    if (l->cache)
        memcpy(l->cache->data, out->data, (size_t)out->total_size * sizeof(double));
    if (h_prev) aurora_tensor_free(h_prev);
    aurora_tensor_free(gates);
    return out;
}

AuroraTensor* gru_backward(Layer* l, AuroraTensor* dout, AuroraTensor* input) {
    if (!l->w || !l->hc_w || !dout || !input) return nullptr;
    int64_t batch = dout->shape[0], units = l->units;
    int64_t input_size = input->shape[1];
    double* gates_pre = l->kv_cache_k ? l->kv_cache_k->data : nullptr;
    double* h_prev_data = l->kv_cache_v ? l->kv_cache_v->data : nullptr;
    if (!gates_pre) return nullptr;

    if (!l->dw) {
        l->dw = aurora_tensor_new(2, l->w->shape);
        l->db = aurora_tensor_new(2, l->b->shape);
    }
    if (!l->hc_c) l->hc_c = aurora_tensor_new(2, l->hc_w->shape);
    if (!l->dw || !l->db || !l->hc_c) return nullptr;

    memset(l->dw->data, 0, (size_t)l->dw->total_size * sizeof(double));
    memset(l->db->data, 0, (size_t)l->db->total_size * sizeof(double));
    memset(l->hc_c->data, 0, (size_t)l->hc_c->total_size * sizeof(double));

    AuroraTensor* dgates = aurora_tensor_new(l->kv_cache_k->ndim, l->kv_cache_k->shape);
    memset(dgates->data, 0, (size_t)dgates->total_size * sizeof(double));

    for (int64_t b = 0; b < batch; b++) {
        int64_t bo = b * 3 * units;
        double* g = &gates_pre[bo];
        double* dg = &dgates->data[bo];
        double* hp = h_prev_data ? &h_prev_data[b * units] : nullptr;
        double* dout_row = &dout->data[b * units];

        for (int64_t j = 0; j < units; j++) {
            double r_pre = g[j];
            double z_pre = g[units + j];
            double n_pre = g[2 * units + j];

            double r = 1.0 / (1.0 + exp(-r_pre));
            double z = 1.0 / (1.0 + exp(-z_pre));
            double h_prev_val = hp ? hp[j] : 0.0;
            double n_gate = n_pre + r * h_prev_val;
            double n = tanh(n_gate);
            double h_new = (1.0 - z) * n + z * h_prev_val;

            (void)h_new;
            double dh = dout_row[j];

            /* dL/d(z) = dh * (h_prev_val - n) */
            double dz = dh * (h_prev_val - n);
            dg[units + j] = dz * z * (1.0 - z);

            /* dL/d(n) = dh * (1 - z) */
            double dn = dh * (1.0 - z);
            /* dL/d(n_gate) = dn * tanh_deriv(n) */
            double dn_gate = dn * (1.0 - n * n);
            dg[2 * units + j] = dn_gate;

            /* dL/d(r) = dn_gate * h_prev_val */
            double dr = dn_gate * h_prev_val;
            dg[j] = dr * r * (1.0 - r);
        }
    }

    /* Weight gradients: input^T @ dgates */
    for (int64_t j = 0; j < 3 * units; j++) {
        for (int64_t h = 0; h < input_size; h++) {
            double sum = 0.0;
            for (int64_t b = 0; b < batch; b++)
                sum += input->data[b * input_size + h] * dgates->data[b * 3 * units + j];
            l->dw->data[h * 3 * units + j] = sum / (double)batch;
        }
        double bsum = 0.0;
        for (int64_t b = 0; b < batch; b++)
            bsum += dgates->data[b * 3 * units + j];
        l->db->data[j] = bsum / (double)batch;
    }

    /* Hidden weight gradients: h_prev^T @ dgates */
    if (h_prev_data) {
        for (int64_t j = 0; j < 3 * units; j++) {
            for (int64_t h = 0; h < units; h++) {
                double sum = 0.0;
                for (int64_t b = 0; b < batch; b++)
                    sum += h_prev_data[b * units + h] * dgates->data[b * 3 * units + j];
                l->hc_c->data[h * 3 * units + j] = sum / (double)batch;
            }
        }
    }

    /* Input gradient: dx = dgates @ w^T */
    AuroraTensor* dx = aurora_tensor_new(input->ndim, input->shape);
    memset(dx->data, 0, (size_t)dx->total_size * sizeof(double));
    for (int64_t h = 0; h < input_size; h++) {
        for (int64_t b = 0; b < batch; b++) {
            double sum = 0.0;
            for (int64_t j = 0; j < 3 * units; j++)
                sum += l->w->data[h * 3 * units + j] * dgates->data[b * 3 * units + j];
            dx->data[b * input_size + h] = sum;
        }
    }

    aurora_tensor_free(dgates);
    if (l->kv_cache_k) { aurora_tensor_free(l->kv_cache_k); l->kv_cache_k = nullptr; }
    if (l->kv_cache_v) { aurora_tensor_free(l->kv_cache_v); l->kv_cache_v = nullptr; }

    return dx;
}

/* === MoE (Mixture of Experts) Layer === */
/* Layer config: l->num_heads = num_experts, l->units = expert_hidden_dim */
/* l->w = router weights [d, num_experts], l->hc_w = expert weights [num_experts * d, hidden] */
/* During training: l->cache stores router probs [n, num_experts] for backward */

AuroraTensor* moe_forward(Layer* l, AuroraTensor* input, int training) {
    if (!input || input->ndim < 2) return nullptr;
    int64_t n = input->shape[0], d = input->shape[1];
    int64_t num_experts = l->num_heads > 0 ? l->num_heads : 4;
    int64_t hidden = l->units > 0 ? l->units : d * 2;
    int64_t k = 2; /* top-2 routing */
    if (!l->w) {
        double scale = sqrt(2.0 / (double)(d + num_experts));
        int64_t rs[2] = { d, num_experts };
        l->w = aurora_tensor_new(2, rs);
        for (int64_t i = 0; i < l->w->total_size; i++) l->w->data[i] = rand_uniform() * scale;
        int64_t es[2] = { num_experts * d, hidden };
        l->hc_w = aurora_tensor_new(2, es);
        for (int64_t i = 0; i < l->hc_w->total_size; i++) l->hc_w->data[i] = rand_uniform() * scale;
    }
    /* Router: logits = input @ W_router */
    AuroraTensor* logits = aurora_tensor_matmul(input, l->w);
    /* Softmax over experts — store probs in logits (in-place) */
    for (int64_t i = 0; i < n; i++) {
        double mx = logits->data[i * num_experts];
        for (int64_t j = 1; j < num_experts; j++) if (logits->data[i * num_experts + j] > mx) mx = logits->data[i * num_experts + j];
        double sm = 0.0;
        for (int64_t j = 0; j < num_experts; j++) { logits->data[i * num_experts + j] = exp(logits->data[i * num_experts + j] - mx); sm += logits->data[i * num_experts + j]; }
        if (sm > 1e-15) for (int64_t j = 0; j < num_experts; j++) logits->data[i * num_experts + j] /= sm;
    }
    /* Top-k selection per token */
    int64_t* top_k = (int64_t*)malloc((size_t)n * k * sizeof(int64_t));
    double* top_w = (double*)malloc((size_t)n * k * sizeof(double));
    for (int64_t i = 0; i < n; i++) {
        for (int64_t ki = 0; ki < k; ki++) {
            int64_t best = -1; double best_v = -1e9;
            for (int64_t j = 0; j < num_experts; j++) {
                double v = logits->data[i * num_experts + j];
                int already = 0;
                for (int64_t t = 0; t < ki; t++) if (top_k[i * k + t] == j) { already = 1; break; }
                if (!already && v > best_v) { best_v = v; best = j; }
            }
            top_k[i * k + ki] = best;
            top_w[i * k + ki] = best_v;
        }
    }
    /* Expert computation — accumulate weighted expert outputs */
    int64_t os[2] = { n, hidden };
    AuroraTensor* out = aurora_tensor_new(2, os);
    memset(out->data, 0, (size_t)out->total_size * sizeof(double));
    for (int64_t i = 0; i < n; i++) {
        for (int64_t ki = 0; ki < k; ki++) {
            int64_t e = top_k[i * k + ki];
            if (e < 0) continue;
            double* expert_w = &l->hc_w->data[e * d * hidden];
            double* out_vec = &out->data[i * hidden];
            double w = top_w[i * k + ki];
            for (int64_t j = 0; j < hidden; j++) {
                double sum = 0.0;
                for (int64_t h = 0; h < d; h++)
                    sum += input->data[i * d + h] * expert_w[h * hidden + j];
                out_vec[j] += sum * w; /* accumulate weighted expert outputs */
            }
        }
    }
    /* Cache router probs for backward */
    if (training) {
        /* Compute auxiliary load-balancing loss */
        double* probs = logits->data;
        double* f_i = (double*)calloc((size_t)num_experts, sizeof(double));
        double* P_i = (double*)calloc((size_t)num_experts, sizeof(double));
        for (int64_t i = 0; i < n; i++) {
            for (int64_t ki = 0; ki < k; ki++) {
                int64_t e = top_k[i * k + ki];
                if (e >= 0) f_i[e] += 1.0;
            }
            for (int64_t j = 0; j < num_experts; j++)
                P_i[j] += probs[i * num_experts + j];
        }
        double inv_n = 1.0 / (double)n;
        double aux = 0.0;
        for (int64_t j = 0; j < num_experts; j++) {
            f_i[j] *= inv_n;
            P_i[j] *= inv_n;
            aux += f_i[j] * P_i[j];
        }
        aux *= (double)num_experts;
        l->aux_loss = aux;
        free(f_i); free(P_i);

        if (l->cache) aurora_tensor_free(l->cache);
        l->cache = logits;
        /* Note: logits now holds softmax probs — we keep it alive as l->cache */
    } else {
        aurora_tensor_free(logits);
    }
    free(top_k); free(top_w);
    return out;
}



AuroraTensor* moe_backward(Layer* l, AuroraTensor* dout, AuroraTensor* input) {
    if (!l->w || !l->hc_w || !dout || !input) return nullptr;
    int64_t n = dout->shape[0], hidden = dout->shape[1];
    int64_t d = input->shape[1];
    int64_t num_experts = l->num_heads > 0 ? l->num_heads : 4;
    int64_t k = 2;
    double* cache_probs = l->cache ? l->cache->data : nullptr;
    if (!cache_probs) return nullptr;

    /* Lazily allocate gradient buffers */
    if (!l->dw) l->dw = aurora_tensor_new(l->w->ndim, l->w->shape);
    if (!l->hc_c) l->hc_c = aurora_tensor_new(l->hc_w->ndim, l->hc_w->shape);
    if (!l->dw || !l->hc_c) return nullptr;

    /* Zero gradients */
    memset(l->dw->data, 0, (size_t)l->dw->total_size * sizeof(double));
    memset(l->hc_c->data, 0, (size_t)l->hc_c->total_size * sizeof(double));

    /* Input gradient */
    AuroraTensor* dx = aurora_tensor_new(input->ndim, input->shape);
    memset(dx->data, 0, (size_t)dx->total_size * sizeof(double));

    /* Per-token backward */
    for (int64_t i = 0; i < n; i++) {
        int64_t off_base = i * k;

        /* Recompute top-2 from cached probs */
        int64_t top_idx[2] = { -1, -1 };
        double top_val[2] = { -1e9, -1e9 };
        for (int64_t e = 0; e < num_experts; e++) {
            double v = cache_probs[i * num_experts + e];
            for (int64_t ki = 0; ki < k; ki++) {
                if (v > top_val[ki]) {
                    for (int64_t sh = k - 1; sh > ki; sh--) {
                        top_val[sh] = top_val[sh - 1];
                        top_idx[sh] = top_idx[sh - 1];
                    }
                    top_val[ki] = v;
                    top_idx[ki] = e;
                    break;
                }
            }
        }

        /* Compute expert contribution for router gradient */
        double* expert_contrib = (double*)calloc((size_t)num_experts, sizeof(double));

        for (int64_t ki = 0; ki < k; ki++) {
            int64_t e = top_idx[ki];
            if (e < 0) continue;
            double w = cache_probs[i * num_experts + e]; /* gating weight = softmax prob */
            double* expert_w = &l->hc_w->data[e * d * hidden];
            double* in_row = &input->data[i * d];
            double* dout_row = &dout->data[i * hidden];

            /* Expert weight gradients: hc_c += w * input^T @ dout (averaged over batch) */
            for (int64_t h = 0; h < d; h++) {
                double in_val = in_row[h];
                if (in_val == 0.0) continue;
                double* hc_c_row = &l->hc_c->data[e * d * hidden + h * hidden];
                for (int64_t j = 0; j < hidden; j++)
                    hc_c_row[j] += w * in_val * dout_row[j] / (double)n;
            }

            /* Input gradient from expert path: dx += w * dout @ W_expert^T */
            for (int64_t h = 0; h < d; h++) {
                double sum = 0.0;
                for (int64_t j = 0; j < hidden; j++)
                    sum += expert_w[h * hidden + j] * dout_row[j];
                dx->data[i * d + h] += w * sum;
            }

            /* Expert contribution to output (for router gradient) */
            for (int64_t j = 0; j < hidden; j++) {
                double expert_out_j = 0.0;
                for (int64_t h = 0; h < d; h++)
                    expert_out_j += in_row[h] * expert_w[h * hidden + j];
                expert_contrib[e] += expert_out_j * dout_row[j];
            }
        }

        /* Router gradient: softmax Jacobian */
        for (int64_t e = 0; e < num_experts; e++) {
            double p = cache_probs[i * num_experts + e];
            /* weighted_avg_contrib = sum_f p_f * expert_contrib[f] */
            double weighted_avg = 0.0;
            for (int64_t f = 0; f < num_experts; f++)
                weighted_avg += cache_probs[i * num_experts + f] * expert_contrib[f];
            double dl_d_logit = p * (expert_contrib[e] - weighted_avg);

            /* dw += input^T @ dl_d_logits (averaged over batch) */
            for (int64_t h = 0; h < d; h++)
                l->dw->data[h * num_experts + e] += input->data[i * d + h] * dl_d_logit / (double)n;
        }

        free(expert_contrib);
    }

    /* Free cached probs */
    if (l->cache) { aurora_tensor_free(l->cache); l->cache = nullptr; }

    return dx;
}


/* === INT8 Quantization === */
int64_t quantize_weights(Model* m) {
    if (!m) return 0;
    int64_t total = 0;
    for (int i = 0; i < m->n_layers; i++) {
        Layer* l = &m->layers[i];
        AuroraTensor* tensors[] = { l->w, l->b, l->hc_w, l->hc_b, l->running_mean, l->running_var };
        for (int t = 0; t < 6; t++) {
            if (!tensors[t]) continue;
            AuroraTensor* wt = tensors[t];
            int64_t sz = wt->total_size;
            double mn = wt->data[0], mx = wt->data[0];
            for (int64_t j = 1; j < sz; j++) {
                if (wt->data[j] < mn) mn = wt->data[j];
                if (wt->data[j] > mx) mx = wt->data[j];
            }
            double scale_q = (mx - mn) / 255.0;
            if (scale_q < 1e-15) scale_q = 1e-15;
            /* Primary weight tensor (w) — store int8 data for inference */
            if (t == 0) {
                if (l->q_data) free(l->q_data);
                l->q_data = (int8_t*)malloc((size_t)sz * sizeof(int8_t));
                l->q_size = sz;
                l->q_scale = scale_q;
                l->q_zero = mn;
                for (int64_t j = 0; j < sz; j++) {
                    l->q_data[j] = (int8_t)((wt->data[j] - mn) / scale_q - 128.0);
                    double reconstructed = mn + scale_q * ((double)l->q_data[j] + 128.0);
                    wt->data[j] = reconstructed;
                }
            } else {
                /* Other tensors: just measure error */
                int8_t* qd = (int8_t*)malloc((size_t)sz * sizeof(int8_t));
                for (int64_t j = 0; j < sz; j++)
                    qd[j] = (int8_t)((wt->data[j] - mn) / scale_q - 128.0);
                double max_err = 0.0;
                for (int64_t j = 0; j < sz && j < 10; j++) {
                    double reconstructed = mn + scale_q * ((double)qd[j] + 128.0);
                    double err = fabs(reconstructed - wt->data[j]);
                    if (err > max_err) max_err = err;
                }
                free(qd);
            }
            total += sz;
        }
    }
    return total;
}

/* === Group-wise INT8 Quantization === */
/* Quantize weights in groups of `group_size` elements, each with own scale/zero */
/* Stores result in layer's q_data but with interleaved metadata */
int64_t quantize_groupwise(Model* m, int64_t group_size) {
    if (!m) return 0;
    if (group_size < 1) group_size = 64;
    int64_t total = 0;
    for (int i = 0; i < m->n_layers; i++) {
        Layer* l = &m->layers[i];
        if (!l->w || l->w->total_size <= 0) continue;
        int64_t sz = l->w->total_size;
        int64_t n_groups = (sz + group_size - 1) / group_size;

        /* Free old quant data */
        if (l->q_data) { free(l->q_data); l->q_data = nullptr; }
        l->q_size = 0;

        /* Allocate: q_data will hold { scale[0], zero[0], qdata[0..gs-1], scale[1], zero[1], ... } as doubles */
        /* Layout: per group = [scale (double), zero (double), q_data (int8_t[group_size])] */
        int64_t header_per_group = 2; /* scale + zero as doubles */
        int64_t bytes_per_group = header_per_group * (int64_t)sizeof(double) + group_size * (int64_t)sizeof(int8_t);
        l->q_data = (int8_t*)calloc((size_t)n_groups, (size_t)bytes_per_group);
        if (!l->q_data) continue;
        l->q_size = n_groups * bytes_per_group;

        for (int64_t g = 0; g < n_groups; g++) {
            int64_t start = g * group_size;
            int64_t end = start + group_size;
            if (end > sz) end = sz;
            int64_t gsz = end - start;

            /* Find min/max */
            double mn = l->w->data[start], mx = l->w->data[start];
            for (int64_t j = start + 1; j < end; j++) {
                if (l->w->data[j] < mn) mn = l->w->data[j];
                if (l->w->data[j] > mx) mx = l->w->data[j];
            }
            double scale_q = (mx - mn) / 255.0;
            if (scale_q < 1e-15) scale_q = 1e-15;

            /* Write scale and zero as doubles at start of group block */
            double* hdr = (double*)(l->q_data + g * bytes_per_group);
            hdr[0] = scale_q;
            hdr[1] = mn;

            /* Write quantized data */
            int8_t* qd = (int8_t*)(hdr + 2);
            for (int64_t j = 0; j < gsz; j++)
                qd[j] = (int8_t)((l->w->data[start + j] - mn) / scale_q - 128.0);

            /* Replace original weights with dequantized values (for backward compat) */
            for (int64_t j = 0; j < gsz; j++)
                l->w->data[start + j] = mn + scale_q * ((double)qd[j] + 128.0);
        }
        total += sz;
    }
    return total;
}

/* KV cache quantization: quantize K/V cache in all attention layers to int8 */
/* Returns KV cache parameter count. Real quantization is applied per-token in MHA forward. */
int64_t quantize_kv_cache(Model* m) {
    if (!m) return 0;
    int64_t total_orig = 0, total_quant = 0;
    for (int i = 0; i < m->n_layers; i++) {
        Layer* l = &m->layers[i];
        if (l->type != LAYER_ATTENTION) continue;
        if (!l->kv_cache_k || !l->kv_cache_v) continue;

        int64_t count = l->kv_cache_k->total_size;
        total_orig += count * 2;

        /* Allocate quantized buffer (int8 = 1 byte per value) */
        free(l->kv_qbuf);
        l->kv_qbuf = (int8_t*)malloc((size_t)(count * 2));
        if (!l->kv_qbuf) continue;
        l->kv_qlen = count * 2;

        /* Compute min/max for K cache to determine scale */
        double k_min = l->kv_cache_k->data[0], k_max = l->kv_cache_k->data[0];
        for (int64_t j = 1; j < count; j++) {
            if (l->kv_cache_k->data[j] < k_min) k_min = l->kv_cache_k->data[j];
            if (l->kv_cache_k->data[j] > k_max) k_max = l->kv_cache_k->data[j];
        }
        double v_min = l->kv_cache_v->data[0], v_max = l->kv_cache_v->data[0];
        for (int64_t j = 1; j < count; j++) {
            if (l->kv_cache_v->data[j] < v_min) v_min = l->kv_cache_v->data[j];
            if (l->kv_cache_v->data[j] > v_max) v_max = l->kv_cache_v->data[j];
        }

        double k_range = k_max - k_min;
        double v_range = v_max - v_min;
        l->kv_qscale = (k_range > v_range ? k_range : v_range) / 255.0;
        if (l->kv_qscale < 1e-15) l->kv_qscale = 1.0;
        l->kv_qzero = 128.0;

        /* Quantize K cache */
        for (int64_t j = 0; j < count; j++) {
            double val = l->kv_cache_k->data[j] / l->kv_qscale + l->kv_qzero;
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            l->kv_qbuf[j] = (int8_t)(unsigned char)(int)val;
        }
        /* Quantize V cache */
        for (int64_t j = 0; j < count; j++) {
            double val = l->kv_cache_v->data[j] / l->kv_qscale + l->kv_qzero;
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            l->kv_qbuf[count + j] = (int8_t)(unsigned char)(int)val;
        }

        total_quant += l->kv_qlen;
    }
    return total_quant | (total_orig << 32);
}

} /* extern "C" */
