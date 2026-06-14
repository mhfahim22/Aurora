#include "runtime/ai/ai_common.h"
#include "runtime/ai/ai_paged_attention.h"

extern "C" {

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
        l->units = d;
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
        int64_t ws[2] = { d, hidden };
        l->w = aurora_tensor_new(2, ws);
        for (int64_t i = 0; i < l->w->total_size; i++) l->w->data[i] = rand_uniform() * scale;
        int64_t bs[2] = { 1, hidden };
        l->b = aurora_tensor_new(2, bs);
        memset(l->b->data, 0, (size_t)l->b->total_size * sizeof(double));
        l->hc_w = aurora_tensor_new(2, ws);
        for (int64_t i = 0; i < l->hc_w->total_size; i++) l->hc_w->data[i] = rand_uniform() * scale;
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

    /* Bias addition */
    if (l->dtype == TENSOR_F32 && z->dtype == TENSOR_F32) {
        for (int64_t i = 0; i < z->total_size; i++)
            z->data_f32[i] += l->b->data_f32[i % l->b->total_size];
    } else {
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

    if (l->lora_enabled && l->lora_A && l->lora_B && l->lora_r > 0 && l->lora_A_grad && l->lora_B_grad) {
        double lora_scale = l->lora_alpha / (double)l->lora_r;
        AuroraTensor* dout_f64_2 = (dout->dtype == TENSOR_F64) ? dout : aurora_tensor_to_f64(dout);
        AuroraTensor* input_f64_2 = (input->dtype == TENSOR_F64) ? input : aurora_tensor_to_f64(input);

        AuroraTensor* xA = aurora_tensor_matmul(input_f64_2, l->lora_A);

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

} /* extern "C" */
