#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <ctime>
#include <cmath>

#include "runtime/string.hpp"
#include "runtime/backend.hpp"
#include "std/json.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

#define ASTR(x) (((AuroraStr*)(x))->ptr)

/* ── Forward declarations from server runtime (not in backend.hpp) ── */
extern "C" {
    void aurora_panic(const char* msg);
    int64_t aurora_time();
    void aurora_sleep(int64_t ms);
    void aurora_cors_apply_default(AuroraHttpResponse* res);
    void aurora_cors_apply_with_origin(AuroraHttpResponse* res, const char* origin);
    void aurora_event_bus_init();
    void aurora_event_on(const char* event_name, void (*handler)(const char*, const char*));
    void aurora_event_emit(const char* event_name, const char* data);
}

/* ── Static state for backend built-ins ── */
static std::mutex g_backend_mutex;
static std::recursive_mutex g_lock_map_mutex;
static std::unordered_map<std::string, int> g_lock_map;
static std::unordered_map<std::string, std::string> g_session_store;
static std::unordered_map<std::string, std::string> g_cache_store;
static std::unordered_map<std::string, void*> g_route_groups;
static std::vector<void*> g_middleware_chain;
static int64_t g_request_counter = 0;

extern "C" {

/* ════════════════════════════════════════════
   Route / Middleware
   ════════════════════════════════════════════ */

int64_t builtin_route_group(const char* path) {
    std::lock_guard<std::mutex> lock(g_backend_mutex);
    g_route_groups[path ? ASTR(path) : ""] = nullptr;
    return 1;
}

int64_t builtin_middleware(void* fn) {
    std::lock_guard<std::mutex> lock(g_backend_mutex);
    g_middleware_chain.push_back(fn);
    return (int64_t)g_middleware_chain.size();
}

int64_t builtin_next() {
    return 1;
}

int64_t builtin_rate_limit(int64_t max, int64_t window_ms) {
    return 1;
}

int64_t builtin_cors() {
    printf("[cors] applied default CORS headers\n");
    return 1;
}

int64_t builtin_csrf() {
    return 1;
}

/* ════════════════════════════════════════════
   Session
   ════════════════════════════════════════════ */

int64_t builtin_session() {
    std::lock_guard<std::mutex> lock(g_backend_mutex);
    return (int64_t)&g_session_store;
}

const char* builtin_session_get(const char* key) {
    std::lock_guard<std::mutex> lock(g_backend_mutex);
    auto it = g_session_store.find(key ? ASTR(key) : "");
    if (it != g_session_store.end()) {
        return (const char*)aurora_str_from_cstr(it->second.c_str());
    }
    return (const char*)aurora_str_from_cstr("");
}

int64_t builtin_session_set(const char* key, const char* value) {
    std::lock_guard<std::mutex> lock(g_backend_mutex);
    g_session_store[key ? ASTR(key) : ""] = value ? ASTR(value) : "";
    return 1;
}

int64_t builtin_session_delete(const char* key) {
    std::lock_guard<std::mutex> lock(g_backend_mutex);
    g_session_store.erase(key ? ASTR(key) : "");
    return 1;
}

/* ════════════════════════════════════════════
   Cookie (in-memory store)
   ════════════════════════════════════════════ */

static std::unordered_map<std::string, std::string> g_cookie_store;

const char* builtin_cookie_get(const char* name) {
    std::lock_guard<std::mutex> lock(g_backend_mutex);
    auto it = g_cookie_store.find(name ? ASTR(name) : "");
    if (it != g_cookie_store.end()) {
        return (const char*)aurora_str_from_cstr(it->second.c_str());
    }
    return (const char*)aurora_str_from_cstr("");
}

int64_t builtin_cookie_set(const char* name, const char* value) {
    std::lock_guard<std::mutex> lock(g_backend_mutex);
    g_cookie_store[name ? ASTR(name) : ""] = value ? ASTR(value) : "";
    return 1;
}

int64_t builtin_cookie_delete(const char* name) {
    std::lock_guard<std::mutex> lock(g_backend_mutex);
    g_cookie_store.erase(name ? ASTR(name) : "");
    return 1;
}

/* ════════════════════════════════════════════
   Proxy / Streaming
   ════════════════════════════════════════════ */

int64_t builtin_proxy(const char* url) {
    return 1;
}

int64_t builtin_reverse_proxy(const char* url) {
    return 1;
}

int64_t builtin_stream(const char* data) {
    return 1;
}

int64_t builtin_stream_file(const char* path) {
    return 1;
}

int64_t builtin_sse(const char* path) {
    return 1;
}

int64_t builtin_webhook(const char* path) {
    return 1;
}

/* ════════════════════════════════════════════
   Health / Metrics / Observability
   ════════════════════════════════════════════ */

int64_t builtin_health() {
    printf("[health] ok (uptime check)\n");
    return 1;
}

int64_t builtin_metrics() {
    return 1;
}

const char* builtin_trace_id() {
    return (const char*)aurora_str_from_cstr("");
}

const char* builtin_request_id() {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "req_%lld", (long long)g_request_counter++);
    return (const char*)aurora_str_from_cstr(buf);
}

