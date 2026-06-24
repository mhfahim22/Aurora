#include "runtime/ai/ai_common.h"
#include "runtime/ai/ai_paged_attention.h"

/* Global active paged manager — set by model_forward during inference */
PagedManager* g_active_paged_mgr = nullptr;

extern "C" {

/* Helpers l_tensor/l_grad_tensor moved to ai_common.h */

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
    if (!out) return nullptr;
    if (l->cache) aurora_tensor_free(l->cache);
    l->cache = aurora_tensor_new(1, &nr);
    if (!l->cache) { aurora_tensor_free(out); return nullptr; }
    if (l->running_mean) aurora_tensor_free(l->running_mean);
    l->running_mean = aurora_tensor_new(1, &nr);
    if (!l->running_mean) { aurora_tensor_free(out); return nullptr; }
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
    if (!dx) return nullptr;
    for (int64_t r = 0; r < nr; r++) {
        double mn = l->cache ? l->cache->data[r] : 0.0;
        double inv_std = l->running_mean ? l->running_mean->data[r] : 1.0;
        for (int64_t j = 0; j < nf; j++) {
            double x = input->data[r * nf + j];
            double d = dout->data[r * nf + j];
            double xnorm = (x - mn) * inv_std;
            /* gamma grad */
            l->dw->data[j] += d * xnorm;
            /* beta grad */
            l->db->data[j] += d;
            /* dx: sum across features of gradient contributions */
            dx->data[r * nf + j] = d * l->w->data[j] * inv_std;
            double sum_d = 0.0, sum_dx = 0.0;
            for (int64_t k = 0; k < nf; k++) {
                double xk = input->data[r * nf + k];
                double dk = dout->data[r * nf + k];
                sum_d += dk * l->w->data[k];
                sum_dx += dk * l->w->data[k] * (xk - mn);
            }
            double variance = 1.0 / (inv_std * inv_std);
            dx->data[r * nf + j] -= (sum_d / (double)nf + (x - mn) * sum_dx / ((double)nf * variance));
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
    if (!out) return nullptr;
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
    if (!out) return nullptr;
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
    if (!dx) return nullptr;
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
    if (!out) return nullptr;
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
    if (!dx) return nullptr;
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
    if (!out) return nullptr;
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
    if (!col) return nullptr;
    im2col(input->data, batch, h, w, c, kh, kw, sh, sw, col->data, oh, ow);
    AuroraTensor* out = aurora_tensor_matmul(col, l->w);
    if (!out) { aurora_tensor_free(col); return nullptr; }
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
    if (!dx_col) { aurora_tensor_free(col); return nullptr; }
    int64_t in_shape[4] = { batch, h, w, c };
    AuroraTensor* dx = aurora_tensor_new(4, in_shape);
    if (!dx) { aurora_tensor_free(dx_col); aurora_tensor_free(col); return nullptr; }
    col2im(dx_col->data, batch, h, w, c, kh, kw, sh, sw, dx->data, oh, ow);
    aurora_tensor_free(dx_col);
    aurora_tensor_free(col);
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
