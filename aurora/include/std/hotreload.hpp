#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* ── File Watching (4) ── */
int         aurora_hotreload_watch(const char* path);
void        aurora_hotreload_unwatch(const char* path);
const char* aurora_hotreload_poll(void);
void        aurora_hotreload_clear(void);

/* ── UI Reload (4) ── */
void        aurora_hotreload_ui_set_rebuild_fn(void (*fn)(const char*));
void        aurora_hotreload_ui_rebuild(const char* widget_id);
void        aurora_hotreload_ui_preserve_state(const char* id, const char* data);
const char* aurora_hotreload_ui_get_state(const char* id);

/* ── Code Reload (4) ── */
int         aurora_hotreload_code_reload(const char* module);
int         aurora_hotreload_code_is_stale(const char* module);
void        aurora_hotreload_code_set_reload_fn(void (*fn)(const char*));
const char* aurora_hotreload_code_get_version(const char* module);

/* ── Asset Reload (3) ── */
void        aurora_hotreload_asset_reload(const char* path);
void        aurora_hotreload_asset_set_reload_fn(void (*fn)(const char*));
int         aurora_hotreload_asset_is_dirty(const char* path);

/* ── State Preservation (3) ── */
void        aurora_hotreload_state_save(const char* key, const char* data);
const char* aurora_hotreload_state_load(const char* key);
void        aurora_hotreload_state_clear(void);

/* ── Developer Console (4) ── */
void        aurora_hotreload_console_open(void);
void        aurora_hotreload_console_close(void);
void        aurora_hotreload_console_log(const char* msg);
const char* aurora_hotreload_console_exec(const char* command);

#ifdef __cplusplus
}
#endif