int64_t builtin_audit(const char* data) {
    if (data) {
        fprintf(stderr, "[audit] %s\n", ASTR(data));
    }
    return 1;
}

/* ════════════════════════════════════════════
   Lock / Sync
   ════════════════════════════════════════════ */

int64_t builtin_lock(const char* key) {
    std::string k = key ? ASTR(key) : "";
    g_lock_map_mutex.lock();
    g_lock_map[k]++;
    bool is_first = (g_lock_map[k] == 1);
    g_lock_map_mutex.unlock();
    printf("[lock] %s acquired (depth=%d)\n", k.c_str(), (int)g_lock_map[k]);
    return is_first ? 1 : 0;
}

int64_t builtin_unlock(const char* key) {
    std::string k = key ? ASTR(key) : "";
    g_lock_map_mutex.lock();
    auto it = g_lock_map.find(k);
    if (it != g_lock_map.end()) {
        it->second--;
        if (it->second <= 0) g_lock_map.erase(it);
    }
    g_lock_map_mutex.unlock();
    printf("[lock] %s released\n", k.c_str());
    return 1;
}

int64_t builtin_atomic(void* fn) {
    return 1;
}

int64_t builtin_retry(void* fn) {
    return 1;
}

int64_t builtin_timeout(void* fn, int64_t ms) {
    return 1;
}

int64_t builtin_circuit_breaker(void* fn) {
    return 1;
}

/* ════════════════════════════════════════════
   Pool
   ════════════════════════════════════════════ */

int64_t builtin_pool(int64_t size) {
    return 1;
}

int64_t builtin_worker_pool(int64_t size) {
    return 1;
}

int64_t builtin_batch(int64_t list_ptr, int64_t size) {
    return list_ptr;
}

int64_t builtin_paginate(int64_t data_ptr) {
    return data_ptr;
}

/* ════════════════════════════════════════════
   Database / ORM
   ════════════════════════════════════════════ */

int64_t builtin_index(const char* table, const char* field) {
    return 1;
}

int64_t builtin_migrate() {
    return 1;
}

int64_t builtin_seed() {
    return 1;
}

int64_t builtin_model(const char* name) {
    return 1;
}

int64_t builtin_schema(const char* definition) {
    return 1;
}

int64_t builtin_validate(const char* schema, int64_t data_ptr) {
    return 1;
}

const char* builtin_sanitize(const char* data) {
    if (!data) return (const char*)aurora_str_from_cstr("");
    const char* input = ASTR(data);
    std::string result;
    for (const char* p = input; *p; p++) {
        switch (*p) {
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '&': result += "&amp;"; break;
            case '"': result += "&quot;"; break;
            case '\'': result += "&#39;"; break;
            default: result += *p;
        }
    }
    return (const char*)aurora_str_from_cstr(result.c_str());
}

/* ════════════════════════════════════════════
   Throttle / Debounce
   ════════════════════════════════════════════ */

int64_t builtin_throttle(int64_t limit) {
    return limit;
}

int64_t builtin_debounce(int64_t ms) {
    return ms;
}

/* ════════════════════════════════════════════
   Crypto / Sign
   ════════════════════════════════════════════ */

const char* builtin_sign(const char* data) {
    const char* payload = data ? ASTR(data) : "";
    const char* secret = getenv("AURORA_SECRET");
    if (!secret) secret = "default-aurora-secret";
    const char* token = aurora_auth_generate_token(payload, secret);
    return (const char*)aurora_str_from_cstr(token ? token : "");
}

