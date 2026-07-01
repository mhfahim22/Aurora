#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cassert>
#include <ctime>
#include <cstring>
#include "runtime/tensor.hpp"

/* Manual backward for small known graphs using backward_fn pointers directly */

static void backward_manual_2node(AuroraTensor* add_node, AuroraTensor* mul_node,
                                   double* dloss, int64_t n) {
    if (add_node->grad)
        memcpy(add_node->grad, dloss, n * sizeof(double));
    if (add_node->backward_fn)
        add_node->backward_fn(add_node);
    if (mul_node->backward_fn)
        mul_node->backward_fn(mul_node);
}

/* ── Unit tests for individual backward functions ── */

static void test_backward_add() {
    int64_t shape[] = {3, 1};
    AuroraTensor* a = aurora_tensor_new(2, shape);
    AuroraTensor* b = aurora_tensor_new(2, shape);
    a->data[0] = 1.0; a->data[1] = 2.0; a->data[2] = 3.0;
    b->data[0] = 4.0; b->data[1] = 5.0; b->data[2] = 6.0;
    aurora_tensor_set_requires_grad(a, 1);
    aurora_tensor_set_requires_grad(b, 1);

    AuroraTensor* r = aurora_tensor_add(a, b);
    assert(r != nullptr);
    r->grad[0] = 1.0; r->grad[1] = 1.0; r->grad[2] = 1.0;
    backward_add(r);

    assert(a->grad[0] == 1.0 && a->grad[1] == 1.0 && a->grad[2] == 1.0);
    assert(b->grad[0] == 1.0 && b->grad[1] == 1.0 && b->grad[2] == 1.0);

    aurora_tensor_free(a); aurora_tensor_free(b); aurora_tensor_free(r);
    printf("  PASS test_backward_add\n");
}

static void test_backward_mul() {
    int64_t shape[] = {3, 1};
    AuroraTensor* a = aurora_tensor_new(2, shape);
    AuroraTensor* b = aurora_tensor_new(2, shape);
    a->data[0] = 2.0; a->data[1] = 3.0; a->data[2] = 4.0;
    b->data[0] = 5.0; b->data[1] = 6.0; b->data[2] = 7.0;
    aurora_tensor_set_requires_grad(a, 1);
    aurora_tensor_set_requires_grad(b, 1);

    AuroraTensor* r = aurora_tensor_mul(a, b);
    assert(r != nullptr);
    r->grad[0] = 1.0; r->grad[1] = 1.0; r->grad[2] = 1.0;
    backward_mul(r);

    assert(a->grad[0] == 5.0 && a->grad[1] == 6.0 && a->grad[2] == 7.0);
    assert(b->grad[0] == 2.0 && b->grad[1] == 3.0 && b->grad[2] == 4.0);

    aurora_tensor_free(a); aurora_tensor_free(b); aurora_tensor_free(r);
    printf("  PASS test_backward_mul\n");
}

static void test_backward_sub() {
    int64_t shape[] = {2, 1};
    AuroraTensor* a = aurora_tensor_new(2, shape);
    AuroraTensor* b = aurora_tensor_new(2, shape);
    a->data[0] = 10.0; a->data[1] = 20.0;
    b->data[0] = 3.0;  b->data[1] = 5.0;
    aurora_tensor_set_requires_grad(a, 1);
    aurora_tensor_set_requires_grad(b, 1);

    AuroraTensor* r = aurora_tensor_sub(a, b);
    assert(r != nullptr);
    r->grad[0] = 1.0; r->grad[1] = 1.0;
    backward_sub(r);

    assert(a->grad[0] == 1.0 && a->grad[1] == 1.0);
    assert(b->grad[0] == -1.0 && b->grad[1] == -1.0);

    aurora_tensor_free(a); aurora_tensor_free(b); aurora_tensor_free(r);
    printf("  PASS test_backward_sub\n");
}

static void test_backward_pow() {
    int64_t shape[] = {2, 1};
    AuroraTensor* a = aurora_tensor_new(2, shape);
    a->data[0] = 3.0; a->data[1] = 4.0;
    aurora_tensor_set_requires_grad(a, 1);

    AuroraTensor* r = aurora_tensor_pow(a, 2.0);
    assert(r != nullptr);
    r->grad[0] = 1.0; r->grad[1] = 1.0;
    backward_pow(r);

    assert(fabs(a->grad[0] - 6.0) < 1e-10);
    assert(fabs(a->grad[1] - 8.0) < 1e-10);

    aurora_tensor_free(a); aurora_tensor_free(r);
    printf("  PASS test_backward_pow\n");
}

static void test_backward_relu() {
    int64_t shape[] = {4, 1};
    AuroraTensor* input = aurora_tensor_new(2, shape);
    input->data[0] = -1.0; input->data[1] = 0.0; input->data[2] = 2.0; input->data[3] = 3.0;
    aurora_tensor_set_requires_grad(input, 1);

    AuroraTensor* result = aurora_tensor_clone(input);
    assert(result != nullptr);
    aurora_tensor_relu(result);
    link_grad(result, backward_relu, input, 0, 0, 0);
    result->grad[0] = 1.0; result->grad[1] = 1.0; result->grad[2] = 1.0; result->grad[3] = 1.0;
    backward_relu(result);

    assert(input->grad[0] == 0.0);
    assert(input->grad[1] == 0.0);
    assert(input->grad[2] == 1.0);
    assert(input->grad[3] == 1.0);

    aurora_tensor_free(input); aurora_tensor_free(result);
    printf("  PASS test_backward_relu\n");
}

