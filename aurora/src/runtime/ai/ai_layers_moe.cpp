#include "runtime/ai/ai_common.h"
#include "runtime/ai/ai_paged_attention.h"

extern "C" {

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
                out_vec[j] += sum * w;
            }
        }
    }
    if (training) {
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

    if (!l->dw) l->dw = aurora_tensor_new(l->w->ndim, l->w->shape);
    if (!l->hc_c) l->hc_c = aurora_tensor_new(l->hc_w->ndim, l->hc_w->shape);
    if (!l->dw || !l->hc_c) return nullptr;

    memset(l->dw->data, 0, (size_t)l->dw->total_size * sizeof(double));
    memset(l->hc_c->data, 0, (size_t)l->hc_c->total_size * sizeof(double));

    AuroraTensor* dx = aurora_tensor_new(input->ndim, input->shape);
    memset(dx->data, 0, (size_t)dx->total_size * sizeof(double));

    for (int64_t i = 0; i < n; i++) {
        int64_t off_base = i * k;

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

        double* expert_contrib = (double*)calloc((size_t)num_experts, sizeof(double));

        for (int64_t ki = 0; ki < k; ki++) {
            int64_t e = top_idx[ki];
            if (e < 0) continue;
            double w = cache_probs[i * num_experts + e];
            double* expert_w = &l->hc_w->data[e * d * hidden];
            double* in_row = &input->data[i * d];
            double* dout_row = &dout->data[i * hidden];

            for (int64_t h = 0; h < d; h++) {
                double in_val = in_row[h];
                if (in_val == 0.0) continue;
                double* hc_c_row = &l->hc_c->data[e * d * hidden + h * hidden];
                for (int64_t j = 0; j < hidden; j++)
                    hc_c_row[j] += w * in_val * dout_row[j] / (double)n;
            }

            for (int64_t h = 0; h < d; h++) {
                double sum = 0.0;
                for (int64_t j = 0; j < hidden; j++)
                    sum += expert_w[h * hidden + j] * dout_row[j];
                dx->data[i * d + h] += w * sum;
            }

            for (int64_t j = 0; j < hidden; j++) {
                double expert_out_j = 0.0;
                for (int64_t h = 0; h < d; h++)
                    expert_out_j += in_row[h] * expert_w[h * hidden + j];
                expert_contrib[e] += expert_out_j * dout_row[j];
            }
        }

        for (int64_t e = 0; e < num_experts; e++) {
            double p = cache_probs[i * num_experts + e];
            double weighted_avg = 0.0;
            for (int64_t f = 0; f < num_experts; f++)
                weighted_avg += cache_probs[i * num_experts + f] * expert_contrib[f];
            double dl_d_logit = p * (expert_contrib[e] - weighted_avg);

            for (int64_t h = 0; h < d; h++)
                l->dw->data[h * num_experts + e] += input->data[i * d + h] * dl_d_logit / (double)n;
        }

        free(expert_contrib);
    }

    if (l->cache) { aurora_tensor_free(l->cache); l->cache = nullptr; }

    return dx;
}

} /* extern "C" */
