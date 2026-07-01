#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Forward declarations ── */
struct AuroraStaticRoute;
struct AuroraFileWatchState;
struct AuroraTLSContext;

/* ── Server ── */
typedef struct AuroraServer {
    int       port;
    int       running;
    void*     handle;
    void**    middleware_handlers;
    int       middleware_count;
    int       middleware_cap;
    AuroraStaticRoute* static_routes;
    int       static_count;
    int       static_cap;
    struct AuroraTLSContext* tls_ctx;  /* NULL = plain HTTP */
} AuroraServer;

AuroraServer* aurora_server_init(int64_t port);
void          aurora_server_start(AuroraServer* srv);
void          aurora_server_stop(AuroraServer* srv);
void          aurora_server_add_middleware(AuroraServer* srv, void* handler);
void          aurora_server_clear_middleware(AuroraServer* srv);

/* ── HTTP Request ── */
typedef struct AuroraHttpRequest {
    char* method;
    char* path;
    char* path_without_query;
    char* query_string;
    char* version;
    char** header_names;
    char** header_values;
    int    header_count;
    char** query_param_names;
    char** query_param_values;
    int    query_param_count;
    char*  body;
    /* Route path parameters (e.g. :id, :name) */
    char** param_names;
    char** param_values;
    int    param_count;
} AuroraHttpRequest;

/* ── HTTP Response ── */
typedef struct AuroraHttpResponse {
    int      status_code;
    char*    status_text;
    char**   header_names;
    char**   header_values;
    int      header_count;
    int      header_cap;
    char*    body;
    size_t   body_len;
    int      sent;
    int64_t  tls_handle;  /* 0 = raw socket, >0 = TLS connection handle */
} AuroraHttpResponse;

/* ── HTTP Router entry ── */
typedef struct AuroraRouteEntry {
    char*   method;
    char*   path_pattern;
    void*   handler;
} AuroraRouteEntry;

/* ── HTTP Route table ── */
typedef struct AuroraRouter {
    AuroraRouteEntry* entries;
    int               count;
    int               cap;
    int               use_prefix_match;
} AuroraRouter;

/* ── Request parser ── */
AuroraHttpRequest*  aurora_http_parse_request(const char* raw);
void                aurora_http_request_free(AuroraHttpRequest* req);
const char*         aurora_http_get_query_param(AuroraHttpRequest* req, const char* name);
const char*         aurora_http_get_header(AuroraHttpRequest* req, const char* name);
const char*         aurora_http_get_field(AuroraHttpRequest* req, const char* field);
const char*         aurora_http_get_param(AuroraHttpRequest* req, const char* name);

/* ── Response builder ── */
AuroraHttpResponse* aurora_http_response_new(void);
void                aurora_http_response_set_status(AuroraHttpResponse* res, int code, const char* text);
void                aurora_http_response_set_header(AuroraHttpResponse* res, const char* name, const char* value);
void                aurora_http_response_set_body(AuroraHttpResponse* res, const char* body);
void                aurora_http_response_set_body_n(AuroraHttpResponse* res, const char* body, size_t len);
void                aurora_http_response_set_json(AuroraHttpResponse* res, const char* body);
int                 aurora_http_response_send(AuroraHttpResponse* res, int64_t sock);
int                 aurora_http_response_send_chunked(AuroraHttpResponse* res, int64_t sock, int chunk_size);
void                aurora_http_response_free(AuroraHttpResponse* res);
void                aurora_http_response_set_content_type(AuroraHttpResponse* res, const char* content_type);
void                aurora_http_response_set_status_code(AuroraHttpResponse* res, int code);

/* ── Router ── */
AuroraRouter*       aurora_router_new(void);
void                aurora_route_add(AuroraRouter* router, const char* method, const char* path_pattern, void* handler);
int                 aurora_route_dispatch(AuroraRouter* router, AuroraHttpRequest* req, AuroraHttpResponse* res);
void                aurora_router_set_prefix_match(AuroraRouter* router, int enable);

/* ── Middleware ── */
typedef int (*AuroraMiddlewareFn)(AuroraHttpRequest*, AuroraHttpResponse*, void*);
int                 aurora_middleware_run_chain(void** handlers, int count, AuroraHttpRequest* req, AuroraHttpResponse* res);

/* ── CORS ── */
void aurora_cors_apply(AuroraHttpResponse* res, const char* origin, const char* methods, const char* headers);
void aurora_cors_apply_default(AuroraHttpResponse* res);
void aurora_cors_apply_with_origin(AuroraHttpResponse* res, const char* origin);

