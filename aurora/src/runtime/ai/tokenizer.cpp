#include "runtime/ai/tokenizer.hpp"
#include "std/json.hpp"
#include "common/platform.hpp"
#include <cstdio>
#include <cinttypes>

#define MAX_WORDS 65536
#define MAX_WORD_LEN 4096

typedef struct {
    char word[MAX_WORD_LEN];
    int64_t len;
    int64_t freq;
} WordEntry;

typedef struct {
    int64_t* tokens;
    int64_t len;
    int64_t cap;
} TokenSeq;

typedef struct {
    int64_t first;
    int64_t second;
    int64_t freq;
} PairCount;

static int64_t count_words(const char* text, WordEntry* entries, int64_t max_entries) {
    int64_t n = 0;
    const char* p = text;
    while (*p && n < max_entries) {
        while (*p && (unsigned char)*p <= 32) p++;
        if (!*p) break;
        const char* start = p;
        while (*p && (unsigned char)*p > 32) p++;
        int64_t wlen = (int64_t)(p - start);
        if (wlen > MAX_WORD_LEN - 1) wlen = MAX_WORD_LEN - 1;
        int64_t found = -1;
        for (int64_t i = 0; i < n; i++) {
            if (entries[i].len == wlen && memcmp(entries[i].word, start, (size_t)wlen) == 0) {
                found = i;
                break;
            }
        }
        if (found >= 0) {
            entries[found].freq++;
        } else {
            memcpy(entries[n].word, start, (size_t)wlen);
            entries[n].word[wlen] = '\0';
            entries[n].len = wlen;
            entries[n].freq = 1;
            n++;
        }
    }
    return n;
}

static void token_seq_init(TokenSeq* ts, const char* word, int64_t wlen) {
    ts->cap = wlen + 8;
    ts->tokens = (int64_t*)malloc((size_t)ts->cap * sizeof(int64_t));
    ts->len = wlen;
    for (int64_t i = 0; i < wlen; i++) {
        ts->tokens[i] = (unsigned char)word[i];
    }
}

static void token_seq_merge_all(TokenSeq* ts, int64_t first, int64_t second, int64_t new_id) {
    int64_t* old = ts->tokens;
    int64_t old_len = ts->len;
    int64_t* new_tok = (int64_t*)malloc((size_t)old_len * sizeof(int64_t));
    int64_t new_len = 0;
    int64_t i = 0;
    while (i < old_len) {
        if (i < old_len - 1 && old[i] == first && old[i + 1] == second) {
            new_tok[new_len++] = new_id;
            i += 2;
        } else {
            new_tok[new_len++] = old[i];
            i++;
        }
    }
    free(ts->tokens);
    ts->tokens = new_tok;
    ts->len = new_len;
    ts->cap = old_len;
}

