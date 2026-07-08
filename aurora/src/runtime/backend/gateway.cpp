#include "runtime/gateway.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <mutex>

/* ── External HTTP client functions from net.cpp (C linkage) ── */
extern "C" {
    int aurora_net_http_get_ex(const char* url, const char* headers,
                                char* buffer, int buffer_size);
    int aurora_net_http_post_ex(const char* url, const char* headers,
                                 const char* body, const char* content_type,
                                 char* buffer, int buffer_size);
}

/* ── Helper ── */

static char* strdup_c(const char* s) {
    if (!s) return nullptr;
    size_t n = strlen(s) + 1;
    char* p = (char*)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ═══════════════════════════════════════════════════════════════
   Rate Limiter (token bucket with sliding window)
   ═══════════════════════════════════════════════════════════════ */

struct BucketEntry {
    int tokens;
    std::chrono::steady_clock::time_point window_start;
};

struct AuroraRateLimiter {
    int max_tokens;
    int window_sec;
    std::unordered_map<std::string, BucketEntry> buckets;
    std::mutex mtx;
};

AuroraRateLimiter* aurora_rate_limiter_new(int max_requests, int window_sec) {
    AuroraRateLimiter* rl = new AuroraRateLimiter();
    if (!rl) return nullptr;
    rl->max_tokens = max_requests > 0 ? max_requests : 100;
    rl->window_sec = window_sec > 0 ? window_sec : 60;
    return rl;
}

void aurora_rate_limiter_free(AuroraRateLimiter* rl) {
    delete rl;
}

int aurora_rate_limiter_allow(AuroraRateLimiter* rl, const char* key) {
    if (!rl || !key) return 0;
    std::lock_guard<std::mutex> lock(rl->mtx);
    auto now = std::chrono::steady_clock::now();
    auto& entry = rl->buckets[key];
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - entry.window_start).count();
    if (elapsed >= rl->window_sec) {
        entry.tokens = rl->max_tokens;
        entry.window_start = now;
    }
    if (entry.tokens > 0) {
        entry.tokens--;
        return 1;
    }
    return 0;
}

int aurora_rate_limiter_remaining(AuroraRateLimiter* rl, const char* key) {
    if (!rl || !key) return 0;
    std::lock_guard<std::mutex> lock(rl->mtx);
    auto it = rl->buckets.find(key);
    if (it == rl->buckets.end()) return rl->max_tokens;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.window_start).count();
    if (elapsed >= rl->window_sec) return rl->max_tokens;
    return it->second.tokens;
}

void aurora_rate_limiter_reset(AuroraRateLimiter* rl, const char* key) {
    if (!rl || !key) return;
    std::lock_guard<std::mutex> lock(rl->mtx);
    rl->buckets.erase(key);
}

/* ═══════════════════════════════════════════════════════════════
   API Gateway
   ═══════════════════════════════════════════════════════════════ */

struct GatewayRoute {
    std::string method;
    std::string path;
    std::string upstream_url;
};

struct AuroraGateway {
    AuroraRateLimiter* rate_limiter;
    std::vector<GatewayRoute> routes;
    std::mutex mtx;
};

AuroraGateway* aurora_gateway_new(AuroraRateLimiter* rl) {
    AuroraGateway* gw = new AuroraGateway();
    if (!gw) return nullptr;
    gw->rate_limiter = rl;
    return gw;
}

void aurora_gateway_free(AuroraGateway* gw) {
    delete gw;
}

int aurora_gateway_add_route(AuroraGateway* gw, const char* method, const char* path,
                              const char* upstream_url) {
    if (!gw || !method || !path || !upstream_url) return 0;
    std::lock_guard<std::mutex> lock(gw->mtx);
    GatewayRoute r;
    r.method = method;
    r.path = path;
    r.upstream_url = upstream_url;
    gw->routes.push_back(r);
    return 1;
}

int aurora_gateway_remove_route(AuroraGateway* gw, const char* method, const char* path) {
    if (!gw || !method || !path) return 0;
    std::lock_guard<std::mutex> lock(gw->mtx);
    for (auto it = gw->routes.begin(); it != gw->routes.end(); ++it) {
        if (it->method == method && it->path == path) {
            gw->routes.erase(it);
            return 1;
        }
    }
    return 0;
}

static const GatewayRoute* match_route(AuroraGateway* gw, const char* method, const char* path) {
    for (const auto& r : gw->routes) {
        if (r.method == "*" || r.method == method) {
            /* Simple prefix match */
            if (r.path == path || (r.path.back() == '*' &&
                strncmp(path, r.path.c_str(), r.path.size() - 1) == 0)) {
                return &r;
            }
        }
    }
    return nullptr;
}

