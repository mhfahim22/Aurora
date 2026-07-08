#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Crash Reporting & Analytics (11) ── */
int         aurora_telemetry_init(const char* endpoint, int enable_crash, int enable_analytics);
int         aurora_telemetry_set_user_id(const char* user_id);
int         aurora_telemetry_set_app_version(const char* version);
int         aurora_telemetry_track_event(const char* category, const char* action, const char* label);
int         aurora_telemetry_track_error(const char* message, const char* stack_trace);
int         aurora_telemetry_opt_in(void);
int         aurora_telemetry_opt_out(void);
int         aurora_telemetry_is_opted_in(void);
int         aurora_telemetry_send_now(void);
const char* aurora_telemetry_get_endpoint(void);
void        aurora_telemetry_shutdown(void);

#ifdef __cplusplus
}
#endif
