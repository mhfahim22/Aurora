#include "std/telemetry.hpp"
#include "std/json.hpp"
#include "std/net.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define PATH_SEP '\\'
#else
#include <unistd.h>
#define PATH_SEP '/'
#endif

static struct {
    char endpoint[1024];
    char user_id[256];
    char app_version[64];
    int initialized;
    int crash_enabled;
    int analytics_enabled;
    int opted_in;
    std::vector<std::string> pending_events;
} g_telemetry = {};

static std::string make_timestamp() {
    time_t t = time(nullptr);
    char buf[64];
    struct tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm_buf);
    return buf;
}

int aurora_telemetry_init(const char* endpoint, int enable_crash, int enable_analytics) {
    if (!endpoint) return 0;
    memset(&g_telemetry, 0, sizeof(g_telemetry));
    strncpy(g_telemetry.endpoint, endpoint, sizeof(g_telemetry.endpoint) - 1);
    g_telemetry.crash_enabled = enable_crash;
    g_telemetry.analytics_enabled = enable_analytics;
    g_telemetry.opted_in = 1;
    g_telemetry.initialized = 1;
    return 1;
}

int aurora_telemetry_set_user_id(const char* user_id) {
    if (!user_id || !g_telemetry.initialized) return 0;
    strncpy(g_telemetry.user_id, user_id, sizeof(g_telemetry.user_id) - 1);
    return 1;
}

int aurora_telemetry_set_app_version(const char* version) {
    if (!version || !g_telemetry.initialized) return 0;
    strncpy(g_telemetry.app_version, version, sizeof(g_telemetry.app_version) - 1);
    return 1;
}

int aurora_telemetry_track_event(const char* category, const char* action, const char* label) {
    if (!g_telemetry.initialized || !g_telemetry.opted_in || !g_telemetry.analytics_enabled)
        return 0;
    if (!category || !action) return 0;

    JsonValue* event = aurora_json_new_object();
    aurora_json_set_str(event, "type", "event");
    aurora_json_set_str(event, "category", category);
    aurora_json_set_str(event, "action", action);
    if (label) aurora_json_set_str(event, "label", label);
    aurora_json_set_str(event, "timestamp", make_timestamp().c_str());
    if (g_telemetry.user_id[0]) aurora_json_set_str(event, "user_id", g_telemetry.user_id);
    if (g_telemetry.app_version[0]) aurora_json_set_str(event, "app_version", g_telemetry.app_version);

    char* json = aurora_json_serialize(event);
    if (json) g_telemetry.pending_events.push_back(json);
    free(json);
    aurora_json_free(event);
    return 1;
}

int aurora_telemetry_track_error(const char* message, const char* stack_trace) {
    if (!g_telemetry.initialized || !g_telemetry.opted_in || !g_telemetry.crash_enabled)
        return 0;
    if (!message) return 0;

    JsonValue* error = aurora_json_new_object();
    aurora_json_set_str(error, "type", "error");
    aurora_json_set_str(error, "message", message);
    if (stack_trace) aurora_json_set_str(error, "stack_trace", stack_trace);
    aurora_json_set_str(error, "timestamp", make_timestamp().c_str());
    if (g_telemetry.user_id[0]) aurora_json_set_str(error, "user_id", g_telemetry.user_id);
    if (g_telemetry.app_version[0]) aurora_json_set_str(error, "app_version", g_telemetry.app_version);

    char* json = aurora_json_serialize(error);
    if (json) g_telemetry.pending_events.push_back(json);
    free(json);
    aurora_json_free(error);
    return 1;
}

int aurora_telemetry_opt_in(void) {
    if (!g_telemetry.initialized) return 0;
    g_telemetry.opted_in = 1;
    return 1;
}

int aurora_telemetry_opt_out(void) {
    if (!g_telemetry.initialized) return 0;
    g_telemetry.opted_in = 0;
    return 1;
}

int aurora_telemetry_is_opted_in(void) {
    if (!g_telemetry.initialized) return 0;
    return g_telemetry.opted_in;
}

int aurora_telemetry_send_now(void) {
    if (!g_telemetry.initialized || g_telemetry.pending_events.empty()) return 0;

    JsonValue* batch = aurora_json_new_array();
    for (size_t i = 0; i < g_telemetry.pending_events.size(); i++) {
        JsonValue* ev = aurora_json_parse(g_telemetry.pending_events[i].c_str());
        if (ev) { aurora_json_array_push(batch, ev); }
    }
    char* json = aurora_json_serialize(batch);
    if (!json) { aurora_json_free(batch); return 0; }

    /* Send via HTTP POST */
    std::string url = g_telemetry.endpoint;
    url += "/api/events";
    char resp_buf[4096] = {};
    int ret = aurora_net_http_post(url.c_str(), json, "application/json",
                                    resp_buf, (int)sizeof(resp_buf));
    (void)ret;

    free(json);
    aurora_json_free(batch);
    g_telemetry.pending_events.clear();
    return 1;
}

const char* aurora_telemetry_get_endpoint(void) {
    if (!g_telemetry.initialized) return nullptr;
    return g_telemetry.endpoint;
}

void aurora_telemetry_shutdown(void) {
    if (g_telemetry.initialized && !g_telemetry.pending_events.empty())
        aurora_telemetry_send_now();
    g_telemetry.pending_events.clear();
    memset(&g_telemetry, 0, sizeof(g_telemetry));
}
