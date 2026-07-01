/* ── Aurora Runtime — Autograd Engine (computation graph & chain-rule backward pass) ──
 *
 * Builds a dynamic computation graph during forward operations.  Every tensor
 * operation (add, mul, matmul, etc.) records its inputs via link_grad() so
 * that aurora_tensor_backward() can traverse the graph in topological order
 * and accumulate gradients using the chain rule.
 *
 * Architecture:
 *   Each AuroraTensor carries:
 *     grad[]         – gradient buffer (allocated when requires_grad is set)
 *     requires_grad  – flag
 *     backward_fn    – pointer to the backward implementation for this node
 *     prev[8]        – up to 8 parent tensors in the graph
 *     prev_count     – number of registered parents
 *
 *   The forward ops (in tensor.cpp) call link_grad() to register parents,
 *   then aurora_tensor_backward() does a reverse topological traversal
 *   invoking each node's backward_fn.
 * ── */
#include "runtime/tensor.hpp"
#include <cmath>
#include <cstdlib>
#include <cstring>

/* ── Dtype-aware read helper (used by all backward functions) ── */
static inline double tensor_read_f64(const AuroraTensor* t, int64_t i) {
    switch (t->dtype) {
        case TENSOR_F32: return (double)t->data_f32[i];
        case TENSOR_F16: { uint16_t* p = (uint16_t*)t->data_ptr; return (double)f16_to_f32(p[i]); }
        case TENSOR_BF16:{ uint16_t* p = (uint16_t*)t->data_ptr; return (double)bf16_to_f32(p[i]); }
        default: return t->data[i];
    }
}

/* ════════════════════════════════════════════════════════════
   Backward functions — each implements dL/d(inputs) given
   dL/d(output) stored in self->grad[].
   ════════════════════════════════════════════════════════════ */

void backward_add(AuroraTensor* self) {
    int64_t n = self->total_size;
    for (int i = 0; i < self->prev_count && i < 2; i++) {
        AuroraTensor* p = self->prev[i];
        if (p && p->grad) {
            int64_t limit = n < p->total_size ? n : p->total_size;
            for (int64_t j = 0; j < limit; j++)
                p->grad[j] += self->grad[j];
        }
    }
}

void backward_sub(AuroraTensor* self) {
    int64_t n = self->total_size;
    if (self->prev_count >= 1 && self->prev[0] && self->prev[0]->grad) {
        int64_t limit = n < self->prev[0]->total_size ? n : self->prev[0]->total_size;
        for (int64_t j = 0; j < limit; j++)
            self->prev[0]->grad[j] += self->grad[j];
    }
    if (self->prev_count >= 2 && self->prev[1] && self->prev[1]->grad) {
        int64_t limit = n < self->prev[1]->total_size ? n : self->prev[1]->total_size;
        for (int64_t j = 0; j < limit; j++)
            self->prev[1]->grad[j] -= self->grad[j];
    }
}

void backward_mul(AuroraTensor* self) {
    int64_t n = self->total_size;
    AuroraTensor* a = (self->prev_count >= 1) ? self->prev[0] : nullptr;
    AuroraTensor* b = (self->prev_count >= 2) ? self->prev[1] : nullptr;
    if (a && a->grad) {
        int64_t limit = n < a->total_size ? n : a->total_size;
        for (int64_t j = 0; j < limit; j++)
            a->grad[j] += self->grad[j] * (b ? tensor_read_f64(b, j) : 1.0);
    }
    if (b && b->grad) {
        int64_t limit = n < b->total_size ? n : b->total_size;
        for (int64_t j = 0; j < limit; j++)
            b->grad[j] += self->grad[j] * (a ? tensor_read_f64(a, j) : 1.0);
    }
}

void backward_div(AuroraTensor* self) {
    AuroraTensor* a = (self->prev_count >= 1) ? self->prev[0] : nullptr;
    AuroraTensor* b = (self->prev_count >= 2) ? self->prev[1] : nullptr;
    if (a && a->grad) {
        for (int64_t j = 0; j < a->total_size; j++) {
            double bv = b ? tensor_read_f64(b, j) : 1.0;
            a->grad[j] += (j < self->total_size ? self->grad[j] : 0.0) / (bv != 0.0 ? bv : 1.0);
        }
    }
    if (b && b->grad) {
        for (int64_t j = 0; j < b->total_size; j++) {
            double bv = tensor_read_f64(b, j);
            double av = a ? tensor_read_f64(a, j) : 1.0;
            double sg = (j < self->total_size) ? self->grad[j] : 0.0;
            b->grad[j] -= sg * av / (bv * bv + 1e-12);
        }
    }
}

