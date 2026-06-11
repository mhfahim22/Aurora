#include "runtime/ai/ai_common.h"
#include "runtime/ai/ai_distributed.h"

OptimState g_optims[8];
int g_n_optims = 0;

extern "C" {

#pragma optimize("", off)
double loss_compute(AuroraTensor* pred, AuroraTensor* target, int loss_type, AuroraTensor* dloss) {
    if (!pred || !target || pred->total_size == 0) return 0.0;
    double loss = 0.0;
    int64_t n = pred->total_size;
    switch (loss_type) {
        case LOSS_MSE:
            for (int64_t i = 0; i < n; i++) {
                double diff = pred->data[i] - target->data[i];
                loss += diff * diff;
                if (dloss) dloss->data[i] = 2.0 * diff / (double)n;
            }
            loss /= (double)n;
            break;
        case LOSS_BINARY_CROSS_ENTROPY: {
            double eps = 1e-15;
            for (int64_t i = 0; i < n; i++) {
                double p = fmax(fmin(pred->data[i], 1.0 - eps), eps);
                loss -= target->data[i] * log(p) + (1.0 - target->data[i]) * log(1.0 - p);
                if (dloss) dloss->data[i] = (p - target->data[i]);
            }
            break;
        }
        case LOSS_CATEGORICAL_CROSS_ENTROPY: {
            double eps = 1e-15;
            for (int64_t i = 0; i < n; i++) {
                double p = fmax(pred->data[i], eps);
                loss -= target->data[i] * log(p);
                if (dloss) dloss->data[i] = (pred->data[i] - target->data[i]);
            }
            break;
        }
    }
    return loss;
}
#pragma optimize("", on)

/* === Metrics === */
double metric_accuracy(AuroraTensor* pred, AuroraTensor* target) {
    if (!pred || !target || pred->shape[0] == 0) return 0.0;
    int64_t correct = 0, n = pred->shape[0];
    if (pred->ndim == 1 || (pred->ndim >= 2 && pred->shape[1] == 1)) {
        for (int64_t i = 0; i < n; i++) {
            double p = pred->data[i] > 0.5 ? 1.0 : 0.0;
            if (fabs(p - target->data[i]) < 0.5) correct++;
        }
    } else {
        int64_t nc = pred->shape[1];
        for (int64_t i = 0; i < n; i++) {
            int64_t pc = 0, tc = 0;
            double pv = pred->data[i * nc], tv = target->data[i * nc];
            for (int64_t j = 1; j < nc; j++) {
                if (pred->data[i * nc + j] > pv) { pv = pred->data[i * nc + j]; pc = j; }
                if (target->data[i * nc + j] > tv) { tv = target->data[i * nc + j]; tc = j; }
            }
            if (pc == tc) correct++;
        }
    }
    return (double)correct / (double)n;
}

int activation_from_name(const char* name) {
    if (!name) return ACT_SIGMOID;
    if (strcmp(name, "relu") == 0) return ACT_RELU;
    if (strcmp(name, "sigmoid") == 0) return ACT_SIGMOID;
    if (strcmp(name, "tanh") == 0) return ACT_TANH;
    if (strcmp(name, "leaky_relu") == 0) return ACT_LEAKY_RELU;
    if (strcmp(name, "softmax") == 0) return ACT_SOFTMAX;
    if (strcmp(name, "linear") == 0) return ACT_LINEAR;
    if (strcmp(name, "silu") == 0) return ACT_SILU;
    if (strcmp(name, "swish") == 0) return ACT_SILU;
    if (strcmp(name, "gelu") == 0) return ACT_GELU;
    return ACT_SIGMOID;
}

int loss_from_name(const char* name) {
    if (!name) return LOSS_MSE;
    if (strcmp(name, "mse") == 0) return LOSS_MSE;
    if (strcmp(name, "binary_crossentropy") == 0) return LOSS_BINARY_CROSS_ENTROPY;
    if (strcmp(name, "categorical_crossentropy") == 0) return LOSS_CATEGORICAL_CROSS_ENTROPY;
    return LOSS_MSE;
}

int optim_from_name(const char* name) {
    if (!name) return OPTIM_ADAM;
    if (strcmp(name, "adam") == 0) return OPTIM_ADAM;
    if (strcmp(name, "sgd") == 0) return OPTIM_SGD;
    if (strcmp(name, "rmsprop") == 0) return OPTIM_RMSPROP;
    return OPTIM_ADAM;
}

/* === Optimizer === */
void optim_add_optim(int type, int64_t param_count, double lr) {
    if (g_n_optims >= 8) return;
    OptimState* o = &g_optims[g_n_optims++];
    memset(o, 0, sizeof(OptimState));
    o->type = type; o->lr = lr; o->beta1 = 0.9; o->beta2 = 0.999; o->epsilon = 1e-8;
    o->momentum = 0.9; o->param_count = param_count; o->t = 1;
    if (type == OPTIM_ADAM || type == OPTIM_RMSPROP) {
        o->m = (double*)calloc((size_t)param_count, sizeof(double));
        o->v = (double*)calloc((size_t)param_count, sizeof(double));
    } else {
        o->m = (double*)calloc((size_t)param_count, sizeof(double));
    }
}

void optim_update(OptimState* o, double* params, double* grads) {
    optim_update_offset(o, params, grads, 0, o->param_count);
}

#pragma optimize("", off)
void optim_update_offset(OptimState* o, double* params, double* grads, int64_t offset, int64_t n) {
    if (!o || !params || !grads) return;
    double lr = o->lr / (1.0 + o->decay * (double)o->t);
    switch (o->type) {
        case OPTIM_SGD:
            for (int64_t i = 0; i < n; i++) {
                o->m[offset + i] = o->momentum * o->m[offset + i] + lr * grads[i];
                params[i] -= o->m[offset + i];
            }
            break;
        case OPTIM_ADAM:
            for (int64_t i = 0; i < n; i++) {
                o->m[offset + i] = o->beta1 * o->m[offset + i] + (1.0 - o->beta1) * grads[i];
                o->v[offset + i] = o->beta2 * o->v[offset + i] + (1.0 - o->beta2) * grads[i] * grads[i];
                double mh = o->m[offset + i] / (1.0 - pow(o->beta1, (double)o->t));
                double vh = o->v[offset + i] / (1.0 - pow(o->beta2, (double)o->t));
                params[i] -= lr * mh / (sqrt(vh) + o->epsilon);
            }
            o->t++;
            break;
        case OPTIM_RMSPROP:
            for (int64_t i = 0; i < n; i++) {
                o->v[offset + i] = o->beta2 * o->v[offset + i] + (1.0 - o->beta2) * grads[i] * grads[i];
                params[i] -= lr * grads[i] / (sqrt(o->v[offset + i]) + o->epsilon);
            }
            break;
    }
}
#pragma optimize("", on)

/* === Distributed Gradient Synchronization === */
void distributed_sync_gradients(Model* m) {
    if (g_comm.world_size <= 1) return;
    for (int i = 0; i < m->n_layers; i++) {
        Layer* l = &m->layers[i];
        if (l->dw) comm_allreduce(l->dw->data, l->dw->total_size);
        if (l->db) comm_allreduce(l->db->data, l->db->total_size);
        if (l->hc_c) comm_allreduce(l->hc_c->data, l->hc_c->total_size);
        if (l->hc_d) comm_allreduce(l->hc_d->data, l->hc_d->total_size);
    }
}

void distributed_broadcast_weights(Model* m, int root) {
    if (g_comm.world_size <= 1) return;
    for (int i = 0; i < m->n_layers; i++) {
        Layer* l = &m->layers[i];
        if (l->w) comm_broadcast(l->w->data, l->w->total_size, root);
        if (l->b) comm_broadcast(l->b->data, l->b->total_size, root);
        if (l->hc_w) comm_broadcast(l->hc_w->data, l->hc_w->total_size, root);
        if (l->hc_b) comm_broadcast(l->hc_b->data, l->hc_b->total_size, root);
    }
}

/* === Gradient Clipping === */
void clip_gradients(Model* m, double max_norm) {
    if (max_norm <= 0.0) return;
    double total_norm = 0.0;
    for (int i = 0; i < m->n_layers; i++) {
        if (m->layers[i].dw) {
            for (int64_t j = 0; j < m->layers[i].dw->total_size; j++)
                total_norm += m->layers[i].dw->data[j] * m->layers[i].dw->data[j];
        }
        if (m->layers[i].hc_c) {
            for (int64_t j = 0; j < m->layers[i].hc_c->total_size; j++)
                total_norm += m->layers[i].hc_c->data[j] * m->layers[i].hc_c->data[j];
        }
        if (m->layers[i].hc_d) {
            for (int64_t j = 0; j < m->layers[i].hc_d->total_size; j++)
                total_norm += m->layers[i].hc_d->data[j] * m->layers[i].hc_d->data[j];
        }
        if (m->layers[i].lora_A_grad) {
            for (int64_t j = 0; j < m->layers[i].lora_A_grad->total_size; j++)
                total_norm += m->layers[i].lora_A_grad->data[j] * m->layers[i].lora_A_grad->data[j];
        }
        if (m->layers[i].lora_B_grad) {
            for (int64_t j = 0; j < m->layers[i].lora_B_grad->total_size; j++)
                total_norm += m->layers[i].lora_B_grad->data[j] * m->layers[i].lora_B_grad->data[j];
        }
    }
    total_norm = sqrt(total_norm);
    if (total_norm > max_norm) {
        double scale = max_norm / total_norm;
        for (int i = 0; i < m->n_layers; i++) {
            if (m->layers[i].dw)
                for (int64_t j = 0; j < m->layers[i].dw->total_size; j++)
                    m->layers[i].dw->data[j] *= scale;
            if (m->layers[i].hc_c)
                for (int64_t j = 0; j < m->layers[i].hc_c->total_size; j++)
                    m->layers[i].hc_c->data[j] *= scale;
            if (m->layers[i].hc_d)
                for (int64_t j = 0; j < m->layers[i].hc_d->total_size; j++)
                    m->layers[i].hc_d->data[j] *= scale;
            if (m->layers[i].lora_A_grad)
                for (int64_t j = 0; j < m->layers[i].lora_A_grad->total_size; j++)
                    m->layers[i].lora_A_grad->data[j] *= scale;
            if (m->layers[i].lora_B_grad)
                for (int64_t j = 0; j < m->layers[i].lora_B_grad->total_size; j++)
                    m->layers[i].lora_B_grad->data[j] *= scale;
        }
    }
}

/* === Data Augmentation === */
void augment_data(AuroraTensor* data, int64_t nf, int64_t ns) {
    for (int64_t i = 0; i < ns; i++)
        for (int64_t j = 0; j < nf; j++)
            data->data[i * (nf + 1) + j] += randn() * 0.01;
}

/* === Learning Rate Scheduling === */
double lr_reduce_on_plateau(double current_lr, double factor, double min_lr) {
    double new_lr = current_lr * factor;
    return new_lr > min_lr ? new_lr : min_lr;
}

/* === Forward helper: run layers 0..idx-1 and return intermediate output === */
#pragma optimize("", off)
static AuroraTensor* forward_to_layer(Model* m, AuroraTensor* X, int idx) {
    if (idx <= 0) return X;
    AuroraTensor* cur = aurora_tensor_new(X->ndim, X->shape);
    memcpy(cur->data, X->data, (size_t)X->total_size * sizeof(double));
    for (int j = 0; j < idx; j++) {
        Layer* l = &m->layers[j];
        AuroraTensor* nxt = nullptr;
        switch (l->type) {
            case LAYER_DENSE: nxt = dense_forward(l, cur, 0); break;
            case LAYER_ATTENTION: nxt = mha_forward(l, cur, 1); break;
            case LAYER_LAYERNORM: nxt = layernorm_forward(l, cur, 0); break;
            case LAYER_EMBEDDING: nxt = embedding_forward(l, cur, 0); break;
            case LAYER_POS_ENCODING: nxt = pos_encoding_forward(l, cur, 0); break;
            case LAYER_ROPE: nxt = rope_forward(l, cur, 0); break;
            case LAYER_MUL: nxt = mul_forward(l, cur, 0); break;
            case LAYER_MOE: nxt = moe_forward(l, cur, 0); break;
            case LAYER_SWIGLU: nxt = swiglu_forward(l, cur, 1); break;
            case LAYER_UNEMBED: nxt = unembed_forward(l, cur, 0); break;
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
                    for (int64_t k = 0; k < cur->total_size && k < save->residual->total_size; k++)
                        cur->data[k] += save->residual->data[k];
                }
                nxt = cur; cur = nullptr;
                break;
            }
            default:
                nxt = aurora_tensor_new(cur->ndim, cur->shape);
                memcpy(nxt->data, cur->data, (size_t)cur->total_size * sizeof(double));
                break;
        }
        if (cur != X) aurora_tensor_free(cur);
        if (!nxt) return nullptr;
        cur = nxt;
    }
    return cur;
}
#pragma optimize("", on)

