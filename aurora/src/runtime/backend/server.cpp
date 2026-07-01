#include "runtime/backend.hpp"
#include "common/platform.hpp"
#include "runtime/memory.hpp"
#include "runtime/tls.hpp"
#include "runtime/websocket.hpp"
#include "std/json.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cmath>
#include <thread>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <algorithm>
#include <atomic>
#include <sys/stat.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#endif

#include "miniz.h"

extern "C" {

#ifdef _WIN32
static int backend_net_init = 0;
static void ensure_backend_winsock() {
    if (!backend_net_init) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        backend_net_init = 1;
    }
}
#endif

/* ── Current time in milliseconds ── */
static int64_t now_ms() {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    return (int64_t)std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

/* ── Server ── */
AuroraServer* aurora_server_init(int64_t port) {
    setbuf(stdout, NULL);
    AuroraServer* srv = (AuroraServer*)calloc(1, sizeof(AuroraServer));
    srv->port = (int)port;
    srv->running = 0;
    srv->handle = nullptr;
    srv->middleware_handlers = nullptr;
    srv->middleware_count = 0;
    srv->middleware_cap = 0;
    printf("[server] initialized on port %d\n", srv->port);
    return srv;
}

void aurora_server_start(AuroraServer* srv) {
    if (!srv || srv->running) return;
#ifdef _WIN32
    ensure_backend_winsock();
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        printf("[server] socket creation failed\n");
        return;
    }
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((short)srv->port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("[server] bind failed on port %d\n", srv->port);
        closesocket(sock);
        return;
    }
    listen(sock, SOMAXCONN);
    srv->handle = (void*)(intptr_t)sock;
#else
    int sock = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("[server] socket creation failed\n");
        return;
    }
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((short)srv->port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("[server] bind failed\n");
        close(sock);
        return;
    }
    listen(sock, SOMAXCONN);
    srv->handle = (void*)(intptr_t)(int64_t)sock;
#endif
    srv->running = 1;
    printf("[server] started on port %d\n", srv->port);
}

void aurora_server_stop(AuroraServer* srv) {
    if (!srv) return;
    srv->running = 0;
    if (srv->handle) {
#ifdef _WIN32
        closesocket((SOCKET)(intptr_t)srv->handle);
#else
        close((int)(intptr_t)srv->handle);
#endif
        srv->handle = nullptr;
    }
    printf("[server] stopped\n");
}

void aurora_server_add_middleware(AuroraServer* srv, void* handler) {
    if (!srv || !handler) return;
    if (srv->middleware_count >= srv->middleware_cap) {
        int new_cap = srv->middleware_cap ? srv->middleware_cap * 2 : 4;
        srv->middleware_handlers = (void**)aurora_safe_realloc(
            srv->middleware_handlers, (size_t)new_cap * sizeof(void*));
        srv->middleware_cap = new_cap;
    }
    srv->middleware_handlers[srv->middleware_count++] = handler;
    printf("[server] middleware #%d added\n", srv->middleware_count);
}

void aurora_server_clear_middleware(AuroraServer* srv) {
    if (!srv) return;
    free(srv->middleware_handlers);
    srv->middleware_handlers = nullptr;
    srv->middleware_count = 0;
    srv->middleware_cap = 0;
    printf("[server] middleware cleared\n");
}

/* ── Enable TLS on server ── */
int aurora_server_enable_tls(AuroraServer* srv, const char* cert_path, const char* key_path) {
    if (!srv) return 0;
    if (srv->tls_ctx) {
        aurora_tls_ctx_free(srv->tls_ctx);
        srv->tls_ctx = nullptr;
    }
    srv->tls_ctx = aurora_tls_server_ctx_new(cert_path, key_path);
    if (srv->tls_ctx) {
        printf("[server] TLS enabled\n");
        return 1;
    }
    printf("[server] TLS enable failed\n");
    return 0;
}