void backward_pow(AuroraTensor* self) {
    if (!self->prev_count || !self->prev[0] || !self->prev[0]->grad) return;
    AuroraTensor* a = self->prev[0];
    for (int64_t j = 0; j < a->total_size; j++) {
        double av = tensor_read_f64(a, j);
        double sg = (j < self->total_size) ? self->grad[j] : 0.0;
        a->grad[j] += sg * 2.0 * av;
    }
}

void backward_matmul(AuroraTensor* self) {
    if (!self->prev_count) return;
    AuroraTensor* a = self->prev[0];
    AuroraTensor* b = (self->prev_count >= 2) ? self->prev[1] : nullptr;
    if (!a || !b) return;

    int64_t M = self->shape[0], N = self->shape[1];

    /* grad_a = self->grad @ b.T  (M x Ka) */
    if (a->grad && a->ndim == 2) {
        int64_t Ka = a->shape[1];
        for (int64_t i = 0; i < M; i++)
            for (int64_t k = 0; k < Ka; k++)
                for (int64_t j = 0; j < N; j++)
                    a->grad[i * Ka + k] += self->grad[i * N + j] * tensor_read_f64(b, k * N + j);
    }

    /* grad_b = a.T @ self->grad  (Ka x N) */
    if (b->grad && b->ndim == 2) {
        int64_t Kb = b->shape[0];
        for (int64_t k = 0; k < Kb; k++)
            for (int64_t j = 0; j < N; j++)
                for (int64_t i = 0; i < M; i++)
                    b->grad[k * N + j] += tensor_read_f64(a, i * Kb + k) * self->grad[i * N + j];
    }
}

void backward_relu(AuroraTensor* self) {
    if (!self->prev_count || !self->prev[0] || !self->prev[0]->grad) return;
    AuroraTensor* input = self->prev[0];
    for (int64_t j = 0; j < input->total_size; j++) {
        double iv = tensor_read_f64(input, j);
        double sg = (self->grad && j < self->total_size) ? self->grad[j] : 0.0;
        input->grad[j] += (iv > 0.0) ? sg : 0.0;
    }
}

void backward_sigmoid(AuroraTensor* self) {
    if (!self->prev_count || !self->prev[0] || !self->prev[0]->grad) return;
    AuroraTensor* input = self->prev[0];
    for (int64_t j = 0; j < input->total_size; j++) {
        double out_val = tensor_read_f64(input, j);
        double sg = (self->grad && j < self->total_size) ? self->grad[j] : 0.0;
        input->grad[j] += sg * out_val * (1.0 - out_val);
    }
}

void backward_tanh(AuroraTensor* self) {
    if (!self->prev_count || !self->prev[0] || !self->prev[0]->grad) return;
    AuroraTensor* input = self->prev[0];
    for (int64_t j = 0; j < input->total_size; j++) {
        double out_val = tensor_read_f64(input, j);
        double sg = (self->grad && j < self->total_size) ? self->grad[j] : 0.0;
        input->grad[j] += sg * (1.0 - out_val * out_val);
    }
}

void backward_softmax(AuroraTensor* self) {
    if (!self->prev_count || !self->prev[0] || !self->prev[0]->grad) return;
    AuroraTensor* input = self->prev[0];
    int64_t n = input->total_size;
    int64_t m = self->total_size < n ? self->total_size : n;
    for (int64_t j = 0; j < m; j++) {
        double sm = tensor_read_f64(self, j);
        double sg = (self->grad && j < self->total_size) ? self->grad[j] : 0.0;
        input->grad[j] += sm * sg * (1.0 - sm);
    }
}

void backward_cross_entropy(AuroraTensor* self) {
    if (self->prev_count < 2 || !self->prev[0] || !self->prev[0]->grad) return;
    AuroraTensor* pred = self->prev[0];
    AuroraTensor* target = self->prev[1];
    double sg = (self->grad && self->total_size > 0) ? self->grad[0] : 1.0;
    int64_t n = pred->total_size < target->total_size ? pred->total_size : target->total_size;
    for (int64_t j = 0; j < n; j++) {
        double p = tensor_read_f64(pred, j);
        double t = tensor_read_f64(target, j);
        pred->grad[j] += sg * (p - t);
    }
}

