#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Opaque types ── */
typedef struct AuroraRateLimiter AuroraRateLimiter;
typedef struct AuroraGateway     AuroraGateway;

/* ═══════════════════════════════════════════════════════════════
   Rate Limiter (token bucket)
   ═══════════════════════════════════════════════════════════════ */

AuroraRateLimiter* aurora_rate_limiter_new(int max_requests, int window_sec);
void               aurora_rate_limiter_free(AuroraRateLimiter* rl);
int                aurora_rate_limiter_allow(AuroraRateLimiter* rl, const char* key);
int                aurora_rate_limiter_remaining(AuroraRateLimiter* rl, const char* key);
void               aurora_rate_limiter_reset(AuroraRateLimiter* rl, const char* key);

/* ═══════════════════════════════════════════════════════════════
   API Gateway
   ═══════════════════════════════════════════════════════════════ */

AuroraGateway* aurora_gateway_new(AuroraRateLimiter* rl);
void           aurora_gateway_free(AuroraGateway* gw);

/* ── Route registration ── */
int aurora_gateway_add_route(AuroraGateway* gw, const char* method, const char* path,
                              const char* upstream_url);
int aurora_gateway_remove_route(AuroraGateway* gw, const char* method, const char* path);

/* ── Forward request to upstream ── */
int aurora_gateway_forward(AuroraGateway* gw, const char* method, const char* path,
                           const char* body, const char* headers,
                           char* out_buffer, int out_size);

/* ── Request aggregation (batch multiple requests into one) ── */
int aurora_gateway_batch(AuroraGateway* gw, const char* requests_json,
                         char* out_buffer, int out_size);

/* ── Health check endpoint ── */
int aurora_gateway_health(AuroraGateway* gw, char* out_buffer, int out_size);

/* ── Middleware-style handler for request pipeline ── */
int aurora_gateway_handle(AuroraGateway* gw, const char* method, const char* path,
                          const char* body, const char* headers,
                          const char* client_ip,
                          char* out_buffer, int out_size);

#ifdef __cplusplus
}
#endif
