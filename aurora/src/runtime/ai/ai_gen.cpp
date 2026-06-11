#include "runtime/ai/ai_common.h"
#include "runtime/ai/tokenizer.hpp"
#include <mutex>

static Agent* g_agents[16];
static int g_nagents = 0;
static ChatS* g_chats2[16];
static int g_nchats = 0;
static std::mutex g_ai_mutex;
double g_temperature = 1.0;
double g_top_p = 1.0;
int64_t g_max_tokens = 256;
Model* g_active_model = nullptr;
int64_t g_active_tokenizer = 0;

extern "C" {

/* ── Sample from logits with temperature & top-p ── */
static int64_t sample_from_logits(double* logits, int64_t vocab) {
    if (g_temperature > 0.0 && fabs(g_temperature - 1.0) > 1e-6)
        for (int64_t i = 0; i < vocab; i++) logits[i] /= g_temperature;
    double maxv = logits[0];
    for (int64_t i = 1; i < vocab; i++) if (logits[i] > maxv) maxv = logits[i];
    double sum = 0.0;
    for (int64_t i = 0; i < vocab; i++) { logits[i] = exp(logits[i] - maxv); sum += logits[i]; }
    if (sum > 1e-15) for (int64_t i = 0; i < vocab; i++) logits[i] /= sum;
    if (g_top_p < 1.0) {
        double cum = 0.0;
        for (int64_t i = 0; i < vocab; i++) {
            cum += logits[i];
            if (cum > g_top_p) logits[i] = 0.0;
        }
        sum = 0.0;
        for (int64_t i = 0; i < vocab; i++) sum += logits[i];
        if (sum > 1e-15) for (int64_t i = 0; i < vocab; i++) logits[i] /= sum;
    }
    double r = (double)rand() / RAND_MAX;
    double cum2 = 0.0;
    for (int64_t i = 0; i < vocab; i++) { cum2 += logits[i]; if (r <= cum2) return i; }
    return 0;
}

int64_t generate_step(Model* m, int64_t token, int64_t* cache_len_ptr) {
    if (!m) return 0;
    int64_t is[2] = { 1, 1 };
    AuroraTensor* in = aurora_tensor_new(2, is);
    in->data[0] = (double)token;
    AuroraTensor* out = model_forward(m, in, 0);
    if (!out) { aurora_tensor_free(in); return 0; }
    int64_t vocab = out->total_size;
    int64_t next = sample_from_logits(out->data, vocab);
    aurora_tensor_free(in); aurora_tensor_free(out);
    if (cache_len_ptr) (*cache_len_ptr)++;
    return next;
}

void reset_kv_cache(Model* m) {
    for (int i = 0; i < m->n_layers; i++) {
        Layer* l = &m->layers[i];
        if (l->type == LAYER_ATTENTION) {
            aurora_tensor_free(l->kv_cache_k); l->kv_cache_k = nullptr;
            aurora_tensor_free(l->kv_cache_v); l->kv_cache_v = nullptr;
            l->cache_len = 0;
        }
    }
}

/* ── Generate text from a sequence of input tokens ──
   input_ids: flat int64 array of token IDs, n_input = length
   model generates up to max_new_tokens additional tokens
   returns flat tensor of all output token IDs */
static int64_t* generate_ids(Model* m, int64_t* input_ids, int64_t n_input, int64_t max_new_toks, int64_t* out_n) {
    reset_kv_cache(m);
    int64_t cap = n_input + max_new_toks + 64;
    int64_t* ids = (int64_t*)malloc((size_t)cap * sizeof(int64_t));
    int64_t n = n_input;
    for (int64_t i = 0; i < n_input; i++) ids[i] = input_ids[i];
    int64_t clen = 0;
    /* Prefill — run each input token through the model to build KV cache */
    for (int64_t i = 0; i < n_input; i++) {
        int64_t next = generate_step(m, ids[i], &clen);
        if (i == n_input - 1) ids[n++] = next;
    }
    /* Auto-regressive generation */
    for (int64_t i = 0; i < max_new_toks && n < cap; i++) {
        if (ids[n - 1] == 0 || ids[n - 1] == 2) break;
        ids[n] = generate_step(m, ids[n - 1], &clen);
        n++;
    }
    *out_n = n;
    return ids;
}

char* generate_text(Model* m, int64_t start_token, int64_t max_toks) {
    int64_t input = start_token;
    int64_t out_n = 0;
    int64_t* ids = generate_ids(m, &input, 1, max_toks, &out_n);
    char buf[4096]; int64_t pos = 0;
    for (int64_t i = 0; i < out_n && pos < 4000; i++)
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "%lld ", (long long)ids[i]);
    free(ids);
    return AURORA_STRDUP(buf);
}

/* ── model_generate: tokenize → generate → decode ──
   Returns decoded text string. Uses g_active_tokenizer if tokenizer_ptr=0. */
