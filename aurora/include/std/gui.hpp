#pragma once
#include <cstdint>

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
AuroraWidget aurora_gui_window_new(const char* title, int width, int height);
void         aurora_gui_window_set_title(AuroraWidget win, const char* title);
void         aurora_gui_window_resize(AuroraWidget win, int w, int h);
void         aurora_gui_window_show(AuroraWidget win);
void         aurora_gui_window_hide(AuroraWidget win);
void         aurora_gui_window_destroy(AuroraWidget win);

/* ── Button ── */
AuroraWidget aurora_gui_button_new(AuroraWidget parent, const char* text, int x, int y, int w, int h);
void         aurora_gui_button_set_text(AuroraWidget btn, const char* text);
const char*  aurora_gui_button_get_text(AuroraWidget btn);

/* ── Label ── */
AuroraWidget aurora_gui_label_new(AuroraWidget parent, const char* text, int x, int y, int w, int h);
void         aurora_gui_label_set_text(AuroraWidget lbl, const char* text);
const char*  aurora_gui_label_get_text(AuroraWidget lbl);

/* ── TextBox ── */
AuroraWidget aurora_gui_textbox_new(AuroraWidget parent, int x, int y, int w, int h);
void         aurora_gui_textbox_set_text(AuroraWidget tb, const char* text);
const char*  aurora_gui_textbox_get_text(AuroraWidget tb);

/* ── ListBox ── */
AuroraWidget aurora_gui_listbox_new(AuroraWidget parent, int x, int y, int w, int h);
void         aurora_gui_listbox_add_item(AuroraWidget lb, const char* item);
void         aurora_gui_listbox_clear(AuroraWidget lb);
int          aurora_gui_listbox_get_selected(AuroraWidget lb);
const char*  aurora_gui_listbox_get_item(AuroraWidget lb, int idx);
int          aurora_gui_listbox_count(AuroraWidget lb);

/* ── Event loop ── */
void aurora_gui_set_callback(AuroraWidget widget, AuroraEventCallback cb);
void aurora_gui_run(void);
void aurora_gui_quit(void);

/* ── Layout helpers ── */
void aurora_gui_layout_horizontal(AuroraWidget parent, int margin);
void aurora_gui_layout_vertical(AuroraWidget parent, int margin);

#ifdef __cplusplus
}
#endif