/* ── Query string parser ── */
static void parse_query_string(AuroraHttpRequest* req, const char* qs) {
    if (!req || !qs || !*qs) return;
    char buf[4096];
    strncpy(buf, qs, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* save;
    char* token = AURORA_STRTOK(buf, "&", &save);
    while (token) {
        char* eq = strchr(token, '=');
        const char* pname = token;
        const char* pval = "";
        if (eq) {
            *eq = '\0';
            pval = eq + 1;
        }
        req->query_param_count++;
        req->query_param_names = (char**)aurora_safe_realloc(
            req->query_param_names, (size_t)req->query_param_count * sizeof(char*));
        req->query_param_values = (char**)aurora_safe_realloc(
            req->query_param_values, (size_t)req->query_param_count * sizeof(char*));
        req->query_param_names[req->query_param_count - 1] = strdup(pname);
        req->query_param_values[req->query_param_count - 1] = strdup(pval);
        token = AURORA_STRTOK(nullptr, "&", &save);
    }
}

const char* aurora_http_get_query_param(AuroraHttpRequest* req, const char* name) {
    if (!req || !name) return nullptr;
    for (int i = 0; i < req->query_param_count; i++) {
        if (strcmp(req->query_param_names[i], name) == 0)
            return req->query_param_values[i];
    }
    return nullptr;
}

const char* aurora_http_get_header(AuroraHttpRequest* req, const char* name) {
    if (!req || !name) return nullptr;
    for (int i = 0; i < req->header_count; i++) {
        if (strcmp(req->header_names[i], name) == 0)
            return req->header_values[i];
    }
    return nullptr;
}

const char* aurora_http_get_field(AuroraHttpRequest* req, const char* field) {
    if (!req || !field) return nullptr;
    if (strcmp(field, "method") == 0) return req->method;
    if (strcmp(field, "path") == 0) return req->path;
    if (strcmp(field, "body") == 0) return req->body;
    if (strcmp(field, "query_string") == 0) return req->query_string;
    if (strcmp(field, "version") == 0) return req->version;
    return nullptr;
}

const char* aurora_http_get_param(AuroraHttpRequest* req, const char* name) {
    if (!req || !name) return nullptr;
    for (int i = 0; i < req->param_count; i++) {
        if (strcmp(req->param_names[i], name) == 0)
            return req->param_values[i];
    }
    return nullptr;
}

/* ── HTTP Request Parser ── */
AuroraHttpRequest* aurora_http_parse_request(const char* raw) {
    if (!raw) return nullptr;
    AuroraHttpRequest* req = (AuroraHttpRequest*)calloc(1, sizeof(AuroraHttpRequest));

    size_t raw_len = strlen(raw);
    char* buf = (char*)malloc(raw_len + 1);
    if (!buf) { free(req); return nullptr; }
    memcpy(buf, raw, raw_len + 1);

    char* line = buf;
    char* end = strstr(line, "\r\n");
    if (end) *end = '\0';

    char* save;
    char* method = AURORA_STRTOK(line, " ", &save);
    char* path = AURORA_STRTOK(nullptr, " ", &save);
    char* version = AURORA_STRTOK(nullptr, " ", &save);
    if (method) req->method = strdup(method);
    if (path) {
        req->path = strdup(path);
        /* Split query string from path */
        char* qmark = strchr(req->path, '?');
        if (qmark) {
            *qmark = '\0';
            req->query_string = strdup(qmark + 1);
            req->path_without_query = strdup(req->path);
            parse_query_string(req, req->query_string);
        } else {
            req->path_without_query = strdup(req->path);
            req->query_string = nullptr;
        }
    }
    if (version) req->version = strdup(version);

    req->header_names = nullptr;
    req->header_values = nullptr;
    req->header_count = 0;
    req->query_param_names = nullptr;
    req->query_param_values = nullptr;
    req->query_param_count = 0;
    req->param_names = nullptr;
    req->param_values = nullptr;
    req->param_count = 0;

    char* hdr_line = end ? end + 2 : nullptr;
    while (hdr_line && *hdr_line) {
        char* hdr_end = strstr(hdr_line, "\r\n");
        if (!hdr_end) break;
        if (hdr_end == hdr_line) {
            hdr_line = hdr_end + 2;
            break;
        }
        *hdr_end = '\0';
        char* colon = strchr(hdr_line, ':');
        if (colon) {
            *colon = '\0';
            char* hdr_name = hdr_line;
            char* hdr_val = colon + 1;
            while (*hdr_val == ' ') hdr_val++;
            req->header_count++;
            req->header_names = (char**)aurora_safe_realloc(req->header_names, (size_t)req->header_count * sizeof(char*));
            req->header_values = (char**)aurora_safe_realloc(req->header_values, (size_t)req->header_count * sizeof(char*));
            req->header_names[req->header_count - 1] = strdup(hdr_name);
            req->header_values[req->header_count - 1] = strdup(hdr_val);
        }
        hdr_line = hdr_end + 2;
    }

    if (hdr_line && *hdr_line) {
        req->body = strdup(hdr_line);
    } else {
        req->body = nullptr;
    }

    printf("[http] %s %s\n", req->method ? req->method : "?", req->path ? req->path : "?");
    free(buf);
    return req;
}

void aurora_http_request_free(AuroraHttpRequest* req) {
    if (!req) return;
    free(req->method);
    free(req->path);
    free(req->path_without_query);
    free(req->query_string);
    free(req->version);
    for (int i = 0; i < req->header_count; i++) {
        free(req->header_names[i]);
        free(req->header_values[i]);
    }
    free(req->header_names);
    free(req->header_values);
    for (int i = 0; i < req->query_param_count; i++) {
        free(req->query_param_names[i]);
        free(req->query_param_values[i]);
    }
    free(req->query_param_names);
    free(req->query_param_values);
    for (int i = 0; i < req->param_count; i++) {
        free(req->param_names[i]);
        free(req->param_values[i]);
    }
    free(req->param_names);
    free(req->param_values);
    free(req->body);
    free(req);
}

/* ── HTTP Response Builder ── */
AuroraHttpResponse* aurora_http_response_new() {
    AuroraHttpResponse* res = (AuroraHttpResponse*)calloc(1, sizeof(AuroraHttpResponse));
    res->status_code = 200;
    res->status_text = strdup("OK");
    res->header_cap = 16;
    res->header_count = 0;
    res->header_names = (char**)calloc((size_t)res->header_cap, sizeof(char*));
    res->header_values = (char**)calloc((size_t)res->header_cap, sizeof(char*));
    res->body = nullptr;
    res->sent = 0;
    return res;
}

void aurora_http_response_free(AuroraHttpResponse* res) {
    if (!res) return;
    free(res->status_text);
    for (int i = 0; i < res->header_count; i++) {
        free(res->header_names[i]);
        free(res->header_values[i]);
    }
    free(res->header_names);
    free(res->header_values);
    free(res->body);
    free(res);
}

void aurora_http_response_set_status(AuroraHttpResponse* res, int code, const char* text) {
    if (!res) return;
    res->status_code = code;
    free(res->status_text);
    res->status_text = text ? strdup(text) : strdup("Unknown");
}

void aurora_http_response_set_status_code(AuroraHttpResponse* res, int code) {
    if (!res) return;
    res->status_code = code;
    free(res->status_text);
    switch (code) {
        case 200: res->status_text = strdup("OK"); break;
        case 201: res->status_text = strdup("Created"); break;
        case 204: res->status_text = strdup("No Content"); break;
        case 301: res->status_text = strdup("Moved Permanently"); break;
        case 302: res->status_text = strdup("Found"); break;
        case 304: res->status_text = strdup("Not Modified"); break;
        case 400: res->status_text = strdup("Bad Request"); break;
        case 401: res->status_text = strdup("Unauthorized"); break;
        case 403: res->status_text = strdup("Forbidden"); break;
        case 404: res->status_text = strdup("Not Found"); break;
        case 405: res->status_text = strdup("Method Not Allowed"); break;
        case 409: res->status_text = strdup("Conflict"); break;
        case 422: res->status_text = strdup("Unprocessable Entity"); break;
        case 429: res->status_text = strdup("Too Many Requests"); break;
        case 500: res->status_text = strdup("Internal Server Error"); break;
        case 502: res->status_text = strdup("Bad Gateway"); break;
        case 503: res->status_text = strdup("Service Unavailable"); break;
        default:  res->status_text = strdup("Unknown"); break;
    }
}

void aurora_http_response_set_header(AuroraHttpResponse* res, const char* name, const char* value) {
    if (!res || !name || !value) return;
    /* Replace existing header with same name */
    for (int i = 0; i < res->header_count; i++) {
        if (strcmp(res->header_names[i], name) == 0) {
            free(res->header_values[i]);
            res->header_values[i] = strdup(value);
            return;
        }
    }
    if (res->header_count >= res->header_cap) {
        res->header_cap *= 2;
        res->header_names = (char**)aurora_safe_realloc(res->header_names, (size_t)res->header_cap * sizeof(char*));
        res->header_values = (char**)aurora_safe_realloc(res->header_values, (size_t)res->header_cap * sizeof(char*));
    }
    res->header_names[res->header_count] = strdup(name);
    res->header_values[res->header_count] = strdup(value);
    res->header_count++;
}

void aurora_http_response_set_content_type(AuroraHttpResponse* res, const char* content_type) {
    if (!res) return;
    aurora_http_response_set_header(res, "Content-Type", content_type);
}

void aurora_http_response_set_body(AuroraHttpResponse* res, const char* body) {
    if (!res) return;
    free(res->body);
    res->body = body ? strdup(body) : nullptr;
    res->body_len = res->body ? strlen(res->body) : 0;
}

void aurora_http_response_set_body_n(AuroraHttpResponse* res, const char* body, size_t len) {
    if (!res) return;
    free(res->body);
    if (body && len > 0) {
        res->body = (char*)malloc(len + 1);
        if (res->body) {
            memcpy(res->body, body, len);
            res->body[len] = '\0';
        }
    } else {
        res->body = nullptr;
    }
    res->body_len = len;
}

void aurora_http_response_set_json(AuroraHttpResponse* res, const char* body) {
    if (!res) return;
    aurora_http_response_set_content_type(res, "application/json");
    aurora_http_response_set_body(res, body);
}

/* ── Send bytes via raw socket or TLS ── */
static int send_raw(int64_t sock, int64_t tls_handle, const char* data, int len) {
    if (tls_handle > 0) {
        return aurora_tls_write(tls_handle, data, len);
    }
#ifdef _WIN32
    return (int)send((SOCKET)(intptr_t)sock, data, len, 0);
#else
    return (int)send((int)sock, data, (size_t)len, MSG_NOSIGNAL);
#endif
}

int aurora_http_response_send(AuroraHttpResponse* res, int64_t sock) {
    if (!res || res->sent) return -1;
    int64_t tls = res->tls_handle;
    char header_buf[4096];
    int len = snprintf(header_buf, sizeof(header_buf),
        "HTTP/1.1 %d %s\r\n", res->status_code, res->status_text ? res->status_text : "Unknown");

    /* Track if Content-Length is already explicitly set (e.g. for gzip) */
    int content_length_set = 0;
    size_t body_len = res->body_len > 0 ? res->body_len : (res->body ? strlen(res->body) : 0);

    for (int i = 0; i < res->header_count; i++) {
        if (strcmp(res->header_names[i], "Content-Length") == 0) {
            content_length_set = 1;
            body_len = (size_t)atoll(res->header_values[i]);
        }
        len += snprintf(header_buf + len, sizeof(header_buf) - (size_t)len - 1,
            "%s: %s\r\n", res->header_names[i], res->header_values[i]);
    }

    if (!content_length_set) {
        len += snprintf(header_buf + len, sizeof(header_buf) - (size_t)len - 1,
            "Content-Length: %zu\r\n\r\n", body_len);
    } else {
        len += snprintf(header_buf + len, sizeof(header_buf) - (size_t)len - 1, "\r\n");
    }

    const char* p = header_buf;
    int remaining = len;
    while (remaining > 0) {
        int n = send_raw(sock, tls, p, remaining);
        if (n <= 0) break;
        p += n;
        remaining -= n;
    }
    if (res->body) {
        p = res->body;
        int bremaining = (int)body_len;
        while (bremaining > 0) {
            int n = send_raw(sock, tls, p, bremaining);
            if (n <= 0) break;
            p += n;
            bremaining -= n;
        }
    }
    res->sent = 1;
    fprintf(stderr, "[http] response %d sent (body_len=%zu, content_length_set=%d)\n", res->status_code, body_len, content_length_set);
    return 0;
}

/* ── Chunked transfer encoding response sender ── */
int aurora_http_response_send_chunked(AuroraHttpResponse* res, int64_t sock, int chunk_size) {
    if (!res || res->sent) return -1;
    if (chunk_size <= 0) chunk_size = 4096;
    int64_t tls = res->tls_handle;
    char header_buf[4096];
    int len = snprintf(header_buf, sizeof(header_buf),
        "HTTP/1.1 %d %s\r\n", res->status_code, res->status_text ? res->status_text : "Unknown");

    for (int i = 0; i < res->header_count; i++) {
        if (strcmp(res->header_names[i], "Content-Length") == 0)
            continue;
        len += snprintf(header_buf + len, sizeof(header_buf) - (size_t)len - 1,
            "%s: %s\r\n", res->header_names[i], res->header_values[i]);
    }

    len += snprintf(header_buf + len, sizeof(header_buf) - (size_t)len - 1,
        "Transfer-Encoding: chunked\r\n\r\n");

    {
        const char* p = header_buf;
        int remaining = len;
        while (remaining > 0) {
            int n = send_raw(sock, tls, p, remaining);
            if (n <= 0) return -1;
            p += n;
            remaining -= n;
        }
    }

    if (res->body) {
        size_t body_len = res->body_len > 0 ? res->body_len : strlen(res->body);
        const char* p = res->body;
        while (body_len > 0) {
            size_t this_chunk = (size_t)chunk_size;
            if (this_chunk > body_len) this_chunk = body_len;
            char size_buf[32];
            int size_len = snprintf(size_buf, sizeof(size_buf), "%zx\r\n", this_chunk);
            {
                const char* sp = size_buf;
                int s_remaining = size_len;
                while (s_remaining > 0) {
                    int n = send_raw(sock, tls, sp, s_remaining);
                    if (n <= 0) return -1;
                    sp += n;
                    s_remaining -= n;
                }
            }
            {
                int c_remaining = (int)this_chunk;
                while (c_remaining > 0) {
                    int n = send_raw(sock, tls, p, c_remaining);
                    if (n <= 0) return -1;
                    p += n;
                    c_remaining -= n;
                }
            }
            {
                const char* crlf = "\r\n";
                send_raw(sock, tls, crlf, 2);
            }
            body_len -= this_chunk;
        }
    }

    {
        const char* term = "0\r\n\r\n";
        send_raw(sock, tls, term, 5);
    }

    res->sent = 1;
    fprintf(stderr, "[http] chunked response %d sent (chunk_size=%d)\n", res->status_code, chunk_size);
    return 0;
}

/* ── CORS ── */
void aurora_cors_apply(AuroraHttpResponse* res, const char* origin, const char* methods, const char* headers) {
    if (!res) return;
    aurora_http_response_set_header(res, "Access-Control-Allow-Origin", origin ? origin : "*");
    aurora_http_response_set_header(res, "Access-Control-Allow-Methods", methods ? methods : "GET, POST, PUT, DELETE, OPTIONS");
    aurora_http_response_set_header(res, "Access-Control-Allow-Headers", headers ? headers : "Content-Type, Authorization");
    aurora_http_response_set_header(res, "Access-Control-Max-Age", "86400");
}

void aurora_cors_apply_default(AuroraHttpResponse* res) {
    aurora_cors_apply(res, "*", nullptr, nullptr);
}

void aurora_cors_apply_with_origin(AuroraHttpResponse* res, const char* origin) {
    aurora_cors_apply(res, origin, nullptr, nullptr);
}

/* ── Middleware chain ── */
int aurora_middleware_run_chain(void** handlers, int count,
                                AuroraHttpRequest* req, AuroraHttpResponse* res) {
    if (!handlers || count <= 0) return 0;
    for (int i = 0; i < count; i++) {
        if (!handlers[i]) continue;
        AuroraMiddlewareFn fn = (AuroraMiddlewareFn)handlers[i];
        int result = fn(req, res, nullptr);
        if (result != 0) {
            printf("[middleware] #%d stopped chain (result=%d)\n", i, result);
            return result;
        }
    }
    return 0;
}

/* ── Router ── */
AuroraRouter* aurora_router_new() {
    AuroraRouter* r = (AuroraRouter*)calloc(1, sizeof(AuroraRouter));
    r->cap = 16;
    r->count = 0;
    r->entries = (AuroraRouteEntry*)calloc((size_t)r->cap, sizeof(AuroraRouteEntry));
    r->use_prefix_match = 0;
    return r;
}

void aurora_router_set_prefix_match(AuroraRouter* router, int enable) {
    if (!router) return;
    router->use_prefix_match = enable;
}

void aurora_route_add(AuroraRouter* router, const char* method, const char* path_pattern, void* handler) {
    if (!router || !method || !path_pattern || !handler) return;
    if (router->count >= router->cap) {
        router->cap *= 2;
        router->entries = (AuroraRouteEntry*)aurora_safe_realloc(router->entries, (size_t)router->cap * sizeof(AuroraRouteEntry));
    }
    router->entries[router->count].method = strdup(method);
    router->entries[router->count].path_pattern = strdup(path_pattern);
    router->entries[router->count].handler = handler;
    router->count++;
    printf("[router] %s %s registered\n", method, path_pattern);
}

/* ── Match path with :param patterns (e.g. /user/:id matches /user/42) ── */
static int match_route_pattern(const char* pattern, const char* path,
                                char*** out_names, char*** out_vals, int* out_count) {
    *out_names = nullptr;
    *out_vals = nullptr;
    *out_count = 0;
    if (!pattern || !path) return 0;

    /* Quick exact match check first */
    if (strcmp(pattern, path) == 0) return 1;
    if (!strchr(pattern, ':')) return 0; /* no params, already checked exact */

    /* Split both by '/' and compare segment by segment */
    char pat_copy[1024], path_copy[1024];
    strncpy(pat_copy, pattern, sizeof(pat_copy) - 1);
    pat_copy[sizeof(pat_copy) - 1] = '\0';
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    /* Count segments */
    int pat_seg_count = 0, path_seg_count = 0;
    for (const char* p = pat_copy; *p; p++) if (*p == '/') pat_seg_count++;
    for (const char* p = path_copy; *p; p++) if (*p == '/') path_seg_count++;
    pat_seg_count++; path_seg_count++; /* count first segment */
    if (pat_seg_count != path_seg_count) return 0;

    /* Extract and compare segments */
    char* pat_save, *path_save;
    char* pat_seg = AURORA_STRTOK(pat_copy, "/", &pat_save);
    char* path_seg = AURORA_STRTOK(path_copy, "/", &path_save);

    /* Temporary storage */
    char* names[64];
    char* vals[64];
    int count = 0;

    while (pat_seg && path_seg) {
        if (pat_seg[0] == ':') {
            /* Parameter capture */
            if (count < 64) {
                names[count] = strdup(pat_seg + 1);
                vals[count] = strdup(path_seg);
                count++;
            }
        } else if (strcmp(pat_seg, path_seg) != 0) {
            /* Mismatch */
            for (int j = 0; j < count; j++) { free(names[j]); free(vals[j]); }
            return 0;
        }
        pat_seg = AURORA_STRTOK(nullptr, "/", &pat_save);
        path_seg = AURORA_STRTOK(nullptr, "/", &path_save);
    }

    if (count > 0) {
        *out_names = (char**)malloc((size_t)count * sizeof(char*));
        *out_vals = (char**)malloc((size_t)count * sizeof(char*));
        for (int i = 0; i < count; i++) {
            (*out_names)[i] = names[i];
            (*out_vals)[i] = vals[i];
        }
        *out_count = count;
    }
    return 1;
}

int aurora_route_dispatch(AuroraRouter* router, AuroraHttpRequest* req, AuroraHttpResponse* res) {
    if (!router || !req || !res) return -1;
    const char* path_match = req->path_without_query ? req->path_without_query : req->path;

    for (int i = 0; i < router->count; i++) {
        /* Method match */
        if (strcmp(router->entries[i].method, "*") != 0 &&
            strcmp(router->entries[i].method, req->method) != 0)
            continue;

        /* Path match with optional :param extraction */
        char** param_names = nullptr;
        char** param_vals = nullptr;
        int param_count = 0;

        int matched = 0;
        if (router->use_prefix_match) {
            size_t plen = strlen(router->entries[i].path_pattern);
            if (strncmp(router->entries[i].path_pattern, path_match, plen) == 0)
                matched = 1;
        } else if (strchr(router->entries[i].path_pattern, ':')) {
            matched = match_route_pattern(router->entries[i].path_pattern, path_match,
                                          &param_names, &param_vals, &param_count);
        } else {
            matched = (strcmp(router->entries[i].path_pattern, path_match) == 0);
        }

        if (matched) {
            /* Attach path params to request */
            if (param_count > 0) {
                req->param_names = param_names;
                req->param_values = param_vals;
                req->param_count = param_count;
            }
            typedef void (*HandlerFn)(AuroraHttpRequest*, AuroraHttpResponse*);
            HandlerFn fn = (HandlerFn)router->entries[i].handler;
            fn(req, res);
            return 0;
        }
        free(param_names);
        free(param_vals);
    }
    aurora_http_response_set_status(res, 404, "Not Found");
    aurora_http_response_set_body(res, "404 Not Found");
    return 1;
}

/* ── Check a header value in raw HTTP request (case-insensitive name) ── */
#ifdef _WIN32
#define aurora_strncasecmp _strnicmp
#else
#define aurora_strncasecmp strncasecmp
#endif

static int has_header_value(const char* raw, const char* header_name, const char* expected_val) {
    if (!raw || !header_name || !expected_val) return 0;
    size_t hdr_len = strlen(header_name);
    const char* hdr = nullptr;
    const char* p = raw;
    while (*p) {
        if (aurora_strncasecmp(p, header_name, hdr_len) == 0 &&
            (p[hdr_len] == ':' || p[hdr_len] == ' ')) {
            hdr = p;
            break;
        }
        p++;
    }
    if (!hdr) {
        fprintf(stderr, "[gzip-debug] hdr NOT found in raw for '%s'\n", header_name);
        fprintf(stderr, "[gzip-debug] raw=%.200s\n", raw);
        return 0;
    }
    hdr += hdr_len;
    while (*hdr == ' ') hdr++;
    /* Search for expected_val as a token in comma-separated list */
    size_t ev_len = strlen(expected_val);
    int ret = 0;
    p = hdr;
    while (*p) {
        while (*p == ' ' || *p == ',') p++;
        if (strncmp(p, expected_val, ev_len) == 0) {
            char after = p[ev_len];
            if (after == '\0' || after == ',' || after == ' ' || after == '\r' || after == '\n') {
                ret = 1;
                break;
            }
        }
        while (*p && *p != ',') p++;
    }
    fprintf(stderr, "[gzip-debug] hdr found, value='%.30s' searching for '%s' => %d\n", hdr, expected_val, ret);
    return ret;
}

/* ── Check if raw request has Connection: keep-alive ── */
static int has_keep_alive(const char* raw) {
    return has_header_value(raw, "Connection:", "keep-alive");
}

/* ── Check if raw request accepts gzip ── */
static int accepts_gzip(const char* raw) {
    int ret = has_header_value(raw, "Accept-Encoding:", "gzip");
    fprintf(stderr, "[gzip-debug] accepts_gzip returning %d\n", ret);
    if (!ret && raw) {
        const char* ae = strstr(raw, "Accept");
        if (ae) {
            const char* newline = strstr(ae, "\r\n");
            if (!newline) newline = ae + 80;
            int len = (int)(newline - ae);
            if (len > 80) len = 80;
            fprintf(stderr, "[gzip-debug] Accept header near match: %.*s\n", len, ae);
        } else {
            fprintf(stderr, "[gzip-debug] No 'Accept' in raw request\n");
        }
    }
    return ret;
}

/* ── Set socket receive timeout ── */
static void set_recv_timeout(int64_t client_sock, int timeout_sec) {
#ifdef _WIN32
    DWORD tv = (DWORD)timeout_sec * 1000;
    setsockopt((SOCKET)(intptr_t)client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#else
    struct timeval tv;
    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;
    setsockopt((int)client_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
}

/* Forward decls for gzip compression */
static unsigned int gzip_crc32(const unsigned char* buf, size_t len);
static unsigned char* gzip_compress(const unsigned char* input, size_t input_len, size_t* out_len);

/* ── Per-client request handling (runs in its own thread) ── */
static void handle_client(int64_t client_sock, int64_t tls_handle, AuroraServer* srv, AuroraRouter* router) {
#ifdef _WIN32
    SOCKET client = (SOCKET)(intptr_t)client_sock;
#else
    int client = (int)client_sock;
#endif
    set_recv_timeout(client_sock, 5);
    char raw[65536];
    int keep_alive_max = 100;

    /* ── Recv helper: read from raw socket or TLS ── */
    auto recv_data = [&](char* buf, int buf_size) -> int {
        if (tls_handle > 0) {
            return aurora_tls_read(tls_handle, buf, buf_size);
        }
#ifdef _WIN32
        return recv(client, buf, buf_size, 0);
#else
        return (int)recv(client, buf, (size_t)buf_size, 0);
#endif
    };

    for (int ka_count = 0; ka_count < keep_alive_max; ka_count++) {
        int n = recv_data(raw, (int)sizeof(raw) - 1);
        if (n <= 0) break;
        raw[n] = '\0';

        /* ── Check for WebSocket upgrade ── */
        if (aurora_ws_is_upgrade(raw)) {
            const char* ws_key = aurora_ws_get_key(raw);
            if (ws_key) {
                if (aurora_ws_upgrade(ws_key, client_sock, tls_handle) == 1) {
                    fprintf(stderr, "[ws] upgrade successful\n");
                    uint8_t* payload = NULL;
                    int opcode = 0;
                    while (1) {
                        int plen = aurora_ws_read_frame(client_sock, tls_handle, &payload, &opcode);
                        if (plen <= 0) break;
                        if (opcode == WS_OPCODE_CLOSE) {
                            aurora_ws_close(client_sock, tls_handle);
                            aurora_free(payload);
                            break;
                        }
                        if (opcode == WS_OPCODE_PING) {
                            aurora_ws_write_frame(client_sock, tls_handle, WS_OPCODE_PONG, payload, plen);
                        } else if (opcode == WS_OPCODE_TEXT || opcode == WS_OPCODE_BIN) {
                            aurora_ws_write_frame(client_sock, tls_handle, opcode, payload, plen);
                        }
                        aurora_free(payload);
                        payload = NULL;
                    }
                }
            }
            break;
        }

        char* body_start = strstr(raw, "\r\n\r\n");
        if (body_start) {
            int header_end = (int)(body_start - raw) + 4;
            int content_length = 0;
            char* cl = strstr(raw, "Content-Length:");
            if (!cl) cl = strstr(raw, "content-length:");
            if (cl) {
                cl += 15;
                while (*cl == ' ') cl++;
                content_length = atoi(cl);
            }
            int needed = header_end + content_length;
            if (needed > (int)sizeof(raw) - 1)
                needed = (int)sizeof(raw) - 1;
            while (n < needed) {
                int r = recv_data(raw + n, needed - n);
                if (r <= 0) break;
                n += r;
                raw[n] = '\0';
            }
        }
        int ka = has_keep_alive(raw);
        int gzip_accepted = accepts_gzip(raw);
        AuroraHttpRequest* req = aurora_http_parse_request(raw);
        AuroraHttpResponse* res = aurora_http_response_new();
        res->tls_handle = tls_handle;
        aurora_http_response_set_header(res, "Content-Type", "text/plain");
        if (ka) {
            aurora_http_response_set_header(res, "Connection", "keep-alive");
            aurora_http_response_set_header(res, "Keep-Alive", "timeout=5, max=100");
        }
        if (req) {
            int mw_result = aurora_middleware_run_chain(
                srv->middleware_handlers, srv->middleware_count, req, res);
            if (mw_result == 0) {
                if (!aurora_server_serve_static(req, res, srv))
                    aurora_route_dispatch(router, req, res);
            }
        } else {
            aurora_http_response_set_status(res, 400, "Bad Request");
            aurora_http_response_set_body(res, "Bad Request");
        }
        if (gzip_accepted && res->body) {
            size_t orig_len = res->body_len > 0 ? res->body_len : strlen(res->body);
            if (orig_len > 512) {
                size_t gz_len = 0;
                unsigned char* gz = gzip_compress((const unsigned char*)res->body, orig_len, &gz_len);
                if (gz) {
                    free(res->body);
                    res->body = (char*)gz;
                    res->body_len = gz_len;
                    aurora_http_response_set_header(res, "Content-Encoding", "gzip");
                    char cl_buf[32];
                    snprintf(cl_buf, sizeof(cl_buf), "%zu", gz_len);
                    aurora_http_response_set_header(res, "Content-Length", cl_buf);
                }
            }
        }
        aurora_http_response_send(res, client_sock);
        aurora_http_request_free(req);
        aurora_http_response_free(res);
        if (!ka) break;
    }
    if (tls_handle > 0)
        aurora_tls_close(tls_handle);
    else {
#ifdef _WIN32
        closesocket(client);
#else
        close(client);
#endif
    }
}

/* ── Create a TLS handle for an accepted socket, or return 0 ── */
static int64_t try_tls_accept(int64_t client_sock, AuroraServer* srv) {
    if (!srv || !srv->tls_ctx) return 0;
    int64_t tls_handle = aurora_tls_accept(client_sock, srv->tls_ctx);
    if (tls_handle < 0) {
        fprintf(stderr, "[server] TLS accept failed\n");
        return -1;
    }
    return tls_handle;
}

/* ── Server accept + handle (single-thread, backward compat) ── */
void aurora_server_accept_and_handle(AuroraServer* srv, AuroraRouter* router) {
    if (!srv || !srv->handle || !srv->running) return;
#ifdef _WIN32
    SOCKET listen_sock = (SOCKET)(intptr_t)srv->handle;
    struct sockaddr_in client_addr;
    int addr_len = sizeof(client_addr);
    SOCKET client = accept(listen_sock, (struct sockaddr*)&client_addr, &addr_len);
    if (client == INVALID_SOCKET) return;
    int64_t client_sock = (int64_t)(intptr_t)client;
    int64_t tls = try_tls_accept(client_sock, srv);
    if (tls >= 0) handle_client(client_sock, tls, srv, router);
    else closesocket(client);
#else
    int listen_sock = (int)(intptr_t)srv->handle;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client = accept(listen_sock, (struct sockaddr*)&client_addr, &addr_len);
    if (client < 0) return;
    int64_t tls = try_tls_accept((int64_t)client, srv);
    if (tls >= 0) handle_client((int64_t)client, tls, srv, router);
    else close(client);
#endif
}

/* ── Thread-per-connection accept loop ── */
static std::atomic<int> g_active_connections{0};
static std::atomic<int64_t> g_total_connections{0};

void aurora_server_accept_loop(AuroraServer* srv, AuroraRouter* router) {
    if (!srv || !srv->handle || !srv->running) return;
    printf("[server] accept loop (thread-per-connection) started on port %d\n", srv->port);
    if (srv->tls_ctx)
        printf("[server] TLS enabled\n");
    while (srv->running) {
#ifdef _WIN32
        SOCKET listen_sock = (SOCKET)(intptr_t)srv->handle;
        struct sockaddr_in client_addr;
        int addr_len = sizeof(client_addr);
        SOCKET client = accept(listen_sock, (struct sockaddr*)&client_addr, &addr_len);
        if (client == INVALID_SOCKET) {
            if (!srv->running) break;
            continue;
        }
#else
        int listen_sock = (int)(intptr_t)srv->handle;
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client = accept(listen_sock, (struct sockaddr*)&client_addr, &addr_len);
        if (client < 0) {
            if (!srv->running) break;
            continue;
        }
#endif
        int64_t client_sock = (int64_t)(intptr_t)client;
        int64_t tls = try_tls_accept(client_sock, srv);
        if (tls < 0) { /* TLS accept failed — close connection */
#ifdef _WIN32
            closesocket(client);
#else
            close(client);
#endif
            continue;
        }
        g_active_connections.fetch_add(1);
        g_total_connections.fetch_add(1);
        std::thread([client_sock, tls, srv, router]() {
            handle_client(client_sock, tls, srv, router);
            g_active_connections.fetch_sub(1);
        }).detach();
    }
    printf("[server] accept loop ended (total connections: %lld)\n", (long long)g_total_connections.load());
}

/* ════════════════════════════════════════════════════════════
   Static file serving
   ════════════════════════════════════════════════════════════ */

static const char* mime_type(const char* path) {
    const char* ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html; charset=utf-8";
    if (strcmp(ext, ".css") == 0) return "text/css; charset=utf-8";
    if (strcmp(ext, ".js") == 0) return "application/javascript; charset=utf-8";
    if (strcmp(ext, ".json") == 0) return "application/json";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".svg") == 0) return "image/svg+xml";
    if (strcmp(ext, ".ico") == 0) return "image/x-icon";
    if (strcmp(ext, ".woff2") == 0) return "font/woff2";
    if (strcmp(ext, ".woff") == 0) return "font/woff";
    if (strcmp(ext, ".ttf") == 0) return "font/ttf";
    if (strcmp(ext, ".otf") == 0) return "font/otf";
    if (strcmp(ext, ".wasm") == 0) return "application/wasm";
    if (strcmp(ext, ".txt") == 0) return "text/plain; charset=utf-8";
    if (strcmp(ext, ".xml") == 0) return "application/xml";
    if (strcmp(ext, ".yaml") == 0 || strcmp(ext, ".yml") == 0) return "text/yaml";
    if (strcmp(ext, ".md") == 0) return "text/markdown; charset=utf-8";
    if (strcmp(ext, ".pdf") == 0) return "application/pdf";
    if (strcmp(ext, ".zip") == 0) return "application/zip";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    if (strcmp(ext, ".mp4") == 0) return "video/mp4";
    if (strcmp(ext, ".webp") == 0) return "image/webp";
    return "application/octet-stream";
}

void aurora_server_static(AuroraServer* srv, const char* prefix, const char* directory) {
    if (!srv || !prefix || !directory) return;
    if (srv->static_count >= srv->static_cap) {
        int new_cap = srv->static_cap ? srv->static_cap * 2 : 4;
        srv->static_routes = (AuroraStaticRoute*)aurora_safe_realloc(
            srv->static_routes, (size_t)new_cap * sizeof(AuroraStaticRoute));
        srv->static_cap = new_cap;
    }
    AuroraStaticRoute* r = &srv->static_routes[srv->static_count++];
    r->prefix = strdup(prefix);
    r->directory = strdup(directory);
    if (r->prefix[strlen(r->prefix) - 1] == '/')
        r->prefix[strlen(r->prefix) - 1] = '\0';
    printf("[server] static route: %s -> %s\n", r->prefix, r->directory);
}

int aurora_server_serve_static(AuroraHttpRequest* req, AuroraHttpResponse* res, AuroraServer* srv) {
    if (!req || !res || !srv || !req->path_without_query) return 0;
    for (int i = 0; i < srv->static_count; i++) {
        const char* prefix = srv->static_routes[i].prefix;
        const char* dir = srv->static_routes[i].directory;
        const char* path = req->path_without_query;
        int plen = (int)strlen(prefix);
        if (strncmp(path, prefix, plen) != 0) continue;
        if (path[plen] != '/' && path[plen] != '\0') continue;
        const char* rel = path[plen] == '/' ? path + plen + 1 : "";
        if (!*rel) rel = "index.html";
        /* Build full path */
        char full[1024];
        snprintf(full, sizeof(full), "%s/%s", dir, rel);
        /* Prevent path traversal */
        if (strstr(rel, "..")) {
            aurora_http_response_set_status(res, 403, "Forbidden");
            aurora_http_response_set_body(res, "Forbidden");
            return 1;
        }
        /* Check file existence */
        struct stat st;
        if (stat(full, &st) != 0 || (st.st_mode & S_IFDIR)) {
            continue; /* not found or directory, try next static route */
        }
        /* Read and serve */
        FILE* fp = fopen(full, "rb");
        if (!fp) continue;
        fseek(fp, 0, SEEK_END);
        long fsize = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        char* buf = (char*)aurora_alloc((size_t)fsize + 1);
        size_t nread = fread(buf, 1, (size_t)fsize, fp);
        fclose(fp);
        aurora_http_response_set_status(res, 200, "OK");
        aurora_http_response_set_content_type(res, mime_type(full));
        aurora_http_response_set_header(res, "Cache-Control", "no-cache");
        aurora_http_response_set_body_n(res, buf, nread);
        aurora_free(buf);
        return 1;
    }
    return 0;
}

/* ════════════════════════════════════════════════════════════
   File watcher (polling)
   ════════════════════════════════════════════════════════════ */

#ifndef _WIN32
#include <dirent.h>
#endif

static int64_t file_mtime(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
#ifdef _WIN32
    return (int64_t)st.st_mtime * 1000;
#elif defined(__APPLE__)
    return (int64_t)st.st_mtimespec.tv_sec * 1000 + st.st_mtimespec.tv_nsec / 1000000;
#else
    return (int64_t)st.st_mtim.tv_sec * 1000 + st.st_mtim.tv_nsec / 1000000;
#endif
}

static void scan_directory(const char* dir, std::vector<std::string>& out) {
#ifdef _WIN32
    std::string pattern = std::string(dir) + "\\*";
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0) continue;
        std::string full = std::string(dir) + "\\" + ffd.cFileName;
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            scan_directory(full.c_str(), out);
        } else {
            out.push_back(full);
        }
    } while (FindNextFileA(hFind, &ffd) != 0);
    FindClose(hFind);
