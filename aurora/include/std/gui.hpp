#pragma once
#include <cstdint>
#include "common/platform.hpp"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Widget types ── */
#define AURORA_WIDGET_WINDOW       1
#define AURORA_WIDGET_BUTTON       2
#define AURORA_WIDGET_LABEL        3
#define AURORA_WIDGET_TEXTBOX      4
#define AURORA_WIDGET_LISTBOX      5
#define AURORA_WIDGET_PASSWORDBOX  6
#define AURORA_WIDGET_CHECKBOX     7
#define AURORA_WIDGET_RADIOBUTTON  8
#define AURORA_WIDGET_SLIDER       9
#define AURORA_WIDGET_PROGRESSBAR  10
#define AURORA_WIDGET_COMBOBOX     11
#define AURORA_WIDGET_DROPDOWN     12
#define AURORA_WIDGET_TREEVIEW     13
#define AURORA_WIDGET_TABLE        14
#define AURORA_WIDGET_TABVIEW      15
#define AURORA_WIDGET_SCROLLVIEW   16
#define AURORA_WIDGET_CANVAS       17
#define AURORA_WIDGET_IMAGE        18
#define AURORA_WIDGET_TOOLBAR      19
#define AURORA_WIDGET_STATUSBAR    20
#define AURORA_WIDGET_MENUBAR      21
#define AURORA_WIDGET_SPLITVIEW    22
#define AURORA_WIDGET_SWITCH       23
#define AURORA_WIDGET_GROUPBOX     24
#define AURORA_WIDGET_DIALOG      25
#define AURORA_WIDGET_ROW         26
#define AURORA_WIDGET_COLUMN      27
#define AURORA_WIDGET_STACK       28
#define AURORA_WIDGET_GRID        29
#define AURORA_WIDGET_WRAP        30
#define AURORA_WIDGET_FLOW        31
#define AURORA_WIDGET_SPACER      32
#define AURORA_WIDGET_PADDING     33
#define AURORA_WIDGET_MARGIN      34
#define AURORA_WIDGET_CENTER      35
#define AURORA_WIDGET_ALIGN       36
#define AURORA_WIDGET_EXPAND      37
#define AURORA_WIDGET_FLEXIBLE    38
#define AURORA_WIDGET_CONTAINER   39
#define AURORA_WIDGET_DIVIDER     40
#define AURORA_WIDGET_ASPECT_RATIO 41

/* ── Layout alignment constants ── */
#define AURORA_MAIN_START          0
#define AURORA_MAIN_CENTER         1
#define AURORA_MAIN_END            2
#define AURORA_MAIN_SPACE_BETWEEN  3
#define AURORA_MAIN_SPACE_AROUND   4
#define AURORA_MAIN_SPACE_EVENLY   5
#define AURORA_CROSS_START         0
#define AURORA_CROSS_CENTER        1
#define AURORA_CROSS_END           2
#define AURORA_CROSS_STRETCH       3
#define AURORA_FIT_LOOSE           0
#define AURORA_FIT_TIGHT           1

/* ── Event types ── */
#define AURORA_EVENT_NONE      0
#define AURORA_EVENT_CLICK     1
#define AURORA_EVENT_CLOSE     2
#define AURORA_EVENT_KEY       3
#define AURORA_EVENT_CHANGE    4
#define AURORA_EVENT_SELECT    5
#define AURORA_EVENT_SCROLL    6
#define AURORA_EVENT_VALUE     7
#define AURORA_EVENT_ACTIVATE  8
#define AURORA_EVENT_MENU      9
#define AURORA_EVENT_KEY_DOWN  10
#define AURORA_EVENT_KEY_UP    11
#define AURORA_EVENT_MOUSE_MOVE 12
#define AURORA_EVENT_MOUSE_DOWN 13
#define AURORA_EVENT_MOUSE_UP  14
#define AURORA_EVENT_NOTIFY    15
#define AURORA_EVENT_TAB_CHANGE 16
#define AURORA_EVENT_TREE_SELECT 17

