#pragma once
#include <cstdint>
#include "common/platform.hpp"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Widget types ── */
#define AURORA_WIDGET_WINDOW  1
#define AURORA_WIDGET_BUTTON  2
#define AURORA_WIDGET_LABEL   3
#define AURORA_WIDGET_TEXTBOX 4
#define AURORA_WIDGET_LISTBOX 5

/* ── Event types ── */
#define AURORA_EVENT_NONE     0
#define AURORA_EVENT_CLICK    1
#define AURORA_EVENT_CLOSE    2
#define AURORA_EVENT_KEY      3
#define AURORA_EVENT_CHANGE   4
#define AURORA_EVENT_SELECT   5

/* ── Opaque handle ── */
typedef void* AuroraWidget;

/* ── Event callback ── */
typedef void (*AuroraEventCallback)(int widget_id, int event_type, int param1, int param2);

/* ── Window ── */
AURORA_EXPORT AuroraWidget aurora_gui_window_new(const char* title, int width, int height);
AURORA_EXPORT void         aurora_gui_window_set_title(AuroraWidget win, const char* title);
AURORA_EXPORT void         aurora_gui_window_resize(AuroraWidget win, int w, int h);
AURORA_EXPORT void         aurora_gui_window_show(AuroraWidget win);
AURORA_EXPORT void         aurora_gui_window_hide(AuroraWidget win);
AURORA_EXPORT void         aurora_gui_window_destroy(AuroraWidget win);

/* ── Button ── */
AURORA_EXPORT AuroraWidget aurora_gui_button_new(AuroraWidget parent, const char* text, int x, int y, int w, int h);
AURORA_EXPORT void         aurora_gui_button_set_text(AuroraWidget btn, const char* text);
AURORA_EXPORT const char*  aurora_gui_button_get_text(AuroraWidget btn);

/* ── Label ── */
AURORA_EXPORT AuroraWidget aurora_gui_label_new(AuroraWidget parent, const char* text, int x, int y, int w, int h);
AURORA_EXPORT void         aurora_gui_label_set_text(AuroraWidget lbl, const char* text);
AURORA_EXPORT const char*  aurora_gui_label_get_text(AuroraWidget lbl);

/* ── TextBox ── */
AURORA_EXPORT AuroraWidget aurora_gui_textbox_new(AuroraWidget parent, int x, int y, int w, int h);
AURORA_EXPORT void         aurora_gui_textbox_set_text(AuroraWidget tb, const char* text);
AURORA_EXPORT const char*  aurora_gui_textbox_get_text(AuroraWidget tb);

/* ── ListBox ── */
AURORA_EXPORT AuroraWidget aurora_gui_listbox_new(AuroraWidget parent, int x, int y, int w, int h);
AURORA_EXPORT void         aurora_gui_listbox_add_item(AuroraWidget lb, const char* item);
AURORA_EXPORT void         aurora_gui_listbox_clear(AuroraWidget lb);
AURORA_EXPORT int          aurora_gui_listbox_get_selected(AuroraWidget lb);
AURORA_EXPORT const char*  aurora_gui_listbox_get_item(AuroraWidget lb, int idx);
AURORA_EXPORT int          aurora_gui_listbox_count(AuroraWidget lb);

/* ── Event loop ── */
AURORA_EXPORT void aurora_gui_set_callback(AuroraWidget widget, AuroraEventCallback cb);
AURORA_EXPORT void aurora_gui_run(void);
AURORA_EXPORT void aurora_gui_quit(void);

/* ── Layout helpers ── */
AURORA_EXPORT void aurora_gui_layout_horizontal(AuroraWidget parent, int margin);
AURORA_EXPORT void aurora_gui_layout_vertical(AuroraWidget parent, int margin);

#ifdef __cplusplus
}
#endif