int64_t bpe_train_impl(const char* text, int64_t vocab_size) {
    if (!text) return 0;
    if (vocab_size < 256) vocab_size = 256;

    BPETokenizer* tok = (BPETokenizer*)calloc(1, sizeof(BPETokenizer));
    if (!tok) return 0;

    tok->vocab_size = 256;
    tok->vocab = (char**)malloc(256 * sizeof(char*));
    for (int64_t i = 0; i < 256; i++) {
        tok->vocab[i] = (char*)malloc(2);
        tok->vocab[i][0] = (char)(unsigned char)i;
        tok->vocab[i][1] = '\0';
    }

    int64_t max_merges = vocab_size - 256;
    tok->num_merges = 0;
    tok->merges = (BPEMerge*)calloc((size_t)(max_merges > 0 ? max_merges : 1), sizeof(BPEMerge));
    tok->merge_rank = (int64_t*)malloc((size_t)(max_merges > 0 ? max_merges : 1) * sizeof(int64_t));

    if (max_merges <= 0) {
        /* Add BOS/EOS special tokens */
        tok->bos_id = tok->vocab_size;
        tok->eos_id = tok->vocab_size + 1;
        tok->vocab_size += 2;
        tok->vocab = (char**)realloc(tok->vocab, (size_t)tok->vocab_size * sizeof(char*));
        tok->vocab[tok->bos_id] = AURORA_STRDUP("<s>");
        tok->vocab[tok->eos_id] = AURORA_STRDUP("</s>");
        return (int64_t)tok;
    }

    WordEntry* words = (WordEntry*)calloc(MAX_WORDS, sizeof(WordEntry));
    int64_t n_words = count_words(text, words, MAX_WORDS);
    if (n_words <= 0) {
        free(words);
        return (int64_t)tok;
    }

    TokenSeq* seqs = (TokenSeq*)calloc((size_t)n_words, sizeof(TokenSeq));
    for (int64_t i = 0; i < n_words; i++) {
        token_seq_init(&seqs[i], words[i].word, words[i].len);
    }

    for (int64_t m = 0; m < max_merges; m++) {
        int64_t pair_cap = n_words * 8;
        PairCount* pairs = (PairCount*)calloc((size_t)pair_cap, sizeof(PairCount));
        int64_t n_pairs = 0;

        for (int64_t w = 0; w < n_words; w++) {
            int64_t freq = words[w].freq;
            for (int64_t i = 0; i < seqs[w].len - 1; i++) {
                int64_t a = seqs[w].tokens[i];
                int64_t b = seqs[w].tokens[i + 1];
                int64_t pidx = -1;
                for (int64_t p = 0; p < n_pairs; p++) {
                    if (pairs[p].first == a && pairs[p].second == b) {
                        pidx = p;
                        break;
                    }
                }
                if (pidx < 0) {
                    if (n_pairs >= pair_cap) {
                        pair_cap *= 2;
                        pairs = (PairCount*)realloc(pairs, (size_t)pair_cap * sizeof(PairCount));
                    }
                    pairs[n_pairs].first = a;
                    pairs[n_pairs].second = b;
                    pairs[n_pairs].freq = freq;
                    n_pairs++;
                } else {
                    pairs[pidx].freq += freq;
                }
            }
        }

        if (n_pairs == 0) {
            free(pairs);
            break;
        }

        int64_t best = 0;
        for (int64_t p = 1; p < n_pairs; p++) {
            if (pairs[p].freq > pairs[best].freq) best = p;
        }

        int64_t first_id = pairs[best].first;
        int64_t second_id = pairs[best].second;
        int64_t new_id = tok->vocab_size;

        size_t l1 = strlen(tok->vocab[first_id]);
        size_t l2 = strlen(tok->vocab[second_id]);
        char* merged = (char*)malloc(l1 + l2 + 1);
        memcpy(merged, tok->vocab[first_id], l1);
        memcpy(merged + l1, tok->vocab[second_id], l2);
        merged[l1 + l2] = '\0';

        tok->merges[tok->num_merges].id = new_id;
        tok->merges[tok->num_merges].first = first_id;
        tok->merges[tok->num_merges].second = second_id;
        tok->merges[tok->num_merges].token = merged;
        tok->merge_rank[tok->num_merges] = tok->num_merges;
        tok->num_merges++;

        tok->vocab_size++;
        tok->vocab = (char**)realloc(tok->vocab, (size_t)tok->vocab_size * sizeof(char*));
        tok->vocab[new_id] = (char*)malloc(l1 + l2 + 1);
        memcpy(tok->vocab[new_id], merged, l1 + l2 + 1);

        for (int64_t w = 0; w < n_words; w++) {
            token_seq_merge_all(&seqs[w], first_id, second_id, new_id);
        }

        free(pairs);
    }

    /* Add BOS/EOS special tokens at the end of the vocabulary */
    tok->bos_id = tok->vocab_size;
    tok->eos_id = tok->vocab_size + 1;
    tok->vocab_size += 2;
    tok->vocab = (char**)realloc(tok->vocab, (size_t)tok->vocab_size * sizeof(char*));
    tok->vocab[tok->bos_id] = AURORA_STRDUP("<s>");
    tok->vocab[tok->eos_id] = AURORA_STRDUP("</s>");

    for (int64_t i = 0; i < n_words; i++) free(seqs[i].tokens);
    free(seqs);
    free(words);

    return (int64_t)tok;
}