/* ── Cursor types ── */
#define AURORA_CURSOR_ARROW       0
#define AURORA_CURSOR_IBEAM       1
#define AURORA_CURSOR_WAIT        2
#define AURORA_CURSOR_CROSS       3
#define AURORA_CURSOR_HAND        4
#define AURORA_CURSOR_SIZE_NESW   5
#define AURORA_CURSOR_SIZE_NS     6
#define AURORA_CURSOR_SIZE_NWSE   7
#define AURORA_CURSOR_SIZE_WE     8
#define AURORA_CURSOR_SIZE_ALL    9
#define AURORA_CURSOR_NO          10

/* ── MessageBox types ── */
#define AURORA_MSGBOX_OK              0
#define AURORA_MSGBOX_OK_CANCEL       1
#define AURORA_MSGBOX_YES_NO          2
#define AURORA_MSGBOX_YES_NO_CANCEL   3
#define AURORA_MSGBOX_RETRY_CANCEL    4

/* ── MessageBox results ── */
#define AURORA_MSGBOX_RESULT_OK       1
#define AURORA_MSGBOX_RESULT_CANCEL   2
#define AURORA_MSGBOX_RESULT_YES      6
#define AURORA_MSGBOX_RESULT_NO       7
#define AURORA_MSGBOX_RESULT_RETRY    4

/* ── Orientation ── */
#define AURORA_ORIENT_HORIZONTAL      0
#define AURORA_ORIENT_VERTICAL        1

/* ── Opaque handle ── */
typedef void* AuroraWidget;

/* ── Event callback ── */
typedef void (*AuroraEventCallback)(int widget_id, int event_type, int param1, int param2);

/* ── Font info (returned by font picker) ── */
typedef struct {
    char  name[256];
    int   size;
    int   bold;
    int   italic;
    int   underline;
    int   strikethrough;
    unsigned int color;
} AuroraFontInfo;

/* ════════════════════════════════════════════════════════════
   Window
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraWidget aurora_gui_window_new(const char* title, int width, int height);
AURORA_EXPORT void         aurora_gui_window_set_title(AuroraWidget win, const char* title);
AURORA_EXPORT void         aurora_gui_window_resize(AuroraWidget win, int w, int h);
AURORA_EXPORT void         aurora_gui_window_show(AuroraWidget win);
AURORA_EXPORT void         aurora_gui_window_hide(AuroraWidget win);
AURORA_EXPORT void         aurora_gui_window_destroy(AuroraWidget win);
AURORA_EXPORT void         aurora_gui_window_maximize(AuroraWidget win);
AURORA_EXPORT void         aurora_gui_window_minimize(AuroraWidget win);
AURORA_EXPORT void         aurora_gui_window_restore(AuroraWidget win);
AURORA_EXPORT int          aurora_gui_window_get_width(AuroraWidget win);
AURORA_EXPORT int          aurora_gui_window_get_height(AuroraWidget win);
AURORA_EXPORT void         aurora_gui_window_set_min_size(AuroraWidget win, int w, int h);
AURORA_EXPORT void         aurora_gui_window_set_max_size(AuroraWidget win, int w, int h);
AURORA_EXPORT void         aurora_gui_window_set_resizable(AuroraWidget win, int resizable);

/* ════════════════════════════════════════════════════════════
   Application
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT int          aurora_gui_app_init(void);
AURORA_EXPORT void         aurora_gui_app_run(void);
AURORA_EXPORT void         aurora_gui_app_quit(void);

/* ════════════════════════════════════════════════════════════
   Generic widget operations
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT void         aurora_gui_set_callback(AuroraWidget widget, AuroraEventCallback cb);
AURORA_EXPORT void         aurora_gui_set_enabled(AuroraWidget widget, int enabled);
AURORA_EXPORT int          aurora_gui_get_enabled(AuroraWidget widget);
AURORA_EXPORT void         aurora_gui_set_visible(AuroraWidget widget, int visible);
AURORA_EXPORT int          aurora_gui_get_visible(AuroraWidget widget);
AURORA_EXPORT void         aurora_gui_set_focus(AuroraWidget widget);
AURORA_EXPORT void         aurora_gui_move(AuroraWidget widget, int x, int y, int w, int h);
AURORA_EXPORT void*        aurora_gui_get_native_handle(AuroraWidget widget);

/* ════════════════════════════════════════════════════════════
   Label / Text
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraWidget aurora_gui_label_new(AuroraWidget parent, const char* text, int x, int y, int w, int h);
AURORA_EXPORT void         aurora_gui_label_set_text(AuroraWidget lbl, const char* text);
AURORA_EXPORT const char*  aurora_gui_label_get_text(AuroraWidget lbl);
AURORA_EXPORT void         aurora_gui_label_set_font_size(AuroraWidget lbl, int size);
AURORA_EXPORT void         aurora_gui_label_set_color(AuroraWidget lbl, unsigned int color);
AURORA_EXPORT void         aurora_gui_label_set_align(AuroraWidget lbl, int align);

/* ── Text (read-only selectable text) ── */
AURORA_EXPORT AuroraWidget aurora_gui_text_new(AuroraWidget parent, const char* text, int x, int y, int w, int h);
AURORA_EXPORT void         aurora_gui_text_set_text(AuroraWidget txt, const char* text);
AURORA_EXPORT const char*  aurora_gui_text_get_text(AuroraWidget txt);

