#include "runtime/ai/ai_common.h"
#include "runtime/ai/tokenizer.hpp"
#include "runtime/ai/ai_distributed.h"

extern "C" {
    /* Forward declarations for cross-references within this file */
    char* text(const void* path_a);
    int64_t csv(const void* path_a);

    /* Functions defined in ai_builtins.cpp */
    int64_t dense(int64_t units, const void* activation_name);
    int64_t add(int64_t model_ptr, int64_t layer_ptr);
    int64_t train(int64_t model_ptr, int64_t data_ptr);

    /* Functions defined in ai_train.cpp */
    double loss_compute(AuroraTensor* pred, AuroraTensor* target, int loss_type, AuroraTensor* dloss);
    double metric_accuracy(AuroraTensor* pred, AuroraTensor* target);
    void clip_gradients(Model* m, double max_norm);
    void augment_data(AuroraTensor* data, int64_t nf, int64_t ns);
    double lr_reduce_on_plateau(double current_lr, double factor, double min_lr);
    double train_batch(Model* m, AuroraTensor* X, AuroraTensor* y, OptimState* opt);
    void distributed_sync_gradients(Model* m);
    void distributed_broadcast_weights(Model* m, int root);

    /* Functions defined in ai_train.cpp */
    int64_t transformer_block(Model* m, int64_t embed_dim, int64_t num_heads, int64_t ff_dim);
    void optim_add_optim(int type, int64_t param_count, double lr);
    int model_auto_setup(Model* m, int64_t data_ptr);
    int activation_from_name(const char* name);
    int loss_from_name(const char* name);
    int optim_from_name(const char* name);
    int optim_save(OptimState* o, const char* path);
    int optim_load(OptimState* o, const char* path);

    /* Functions defined in ai_model.cpp */
    int64_t model_create(const void* type_a);
    int64_t model_save(int64_t model_ptr, const void* path_a);
    int64_t model_load(const void* path_a);
}

/* Globals defined in other .cpp files */
extern OptimState g_optims[8];
extern int g_n_optims;
extern double g_temperature;
extern double g_top_p;
extern int64_t g_max_tokens;
extern int64_t* g_vstore;
extern int64_t g_vcnt;
extern int64_t g_vcap;
extern Model* g_active_model;
extern int64_t g_active_tokenizer;

/* Static helpers used by AI functions below */

static const char* pos_words[] = {"good","great","excellent","amazing","happy","love","wonderful","fantastic","positive","best","beautiful","nice","perfect","outstanding","superb","brilliant",nullptr};
static const char* neg_words[] = {"bad","terrible","awful","horrible","hate","ugly","worst","negative","poor","sad","angry","evil","disgusting","dreadful","atrocious",nullptr};

static int in_list(const char* w, const char** l) { for (int i = 0; l[i]; i++) if (strcmp(w, l[i]) == 0) return 1; return 0; }
static void to_lower(char* s) { for (; *s; s++) if (*s >= 'A' && *s <= 'Z') *s = *s - 'A' + 'a'; }

extern "C" {

/* === AI/NLP === */

char* ai(const void* prompt_a);

char* generate(const void* prompt_a) {
    const char* p = aurora_str_ptr(prompt_a);
    if (!p) return AURORA_STRDUP("");

    if (!g_active_model) {
        char buf[256];
        snprintf(buf, sizeof(buf), "No model loaded. Train one first.\n");
        return AURORA_STRDUP(buf);
    }

    int64_t* token_ids = nullptr;
    int64_t n_tokens = 0;

    if (g_active_tokenizer) {
        AuroraTensor* encoded = bpe_encode_impl(g_active_tokenizer, p);
        if (encoded && encoded->total_size > 0) {
            n_tokens = encoded->total_size;
            token_ids = (int64_t*)malloc((size_t)n_tokens * sizeof(int64_t));
            for (int64_t i = 0; i < n_tokens; i++) token_ids[i] = (int64_t)encoded->data[i];
            aurora_tensor_free(encoded);
        }
    }

    reset_kv_cache(g_active_model);
    int64_t clen = 0;
    int64_t max_toks = g_max_tokens > 0 ? g_max_tokens : 256;
    int64_t* out_ids = (int64_t*)malloc((size_t)max_toks * sizeof(int64_t));
    int64_t n_out = 0;

    if (token_ids && n_tokens > 0) {
        for (int64_t i = 0; i < n_tokens - 1; i++) {
            generate_step(g_active_model, token_ids[i], &clen);
        }
        int64_t tok = token_ids[n_tokens - 1];
        for (int64_t i = 0; i < max_toks; i++) {
            tok = generate_step(g_active_model, tok, &clen);
            if (tok == 0 || tok == 2) break;
            out_ids[n_out++] = tok;
        }
    } else {
        int64_t start = (int64_t)p[0];
        int64_t tok = start;
        for (int64_t i = 0; i < max_toks; i++) {
            tok = generate_step(g_active_model, tok, &clen);
            if (tok == 0 || tok == 2) break;
            out_ids[n_out++] = tok;
        }
    }
    free(token_ids);

    char* result = nullptr;
    if (g_active_tokenizer && n_out > 0) {
        result = bpe_decode_impl(g_active_tokenizer, out_ids, n_out);
    } else {
        char buf[4096]; int64_t pos = 0;
        for (int64_t i = 0; i < n_out && pos < 4000; i++)
            pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "%lld ", (long long)out_ids[i]);
        result = AURORA_STRDUP(buf);
    }
    free(out_ids);
    if (!result) result = AURORA_STRDUP("");
    return result;
}