#else
    DIR* d = opendir(dir);
    if (!d) return;
    struct dirent* de;
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        std::string full = std::string(dir) + "/" + de->d_name;
        struct stat st;
        if (stat(full.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            scan_directory(full.c_str(), out);
        } else {
            out.push_back(full);
        }
    }
    closedir(d);
#endif
}

AuroraFileWatchState* aurora_fs_watch_init(const char* dir) {
    AuroraFileWatchState* state = (AuroraFileWatchState*)calloc(1, sizeof(AuroraFileWatchState));
    if (!state) return nullptr;
    std::vector<std::string> files;
    scan_directory(dir, files);
    state->count = (int)files.size();
    state->cap = state->count > 0 ? state->count : 8;
    state->files = (char**)calloc((size_t)state->cap, sizeof(char*));
    state->mtimes = (int64_t*)calloc((size_t)state->cap, sizeof(int64_t));
    for (int i = 0; i < state->count; i++) {
        state->files[i] = strdup(files[i].c_str());
        state->mtimes[i] = file_mtime(files[i].c_str());
    }
    printf("[watch] watching %d files in %s\n", state->count, dir);
    return state;
}

int aurora_fs_watch_poll(AuroraFileWatchState* state) {
    if (!state || state->count == 0) return 0;
    std::string dir;
    if (state->files[0]) {
        dir = state->files[0];
        auto pos = dir.find_last_of("/\\");
        if (pos != std::string::npos) dir = dir.substr(0, pos);
    }
    if (dir.empty()) return 0;
    /* Check modification times first (fast path) */
    for (int i = 0; i < state->count; i++) {
        int64_t mtime = file_mtime(state->files[i]);
        if (mtime != state->mtimes[i]) {
            state->mtimes[i] = mtime;
            return i + 1;
        }
    }
    /* Check for added/removed files (slow path — only on no mtime change) */
    std::vector<std::string> current;
    scan_directory(dir.c_str(), current);
    if (current.size() != (size_t)state->count) {
        aurora_fs_watch_free(state);
        AuroraFileWatchState* ns = aurora_fs_watch_init(dir.c_str());
        if (ns) {
            *state = *ns;
            free(ns);
        }
        return 1;
    }
    return 0;
}

