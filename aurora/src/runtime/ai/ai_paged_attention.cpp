#include "runtime/ai/ai_paged_attention.h"
#include "runtime/ai/ai_common.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdio>

/* ── Manager ── */

PagedManager* paged_manager_create(int64_t num_heads, int64_t head_dim, int64_t block_size) {
    PagedManager* pm = (PagedManager*)calloc(1, sizeof(PagedManager));
    if (!pm) return nullptr;
    pm->n_blocks = 0;
    pm->n_sequences = 0;
    pm->free_head = -1;
    pm->block_size = (block_size > 0) ? block_size : PAGE_BLOCK_SIZE;
    pm->num_heads = num_heads;
    pm->head_dim = head_dim;
    pm->current_seq_id = 0;
    return pm;
}

void paged_manager_destroy(PagedManager* pm) {
    if (!pm) return;
    for (int i = 0; i < MAX_BLOCKS; i++) {
        if (pm->blocks[i].allocated) {
            free(pm->blocks[i].k_data);
            free(pm->blocks[i].v_data);
        }
    }
    for (int i = 0; i < MAX_SEQUENCES; i++) {
        if (pm->sequences[i].block_table)
            free(pm->sequences[i].block_table);
    }
    free(pm);
}

/* ── Block allocation ── */

static int alloc_block(PagedManager* pm) {
    int idx;
    if (pm->free_head >= 0) {
        idx = (int)pm->free_head;
        pm->free_head = pm->blocks[idx].next_free;
    } else {
        if (pm->n_blocks >= MAX_BLOCKS) return -1;
        idx = pm->n_blocks++;
    }
    PagedBlock* b = &pm->blocks[idx];
    int64_t elem = pm->num_heads * pm->head_dim * pm->block_size;
    b->k_data = (double*)calloc((size_t)elem, sizeof(double));
    b->v_data = (double*)calloc((size_t)elem, sizeof(double));
    if (!b->k_data || !b->v_data) { free(b->k_data); free(b->v_data); return -1; }
    b->num_heads = pm->num_heads;
    b->head_dim = pm->head_dim;
    b->block_size = pm->block_size;
    b->allocated = 1;
    b->next_free = -1;
    return idx;
}

/* ── Sequence management ── */

int64_t paged_sequence_new(PagedManager* pm) {
    if (!pm || pm->n_sequences >= MAX_SEQUENCES) return -1;
    int idx = pm->n_sequences++;
    PagedSequence* s = &pm->sequences[idx];
    s->seq_id = idx;
    s->length = 0;
    s->n_blocks = 0;
    s->block_capacity = 4;
    s->block_table = (BlockEntry*)calloc((size_t)s->block_capacity, sizeof(BlockEntry));
    if (!s->block_table) return -1;
    s->active = 1;
    return idx;
}

static void free_block(PagedManager* pm, int idx) {
    if (idx < 0 || idx >= MAX_BLOCKS) return;
    PagedBlock* b = &pm->blocks[idx];
    if (!b->allocated) return;
    free(b->k_data); b->k_data = nullptr;
    free(b->v_data); b->v_data = nullptr;
    b->allocated = 0;
    b->next_free = pm->free_head;
    pm->free_head = idx;
}

void paged_sequence_free(PagedManager* pm, int64_t seq_id) {
    if (!pm || seq_id < 0 || seq_id >= MAX_SEQUENCES) return;
    PagedSequence* s = &pm->sequences[seq_id];
    for (int64_t i = 0; i < s->n_blocks; i++) {
        free_block(pm, (int)s->block_table[i].block_id);
    }
    s->active = 0;
    s->length = 0;
    s->n_blocks = 0;
    /* blocks already returned to free list by free_block() */
}

/* ── Append KV ── */

