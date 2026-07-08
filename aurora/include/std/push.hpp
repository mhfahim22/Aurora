#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Push Notifications (9) ── */
int         aurora_push_init(void);
int         aurora_push_register(void);
int         aurora_push_unregister(void);
int         aurora_push_is_registered(void);
int         aurora_push_set_callback(void* callback);
const char* aurora_push_get_token(void);
int         aurora_push_send_local(const char* title, const char* body, int delay_ms);
int         aurora_push_cancel_local(int notification_id);
void        aurora_push_shutdown(void);

#ifdef __cplusplus
}
#endif
