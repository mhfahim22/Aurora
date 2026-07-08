#include "runtime/rest.hpp"
#include "runtime/orm.hpp"
#include "runtime/backend.hpp"
#include "std/db.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <sstream>

#if defined(_MSC_VER) && !defined(strdup)
#define strdup _strdup
#endif

/* ── Internal structures ── */

struct RestHookEntry {
    int hook_type;
    void* callback;
    void* user_data;
};

struct RestResource {
    std::string table_name;
    void* db;
    AuroraModelSchema* schema;
    std::vector<RestHookEntry> hooks;
    std::string prefix;
};

static std::vector<RestResource> g_rest_resources;
static std::mutex g_rest_mutex;

/* ── Internal: find resource by URL path ── */
static RestResource* find_resource(const char* path) {
    if (!path) return nullptr;
    std::string spath = path;
    for (auto& res : g_rest_resources) {
        std::string expected = "/" + res.prefix + (res.prefix.empty() ? "" : "/") + res.table_name;
        if (spath == expected || spath.find(expected + "/") == 0)
            return &res;
    }
    return nullptr;
}

static RestHookEntry* find_hook(RestResource* res, int hook_type) {
    if (!res) return nullptr;
    for (auto& h : res->hooks) {
        if (h.hook_type == hook_type) return &h;
    }
    return nullptr;
}

static void run_hook(RestResource* res, int hook_type,
                     AuroraHttpRequest* req, AuroraHttpResponse* res2) {
    auto* h = find_hook(res, hook_type);
    if (h && h->callback) {
        auto* fn = (int(*)(AuroraHttpRequest*, AuroraHttpResponse*, void*))h->callback;
        fn(req, res2, h->user_data);
    }
}

static int run_hook_abortable(RestResource* res, int hook_type,
                               AuroraHttpRequest* req, AuroraHttpResponse* res2) {
    auto* h = find_hook(res, hook_type);
    if (h && h->callback) {
        auto* fn = (int(*)(AuroraHttpRequest*, AuroraHttpResponse*, void*))h->callback;
        return fn(req, res2, h->user_data);
    }
    return 0;
}

/* ── Generic handler trampoline ── */
/* All REST routes dispatch through this single handler,
   which looks up the resource by URL path. */
static void rest_dispatch_handler(AuroraHttpRequest* req, AuroraHttpResponse* res) {
    RestResource* rr = find_resource(req->path);
    if (!rr || !rr->schema || !rr->db) {
        res->status_code = 500;
        aurora_http_response_set_body(res, "{\"error\":\"resource not found\"}");
        return;
    }

    /* Determine method and whether an :id param is present */
    const char* method = req->method;
    const char* id_str = aurora_http_get_param(req, "id");
    bool has_id = (id_str != nullptr && *id_str);

    if (strcmp(method, "GET") == 0) {
        if (has_id) {
            /* FIND */
            if (run_hook_abortable(rr, AURORA_REST_HOOK_BEFORE_FIND, req, res)) return;
            int64_t id = atoll(id_str);
            char* result = aurora_orm_find(rr->db, rr->schema, id);
            if (result) {
                aurora_http_response_set_content_type(res, "application/json");
                aurora_http_response_set_body(res, result);
                free(result);
            } else {
                aurora_http_response_set_status(res, 404, "Not Found");
                aurora_http_response_set_body(res, "{\"error\":\"not found\"}");
            }
            run_hook(rr, AURORA_REST_HOOK_AFTER_FIND, req, res);
        } else {
            /* LIST ALL */
            if (run_hook_abortable(rr, AURORA_REST_HOOK_BEFORE_LIST, req, res)) return;
            char* result = aurora_orm_all(rr->db, rr->schema);
            if (result) {
                aurora_http_response_set_content_type(res, "application/json");
                aurora_http_response_set_body(res, result);
                free(result);
            } else {
                aurora_http_response_set_status(res, 500, "Internal Server Error");
                aurora_http_response_set_body(res, "{\"error\":\"query failed\"}");
            }
            run_hook(rr, AURORA_REST_HOOK_AFTER_LIST, req, res);
        }
    } else if (strcmp(method, "POST") == 0) {
        /* CREATE */
        if (run_hook_abortable(rr, AURORA_REST_HOOK_BEFORE_CREATE, req, res)) return;
        if (!req->body || !*req->body) {
            aurora_http_response_set_status(res, 400, "Bad Request");
            aurora_http_response_set_body(res, "{\"error\":\"missing body\"}");
            return;
        }
        int64_t id = aurora_orm_create(rr->db, rr->schema, req->body);
        if (id >= 0) {
            char id_buf[64];
            snprintf(id_buf, sizeof(id_buf), "%lld", (long long)id);
            std::string result = "{\"id\":";
            result += id_buf;
            result += "}";
            aurora_http_response_set_status(res, 201, "Created");
            aurora_http_response_set_content_type(res, "application/json");
            aurora_http_response_set_body(res, result.c_str());
        } else {
            aurora_http_response_set_status(res, 500, "Internal Server Error");
            aurora_http_response_set_body(res, "{\"error\":\"create failed\"}");
        }
        run_hook(rr, AURORA_REST_HOOK_AFTER_CREATE, req, res);
    } else if (strcmp(method, "PUT") == 0) {
        /* UPDATE */
        if (run_hook_abortable(rr, AURORA_REST_HOOK_BEFORE_UPDATE, req, res)) return;
        if (!has_id || !req->body || !*req->body) {
            aurora_http_response_set_status(res, 400, "Bad Request");
            aurora_http_response_set_body(res, "{\"error\":\"missing id or body\"}");
            return;
        }
        int64_t id = atoll(id_str);
        int rc = aurora_orm_update(rr->db, rr->schema, id, req->body);
        if (rc >= 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "{\"affected\":%d}", rc);
            aurora_http_response_set_content_type(res, "application/json");
            aurora_http_response_set_body(res, buf);
        } else {
            aurora_http_response_set_status(res, 500, "Internal Server Error");
            aurora_http_response_set_body(res, "{\"error\":\"update failed\"}");
        }
        run_hook(rr, AURORA_REST_HOOK_AFTER_UPDATE, req, res);
    } else if (strcmp(method, "DELETE") == 0) {
        /* DELETE */
        if (run_hook_abortable(rr, AURORA_REST_HOOK_BEFORE_DELETE, req, res)) return;
        if (!has_id) {
            aurora_http_response_set_status(res, 400, "Bad Request");
            aurora_http_response_set_body(res, "{\"error\":\"missing id\"}");
            return;
        }
        int64_t id = atoll(id_str);
        int rc = aurora_orm_delete(rr->db, rr->schema, id);
        if (rc >= 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "{\"affected\":%d}", rc);
            aurora_http_response_set_content_type(res, "application/json");
            aurora_http_response_set_body(res, buf);
        } else {
            aurora_http_response_set_status(res, 500, "Internal Server Error");
            aurora_http_response_set_body(res, "{\"error\":\"delete failed\"}");
        }
        run_hook(rr, AURORA_REST_HOOK_AFTER_DELETE, req, res);
    } else {
        aurora_http_response_set_status(res, 405, "Method Not Allowed");
        aurora_http_response_set_body(res, "{\"error\":\"method not allowed\"}");
    }
}