int paged_append_kv(PagedManager* pm, int64_t seq_id, double* k_ptr, double* v_ptr,
                     int64_t n_heads, int64_t head_dim) {
    if (!pm || seq_id < 0 || seq_id >= MAX_SEQUENCES) return 0;
    PagedSequence* s = &pm->sequences[seq_id];
    if (!s->active) return 0;

    int64_t blk_idx = s->length / pm->block_size;
    int64_t pos_in_blk = s->length % pm->block_size;

    /* Expand block table if needed */
    if (blk_idx >= s->block_capacity) {
        int64_t new_cap = s->block_capacity * 2;
        BlockEntry* tmp = (BlockEntry*)realloc(s->block_table, (size_t)new_cap * sizeof(BlockEntry));
        if (!tmp) return 0;
        memset(tmp + s->block_capacity, 0, (size_t)(new_cap - s->block_capacity) * sizeof(BlockEntry));
        s->block_table = tmp;
        s->block_capacity = new_cap;
    }

    /* Allocate new physical block if needed */
    if (pos_in_blk == 0) {
        int phys = alloc_block(pm);
        if (phys < 0) return 0;
        s->block_table[blk_idx].block_id = phys;
    }

    int phys_id = (int)s->block_table[blk_idx].block_id;
    if (phys_id < 0 || phys_id >= MAX_BLOCKS) return 0;
    PagedBlock* blk = &pm->blocks[phys_id];

    /* Copy K: [n_heads, head_dim] -> block slot */
    int64_t slot_offset = pos_in_blk * n_heads * head_dim;
    memcpy(blk->k_data + slot_offset, k_ptr, (size_t)(n_heads * head_dim) * sizeof(double));
    memcpy(blk->v_data + slot_offset, v_ptr, (size_t)(n_heads * head_dim) * sizeof(double));

    s->block_table[blk_idx].filled = pos_in_blk + 1;
    s->length++;
    return 1;
}

/* ── Paged Attention Forward ── */

int paged_attention_forward(
    PagedManager* pm, int64_t seq_id,
    double* q, double* output,
    int64_t n_heads, int64_t head_dim, double scale)
{
    if (!pm || seq_id < 0 || seq_id >= MAX_SEQUENCES) return 0;
    PagedSequence* s = &pm->sequences[seq_id];
    if (!s->active || s->length == 0) return 0;

    /* For each head, compute attention over cached KV pages */
    for (int64_t h = 0; h < n_heads; h++) {
        double* q_h = q + h * head_dim;

        /* Online softmax accumulators */
        double m = -1e18;
        double d = 0.0;
        double* o_h = output + h * head_dim;
        memset(o_h, 0, (size_t)head_dim * sizeof(double));

        /* Iterate over all blocks */
        int64_t n_full_blks = s->length / pm->block_size;
        int64_t last_blk_fill = s->length % pm->block_size;
        if (last_blk_fill == 0 && n_full_blks > 0) { n_full_blks--; last_blk_fill = pm->block_size; }

        for (int64_t bi = 0; bi <= n_full_blks; bi++) {
            int64_t phys_id = s->block_table[bi].block_id;
            if (phys_id < 0) continue;
            PagedBlock* blk = &pm->blocks[phys_id];
            int64_t fill = (bi == n_full_blks) ? last_blk_fill : pm->block_size;

            double* k_blk = blk->k_data;
            double* v_blk = blk->v_data;

            /* For each token in the block */
            for (int64_t ti = 0; ti < fill; ti++) {
                double* k_h = k_blk + (ti * n_heads + h) * head_dim;
                double* v_h = v_blk + (ti * n_heads + h) * head_dim;

                /* score = q_h · k_h * scale */
                double score = 0.0;
                for (int64_t i = 0; i < head_dim; i++)
                    score += q_h[i] * k_h[i];
                score *= scale;

                /* Online softmax update */
                double m_new = (score > m) ? score : m;
                double exp_old = exp(m - m_new);
                double exp_cur = exp(score - m_new);

                d = d * exp_old + exp_cur;

                /* Update output */
                for (int64_t i = 0; i < head_dim; i++)
                    o_h[i] = o_h[i] * exp_old + exp_cur * v_h[i];

                m = m_new;
            }
        }

        /* Normalize */
        if (d > 1e-15) {
            for (int64_t i = 0; i < head_dim; i++)
                o_h[i] /= d;
        }
    }
    return 1;
}