/* ════════════════════════════════════════════════════════════
   Button
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraWidget aurora_gui_button_new(AuroraWidget parent, const char* text, int x, int y, int w, int h);
AURORA_EXPORT void         aurora_gui_button_set_text(AuroraWidget btn, const char* text);
AURORA_EXPORT const char*  aurora_gui_button_get_text(AuroraWidget btn);

/* ════════════════════════════════════════════════════════════
   CheckBox
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraWidget aurora_gui_checkbox_new(AuroraWidget parent, const char* text, int x, int y, int w, int h);
AURORA_EXPORT void         aurora_gui_checkbox_set_text(AuroraWidget cb, const char* text);
AURORA_EXPORT const char*  aurora_gui_checkbox_get_text(AuroraWidget cb);
AURORA_EXPORT int          aurora_gui_checkbox_is_checked(AuroraWidget cb);
AURORA_EXPORT void         aurora_gui_checkbox_set_checked(AuroraWidget cb, int checked);

/* ════════════════════════════════════════════════════════════
   RadioButton
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraWidget aurora_gui_radiobutton_new(AuroraWidget parent, const char* text, int x, int y, int w, int h, int group_id);
AURORA_EXPORT void         aurora_gui_radiobutton_set_text(AuroraWidget rb, const char* text);
AURORA_EXPORT const char*  aurora_gui_radiobutton_get_text(AuroraWidget rb);
AURORA_EXPORT int          aurora_gui_radiobutton_is_checked(AuroraWidget rb);
AURORA_EXPORT void         aurora_gui_radiobutton_set_checked(AuroraWidget rb, int checked);

/* ════════════════════════════════════════════════════════════
   Switch (toggle)
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraWidget aurora_gui_switch_new(AuroraWidget parent, const char* text, int x, int y, int w, int h);
AURORA_EXPORT int          aurora_gui_switch_is_on(AuroraWidget sw);
AURORA_EXPORT void         aurora_gui_switch_set_on(AuroraWidget sw, int on);

/* ════════════════════════════════════════════════════════════
   TextBox
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraWidget aurora_gui_textbox_new(AuroraWidget parent, int x, int y, int w, int h);
AURORA_EXPORT void         aurora_gui_textbox_set_text(AuroraWidget tb, const char* text);
AURORA_EXPORT const char*  aurora_gui_textbox_get_text(AuroraWidget tb);
AURORA_EXPORT void         aurora_gui_textbox_set_readonly(AuroraWidget tb, int readonly);
AURORA_EXPORT void         aurora_gui_textbox_set_placeholder(AuroraWidget tb, const char* text);
AURORA_EXPORT void         aurora_gui_textbox_set_multiline(AuroraWidget tb, int multiline);
AURORA_EXPORT int          aurora_gui_textbox_get_line_count(AuroraWidget tb);

/* ════════════════════════════════════════════════════════════
   PasswordBox
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraWidget aurora_gui_passwordbox_new(AuroraWidget parent, int x, int y, int w, int h);
AURORA_EXPORT void         aurora_gui_passwordbox_set_text(AuroraWidget pb, const char* text);
AURORA_EXPORT const char*  aurora_gui_passwordbox_get_text(AuroraWidget pb);

/* ════════════════════════════════════════════════════════════
   Slider
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraWidget aurora_gui_slider_new(AuroraWidget parent, int x, int y, int w, int h, int min, int max);
AURORA_EXPORT int          aurora_gui_slider_get_value(AuroraWidget sl);
AURORA_EXPORT void         aurora_gui_slider_set_value(AuroraWidget sl, int value);
AURORA_EXPORT void         aurora_gui_slider_set_range(AuroraWidget sl, int min, int max);

/* ════════════════════════════════════════════════════════════
   ProgressBar
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraWidget aurora_gui_progressbar_new(AuroraWidget parent, int x, int y, int w, int h);
AURORA_EXPORT void         aurora_gui_progressbar_set_value(AuroraWidget pb, int value);
AURORA_EXPORT int          aurora_gui_progressbar_get_value(AuroraWidget pb);
AURORA_EXPORT void         aurora_gui_progressbar_set_range(AuroraWidget pb, int min, int max);
AURORA_EXPORT void         aurora_gui_progressbar_set_marquee(AuroraWidget pb, int on);

/* ════════════════════════════════════════════════════════════
   ComboBox
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraWidget aurora_gui_combobox_new(AuroraWidget parent, int x, int y, int w, int h);
AURORA_EXPORT void         aurora_gui_combobox_add_item(AuroraWidget cb, const char* item);
AURORA_EXPORT void         aurora_gui_combobox_clear(AuroraWidget cb);
AURORA_EXPORT int          aurora_gui_combobox_get_selected(AuroraWidget cb);
AURORA_EXPORT void         aurora_gui_combobox_set_selected(AuroraWidget cb, int idx);
AURORA_EXPORT int          aurora_gui_combobox_count(AuroraWidget cb);
AURORA_EXPORT const char*  aurora_gui_combobox_get_item(AuroraWidget cb, int idx);

/* ── DropDown (same as combobox but dropdownlist style) ── */
AURORA_EXPORT AuroraWidget aurora_gui_dropdown_new(AuroraWidget parent, int x, int y, int w, int h);
AURORA_EXPORT void         aurora_gui_dropdown_add_item(AuroraWidget dd, const char* item);
AURORA_EXPORT void         aurora_gui_dropdown_clear(AuroraWidget dd);
AURORA_EXPORT int          aurora_gui_dropdown_get_selected(AuroraWidget dd);
AURORA_EXPORT void         aurora_gui_dropdown_set_selected(AuroraWidget dd, int idx);
AURORA_EXPORT int          aurora_gui_dropdown_count(AuroraWidget dd);
AURORA_EXPORT const char*  aurora_gui_dropdown_get_item(AuroraWidget dd, int idx);