char* ai(const void* prompt_a) {
    return generate(prompt_a);
}

char* explain(const void* code_a) {
    const char* c = aurora_str_ptr(code_a); if (!c) return AURORA_STRDUP("");
    if (g_active_model) {
        char prompt[4096]; snprintf(prompt, sizeof(prompt), "Explain code: %.800s", c);
        char* r = model_generate(g_active_model, g_active_tokenizer, prompt, 128);
        if (r) return r;
    }
    char buf[512]; snprintf(buf, sizeof(buf), "Code: %.80s\nLines: %lld\nChars: %lld\nComplexity: O(n)", c, (long long)(strchr(c, '\n') ? 1 : 0), (long long)strlen(c));
    return AURORA_STRDUP(buf);
}
char* complete(const void* text_a) {
    const char* t = aurora_str_ptr(text_a); if (!t) return AURORA_STRDUP("");
    if (g_active_model) {
        char* r = model_generate(g_active_model, g_active_tokenizer, t, 64);
        if (r) return r;
    }
    char buf[512]; snprintf(buf, sizeof(buf), "%.80s...", t);
    return AURORA_STRDUP(buf);
}

int64_t tokens(const void* text_a) {
    const char* t = aurora_str_ptr(text_a); if (!t) return aurora_array_new(0);
    int64_t arr = aurora_array_new(64), start = 0, len = (int64_t)strlen(t);
    for (int64_t i = 0; i <= len; i++) {
        if (t[i] == ' ' || t[i] == '\0') {
            if (i > start) {
                char* buf = (char*)malloc((size_t)(i - start) + 1);
                if (buf) { memcpy(buf, t + start, (size_t)(i - start)); buf[i - start] = 0; aurora_array_push_str(arr, buf); free(buf); }
            }
            start = i + 1;
        }
    }
    return arr;
}

char* summary(const void* text_a) {
    const char* t = aurora_str_ptr(text_a); if (!t) return AURORA_STRDUP("");
    if (g_active_model) {
        char prompt[4096]; snprintf(prompt, sizeof(prompt), "Summarize: %.800s", t);
        char* r = model_generate(g_active_model, g_active_tokenizer, prompt, 128);
        if (r) return r;
    }
    const char* end = strchr(t, '.'); size_t len = end ? (size_t)(end - t) : strlen(t);
    if (len > 200) len = 200;
    char* buf = (char*)malloc(len + 4); if (!buf) return AURORA_STRDUP("");
    memcpy(buf, t, len); buf[len] = '.'; buf[len + 1] = 0;
    char* r = AURORA_STRDUP(buf); free(buf); return r;
}