/* === Gradient Checkpointing: forward from nearest checkpoint to layer idx ===
   The forward pass saved checkpoint activations every checkpoint_freq layers.
   This helper finds the nearest checkpoint ≤ idx and runs forward from there,
   repopulating per-layer caches for the backward pass. */
static AuroraTensor* forward_from_checkpoint(Model* m, AuroraTensor* X, int idx) {
    if (idx <= 0 || m->checkpoint_freq <= 0) return forward_to_layer(m, X, idx);

    /* Find nearest checkpoint ≤ idx */
    int ckpt_start = 0;
    AuroraTensor* cur = aurora_tensor_new(X->ndim, X->shape);
    memcpy(cur->data, X->data, (size_t)X->total_size * sizeof(double));
    for (int ci = 0; ci < m->n_checkpoints; ci++) {
        if (m->checkpoint_layers[ci] <= idx) {
            ckpt_start = m->checkpoint_layers[ci];
            memcpy(cur->data, m->checkpoint_acts[ci]->data, (size_t)cur->total_size * sizeof(double));
        }
    }

    /* Run forward from checkpoint to idx-1 (skip layers already computed) */
    for (int j = ckpt_start; j < idx; j++) {
        Layer* l = &m->layers[j];
        AuroraTensor* nxt = nullptr;
        switch (l->type) {
            case LAYER_DENSE: nxt = dense_forward(l, cur, 0); break;
            case LAYER_ATTENTION: nxt = mha_forward(l, cur, 1); break;
            case LAYER_LAYERNORM: nxt = layernorm_forward(l, cur, 0); break;
            case LAYER_EMBEDDING: nxt = embedding_forward(l, cur, 0); break;
            case LAYER_POS_ENCODING: nxt = pos_encoding_forward(l, cur, 0); break;
            case LAYER_ROPE: nxt = rope_forward(l, cur, 0); break;
            case LAYER_MUL: nxt = mul_forward(l, cur, 0); break;
            case LAYER_MOE: nxt = moe_forward(l, cur, 0); break;
            case LAYER_SWIGLU: nxt = swiglu_forward(l, cur, 1); break;
            case LAYER_UNEMBED: nxt = unembed_forward(l, cur, 0); break;
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
                    for (int64_t k = 0; k < cur->total_size && k < save->residual->total_size; k++)
                        cur->data[k] += save->residual->data[k];
                }
                nxt = cur; cur = nullptr;
                break;
            }
            default:
                nxt = aurora_tensor_new(cur->ndim, cur->shape);
                memcpy(nxt->data, cur->data, (size_t)cur->total_size * sizeof(double));
                break;
        }
        if (cur != X) aurora_tensor_free(cur);
        if (!nxt) return nullptr;
        cur = nxt;
    }
    return cur;
}