void backward_conv2d(AuroraTensor* self) {
    if (self->prev_count < 2 || !self->prev[0] || !self->prev[0]->grad) return;
    AuroraTensor* input = self->prev[0];
    AuroraTensor* kernel = self->prev[1];
    if (!input->grad || !kernel->grad) return;
    int64_t IC = input->shape[1], IH = input->shape[2], IW = input->shape[3];
    int64_t OC = kernel->shape[0], KC = kernel->shape[1], KH = kernel->shape[2], KW = kernel->shape[3];
    int64_t OH = self->shape[2], OW = self->shape[3];
    int64_t OH_OW = OH * OW;
    for (int64_t oc = 0; oc < OC; oc++) {
        for (int64_t oh = 0; oh < OH; oh++) {
            for (int64_t ow = 0; ow < OW; ow++) {
                double sg = (self->grad) ? self->grad[oc * OH_OW + oh * OW + ow] : 0.0;
                for (int64_t kc = 0; kc < KC; kc++) {
                    int64_t ih = oh, iw = ow;
                    double kv = tensor_read_f64(kernel, oc * KC * KH * KW + kc * KH * KW + 0 * KW + 0);
                    input->grad[kc * IH * IW + ih * IW + iw] += sg * kv;
                    kernel->grad[oc * KC * KH * KW + kc * KH * KW + 0 * KW + 0] += sg * tensor_read_f64(input, kc * IH * IW + ih * IW + iw);
                }
            }
        }
    }
}

void backward_maxpool2d(AuroraTensor* self) {
    if (!self->prev_count || !self->prev[0] || !self->prev[0]->grad) return;
    AuroraTensor* input = self->prev[0];
    int64_t C = input->shape[1], H = input->shape[2], W = input->shape[3];
    int64_t OH = self->shape[2], OW = self->shape[3];
    int64_t pool_h = H / OH, pool_w = W / OW;
    for (int64_t c = 0; c < C; c++) {
        for (int64_t oh = 0; oh < OH; oh++) {
            for (int64_t ow = 0; ow < OW; ow++) {
                int64_t max_h = 0, max_w = 0;
                double max_val = -1e100;
                for (int64_t ph = 0; ph < pool_h; ph++) {
                    for (int64_t pw = 0; pw < pool_w; pw++) {
                        int64_t ih = oh * pool_h + ph, iw = ow * pool_w + pw;
                        double v = tensor_read_f64(input, c * H * W + ih * W + iw);
                        if (v > max_val) { max_val = v; max_h = ih; max_w = iw; }
                    }
                }
                double sg = (self->grad) ? self->grad[c * OH * OW + oh * OW + ow] : 0.0;
                input->grad[c * H * W + max_h * W + max_w] += sg;
            }
        }
    }
}

void backward_batchnorm(AuroraTensor* self) {
    if (self->prev_count < 3 || !self->prev[0] || !self->prev[0]->grad) return;
    AuroraTensor* input = self->prev[0];
    AuroraTensor* gamma = self->prev[1];
    AuroraTensor* beta  = self->prev[2];
    if (!input->grad) return;
    int64_t C = input->shape[1], H = input->shape[2], W = input->shape[3];
    int64_t N = input->shape[0];
    int64_t spatial = H * W, per_channel = N * spatial;
    for (int64_t c = 0; c < C; c++) {
        double mean = 0.0, var = 0.0;
        for (int64_t n = 0; n < N; n++)
            for (int64_t s = 0; s < spatial; s++)
                mean += tensor_read_f64(input, n * C * spatial + c * spatial + s);
        mean /= (double)per_channel;
        for (int64_t n = 0; n < N; n++)
            for (int64_t s = 0; s < spatial; s++) {
                double d = tensor_read_f64(input, n * C * spatial + c * spatial + s) - mean;
                var += d * d;
            }
        var /= (double)per_channel;
        double std_inv = 1.0 / sqrt(var + 1e-8);
        double g = gamma ? tensor_read_f64(gamma, c) : 1.0;
        double sum_dy = 0.0, sum_dy_x = 0.0;
        for (int64_t n = 0; n < N; n++)
            for (int64_t s = 0; s < spatial; s++) {
                int64_t idx = n * C * spatial + c * spatial + s;
                double dy = (self->grad) ? self->grad[idx] : 0.0;
                double x = tensor_read_f64(input, idx);
                sum_dy += dy;
                sum_dy_x += dy * x;
            }
        double dx_mean = -sum_dy * std_inv / (double)per_channel;
        double dx_var = -sum_dy_x * std_inv / (2.0 * (var + 1e-8) * (double)per_channel);
        for (int64_t n = 0; n < N; n++)
            for (int64_t s = 0; s < spatial; s++) {
                int64_t idx = n * C * spatial + c * spatial + s;
                double dy = (self->grad) ? self->grad[idx] : 0.0;
                double x = tensor_read_f64(input, idx);
                double x_hat = (x - mean) * std_inv;
                input->grad[idx] += g * std_inv * (dy - sum_dy / (double)per_channel - x_hat * sum_dy_x / (double)per_channel);
                if (gamma && gamma->grad) gamma->grad[c] += dy * x_hat;
                if (beta && beta->grad)  beta->grad[c] += dy;
            }
    }
}

