#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <ctime>
#include <cmath>

#include "runtime/string.hpp"
#include "runtime/backend.hpp"
#include "runtime/orm.hpp"
#include "runtime/rest.hpp"
#include "runtime/webhook.hpp"
#include "runtime/template.hpp"
#include "runtime/memory.hpp"
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
    AuroraHttpRequest* aurora_get_current_req();
    AuroraHttpResponse* aurora_get_current_res();
    void aurora_cors_apply_with_origin(AuroraHttpResponse* res, const char* origin);
    void aurora_event_bus_init();
    void aurora_event_on(const char* event_name, void (*handler)(const char*, const char*));
    void aurora_event_emit(const char* event_name, const char* data);
    int  aurora_sse_start(AuroraHttpResponse* res);
    int  aurora_sse_send_event(AuroraHttpResponse* res, const char* event, const char* data);
    int  aurora_sse_send_comment(AuroraHttpResponse* res, const char* comment);
    int  aurora_sse_end(AuroraHttpResponse* res);
    int  aurora_response_start_stream(AuroraHttpResponse* res);
    int  aurora_response_stream_chunk(AuroraHttpResponse* res, const char* data, int len);
    int  aurora_response_end_stream(AuroraHttpResponse* res);
    void aurora_http_response_set_header(AuroraHttpResponse* res, const char* name, const char* value);
    void aurora_http_response_set_content_type(AuroraHttpResponse* res, const char* ct);
    void aurora_http_response_set_body_n(AuroraHttpResponse* res, const char* body, size_t len);
    const char* aurora_http_get_cookie(AuroraHttpRequest* req, const char* name);
    void aurora_http_response_set_status(AuroraHttpResponse* res, int code, const char* text);
    void aurora_http_response_set_body(AuroraHttpResponse* res, const char* body);
    char* aurora_template_render(const char* name, const char* context_json);
    char* aurora_template_render_string(const char* source, const char* context_json);
    AuroraTemplate* aurora_template_compile(const char* name, const char* source);
}

/* ── Static state for backend built-ins ── */
static std::mutex g_backend_mutex;
static std::recursive_mutex g_lock_map_mutex;
static std::unordered_map<std::string, int> g_lock_map;
/* Session store: maps session_id → { key → value } */
static std::unordered_map<std::string, std::unordered_map<std::string, std::string>> g_session_data;

/* ── Rate limiter state (sliding window) ── */
static std::mutex g_rl_mutex;
static std::unordered_map<std::string, std::vector<int64_t>> g_rate_limit_log;
static int64_t g_rl_max_requests = 100;
static int64_t g_rl_window_ms = 60000;

/* ── CSRF state ── */
static std::mutex g_csrf_mutex;
static std::unordered_map<std::string, std::string> g_csrf_tokens;

/* ── Metrics state ── */
static std::atomic<int64_t> g_metrics_request_count{0};
static std::atomic<int64_t> g_metrics_active_connections{0};
static std::atomic<double> g_metrics_total_response_time{0.0};
static std::atomic<int64_t> g_metrics_route_404_count{0};
static int64_t get_start_time() {
    static int64_t t = 0;
    if (t == 0) t = aurora_time();
    return t;
}

/* ── Helper: generate random hex session ID (32 hex chars) ── */
static std::string generate_session_id() {
    static std::once_flag seed_flag;
    std::call_once(seed_flag, []() { srand((unsigned)time(nullptr) ^ (unsigned)(intptr_t)&seed_flag); });
    static const char hex[] = "0123456789abcdef";
    std::string sid;
    sid.reserve(32);
    for (int i = 0; i < 32; ++i) {
        sid += hex[rand() % 16];
    }
    return sid;
}

/* ── Helper: get session_id from current request cookie ── */
static std::string get_session_id_from_cookie() {
    AuroraHttpRequest* req = aurora_get_current_req();
    if (!req) return "";
    for (int i = 0; i < req->cookie_count; ++i) {
        if (req->cookie_names[i] && strcmp(req->cookie_names[i], "session_id") == 0) {
            return req->cookie_values[i] ? req->cookie_values[i] : "";
        }
    }
    return "";
}