/* === Mini-batch training engine === */
double train_batch(Model* m, AuroraTensor* X, AuroraTensor* y, OptimState* opt) {
    if (!m || !X || !y) return 0.0;
    /* Clear residual gradient accumulators */
    for (int i = 0; i < m->n_layers; i++) {
        if (m->layers[i].residual_dout) {
            aurora_tensor_free(m->layers[i].residual_dout);
            m->layers[i].residual_dout = nullptr;
        }
    }

    AuroraTensor* out = model_forward(m, X, 1);
    if (!out) return 1e9;

    /* PP: forward communication — receive from previous stage */
    if (m->parallel_strategy == PARALLEL_PP) {
        pp_forward_stage(out->data, out->data, out->total_size,
                         m->pp_stage_rank, m->pp_group_size);
    }

    AuroraTensor* dl = aurora_tensor_new(out->ndim, out->shape);
    double loss = loss_compute(out, y, m->loss_type, dl);
    /* Add MoE auxiliary load-balancing loss */
    if (m->moe_aux_weight > 0.0) loss += m->moe_aux_weight * m->moe_aux_loss;
    AuroraTensor* dout = dl;

    /* PP: backward communication — receive gradient from next stage */
    if (m->parallel_strategy == PARALLEL_PP) {
        pp_backward_stage(dout->data, dout->data, dout->total_size,
                          m->pp_stage_rank, m->pp_group_size);
    }

    for (int i = m->n_layers - 1; i >= 0; i--) {
        Layer* l = &m->layers[i];
        AuroraTensor* dp = nullptr;
        AuroraTensor* layer_in = (i == 0) ? X : forward_from_checkpoint(m, X, i);
        if (l->type == LAYER_DENSE) {
            dp = dense_backward(l, dout, layer_in);
        } else if (l->type == LAYER_CONV) {
            dp = conv2d_backward(l, dout, layer_in);
        } else if (l->type == LAYER_LSTM) {
            dp = lstm_backward(l, dout, layer_in);
        } else if (l->type == LAYER_GRU) {
            dp = gru_backward(l, dout, layer_in);
        } else if (l->type == LAYER_ATTENTION) {
            dp = mha_backward(l, dout, layer_in);
        } else if (l->type == LAYER_LAYERNORM) {
            dp = layernorm_backward(l, dout, layer_in);
        } else if (l->type == LAYER_EMBEDDING) {
            dp = embedding_backward(l, dout, layer_in);
        } else if (l->type == LAYER_POS_ENCODING) {
            dp = pos_encoding_backward(l, dout, layer_in);
        } else if (l->type == LAYER_ROPE) {
            dp = rope_backward(l, dout, layer_in);
        } else if (l->type == LAYER_MUL) {
            dp = mul_backward(l, dout, layer_in);
        } else if (l->type == LAYER_MOE) {
            dp = moe_backward(l, dout, layer_in);
        } else if (l->type == LAYER_SWIGLU) {
            dp = swiglu_backward(l, dout, layer_in);
        } else if (l->type == LAYER_UNEMBED) {
            dp = unembed_backward(l, dout, layer_in);
        } else if (l->type == LAYER_RESIDUAL_ADD) {
            /* Gradient flows equally to both branches */
            /* Accumulate gradient to the SAVE layer's residual_dout */
            Layer* save = &m->layers[l->residual_layer];
            if (!save->residual_dout) {
                save->residual_dout = aurora_tensor_new(dout->ndim, dout->shape);
                memset(save->residual_dout->data, 0, (size_t)dout->total_size * sizeof(double));
            }
            for (int64_t j = 0; j < dout->total_size; j++)
                save->residual_dout->data[j] += dout->data[j];
            dp = aurora_tensor_new(dout->ndim, dout->shape);
            memcpy(dp->data, dout->data, (size_t)dout->total_size * sizeof(double));
        } else if (l->type == LAYER_RESIDUAL_SAVE) {
            /* Sum incoming gradient with accumulated residual gradient */
            if (l->residual_dout) {
                for (int64_t j = 0; j < dout->total_size && j < l->residual_dout->total_size; j++)
                    dout->data[j] += l->residual_dout->data[j];
            }
            dp = aurora_tensor_new(dout->ndim, dout->shape);
            memcpy(dp->data, dout->data, (size_t)dout->total_size * sizeof(double));
        } else {
            dp = aurora_tensor_new(dout->ndim, dout->shape);
            memcpy(dp->data, dout->data, (size_t)dout->total_size * sizeof(double));
        }
        if (layer_in != X) aurora_tensor_free(layer_in);
        if (dout != dl) aurora_tensor_free(dout);
        dout = dp;
        if (!dout) break;
    }
    if (dout && dout != dl) aurora_tensor_free(dout);
    aurora_tensor_free(dl);

    /* Distributed: sync gradients across workers (only for DP — TP/PP ranks hold exclusive shards) */
    if (m->parallel_strategy != PARALLEL_TP && m->parallel_strategy != PARALLEL_PP) {
        distributed_sync_gradients(m);
    }

    /* Gradient clipping */
    if (m->gradient_clip > 0.0) clip_gradients(m, m->gradient_clip);

    /* Apply gradients */
    if (opt) {
        int64_t off = 0;
        for (int i = 0; i < m->n_layers; i++) {
            Layer* l = &m->layers[i];
            if (l->w && l->dw) { optim_update_offset(opt, l->w->data, l->dw->data, off, l->w->total_size); off += l->w->total_size; }
            if (l->b && l->db) { optim_update_offset(opt, l->b->data, l->db->data, off, l->b->total_size); off += l->b->total_size; }
            if (l->hc_w && l->hc_c) { optim_update_offset(opt, l->hc_w->data, l->hc_c->data, off, l->hc_w->total_size); off += l->hc_w->total_size; }
            if (l->hc_b && l->hc_d) { optim_update_offset(opt, l->hc_b->data, l->hc_d->data, off, l->hc_b->total_size); off += l->hc_b->total_size; }
            if (l->lora_A && l->lora_A_grad) { optim_update_offset(opt, l->lora_A->data, l->lora_A_grad->data, off, l->lora_A->total_size); off += l->lora_A->total_size; }
            if (l->lora_B && l->lora_B_grad) { optim_update_offset(opt, l->lora_B->data, l->lora_B_grad->data, off, l->lora_B->total_size); off += l->lora_B->total_size; }
        }
    }
    aurora_tensor_free(out);
    return loss;
}

