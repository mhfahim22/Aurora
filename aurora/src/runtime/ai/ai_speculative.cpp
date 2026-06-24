#include "runtime/ai/ai_common.h"

extern "C" {

/* Generate a single token from model (helper - just like generate_step but with model pointer) */
static int64_t draft_step(Model* m, int64_t token, int64_t* cache_len_ptr) {
    if (!m) return 0;
    int has_embed = 0;
    for (int i = 0; i < m->n_layers; i++) {
        if (m->layers[i].type == LAYER_EMBEDDING) {
            has_embed = 1;
            break;
        }
    }
    if (!has_embed) return 0;
    int64_t is[2] = {1, 1};
    AuroraTensor* in = aurora_tensor_new(2, is);
    in->data[0] = (double)token;
    AuroraTensor* out = model_forward(m, in, 0);
    if (!out) { aurora_tensor_free(in); return 0; }
    int64_t vocab = out->shape[1];
    double* logits = &out->data[(out->shape[0] - 1) * vocab];
    double maxv = logits[0];
    for (int64_t i = 1; i < vocab; i++) if (logits[i] > maxv) maxv = logits[i];
    double sum = 0.0;
    for (int64_t i = 0; i < vocab; i++) { logits[i] = exp(logits[i] - maxv); sum += logits[i]; }
    if (sum > 1e-15) for (int64_t i = 0; i < vocab; i++) logits[i] /= sum;
    double r = (double)rand() / RAND_MAX;
    double cum = 0.0;
    int64_t next = 0;
    for (int64_t i = 0; i < vocab; i++) { cum += logits[i]; if (r <= cum) { next = i; break; } }
    aurora_tensor_free(in); aurora_tensor_free(out);
    if (cache_len_ptr) (*cache_len_ptr)++;
    return next;
}

/* Reset KV cache for all attention layers */
static void reset_cache(Model* m) {
    for (int i = 0; i < m->n_layers; i++) {
        Layer* l = &m->layers[i];
        if (l->type == LAYER_ATTENTION) {
            aurora_tensor_free(l->kv_cache_k); l->kv_cache_k = nullptr;
            aurora_tensor_free(l->kv_cache_v); l->kv_cache_v = nullptr;
            l->cache_len = 0;
        }
    }
}

/*
 * Speculative decoding: use draft model to propose K tokens,
 * then verify with target model's distribution.
 */
char* speculative_decode(Model* target, Model* draft, int64_t start_token, int64_t max_tokens, int64_t K) {
    if (!target || !draft || K < 1) return AURORA_STRDUP("");
    if (K > 10) K = 10;

    /* Reset both KV caches */
    reset_cache(target);
    reset_cache(draft);

    char buf[4096];
    int64_t pos = 0;
    int64_t cur_tok = start_token;
    int64_t target_clen = 0, draft_clen = 0;

    for (int64_t step = 0; step < max_tokens && pos < 4000; ) {
        /* ── Draft phase: generate K candidate tokens ── */
        int64_t draft_tokens[10];
        int64_t nd = 0;
        for (int64_t i = 0; i < K; i++) {
            int64_t tok = draft_step(draft, cur_tok, &draft_clen);
            draft_tokens[nd++] = tok;
            cur_tok = tok;
            if (tok == 0) break;
        }
        if (nd == 0) break;

        /* ── Verification phase: run target model ── */
        /* We need to verify all draft tokens. Run target model forward
           on the last accepted token + draft tokens one at a time,
           comparing target distribution vs what draft predicted. */

        int accept_count = 0;
        /* Reset target to re-evaluate from original position */
        reset_cache(target);
        target_clen = 0;

        int64_t verify_tok = start_token;
        for (int64_t i = 0; i < nd; i++) {
            /* Run target model on this position */
            int64_t is[2] = {1, 1};
            AuroraTensor* in = aurora_tensor_new(2, is);
            in->data[0] = (double)verify_tok;
            AuroraTensor* out = model_forward(target, in, 0);
            aurora_tensor_free(in);
            if (!out) break;

            int64_t vocab = out->shape[1];
            double* logits = &out->data[(out->shape[0] - 1) * vocab];
            /* Softmax target */
            double maxv = logits[0];
            for (int64_t j = 1; j < vocab; j++) if (logits[j] > maxv) maxv = logits[j];
            double sum = 0.0;
            for (int64_t j = 0; j < vocab; j++) { logits[j] = exp(logits[j] - maxv); sum += logits[j]; }
            if (sum > 1e-15) for (int64_t j = 0; j < vocab; j++) logits[j] /= sum;

            double p_target = logits[draft_tokens[i]];
            /* Rejection sampling: accept with probability min(1, p_target / p_draft)
               Since we don't have draft's full distribution here, use a simpler heuristic:
               accept if p_target > threshold */
            if (p_target > 0.01 || ((double)rand() / RAND_MAX) < p_target * 10.0) {
                accept_count++;
                verify_tok = draft_tokens[i];
                target_clen++;
                aurora_tensor_free(out);
                if (draft_tokens[i] == 0) break;
            } else {
                /* Rejection: sample from residual distribution */
                double r = (double)rand() / RAND_MAX;
                double cum = 0.0;
                int64_t replacement = 0;
                for (int64_t j = 0; j < vocab; j++) {
                    cum += logits[j];
                    if (r <= cum) { replacement = j; break; }
                }
                verify_tok = replacement;
                aurora_tensor_free(out);
                break;
            }
        }

        /* Append accepted tokens */
        for (int64_t i = 0; i < accept_count && pos < 4000; i++) {
            pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "%lld ", (long long)draft_tokens[i]);
            if (draft_tokens[i] == 0) break;
        }
        step += accept_count + 1;

        /* Update for next iteration */
        cur_tok = verify_tok;

        /* Sync draft model's position to target's (reset draft, fast-forward) */
        reset_cache(draft);
        draft_clen = 0;
        int64_t replay_tok = start_token;
        for (int64_t i = 0; i < accept_count && pos < 4000; i++) {
            draft_step(draft, replay_tok, &draft_clen);
            replay_tok = draft_tokens[i];
        }

        if (cur_tok == 0) break;
    }

    return AURORA_STRDUP(buf);
}

} /* extern "C" */