/* ── Public API ── */

int aurora_rest_register(AuroraRouter* router, void* db,
                          AuroraModelSchema* schema, const char* prefix) {
    if (!router || !db || !schema) return -1;

    std::string p = prefix ? prefix : "";

    /* Register resource in global list */
    {
        std::lock_guard<std::mutex> lock(g_rest_mutex);
        RestResource rr;
        rr.table_name = schema->table_name;
        rr.db = db;
        rr.schema = schema;
        rr.prefix = p;
        g_rest_resources.push_back(rr);
    }

    /* Register routes — all dispatch through rest_dispatch_handler */
    std::string base_path = "/" + p + (p.empty() ? "" : "/") + schema->table_name;
    std::string id_path = base_path + "/:id";

    aurora_route_add(router, "GET",    base_path.c_str(), (void*)rest_dispatch_handler);
    aurora_route_add(router, "GET",    id_path.c_str(),   (void*)rest_dispatch_handler);
    aurora_route_add(router, "POST",   base_path.c_str(), (void*)rest_dispatch_handler);
    aurora_route_add(router, "PUT",    id_path.c_str(),   (void*)rest_dispatch_handler);
    aurora_route_add(router, "DELETE", id_path.c_str(),   (void*)rest_dispatch_handler);

    return 0;
}

int aurora_rest_unregister(AuroraRouter* router, AuroraModelSchema* schema) {
    (void)router;
    if (!schema) return -1;
    std::lock_guard<std::mutex> lock(g_rest_mutex);
    for (size_t i = 0; i < g_rest_resources.size(); i++) {
        if (g_rest_resources[i].table_name == schema->table_name) {
            g_rest_resources.erase(g_rest_resources.begin() + i);
            return 0;
        }
    }
    return -1;
}

int aurora_rest_set_hook(AuroraRouter* router, AuroraModelSchema* schema,
                          int hook_type, void* callback, void* user_data) {
    (void)router;
    if (!schema) return -1;
    std::lock_guard<std::mutex> lock(g_rest_mutex);
    for (auto& res : g_rest_resources) {
        if (res.table_name == schema->table_name) {
            for (auto& h : res.hooks) {
                if (h.hook_type == hook_type) {
                    h.callback = callback;
                    h.user_data = user_data;
                    return 0;
                }
            }
            RestHookEntry entry;
            entry.hook_type = hook_type;
            entry.callback = callback;
            entry.user_data = user_data;
            res.hooks.push_back(entry);
            return 0;
        }
    }
    /* Not registered yet — create a pending entry */
    RestResource rr;
    rr.table_name = schema->table_name;
    rr.db = nullptr;
    rr.schema = schema;
    rr.prefix = "";
    RestHookEntry entry;
    entry.hook_type = hook_type;
    entry.callback = callback;
    entry.user_data = user_data;
    rr.hooks.push_back(entry);
    g_rest_resources.push_back(rr);
    return 0;
}