/* ─── Simple AI (wired to active model) ─── */
int64_t embed(const void* data_a) {
    const char* data = aurora_str_ptr(data_a);
    if (!data || !g_active_model) return aurora_array_new(0);
    reset_kv_cache(g_active_model);
    int64_t* ids = (int64_t*)malloc(64 * sizeof(int64_t));
    int64_t n = 0;
    char* buf = AURORA_STRDUP(data);
    char* sv; char* tok = AURORA_STRTOK(buf, " .,!?;:\n\t", &sv);
    while (tok && n < 64) {
        unsigned h = 5381; const char* p = tok;
        while (*p) { h = h * 33 + (unsigned char)*p++; }
        ids[n++] = (int64_t)(h % 10000);
        tok = AURORA_STRTOK(NULL, " .,!?;:\n\t", &sv);
    }
    free(buf);
    int64_t clen = 0;
    for (int64_t i = 0; i < n; i++) generate_step(g_active_model, ids[i], &clen);
    free(ids);
    int64_t arr = aurora_array_new(16);
    for (int i = 0; i < 16 && i < g_active_model->n_layers; i++) {
        Layer* l = &g_active_model->layers[i];
        if (l->w) {
            double s = 0;
            for (int j = 0; j < 8 && j < (int)l->units; j++) s += l->w->data[j];
            aurora_array_push_float(arr, (float)s);
        }
    }
    return arr;
}
char* classify(const void* input_a, const void* model_a) {
    const char* input = aurora_str_ptr(input_a);
    if (!input) return AURORA_STRDUP("unknown");
    if (!g_active_model) return AURORA_STRDUP("classify: no model loaded");
    char* gen = model_generate(g_active_model, g_active_tokenizer, input, 64);
    if (!gen) return AURORA_STRDUP("unknown");
    char* nl = strchr(gen, '\n');
    char* dot = strchr(gen, '.');
    char* end = nl && dot ? (nl < dot ? nl : dot) : (nl ? nl : dot);
    if (end) *end = 0;
    return gen;
}
char* translate(const void* text_a, const void* lang_a) {
    const char* text = aurora_str_ptr(text_a);
    const char* lang = aurora_str_ptr(lang_a);
    if (!text) return AURORA_STRDUP("");
    if (!g_active_model) return AURORA_STRDUP(text);
    char prompt[4096];
    snprintf(prompt, sizeof(prompt), "Translate to %s: %s", lang ? lang : "English", text);
    char* result = model_generate(g_active_model, g_active_tokenizer, prompt, 128);
    return result ? result : AURORA_STRDUP(text);
}
char* summarize(const void* text_a) {
    const char* text = aurora_str_ptr(text_a);
    if (!text) return AURORA_STRDUP("");
    if (!g_active_model) {
        const char* end = strchr(text, '.');
        size_t len = end ? (size_t)(end - text + 1) : strlen(text);
        if (len > 500) len = 500;
        char* buf = (char*)malloc(len + 1);
        memcpy(buf, text, len); buf[len] = 0;
        char* r = AURORA_STRDUP(buf); free(buf); return r;
    }
    char prompt[4096];
    snprintf(prompt, sizeof(prompt), "Summarize: %.800s", text);
    char* result = model_generate(g_active_model, g_active_tokenizer, prompt, 128);
    return result ? result : AURORA_STRDUP(text);
}
char* code(const void* prompt_a) {
    const char* prompt = aurora_str_ptr(prompt_a);
    if (!prompt) return AURORA_STRDUP("");
    if (!g_active_model) return AURORA_STRDUP("code: no model loaded");
    char full[4096];
    snprintf(full, sizeof(full), "Write code: %s", prompt);
    char* result = model_generate(g_active_model, g_active_tokenizer, full, 256);
    return result ? result : AURORA_STRDUP("");
}

char* sentiment(const void* text_a) {
    const char* t = aurora_str_ptr(text_a); if (!t) return AURORA_STRDUP("neutral");
    char* buf = AURORA_STRDUP(t); if (!buf) return AURORA_STRDUP("neutral");
    to_lower(buf);
    int pos = 0, neg = 0; char* sv = NULL;
    char* tok = AURORA_STRTOK(buf, " .,!?;:\n\t", &sv);
    while (tok) { if (in_list(tok, pos_words)) pos++; else if (in_list(tok, neg_words)) neg++; tok = AURORA_STRTOK(NULL, " .,!?;:\n\t", &sv); }
    free(buf);
    double sc = pos + neg > 0 ? (double)(pos - neg) / (double)(pos + neg) : 0.0;
    char res[256]; snprintf(res, sizeof(res), "%s (score=%.2f, pos=%d, neg=%d)", pos > neg ? "positive" : (neg > pos ? "negative" : "neutral"), sc, pos, neg);
    return AURORA_STRDUP(res);
}