char* model_generate(Model* m, int64_t tokenizer_ptr, const char* text, int64_t max_toks) {
    if (!m || !text) return AURORA_STRDUP("");
    int64_t tokp = tokenizer_ptr ? tokenizer_ptr : g_active_tokenizer;
    if (!tokp) {
        /* No tokenizer — just use a dummy single-token input */
        int64_t out_n = 0;
        int64_t dummy = 1;
        int64_t* ids = generate_ids(m, &dummy, 1, max_toks, &out_n);
        char buf[4096]; int64_t pos = 0;
        for (int64_t i = 0; i < out_n && pos < 4000; i++)
            pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "%lld ", (long long)ids[i]);
        free(ids);
        return AURORA_STRDUP(buf);
    }
    AuroraTensor* encoded = bpe_encode_impl(tokp, text);
    if (!encoded || encoded->total_size <= 0) {
        if (encoded) aurora_tensor_free(encoded);
        return AURORA_STRDUP("");
    }
    int64_t n_input = encoded->total_size;
    int64_t* input_ids = (int64_t*)malloc((size_t)n_input * sizeof(int64_t));
    for (int64_t i = 0; i < n_input; i++) input_ids[i] = (int64_t)encoded->data[i];
    aurora_tensor_free(encoded);
    int64_t out_n = 0;
    int64_t* out_ids = generate_ids(m, input_ids, n_input, max_toks, &out_n);
    free(input_ids);
    char* decoded = bpe_decode_impl(tokp, out_ids, out_n);
    free(out_ids);
    /* Truncate at first EOS */
    if (decoded) {
        char* eos = strstr(decoded, "</s>");
        if (eos) *eos = '\0';
    }
    return decoded ? decoded : AURORA_STRDUP("");
}

/* ── Chat system ── */

static ChatS* chat_find(int64_t session) {
    for (int i = 0; i < g_nchats; i++) if ((int64_t)g_chats2[i] == session) return g_chats2[i];
    return nullptr;
}

int64_t chat_new() {
    std::lock_guard<std::mutex> lock(g_ai_mutex);
    if (g_nchats >= 16) return 0;
    ChatS* cs = (ChatS*)calloc(1, sizeof(ChatS));
    if (!cs) return 0;
    g_chats2[g_nchats++] = cs;
    return (int64_t)cs;
}

int64_t chat_system(int64_t session, const char* prompt) {
    ChatS* cs = chat_find(session);
    if (!cs || !prompt) return 0;
    snprintf(cs->system_prompt, sizeof(cs->system_prompt), "%s", prompt);
    return 1;
}

char* chat_send(int64_t session, const char* prompt) {
    ChatS* cs = chat_find(session);
    if (!cs || !prompt) return AURORA_STRDUP("");
    /* Store user message */
    if (cs->n < CHAT_MAX) {
        Msg* m = &cs->hist[cs->n++];
        snprintf(m->role, sizeof(m->role), "user");
        snprintf(m->content, sizeof(m->content), "%s", prompt);
    }
    /* Build full prompt from history */
    char full_prompt[16384];
    int64_t pos = 0;
    if (cs->system_prompt[0]) {
        pos += snprintf(full_prompt + pos, sizeof(full_prompt) - (size_t)pos,
            "<|system|>\n%s\n<|end|>\n", cs->system_prompt);
    }
    for (int i = 0; i < cs->n; i++) {
        if (strcmp(cs->hist[i].role, "user") == 0) {
            pos += snprintf(full_prompt + pos, sizeof(full_prompt) - (size_t)pos,
                "<|user|>\n%s\n<|end|>\n", cs->hist[i].content);
        } else if (strcmp(cs->hist[i].role, "assistant") == 0) {
            pos += snprintf(full_prompt + pos, sizeof(full_prompt) - (size_t)pos,
                "<|assistant|>\n%s\n<|end|>\n", cs->hist[i].content);
        }
    }
    pos += snprintf(full_prompt + pos, sizeof(full_prompt) - (size_t)pos, "<|assistant|>\n");
    /* Generate response */
    Model* m = g_active_model;
    if (!m) {
        char* fallback = (char*)malloc(256);
        snprintf(fallback, 256, "No active model. Set g_active_model first.");
        if (cs->n < CHAT_MAX) {
            Msg* msg = &cs->hist[cs->n++];
            snprintf(msg->role, sizeof(msg->role), "assistant");
            snprintf(msg->content, sizeof(msg->content), "%s", fallback);
        }
        return fallback;
    }
    char* response = model_generate(m, g_active_tokenizer, full_prompt, g_max_tokens);
    if (!response) response = AURORA_STRDUP("");
    /* Store assistant response */
    if (cs->n < CHAT_MAX) {
        Msg* msg = &cs->hist[cs->n++];
        snprintf(msg->role, sizeof(msg->role), "assistant");
        snprintf(msg->content, sizeof(msg->content), "%s", response);
    }
    return response;
}