AuroraTensor* bpe_encode_impl(int64_t tokenizer_ptr, const char* text) {
    BPETokenizer* tok = (BPETokenizer*)tokenizer_ptr;
    if (!tok || !text) {
        int64_t zero = 0;
        return aurora_tensor_new(1, &zero);
    }

    int64_t text_len = (int64_t)strlen(text);
    if (text_len <= 0) {
        int64_t zero = 0;
        return aurora_tensor_new(1, &zero);
    }

    int64_t max_ids = text_len + 2;
    int64_t* all_ids = (int64_t*)malloc((size_t)max_ids * sizeof(int64_t));
    int64_t n_all = 0;

    /* Prepend BOS token */
    all_ids[n_all++] = tok->bos_id;

    const char* p = text;
    while (*p) {
        while (*p && (unsigned char)*p <= 32) p++;
        if (!*p) break;
        const char* word_start = p;
        while (*p && (unsigned char)*p > 32) p++;
        int64_t wlen = (int64_t)(p - word_start);

        int64_t* word_tokens = (int64_t*)malloc((size_t)wlen * sizeof(int64_t));
        int64_t word_len = wlen;
        for (int64_t i = 0; i < wlen; i++) {
            word_tokens[i] = (unsigned char)word_start[i];
        }

        for (int64_t mi = 0; mi < tok->num_merges; mi++) {
            int64_t first = tok->merges[mi].first;
            int64_t second = tok->merges[mi].second;
            int64_t new_id = tok->merges[mi].id;

            int64_t* new_tok = (int64_t*)malloc((size_t)word_len * sizeof(int64_t));
            int64_t new_len = 0;
            int64_t i = 0;
            while (i < word_len) {
                if (i < word_len - 1 && word_tokens[i] == first && word_tokens[i + 1] == second) {
                    new_tok[new_len++] = new_id;
                    i += 2;
                } else {
                    new_tok[new_len++] = word_tokens[i];
                    i++;
                }
            }
            free(word_tokens);
            word_tokens = new_tok;
            word_len = new_len;
        }

        if (n_all + word_len > max_ids) {
            max_ids = n_all + word_len + 256;
            all_ids = (int64_t*)realloc(all_ids, (size_t)max_ids * sizeof(int64_t));
        }
        for (int64_t i = 0; i < word_len; i++) {
            all_ids[n_all++] = word_tokens[i];
        }
        free(word_tokens);
    }

    int64_t shape[2] = { n_all, 1 };
    AuroraTensor* result = aurora_tensor_new(2, shape);
    for (int64_t i = 0; i < n_all; i++) {
        result->data[i] = (double)all_ids[i];
    }

    free(all_ids);
    return result;
}

char* bpe_decode_impl(int64_t tokenizer_ptr, int64_t* ids, int64_t n_ids) {
    BPETokenizer* tok = (BPETokenizer*)tokenizer_ptr;
    if (!tok || !ids || n_ids <= 0) {
        char* empty = (char*)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }

    size_t total = 1;
    for (int64_t i = 0; i < n_ids; i++) {
        int64_t id = ids[i];
        if (id >= 0 && id < tok->vocab_size) {
            total += strlen(tok->vocab[id]);
        } else {
            total++;
        }
    }

    char* result = (char*)malloc(total);
    if (!result) return nullptr;
    size_t pos = 0;

    for (int64_t i = 0; i < n_ids; i++) {
        int64_t id = ids[i];
        if (id >= 0 && id < tok->vocab_size) {
            size_t tlen = strlen(tok->vocab[id]);
            memcpy(result + pos, tok->vocab[id], tlen);
            pos += tlen;
        }
    }
    result[pos] = '\0';

    return result;
}