/* ════════════════════════════════════════════════════════════
   ListBox
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraWidget aurora_gui_listbox_new(AuroraWidget parent, int x, int y, int w, int h);
AURORA_EXPORT void         aurora_gui_listbox_add_item(AuroraWidget lb, const char* item);
AURORA_EXPORT void         aurora_gui_listbox_clear(AuroraWidget lb);
AURORA_EXPORT int          aurora_gui_listbox_get_selected(AuroraWidget lb);
AURORA_EXPORT const char*  aurora_gui_listbox_get_item(AuroraWidget lb, int idx);
AURORA_EXPORT int          aurora_gui_listbox_count(AuroraWidget lb);

/* ════════════════════════════════════════════════════════════
   TreeView
   ════════════════════════════════════════════════════════════ */
typedef void* AuroraTreeItem;

AURORA_EXPORT AuroraWidget   aurora_gui_treeview_new(AuroraWidget parent, int x, int y, int w, int h);
AURORA_EXPORT AuroraTreeItem aurora_gui_treeview_add_item(AuroraWidget tv, const char* text, AuroraTreeItem parent_item);
AURORA_EXPORT void           aurora_gui_treeview_remove_item(AuroraWidget tv, AuroraTreeItem item);
AURORA_EXPORT void           aurora_gui_treeview_clear(AuroraWidget tv);
AURORA_EXPORT AuroraTreeItem aurora_gui_treeview_get_selected(AuroraWidget tv);
AURORA_EXPORT void           aurora_gui_treeview_expand(AuroraWidget tv, AuroraTreeItem item);
AURORA_EXPORT void           aurora_gui_treeview_collapse(AuroraWidget tv, AuroraTreeItem item);
AURORA_EXPORT void           aurora_gui_treeview_set_item_text(AuroraWidget tv, AuroraTreeItem item, const char* text);