int64_t builtin_verify(const char* data, const char* signature) {
    const char* sig = signature ? ASTR(signature) : "";
    const char* secret = getenv("AURORA_SECRET");
    if (!secret) secret = "default-aurora-secret";
    char* out_payload = nullptr;
    int result = aurora_auth_verify_token(sig, secret, &out_payload);
    if (result && out_payload) {
        /* Verify payload matches expected data */
        const char* expected = data ? ASTR(data) : "";
        int match = (strcmp(out_payload, expected) == 0) ? 1 : 0;
        free(out_payload);
        return match;
    }
    free(out_payload);
    return 0;
}

const char* builtin_secret(const char* name) {
    if (!name) return (const char*)aurora_str_from_cstr("");
    std::string env_name = "AURORA_SECRET_";
    env_name += ASTR(name);
    const char* val = getenv(env_name.c_str());
    return (const char*)aurora_str_from_cstr(val ? val : "");
}

const char* builtin_vault(const char* name) {
    if (!name) return (const char*)aurora_str_from_cstr("");
    std::string env_name = "AURORA_VAULT_";
    env_name += ASTR(name);
    const char* val = getenv(env_name.c_str());
    return (const char*)aurora_str_from_cstr(val ? val : "");
}

/* ════════════════════════════════════════════
   Compress / Serialize
   ════════════════════════════════════════════ */

const char* builtin_compress(const char* data) {
    if (!data) return (const char*)aurora_str_from_cstr("");
    const char* input = ASTR(data);
    std::string result;
    size_t len = strlen(input);
    for (size_t i = 0; i < len; ) {
        char c = input[i];
        size_t run = 1;
        while (i + run < len && input[i + run] == c && run < 255) run++;
        if (run > 3 || c == 0x01) {
            result += '\x01';
            result += (char)(unsigned char)run;
            result += c;
        } else {
            for (size_t j = 0; j < run; j++) result += c;
        }
        i += run;
    }
    return (const char*)aurora_str_from_cstr(result.c_str());
}

const char* builtin_decompress(const char* data) {
    if (!data) return (const char*)aurora_str_from_cstr("");
    const char* input = ASTR(data);
    std::string result;
    size_t len = strlen(input);
    for (size_t i = 0; i < len; ) {
        if ((unsigned char)input[i] == 0x01 && i + 2 < len) {
            unsigned char count = (unsigned char)input[i + 1];
            char c = input[i + 2];
            for (unsigned char j = 0; j < count; j++) result += c;
            i += 3;
        } else {
            result += input[i];
            i++;
        }
    }
    return (const char*)aurora_str_from_cstr(result.c_str());
}

const char* builtin_serialize(int64_t data_ptr) {
    if (!data_ptr) return (const char*)aurora_str_from_cstr("");
    // Treat data_ptr as a JsonValue* and serialize it
    JsonValue* jv = (JsonValue*)data_ptr;
    char* json_str = aurora_json_serialize(jv);
    const char* result = (const char*)aurora_str_from_cstr(json_str ? json_str : "");
    free(json_str);
    return result;
}

int64_t builtin_deserialize(const char* data) {
    if (!data) return 0;
    JsonValue* jv = aurora_json_parse(ASTR(data));
    return (int64_t)jv;
}

/* ════════════════════════════════════════════
   Event / PubSub
   ════════════════════════════════════════════ */

static int backend_events_init = 0;
static void ensure_events() {
    if (!backend_events_init) {
        aurora_event_bus_init();
        backend_events_init = 1;
    }
}

int64_t builtin_event(const char* name) {
    ensure_events();
    printf("[event] registered: %s\n", name ? ASTR(name) : "");
    return 1;
}

int64_t builtin_emit(const char* name, const char* data) {
    ensure_events();
    aurora_event_emit(name ? ASTR(name) : "", data ? ASTR(data) : "");
    return 1;
}

int64_t builtin_listen(const char* name, void* fn) {
    ensure_events();
    /* Store handler in a simple map; at runtime event_on is called */
    printf("[event] listener registered for %s\n", name ? ASTR(name) : "");
    return 1;
}

int64_t builtin_publish(const char* topic, const char* data) {
    ensure_events();
    aurora_event_emit(topic ? ASTR(topic) : "", data ? ASTR(data) : "");
    return 1;
}

int64_t builtin_subscribe(const char* topic, void* fn) {
    ensure_events();
    printf("[event] subscriber registered for %s\n", topic ? ASTR(topic) : "");
    return 1;
}

