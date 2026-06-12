#include "runtime/backend.hpp"
#include "common/platform.hpp"
#include "runtime/memory.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cmath>
#include <thread>
#include <chrono>
#include <mutex>

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

/* ── Current time in milliseconds ── */
static int64_t now_ms() {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    return (int64_t)std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

/* ── Server ── */
AuroraServer* aurora_server_init(int64_t port) {
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

int aurora_route_dispatch(AuroraRouter* router, AuroraHttpRequest* req, AuroraHttpResponse* res) {
    if (!router || !req || !res) return -1;
    const char* path_match = req->path_without_query ? req->path_without_query : req->path;

    for (int i = 0; i < router->count; i++) {
        if (strcmp(router->entries[i].method, req->method) != 0)
            continue;

        if (router->use_prefix_match) {
            size_t plen = strlen(router->entries[i].path_pattern);
            if (strncmp(router->entries[i].path_pattern, path_match, plen) == 0) {
                typedef void (*HandlerFn)(AuroraHttpRequest*, AuroraHttpResponse*);
                HandlerFn fn = (HandlerFn)router->entries[i].handler;
                fn(req, res);
                return 0;
            }
        } else {
            if (strcmp(router->entries[i].path_pattern, path_match) == 0) {
                typedef void (*HandlerFn)(AuroraHttpRequest*, AuroraHttpResponse*);
                HandlerFn fn = (HandlerFn)router->entries[i].handler;
                fn(req, res);
                return 0;
            }
        }
    }
    aurora_http_response_set_status(res, 404, "Not Found");
    aurora_http_response_set_body(res, "404 Not Found");
    return 1;
}

/* ── Server accept + handle (with middleware) ── */
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
            /* Run middleware chain first */
            int mw_result = aurora_middleware_run_chain(
                srv->middleware_handlers, srv->middleware_count, req, res);
            if (mw_result == 0) {
                aurora_route_dispatch(router, req, res);
            }
        } else {
            aurora_http_response_set_status(res, 400, "Bad Request");
            aurora_http_response_set_body(res, "Bad Request");
        }
        aurora_http_response_send(res, (int64_t)(intptr_t)client);
        aurora_http_request_free(req);
        aurora_http_response_free(res);
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
            int mw_result = aurora_middleware_run_chain(
                srv->middleware_handlers, srv->middleware_count, req, res);
            if (mw_result == 0) {
                aurora_route_dispatch(router, req, res);
            }
        } else {
            aurora_http_response_set_status(res, 400, "Bad Request");
            aurora_http_response_set_body(res, "Bad Request");
        }
        aurora_http_response_send(res, (int64_t)client);
        aurora_http_request_free(req);
        aurora_http_response_free(res);
    }
    close(client);
#endif
}

/* ── In-memory database engine ── */
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <algorithm>

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

/* Helper: trim whitespace */
static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

/* Helper: split string by delimiter */
static std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> parts;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim))
        parts.push_back(trim(item));
    return parts;
}

/* Helper: split keeping quoted strings together */
static std::vector<std::string> split_sql_args(const std::string& s) {
    std::vector<std::string> parts;
    std::string cur;
    bool in_quote = false;
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (c == '\'') {
            in_quote = !in_quote;
            cur += c;
        } else if (c == ',' && !in_quote) {
            parts.push_back(trim(cur));
            cur.clear();
        } else {
            cur += c;
        }
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
    if (result.empty()) {
        /* Return 1 to indicate success for non-SELECT queries */
        return (void*)(intptr_t)1;
    }
    /* Return result as a duplicated string */
    return (void*)strdup(result.c_str());
}

void aurora_db_query_free(void* result) {
    if (result && result != (void*)(intptr_t)1)
        free(result);
}

void aurora_db_close(AuroraDB* db) {
    if (!db) return;
    db->connected = 0;
    free(db->conn_str);
    printf("[database] closed\n");
    free(db);
}

/* ── Cache (in-memory with optional TTL) ── */
AuroraCache* aurora_cache_init() {
    AuroraCache* c = (AuroraCache*)calloc(1, sizeof(AuroraCache));
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
    int idx = cache_find(cache, key);
    if (idx >= 0) {
        free(cache->values[idx]);
        cache->values[idx] = strdup(val);
        cache->expires_at[idx] = ttl_ms > 0 ? now_ms() + ttl_ms : 0;
        return;
    }
    if (cache->count >= cache->cap) {
        cache->cap *= 2;
        cache->keys = (char**)aurora_safe_realloc(cache->keys, (size_t)cache->cap * sizeof(char*));
        cache->values = (char**)aurora_safe_realloc(cache->values, (size_t)cache->cap * sizeof(char*));
        cache->expires_at = (int64_t*)aurora_safe_realloc(cache->expires_at, (size_t)cache->cap * sizeof(int64_t));
    }
    cache->keys[cache->count] = strdup(key);
    cache->values[cache->count] = strdup(val);
    cache->expires_at[cache->count] = ttl_ms > 0 ? now_ms() + ttl_ms : 0;
    cache->count++;
}

char* aurora_cache_get(AuroraCache* cache, const char* key) {
    if (!cache || !key) return nullptr;
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
    for (int i = 0; i < cache->count; i++) {
        free(cache->keys[i]);
        free(cache->values[i]);
    }
    cache->count = 0;
}

void aurora_cache_clean_expired(AuroraCache* cache) {
    if (!cache) return;
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

}
