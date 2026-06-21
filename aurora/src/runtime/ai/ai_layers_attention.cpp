#include "runtime/ai/ai_common.h"
#include "runtime/ai/ai_paged_attention.h"

extern "C" {

/* === Element-wise Multiply Layer (for SwiGLU gating) === */
AuroraTensor* mul_forward(Layer* l, AuroraTensor* input, int training) {
    (void)training;
    if (!input) return nullptr;
    int64_t half = input->total_size / 2;
    AuroraTensor* out = aurora_tensor_new(input->ndim, input->shape);
    if (!out) return nullptr;
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
        if (l->cache)
            memcpy(l->cache->data, input->data, (size_t)input->total_size * sizeof(double));
    }
    return out;
}

AuroraTensor* mul_backward(Layer* l, AuroraTensor* dout, AuroraTensor* input) {
    (void)input;
    if (!dout || !l->cache) return nullptr;
    int64_t half = l->cache->total_size / 2;
    AuroraTensor* dx = aurora_tensor_new(l->cache->ndim, l->cache->shape);
    if (!dx) return nullptr;
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

#define FLASH_BLOCK 64

AuroraTensor* mha_forward(Layer* l, AuroraTensor* input, int training) {
    if (!input || input->ndim < 2) return nullptr;
    int64_t n = input->shape[0], d = input->shape[1];
    int64_t h = l->num_heads > 0 ? l->num_heads : 4;
    int64_t d_h = d / h;
    if (d_h < 1) d_h = 1;
    int dt = l->dtype;

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

    AuroraTensor* qkv = aurora_tensor_matmul(input, l->w);
    if (!qkv) return nullptr;
    if (qkv->dtype == TENSOR_F32) {
        for (int64_t i = 0; i < qkv->total_size; i++)
            qkv->data_f32[i] += l->b->data_f32[i % (3 * d)];
    } else {
        for (int64_t i = 0; i < qkv->total_size; i++)
            qkv->data[i] += l->b->data[i % (3 * d)];
    }

    if (!training) {
        int64_t new_n = n;

        if (g_active_paged_mgr) {
            PagedManager* pm = g_active_paged_mgr;
            int64_t seq_id = pm->current_seq_id;
            AuroraTensor* out = (dt == TENSOR_F32)
                ? aurora_tensor_new_f32(input->ndim, input->shape)
                : aurora_tensor_new(input->ndim, input->shape);
            if (!out) { aurora_tensor_free(qkv); return nullptr; }
            double scale = 1.0 / sqrt((double)d_h);
            double scale = 1.0 / sqrt((double)d_h);
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
                        if (ci > cache_n + ri) break;
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

    double inv_scale_d = 1.0 / sqrt((double)d_h);
    float  inv_scale_f = 1.0f / sqrtf((float)d_h);

    if (!l->flash_m) l->flash_m = (double*)calloc((size_t)(h * n), sizeof(double));
    if (!l->flash_d) l->flash_d = (double*)calloc((size_t)(h * n), sizeof(double));

    AuroraTensor* out = l_tensor(l, input->ndim, input->shape);
    if (!out) { aurora_tensor_free(qkv); return nullptr; }
    memset(out->data_ptr, 0, (size_t)out->total_size * ((dt == TENSOR_F32) ? sizeof(float) : sizeof(double)));

    int64_t B_c = FLASH_BLOCK;
    int64_t B_r = FLASH_BLOCK;

    for (int64_t hi = 0; hi < h; hi++) {
        int64_t ho = hi * d_h;
        int64_t base = hi * n;

        for (int64_t i = 0; i < n; i++) {
            l->flash_m[base + i] = -1e18;
            l->flash_d[base + i] = 0.0;
        }

        for (int64_t j = 0; j < n; j += B_c) {
            int64_t j_end = (j + B_c < n) ? j + B_c : n;

            int64_t q_start = j;
            for (int64_t qi = q_start; qi < n; qi += B_r) {
                int64_t qi_end = (qi + B_r < n) ? qi + B_r : n;

                if (dt == TENSOR_F32) {
                    for (int64_t i = qi; i < qi_end; i++) {
                        if (j > i) continue;
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

    AuroraTensor* final = aurora_tensor_matmul(out, l->hc_w);
    if (final && final->dtype == TENSOR_F32) {
        for (int64_t i = 0; i < final->total_size; i++)
            final->data_f32[i] += l->hc_b->data_f32[i % d];
    } else if (final) {
        for (int64_t i = 0; i < final->total_size; i++)
            final->data[i] += l->hc_b->data[i % d];
    }

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

    if (!l->hc_c) { int64_t ws[2] = { d, d }; l->hc_c = aurora_tensor_new(2, ws); }
    memset(l->hc_c->data, 0, (size_t)d * d * sizeof(double));
    for (int64_t i = 0; i < d; i++)
        for (int64_t j = 0; j < d; j++) {
            double sum = 0.0;
            for (int64_t k = 0; k < n; k++)
                sum += attn_out->data[k * d + i] * dout->data[k * d + j];
            l->hc_c->data[i * d + j] = sum / (double)n;
        }

    double* d_attn = (double*)malloc((size_t)n * d * sizeof(double));
    if (!d_attn) return nullptr;
    for (int64_t i = 0; i < n; i++)
        for (int64_t j = 0; j < d; j++) {
            double sum = 0.0;
            for (int64_t k = 0; k < d; k++)
                sum += dout->data[i * d + k] * l->hc_w->data[j * d + k];
            d_attn[i * d + j] = sum;
        }

    double* dqkv = (double*)calloc((size_t)n * 3 * d, sizeof(double));
    if (!dqkv) { free(d_attn); return nullptr; }

    for (int64_t h = 0; h < nh; h++) {
        int64_t ho = h * hd;

        for (int64_t b = 0; b < n; b++) {
            double* Qb = &qkv->data[b * 3 * d + ho];

            double row_max = -1e18;
            double row_sum = 0.0;
            double scores[FLASH_BLOCK];
            int64_t cols_this_row = b + 1;

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
                    for (int64_t k = 0; k < hd; k++)
                        dqkv[t * 3 * d + 2 * d + ho + k] += p * d_attn[b * d + ho + k];
                }
            }
        }

        for (int64_t b = 0; b < n; b++) {
            double* Qb = &qkv->data[b * 3 * d + ho];

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
            for (int64_t t = 0; t <= b; t++) scores[t] *= inv_sum;

            double dP[FLASH_BLOCK];
            memset(dP, 0, (size_t)(b + 1) * sizeof(double));
            for (int64_t t = 0; t <= b; t++) {
                double* Vt = &qkv->data[t * 3 * d + 2 * d + ho];
                double sum = 0.0;
                for (int64_t k = 0; k < hd; k++)
                    sum += d_attn[b * d + ho + k] * Vt[k];
                dP[t] = sum;
            }

            double dot_soft = 0.0;
            for (int64_t t = 0; t <= b; t++) dot_soft += scores[t] * dP[t];

            for (int64_t t = 0; t <= b; t++) {
                double ds = scores[t] * (dP[t] - dot_soft);
                double* Kt = &qkv->data[t * 3 * d + d + ho];
                for (int64_t k = 0; k < hd; k++)
                    dqkv[b * 3 * d + ho + k] += ds * Kt[k] * inv_scale;
                for (int64_t k = 0; k < hd; k++)
                    dqkv[t * 3 * d + d + ho + k] += ds * Qb[k] * inv_scale;
            }
        }
    }

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
    Layer* save1 = &m->layers[m->n_layers++];
    memset(save1, 0, sizeof(Layer)); save1->type = LAYER_RESIDUAL_SAVE;
    Layer* ln1 = &m->layers[m->n_layers++];
    memset(ln1, 0, sizeof(Layer)); ln1->type = LAYER_LAYERNORM; ln1->units = embed_dim;
    Layer* attn = &m->layers[m->n_layers++];
    memset(attn, 0, sizeof(Layer)); attn->type = LAYER_ATTENTION; attn->units = embed_dim;
    attn->num_heads = num_heads; attn->embed_dim = embed_dim;
    Layer* add1 = &m->layers[m->n_layers++];
    memset(add1, 0, sizeof(Layer)); add1->type = LAYER_RESIDUAL_ADD;
    add1->residual_layer = start;
    Layer* save2 = &m->layers[m->n_layers++];
    memset(save2, 0, sizeof(Layer)); save2->type = LAYER_RESIDUAL_SAVE;
    Layer* ln2 = &m->layers[m->n_layers++];
    memset(ln2, 0, sizeof(Layer)); ln2->type = LAYER_LAYERNORM; ln2->units = embed_dim;
    Layer* ff1 = &m->layers[m->n_layers++];
    memset(ff1, 0, sizeof(Layer)); ff1->type = LAYER_DENSE; ff1->units = ff_dim; ff1->activation = ACT_RELU;
    Layer* ff2 = &m->layers[m->n_layers++];
    memset(ff2, 0, sizeof(Layer)); ff2->type = LAYER_DENSE; ff2->units = embed_dim; ff2->activation = ACT_LINEAR;
    Layer* add2 = &m->layers[m->n_layers++];
    memset(add2, 0, sizeof(Layer)); add2->type = LAYER_RESIDUAL_ADD;
    add2->residual_layer = start + 4;
    return start;
}

} /* extern "C" */