void aurora_fs_watch_free(AuroraFileWatchState* state) {
    if (!state) return;
    for (int i = 0; i < state->count; i++)
        free(state->files[i]);
    free(state->files);
    free(state->mtimes);
    state->files = nullptr;
    state->mtimes = nullptr;
    state->count = 0;
    state->cap = 0;
}

/* ════════════════════════════════════════════════════════════
   Dev server (built-in hot reload)
   ════════════════════════════════════════════════════════════ */

static const char* LIVE_RELOAD_SCRIPT =
    "<script>\n"
    "(function(){var ws=new WebSocket('ws://'+location.host+'/__aurora_livereload');\n"
    "ws.onmessage=function(e){if(e.data==='reload')location.reload();};\n"
    "ws.onclose=function(){setTimeout(function(){location.reload();},1000);};\n"
    "})();\n"
    "</script>\n";

static void inject_live_reload(char* body, size_t body_len) {
    if (!body) return;
    const char* insert_before = "</body>";
    char* pos = strstr(body, insert_before);
    if (!pos) {
        insert_before = "</html>";
        pos = strstr(body, insert_before);
    }
    if (!pos) return;
    size_t before_len = (size_t)(pos - body);
    size_t script_len = strlen(LIVE_RELOAD_SCRIPT);
    size_t after_len = strlen(pos);
    size_t new_len = before_len + script_len + after_len + 1;
    if (new_len > body_len) return;
    memmove(pos + script_len, pos, after_len + 1);
    memcpy(pos, LIVE_RELOAD_SCRIPT, script_len);
}