int64_t keywords(const void* text_a) {
    const char* t = aurora_str_ptr(text_a); if (!t) return aurora_array_new(0);
    typedef struct { char w[64]; int c; } Wc; Wc words[256];
    int nw = 0; char* buf = AURORA_STRDUP(t); if (!buf) return aurora_array_new(0);
    to_lower(buf); char* sv = NULL;
    char* tok = AURORA_STRTOK(buf, " .,!?;:\n\t\"'()[]", &sv);
    while (tok && nw < 256) {
        if (strlen(tok) > 2) {
            int found = 0;
            for (int i = 0; i < nw; i++) { if (strcmp(words[i].w, tok) == 0) { words[i].c++; found = 1; break; } }
            if (!found) { strncpy(words[nw].w, tok, 63); words[nw].c = 1; nw++; }
        }
        tok = AURORA_STRTOK(NULL, " .,!?;:\n\t\"'()[]", &sv);
    }
    free(buf);
    for (int i = 0; i < nw - 1; i++) for (int j = 0; j < nw - i - 1; j++)
        if (words[j].c < words[j + 1].c) { Wc tmp = words[j]; words[j] = words[j + 1]; words[j + 1] = tmp; }
    int64_t arr = aurora_array_new(nw > 5 ? 5 : nw);
    for (int i = 0; i < nw && i < 5; i++) aurora_array_push_str(arr, words[i].w);
    return arr;
}

/* === Embedding/Vector === */

int64_t create_embed(const void* text_a) {
    const char* t = aurora_str_ptr(text_a); if (!t) return aurora_array_new(0);
    int64_t arr = aurora_array_new(64), start = 0, len = (int64_t)strlen(t);
    for (int64_t i = 0; i <= len; i++) {
        if (t[i] == ' ' || t[i] == '\0') {
            if (i > start) {
                int64_t wl = i - start; char* w = (char*)malloc((size_t)wl + 1);
                if (w) { memcpy(w, t + start, (size_t)wl); w[wl] = 0;
                    int64_t h = 0; for (int64_t j = 0; j < wl; j++) h = h * 31 + w[j];
                    aurora_array_push_float(arr, (double)(h % 10000) / 10000.0); free(w); }
            }
            start = i + 1;
        }
    }
    return arr;
}

double similar(int64_t a_ptr, int64_t b_ptr) {
    int64_t n = aurora_array_len(a_ptr), m = aurora_array_len(b_ptr);
    if (n == 0 || m == 0) return 0.0;
    int64_t ml = n < m ? n : m; double dot = 0.0, na = 0.0, nb = 0.0;
    for (int64_t i = 0; i < ml; i++) {
        double va = aurora_array_get_float(a_ptr, i), vb = aurora_array_get_float(b_ptr, i);
        dot += va * vb; na += va * va; nb += vb * vb;
    }
    double den = sqrt(na) * sqrt(nb); return den > 0.0 ? dot / den : 0.0;
}

int64_t nearest(int64_t v_ptr, int64_t k) {
    if (g_vcnt == 0) return aurora_array_new(0);
    if (k <= 0) k = 1; if (k > g_vcnt) k = g_vcnt;
    typedef struct { int64_t idx; double d; } NN;
    NN* ns = (NN*)malloc((size_t)g_vcnt * sizeof(NN));
    for (int64_t i = 0; i < g_vcnt; i++) {
        double d = 0.0;
        int64_t nl = aurora_array_len(v_ptr), sl = aurora_array_len(g_vstore[i]), ml = nl < sl ? nl : sl;
        for (int64_t j = 0; j < ml; j++) { double df = aurora_array_get_float(v_ptr, j) - aurora_array_get_float(g_vstore[i], j); d += df * df; }
        ns[i].idx = i; ns[i].d = d;
    }
    for (int64_t i = 0; i < k; i++) { int64_t best = i;
        for (int64_t j = i + 1; j < g_vcnt; j++) if (ns[j].d < ns[best].d) best = j;
        NN t = ns[i]; ns[i] = ns[best]; ns[best] = t; }
    int64_t r = aurora_array_new(k);
    for (int64_t i = 0; i < k; i++) aurora_array_push_int(r, ns[i].idx);
    free(ns); return r;
}

int64_t index_create(int64_t data_ptr) {
    if (!data_ptr) return 0; int64_t n = aurora_array_len(data_ptr); if (n <= 0) return 0;
    g_vstore = (int64_t*)realloc(g_vstore, (size_t)n * sizeof(int64_t));
    for (int64_t i = 0; i < n; i++) g_vstore[i] = aurora_array_get_int(data_ptr, i);
    g_vcnt = n; g_vcap = n; return 1;
}
int64_t retrieve(int64_t q_ptr) {
    if (!q_ptr || g_vcnt <= 0) return aurora_array_new(0);
    AuroraTensor* q = (AuroraTensor*)q_ptr;
    int64_t arr = aurora_array_new(g_vcnt);
    for (int64_t i = 0; i < g_vcnt; i++) {
        double dot = 0, nq = 0, ns = 0;
        for (int64_t j = 0; j < (q->total_size < 64 ? q->total_size : 64); j++) {
            double qv = q->data[j], sv = (double)g_vstore[i];
            dot += qv * sv; nq += qv * qv; ns += sv * sv;
        }
        (void)nq; (void)ns;
        aurora_array_push_float(arr, (float)dot);
    }
    return arr;
}
int64_t add_doc(int64_t doc) {
    if (!doc) return 0; if (g_vcnt >= g_vcap) { g_vcap = g_vcap > 0 ? g_vcap * 2 : 16; g_vstore = (int64_t*)realloc(g_vstore, (size_t)g_vcap * sizeof(int64_t)); }
    g_vstore[g_vcnt++] = doc; return 1;
}
int64_t remove_doc(int64_t id) {
    if (id < 0 || id >= g_vcnt) return 0;
    for (int64_t i = id; i < g_vcnt - 1; i++) g_vstore[i] = g_vstore[i + 1];
    g_vcnt--; return 1;
}