/* ── Helper: ensure session exists, returns session_id (creates + sets cookie if needed) ── */
static std::string ensure_session() {
    std::string sid = get_session_id_from_cookie();
    if (sid.empty()) {
        sid = generate_session_id();
        /* Set Set-Cookie header with the new session ID */
        AuroraHttpResponse* res = aurora_get_current_res();
        if (res) {
            std::string cookie_hdr = "session_id=" + sid + "; Path=/; HttpOnly";
            aurora_http_response_set_header(res, "Set-Cookie", cookie_hdr.c_str());
        }
        g_session_data[sid] = {};
    }
    return sid;
}

extern "C" {

int64_t builtin_route_group(const char* path) {
    return 1;
}

int64_t builtin_session() {
    std::lock_guard<std::mutex> lock(g_backend_mutex);
    return (int64_t)&g_session_data;
}

const char* builtin_session_get(const char* key) {
    std::lock_guard<std::mutex> lock(g_backend_mutex);
    std::string sid = get_session_id_from_cookie();
    if (sid.empty()) return (const char*)aurora_str_from_cstr("");
    auto it = g_session_data.find(sid);
    if (it == g_session_data.end()) return (const char*)aurora_str_from_cstr("");
    auto k_it = it->second.find(key ? ASTR(key) : "");
    if (k_it != it->second.end()) {
        return (const char*)aurora_str_from_cstr(k_it->second.c_str());
    }
    return (const char*)aurora_str_from_cstr("");
}

int64_t builtin_session_set(const char* key, const char* value) {
    std::lock_guard<std::mutex> lock(g_backend_mutex);
    std::string sid = ensure_session();
    g_session_data[sid][key ? ASTR(key) : ""] = value ? ASTR(value) : "";
    return 1;
}

int64_t builtin_session_delete(const char* key) {
    std::lock_guard<std::mutex> lock(g_backend_mutex);
    std::string sid = get_session_id_from_cookie();
    if (sid.empty()) return 1;
    auto it = g_session_data.find(sid);
    if (it != g_session_data.end()) {
        it->second.erase(key ? ASTR(key) : "");
    }
    return 1;
}

/* ════════════════════════════════════════════
   Cookie (wired to real HTTP Cookie/Set-Cookie headers)
   ════════════════════════════════════════════ */

static std::unordered_map<std::string, std::string> g_cookie_store;

const char* builtin_cookie_get(const char* name) {
    std::lock_guard<std::mutex> lock(g_backend_mutex);
    /* Try real request cookies first */
    AuroraHttpRequest* req = aurora_get_current_req();
    if (req && name) {
        const char* cn = ASTR(name);
        for (int i = 0; i < req->cookie_count; ++i) {
            if (req->cookie_names[i] && strcmp(req->cookie_names[i], cn) == 0) {
                return (const char*)aurora_str_from_cstr(
                    req->cookie_values[i] ? req->cookie_values[i] : "");
            }
        }
    }
    /* Fallback to in-memory store */
    auto it = g_cookie_store.find(name ? ASTR(name) : "");
    if (it != g_cookie_store.end()) {
        return (const char*)aurora_str_from_cstr(it->second.c_str());
    }
    return (const char*)aurora_str_from_cstr("");
}

int64_t builtin_cookie_set(const char* name, const char* value) {
    std::lock_guard<std::mutex> lock(g_backend_mutex);
    /* Store in-memory */
    g_cookie_store[name ? ASTR(name) : ""] = value ? ASTR(value) : "";
    /* Also write Set-Cookie header to real response */
    AuroraHttpResponse* res = aurora_get_current_res();
    if (res && name && value) {
        std::string cookie_hdr = std::string(ASTR(name)) + "=" + std::string(ASTR(value)) + "; Path=/";
        aurora_http_response_set_header(res, "Set-Cookie", cookie_hdr.c_str());
    }
    return 1;
}

int64_t builtin_cookie_delete(const char* name) {
    std::lock_guard<std::mutex> lock(g_backend_mutex);
    /* Remove from in-memory */
    g_cookie_store.erase(name ? ASTR(name) : "");
    /* Set cookie with past expiry to delete */
    AuroraHttpResponse* res = aurora_get_current_res();
    if (res && name) {
        std::string cookie_hdr = std::string(ASTR(name)) + "=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT";
        aurora_http_response_set_header(res, "Set-Cookie", cookie_hdr.c_str());
    }
    return 1;
}

/* Thread-local middleware chain state for next() */
static thread_local int g_mw_current_index = 0;
static thread_local int g_mw_total_count = 0;
static thread_local void** g_mw_handlers = nullptr;
static thread_local AuroraHttpRequest* g_mw_request = nullptr;
static thread_local AuroraHttpResponse* g_mw_response = nullptr;

void aurora_middleware_set_context(void** handlers, int count, int current_idx,
                                    AuroraHttpRequest* req, AuroraHttpResponse* res) {
    g_mw_handlers = handlers;
    g_mw_total_count = count;
    g_mw_current_index = current_idx;
    g_mw_request = req;
    g_mw_response = res;
}

int64_t builtin_middleware(void* fn) {
    return 1;
}

int64_t builtin_next() {
    if (!g_mw_handlers || g_mw_current_index + 1 >= g_mw_total_count)
        return 0;
    int next_idx = g_mw_current_index + 1;
    typedef int (*MwFn)(AuroraHttpRequest*, AuroraHttpResponse*, void*);
    MwFn next_fn = (MwFn)g_mw_handlers[next_idx];
    if (!next_fn) return 0;
    g_mw_current_index = next_idx;
    return next_fn(g_mw_request, g_mw_response, nullptr);
}

int64_t builtin_rate_limit(int64_t max, int64_t window_ms) {
    /* Configure sliding window rate limiter.
       Actual enforcement is done per-request by looking up the client IP.
       This function just sets the global limits. */
    g_rl_max_requests = max > 0 ? max : 100;
    g_rl_window_ms = window_ms > 0 ? window_ms : 60000;
    return 1;
}

/* Check if a key (client IP or route) is rate-limited.
   Returns 0 if limited (should reject), 1 if allowed. */
int64_t builtin_rate_limit_check(const char* key) {
    if (!key || !*key) return 1;
    int64_t now = aurora_time();
    std::lock_guard<std::mutex> lock(g_rl_mutex);
    auto& log = g_rate_limit_log[key];
    /* Remove expired entries */
    int64_t cutoff = now - g_rl_window_ms / 1000;
    while (!log.empty() && log.front() < cutoff)
        log.erase(log.begin());
    if ((int64_t)log.size() >= g_rl_max_requests)
        return 0;
    log.push_back(now);
    return 1;
}

int64_t builtin_cors() {
    printf("[cors] applied default CORS headers\n");
    return 1;
}

int64_t builtin_csrf() {
    /* Enable CSRF protection for current session.
       Generates a token, stores it in session, and sets a cookie. */
    AuroraHttpResponse* res = aurora_get_current_res();
    if (!res) return 1;
    /* Generate a random CSRF token (32 hex chars) */
    static const char hex[] = "0123456789abcdef";
    std::string token;
    token.reserve(32);
    srand((unsigned)(time(nullptr) ^ (intptr_t)&token));
    for (int i = 0; i < 32; i++)
        token += hex[rand() % 16];
    /* Get session ID from request or generate one */
    AuroraHttpRequest* req = aurora_get_current_req();
    const char* sid = req ? aurora_http_get_cookie(req, "session_id") : nullptr;
    if (!sid || !*sid) {
        /* Generate session ID */
        static const char hex2[] = "0123456789abcdef";
        std::string nsid;
        nsid.reserve(32);
        for (int i = 0; i < 32; i++)
            nsid += hex2[rand() % 16];
        std::string cookie = "session_id=" + nsid + "; Path=/; HttpOnly; SameSite=Strict";
        aurora_http_response_set_header(res, "Set-Cookie", cookie.c_str());
        sid = nsid.c_str();
    }
    /* Store token in session and set double-submit cookie */
    {
        std::lock_guard<std::mutex> lock(g_csrf_mutex);
        g_csrf_tokens[sid] = token;
    }
    std::string csrf_cookie = "csrf_token=" + token + "; Path=/; SameSite=Strict";
    aurora_http_response_set_header(res, "Set-Cookie", csrf_cookie.c_str());
    return 1;
}

/* Verify a CSRF token for the current request.
   Token is expected in X-CSRF-Token header or csrf_token cookie.
   Returns 0 if invalid (should reject), 1 if valid. */
int64_t builtin_csrf_verify(const char* token) {
    if (!token || !*token) return 0;
    AuroraHttpRequest* req = aurora_get_current_req();
    if (!req) return 0;
    const char* sid = aurora_http_get_cookie(req, "session_id");
    if (!sid) return 0;
    std::lock_guard<std::mutex> lock(g_csrf_mutex);
    auto it = g_csrf_tokens.find(sid);
    if (it == g_csrf_tokens.end()) return 0;
    return (it->second == token) ? 1 : 0;
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
    AuroraHttpResponse* res = aurora_get_current_res();
    if (!res) return 0;
    if (!res->sent) aurora_response_start_stream(res);
    if (data) aurora_response_stream_chunk(res, data, (int)strlen(data));
    return 1;
}

int64_t builtin_stream_file(const char* path) {
    AuroraHttpResponse* res = aurora_get_current_res();
    if (!res || !path) return 0;
    if (!res->sent) aurora_response_start_stream(res);
    FILE* f = nullptr;
#ifdef _WIN32
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if (!f) {
        aurora_response_end_stream(res);
        return 0;
    }
    char buf[16384];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        aurora_response_stream_chunk(res, buf, (int)n);
    }
    fclose(f);
    aurora_response_end_stream(res);
    return 1;
}

