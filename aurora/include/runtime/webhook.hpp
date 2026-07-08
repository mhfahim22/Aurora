#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Webhook system ── */

/* Register a webhook: when `event` is triggered, POST to `url` with `secret` signing */
int aurora_webhook_register(const char* event, const char* url, const char* secret);

/* Unregister a webhook */
int aurora_webhook_unregister(const char* event, const char* url);

/* Trigger all webhooks registered for `event` with `payload_json` */
int aurora_webhook_trigger(const char* event, const char* payload_json);

/* Verify an incoming webhook request signature (HMAC-SHA256 hex) */
int aurora_webhook_verify(const char* payload, const char* signature, const char* secret);

/* Clear all webhooks */
void aurora_webhook_clear_all(void);

#ifdef __cplusplus
}
#endif