void bpe_free_impl(int64_t tokenizer_ptr) {
    BPETokenizer* tok = (BPETokenizer*)tokenizer_ptr;
    if (!tok) return;
    for (int64_t i = 0; i < tok->vocab_size; i++) {
        free(tok->vocab[i]);
    }
    free(tok->vocab);
    for (int64_t i = 0; i < tok->num_merges; i++) {
        free(tok->merges[i].token);
    }
    free(tok->merges);
    free(tok->merge_rank);
    free(tok);
}

/* ════════════════════════════════════════════════════════════
   Pre-trained Tokenizer Loading
   ════════════════════════════════════════════════════════════ */

/* Load GPT-2 BPE tokenizer from encoder.json + vocab.bpe files.
   Returns 0 on failure, BPETokenizer pointer on success. */
int64_t bpe_load_gpt2(const char* encoder_path, const char* merges_path) {
    if (!encoder_path || !merges_path) return 0;

    /* Read encoder.json */
    FILE* f_enc = fopen(encoder_path, "rb");
    if (!f_enc) return 0;
    fseek(f_enc, 0, SEEK_END);
    size_t enc_size = (size_t)ftell(f_enc);
    fseek(f_enc, 0, SEEK_SET);
    char* enc_data = (char*)malloc(enc_size + 1);
    if (!enc_data) { fclose(f_enc); return 0; }
    fread(enc_data, 1, enc_size, f_enc);
    enc_data[enc_size] = '\0';
    fclose(f_enc);

    /* Parse JSON */
    JsonValue* enc_json = aurora_json_parse(enc_data);
    free(enc_data);
    if (!enc_json || enc_json->type != JSON_OBJECT) {
        if (enc_json) aurora_json_free(enc_json);
        return 0;
    }

    int64_t vocab_size = enc_json->count;

    /* Read vocab.bpe (merges) */
    FILE* f_merges = fopen(merges_path, "rb");
    if (!f_merges) { aurora_json_free(enc_json); return 0; }
    fseek(f_merges, 0, SEEK_END);
    size_t merges_size = (size_t)ftell(f_merges);
    fseek(f_merges, 0, SEEK_SET);
    char* merges_data = (char*)malloc(merges_size + 1);
    if (!merges_data) { fclose(f_merges); aurora_json_free(enc_json); return 0; }
    fread(merges_data, 1, merges_size, f_merges);
    merges_data[merges_size] = '\0';
    fclose(f_merges);

    /* Count merge lines */
    int64_t num_merges = 0;
    char* ml = merges_data;
    while (*ml) {
        if (num_merges == 0 && ml[0] == '#') {
            while (*ml && *ml != '\n') ml++;
            if (*ml == '\n') ml++;
            continue;
        }
        if (*ml == '\n' || *ml == '\r') { ml++; continue; }
        char* lp = ml;
        while (*lp && *lp != '\n' && *lp != '\r') lp++;
        if (lp > ml) num_merges++;
        ml = lp;
        if (*ml == '\n') ml++;
    }

    BPETokenizer* tok = (BPETokenizer*)calloc(1, sizeof(BPETokenizer));
    if (!tok) { free(merges_data); aurora_json_free(enc_json); return 0; }

    /* Add 2 slots for BOS/EOS */
    tok->vocab_size = vocab_size + 2;
    tok->vocab = (char**)calloc((size_t)tok->vocab_size, sizeof(char*));

    /* Fill vocabulary from JSON */
    for (int i = 0; i < enc_json->count; i++) {
        int64_t id = (int64_t)enc_json->items[i]->num_val;
        if (id >= 0 && id < vocab_size) {
            if (tok->vocab[id]) free(tok->vocab[id]);
            tok->vocab[id] = AURORA_STRDUP(enc_json->keys[i]);
        }
    }

    /* BOS/EOS tokens */
    tok->bos_id = vocab_size;
    tok->eos_id = vocab_size + 1;
    tok->vocab[tok->bos_id] = AURORA_STRDUP("<s>");
    tok->vocab[tok->eos_id] = AURORA_STRDUP("</s>");

    /* Parse merge rules */
    tok->num_merges = num_merges;
    tok->merges = (BPEMerge*)calloc((size_t)num_merges, sizeof(BPEMerge));
    tok->merge_rank = (int64_t*)malloc((size_t)num_merges * sizeof(int64_t));

    ml = merges_data;
    int64_t mi = 0;
    int64_t next_id = 256; /* GPT-2 starts merged tokens at 256 */
    while (*ml && mi < num_merges) {
        /* Skip header */
        if (mi == 0 && ml[0] == '#') {
            while (*ml && *ml != '\n') ml++;
            if (*ml == '\n') ml++;
            continue;
        }
        if (*ml == '\n' || *ml == '\r') { ml++; continue; }
        char* line_start = ml;
        while (*ml && *ml != '\n' && *ml != '\r') ml++;
        size_t line_len = (size_t)(ml - line_start);
        if (line_len > 0) {
            char* space = (char*)memchr(line_start, ' ', line_len);
            if (space) {
                size_t first_len = (size_t)(space - line_start);
                size_t second_len = line_len - first_len - 1;
                char* first_s = (char*)malloc(first_len + 1);
                char* second_s = (char*)malloc(second_len + 1);
                if (first_s && second_s) {
                    memcpy(first_s, line_start, first_len);
                    first_s[first_len] = '\0';
                    memcpy(second_s, space + 1, second_len);
                    second_s[second_len] = '\0';

                    /* Find token IDs from vocab */
                    int64_t first_id = -1, second_id = -1;
                    for (int64_t vi = 0; vi < vocab_size; vi++) {
                        if (tok->vocab[vi] && strcmp(tok->vocab[vi], first_s) == 0) { first_id = vi; break; }
                    }
                    for (int64_t vi = 0; vi < vocab_size; vi++) {
                        if (tok->vocab[vi] && strcmp(tok->vocab[vi], second_s) == 0) { second_id = vi; break; }
                    }

                    if (first_id >= 0 && second_id >= 0) {
                        size_t l1 = strlen(first_s), l2 = strlen(second_s);
                        char* merged = (char*)malloc(l1 + l2 + 1);
                        memcpy(merged, first_s, l1);
                        memcpy(merged + l1, second_s, l2);
                        merged[l1 + l2] = '\0';
                        tok->merges[mi].id = next_id++;
                        tok->merges[mi].first = first_id;
                        tok->merges[mi].second = second_id;
                        tok->merges[mi].token = merged;
                        tok->merge_rank[mi] = mi;
                        mi++;
                    }
                    free(first_s);
                    free(second_s);
                }
            }
        }
        if (*ml == '\n') ml++;
    }
    tok->num_merges = mi;

    free(merges_data);
    aurora_json_free(enc_json);
    return (int64_t)tok;
}