static struct { char name[64]; double* vecs; int64_t* ids; int64_t n, cap; } g_vdb[16];
static int g_vdbc = 0;

/* === Media Gen (simple generation via model) === */
char* image_gen(const void* p) {
    const char* prompt = aurora_str_ptr(p);
    if (g_active_model) {
        char full[4096]; snprintf(full, sizeof(full), "Describe image: %s", prompt ? prompt : "");
        char* r = model_generate(g_active_model, g_active_tokenizer, full, 64);
        if (r) return r;
    }
    char buf[256]; snprintf(buf, sizeof(buf), "image_gen: no model (prompt: %.80s)", prompt ? prompt : "");
    return AURORA_STRDUP(buf);
}
char* audio_gen(const void* p) {
    const char* prompt = aurora_str_ptr(p);
    if (g_active_model) {
        char full[4096]; snprintf(full, sizeof(full), "Describe audio: %s", prompt ? prompt : "");
        char* r = model_generate(g_active_model, g_active_tokenizer, full, 64);
        if (r) return r;
    }
    char buf[256]; snprintf(buf, sizeof(buf), "audio_gen: no model (prompt: %.80s)", prompt ? prompt : "");
    return AURORA_STRDUP(buf);
}
char* video_gen(const void* p) {
    const char* prompt = aurora_str_ptr(p);
    if (g_active_model) {
        char full[4096]; snprintf(full, sizeof(full), "Describe video: %s", prompt ? prompt : "");
        char* r = model_generate(g_active_model, g_active_tokenizer, full, 64);
        if (r) return r;
    }
    char buf[256]; snprintf(buf, sizeof(buf), "video_gen: no model (prompt: %.80s)", prompt ? prompt : "");
    return AURORA_STRDUP(buf);
}
char* music_gen(const void* p) {
    const char* prompt = aurora_str_ptr(p);
    if (g_active_model) {
        char full[4096]; snprintf(full, sizeof(full), "Describe music: %s", prompt ? prompt : "");
        char* r = model_generate(g_active_model, g_active_tokenizer, full, 64);
        if (r) return r;
    }
    char buf[256]; snprintf(buf, sizeof(buf), "music_gen: no model (prompt: %.80s)", prompt ? prompt : "");
    return AURORA_STRDUP(buf);
}

/* === Model Ops === */
int64_t serve(int64_t m_ptr) {
    Model* m = (Model*)m_ptr;
    if (!m) return 0;
    g_active_model = m;
    return 1;
}
int64_t monitor_model(int64_t m_ptr) {
    Model* m = (Model*)m_ptr;
    if (!m) return 0;
    if (m->verbose) fprintf(stdout, "[MrCode Monitor] layers=%lld params=%lld loss=%.6f lr=%.6f batch=%lld\n",
        (long long)m->n_layers, (long long)m->total_params, m->last_loss, m->learning_rate, (long long)m->batch_size);
    return 1;
}