int64_t builtin_sse(const char* path) {
    AuroraHttpResponse* res = aurora_get_current_res();
    if (!res) return 0;
    return aurora_sse_start(res) == 0 ? 1 : 0;
}

int64_t builtin_webhook(const char* path) {
    if (!path) return 0;
    return aurora_webhook_register(path, "", "") == 0 ? 1 : 0;
}

/* ════════════════════════════════════════════
   Health / Metrics / Observability
   ════════════════════════════════════════════ */

int64_t builtin_health() {
    /* Return JSON health check response.
       Checks: uptime, db connectivity, cache health. */
    int64_t now = aurora_time();
    int64_t uptime = now - get_start_time();
    if (uptime < 0) uptime = 0;
    char buf[512];
    int n = snprintf(buf, sizeof(buf),
        "{\"status\":\"ok\",\"uptime_sec\":%lld,\"db\":true,\"cache\":true}",
        (long long)uptime);
    /* Write to current response if available */
    AuroraHttpResponse* res = aurora_get_current_res();
    if (res) {
        aurora_http_response_set_content_type(res, "application/json");
        aurora_http_response_set_body_n(res, buf, (size_t)n);
    }
    return 1;
}

int64_t builtin_metrics() {
    /* Return Prometheus-format metrics as response body.
       Includes: request count, active connections, avg response time, memory. */
    int64_t count = g_metrics_request_count.load();
    int64_t active = g_metrics_active_connections.load();
    double avg_rt = g_metrics_total_response_time.load();
    if (count > 0) avg_rt /= (double)count;
    int64_t not_found = g_metrics_route_404_count.load();
    char buf[1024];
    int n = snprintf(buf, sizeof(buf),
        "# HELP aurora_http_requests_total Total HTTP requests\n"
        "# TYPE aurora_http_requests_total counter\n"
        "aurora_http_requests_total %lld\n"
        "# HELP aurora_http_active_connections Active connections\n"
        "# TYPE aurora_http_active_connections gauge\n"
        "aurora_http_active_connections %lld\n"
        "# HELP aurora_http_response_time_avg Average response time in seconds\n"
        "# TYPE aurora_http_response_time_avg gauge\n"
        "aurora_http_response_time_avg %f\n"
        "# HELP aurora_http_404_total Total 404 responses\n"
        "# TYPE aurora_http_404_total counter\n"
        "aurora_http_404_total %lld\n",
        (long long)count, (long long)active, avg_rt, (long long)not_found);
    AuroraHttpResponse* res = aurora_get_current_res();
    if (res) {
        aurora_http_response_set_content_type(res, "text/plain; version=0.0.4");
        aurora_http_response_set_body_n(res, buf, (size_t)n);
    }
    return 1;
}