static void dev_server_handle_request(AuroraHttpRequest* req, AuroraHttpResponse* res,
                                       AuroraServer* srv) {
    /* Check for live-reload WebSocket upgrade */
    if (req->path_without_query && strcmp(req->path_without_query, "/__aurora_livereload") == 0) {
        /* Simple SSE-style keep-alive instead of full WebSocket */
        const char* sse = "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/event-stream\r\n"
                          "Cache-Control: no-cache\r\n"
                          "Connection: keep-alive\r\n"
                          "Access-Control-Allow-Origin: *\r\n"
                          "\r\n"
                          "data: connected\n\n";
        /* Can't easily inject this — let the connection handler open a pipe. We handle
           this in the accept loop by not sending the response and keeping the socket open. */
    }
    /* Try static files first */
    if (aurora_server_serve_static(req, res, srv))
        return;
    /* 404 fallback */
    aurora_http_response_set_status(res, 404, "Not Found");
    aurora_http_response_set_content_type(res, "text/html; charset=utf-8");
    aurora_http_response_set_body(res,
        "<!DOCTYPE html><html><head><title>404</title></head>"
        "<body><h1>404 — Not Found</h1><p>Dev server could not find the requested file.</p>"
        "</body></html>");
}

/* ── Global router + route_register (used by codegen) ── */
static AuroraRouter* g_global_router = nullptr;
static AuroraServer* g_global_server = nullptr;

void aurora_route_register(const char* method, const char* path, void* handler) {
    if (!g_global_router) {
        g_global_router = aurora_router_new();
    }
    aurora_route_add(g_global_router, method ? method : "GET", path, handler);
}

void aurora_server_set_router(AuroraServer* srv, AuroraRouter* router) {
    g_global_server = srv;
    if (router) g_global_router = router;
}

void aurora_server_run(AuroraServer* srv) {
    if (!srv) return;
    if (!g_global_router) g_global_router = aurora_router_new();
    aurora_server_start(srv);
    if (srv->running) {
        printf("[server] running on port %d\n", srv->port);
        aurora_server_accept_loop(srv, g_global_router);
    }
}

void aurora_dev_server(int64_t port, const char* src_dir) {
    printf("[dev] starting dev server on port %lld, serving %s\n", (long long)port, src_dir);
    AuroraServer* srv = aurora_server_init(port);
    if (!srv) return;
    aurora_server_static(srv, "/", src_dir);
    aurora_server_start(srv);
    if (!srv->running) return;

    AuroraFileWatchState* watch = aurora_fs_watch_init(src_dir);
    if (!watch) return;

    /* Accept + handle loop with polling */
    while (srv->running) {
#ifdef _WIN32
        SOCKET listen_sock = (SOCKET)(intptr_t)srv->handle;
        fd_set readfds;
        struct timeval tv;
        FD_ZERO(&readfds);
        FD_SET(listen_sock, &readfds);
        tv.tv_sec = 0;
        tv.tv_usec = 200000; /* 200ms timeout for polling */
        if (select(0, &readfds, NULL, NULL, &tv) <= 0) {
            /* Check for file changes */
            if (aurora_fs_watch_poll(watch)) {
                printf("[dev] file change detected, clients will reload\n");
            }
            continue;
        }
#else
        int listen_sock = (int)(intptr_t)srv->handle;
        fd_set readfds;
        struct timeval tv;
        FD_ZERO(&readfds);
        FD_SET(listen_sock, &readfds);
        tv.tv_sec = 0;
        tv.tv_usec = 200000;
        if (select(listen_sock + 1, &readfds, NULL, NULL, &tv) <= 0) {
            if (aurora_fs_watch_poll(watch)) {
                printf("[dev] file change detected, clients will reload\n");
            }
            continue;
        }
#endif
        struct sockaddr_in client_addr;
        int addr_len = sizeof(client_addr);
#ifdef _WIN32
        SOCKET client = accept(listen_sock, (struct sockaddr*)&client_addr, &addr_len);
        if (client == INVALID_SOCKET) continue;
#else
        int client = accept(listen_sock, (struct sockaddr*)&client_addr, (socklen_t*)&addr_len);
        if (client < 0) continue;
#endif
        char raw[32768];
#ifdef _WIN32
        int n = recv(client, raw, sizeof(raw) - 1, 0);
#else
        int n = (int)recv(client, raw, sizeof(raw) - 1, 0);
#endif
        if (n > 0) {
            raw[n] = '\0';
            /* Check for live-reload event-stream request */
            if (strstr(raw, "/__aurora_livereload")) {
                /* Send SSE response and keep connection open; poll for changes */
                const char* sse_hdr = "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/event-stream\r\n"
                    "Cache-Control: no-cache\r\n"
                    "Connection: keep-alive\r\n"
                    "Access-Control-Allow-Origin: *\r\n\r\n"
                    "data: connected\n\n";
#ifdef _WIN32
                send(client, sse_hdr, (int)strlen(sse_hdr), 0);
#else
                send(client, sse_hdr, strlen(sse_hdr), MSG_NOSIGNAL);
#endif
                /* Poll for changes in a tight loop */
                int64_t last_change = 0;
                while (srv->running) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    int changed = aurora_fs_watch_poll(watch);
                    if (changed) {
                        last_change = now_ms();
                        const char* msg = "data: reload\n\n";
#ifdef _WIN32
                        send(client, msg, (int)strlen(msg), 0);
#else
                        send(client, msg, strlen(msg), MSG_NOSIGNAL);
#endif
                    }
                    if (last_change && now_ms() - last_change > 2000)
                        break;
                }
#ifdef _WIN32
                closesocket(client);
#else
                close(client);
#endif
                continue;
            }
            AuroraHttpRequest* req = aurora_http_parse_request(raw);
            AuroraHttpResponse* res = aurora_http_response_new();
            if (req) {
                dev_server_handle_request(req, res, srv);
            } else {
                aurora_http_response_set_status(res, 400, "Bad Request");
                aurora_http_response_set_body(res, "Bad Request");
            }
            /* Inject live-reload script into HTML responses */
            if (res->body && strstr(res->body, "<html") && strstr(res->body, "</body>")) {
                inject_live_reload(res->body, strlen(res->body) + 512);
                aurora_http_response_set_header(res, "Content-Length", "");
            }
            aurora_http_response_send(res, (int64_t)(intptr_t)client);
            aurora_http_request_free(req);
            aurora_http_response_free(res);
        }
#ifdef _WIN32
        closesocket(client);
#else
        close(client);
#endif
    }
    aurora_fs_watch_free(watch);
    aurora_server_stop(srv);
    aurora_server_clear_middleware(srv);
    for (int i = 0; i < srv->static_count; i++) {
        free(srv->static_routes[i].prefix);
        free(srv->static_routes[i].directory);
    }
    free(srv->static_routes);
    free(srv);
    printf("[dev] dev server stopped\n");
}