/* === Dense Layer === */
int model_add_dense(Model* m, int64_t input_size, int64_t units, int activation) {
    if (m->n_layers >= 128) return 0;
    Layer* l = &m->layers[m->n_layers++];
    memset(l, 0, sizeof(Layer));
    l->type = LAYER_DENSE; l->units = units; l->activation = activation; l->epsilon = 1e-8;
    int64_t ws[2] = { input_size, units };
    l->w = aurora_tensor_new(2, ws);
    double scale = sqrt(2.0 / (double)(input_size + units));
    for (int64_t i = 0; i < l->w->total_size; i++) l->w->data[i] = rand_uniform() * scale;
    int64_t bs[2] = { 1, units };
    l->b = aurora_tensor_new(2, bs);
    memset(l->b->data, 0, (size_t)l->b->total_size * sizeof(double));
    m->total_params += l->w->total_size + l->b->total_size;
    return 1;
}

/* === Model auto-setup === */
int model_auto_setup(Model* m, int64_t data_ptr) {
    AuroraTensor* data = (AuroraTensor*)data_ptr;
    if (!m || !data || data->ndim < 2 || data->ndim > 10) return 0;
    if (m->n_layers > 0) return 1;
    int64_t nf = data->shape[1] - 1;
    int64_t h1 = nf > 64 ? 64 : (nf > 16 ? 32 : 16);
    int64_t h2 = h1 > 4 ? h1 / 2 : 4;
    model_add_dense(m, nf, h1, ACT_RELU);
    model_add_dense(m, h1, h2, ACT_RELU);
    if (m->loss_type == LOSS_BINARY_CROSS_ENTROPY)
        model_add_dense(m, h2, 1, ACT_SIGMOID);
    else if (m->loss_type == LOSS_CATEGORICAL_CROSS_ENTROPY)
        model_add_dense(m, h2, data->shape[1] > nf + 1 ? data->shape[1] - nf - 1 : 1, ACT_SOFTMAX);
    else model_add_dense(m, h2, 1, ACT_LINEAR);
    return 1;
}