int64_t chat_reset(int64_t session) {
    ChatS* cs = chat_find(session);
    if (!cs) return 0;
    cs->n = 0;
    cs->system_prompt[0] = '\0';
    return 1;
}

static Agent* ag_find(int64_t p) { for (int i = 0; i < g_nagents; i++) if ((int64_t)g_agents[i] == p) return g_agents[i]; return nullptr; }

int64_t agent() {
    std::lock_guard<std::mutex> lock(g_ai_mutex);
    if (g_nagents >= 16) return 0;
    Agent* a = (Agent*)calloc(1, sizeof(Agent)); if (!a) return 0;
    a->id = g_nagents + 1; a->active = 1; g_agents[g_nagents++] = a; return (int64_t)a;
}
int64_t train_agent(int64_t p) { Agent* a = ag_find(p); if (!a) return 0; a->episodes++; return 1; }
int64_t reward(int64_t p, double v) { Agent* a = ag_find(p); if (!a) return 0; a->reward += (int64_t)v; return 1; }

int64_t ag_run(const void* task_a) {
    const char* task = aurora_str_ptr(task_a);
    if (!task) return 0;
    std::lock_guard<std::mutex> lock(g_ai_mutex);
    /* Find active agent */
    for (int i = 0; i < g_nagents; i++) {
        if (g_agents[i]->active) {
            g_agents[i]->episodes++;
            /* Use model to process task if available */
            if (g_active_model) {
                char* result = model_generate(g_active_model, g_active_tokenizer, task, 128);
                if (result) {
                    free(result);
                    return 1;
                }
            }
        }
    }
    return 1;
}

/* ─── Simple in-memory vector DB ─── */
typedef struct { char name[64]; double* vecs; int64_t* ids; int64_t n, cap; int64_t name_len; } VecDB;
static VecDB g_vec_dbs[16];
static int g_vec_dbc = 0;

int64_t vector_db(const void* name_a) {
    const char* name = aurora_str_ptr(name_a);
    if (!name) return 0;
    std::lock_guard<std::mutex> lock(g_ai_mutex);
    /* Find or create */
    for (int i = 0; i < g_vec_dbc; i++)
        if (strcmp(g_vec_dbs[i].name, name) == 0) return (int64_t)(&g_vec_dbs[i]);
    if (g_vec_dbc >= 16) return 0;
    VecDB* db = &g_vec_dbs[g_vec_dbc++];
    strncpy(db->name, name, 63); db->name[63] = 0;
    db->vecs = nullptr; db->ids = nullptr; db->n = 0; db->cap = 0;
    return (int64_t)db;
}
int64_t vector_insert(int64_t db_ptr, const void* vec_a) {
    VecDB* db = (VecDB*)db_ptr;
    const char* vec_str = aurora_str_ptr(vec_a);
    if (!db || !vec_str) return 0;
    if (db->n >= db->cap) {
        db->cap = db->cap > 0 ? db->cap * 2 : 16;
        void* tmp_v = realloc(db->vecs, (size_t)db->cap * 64 * sizeof(double));
        if (!tmp_v) return 0; db->vecs = (double*)tmp_v;
        void* tmp_i = realloc(db->ids, (size_t)db->cap * sizeof(int64_t));
        if (!tmp_i) return 0; db->ids = (int64_t*)tmp_i;
    }
    /* Parse space-separated floats from string */
    int64_t idx = db->n++;
    db->ids[idx] = idx + 1;
    char* buf = AURORA_STRDUP(vec_str); char* sv; char* tok = AURORA_STRTOK(buf, " ,[]", &sv);
    int64_t dim = 0;
    while (tok && dim < 64) { db->vecs[idx * 64 + dim++] = atof(tok); tok = AURORA_STRTOK(NULL, " ,[]", &sv); }
    while (dim < 64) db->vecs[idx * 64 + dim++] = 0;
    free(buf);
    return 1;
}
int64_t vector_del(int64_t db_ptr, const void* id_a) {
    VecDB* db = (VecDB*)db_ptr;
    const char* id_str = aurora_str_ptr(id_a);
    if (!db || !id_str) return 0;
    int64_t id = atoll(id_str);
    for (int64_t i = 0; i < db->n; i++) {
        if (db->ids[i] == id) {
            for (int64_t j = i; j < db->n - 1; j++) {
                db->ids[j] = db->ids[j + 1];
                memcpy(&db->vecs[j * 64], &db->vecs[(j + 1) * 64], 64 * sizeof(double));
            }
            db->n--;
            return 1;
        }
    }
    return 0;
}
char* vector_query(int64_t db_ptr, const void* query_a) {
    VecDB* db = (VecDB*)db_ptr;
    const char* vec_str = aurora_str_ptr(query_a);
    if (!db || db->n <= 0 || !vec_str) return AURORA_STRDUP("[]");
    /* Parse query vector */
    double qv[64] = {}; int64_t qd = 0;
    char* buf = AURORA_STRDUP(vec_str); char* sv; char* tok = AURORA_STRTOK(buf, " ,[]", &sv);
    while (tok && qd < 64) { qv[qd++] = atof(tok); tok = AURORA_STRTOK(NULL, " ,[]", &sv); }
    free(buf);
    /* Brute-force nearest neighbor */
    int64_t best = 0; double best_sim = -1e18;
    for (int64_t i = 0; i < db->n; i++) {
        double dot = 0, nq = 0, nd = 0;
        for (int64_t j = 0; j < qd; j++) {
            dot += qv[j] * db->vecs[i * 64 + j];
            nq += qv[j] * qv[j];
            nd += db->vecs[i * 64 + j] * db->vecs[i * 64 + j];
        }
        double sim = (nq > 0 && nd > 0) ? dot / (sqrt(nq) * sqrt(nd)) : 0;
        if (sim > best_sim) { best_sim = sim; best = i; }
    }
    char res[256]; snprintf(res, sizeof(res), "[{\"id\":%lld,\"score\":%.4f}]", (long long)db->ids[best], best_sim);
    return AURORA_STRDUP(res);
}
char* ag_memory() {
    char buf[512]; int64_t pos = snprintf(buf, sizeof(buf), "agents: %lld", (long long)g_nagents);
    for (int i = 0; i < g_nagents && i < 8; i++) pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, ", agent%lld: ep=%lld rew=%lld tools=%d", (long long)g_agents[i]->id, (long long)g_agents[i]->episodes, (long long)g_agents[i]->reward, g_agents[i]->nt);
    return AURORA_STRDUP(buf);
}
int64_t ag_tools() {
    std::lock_guard<std::mutex> lock(g_ai_mutex);
    int64_t arr = aurora_array_new(16);
    for (int i = 0; i < g_nagents; i++) for (int j = 0; j < g_agents[i]->nt; j++) if (g_agents[i]->tools[j].name) aurora_array_push_str(arr, g_agents[i]->tools[j].name);
    return arr;
}
int64_t ag_stop() { std::lock_guard<std::mutex> lock(g_ai_mutex); for (int i = 0; i < g_nagents; i++) g_agents[i]->active = 0; return 1; }