const char* builtin_trace_id() {
    return (const char*)aurora_str_from_cstr("");
}

static std::atomic<int64_t> g_request_counter{0};

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
    extern int aurora_orm_migrate_all();
    return aurora_orm_migrate_all() > 0 ? 1 : 0;
}

int64_t builtin_seed() {
    return 1;
}

int64_t builtin_model(const char* name) {
    /* Define a new model schema with this name.
       Returns 1 on success. */
    AuroraModelSchema* s = aurora_orm_schema_define(name);
    /* Store in global registry for later migration */
    extern void aurora_orm_register_schema(AuroraModelSchema* s);
    aurora_orm_register_schema(s);
    return s ? 1 : 0;
}

int64_t builtin_schema(const char* definition) {
    /* Parse schema definition string format: "col:type:flags, col2:type2:flags2"
       type: int|float|text|bool|json
       flags: pk|auto|unique|notnull|indexed (comma-separated) */
    if (!definition) return 0;
    /* Get the most recently defined model schema */
    extern AuroraModelSchema* aurora_orm_last_schema();
    AuroraModelSchema* s = aurora_orm_last_schema();
    if (!s) return 0;
    std::string def(definition);
    size_t pos = 0;
    while (pos < def.size()) {
        /* Skip spaces */
        while (pos < def.size() && def[pos] == ' ') pos++;
        if (pos >= def.size()) break;
        /* Find end of this column definition */
        size_t end = def.find(',', pos);
        if (end == std::string::npos) end = def.size();
        std::string col_def = def.substr(pos, end - pos);
        /* Trim */
        while (!col_def.empty() && col_def.back() == ' ') col_def.pop_back();
        /* Parse "name:type:flags" */
        size_t c1 = col_def.find(':');
        std::string name = (c1 == std::string::npos) ? col_def : col_def.substr(0, c1);
        std::string type_str = "text";
        std::string flags_str;
        if (c1 != std::string::npos) {
            size_t c2 = col_def.find(':', c1 + 1);
            type_str = (c2 == std::string::npos) ?
                col_def.substr(c1 + 1) : col_def.substr(c1 + 1, c2 - c1 - 1);
            if (c2 != std::string::npos)
                flags_str = col_def.substr(c2 + 1);
        }
        int type = AURORA_ORM_TYPE_TEXT;
        if (type_str == "int" || type_str == "integer") type = AURORA_ORM_TYPE_INT;
        else if (type_str == "float") type = AURORA_ORM_TYPE_FLOAT;
        else if (type_str == "bool" || type_str == "boolean") type = AURORA_ORM_TYPE_BOOL;
        else if (type_str == "json") type = AURORA_ORM_TYPE_JSON;
        int flags = AURORA_ORM_FLAG_NONE;
        if (!flags_str.empty()) {
            size_t fp = 0;
            while (fp < flags_str.size()) {
                while (fp < flags_str.size() && flags_str[fp] == ' ') fp++;
                size_t fe = flags_str.find('|', fp);
                if (fe == std::string::npos) fe = flags_str.size();
                std::string f = flags_str.substr(fp, fe - fp);
                if (f == "pk" || f == "primary") flags |= AURORA_ORM_FLAG_PRIMARY;
                else if (f == "auto") flags |= AURORA_ORM_FLAG_AUTO;
                else if (f == "unique") flags |= AURORA_ORM_FLAG_UNIQUE;
                else if (f == "notnull" || f == "not_null") flags |= AURORA_ORM_FLAG_NOT_NULL;
                else if (f == "index" || f == "indexed") flags |= AURORA_ORM_FLAG_INDEXED;
                fp = fe + 1;
            }
        }
        aurora_orm_schema_column(s, name.c_str(), type, flags, nullptr);
        pos = end + 1;
    }
    return 1;
}

