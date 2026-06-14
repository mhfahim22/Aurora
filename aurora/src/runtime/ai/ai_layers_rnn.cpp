#include "runtime/ai/ai_common.h"
#include "runtime/ai/ai_paged_attention.h"

extern "C" {

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
        l->kv_cache_v = h_prev;
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

            double dh = dout_row[j];

            double do_val = dh * tanh_cn;
            dg[2 * units + j] = do_val * o * (1.0 - o);

            double dtanh = dh * o;
            double dc = dtanh * (1.0 - tanh_cn * tanh_cn);

            double di = dc * g_candidate;
            dg[j] = di * i * (1.0 - i);

            double dg_candidate = dc * i;
            dg[3 * units + j] = dg_candidate * (1.0 - g_candidate * g_candidate);

            double df = dc * c_prev_val;
            dg[units + j] = df * f * (1.0 - f);
        }
    }

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

            double dz = dh * (h_prev_val - n);
            dg[units + j] = dz * z * (1.0 - z);

            double dn = dh * (1.0 - z);
            double dn_gate = dn * (1.0 - n * n);
            dg[2 * units + j] = dn_gate;

            double dr = dn_gate * h_prev_val;
            dg[j] = dr * r * (1.0 - r);
        }
    }

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

} /* extern "C" */