void backward_dropout(AuroraTensor* self) {
    if (!self->prev_count || !self->prev[0] || !self->prev[0]->grad) return;
    AuroraTensor* input = self->prev[0];
    double scale = (self->prev_count >= 2 && self->prev[1]) ? (1.0 / (1.0 - tensor_read_f64(self->prev[1], 0))) : 1.0;
    for (int64_t j = 0; j < input->total_size && j < self->total_size; j++) {
        double out = tensor_read_f64(self, j);
        double sg = (self->grad && j < self->total_size) ? self->grad[j] : 0.0;
        input->grad[j] += (out != 0.0) ? sg * scale : 0.0;
    }
}

void backward_attention(AuroraTensor* self) {
    if (self->prev_count < 3 || !self->prev[0] || !self->prev[0]->grad) return;
    AuroraTensor* Q = self->prev[0];
    AuroraTensor* K = self->prev[1];
    AuroraTensor* V = self->prev[2];
    int64_t N = Q->shape[0], L = Q->shape[1], S = K->shape[1], D = Q->shape[2];
    double scale = 1.0 / sqrt((double)D);
    for (int64_t n = 0; n < N; n++) {
        for (int64_t l = 0; l < L; l++) {
            for (int64_t s = 0; s < S; s++) {
                double sum_exp = 0.0;
                double max_qk = -1e100;
                for (int64_t sp = 0; sp < S; sp++) {
                    double qk = 0.0;
                    for (int64_t d = 0; d < D; d++)
                        qk += tensor_read_f64(Q, n * L * D + l * D + d) * tensor_read_f64(K, n * S * D + sp * D + d);
                    if (qk > max_qk) max_qk = qk;
                }
                double attn = 0.0;
                for (int64_t sp = 0; sp < S; sp++) {
                    double qk = 0.0;
                    for (int64_t d = 0; d < D; d++)
                        qk += tensor_read_f64(Q, n * L * D + l * D + d) * tensor_read_f64(K, n * S * D + sp * D + d);
                    sum_exp += exp((qk - max_qk) * scale);
                }
                if (sum_exp > 0) {
                    double qk = 0.0;
                    for (int64_t d = 0; d < D; d++)
                        qk += tensor_read_f64(Q, n * L * D + l * D + d) * tensor_read_f64(K, n * S * D + s * D + d);
                    attn = exp((qk - max_qk) * scale) / sum_exp;
                }
                for (int64_t d = 0; d < D; d++) {
                    int64_t out_idx = n * L * D + l * D + d;
                    double v_val = tensor_read_f64(V, n * S * D + s * D + d);
                    double sg = (self->grad) ? self->grad[out_idx] : 0.0;
                    double dv = sg * attn;
                    if (V->grad) V->grad[n * S * D + s * D + d] += dv;
                    for (int64_t dp = 0; dp < D; dp++) {
                        double k_val = tensor_read_f64(K, n * S * D + s * D + dp);
                        double q_val = tensor_read_f64(Q, n * L * D + l * D + dp);
                        double d_attn = sg * v_val * attn * (1.0 - attn) * scale;
                        if (Q->grad) Q->grad[n * L * D + l * D + dp] += d_attn * k_val;
                        if (K->grad) K->grad[n * S * D + s * D + dp] += d_attn * q_val;
                    }
                }
            }
        }
    }
}