/* === Data Processing === */
int tensor_is_valid_row(AuroraTensor* t, int64_t row) {
    int64_t cols = t->shape[1];
    for (int64_t c = 0; c < cols; c++) {
        int64_t idx[2] = { row, c };
        double v = aurora_tensor_get(t, idx);
        if (std::isnan(v) || std::isinf(v)) return 0;
    }
    return 1;
}

/* === Optimizer Checkpointing === */
int optim_save(OptimState* o, const char* path) {
    if (!o || !path) return 0;
    FILE* f = fopen(path, "wb"); if (!f) return 0;
    int64_t magic = 0x4F5054494D535256;
    fwrite(&magic, sizeof(int64_t), 1, f);
    fwrite(&o->type, sizeof(int), 1, f);
    fwrite(&o->lr, sizeof(double), 1, f);
    fwrite(&o->beta1, sizeof(double), 1, f);
    fwrite(&o->beta2, sizeof(double), 1, f);
    fwrite(&o->epsilon, sizeof(double), 1, f);
    fwrite(&o->decay, sizeof(double), 1, f);
    fwrite(&o->momentum, sizeof(double), 1, f);
    fwrite(&o->t, sizeof(int64_t), 1, f);
    fwrite(&o->param_count, sizeof(int64_t), 1, f);
    if (o->m) fwrite(o->m, sizeof(double), (size_t)o->param_count, f);
    if (o->v) fwrite(o->v, sizeof(double), (size_t)o->param_count, f);
    fclose(f); return 1;
}