/* ════════════════════════════════════════════════════════════
   Table (ListView report mode)
   ════════════════════════════════════════════════════════════ */
typedef void* AuroraTableItem;

AURORA_EXPORT AuroraWidget   aurora_gui_table_new(AuroraWidget parent, int x, int y, int w, int h);
AURORA_EXPORT void           aurora_gui_table_add_column(AuroraWidget tbl, const char* title, int width);
AURORA_EXPORT int            aurora_gui_table_column_count(AuroraWidget tbl);
AURORA_EXPORT AuroraTableItem aurora_gui_table_add_row(AuroraWidget tbl);
AURORA_EXPORT void           aurora_gui_table_set_cell(AuroraWidget tbl, int row, int col, const char* text);
AURORA_EXPORT const char*    aurora_gui_table_get_cell(AuroraWidget tbl, int row, int col);
AURORA_EXPORT void           aurora_gui_table_remove_row(AuroraWidget tbl, int row);
AURORA_EXPORT void           aurora_gui_table_clear(AuroraWidget tbl);
AURORA_EXPORT int            aurora_gui_table_get_selected(AuroraWidget tbl);
AURORA_EXPORT int            aurora_gui_table_row_count(AuroraWidget tbl);

/* ════════════════════════════════════════════════════════════
   TabView
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraWidget aurora_gui_tabview_new(AuroraWidget parent, int x, int y, int w, int h);
AURORA_EXPORT AuroraWidget aurora_gui_tabview_add_page(AuroraWidget tv, const char* title);
AURORA_EXPORT int          aurora_gui_tabview_get_selected(AuroraWidget tv);
AURORA_EXPORT void         aurora_gui_tabview_set_selected(AuroraWidget tv, int idx);
AURORA_EXPORT int          aurora_gui_tabview_page_count(AuroraWidget tv);

/* ════════════════════════════════════════════════════════════
   ScrollView
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraWidget aurora_gui_scrollview_new(AuroraWidget parent, int x, int y, int w, int h);

/* ════════════════════════════════════════════════════════════
   SplitView
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraWidget aurora_gui_splitview_new(AuroraWidget parent, int x, int y, int w, int h, int orientation);
AURORA_EXPORT void         aurora_gui_splitview_set_position(AuroraWidget sv, int pos);
AURORA_EXPORT int          aurora_gui_splitview_get_position(AuroraWidget sv);
AURORA_EXPORT AuroraWidget aurora_gui_splitview_get_pane1(AuroraWidget sv);
AURORA_EXPORT AuroraWidget aurora_gui_splitview_get_pane2(AuroraWidget sv);

/* ════════════════════════════════════════════════════════════
   GroupBox
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraWidget aurora_gui_groupbox_new(AuroraWidget parent, const char* title, int x, int y, int w, int h);

/* ════════════════════════════════════════════════════════════
   Image
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraWidget aurora_gui_image_new(AuroraWidget parent, const char* path, int x, int y, int w, int h);
AURORA_EXPORT void         aurora_gui_image_load(AuroraWidget img, const char* path);
AURORA_EXPORT void         aurora_gui_image_set_data(AuroraWidget img, const unsigned char* data, int len);

/* ════════════════════════════════════════════════════════════
   Canvas (custom paint)
   ════════════════════════════════════════════════════════════ */
typedef void (*AuroraPaintCallback)(void* user_data, int x, int y, int w, int h);

AURORA_EXPORT AuroraWidget aurora_gui_canvas_new(AuroraWidget parent, int x, int y, int w, int h);
AURORA_EXPORT void         aurora_gui_canvas_set_paint_callback(AuroraWidget cv, AuroraPaintCallback cb, void* user_data);
AURORA_EXPORT void         aurora_gui_canvas_repaint(AuroraWidget cv);

/* ════════════════════════════════════════════════════════════
   Menu Bar
   ════════════════════════════════════════════════════════════ */
typedef void* AuroraMenu;