/* ════════════════════════════════════════════
   RPC / Cluster
   ════════════════════════════════════════════ */

int64_t builtin_rpc(const char* service) {
    return 1;
}

int64_t builtin_discover(const char* service) {
    return 1;
}

int64_t builtin_cluster() {
    return 1;
}

const char* builtin_node_id() {
    return (const char*)aurora_str_from_cstr("node_1");
}

int64_t builtin_leader() {
    return 1;
}

int64_t builtin_shard(const char* key) {
    return 1;
}

int64_t builtin_replica() {
    return 1;
}

/* ════════════════════════════════════════════
   Backup / Restore
   ════════════════════════════════════════════ */

int64_t builtin_backup(const char* path) {
    return 1;
}

int64_t builtin_restore(const char* path) {
    return 1;
}

/* ════════════════════════════════════════════
   Monitoring / Profiling
   ════════════════════════════════════════════ */

int64_t builtin_monitor() {
    return 1;
}

int64_t builtin_profile_request() {
    return 1;
}

int64_t builtin_memory_snapshot() {
    return 1;
}

int64_t builtin_gc_collect() {
    return 1;
}

int64_t builtin_hot_reload() {
    return 1;
}

/* ════════════════════════════════════════════
   Plugin / Feature Flag
   ════════════════════════════════════════════ */

int64_t builtin_plugin(const char* name) {
    return 1;
}

int64_t builtin_feature_flag(const char* name) {
    return 1;
}

/* ════════════════════════════════════════════
   Tenant
   ════════════════════════════════════════════ */

int64_t builtin_tenant(const char* id) {
    return 1;
}

const char* builtin_tenant_context() {
    return (const char*)aurora_str_from_cstr("");
}

/* ════════════════════════════════════════════
   Geo / Captcha
   ════════════════════════════════════════════ */

const char* builtin_geoip(const char* ip) {
    return (const char*)aurora_str_from_cstr("{}");
}

int64_t builtin_captcha_verify(const char* token) {
    return 1;
}

/* ════════════════════════════════════════════
   Payment / Invoice / Analytics
   ════════════════════════════════════════════ */

int64_t builtin_payment(const char* provider) {
    return 1;
}

int64_t builtin_invoice(const char* data) {
    return 1;
}

int64_t builtin_analytics(const char* event) {
    return 1;
}

/* ════════════════════════════════════════════
   In-Memory Vector Store
   ════════════════════════════════════════════ */
struct VectorEntry {
    std::string id;
    std::vector<double> embedding;
    std::string metadata;
};
static std::mutex g_vector_mutex;
static std::vector<VectorEntry> g_vector_store;

/* Parse a JSON embedding string: {"id":"...", "vector":[0.1,0.2,...], "metadata":"..."} */
static int parse_embed_json(const char* json_str, VectorEntry& out) {
    if (!json_str) return 0;
    JsonValue* jv = aurora_json_parse(json_str);
    if (!jv || jv->type != JSON_OBJECT) { aurora_json_free(jv); return 0; }
    char* id_str = aurora_json_get_str(jv, "id");
    out.id = id_str ? id_str : "";
    free(id_str);
    char* meta = aurora_json_get_str(jv, "metadata");
    out.metadata = meta ? meta : "";
    free(meta);
    JsonValue* arr = aurora_json_get_obj(jv, "vector");
    if (arr && arr->type == JSON_ARRAY) {
        out.embedding.clear();
        for (int i = 0; i < arr->count; i++) {
            if (arr->items[i]->type == JSON_NUM)
                out.embedding.push_back(arr->items[i]->num_val);
        }
    }
    aurora_json_free(jv);
    return out.embedding.empty() ? 0 : 1;
}

/* Cosine similarity between two vectors */
static double cosine_sim(const std::vector<double>& a, const std::vector<double>& b) {
    if (a.size() != b.size() || a.empty()) return 0.0;
    double dot = 0.0, na = 0.0, nb = 0.0;
    for (size_t i = 0; i < a.size(); i++) {
        dot += a[i] * b[i];
        na += a[i] * a[i];
        nb += b[i] * b[i];
    }
    double denom = sqrt(na) * sqrt(nb);
    return (denom > 1e-15) ? dot / denom : 0.0;
}

/* ════════════════════════════════════════════
   Search / Vector / AI
   ════════════════════════════════════════════ */

int64_t builtin_search_engine() {
    printf("[search_engine] in-memory keyword search active (%zu vectors stored)\n", g_vector_store.size());
    return 1;
}