/* ── In-memory database engine ── */

static std::mutex g_db_mutex;

/* Represents a single row in a table (column name → value) */
using DBRow = std::unordered_map<std::string, std::string>;

/* Represents a table: ordered column names + rows */
struct DBTable {
    std::vector<std::string> columns;
    std::vector<DBRow> rows;
};

/* Global database: table name → table */
static std::unordered_map<std::string, DBTable> g_tables;

/* Result set returned by SELECT queries */
struct DBResult {
    std::vector<std::string> columns;
    std::vector<DBRow> rows;
    int affected_rows;
};

/* Helper: trim whitespace (C-string, no std::string to avoid LLD CRT issues) */
static std::string trim(const std::string& s) {
    if (s.empty()) return std::string();
    const char* cstr = s.c_str();
    const char* start = cstr;
    while (*start && (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n'))
        start++;
    if (!*start) return std::string();
    const char* end = start + strlen(start) - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
        end--;
    return std::string(start, (size_t)(end - start + 1));
}

/* Helper: split string by delimiter (uses C-string iteration, avoids stringstream) */
static std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> parts;
    const char* p = s.c_str();
    while (*p) {
        while (*p == delim) p++;
        if (!*p) break;
        const char* start = p;
        while (*p && *p != delim) p++;
        parts.push_back(trim(std::string(start, (size_t)(p - start))));
    }
    return parts;
}

/* Helper: split keeping quoted strings together */
static std::vector<std::string> split_sql_args(const std::string& s) {
    std::vector<std::string> parts;
    const char* p = s.c_str();
    std::string cur;
    bool in_quote = false;
    while (*p) {
        char c = *p;
        if (c == '\'') {
            in_quote = !in_quote;
            cur += c;
        } else if (c == ',' && !in_quote) {
            parts.push_back(trim(cur));
            cur.clear();
        } else {
            cur += c;
        }
        p++;
    }
    if (!cur.empty()) parts.push_back(trim(cur));
    return parts;
}

/* Parse and execute CREATE TABLE */
static int exec_create(const std::string& rest) {
    /* CREATE TABLE name (col1 type, col2 type, ...) */
    size_t paren = rest.find('(');
    if (paren == std::string::npos) return 0;
    std::string name = trim(rest.substr(0, paren));
    std::string col_part = rest.substr(paren + 1);
    size_t close = col_part.rfind(')');
    if (close != std::string::npos) col_part = col_part.substr(0, close);
    col_part = trim(col_part);
    if (name.empty()) return 0;
    /* Make table name case-insensitive */
    std::string table_name;
    for (char c : name) table_name += (char)tolower(c);
    DBTable tbl;
    auto col_defs = split(col_part, ',');
    for (auto& cd : col_defs) {
        auto parts = split(cd, ' ');
        if (!parts.empty()) tbl.columns.push_back(parts[0]);
    }
    std::lock_guard<std::mutex> lock(g_db_mutex);
    g_tables[table_name] = std::move(tbl);
    printf("[db] table '%s' created (%zu columns)\n", table_name.c_str(), tbl.columns.size());
    return 1;
}

/* Parse and execute INSERT INTO */
static int exec_insert(const std::string& rest) {
    /* INSERT INTO name VALUES (v1, v2, ...) */
    std::string r = trim(rest);
    std::string keyword = "into";
    size_t pos = r.find(keyword);
    if (pos == std::string::npos) return 0;
    r = trim(r.substr(pos + keyword.size()));
    pos = r.find("values");
    if (pos == std::string::npos) {
        pos = r.find("VALUES");
        if (pos == std::string::npos) return 0;
    }
    std::string table_name;
    for (char c : trim(r.substr(0, pos))) table_name += (char)tolower(c);
    std::string val_part = trim(r.substr(pos + 6)); /* skip "values" */
    if (val_part.front() == '(') val_part = val_part.substr(1);
    if (val_part.back() == ')') val_part = val_part.substr(0, val_part.size() - 1);
    auto raw_vals = split_sql_args(val_part);
    std::lock_guard<std::mutex> lock(g_db_mutex);
    auto it = g_tables.find(table_name);
    if (it == g_tables.end()) {
        printf("[db] error: table '%s' not found\n", table_name.c_str());
        return 0;
    }
    DBRow row;
    for (size_t i = 0; i < raw_vals.size() && i < it->second.columns.size(); i++) {
        std::string v = trim(raw_vals[i]);
        if (v.size() >= 2 && v.front() == '\'' && v.back() == '\'')
            v = v.substr(1, v.size() - 2);
        row[it->second.columns[i]] = v;
    }
    it->second.rows.push_back(std::move(row));
    printf("[db] inserted into '%s' (now %zu rows)\n", table_name.c_str(), it->second.rows.size());
    return 1;
}

/* Parse and execute SELECT */
static std::string exec_select(const std::string& rest) {
    /* SELECT cols FROM table WHERE cond */
    std::string r = trim(rest);
    size_t from_pos = r.find(" from ");
    if (from_pos == std::string::npos) from_pos = r.find(" FROM ");
    if (from_pos == std::string::npos) return "";
    std::string col_part = trim(r.substr(0, from_pos));
    std::string rest2 = trim(r.substr(from_pos + 5)); /* skip "from " */
    size_t where_pos = rest2.find(" where ");
    if (where_pos == std::string::npos) where_pos = rest2.find(" WHERE ");
    std::string table_part, where_part;
    if (where_pos != std::string::npos) {
        table_part = trim(rest2.substr(0, where_pos));
        where_part = trim(rest2.substr(where_pos + (rest2[where_pos + 1] == 'W' ? 7 : 6)));
    } else {
        table_part = trim(rest2);
    }
    std::string table_name;
    for (char c : table_part) table_name += (char)tolower(c);
    std::lock_guard<std::mutex> lock(g_db_mutex);
    auto it = g_tables.find(table_name);
    if (it == g_tables.end()) {
        printf("[db] error: table '%s' not found\n", table_name.c_str());
        return std::string();
    }
    bool select_all = (col_part == "*");
    std::vector<std::string> sel_cols;
    if (!select_all) {
        auto parts = split(col_part, ',');
        for (auto& p : parts) sel_cols.push_back(trim(p));
    }
    std::stringstream result;
    /* Header */
    if (select_all) {
        for (size_t i = 0; i < it->second.columns.size(); i++) {
            if (i > 0) result << "|";
            result << it->second.columns[i];
        }
    } else {
        for (size_t i = 0; i < sel_cols.size(); i++) {
            if (i > 0) result << "|";
            result << sel_cols[i];
        }
    }
    result << "\n";
    int match_count = 0;
    for (auto& row : it->second.rows) {
        bool match = true;
        if (!where_part.empty()) {
            /* Simple col = 'val' or col = val */
            size_t eq_pos = where_part.find('=');
            if (eq_pos != std::string::npos) {
                std::string wcol = trim(where_part.substr(0, eq_pos));
                std::string wval = trim(where_part.substr(eq_pos + 1));
                if (wval.size() >= 2 && wval.front() == '\'' && wval.back() == '\'')
                    wval = wval.substr(1, wval.size() - 2);
                auto it2 = row.find(wcol);
                if (it2 == row.end() || it2->second != wval) match = false;
            }
        }
        if (!match) continue;
        match_count++;
        if (select_all) {
            for (size_t i = 0; i < it->second.columns.size(); i++) {
                if (i > 0) result << "|";
                auto it2 = row.find(it->second.columns[i]);
                result << (it2 != row.end() ? it2->second : "NULL");
            }
        } else {
            for (size_t i = 0; i < sel_cols.size(); i++) {
                if (i > 0) result << "|";
                auto it2 = row.find(sel_cols[i]);
                result << (it2 != row.end() ? it2->second : "NULL");
            }
        }
        result << "\n";
    }
    printf("[db] selected %d rows from '%s'\n", match_count, table_name.c_str());
    return result.str();
}

/* Parse and execute DROP TABLE */
static int exec_drop(const std::string& rest) {
    std::string name;
    for (char c : trim(rest)) name += (char)tolower(c);
    std::lock_guard<std::mutex> lock(g_db_mutex);
    auto it = g_tables.find(name);
    if (it == g_tables.end()) {
        printf("[db] error: table '%s' not found\n", name.c_str());
        return 0;
    }
    g_tables.erase(it);
    printf("[db] table '%s' dropped\n", name.c_str());
    return 1;
}

/* Main SQL dispatcher */
static std::string exec_sql(const char* query) {
    if (!query) return std::string();
    std::string q = trim(query);
    if (q.empty()) return std::string();
    /* Normalize: uppercase the keyword */
    std::string upper_q = q;
    for (char& c : upper_q) c = (char)toupper(c);
    if (upper_q.substr(0, 6) == "CREATE") {
        /* CREATE TABLE ... */
        std::string rest = trim(q.substr(6));
        size_t pos = rest.find("TABLE");
        if (pos == std::string::npos) pos = rest.find("table");
        if (pos != std::string::npos)
            rest = trim(rest.substr(pos + 5));
        exec_create(rest);
        return std::string();
    }
    if (upper_q.substr(0, 6) == "INSERT") {
        exec_insert(trim(q.substr(6)));
        return std::string();
    }
    if (upper_q.substr(0, 6) == "SELECT") {
        return exec_select(trim(q.substr(6)));
    }
    if (upper_q.substr(0, 4) == "DROP") {
        std::string rest = trim(q.substr(4));
        size_t pos = rest.find("TABLE");
        if (pos == std::string::npos) pos = rest.find("table");
        if (pos != std::string::npos)
            rest = trim(rest.substr(pos + 5));
        exec_drop(rest);
        return std::string();
    }
    printf("[db] unsupported query: %s\n", q.c_str());
    return std::string();
}

AuroraDB* aurora_db_connect(const char* conn_str) {
    if (!conn_str) return nullptr;
    AuroraDB* db = (AuroraDB*)calloc(1, sizeof(AuroraDB));
    db->conn_str = strdup(conn_str);
    db->connected = 1;
    db->handle = nullptr;
    printf("[database] connected: %s\n", conn_str);
    return db;
}

void* aurora_db_query(AuroraDB* db, const char* query) {
    if (!db || !db->connected || !query) return nullptr;
    printf("[database] query: %s\n", query);
    std::string result = exec_sql(query);
    return (void*)strdup(result.c_str());
}

void aurora_db_query_free(void* result) {
    if (result)
        free(result);
}

void aurora_db_close(AuroraDB* db) {
    if (!db) return;
    db->connected = 0;
    free(db->conn_str);
    printf("[database] closed\n");
    free(db);
}

/* ── Cache mutex for thread safety ── */
static std::mutex g_cache_mutex;

/* ── Cache (in-memory with optional TTL) ── */
AuroraCache* aurora_cache_init() {
    AuroraCache* c = (AuroraCache*)calloc(1, sizeof(AuroraCache));
    if (!c) return nullptr;
    c->cap = 16;
    c->count = 0;
    c->keys = (char**)calloc((size_t)c->cap, sizeof(char*));
    c->values = (char**)calloc((size_t)c->cap, sizeof(char*));
    c->expires_at = (int64_t*)calloc((size_t)c->cap, sizeof(int64_t));
    return c;
}

void aurora_cache_destroy(AuroraCache* cache) {
    if (!cache) return;
    for (int i = 0; i < cache->count; i++) {
        free(cache->keys[i]);
        free(cache->values[i]);
    }
    free(cache->keys);
    free(cache->values);
    free(cache->expires_at);
    free(cache);
}

static int cache_find(AuroraCache* cache, const char* key) {
    for (int i = 0; i < cache->count; i++) {
        if (strcmp(cache->keys[i], key) == 0) return i;
    }
    return -1;
}

void aurora_cache_set(AuroraCache* cache, const char* key, const char* val) {
    aurora_cache_set_with_ttl(cache, key, val, 0);
}

void aurora_cache_set_with_ttl(AuroraCache* cache, const char* key, const char* val, int64_t ttl_ms) {
    if (!cache || !key || !val) return;
    std::lock_guard<std::mutex> lock(g_cache_mutex);
    int idx = cache_find(cache, key);
    if (idx >= 0) {
        free(cache->values[idx]);
        cache->values[idx] = strdup(val);
        cache->expires_at[idx] = ttl_ms > 0 ? now_ms() + ttl_ms : 0;
        return;
    }
    if (cache->count >= cache->cap) {
        int new_cap = cache->cap * 2;
        char** new_keys = (char**)aurora_safe_realloc(cache->keys, (size_t)new_cap * sizeof(char*));
        if (!new_keys) return;
        char** new_vals = (char**)aurora_safe_realloc(cache->values, (size_t)new_cap * sizeof(char*));
        if (!new_vals) { free(new_keys); return; }
        int64_t* new_exp = (int64_t*)aurora_safe_realloc(cache->expires_at, (size_t)new_cap * sizeof(int64_t));
        if (!new_exp) { free(new_keys); free(new_vals); return; }
        cache->keys = new_keys;
        cache->values = new_vals;
        cache->expires_at = new_exp;
        cache->cap = new_cap;
    }
    cache->keys[cache->count] = strdup(key);
    cache->values[cache->count] = strdup(val);
    cache->expires_at[cache->count] = ttl_ms > 0 ? now_ms() + ttl_ms : 0;
    cache->count++;
}

char* aurora_cache_get(AuroraCache* cache, const char* key) {
    if (!cache || !key) return nullptr;
    std::lock_guard<std::mutex> lock(g_cache_mutex);
    int idx = cache_find(cache, key);
    if (idx < 0) return nullptr;
    /* Check expiry */
    if (cache->expires_at[idx] > 0 && now_ms() > cache->expires_at[idx]) {
        /* Expired — remove and return null */
        free(cache->keys[idx]);
        free(cache->values[idx]);
        for (int j = idx; j < cache->count - 1; j++) {
            cache->keys[j] = cache->keys[j + 1];
            cache->values[j] = cache->values[j + 1];
            cache->expires_at[j] = cache->expires_at[j + 1];
        }
        cache->count--;
        return nullptr;
    }
    return cache->values[idx];
}

int aurora_cache_has(AuroraCache* cache, const char* key) {
    return aurora_cache_get(cache, key) != nullptr ? 1 : 0;
}

void aurora_cache_delete(AuroraCache* cache, const char* key) {
    if (!cache || !key) return;
    std::lock_guard<std::mutex> lock(g_cache_mutex);
    int idx = cache_find(cache, key);
    if (idx < 0) return;
    free(cache->keys[idx]);
    free(cache->values[idx]);
    for (int j = idx; j < cache->count - 1; j++) {
        cache->keys[j] = cache->keys[j + 1];
        cache->values[j] = cache->values[j + 1];
        cache->expires_at[j] = cache->expires_at[j + 1];
    }
    cache->count--;
}

void aurora_cache_clear(AuroraCache* cache) {
    if (!cache) return;
    std::lock_guard<std::mutex> lock(g_cache_mutex);
    for (int i = 0; i < cache->count; i++) {
        free(cache->keys[i]);
        free(cache->values[i]);
    }
    cache->count = 0;
}

void aurora_cache_clean_expired(AuroraCache* cache) {
    if (!cache) return;
    std::lock_guard<std::mutex> lock(g_cache_mutex);
    int64_t now = now_ms();
    for (int i = cache->count - 1; i >= 0; i--) {
        if (cache->expires_at[i] > 0 && now > cache->expires_at[i]) {
            free(cache->keys[i]);
            free(cache->values[i]);
            for (int j = i; j < cache->count - 1; j++) {
                cache->keys[j] = cache->keys[j + 1];
                cache->values[j] = cache->values[j + 1];
                cache->expires_at[j] = cache->expires_at[j + 1];
            }
            cache->count--;
        }
    }
}

/* ── Session ── */
static int session_counter = 0;
static std::mutex session_mutex;

AuroraSession* aurora_session_create() {
    AuroraSession* sess = (AuroraSession*)calloc(1, sizeof(AuroraSession));
    char id[64];
    {
        std::lock_guard<std::mutex> lock(session_mutex);
        snprintf(id, sizeof(id), "sess_%d_%lld", ++session_counter, (long long)time(nullptr));
    }
    sess->session_id = strdup(id);
    sess->data = aurora_cache_init();
    sess->created_at = now_ms();
    sess->ttl_ms = 0;
    printf("[session] created: %s\n", sess->session_id);
    return sess;
}

void aurora_session_destroy(AuroraSession* sess) {
    if (!sess) return;
    free(sess->session_id);
    aurora_cache_destroy(sess->data);
    free(sess);
}

void aurora_session_set_ttl(AuroraSession* sess, int64_t ttl_ms) {
    if (!sess) return;
    sess->ttl_ms = ttl_ms;
}

int aurora_session_is_expired(AuroraSession* sess) {
    if (!sess || sess->ttl_ms <= 0) return 0;
    return (now_ms() - sess->created_at) > sess->ttl_ms;
}

int64_t aurora_session_age_ms(AuroraSession* sess) {
    if (!sess) return 0;
    return now_ms() - sess->created_at;
}

/* ── Constant-time string comparison ── */
static int aurora_const_time_cmp(const char* a, const char* b) {
    if (!a || !b) return -1;
    size_t lena = strlen(a), lenb = strlen(b);
    if (lena != lenb) return -1;
    int diff = 0;
    for (size_t i = 0; i < lena; i++) diff |= (unsigned char)a[i] ^ (unsigned char)b[i];
    return diff;
}

/* ════════════════════════════════════════════════════════════
   Gzip compression (Windows Compress API → gzip format)
   ════════════════════════════════════════════════════════════ */

static unsigned int gzip_crc32(const unsigned char* buf, size_t len) {
    static unsigned int table[256];
    static int init = 0;
    if (!init) {
        for (unsigned int i = 0; i < 256; i++) {
            unsigned int crc = i;
            for (int j = 0; j < 8; j++)
                crc = (crc >> 1) ^ (crc & 1 ? 0xEDB88320 : 0);
            table[i] = crc;
        }
        init = 1;
    }
    unsigned int crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++)
        crc = table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFF;
}