/* ── Graph construction helper ── */

AuroraTensor* link_grad(AuroraTensor* result, autograd_backward_fn backward,
                         AuroraTensor* input1, AuroraTensor* input2,
                         AuroraTensor* input3, AuroraTensor* input4) {
    result->backward_fn = backward;
    AuroraTensor* inputs[] = { input1, input2, input3, input4 };
    for (int i = 0; i < 4; i++) {
        if (inputs[i] && inputs[i]->requires_grad) {
            result->prev[result->prev_count++] = inputs[i];
            result->requires_grad = 1;
        }
    }
    if (result->requires_grad) {
        if (result->grad) free(result->grad);
        result->grad = (double*)calloc((size_t)result->total_size, sizeof(double));
    }
    return result;
}

/* ── Topological sort (Kahn's algorithm) ── */

static void topo_sort_kahn(AuroraTensor* t, AuroraTensor** order, int* count) {
    if (!t) return;
    int in_deg[1024] = {0};
    AuroraTensor* queue[1024];
    int qh = 0, qt = 0;
    int node_count = 0;
    AuroraTensor* nodes[1024];
    nodes[node_count++] = t;
    for (int i = 0; i < node_count && i < 1024; i++) {
        AuroraTensor* cur = nodes[i];
        for (int j = 0; j < cur->prev_count; j++) {
            if (cur->prev[j]) {
                int found = 0;
                for (int k = 0; k < node_count; k++)
                    if (nodes[k] == cur->prev[j]) { found = 1; break; }
                if (!found && node_count < 1024)
                    nodes[node_count++] = cur->prev[j];
            }
        }
    }
    for (int i = 0; i < node_count; i++) {
        for (int j = 0; j < nodes[i]->prev_count; j++) {
            if (nodes[i]->prev[j]) {
                for (int k = 0; k < node_count; k++) {
                    if (nodes[k] == nodes[i]->prev[j]) {
                        in_deg[k]++;  /* prev[j] is a dependency of nodes[i] */
                        break;
                    }
                }
            }
        }
    }
    for (int i = 0; i < node_count; i++)
        if (in_deg[i] == 0) queue[qt++] = nodes[i];
    while (qh < qt && *count < 1024) {
        AuroraTensor* u = queue[qh++];
        order[(*count)++] = u;
        for (int k = 0; k < node_count; k++) {
            for (int j = 0; j < nodes[k]->prev_count; j++) {
                if (nodes[k]->prev[j] == u && --in_deg[k] == 0) {
                    queue[qt++] = nodes[k];
                    break;
                }
            }
        }
    }
}

/* ════════════════════════════════════════════════════════════
   Public API
   ════════════════════════════════════════════════════════════ */

extern "C" {

void aurora_tensor_set_requires_grad(AuroraTensor* t, int flag) {
    if (!t) return;
    t->requires_grad = flag;
    if (flag && !t->grad) {
        t->grad = (double*)calloc((size_t)t->total_size, sizeof(double));
        if (!t->grad) t->requires_grad = 0;
    } else if (!flag && t->grad) {
        free(t->grad);
        t->grad = nullptr;
    }
}

void aurora_tensor_zero_grad(AuroraTensor* t) {
    if (!t || !t->grad) return;
    memset(t->grad, 0, (size_t)t->total_size * sizeof(double));
}

void aurora_tensor_backward(AuroraTensor* t) {
    if (!t || !t->requires_grad) return;
    /* Seed the gradient: for a scalar output, dL/dL = 1 */
    if (t->grad && t->total_size > 0)
        t->grad[0] = 1.0;

    /* Topological sort from the output tensor backwards */
    AuroraTensor* order[1024];
    int count = 0;
    topo_sort_kahn(t, order, &count);

    for (int i = 0; i < count; i++) {
        if (order[i]->backward_fn)
            order[i]->backward_fn(order[i]);
    }
}

void aurora_tensor_sgd_step(AuroraTensor* param, double lr) {
    if (!param || !param->grad || !param->data_ptr) return;
    for (int64_t i = 0; i < param->total_size; i++) {
        switch (param->dtype) {
            case TENSOR_F32:
                param->data_f32[i] -= (float)(lr * param->grad[i]);
                break;
            default:
                param->data[i] -= lr * param->grad[i];
                break;
        }
    }
}

} /* extern "C" */