/* ── Database ── */
typedef struct AuroraDB {
    char*     conn_str;
    int       connected;
    void*     handle;
} AuroraDB;

AuroraDB* aurora_db_connect(const char* conn_str);
void*     aurora_db_query(AuroraDB* db, const char* query);
void      aurora_db_query_free(void* result);
void      aurora_db_close(AuroraDB* db);

/* ── Cache (in-memory with optional TTL) ── */
typedef struct AuroraCache {
    char**   keys;
    char**   values;
    int64_t* expires_at;
    int      count;
    int      cap;
} AuroraCache;

AuroraCache* aurora_cache_init(void);
void         aurora_cache_destroy(AuroraCache* cache);
void         aurora_cache_set(AuroraCache* cache, const char* key, const char* val);
void         aurora_cache_set_with_ttl(AuroraCache* cache, const char* key, const char* val, int64_t ttl_ms);
char*        aurora_cache_get(AuroraCache* cache, const char* key);
int          aurora_cache_has(AuroraCache* cache, const char* key);
void         aurora_cache_delete(AuroraCache* cache, const char* key);
void         aurora_cache_clear(AuroraCache* cache);
void         aurora_cache_clean_expired(AuroraCache* cache);

/* ── Session ── */
typedef struct AuroraSession {
    char*     session_id;
    AuroraCache* data;
    int64_t   created_at;
    int64_t   ttl_ms;
} AuroraSession;

AuroraSession* aurora_session_create(void);
void           aurora_session_destroy(AuroraSession* sess);
void           aurora_session_set_ttl(AuroraSession* sess, int64_t ttl_ms);
int            aurora_session_is_expired(AuroraSession* sess);
int64_t        aurora_session_age_ms(AuroraSession* sess);

/* ── Auth ── */
int         aurora_auth_login(const char* user, const char* pass);
const char* aurora_auth_generate_token(const char* payload, const char* secret);
int         aurora_auth_verify_token(const char* token, const char* secret, char** out_payload);
const char* aurora_auth_hash_password(const char* password);

/* ── Static file route ── */
struct AuroraStaticRoute {
    char* prefix;
    char* directory;
};

/* ── File watcher state (polling-based) ── */
struct AuroraFileWatchState {
    char**   files;
    int64_t* mtimes;
    int      count;
    int      cap;
};

/* ── TLS support ── */
struct AuroraTLSContext* aurora_tls_server_ctx_new(const char* cert_path, const char* key_path);
void aurora_tls_ctx_free(struct AuroraTLSContext* ctx);
int  aurora_server_enable_tls(AuroraServer* srv, const char* cert_path, const char* key_path);

/* ── WebSocket support (tls_handle = 0 for raw, >0 for TLS) ── */
int  aurora_ws_is_upgrade(const char* raw_request);
int  aurora_ws_upgrade(const char* key, int64_t sock, int64_t tls_handle);
int  aurora_ws_read_frame(int64_t sock, int64_t tls_handle, uint8_t** out_payload, int* opcode);
int  aurora_ws_write_frame(int64_t sock, int64_t tls_handle, int opcode, const uint8_t* data, int len);
void aurora_ws_close(int64_t sock, int64_t tls_handle);

/* ── Server accept + handle (with middleware + static routes) ── */
void aurora_server_accept_and_handle(AuroraServer* srv, AuroraRouter* router);
void aurora_server_accept_loop(AuroraServer* srv, AuroraRouter* router);

/* ── Static file serving ── */
void aurora_server_static(AuroraServer* srv, const char* prefix, const char* directory);
int  aurora_server_serve_static(AuroraHttpRequest* req, AuroraHttpResponse* res, AuroraServer* srv);

/* ── Simplified server run ── */
void aurora_route_register(const char* method, const char* path, void* handler);
void aurora_server_set_router(AuroraServer* srv, AuroraRouter* router);
void aurora_server_run(AuroraServer* srv);

/* ── Dev server with hot-reload ── */
void aurora_dev_server(int64_t port, const char* src_dir);

/* ── File watcher (polling) ── */
AuroraFileWatchState* aurora_fs_watch_init(const char* dir);
int  aurora_fs_watch_poll(AuroraFileWatchState* state);
void aurora_fs_watch_free(AuroraFileWatchState* state);

/* ── Todo Store ── */
const char* aurora_todo_list(void);
const char* aurora_todo_create(const char* title);
const char* aurora_todo_get(const char* id_str);
const char* aurora_todo_update(const char* id_str, const char* title, int64_t done);
const char* aurora_todo_delete(const char* id_str);

#ifdef __cplusplus
}
#endif
