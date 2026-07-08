#include "runtime/webhook.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

/* ── Webhook registry ── */

struct WebhookEntry {
    std::string event;
    std::string url;
    std::string secret;
};

static std::vector<WebhookEntry> g_webhooks;
static std::mutex g_webhook_mutex;

/* ── External HTTP client helpers ── */
extern "C" {
    int aurora_net_http_post(const char* url, const char* body,
                              const char* content_type, char* buffer, int buffer_size);
}

/* ── HMAC-SHA256 helper (from crypto.cpp) ── */
extern "C" {
    void aurora_hmac_sha256(const unsigned char* key, size_t key_len,
                             const unsigned char* data, size_t data_len,
                             unsigned char* out);
}

static void hex_encode(const unsigned char* in, int in_len, char* out) {
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < in_len; i++) {
        out[i * 2] = hex[(in[i] >> 4) & 0xf];
        out[i * 2 + 1] = hex[in[i] & 0xf];
    }
    out[in_len * 2] = '\0';
}

int aurora_webhook_register(const char* event, const char* url, const char* secret) {
    if (!event || !url) return 0;
    std::lock_guard<std::mutex> lock(g_webhook_mutex);
    WebhookEntry e;
    e.event = event;
    e.url = url;
    e.secret = secret ? secret : "";
    g_webhooks.push_back(e);
    return 1;
}

int aurora_webhook_unregister(const char* event, const char* url) {
    if (!event || !url) return 0;
    std::lock_guard<std::mutex> lock(g_webhook_mutex);
    for (auto it = g_webhooks.begin(); it != g_webhooks.end(); ++it) {
        if (it->event == event && it->url == url) {
            g_webhooks.erase(it);
            return 1;
        }
    }
    return 0;
}

int aurora_webhook_trigger(const char* event, const char* payload_json) {
    if (!event) return 0;
    if (!payload_json) payload_json = "{}";
    std::vector<WebhookEntry> to_trigger;
    {
        std::lock_guard<std::mutex> lock(g_webhook_mutex);
        for (const auto& e : g_webhooks) {
            if (e.event == event) to_trigger.push_back(e);
        }
    }
    int count = 0;
    for (const auto& e : to_trigger) {
        /* Compute HMAC-SHA256 signature */
        char sig_hex[128] = "";
        if (!e.secret.empty()) {
            unsigned char hmac[32];
            aurora_hmac_sha256((const unsigned char*)e.secret.c_str(), e.secret.size(),
                                (const unsigned char*)payload_json, strlen(payload_json),
                                hmac);
            hex_encode(hmac, 32, sig_hex);
        }
        /* Build full URL */
        char buffer[8192];
        int n = aurora_net_http_post(e.url.c_str(), payload_json,
                                      "application/json", buffer, sizeof(buffer));
        if (n > 0) count++;
    }
    return count;
}

int aurora_webhook_verify(const char* payload, const char* signature, const char* secret) {
    if (!payload || !signature || !secret) return 0;
    unsigned char hmac[32];
    aurora_hmac_sha256((const unsigned char*)secret, strlen(secret),
                        (const unsigned char*)payload, strlen(payload),
                        hmac);
    char sig_hex[128];
    hex_encode(hmac, 32, sig_hex);
    return strcmp(sig_hex, signature) == 0 ? 1 : 0;
}

void aurora_webhook_clear_all(void) {
    std::lock_guard<std::mutex> lock(g_webhook_mutex);
    g_webhooks.clear();
}