AURORA_EXPORT AuroraMenu    aurora_gui_menu_bar_new(AuroraWidget parent);
AURORA_EXPORT AuroraMenu    aurora_gui_menu_new(const char* text);
AURORA_EXPORT void          aurora_gui_menu_add_item(AuroraMenu menu, const char* text, int id);
AURORA_EXPORT void          aurora_gui_menu_add_separator(AuroraMenu menu);
AURORA_EXPORT void          aurora_gui_menu_add_submenu(AuroraMenu menu, AuroraMenu submenu);
AURORA_EXPORT void          aurora_gui_menu_bar_add_menu(AuroraMenu menubar, AuroraMenu menu);
AURORA_EXPORT void          aurora_gui_menu_set_checked(AuroraMenu menu, int id, int checked);
AURORA_EXPORT void          aurora_gui_menu_set_enabled(AuroraMenu menu, int id, int enabled);

/* ════════════════════════════════════════════════════════════
   Toolbar
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraWidget aurora_gui_toolbar_new(AuroraWidget parent, int x, int y, int w, int h);
AURORA_EXPORT void         aurora_gui_toolbar_add_button(AuroraWidget tb, const char* text, int id);
AURORA_EXPORT void         aurora_gui_toolbar_add_separator(AuroraWidget tb);

/* ════════════════════════════════════════════════════════════
   StatusBar
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraWidget aurora_gui_statusbar_new(AuroraWidget parent, int x, int y, int w, int h);
AURORA_EXPORT void         aurora_gui_statusbar_set_text(AuroraWidget sb, const char* text);
AURORA_EXPORT const char*  aurora_gui_statusbar_get_text(AuroraWidget sb);
AURORA_EXPORT void         aurora_gui_statusbar_set_parts(AuroraWidget sb, const int* widths, int count);

/* ════════════════════════════════════════════════════════════
   Dialog
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraWidget aurora_gui_dialog_new(AuroraWidget parent, const char* title, int width, int height);
AURORA_EXPORT int          aurora_gui_dialog_show_modal(AuroraWidget dlg);
AURORA_EXPORT void         aurora_gui_dialog_close(AuroraWidget dlg);

/* ════════════════════════════════════════════════════════════
   MessageBox
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT int aurora_gui_messagebox_show(AuroraWidget parent, const char* title, const char* message, int type);

/* ════════════════════════════════════════════════════════════
   FilePicker
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT const char* aurora_gui_file_open_dialog(AuroraWidget parent, const char* title, const char* filter);
AURORA_EXPORT const char* aurora_gui_file_save_dialog(AuroraWidget parent, const char* title, const char* filter);
AURORA_EXPORT const char* aurora_gui_folder_select_dialog(AuroraWidget parent, const char* title);

/* ════════════════════════════════════════════════════════════
   ColorPicker
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT int aurora_gui_color_picker_dialog(AuroraWidget parent, unsigned int initial_color);

/* ════════════════════════════════════════════════════════════
   FontPicker
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT int aurora_gui_font_picker_dialog(AuroraWidget parent, AuroraFontInfo* font_info);

/* ════════════════════════════════════════════════════════════
   Notification (tray icon)
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT int  aurora_gui_notification_show(AuroraWidget parent, const char* title, const char* message, int icon_type);
AURORA_EXPORT void aurora_gui_notification_remove(AuroraWidget parent);

/* ════════════════════════════════════════════════════════════
   Clipboard
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT int          aurora_gui_clipboard_set_text(const char* text);
AURORA_EXPORT const char*  aurora_gui_clipboard_get_text(void);

/* ════════════════════════════════════════════════════════════
   Cursor
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT void aurora_gui_cursor_set(int cursor_type);
AURORA_EXPORT int  aurora_gui_cursor_get(void);

/* ════════════════════════════════════════════════════════════
   Keyboard
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT int aurora_gui_keyboard_is_key_down(int virtual_key);
AURORA_EXPORT int aurora_gui_keyboard_get_modifiers(void);

/* ════════════════════════════════════════════════════════════
   Mouse
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT int  aurora_gui_mouse_get_x(void);
AURORA_EXPORT int  aurora_gui_mouse_get_y(void);
AURORA_EXPORT int  aurora_gui_mouse_button_down(int button);
AURORA_EXPORT void aurora_gui_mouse_set_pos(int x, int y);

/* ════════════════════════════════════════════════════════════
   Layout System — containers
   ════════════════════════════════════════════════════════════ */

