#pragma once
#include <cstdint>
#include "common/platform.hpp"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* AuroraApp;
typedef void* AuroraWidget;

typedef void (*AuroraAppEventCallback)(int widget_id, int event_type, int param1, int param2);

/* ── App Lifecycle ── */
AURORA_EXPORT AuroraApp    aurora_app_init(const char* title, int width, int height);
AURORA_EXPORT void         aurora_app_run(AuroraApp app);
AURORA_EXPORT void         aurora_app_quit(AuroraApp app);

/* ── Windows ── */
AURORA_EXPORT AuroraWidget aurora_app_window_new(AuroraApp app, const char* title, int x, int y, int w, int h);

/* ── Widgets (absolute positioning) ── */
AURORA_EXPORT AuroraWidget aurora_app_button_new(AuroraWidget parent, const char* text, int x, int y, int w, int h);
AURORA_EXPORT AuroraWidget aurora_app_label_new(AuroraWidget parent, const char* text, int x, int y, int w, int h);
AURORA_EXPORT AuroraWidget aurora_app_textbox_new(AuroraWidget parent, int x, int y, int w, int h);
AURORA_EXPORT AuroraWidget aurora_app_listbox_new(AuroraWidget parent, int x, int y, int w, int h);
AURORA_EXPORT AuroraWidget aurora_app_checkbox_new(AuroraWidget parent, const char* text, int x, int y, int w, int h);
AURORA_EXPORT AuroraWidget aurora_app_slider_new(AuroraWidget parent, int x, int y, int w, int h, int min, int max);
AURORA_EXPORT AuroraWidget aurora_app_progressbar_new(AuroraWidget parent, int x, int y, int w, int h);
AURORA_EXPORT AuroraWidget aurora_app_image_new(AuroraWidget parent, const char* path, int x, int y, int w, int h);
AURORA_EXPORT AuroraWidget aurora_app_combobox_new(AuroraWidget parent, int x, int y, int w, int h);
AURORA_EXPORT AuroraWidget aurora_app_switch_new(AuroraWidget parent, const char* text, int x, int y, int w, int h);

/* ── Widget Properties ── */
AURORA_EXPORT void         aurora_app_set_text(AuroraWidget widget, const char* text);
AURORA_EXPORT const char*  aurora_app_get_text(AuroraWidget widget);
AURORA_EXPORT void         aurora_app_listbox_add(AuroraWidget listbox, const char* item);
AURORA_EXPORT void         aurora_app_listbox_clear(AuroraWidget listbox);
AURORA_EXPORT int          aurora_app_listbox_count(AuroraWidget listbox);
AURORA_EXPORT int          aurora_app_listbox_selected(AuroraWidget listbox);
AURORA_EXPORT void         aurora_app_combobox_add(AuroraWidget combobox, const char* item);
AURORA_EXPORT int          aurora_app_slider_value(AuroraWidget slider);
AURORA_EXPORT void         aurora_app_slider_set(AuroraWidget slider, int value);
AURORA_EXPORT void         aurora_app_progress_set(AuroraWidget progress, int value);
AURORA_EXPORT int          aurora_app_checkbox_checked(AuroraWidget checkbox);
AURORA_EXPORT void         aurora_app_checkbox_set(AuroraWidget checkbox, int checked);
AURORA_EXPORT int          aurora_app_switch_on(AuroraWidget sw);
AURORA_EXPORT void         aurora_app_switch_set(AuroraWidget sw, int on);
AURORA_EXPORT void         aurora_app_set_bg(AuroraWidget widget, unsigned int color);
AURORA_EXPORT void         aurora_app_set_font_size(AuroraWidget widget, int size);
AURORA_EXPORT void         aurora_app_set_text_align(AuroraWidget widget, int align);
AURORA_EXPORT void         aurora_app_set_callback(AuroraWidget widget, AuroraAppEventCallback cb);
AURORA_EXPORT void         aurora_app_set_on_click(AuroraWidget widget, AuroraAppEventCallback cb);
AURORA_EXPORT void         aurora_app_set_on_change(AuroraWidget widget, AuroraAppEventCallback cb);

/* ── App sub-system accessors ── */
AURORA_EXPORT void* aurora_app_get_layout(AuroraApp app);
AURORA_EXPORT void* aurora_app_get_nav(AuroraApp app);
AURORA_EXPORT void* aurora_app_get_theme(AuroraApp app);

/* ════════════════════════════════════════════════════════════
   Layout System — flexbox-style
   ════════════════════════════════════════════════════════════ */