/* Compress data to gzip using miniz. Returns malloc'd buffer, sets out_len. NULL on failure. */
static unsigned char* gzip_compress(const unsigned char* input, size_t input_len, size_t* out_len) {
    *out_len = 0;
    if (!input || input_len == 0) return nullptr;

    size_t max_out = input_len + (input_len / 100) + 128;
    std::vector<unsigned char> deflate_buf(max_out);

    size_t deflate_len = tdefl_compress_mem_to_mem(
        deflate_buf.data(), max_out,
        input, input_len,
        128); /* TDEFL_DEFAULT_MAX_PROBES */
    if (deflate_len == 0) {
        fprintf(stderr, "[gzip] tdefl_compress_mem_to_mem failed\n");
        return nullptr;
    }

    /* Build gzip: 10-byte header + raw DEFLATE + 4-byte CRC32 + 4-byte ISIZE */
    *out_len = 10 + deflate_len + 8;
    unsigned char* gz = (unsigned char*)malloc(*out_len);
    if (!gz) return nullptr;

    gz[0] = 0x1F; gz[1] = 0x8B;       /* ID1, ID2 */
    gz[2] = 0x08;                       /* CM = deflate */
    gz[3] = 0;                          /* FLG */
    uint32_t mtime = (uint32_t)time(NULL);
    gz[4] = (unsigned char)(mtime); gz[5] = (unsigned char)(mtime >> 8);
    gz[6] = (unsigned char)(mtime >> 16); gz[7] = (unsigned char)(mtime >> 24);
    gz[8] = 0;                          /* XFL */
    gz[9] = 0xFF;                       /* OS = unknown */

    memcpy(gz + 10, deflate_buf.data(), deflate_len);

    uint32_t crc = gzip_crc32(input, input_len);
    size_t off = 10 + deflate_len;
    gz[off]     = (unsigned char)(crc);
    gz[off + 1] = (unsigned char)(crc >> 8);
    gz[off + 2] = (unsigned char)(crc >> 16);
    gz[off + 3] = (unsigned char)(crc >> 24);

    uint32_t isize = (uint32_t)input_len;
    gz[off + 4] = (unsigned char)(isize);
    gz[off + 5] = (unsigned char)(isize >> 8);
    gz[off + 6] = (unsigned char)(isize >> 16);
    gz[off + 7] = (unsigned char)(isize >> 24);

    return gz;
}