static void test_sgd_step() {
    int64_t shape[] = {1, 1};
    AuroraTensor* param = aurora_tensor_new(2, shape);
    param->data[0] = 10.0;
    aurora_tensor_set_requires_grad(param, 1);
    param->grad[0] = -2.0;

    aurora_tensor_sgd_step(param, 0.5);
    assert(fabs(param->data[0] - 11.0) < 1e-10);
    aurora_tensor_free(param);
    printf("  PASS test_sgd_step\n");
}

/* ── End-to-end linear regression ── */

static double mse(double* pred, double* target, int64_t n) {
    double loss = 0.0;
    for (int64_t i = 0; i < n; i++) {
        double d = pred[i] - target[i];
        loss += d * d;
    }
    return loss / (double)n;
}

static void compute_dloss(double* out, double* pred, double* target, int64_t n) {
    for (int64_t i = 0; i < n; i++)
        out[i] = 2.0 * (pred[i] - target[i]) / (double)n;
}

static void test_linear_regression() {
    srand(42);
    int64_t N = 1;

    double true_w = 3.0, true_b = -2.0;

    int64_t x_shape[] = {N, 1};
    AuroraTensor* x = aurora_tensor_new(2, x_shape);
    for (int64_t i = 0; i < N; i++)
        x->data[i] = (double)rand() / RAND_MAX * 4.0 - 2.0;

    AuroraTensor* y_true = aurora_tensor_new(2, x_shape);
    for (int64_t i = 0; i < N; i++)
        y_true->data[i] = true_w * x->data[i] + true_b + ((double)rand() / RAND_MAX - 0.5) * 0.2;

    int64_t s1[] = {N, 1};
    AuroraTensor* w = aurora_tensor_new(2, s1);
    w->data[0] = 0.0;
    aurora_tensor_set_requires_grad(w, 1);

    AuroraTensor* b = aurora_tensor_new(2, s1);
    b->data[0] = 0.0;
    aurora_tensor_set_requires_grad(b, 1);

    /* x needs requires_grad so link_grad registers both x and w in correct order
       for backward_mul: prev[0]=x, prev[1]=w */
    aurora_tensor_set_requires_grad(x, 1);

    double* dloss_buf = (double*)calloc((size_t)N, sizeof(double));
    double lr = 0.1;
    double initial_loss = 0.0, final_loss = 0.0;

    printf("  Epoch    Loss      w        b\n");
    for (int epoch = 0; epoch <= 100; epoch++) {
        AuroraTensor* z = aurora_tensor_mul(x, w);
        assert(z != nullptr);
        AuroraTensor* y_pred = aurora_tensor_add(z, b);
        assert(y_pred != nullptr);

        double loss = mse(y_pred->data, y_true->data, N);
        if (epoch == 0) initial_loss = loss;
        if (epoch == 100) final_loss = loss;

        if (epoch % 10 == 0 || epoch == 100)
            printf("  %4d   %8.4f  %6.3f  %6.3f\n", epoch, loss, w->data[0], b->data[0]);

        compute_dloss(dloss_buf, y_pred->data, y_true->data, N);
        backward_manual_2node(y_pred, z, dloss_buf, y_pred->total_size);

        aurora_tensor_sgd_step(w, lr);
        aurora_tensor_sgd_step(b, lr);

        aurora_tensor_zero_grad(w);
        aurora_tensor_zero_grad(b);
        aurora_tensor_zero_grad(x);

        aurora_tensor_free(y_pred);
        aurora_tensor_free(z);
    }

    free(dloss_buf);

    printf("\n  Initial loss: %.6f\n", initial_loss);
    printf("  Final loss:   %.6f\n", final_loss);
    printf("  Final w:      %.4f (true: %.1f)\n", w->data[0], true_w);
    printf("  Final b:      %.4f (true: %.1f)\n", b->data[0], true_b);

    assert(final_loss < 0.01);
    assert(fabs(w->data[0] - true_w) < 0.5);
    assert(fabs(b->data[0] - true_b) < 0.5);

    printf("  PASS test_linear_regression (loss=%.6f, w=%.3f, b=%.3f)\n",
           final_loss, w->data[0], b->data[0]);

    aurora_tensor_free(x);
    aurora_tensor_free(y_true);
    aurora_tensor_free(w);
    aurora_tensor_free(b);
}

int main() {
    printf("=== Autograd Engine Tests ===\n\n");
    fflush(stdout);

    test_backward_add();
    test_backward_mul();
    test_backward_sub();
    test_backward_pow();
    test_backward_relu();
    test_sgd_step();
    test_linear_regression();

    printf("\n=== All autograd tests passed! ===\n");
    fflush(stdout);
    return 0;
}