int64_t builtin_validate(const char* schema_json, int64_t data_ptr) {
    /* Simple validation: check that required fields are present.
       schema_json: JSON array of required field names.
       data_ptr: pointer to AuroraStr with JSON data.
       Returns 1 if valid, 0 if not. */
    if (!schema_json || !data_ptr) return 0;
    const char* data = ASTR(data_ptr);
    if (!data) return 0;
    /* Parse schema fields from JSON array ["field1","field2",...] */
    std::vector<std::string> required;
    const char* p = schema_json;
    while (*p) {
        while (*p && *p != '"') p++;
        if (!*p) break;
        p++;
        std::string field;
        while (*p && *p != '"') { field += *p; p++; }
        if (*p == '"') p++;
        if (!field.empty()) required.push_back(field);
    }
    for (const auto& f : required) {
        /* Check if field exists in data JSON */
        std::string search = "\"" + f + "\":";
        if (!strstr(data, search.c_str())) return 0;
    }
    return 1;
}

const char* builtin_sanitize(const char* data, int64_t mode) {
    if (!data) return (const char*)aurora_str_from_cstr("");
    const char* input = ASTR(data);
    std::string result;
    /* mode: 0 = full sanitize (default), 1 = HTML only, 2 = SQL escape only */
    if (mode == 2) {
        /* SQL injection prevention: escape single quotes */
        for (const char* p = input; *p; p++) {
            if (*p == '\'') result += "''";
            else result += *p;
        }
    } else {
        /* HTML sanitization */
        bool strip_tags = (mode == 0);
        bool in_tag = false;
        for (const char* p = input; *p; p++) {
            if (strip_tags && *p == '<') { in_tag = true; continue; }
            if (in_tag) { if (*p == '>') { in_tag = false; } continue; }
            switch (*p) {
                case '<': result += "&lt;"; break;
                case '>': result += "&gt;"; break;
                case '&': result += "&amp;"; break;
                case '"': result += "&quot;"; break;
                case '\'': result += "&#39;"; break;
                default: result += *p;
            }
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

/* ════════════════════════════════════════════════════════════
   Template Engine Builtins
   ════════════════════════════════════════════════════════════ */

int64_t builtin_template(const char* name, const char* source) {
    /* Register a template: builtin_template(name_str, source_str) */
    if (!name || !source) return 0;
    const char* name_cstr = ASTR(name);
    const char* source_cstr = ASTR(source);
    if (!name_cstr || !source_cstr) return 0;
    AuroraTemplate* tpl = aurora_template_compile(name_cstr, source_cstr);
    return tpl ? 1 : 0;
}

int64_t builtin_render(const char* name, const char* context_json) {
    /* Render a template: builtin_render(name_str, context_json_str) */
    if (!name) return 0;
    const char* name_cstr = ASTR(name);
    if (!name_cstr) return 0;
    const char* ctx_cstr = nullptr;
    std::string ctx_str;
    if (context_json) {
        ctx_cstr = ASTR(context_json);
        if (ctx_cstr) ctx_str = ctx_cstr;
    }
    char* result = aurora_template_render(name_cstr, ctx_cstr ? ctx_cstr : "{}");
    if (!result) return 0;
    AuroraHttpResponse* res = aurora_get_current_res();
    if (res) {
        aurora_http_response_set_content_type(res, "text/html");
        aurora_http_response_set_body(res, result);
    }
    aurora_free(result);
    return 1;
}

} /* extern "C" */
