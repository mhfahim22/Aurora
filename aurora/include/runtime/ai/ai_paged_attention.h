#pragma once
#include <cstdint>

#define PAGE_BLOCK_SIZE 16
#define MAX_SEQUENCES 64
#define MAX_BLOCKS 4096

/* Block table entry */
struct BlockEntry {
    int64_t block_id;
    int64_t filled;
};

/* Per-sequence paged KV cache state */
struct PagedSequence {
    int64_t seq_id;
    int64_t length;
    int64_t n_blocks;
    int64_t block_capacity;
    struct BlockEntry* block_table;
    int active;
};

/* Paged KV cache block storage */
struct PagedBlock {
    double* k_data;
    double* v_data;
    int64_t num_heads;
    int64_t head_dim;
    int64_t block_size;
    int allocated;
    int64_t next_free; /* index of next free block in free-list (-1 = end) */
};

/* PagedAttention manager */
struct PagedManager {
    struct PagedBlock blocks[MAX_BLOCKS];
    int n_blocks;
    int64_t free_head; /* index of first free block in free-list (-1 = none) */
    struct PagedSequence sequences[MAX_SEQUENCES];
    int n_sequences;
    int64_t block_size;
    int64_t num_heads;
    int64_t head_dim;
    int64_t current_seq_id; /* seq_id for the current model_forward call (continuous batching) */
};

/* ── API ── */
PagedManager* paged_manager_create(int64_t num_heads, int64_t head_dim, int64_t block_size);
void paged_manager_destroy(PagedManager* pm);

int64_t paged_sequence_new(PagedManager* pm);
void paged_sequence_free(PagedManager* pm, int64_t seq_id);

int paged_append_kv(PagedManager* pm, int64_t seq_id, double* k_ptr, double* v_ptr, int64_t n_heads, int64_t head_dim);

int paged_attention_forward(
    PagedManager* pm, int64_t seq_id,
    double* q,          /* [n_heads, head_dim] */
    double* output,     /* [n_heads, head_dim] */
    int64_t n_heads, int64_t head_dim, double scale
);

void paged_reset_all(PagedManager* pm);

/* For use in mha_forward: cache K,V for a layer */
void paged_cache_kv_for_layer(PagedManager* pm, int64_t seq_id, int layer_idx,
                               double* k, double* v, int64_t n_heads, int64_t head_dim);

/* For use in mha_forward: compute paged attention during inference */
void paged_attention_for_layer(PagedManager* pm, int64_t seq_id,
                                double* q, double* output,
                                int64_t n_heads, int64_t head_dim, double scale);

/* ── Continuous batching ── */
int paged_batch_add(PagedManager* pm, int64_t start_token);
int paged_batch_step(PagedManager* pm, int64_t* tokens_in, int64_t* tokens_out, int n_seqs,
                     double* logits_out, int64_t vocab_size, void* model_ptr);
int paged_batch_remove(PagedManager* pm, int64_t seq_id);
int paged_batch_num_active(PagedManager* pm);

/* ── Global active paged manager for inference — set by model_forward ── */
extern PagedManager* g_active_paged_mgr;