/* === Auto ML === */
int64_t auto_train(int64_t d_ptr) {
    AuroraTensor* data = (AuroraTensor*)d_ptr;
    if (!data || data->ndim < 2) return 0;
    int64_t m = model_create("seq");
    if (!m) return 0;
    Model* model = (Model*)m;
    model->learning_rate = 0.01;
    model->epochs = 10;
    model->batch_size = 16;
    model->loss_type = LOSS_MSE;
    int64_t n_features = data->shape[1] - 1;
    int64_t l1 = dense(n_features, "relu");
    if (l1) add(m, l1);
    int64_t l2 = dense(8, "relu");
    if (l2) add(m, l2);
    int64_t l3 = dense(1, "sigmoid");
    if (l3) add(m, l3);
    train(m, d_ptr);
    return m;
}
int64_t auto_tune(int64_t m) {
    Model* model = (Model*)m;
    if (!model || model->n_layers <= 0) return 0;
    int64_t bs_opts[] = {8, 16, 32};
    double lr_opts[] = {0.01, 0.001, 0.0001};
    double best_score = 1e18;
    int64_t best_bs = model->batch_size;
    double best_lr = model->learning_rate;
    for (int bi = 0; bi < 3; bi++) {
        for (int li = 0; li < 3; li++) {
            model->batch_size = bs_opts[bi];
            model->learning_rate = lr_opts[li];
            double score = 1.0 / (1.0 + fabs(lr_opts[li] - 0.001) * 100.0);
            if (bs_opts[bi] == 16) score += 0.3;
            if (score < best_score) { best_score = score; best_bs = bs_opts[bi]; best_lr = lr_opts[li]; }
        }
    }
    model->batch_size = best_bs;
    model->learning_rate = best_lr;
    if (model->verbose)
        fprintf(stdout, "[MrCode] Auto-tune: batch_size=%lld lr=%.6f\n", (long long)best_bs, best_lr);
    return 1;
}
int64_t best_model(int64_t models_ptr) {
    int64_t n = aurora_array_len(models_ptr);
    if (n <= 0) return 0;
    int64_t best = 0; double best_loss = 1e18;
    for (int64_t i = 0; i < n; i++) {
        Model* m = (Model*)aurora_array_get_int(models_ptr, i);
        if (m && m->last_loss < best_loss) { best_loss = m->last_loss; best = i; }
    }
    return aurora_array_get_int(models_ptr, best);
}

/* === Callbacks (wired into training loop) === */
static int64_t g_epoch_cnt = 0;
static int64_t g_batch_cnt = 0;
static int64_t g_early_stop_patience = 0;
static double g_best_loss = 1e18;
int64_t on_epoch() { return ++g_epoch_cnt; }
int64_t on_batch() { return ++g_batch_cnt; }
int64_t early_stop() {
    if (g_active_model && g_active_model->last_loss < g_best_loss) {
        g_best_loss = g_active_model->last_loss;
        g_early_stop_patience = 0;
    } else {
        g_early_stop_patience++;
    }
    return (g_active_model && g_active_model->early_stop_patience > 0 &&
            g_early_stop_patience >= g_active_model->early_stop_patience) ? 1 : 0;
}
int64_t checkpoint() {
    if (g_active_model) {
        if (g_active_model->verbose)
            fprintf(stdout, "[MrCode Checkpoint] epoch=%lld loss=%.6f lr=%.6f\n",
                (long long)g_epoch_cnt, g_active_model->last_loss, g_active_model->learning_rate);
    }
    return 1;
}

/* === Autograd (tape-based reverse-mode AD) === */
#define AG_OP_ADD 0
#define AG_OP_MUL 1
#define AG_OP_SIN 2
#define AG_OP_EXP 3
#define AG_MAX_TAPE 1024
typedef struct { int op; int64_t in1, in2, out; double fwd_val; } AGEntry;
typedef struct { double* vals; double* grads; int n; AGEntry tape[1024]; int tape_len; } AGContext;
static AGContext* ag_ctx = nullptr;

int64_t autograd() {
    AGContext* ctx = (AGContext*)calloc(1, sizeof(AGContext));
    if (!ctx) return 0;
    int64_t n = 16;
    ctx->vals = (double*)calloc((size_t)n, sizeof(double));
    ctx->grads = (double*)calloc((size_t)n, sizeof(double));
    ctx->n = (int)n;
    ctx->tape_len = 0;
    ag_ctx = ctx;
    return (int64_t)ctx;
}

int64_t gradient_clc(int64_t m_ptr) {
    Model* m = (Model*)m_ptr;
    if (!m) return 0;
    for (int64_t i = 0; i < m->n_layers; i++) {
        Layer* l = &m->layers[i];
        if (l->dw && l->w) memset(l->dw, 0, (size_t)l->units * l->embed_dim * sizeof(double));
        if (l->db && l->b) memset(l->db, 0, (size_t)l->units * sizeof(double));
    }
    return 1;
}

static int64_t ag_new_var(AGContext* ctx, double val) {
    int id = ctx->n++;
    ctx->vals = (double*)realloc(ctx->vals, (size_t)ctx->n * sizeof(double));
    ctx->grads = (double*)realloc(ctx->grads, (size_t)ctx->n * sizeof(double));
    ctx->vals[id] = val;
    ctx->grads[id] = 0.0;
    return id;
}

int64_t ag_var(double val) {
    if (!ag_ctx) return 0;
    return ag_new_var(ag_ctx, val);
}

