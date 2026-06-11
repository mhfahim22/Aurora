#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Server ── */
typedef struct AuroraServer {
    int       port;
    int       running;
    void*     handle;
} AuroraServer;

AuroraServer* aurora_server_init(int64_t port);
void          aurora_server_start(AuroraServer* srv);
void          aurora_server_stop(AuroraServer* srv);

/* ── HTTP Request ── */
typedef struct AuroraHttpRequest {
    char* method;
    char* path;
    char* version;
    char** header_names;
    char** header_values;
    int    header_count;
    char*  body;
} AuroraHttpRequest;

/* ── HTTP Response ── */
typedef struct AuroraHttpResponse {
    int    status_code;
    char*  status_text;
    char** header_names;
    char** header_values;
    int    header_count;
    int    header_cap;
    char*  body;
    int    sent;
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
} AuroraRouter;

/* ── Request parser ── */
AuroraHttpRequest*  aurora_http_parse_request(const char* raw);

/* ── Response builder ── */
AuroraHttpResponse* aurora_http_response_new(void);
void                aurora_http_response_set_status(AuroraHttpResponse* res, int code, const char* text);
void                aurora_http_response_set_header(AuroraHttpResponse* res, const char* name, const char* value);
void                aurora_http_response_set_body(AuroraHttpResponse* res, const char* body);
int                 aurora_http_response_send(AuroraHttpResponse* res, int64_t sock);

/* ── Router ── */
AuroraRouter*       aurora_router_new(void);
void                aurora_route_add(AuroraRouter* router, const char* method, const char* path_pattern, void* handler);
int                 aurora_route_dispatch(AuroraRouter* router, AuroraHttpRequest* req, AuroraHttpResponse* res);

/* ── Database ── */
typedef struct AuroraDB {
    char*     conn_str;
    int       connected;
    void*     handle;
} AuroraDB;

AuroraDB* aurora_db_connect(const char* conn_str);
void*     aurora_db_query(AuroraDB* db, const char* query);
void      aurora_db_close(AuroraDB* db);

/* ── Cache ── */
typedef struct AuroraCache {
    char**   keys;
    char**   values;
    int      count;
    int      cap;
} AuroraCache;

AuroraCache* aurora_cache_init(void);
void         aurora_cache_destroy(AuroraCache* cache);
void         aurora_cache_set(AuroraCache* cache, const char* key, const char* val);
char*        aurora_cache_get(AuroraCache* cache, const char* key);

/* ── Session ── */
typedef struct AuroraSession {
    char*     session_id;
    AuroraCache* data;
} AuroraSession;

AuroraSession* aurora_session_create(void);
void           aurora_session_destroy(AuroraSession* sess);

/* ── Auth ── */
int aurora_auth_login(const char* user, const char* pass);

/* ── Server enhancement (accept + route) ── */
void aurora_server_accept_and_handle(AuroraServer* srv, AuroraRouter* router);

#ifdef __cplusplus
}
#endif
