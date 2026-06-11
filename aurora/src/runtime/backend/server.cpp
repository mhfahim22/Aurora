#include "runtime/backend.hpp"
#include "common/platform.hpp"
#include "runtime/memory.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <thread>
#include <chrono>

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

/* ── Server ── */
AuroraServer* aurora_server_init(int64_t port) {
    AuroraServer* srv = (AuroraServer*)calloc(1, sizeof(AuroraServer));
    srv->port = (int)port;
    srv->running = 0;
    srv->handle = nullptr;
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
    listen(sock, 5);
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
    listen(sock, 5);
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

/* ── HTTP Request Parser ── */
AuroraHttpRequest* aurora_http_parse_request(const char* raw) {
    if (!raw) return nullptr;
    AuroraHttpRequest* req = (AuroraHttpRequest*)calloc(1, sizeof(AuroraHttpRequest));

    char buf[8192];
    strncpy(buf, raw, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* line = buf;
    char* end = strstr(line, "\r\n");
    if (end) *end = '\0';

    char* save;
    char* method = AURORA_STRTOK(line, " ", &save);
    char* path = AURORA_STRTOK(nullptr, " ", &save);
    char* version = AURORA_STRTOK(nullptr, " ", &save);
    if (method) req->method = strdup(method);
    if (path) req->path = strdup(path);
    if (version) req->version = strdup(version);

    req->header_names = nullptr;
    req->header_values = nullptr;
    req->header_count = 0;

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
    return req;
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

void aurora_http_response_set_status(AuroraHttpResponse* res, int code, const char* text) {
    if (!res) return;
    res->status_code = code;
    free(res->status_text);
    res->status_text = text ? strdup(text) : strdup("Unknown");
}

void aurora_http_response_set_header(AuroraHttpResponse* res, const char* name, const char* value) {
    if (!res || !name || !value) return;
    if (res->header_count >= res->header_cap) {
        res->header_cap *= 2;
        res->header_names = (char**)aurora_safe_realloc(res->header_names, (size_t)res->header_cap * sizeof(char*));
        res->header_values = (char**)aurora_safe_realloc(res->header_values, (size_t)res->header_cap * sizeof(char*));
    }
    res->header_names[res->header_count] = strdup(name);
    res->header_values[res->header_count] = strdup(value);
    res->header_count++;
}

void aurora_http_response_set_body(AuroraHttpResponse* res, const char* body) {
    if (!res) return;
    free(res->body);
    res->body = body ? strdup(body) : nullptr;
}

int aurora_http_response_send(AuroraHttpResponse* res, int64_t sock) {
    if (!res || res->sent) return -1;
    char header_buf[4096];
    int len = snprintf(header_buf, sizeof(header_buf),
        "HTTP/1.1 %d %s\r\n", res->status_code, res->status_text ? res->status_text : "Unknown");
    for (int i = 0; i < res->header_count; i++) {
        len += snprintf(header_buf + len, sizeof(header_buf) - (size_t)len - 1,
            "%s: %s\r\n", res->header_names[i], res->header_values[i]);
    }
    size_t body_len = res->body ? strlen(res->body) : 0;
    len += snprintf(header_buf + len, sizeof(header_buf) - (size_t)len - 1,
        "Content-Length: %zu\r\n\r\n", body_len);

    /* Send all bytes with partial write retry */
    const char* p = header_buf;
    int remaining = len;
    while (remaining > 0) {
#ifdef _WIN32
        int n = (int)send((SOCKET)(intptr_t)sock, p, remaining, 0);
#else
        int n = (int)send((int)sock, p, (size_t)remaining, MSG_NOSIGNAL);
#endif
        if (n <= 0) break;
        p += n;
        remaining -= n;
    }
    if (res->body) {
        p = res->body;
        int bremaining = (int)body_len;
        while (bremaining > 0) {
#ifdef _WIN32
            int n = (int)send((SOCKET)(intptr_t)sock, p, bremaining, 0);
#else
            int n = (int)send((int)sock, p, (size_t)bremaining, MSG_NOSIGNAL);
#endif
            if (n <= 0) break;
            p += n;
            bremaining -= n;
        }
    }
    res->sent = 1;
    printf("[http] response %d sent\n", res->status_code);
    return 0;
}

/* ── Router ── */
AuroraRouter* aurora_router_new() {
    AuroraRouter* r = (AuroraRouter*)calloc(1, sizeof(AuroraRouter));
    r->cap = 16;
    r->count = 0;
    r->entries = (AuroraRouteEntry*)calloc((size_t)r->cap, sizeof(AuroraRouteEntry));
    return r;
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

int aurora_route_dispatch(AuroraRouter* router, AuroraHttpRequest* req, AuroraHttpResponse* res) {
    if (!router || !req || !res) return -1;
    for (int i = 0; i < router->count; i++) {
        if (strcmp(router->entries[i].path_pattern, req->path) == 0 &&
            strcmp(router->entries[i].method, req->method) == 0) {
            typedef void (*HandlerFn)(AuroraHttpRequest*, AuroraHttpResponse*);
            HandlerFn fn = (HandlerFn)router->entries[i].handler;
            fn(req, res);
            return 0;
        }
    }
    aurora_http_response_set_status(res, 404, "Not Found");
    aurora_http_response_set_body(res, "404 Not Found");
    return 1;
}

/* ── Server accept + handle ── */
void aurora_server_accept_and_handle(AuroraServer* srv, AuroraRouter* router) {
    if (!srv || !srv->handle || !srv->running) return;
#ifdef _WIN32
    SOCKET listen_sock = (SOCKET)(intptr_t)srv->handle;
    struct sockaddr_in client_addr;
    int addr_len = sizeof(client_addr);
    SOCKET client = accept(listen_sock, (struct sockaddr*)&client_addr, &addr_len);
    if (client == INVALID_SOCKET) return;

    char raw[8192];
    int n = recv(client, raw, sizeof(raw) - 1, 0);
    if (n > 0) {
        raw[n] = '\0';
        AuroraHttpRequest* req = aurora_http_parse_request(raw);
        AuroraHttpResponse* res = aurora_http_response_new();
        aurora_http_response_set_header(res, "Content-Type", "text/plain");
        if (req) {
            aurora_route_dispatch(router, req, res);
        } else {
            aurora_http_response_set_status(res, 400, "Bad Request");
            aurora_http_response_set_body(res, "Bad Request");
        }
        aurora_http_response_send(res, (int64_t)(intptr_t)client);
        if (req) {
            free(req->method); free(req->path); free(req->version);
            for (int i = 0; i < req->header_count; i++) {
                free(req->header_names[i]); free(req->header_values[i]);
            }
            free(req->header_names); free(req->header_values);
            free(req->body); free(req);
        }
        free(res->status_text);
        for (int i = 0; i < res->header_count; i++) {
            free(res->header_names[i]); free(res->header_values[i]);
        }
        free(res->header_names); free(res->header_values);
        free(res->body); free(res);
    }
    closesocket(client);
#else
    int listen_sock = (int)(intptr_t)srv->handle;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client = accept(listen_sock, (struct sockaddr*)&client_addr, &addr_len);
    if (client < 0) return;

    char raw[8192];
    int n = (int)recv(client, raw, sizeof(raw) - 1, 0);
    if (n > 0) {
        raw[n] = '\0';
        AuroraHttpRequest* req = aurora_http_parse_request(raw);
        AuroraHttpResponse* res = aurora_http_response_new();
        aurora_http_response_set_header(res, "Content-Type", "text/plain");
        if (req) {
            aurora_route_dispatch(router, req, res);
        } else {
            aurora_http_response_set_status(res, 400, "Bad Request");
            aurora_http_response_set_body(res, "Bad Request");
        }
        aurora_http_response_send(res, (int64_t)client);
        if (req) {
            free(req->method); free(req->path); free(req->version);
            for (int i = 0; i < req->header_count; i++) {
                free(req->header_names[i]); free(req->header_values[i]);
            }
            free(req->header_names); free(req->header_values);
            free(req->body); free(req);
        }
        free(res->status_text);
        for (int i = 0; i < res->header_count; i++) {
            free(res->header_names[i]); free(res->header_values[i]);
        }
        free(res->header_names); free(res->header_values);
        free(res->body); free(res);
    }
    close(client);
#endif
}

/* ── Database ── */
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
    return (void*)(intptr_t)1;
}

void aurora_db_close(AuroraDB* db) {
    if (!db) return;
    db->connected = 0;
    free(db->conn_str);
    printf("[database] closed\n");
    free(db);
}

/* ── Cache (in-memory, linear scan) ── */
AuroraCache* aurora_cache_init() {
    AuroraCache* c = (AuroraCache*)calloc(1, sizeof(AuroraCache));
    c->cap = 16;
    c->count = 0;
    c->keys = (char**)calloc((size_t)c->cap, sizeof(char*));
    c->values = (char**)calloc((size_t)c->cap, sizeof(char*));
    return c;
}

void aurora_cache_set(AuroraCache* cache, const char* key, const char* val) {
    if (!cache || !key || !val) return;
    for (int i = 0; i < cache->count; i++) {
        if (strcmp(cache->keys[i], key) == 0) {
            free(cache->values[i]);
            cache->values[i] = strdup(val);
            return;
        }
    }
    if (cache->count >= cache->cap) {
        cache->cap *= 2;
        cache->keys = (char**)aurora_safe_realloc(cache->keys, (size_t)cache->cap * sizeof(char*));
        cache->values = (char**)aurora_safe_realloc(cache->values, (size_t)cache->cap * sizeof(char*));
    }
    cache->keys[cache->count] = strdup(key);
    cache->values[cache->count] = strdup(val);
    cache->count++;
}

void aurora_cache_destroy(AuroraCache* cache) {
    if (!cache) return;
    for (int i = 0; i < cache->count; i++) {
        free(cache->keys[i]);
        free(cache->values[i]);
    }
    free(cache->keys);
    free(cache->values);
    free(cache);
}

char* aurora_cache_get(AuroraCache* cache, const char* key) {
    if (!cache || !key) return nullptr;
    for (int i = 0; i < cache->count; i++) {
        if (strcmp(cache->keys[i], key) == 0)
            return cache->values[i];
    }
    return nullptr;
}

/* ── Session ── */
static int session_counter = 0;

AuroraSession* aurora_session_create() {
    AuroraSession* sess = (AuroraSession*)calloc(1, sizeof(AuroraSession));
    char id[64];
    snprintf(id, sizeof(id), "sess_%d_%ld", ++session_counter, (long)time(nullptr));
    sess->session_id = strdup(id);
    sess->data = aurora_cache_init();
    printf("[session] created: %s\n", sess->session_id);
    return sess;
}

void aurora_session_destroy(AuroraSession* sess) {
    if (!sess) return;
    free(sess->session_id);
    aurora_cache_destroy(sess->data);
    free(sess);
}

/* ── Constant-time string comparison (prevents timing side-channel) ── */
static int aurora_const_time_cmp(const char* a, const char* b) {
    if (!a || !b) return -1;
    size_t lena = strlen(a), lenb = strlen(b);
    if (lena != lenb) return -1;
    int diff = 0;
    for (size_t i = 0; i < lena; i++) diff |= (unsigned char)a[i] ^ (unsigned char)b[i];
    return diff; /* 0 ≡ equal, non-zero ≡ different */
}

/* ── Auth (reads AURORA_ADMIN_USER / AURORA_ADMIN_PASS env vars) ── */
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

}