/* ── Container creation ── */
AURORA_EXPORT AuroraWidget aurora_gui_row_new(AuroraWidget parent, int x, int y, int w, int h);
AURORA_EXPORT AuroraWidget aurora_gui_column_new(AuroraWidget parent, int x, int y, int w, int h);
AURORA_EXPORT AuroraWidget aurora_gui_stack_new(AuroraWidget parent, int x, int y, int w, int h);
AURORA_EXPORT AuroraWidget aurora_gui_grid_new(AuroraWidget parent, int x, int y, int w, int h, int columns);
AURORA_EXPORT AuroraWidget aurora_gui_wrap_new(AuroraWidget parent, int x, int y, int w, int h);
AURORA_EXPORT AuroraWidget aurora_gui_flow_new(AuroraWidget parent, int x, int y, int w, int h);

/* ── Child management ── */
AURORA_EXPORT void aurora_gui_layout_add_child(AuroraWidget layout, AuroraWidget child);
AURORA_EXPORT void aurora_gui_layout_remove_child(AuroraWidget layout, int index);
AURORA_EXPORT void aurora_gui_layout_clear(AuroraWidget layout);
AURORA_EXPORT int  aurora_gui_layout_child_count(AuroraWidget layout);
AURORA_EXPORT void aurora_gui_layout_recalc(AuroraWidget layout);

/* ── Layout properties ── */
AURORA_EXPORT void aurora_gui_layout_set_main_align(AuroraWidget layout, int align);
AURORA_EXPORT void aurora_gui_layout_set_cross_align(AuroraWidget layout, int align);
AURORA_EXPORT void aurora_gui_layout_set_spacing(AuroraWidget layout, int spacing);
AURORA_EXPORT void aurora_gui_layout_child_set_flex(AuroraWidget layout, AuroraWidget child, int flex);
AURORA_EXPORT void aurora_gui_layout_child_set_fit(AuroraWidget layout, AuroraWidget child, int fit);

/* ── Grid child position ── */
AURORA_EXPORT void aurora_gui_grid_set_child_pos(AuroraWidget grid, AuroraWidget child, int col, int row, int colspan, int rowspan);

/* ════════════════════════════════════════════════════════════
   Layout System — single-child wrappers
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraWidget aurora_gui_spacer_new(AuroraWidget parent, int flex);
AURORA_EXPORT AuroraWidget aurora_gui_padding_new(AuroraWidget parent, AuroraWidget child, int left, int top, int right, int bottom);
AURORA_EXPORT AuroraWidget aurora_gui_margin_new(AuroraWidget parent, AuroraWidget child, int left, int top, int right, int bottom);
AURORA_EXPORT AuroraWidget aurora_gui_center_new(AuroraWidget parent, AuroraWidget child);
AURORA_EXPORT AuroraWidget aurora_gui_align_new(AuroraWidget parent, AuroraWidget child, int align_x, int align_y);
AURORA_EXPORT AuroraWidget aurora_gui_expand_new(AuroraWidget parent, AuroraWidget child, int flex);
AURORA_EXPORT AuroraWidget aurora_gui_flexible_new(AuroraWidget parent, AuroraWidget child, int flex);
AURORA_EXPORT AuroraWidget aurora_gui_container_new(AuroraWidget parent, AuroraWidget child);
AURORA_EXPORT void         aurora_gui_container_set_padding(AuroraWidget c, int l, int t, int r, int b);
AURORA_EXPORT void         aurora_gui_container_set_margin(AuroraWidget c, int l, int t, int r, int b);
AURORA_EXPORT void         aurora_gui_container_set_bg(AuroraWidget c, unsigned int color);
AURORA_EXPORT AuroraWidget aurora_gui_divider_new(AuroraWidget parent, int orientation, int thickness, int x, int y, int w, int h);
AURORA_EXPORT AuroraWidget aurora_gui_aspect_ratio_new(AuroraWidget parent, AuroraWidget child, float ratio);

/* ════════════════════════════════════════════════════════════
   Layout helpers (legacy)
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT void aurora_gui_layout_horizontal(AuroraWidget parent, int margin);
AURORA_EXPORT void aurora_gui_layout_vertical(AuroraWidget parent, int margin);

/* ── Align constants ── */
#define AURORA_ALIGN_LEFT    0
#define AURORA_ALIGN_CENTER  1
#define AURORA_ALIGN_RIGHT   2

#ifdef __cplusplus
}
#endif
