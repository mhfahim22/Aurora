#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "runtime/tensor.hpp"

typedef struct {
    int64_t id;          // token ID of the merged result
    int64_t first;       // first component token ID
    int64_t second;      // second component token ID
    char* token;         // the merged token string
} BPEMerge;

typedef struct {
    int64_t vocab_size;
    int64_t num_merges;
    int64_t bos_id;
    int64_t eos_id;
    char** vocab;        // list of token strings
    BPEMerge* merges;    // merge rules: pair -> new token
    int64_t* merge_rank; // priority of each merge
} BPETokenizer;

// Core implementation (C++ linkage, const char* strings)
int64_t bpe_train_impl(const char* text, int64_t vocab_size);
AuroraTensor* bpe_encode_impl(int64_t tokenizer_ptr, const char* text);
char* bpe_decode_impl(int64_t tokenizer_ptr, int64_t* ids, int64_t n_ids);
void bpe_free_impl(int64_t tokenizer_ptr);

// Pre-trained tokenizer loading
int64_t bpe_load_gpt2(const char* encoder_path, const char* merges_path);
int64_t bpe_load_json(const char* tokenizer_json_path);