void paged_reset_all(PagedManager* pm) {
    if (!pm) return;
    for (int i = 0; i < MAX_SEQUENCES; i++) {
        if (pm->sequences[i].active) paged_sequence_free(pm, i);
    }
    pm->n_sequences = 0;
    pm->free_head = -1;
    for (int i = 0; i < MAX_BLOCKS; i++) {
        PagedBlock* b = &pm->blocks[i];
        free(b->k_data); b->k_data = nullptr;
        free(b->v_data); b->v_data = nullptr;
        b->allocated = 0;
        b->next_free = -1;
    }
    pm->n_blocks = 0;
}

/* ── Continuous Batching ── */

#define BATCH_MAX 64

int paged_batch_add(PagedManager* pm, int64_t start_token) {
    (void)start_token;
    return (int)paged_sequence_new(pm);
}

int paged_batch_step(PagedManager* pm, int64_t* tokens_in, int64_t* tokens_out, int n_seqs,
                     double* logits_out, int64_t vocab_size, void* model_ptr)
{
    if (!pm || !tokens_in || !tokens_out || !model_ptr) return 0;
    Model* m = (Model*)model_ptr;

    /* For each active sequence in the batch: batch index = seq_id */
    for (int si = 0; si < n_seqs && si < BATCH_MAX; si++) {
        int64_t seq_id = si;
        if (seq_id < 0 || seq_id >= MAX_SEQUENCES || !pm->sequences[seq_id].active) {
            tokens_out[si] = 0;
            continue;
        }

        /* Set current sequence ID so mha_forward uses the right seq_id */
        pm->current_seq_id = seq_id;

        /* Build input tensor for this sequence */
        int64_t token_val = tokens_in[si];
        int64_t is[2] = { 1, 1 };
        AuroraTensor* in = aurora_tensor_new(2, is);
        in->data[0] = (double)token_val;

        /* Forward pass */
        AuroraTensor* out = model_forward(m, in, 0);
        if (!out) { aurora_tensor_free(in); tokens_out[si] = 0; continue; }

        /* Extract logits for the last position */
        int64_t vocab = out->shape[1];
        double* logits = &out->data[(out->shape[0] - 1) * vocab];

        /* Copy logits if requested */
        if (logits_out) {
            memcpy(logits_out + si * vocab_size, logits, (size_t)vocab * sizeof(double));
        }

        /* Sample next token */
        double maxv = logits[0];
        for (int64_t i = 1; i < vocab; i++) if (logits[i] > maxv) maxv = logits[i];
        double sum = 0.0;
        for (int64_t i = 0; i < vocab; i++) { logits[i] = exp(logits[i] - maxv); sum += logits[i]; }
        if (sum > 1e-15) for (int64_t i = 0; i < vocab; i++) logits[i] /= sum;
        double r = (double)rand() / RAND_MAX;
        double cum = 0.0;
        int64_t next = 0;
        for (int64_t i = 0; i < vocab; i++) { cum += logits[i]; if (r <= cum) { next = i; break; } }

        tokens_out[si] = next;
        aurora_tensor_free(in);
        aurora_tensor_free(out);
    }
    return n_seqs;
}

int paged_batch_remove(PagedManager* pm, int64_t seq_id) {
    if (!pm || seq_id < 0 || seq_id >= MAX_SEQUENCES) return 0;
    pm->sequences[seq_id].active = 0;
    return 1;
}

int paged_batch_num_active(PagedManager* pm) {
    if (!pm) return 0;
    int cnt = 0;
    for (int i = 0; i < MAX_SEQUENCES; i++)
        if (pm->sequences[i].active) cnt++;
    return cnt;
}

/* ── Model integration helpers ── */

/* For use in mha_forward: cache K,V during inference */
void paged_cache_kv_for_layer(PagedManager* pm, int64_t seq_id, int layer_idx,
                               double* k, double* v, int64_t n_heads, int64_t head_dim)
{
    (void)layer_idx;
    if (pm && seq_id >= 0)
        paged_append_kv(pm, seq_id, k, v, n_heads, head_dim);
}

/* For use in mha_forward: compute paged attention during inference */
void paged_attention_for_layer(PagedManager* pm, int64_t seq_id,
                                double* q, double* output,
                                int64_t n_heads, int64_t head_dim, double scale)
{
    if (pm && seq_id >= 0)
        paged_attention_forward(pm, seq_id, q, output, n_heads, head_dim, scale);
}