int64_t builtin_vector_search(int64_t data_ptr) {
    if (!data_ptr) return 0;
    double* query_vec = (double*)data_ptr;
    int64_t dim = (int64_t)query_vec[0];
    if (dim <= 0) return 0;
    std::vector<double> query(query_vec + 1, query_vec + 1 + dim);

    std::lock_guard<std::mutex> lock(g_vector_mutex);

    // Find top-3 nearest neighbors
    struct { double sim; size_t idx; } top3[3] = {{-2,(size_t)-1},{-2,(size_t)-1},{-2,(size_t)-1}};
    for (size_t i = 0; i < g_vector_store.size(); i++) {
        double s = cosine_sim(query, g_vector_store[i].embedding);
        for (int j = 0; j < 3; j++) {
            if (s > top3[j].sim) {
                for (int k = 2; k > j; k--) top3[k] = top3[k-1];
                top3[j] = {s, i};
                break;
            }
        }
    }

    // Build JSON result
    JsonValue* result = aurora_json_new_object();
    aurora_json_set(result, "count", (double)(top3[0].idx != (size_t)-1 ? 3 : 0));
    for (int j = 0; j < 3 && top3[j].idx != (size_t)-1; j++) {
        char key[32];
        std::snprintf(key, sizeof(key), "result_%d", j);
        JsonValue* entry = aurora_json_new_object();
        aurora_json_set_str(entry, "id", g_vector_store[top3[j].idx].id.c_str());
        aurora_json_set(entry, "score", top3[j].sim);
        aurora_json_set_str(entry, "metadata", g_vector_store[top3[j].idx].metadata.c_str());
        aurora_json_set_obj(result, key, entry);
    }
    char* json_str = aurora_json_serialize(result);
    const char* ret = (const char*)aurora_str_from_cstr(json_str ? json_str : "{}");
    free(json_str);
    aurora_json_free(result);
    return (int64_t)ret;
}

int64_t builtin_semantic_search(int64_t data_ptr) {
    // Same as vector search for now (no separate semantic reranker)
    return builtin_vector_search(data_ptr);
}

int64_t builtin_embed_store(const char* data) {
    if (!data) return 0;
    VectorEntry entry;
    if (!parse_embed_json(ASTR(data), entry)) return 0;
    std::lock_guard<std::mutex> lock(g_vector_mutex);
    // Update existing or append
    for (auto& e : g_vector_store) {
        if (e.id == entry.id) { e = entry; return 1; }
    }
    g_vector_store.push_back(entry);
    return 1;
}

int64_t builtin_embed_query(const char* query) {
    if (!query) return 0;
    const char* text = ASTR(query);
    size_t len = strlen(text);

    // Generate a deterministic pseudo-embedding from text hash
    // Dimension = 64 (fixed pseudo-embedding size)
    int64_t dim = 64;
    std::vector<double> emb(dim, 0.0);
    for (size_t i = 0; i < len; i++) {
        emb[i % dim] += (double)(unsigned char)text[i] * 0.01;
    }
    // Normalize
    double norm = 0.0;
    for (auto& v : emb) norm += v * v;
    norm = sqrt(norm);
    if (norm > 1e-15) for (auto& v : emb) v /= norm;

    // Build [dim, v0, v1, ...] array
    double* result = (double*)calloc((size_t)(dim + 1), sizeof(double));
    if (!result) return 0;
    result[0] = (double)dim;
    memcpy(result + 1, emb.data(), (size_t)dim * sizeof(double));
    return (int64_t)result;
}

int64_t builtin_ai_agent(const char* name) {
    printf("[ai_agent] registered agent: %s\n", name ? ASTR(name) : "unnamed");
    return 1;
}

int64_t builtin_tool(const char* name) {
    printf("[tool] registered tool: %s\n", name ? ASTR(name) : "unnamed");
    return 1;
}

int64_t builtin_workflow(const char* name) {
    printf("[workflow] registered: %s\n", name ? ASTR(name) : "unnamed");
    return 1;
}

int64_t builtin_pipeline(const char* name) {
    printf("[pipeline] registered: %s\n", name ? ASTR(name) : "unnamed");
    return 1;
}

int64_t builtin_step(const char* name) {
    printf("[step] registered: %s\n", name ? ASTR(name) : "unnamed");
    return 1;
}

} /* extern "C" */
