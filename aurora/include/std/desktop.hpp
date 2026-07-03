#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

#define AURORA_TRAY_ICON_INFO    0
#define AURORA_TRAY_ICON_WARNING 1
#define AURORA_TRAY_ICON_ERROR   2
#define AURORA_TRAY_ICON_NONE    3

#define AURORA_WINDOW_EFFECT_NONE      0
#define AURORA_WINDOW_EFFECT_ACRYLIC   1
#define AURORA_WINDOW_EFFECT_MICA      2
#define AURORA_WINDOW_EFFECT_BLUR      3

void  aurora_desktop_init(void);
void  aurora_desktop_shutdown(void);

void* aurora_desktop_tray_create(const char* tooltip);
void  aurora_desktop_tray_destroy(void* tray);
void  aurora_desktop_tray_set_tooltip(void* tray, const char* tip);
void  aurora_desktop_tray_set_icon(void* tray, const char* path);
void  aurora_desktop_tray_add_menu_item(void* tray, int id, const char* text);
void  aurora_desktop_tray_add_menu_separator(void* tray);
void  aurora_desktop_tray_show_balloon(void* tray, const char* title, const char* text, int icon_type);
void  aurora_desktop_tray_set_callback(void* tray, void* callback);
void  aurora_desktop_tray_set_visible(void* tray, int visible);

int   aurora_desktop_notification_show(const char* title, const char* message);
void  aurora_desktop_notification_hide(void);

int   aurora_desktop_clipboard_set_text(const char* text);
char* aurora_desktop_clipboard_get_text(void);

void* aurora_desktop_drop_target_create(void* hwnd, void* callback);
void  aurora_desktop_drop_target_destroy(void* target);

int   aurora_desktop_assoc_register(const char* ext, const char* prog_id, const char* desc, const char* command);
int   aurora_desktop_assoc_unregister(const char* ext, const char* prog_id);
int   aurora_desktop_assoc_is_registered(const char* ext);

int   aurora_desktop_startup_set(const char* app_name, const char* command, int enable);
int   aurora_desktop_startup_is_enabled(const char* app_name);

int   aurora_desktop_window_set_effect(void* hwnd, int effect);
int   aurora_desktop_window_set_dark_mode(void* hwnd, int enable);
int   aurora_desktop_window_set_round_corners(void* hwnd, int enable);

int   aurora_desktop_hotkey_register(int id, int ctrl, int alt, int shift, int key, void* callback);
void  aurora_desktop_hotkey_unregister(int id);

#ifdef __cplusplus
}
#endif