int optim_load(OptimState* o, const char* path) {
    if (!o || !path) return 0;
    FILE* f = fopen(path, "rb"); if (!f) return 0;
    int64_t magic; size_t read_count;
    read_count = fread(&magic, sizeof(int64_t), 1, f);
    if (read_count != 1 || magic != 0x4F5054494D535256) { fclose(f); return 0; }
    fread(&o->type, sizeof(int), 1, f);
    fread(&o->lr, sizeof(double), 1, f);
    fread(&o->beta1, sizeof(double), 1, f);
    fread(&o->beta2, sizeof(double), 1, f);
    fread(&o->epsilon, sizeof(double), 1, f);
    fread(&o->decay, sizeof(double), 1, f);
    fread(&o->momentum, sizeof(double), 1, f);
    fread(&o->t, sizeof(int64_t), 1, f);
    fread(&o->param_count, sizeof(int64_t), 1, f);
    if (o->m) fread(o->m, sizeof(double), (size_t)o->param_count, f);
    if (o->v) fread(o->v, sizeof(double), (size_t)o->param_count, f);
    fclose(f); return 1;
}

/* ════════════════════════════════════════════════════════════
   Mixed Precision Helpers
   ════════════════════════════════════════════════════════════ */

/* Initialize master weight copies (F64) and downcast working weights to low_dtype.
   Call once at the start of mixed-precision training. */
