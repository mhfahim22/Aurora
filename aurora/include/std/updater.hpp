#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Auto-Update (10) ── */
int         aurora_updater_init(const char* app_name, const char* current_version, const char* update_url);
int         aurora_updater_check(void);
const char* aurora_updater_get_latest_version(void);
const char* aurora_updater_get_download_url(void);
int         aurora_updater_download(void);
int         aurora_updater_download_progress(void);
int         aurora_updater_apply(void);
int         aurora_updater_rollback(void);
int         aurora_updater_set_channel(const char* channel);
const char* aurora_updater_get_channel(void);
void        aurora_updater_shutdown(void);

#ifdef __cplusplus
}
#endif
