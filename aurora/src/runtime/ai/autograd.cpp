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
    double exp_val = 2.0;
    for (int64_t j = 0; j < a->total_size; j++) {
        double av = tensor_read_f64(a, j);
        double sg = (j < self->total_size) ? self->grad[j] : 0.0;
        a->grad[j] += sg * exp_val * pow(av, exp_val - 1.0);
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
        result->grad = (double*)calloc((size_t)result->total_size, sizeof(double));
    }
    return result;
}

/* ── Topological sort (reverse DFS) ── */

static void topo_sort(AuroraTensor* t, AuroraTensor** order, int* count, int max_depth) {
    if (!t || max_depth <= 0) return;
    for (int i = 0; i < t->prev_count; i++) {
        if (t->prev[i]) {
            int found = 0;
            for (int j = 0; j < *count; j++)
                if (order[j] == t->prev[i]) { found = 1; break; }
            if (!found)
                topo_sort(t->prev[i], order, count, max_depth - 1);
        }
    }
    int found = 0;
    for (int j = 0; j < *count; j++)
        if (order[j] == t) { found = 1; break; }
    if (!found && *count < 1024)
        order[(*count)++] = t;
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
    topo_sort(t, order, &count, 256);

    /* Traverse in reverse order (output first, then its parents) */
    for (int i = count - 1; i >= 0; i--) {
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