int model_cast_to_lowp(Model* m, int low_dtype) {
    if (!m || (low_dtype != TENSOR_F16 && low_dtype != TENSOR_BF16)) return 0;
    m->mixed_precision = low_dtype;
    for (int i = 0; i < m->n_layers; i++) {
        Layer* l = &m->layers[i];
        /* Create master copies if not present */
        if (l->w && !l->master_w) {
            l->master_w = aurora_tensor_clone(l->w);
            /* Downcast working weights from F64 to low precision */
            AuroraTensor* low = (low_dtype == TENSOR_F16)
                ? aurora_tensor_to_f16(l->w) : aurora_tensor_to_bf16(l->w);
            aurora_tensor_free(l->w);
            l->w = low;
            l->dtype = low_dtype;
        }
        if (l->b && !l->master_b) {
            l->master_b = aurora_tensor_clone(l->b);
            AuroraTensor* low = (low_dtype == TENSOR_F16)
                ? aurora_tensor_to_f16(l->b) : aurora_tensor_to_bf16(l->b);
            aurora_tensor_free(l->b);
            l->b = low;
        }
        if (l->hc_w && !l->master_hc_w) {
            l->master_hc_w = aurora_tensor_clone(l->hc_w);
            AuroraTensor* low = (low_dtype == TENSOR_F16)
                ? aurora_tensor_to_f16(l->hc_w) : aurora_tensor_to_bf16(l->hc_w);
            aurora_tensor_free(l->hc_w);
            l->hc_w = low;
        }
        if (l->hc_b && !l->master_hc_b) {
            l->master_hc_b = aurora_tensor_clone(l->hc_b);
            AuroraTensor* low = (low_dtype == TENSOR_F16)
                ? aurora_tensor_to_f16(l->hc_b) : aurora_tensor_to_bf16(l->hc_b);
            aurora_tensor_free(l->hc_b);
            l->hc_b = low;
        }
    }
    return 1;
}

/* Copy master weights (F64) → working weights (low precision).
   Call before each forward pass during mixed-precision training. */