int64_t temperature(double v) { std::lock_guard<std::mutex> lock(g_ai_mutex); if (v > 0.0) g_temperature = v; return 1; }
int64_t top_p(double v) { std::lock_guard<std::mutex> lock(g_ai_mutex); if (v > 0.0 && v <= 1.0) g_top_p = v; return 1; }
int64_t max_tokens(int64_t v) { std::lock_guard<std::mutex> lock(g_ai_mutex); if (v > 0) g_max_tokens = v; return 1; }

int64_t ag_add_tool() {
    std::lock_guard<std::mutex> lock(g_ai_mutex);
    if (g_nagents == 0) return 0; Agent* a = g_agents[g_nagents - 1];
    if (a->nt >= MAX_TOOLS) return 0; ToolDef* t = &a->tools[a->nt++];
    t->name = (char*)malloc(32); if (t->name) snprintf(t->name, 32, "tool_%d", a->nt);
    t->desc = (char*)malloc(64); if (t->desc) snprintf(t->desc, 64, "tool %d desc", a->nt);
    return 1;
}
int64_t ag_remove_tool() {
    std::lock_guard<std::mutex> lock(g_ai_mutex);
    if (g_nagents == 0) return 0; Agent* a = g_agents[g_nagents - 1];
    if (a->nt <= 0) return 0; a->nt--; return 1;
}
char* ag_history() {
    std::lock_guard<std::mutex> lock(g_ai_mutex);
    char buf[1024]; int64_t pos = snprintf(buf, sizeof(buf), "agents=%lld", (long long)g_nagents);
    for (int i = 0; i < g_nagents && i < 5; i++) pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "\n  agent %lld: tools=%d active=%d ep=%lld rew=%lld", (long long)g_agents[i]->id, g_agents[i]->nt, g_agents[i]->active, (long long)g_agents[i]->episodes, (long long)g_agents[i]->reward);
    return AURORA_STRDUP(buf);
}
int64_t ag_reset() {
    std::lock_guard<std::mutex> lock(g_ai_mutex);
    for (int i = 0; i < g_nagents; i++) { g_agents[i]->reward = 0; g_agents[i]->episodes = 0; g_agents[i]->nt = 0; g_agents[i]->active = 1; }
    return 1;
}

} /* extern "C" */