/* ── Auth ── */
int aurora_auth_login(const char* user, const char* pass) {
    if (!user || !pass) return 0;
    printf("[auth] login attempt: %s\n", user);
    const char* env_user = getenv("AURORA_ADMIN_USER");
    const char* env_pass = getenv("AURORA_ADMIN_PASS");
    if (env_user && env_pass &&
        aurora_const_time_cmp(user, env_user) == 0 && aurora_const_time_cmp(pass, env_pass) == 0)
        return 1;
    return 0;
}

/* Simple HMAC-like token: base64(payload):base64(HMAC) with XOR-based signing */
static const char hex_chars[] = "0123456789abcdef";

static char* to_hex(const unsigned char* data, size_t len) {
    char* out = (char*)malloc(len * 2 + 1);
    for (size_t i = 0; i < len; i++) {
        out[i * 2] = hex_chars[(data[i] >> 4) & 0x0F];
        out[i * 2 + 1] = hex_chars[data[i] & 0x0F];
    }
    out[len * 2] = '\0';
    return out;
}

static void compute_hmac(const char* payload, const char* secret,
                         unsigned char* out, size_t out_len) {
    size_t plen = strlen(payload);
    size_t slen = strlen(secret);
    size_t block_size = 64;
    unsigned char key_block[64] = {0};
    for (size_t i = 0; i < slen && i < block_size; i++)
        key_block[i] = (unsigned char)secret[i];

    unsigned char ipad[64], opad[64];
    for (size_t i = 0; i < block_size; i++) {
        ipad[i] = key_block[i] ^ 0x36;
        opad[i] = key_block[i] ^ 0x5C;
    }

    /* Inner hash: SHA256-like but simplified for portability */
    /* We use a simple XOR+rotate scheme that produces consistent output */
    unsigned char inner[32] = {0};
    for (size_t i = 0; i < block_size; i++)
        inner[i % 32] ^= ipad[i];
    for (size_t i = 0; i < plen; i++)
        inner[i % 32] ^= (unsigned char)payload[i];
    for (int r = 0; r < 3; r++) {
        for (size_t i = 0; i < 32; i++)
            inner[i] = (inner[i] << 1) | (inner[i] >> 7);
        inner[0] ^= (unsigned char)r;
    }

    unsigned char hmac[32] = {0};
    for (size_t i = 0; i < block_size; i++)
        hmac[i % 32] ^= opad[i];
    for (size_t i = 0; i < 32; i++)
        hmac[i] ^= inner[i];
    for (int r = 0; r < 3; r++) {
        for (size_t i = 0; i < 32; i++)
            hmac[i] = (hmac[i] << 1) | (hmac[i] >> 7);
        hmac[0] ^= (unsigned char)(r + 3);
    }

    size_t copy_len = out_len < 32 ? out_len : 32;
    memcpy(out, hmac, copy_len);
}

const char* aurora_auth_generate_token(const char* payload, const char* secret) {
    if (!payload || !secret) return nullptr;
    size_t plen = strlen(payload);
    char* payload_hex = to_hex((const unsigned char*)payload, plen);

    unsigned char hmac[16];
    compute_hmac(payload, secret, hmac, 16);
    char* sig_hex = to_hex(hmac, 16);

    size_t total_len = strlen(payload_hex) + 1 + strlen(sig_hex) + 1;
    char* token = (char*)malloc(total_len);
    snprintf(token, total_len, "%s:%s", payload_hex, sig_hex);
    free(payload_hex);
    free(sig_hex);

    printf("[auth] token generated for payload: %s\n", payload);
    return token;
}

int aurora_auth_verify_token(const char* token, const char* secret, char** out_payload) {
    if (!token || !secret) return 0;
    if (out_payload) *out_payload = nullptr;

    const char* colon = strchr(token, ':');
    if (!colon) return 0;

    size_t payload_hex_len = (size_t)(colon - token);
    char* payload_hex = (char*)malloc(payload_hex_len + 1);
    memcpy(payload_hex, token, payload_hex_len);
    payload_hex[payload_hex_len] = '\0';

    const char* sig_hex = colon + 1;

    /* Decode payload from hex */
    size_t plen = payload_hex_len / 2;
    char* payload = (char*)malloc(plen + 1);
    for (size_t i = 0; i < plen; i++) {
        unsigned char byte = 0;
        if (payload_hex[i * 2] >= '0' && payload_hex[i * 2] <= '9')
            byte = (unsigned char)(payload_hex[i * 2] - '0') << 4;
        else if (payload_hex[i * 2] >= 'a' && payload_hex[i * 2] <= 'f')
            byte = (unsigned char)(payload_hex[i * 2] - 'a' + 10) << 4;
        if (payload_hex[i * 2 + 1] >= '0' && payload_hex[i * 2 + 1] <= '9')
            byte |= (unsigned char)(payload_hex[i * 2 + 1] - '0');
        else if (payload_hex[i * 2 + 1] >= 'a' && payload_hex[i * 2 + 1] <= 'f')
            byte |= (unsigned char)(payload_hex[i * 2 + 1] - 'a' + 10);
        payload[i] = (char)byte;
    }
    payload[plen] = '\0';

    /* Compute expected signature */
    unsigned char expected[16];
    compute_hmac(payload, secret, expected, 16);

    /* Compare signature hex */
    char* expected_hex = to_hex(expected, 16);
    int match = (strcmp(sig_hex, expected_hex) == 0) ? 1 : 0;
    free(expected_hex);

    if (match && out_payload) {
        *out_payload = payload;
    } else {
        free(payload);
    }
    free(payload_hex);
    return match;
}

const char* aurora_auth_hash_password(const char* password) {
    if (!password) return nullptr;
    size_t plen = strlen(password);
    unsigned char hash[16];
    for (size_t i = 0; i < plen; i++)
        hash[i % 16] ^= (unsigned char)password[i];
    hash[0] ^= (unsigned char)plen;
    for (int r = 0; r < 5; r++) {
        for (size_t i = 0; i < 16; i++)
            hash[i] = (hash[i] << 1) | (hash[i] >> 7);
        hash[r % 16] ^= 0xA5;
    }
    return to_hex(hash, 16);
}

/* ════════════════════════════════════════════════════════════
   In-memory Todo Store (persistent across requests)
   ════════════════════════════════════════════════════════════ */
static std::mutex g_todo_mutex;
static int64_t g_todo_next_id = 1;
struct TodoItem {
    int64_t id;
    std::string title;
    bool done;
};
static std::vector<TodoItem> g_todos;

const char* aurora_todo_list() {
    std::lock_guard<std::mutex> lock(g_todo_mutex);
    std::string result = "[";
    for (size_t i = 0; i < g_todos.size(); i++) {
        if (i > 0) result += ",";
        result += "{\"id\":" + std::to_string(g_todos[i].id) +
                  ",\"title\":\"" + g_todos[i].title + "\"" +
                  ",\"done\":" + (g_todos[i].done ? "true" : "false") + "}";
    }
    result += "]";
    return strdup(result.c_str());
}

const char* aurora_todo_create(const char* body) {
    if (!body) return strdup("");
    /* Parse "title" from JSON body */
    std::string title;
    JsonValue* jv = aurora_json_parse(body);
    if (jv) {
        char* t = aurora_json_get_str(jv, "title");
        if (t) title = t;
        aurora_json_free(jv);
    }
    if (title.empty()) title = body;
    std::lock_guard<std::mutex> lock(g_todo_mutex);
    TodoItem t;
    t.id = g_todo_next_id++;
    t.title = title;
    t.done = false;
    g_todos.push_back(t);
    std::string result = "{\"id\":" + std::to_string(t.id) +
                         ",\"title\":\"" + t.title + "\"" +
                         ",\"done\":false}";
    return strdup(result.c_str());
}

const char* aurora_todo_get(const char* id_str) {
    if (!id_str || !*id_str) return strdup("");
    int64_t id = atoll(id_str);
    std::lock_guard<std::mutex> lock(g_todo_mutex);
    for (auto& t : g_todos) {
        if (t.id == id) {
            std::string result = "{\"id\":" + std::to_string(t.id) +
                                 ",\"title\":\"" + t.title + "\"" +
                                 ",\"done\":" + (t.done ? "true" : "false") + "}";
            return strdup(result.c_str());
        }
    }
    return strdup("");
}

const char* aurora_todo_update(const char* id_str, const char* body, int64_t done) {
    if (!id_str || !*id_str) return strdup("");
    int64_t id = atoll(id_str);
    /* Parse "title" from JSON body */
    std::string title;
    if (body && *body) {
        JsonValue* jv = aurora_json_parse(body);
        if (jv) {
            char* t = aurora_json_get_str(jv, "title");
            if (t) title = t;
            aurora_json_free(jv);
        }
        if (title.empty()) title = body;
    }
    std::lock_guard<std::mutex> lock(g_todo_mutex);
    for (auto& t : g_todos) {
        if (t.id == id) {
            if (!title.empty()) t.title = title;
            t.done = done != 0;
            std::string result = "{\"id\":" + std::to_string(t.id) +
                                 ",\"title\":\"" + t.title + "\"" +
                                 ",\"done\":" + (t.done ? "true" : "false") + "}";
            return strdup(result.c_str());
        }
    }
    return strdup("");
}

const char* aurora_todo_delete(const char* id_str) {
    if (!id_str || !*id_str) return strdup("");
    int64_t id = atoll(id_str);
    std::lock_guard<std::mutex> lock(g_todo_mutex);
    for (auto it = g_todos.begin(); it != g_todos.end(); ++it) {
        if (it->id == id) {
            std::string result = "{\"id\":" + std::to_string(it->id) +
                                 ",\"title\":\"" + it->title + "\"" +
                                 ",\"done\":" + (it->done ? "true" : "false") + "}";
            g_todos.erase(it);
            return strdup(result.c_str());
        }
    }
    return strdup("");
}

}
