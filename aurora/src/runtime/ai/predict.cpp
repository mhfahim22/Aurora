#include "runtime/tensor.hpp"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdio>

extern "C" {

/* ── Simple linear regression predict ──
   model: (n_features + 1,) tensor where last element is bias
   input: (n_features,) tensor
   returns: scalar prediction */
double aurora_predict_linear(AuroraTensor* model, AuroraTensor* input) {
    if (!model || !input) return 0.0;
    int64_t n = model->total_size - 1;
    double result = model->data[n]; /* bias */
    for (int64_t i = 0; i < n && i < input->total_size; i++)
        result += model->data[i] * input->data[i];
    return result;
}

/* ── K-Nearest Neighbors predict (simplified, Euclidean distance) ──
   X_train: (N, D) tensor, y_train: (N,) tensor
   input: (D,) tensor
   k: number of neighbors */
double aurora_predict_knn(AuroraTensor* X_train, AuroraTensor* y_train,
                           AuroraTensor* input, int64_t k) {
    if (!X_train || !y_train || !input) return 0.0;
    int64_t N = X_train->shape[0];
    int64_t D = X_train->shape[1];
    if (k <= 0) k = 1;
    if (k > N) k = N;

    /* Compute distances */
    typedef struct { double dist; double label; } Neighbor;
    Neighbor* neighbors = (Neighbor*)malloc((size_t)N * sizeof(Neighbor));
    if (!neighbors) return 0.0;
    for (int64_t i = 0; i < N; i++) {
        double dist = 0.0;
        for (int64_t j = 0; j < D && j < input->total_size; j++) {
            double diff = X_train->data[i * D + j] - input->data[j];
            dist += diff * diff;
        }
        neighbors[i].dist = sqrt(dist);
        neighbors[i].label = y_train->data[i];
    }

    /* Sort by distance (simple bubble sort for k nearest) */
    for (int64_t i = 0; i < k; i++) {
        int64_t best = i;
        for (int64_t j = i + 1; j < N; j++)
            if (neighbors[j].dist < neighbors[best].dist) best = j;
        Neighbor tmp = neighbors[i];
        neighbors[i] = neighbors[best];
        neighbors[best] = tmp;
    }

    /* Majority vote */
    double sum = 0.0;
    for (int64_t i = 0; i < k; i++) sum += neighbors[i].label;
    free(neighbors);
    return sum / (double)k;
}

/* ── Min-max normalization ──
   Modifies tensor in-place: (x - min) / (max - min) */
void aurora_normalize_minmax(AuroraTensor* t) {
    if (!t || t->total_size == 0) return;
    double mn = t->data[0], mx = t->data[0];
    for (int64_t i = 0; i < t->total_size; i++) {
        if (t->data[i] < mn) mn = t->data[i];
        if (t->data[i] > mx) mx = t->data[i];
    }
    double range = mx - mn;
    if (range == 0.0) return;
    for (int64_t i = 0; i < t->total_size; i++)
        t->data[i] = (t->data[i] - mn) / range;
}

/* ── Z-score normalization ──
   Modifies tensor in-place: (x - mean) / std */
void aurora_normalize_zscore(AuroraTensor* t) {
    if (!t || t->total_size == 0) return;
    double sum = 0.0;
    for (int64_t i = 0; i < t->total_size; i++) sum += t->data[i];
    double mean = sum / (double)t->total_size;
    double var = 0.0;
    for (int64_t i = 0; i < t->total_size; i++)
        var += (t->data[i] - mean) * (t->data[i] - mean);
    double std = sqrt(var / (double)t->total_size);
    if (std == 0.0) return;
    for (int64_t i = 0; i < t->total_size; i++)
        t->data[i] = (t->data[i] - mean) / std;
}

}
