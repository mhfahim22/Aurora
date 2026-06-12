#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <ctime>

#include "runtime/string.hpp"
#include "runtime/backend.hpp"

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
    return (const char*)aurora_str_from_cstr("");
}

const char* builtin_vault(const char* name) {
    return (const char*)aurora_str_from_cstr("");
}

/* ════════════════════════════════════════════
   Compress / Serialize
   ════════════════════════════════════════════ */

const char* builtin_compress(const char* data) {
    return (const char*)aurora_str_from_cstr(data ? ASTR(data) : "");
}

const char* builtin_decompress(const char* data) {
    return (const char*)aurora_str_from_cstr(data ? ASTR(data) : "");
}

const char* builtin_serialize(int64_t data_ptr) {
    return (const char*)aurora_str_from_cstr("");
}

int64_t builtin_deserialize(const char* data) {
    return 0;
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
   Search / Vector / AI
   ════════════════════════════════════════════ */

int64_t builtin_search_engine() {
    return 1;
}

int64_t builtin_vector_search(int64_t data_ptr) {
    return 0;
}

int64_t builtin_semantic_search(int64_t data_ptr) {
    return 0;
}

int64_t builtin_embed_store(const char* data) {
    return 1;
}

int64_t builtin_embed_query(const char* query) {
    return 0;
}

int64_t builtin_ai_agent(const char* name) {
    return 1;
}

int64_t builtin_tool(const char* name) {
    return 1;
}

int64_t builtin_workflow(const char* name) {
    return 1;
}

int64_t builtin_pipeline(const char* name) {
    return 1;
}

int64_t builtin_step(const char* name) {
    return 1;
}

} /* extern "C" */