AURORA_EXPORT void* aurora_app_layout_create(void);
AURORA_EXPORT void  aurora_app_layout_destroy(void* layout);
AURORA_EXPORT void* aurora_app_layout_node_new(void* parent, int type);
AURORA_EXPORT void  aurora_app_layout_node_set_flex(void* node, int grow, int shrink, int basis);
AURORA_EXPORT void  aurora_app_layout_node_set_direction(void* node, int direction);
AURORA_EXPORT void  aurora_app_layout_node_set_justify(void* node, int justify);
AURORA_EXPORT void  aurora_app_layout_node_set_align(void* node, int align);
AURORA_EXPORT void  aurora_app_layout_node_set_wrap(void* node, int wrap);
AURORA_EXPORT void  aurora_app_layout_node_set_gap(void* node, float gap);
AURORA_EXPORT void  aurora_app_layout_node_set_size(void* node, float w, float h);
AURORA_EXPORT void  aurora_app_layout_node_set_min_size(void* node, float w, float h);
AURORA_EXPORT void  aurora_app_layout_node_set_max_size(void* node, float w, float h);
AURORA_EXPORT void  aurora_app_layout_node_set_padding(void* node, float l, float t, float r, float b);
AURORA_EXPORT void  aurora_app_layout_node_set_margin(void* node, float l, float t, float r, float b);
AURORA_EXPORT void  aurora_app_layout_calculate(void* root, float width, float height);
AURORA_EXPORT float aurora_app_layout_node_get_x(void* node);
AURORA_EXPORT float aurora_app_layout_node_get_y(void* node);
AURORA_EXPORT float aurora_app_layout_node_get_w(void* node);
AURORA_EXPORT float aurora_app_layout_node_get_h(void* node);

/* ════════════════════════════════════════════════════════════
   Navigation System
   ════════════════════════════════════════════════════════════ */

AURORA_EXPORT void*       aurora_app_nav_init(void);
AURORA_EXPORT void        aurora_app_nav_destroy(void* nav);
AURORA_EXPORT void        aurora_app_nav_register(void* nav, const char* name, void* screen);
AURORA_EXPORT int         aurora_app_nav_push(void* nav, const char* name);
AURORA_EXPORT int         aurora_app_nav_pop(void* nav);
AURORA_EXPORT int         aurora_app_nav_replace(void* nav, const char* name);
AURORA_EXPORT const char* aurora_app_nav_current(void* nav);
AURORA_EXPORT int         aurora_app_nav_depth(void* nav);
AURORA_EXPORT void        aurora_app_nav_set_on_change(void* nav, void (*cb)(const char*, const char*));

/* ════════════════════════════════════════════════════════════
   Theme System
   ════════════════════════════════════════════════════════════ */

#define APP_THEME_PRIMARY        0
#define APP_THEME_SECONDARY      1
#define APP_THEME_BACKGROUND     2
#define APP_THEME_SURFACE        3
#define APP_THEME_ERROR          4
#define APP_THEME_ON_PRIMARY     5
#define APP_THEME_ON_SECONDARY   6
#define APP_THEME_ON_BACKGROUND  7
#define APP_THEME_ON_SURFACE     8
#define APP_THEME_ON_ERROR       9
#define APP_THEME_OUTLINE        10

#define APP_THEME_FONT_HEADLINE  0
#define APP_THEME_FONT_TITLE     1
#define APP_THEME_FONT_BODY      2
#define APP_THEME_FONT_LABEL     3
#define APP_THEME_FONT_CAPTION   4

#define APP_THEME_SPACING_XS     0
#define APP_THEME_SPACING_SM     1
#define APP_THEME_SPACING_MD     2
#define APP_THEME_SPACING_LG     3
#define APP_THEME_SPACING_XL     4

AURORA_EXPORT void*       aurora_app_theme_init(void);
AURORA_EXPORT void        aurora_app_theme_destroy(void* theme);
AURORA_EXPORT void        aurora_app_theme_set_mode(void* theme, int mode);
AURORA_EXPORT int         aurora_app_theme_get_mode(void* theme);
AURORA_EXPORT void        aurora_app_theme_set_color(void* theme, int key, unsigned int color);
AURORA_EXPORT unsigned int aurora_app_theme_get_color(void* theme, int key);
AURORA_EXPORT void        aurora_app_theme_set_font(void* theme, int key, int size);
AURORA_EXPORT int         aurora_app_theme_get_font(void* theme, int key);
AURORA_EXPORT float       aurora_app_theme_get_spacing(void* theme, int level);

#ifdef __cplusplus
}
#endif