int aurora_gateway_forward(AuroraGateway* gw, const char* method, const char* path,
                           const char* body, const char* headers,
                           char* out_buffer, int out_size) {
    if (!gw || !out_buffer || out_size <= 0) return 0;
    out_buffer[0] = '\0';

    const GatewayRoute* route = match_route(gw, method, path);
    if (!route) {
        snprintf(out_buffer, (size_t)out_size,
                 "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n");
        return 0;
    }

    /* Build upstream URL from route */
    std::string url = route->upstream_url;
    if (!body) body = "";
    if (!headers) headers = "";

    /* Use internal HTTP client (aurora_net_http_get_ex / aurora_net_http_post_ex) */
    char method_upper[16];
    snprintf(method_upper, sizeof(method_upper), "%s", method);
    for (int i = 0; method_upper[i]; i++) method_upper[i] = (char)toupper((unsigned char)method_upper[i]);

    /* Forward via http_get_ex / http_post_ex based on method */
    int result = 0;
    if (strcmp(method_upper, "GET") == 0) {
        /* For GET, forward headers */
        char full_url[4096];
        snprintf(full_url, sizeof(full_url), "%s%s", url.c_str(), path);
        /* Call the existing HTTP client function */
        result = aurora_net_http_get_ex(full_url, headers, out_buffer, out_size);
    } else if (strcmp(method_upper, "POST") == 0) {
        char full_url[4096];
        snprintf(full_url, sizeof(full_url), "%s%s", url.c_str(), path);
        result = aurora_net_http_post_ex(full_url, headers, body,
                                          "application/json", out_buffer, out_size);
    } else {
        snprintf(out_buffer, (size_t)out_size,
                 "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\n\r\n");
    }
    return result;
}

int aurora_gateway_batch(AuroraGateway* gw, const char* requests_json,
                         char* out_buffer, int out_size) {
    if (!gw || !requests_json || !out_buffer || out_size <= 0) return 0;
    out_buffer[0] = '\0';

    /* Simple batch: JSON array of {method, path, body} -> execute each, return array */
    std::string result = "[";
    const char* p = requests_json;
    int first = 1;
    while (*p) {
        /* Find each object */
        const char* obj_start = strchr(p, '{');
        if (!obj_start) break;
        const char* obj_end = strchr(obj_start, '}');
        if (!obj_end) break;

        std::string obj(obj_start, (size_t)(obj_end - obj_start + 1));

        /* Extract method */
        const char* m = strstr(obj.c_str(), "\"method\"");
        std::string method = "GET";
        if (m) {
            m = strchr(m, ':');
            if (m) {
                while (*m && (*m == ':' || *m == ' ' || *m == '"')) m++;
                const char* mend = m;
                while (*mend && *mend != '"') mend++;
                method = std::string(m, (size_t)(mend - m));
            }
        }

        /* Extract path */
        const char* pa = strstr(obj.c_str(), "\"path\"");
        std::string path = "/";
        if (pa) {
            pa = strchr(pa, ':');
            if (pa) {
                while (*pa && (*pa == ':' || *pa == ' ' || *pa == '"')) pa++;
                const char* pend = pa;
                while (*pend && *pend != '"') pend++;
                path = std::string(pa, (size_t)(pend - pa));
            }
        }

        /* Extract body */
        const char* b = strstr(obj.c_str(), "\"body\"");
        std::string body = "";
        if (b) {
            b = strchr(b, ':');
            if (b) {
                while (*b && (*b == ':' || *b == ' ' || *b == '"')) b++;
                const char* bend = b;
                while (*bend && *bend != '"') {
                    if (*bend == '\\' && bend[1]) bend++;
                    bend++;
                }
                body = std::string(b, (size_t)(bend - b));
            }
        }

        char buf[65536];
        aurora_gateway_forward(gw, method.c_str(), path.c_str(),
                                body.c_str(), nullptr, buf, sizeof(buf));

        if (!first) result += ",";
        first = 0;

        /* Wrap result in JSON */
        result += "{\"path\":\"" + path + "\",\"status\":200,\"body\":\"";
        /* Escape buf for JSON */
        for (const char* cp = buf; *cp; cp++) {
            if (*cp == '"') result += "\\\"";
            else if (*cp == '\\') result += "\\\\";
            else if (*cp == '\n') result += "\\n";
            else if (*cp == '\r') result += "\\r";
            else result += *cp;
        }
        result += "\"}";

        p = obj_end + 1;
    }
    result += "]";

    size_t len = result.size();
    if ((int)len < out_size - 1) {
        memcpy(out_buffer, result.c_str(), len);
        out_buffer[len] = '\0';
    }
    return (int)len;
}

int aurora_gateway_health(AuroraGateway* gw, char* out_buffer, int out_size) {
    if (!out_buffer || out_size <= 0) return 0;
    snprintf(out_buffer, (size_t)out_size,
             "{\"status\":\"ok\",\"uptime\":0,\"routes\":%zu}",
             gw ? gw->routes.size() : 0);
    return 1;
}

int aurora_gateway_handle(AuroraGateway* gw, const char* method, const char* path,
                          const char* body, const char* headers,
                          const char* client_ip,
                          char* out_buffer, int out_size) {
    if (!gw || !out_buffer || out_size <= 0) return 0;
    out_buffer[0] = '\0';

    /* 1. Rate limit check */
    if (gw->rate_limiter && client_ip) {
        if (!aurora_rate_limiter_allow(gw->rate_limiter, client_ip)) {
            snprintf(out_buffer, (size_t)out_size,
                     "HTTP/1.1 429 Too Many Requests\r\n"
                     "Content-Type: application/json\r\n"
                     "Retry-After: 60\r\n"
                     "Content-Length: 35\r\n\r\n"
                     "{\"error\":\"rate_limit_exceeded\"}");
            return 0;
        }
    }

    return aurora_gateway_forward(gw, method, path, body, headers, out_buffer, out_size);
}