int model_sync_weights_from_master(Model* m) {
    if (!m) return 0;
    for (int i = 0; i < m->n_layers; i++) {
        Layer* l = &m->layers[i];
        if (l->master_w && l->w) {
            int64_t n = l->master_w->total_size;
            if (m->mixed_precision == TENSOR_F16) {
                uint16_t* dst = (uint16_t*)l->w->data_ptr;
                for (int64_t j = 0; j < n; j++)
                    dst[j] = f32_to_f16((float)l->master_w->data[j]);
            } else {
                uint16_t* dst = (uint16_t*)l->w->data_ptr;
                for (int64_t j = 0; j < n; j++)
                    dst[j] = f32_to_bf16((float)l->master_w->data[j]);
            }
        }
        if (l->master_b && l->b) {
            int64_t n = l->master_b->total_size;
            if (m->mixed_precision == TENSOR_F16) {
                uint16_t* dst = (uint16_t*)l->b->data_ptr;
                for (int64_t j = 0; j < n; j++)
                    dst[j] = f32_to_f16((float)l->master_b->data[j]);
            } else {
                uint16_t* dst = (uint16_t*)l->b->data_ptr;
                for (int64_t j = 0; j < n; j++)
                    dst[j] = f32_to_bf16((float)l->master_b->data[j]);
            }
        }
        if (l->master_hc_w && l->hc_w) {
            int64_t n = l->master_hc_w->total_size;
            if (m->mixed_precision == TENSOR_F16) {
                uint16_t* dst = (uint16_t*)l->hc_w->data_ptr;
                for (int64_t j = 0; j < n; j++)
                    dst[j] = f32_to_f16((float)l->master_hc_w->data[j]);
            } else {
                uint16_t* dst = (uint16_t*)l->hc_w->data_ptr;
                for (int64_t j = 0; j < n; j++)
                    dst[j] = f32_to_bf16((float)l->master_hc_w->data[j]);
            }
        }
        if (l->master_hc_b && l->hc_b) {
            int64_t n = l->master_hc_b->total_size;
            if (m->mixed_precision == TENSOR_F16) {
                uint16_t* dst = (uint16_t*)l->hc_b->data_ptr;
                for (int64_t j = 0; j < n; j++)
                    dst[j] = f32_to_f16((float)l->master_hc_b->data[j]);
            } else {
                uint16_t* dst = (uint16_t*)l->hc_b->data_ptr;
                for (int64_t j = 0; j < n; j++)
                    dst[j] = f32_to_bf16((float)l->master_hc_b->data[j]);
            }
        }
    }
    return 1;
}

/* Copy working weight gradients → master gradient accumulators (F64).
   Call before optimizer step during mixed-precision training.
   The master gradients are stored in l->dw/l->db/l->hc_c/l->hc_d (always F64). */
int model_sync_weights_to_master(Model* m) {
    if (!m) return 0;
    /* Gradients are already in F64 (computed by backward in ai_layers).
       No conversion needed — just ensure master copies exist and update them. */
    for (int i = 0; i < m->n_layers; i++) {
        Layer* l = &m->layers[i];
        /* Ensure master copies have dw/db */
        if (l->dw && !l->master_w) {
            l->master_w = aurora_tensor_to_f64(l->w);
        }
        if (l->db && !l->master_b) {
            l->master_b = aurora_tensor_to_f64(l->b);
        }
        if (l->hc_c && !l->master_hc_w) {
            l->master_hc_w = aurora_tensor_to_f64(l->hc_w);
        }
        if (l->hc_d && !l->master_hc_b) {
            l->master_hc_b = aurora_tensor_to_f64(l->hc_b);
        }
    }
    return 1;
}

/* ════════════════════════════════════════════════════════════
   LoRA Initialization
   ════════════════════════════════════════════════════════════ */

int model_add_lora(Model* m, int rank, double alpha, double dropout) {
    if (!m || rank <= 0) return 0;
    for (int i = 0; i < m->n_layers; i++) {
        Layer* l = &m->layers[i];
        if (l->type != LAYER_DENSE && l->type != LAYER_ATTENTION) continue;
        int64_t in_dim = 0, out_dim = 0;
        if (l->type == LAYER_DENSE && l->w) {
            in_dim = l->w->shape[0];
            out_dim = l->w->shape[1];
        } else if (l->type == LAYER_ATTENTION && l->w) {
            in_dim = l->w->shape[0];
            out_dim = l->w->shape[1]; /* 3*d (QKV) */
        } else continue;

        l->lora_enabled = 1;
        l->lora_r = rank;
        l->lora_alpha = alpha;
        l->lora_dropout = dropout;

        /* A: [in_dim, rank] — zero init */
        int64_t a_shape[2] = { in_dim, rank };
        l->lora_A = aurora_tensor_new(2, a_shape);
        memset(l->lora_A->data, 0, (size_t)l->lora_A->total_size * sizeof(double));

        /* B: [rank, out_dim] — zero init */
        int64_t b_shape[2] = { rank, out_dim };
        l->lora_B = aurora_tensor_new(2, b_shape);
        memset(l->lora_B->data, 0, (size_t)l->lora_B->total_size * sizeof(double));

        /* Gradient buffers */
        l->lora_A_grad = aurora_tensor_new(2, a_shape);
        l->lora_B_grad = aurora_tensor_new(2, b_shape);

        /* Initialize A with Kaiming uniform */
        double a_scale = sqrt(2.0 / (double)(in_dim + rank));
        for (int64_t j = 0; j < l->lora_A->total_size; j++)
            l->lora_A->data[j] = rand_uniform() * a_scale;
    }
    return 1;
}

} /* extern "C" */