/* Load HuggingFace tokenizer.json format.
   Returns 0 on failure, BPETokenizer pointer on success. */
int64_t bpe_load_json(const char* tokenizer_json_path) {
    if (!tokenizer_json_path) return 0;

    FILE* f = fopen(tokenizer_json_path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    size_t fsize = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    char* data = (char*)malloc(fsize + 1);
    if (!data) { fclose(f); return 0; }
    fread(data, 1, fsize, f);
    data[fsize] = '\0';
    fclose(f);

    JsonValue* root = aurora_json_parse(data);
    free(data);
    if (!root || root->type != JSON_OBJECT) {
        if (root) aurora_json_free(root);
        return 0;
    }

    /* Navigate to model.vocab and model.merges */
    JsonValue* model = nullptr;
    for (int i = 0; i < root->count; i++) {
        if (strcmp(root->keys[i], "model") == 0) { model = root->items[i]; break; }
    }
    if (!model || model->type != JSON_OBJECT) { aurora_json_free(root); return 0; }

    JsonValue* vocab_json = nullptr;
    JsonValue* merges_json = nullptr;
    for (int i = 0; i < model->count; i++) {
        if (strcmp(model->keys[i], "vocab") == 0) vocab_json = model->items[i];
        if (strcmp(model->keys[i], "merges") == 0) merges_json = model->items[i];
    }
    if (!vocab_json || !merges_json) { aurora_json_free(root); return 0; }

    int64_t vocab_size = (vocab_json->type == JSON_OBJECT) ? vocab_json->count : 0;
    int64_t num_merges = (merges_json->type == JSON_ARRAY) ? merges_json->count : 0;

    BPETokenizer* tok = (BPETokenizer*)calloc(1, sizeof(BPETokenizer));
    if (!tok) { aurora_json_free(root); return 0; }

    int64_t total_vocab = vocab_size + 2 + num_merges;
    tok->vocab_size = total_vocab;
    tok->vocab = (char**)calloc((size_t)tok->vocab_size, sizeof(char*));

    if (vocab_json->type == JSON_OBJECT) {
        for (int i = 0; i < vocab_json->count; i++) {
            int64_t id = (int64_t)vocab_json->items[i]->num_val;
            if (id >= 0 && id < vocab_size) {
                if (tok->vocab[id]) free(tok->vocab[id]);
                tok->vocab[id] = AURORA_STRDUP(vocab_json->keys[i]);
            }
        }
    }

    /* Parse merges first to assign IDs */
    tok->num_merges = num_merges;
    tok->merges = (BPEMerge*)calloc((size_t)(num_merges > 0 ? num_merges : 1), sizeof(BPEMerge));
    tok->merge_rank = (int64_t*)malloc((size_t)(num_merges > 0 ? num_merges : 1) * sizeof(int64_t));

    int64_t next_id = vocab_size;
    for (int64_t mi = 0; mi < num_merges; mi++) {
        if (merges_json->items[mi]->type == JSON_STR) {
            const char* merge_str = merges_json->items[mi]->str_val;
            if (!merge_str) continue;
            const char* space = strchr(merge_str, ' ');
            if (!space) continue;
            size_t first_len = (size_t)(space - merge_str);
            size_t second_len = strlen(merge_str) - first_len - 1;

            char first_s[256], second_s[256];
            if (first_len >= 256 || second_len >= 256) continue;
            memcpy(first_s, merge_str, first_len);
            first_s[first_len] = '\0';
            memcpy(second_s, space + 1, second_len);
            second_s[second_len] = '\0';

            int64_t first_id = -1, second_id = -1;
            for (int64_t vi = 0; vi < vocab_size; vi++) {
                if (tok->vocab[vi] && strcmp(tok->vocab[vi], first_s) == 0) { first_id = vi; break; }
            }
            for (int64_t vi = 0; vi < vocab_size; vi++) {
                if (tok->vocab[vi] && strcmp(tok->vocab[vi], second_s) == 0) { second_id = vi; break; }
            }

            if (first_id >= 0 && second_id >= 0) {
                size_t l1 = strlen(first_s), l2 = strlen(second_s);
                char* merged = (char*)malloc(l1 + l2 + 1);
                memcpy(merged, first_s, l1);
                memcpy(merged + l1, second_s, l2);
                merged[l1 + l2] = '\0';
                tok->merges[mi].id = next_id;
                tok->merges[mi].first = first_id;
                tok->merges[mi].second = second_id;
                tok->merges[mi].token = merged;
                tok->merge_rank[mi] = mi;
                tok->vocab[next_id] = AURORA_STRDUP(merged);
                next_id++;
            }
        }
    }

    /* BOS/EOS */
    if (next_id + 2 <= total_vocab) {
        tok->bos_id = next_id;
        tok->eos_id = next_id + 1;
        tok->vocab[tok->bos_id] = AURORA_STRDUP("<s>");
        tok->vocab[tok->eos_id] = AURORA_STRDUP("</s>");
    } else {
        tok->bos_id = 0;
        tok->eos_id = 1;
    }

    aurora_json_free(root);
    return (int64_t)tok;
}