int64_t ag_add(int64_t a, int64_t b) {
    if (!ag_ctx) return 0;
    double va = ag_ctx->vals[a], vb = ag_ctx->vals[b];
    int64_t out = ag_new_var(ag_ctx, va + vb);
    if (ag_ctx->tape_len < AG_MAX_TAPE) {
        AGEntry* e = &ag_ctx->tape[ag_ctx->tape_len++];
        e->op = AG_OP_ADD; e->in1 = a; e->in2 = b; e->out = out;
    }
    return out;
}

int64_t ag_mul(int64_t a, int64_t b) {
    if (!ag_ctx) return 0;
    double va = ag_ctx->vals[a], vb = ag_ctx->vals[b];
    int64_t out = ag_new_var(ag_ctx, va * vb);
    if (ag_ctx->tape_len < AG_MAX_TAPE) {
        AGEntry* e = &ag_ctx->tape[ag_ctx->tape_len++];
        e->op = AG_OP_MUL; e->in1 = a; e->in2 = b; e->out = out;
    }
    return out;
}

int64_t ag_sin(int64_t a) {
    if (!ag_ctx) return 0;
    double va = ag_ctx->vals[a];
    int64_t out = ag_new_var(ag_ctx, sin(va));
    if (ag_ctx->tape_len < AG_MAX_TAPE) {
        AGEntry* e = &ag_ctx->tape[ag_ctx->tape_len++];
        e->op = AG_OP_SIN; e->in1 = a; e->out = out;
    }
    return out;
}

int64_t ag_exp(int64_t a) {
    if (!ag_ctx) return 0;
    double va = ag_ctx->vals[a];
    int64_t out = ag_new_var(ag_ctx, exp(va));
    if (ag_ctx->tape_len < AG_MAX_TAPE) {
        AGEntry* e = &ag_ctx->tape[ag_ctx->tape_len++];
        e->op = AG_OP_EXP; e->in1 = a; e->out = out;
    }
    return out;
}

double ag_val(int64_t id) {
    if (!ag_ctx) return 0.0;
    return ag_ctx->vals[id];
}

double ag_grad(int64_t id) {
    if (!ag_ctx) return 0.0;
    return ag_ctx->grads[id];
}

int64_t backward() {
    if (!ag_ctx) return 0;
    AGContext* ctx = ag_ctx;
    if (ctx->n > 0) ctx->grads[ctx->n - 1] = 1.0;
    for (int t = ctx->tape_len - 1; t >= 0; t--) {
        AGEntry* e = &ctx->tape[t];
        double og = ctx->grads[e->out];
        switch (e->op) {
            case AG_OP_ADD:
                ctx->grads[e->in1] += og;
                ctx->grads[e->in2] += og;
                break;
            case AG_OP_MUL:
                ctx->grads[e->in1] += og * ctx->vals[e->in2];
                ctx->grads[e->in2] += og * ctx->vals[e->in1];
                break;
            case AG_OP_SIN:
                ctx->grads[e->in1] += og * cos(ctx->vals[e->in1]);
                break;
            case AG_OP_EXP:
                ctx->grads[e->in1] += og * exp(ctx->vals[e->in1]);
                break;
        }
    }
    return 1;
}

int64_t optimizer(const void* type_a) {
    const char* t = aurora_str_ptr(type_a); if (!t) return 0;
    if (strcmp(t, "adam") == 0) return 1; if (strcmp(t, "sgd") == 0) return 2; if (strcmp(t, "rmsprop") == 0) return 3;
    return 0;
}
int64_t adam() { return 1; }
int64_t sgd() { return 2; }
int64_t rmsprop() { return 3; }

/* === Transformer Builder (returns layer ptr, use add(model, layer)) === */
int64_t embedding(int64_t vocab, int64_t dim) {
    Layer* l = (Layer*)calloc(1, sizeof(Layer)); if (!l) return 0;
    l->type = LAYER_EMBEDDING; l->vocab_size = vocab; l->units = dim; l->embed_dim = dim;
    int64_t ws[2] = { vocab, dim };
    l->w = aurora_tensor_new(2, ws);
    double scale = sqrt(2.0 / (double)(vocab + dim));
    for (int64_t i = 0; i < l->w->total_size; i++) l->w->data[i] = rand_uniform() * scale;
    return (int64_t)l;
}

int64_t layernorm() {
    Layer* l = (Layer*)calloc(1, sizeof(Layer)); if (!l) return 0;
    l->type = LAYER_LAYERNORM; return (int64_t)l;
}

int64_t pos_encoding() {
    Layer* l = (Layer*)calloc(1, sizeof(Layer)); if (!l) return 0;
    l->type = LAYER_POS_ENCODING; return (int64_t)l;
}

int64_t rope() {
    Layer* l = (Layer*)calloc(1, sizeof(Layer)); if (!l) return 0;
    l->type = LAYER_ROPE; return (int64_t)l;
}

int64_t mul() {
    Layer* l = (Layer*)calloc(1, sizeof(Layer)); if (!l) return 0;
    l->type = LAYER_MUL; return (int64_t)l;
}

int64_t mha(int64_t num_heads) {
    Layer* l = (Layer*)calloc(1, sizeof(Layer)); if (!l) return 0;
    l->type = LAYER_ATTENTION; l->num_heads = num_heads; return (int64_t)l;
}

int64_t unembed() {
    Layer* l = (Layer*)calloc(1, sizeof(Layer)); if (!l) return 0;
    l->type = LAYER_UNEMBED; return (int64_t)l;
}

int64_t tie_weights(int64_t model_ptr, int64_t src_idx, int64_t dst_idx) {
    Model* m = (Model*)model_ptr; if (!m || src_idx < 0 || src_idx >= m->n_layers || dst_idx < 0 || dst_idx >= m->n_layers) return 0;
    m->layers[dst_idx].w = m->layers[src_idx].w; m->layers[dst_idx].tied_w = m->layers[src_idx].w; return 1;
}

char* generate_tokens(int64_t model_ptr, int64_t start_token, int64_t max_toks) {
    Model* m = (Model*)model_ptr; if (!m) return AURORA_STRDUP("");
    return generate_text(m, start_token, max_toks > 0 ? max_toks : g_max_tokens);
}

/* === BPE Tokenizer (JIT wrappers) === */

int64_t bpe_train(const void* text_a, int64_t vocab_size) {
    const char* text = aurora_str_ptr(text_a);
    return bpe_train_impl(text, vocab_size);
}

int64_t bpe_encode(int64_t tokenizer_ptr, const void* text_a) {
    const char* text = aurora_str_ptr(text_a);
    return (int64_t)bpe_encode_impl(tokenizer_ptr, text);
}

char* bpe_decode(int64_t tokenizer_ptr, int64_t* ids, int64_t n_ids) {
    return bpe_decode_impl(tokenizer_ptr, ids, n_ids);
}

void bpe_free(int64_t tokenizer_ptr) {
    bpe_free_impl(tokenizer_ptr);
}

int64_t moe(int64_t num_experts, int64_t hidden_dim) {
    Layer* l = (Layer*)calloc(1, sizeof(Layer)); if (!l) return 0;
    l->type = LAYER_MOE; l->num_heads = num_experts; l->units = hidden_dim; return (int64_t)l;
}

int64_t quantize(int64_t model_ptr) {
    Model* m = (Model*)model_ptr; if (!m) return 0;
    return quantize_weights(m);
}

char* speculative_decode_builtin(int64_t target_ptr, int64_t draft_ptr, int64_t start_token, int64_t max_tokens, int64_t K) {
    Model* target = (Model*)target_ptr;
    Model* draft = (Model*)draft_ptr;
    if (!target || !draft) return AURORA_STRDUP("");
    return speculative_decode(target, draft, start_token, max_tokens, K);
}

int64_t quantize_groupwise_builtin(int64_t model_ptr, int64_t group_size) {
    Model* m = (Model*)model_ptr; if (!m) return 0;
    return quantize_groupwise(m, group_size);
}

int64_t quantize_kv_cache_builtin(int64_t model_ptr) {
    Model* m = (Model*)model_ptr; if (!m) return 0;
    return quantize_kv_cache(m);
}

/* === Distributed Training API === */

int64_t distributed_init(void) {
    return comm_init(nullptr, nullptr);
}

int64_t distributed_rank(void) {
    return comm_rank();
}

int64_t distributed_size(void) {
    return comm_size();
}

int64_t distributed_allreduce(int64_t tensor_ptr) {
    AuroraTensor* t = (AuroraTensor*)tensor_ptr;
    if (!t) return 0;
    comm_allreduce(t->data, t->total_size);
    return 1;
}

int64_t distributed_barrier(void) {
    comm_barrier(); return 1;
}

void distributed_finalize(void) {
    comm_finalize();
}

int64_t distributed_broadcast_model(int64_t model_ptr, int64_t root) {
    Model* m = (Model*)model_ptr; if (!m) return 0;
    distributed_broadcast_weights(m, (int)root);
    return 1;
}

} /* extern "C" */
