/* ════════════════════════════════════════════════════════════
   gui.cpp — Platform dispatcher for native GUI
   ════════════════════════════════════════════════════════════ */

#if defined(_WIN32)
  #include "gui_win32.cpp"
#elif defined(__linux__)

#include "../../include/std/gui.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>
#include <map>

struct GuiWidget {
    int id, type, x, y, w, h;
    std::string text;
    std::vector<std::string> items;
    int selected_idx, min_val, max_val, group_id;
    AuroraEventCallback callback;
    GuiWidget* parent;
    unsigned long xwindow;
    void* gc;
    int is_visible;
    void* extra_data;
};

static std::vector<GuiWidget*> g_widgets;
static int g_next_id = 1;
static bool g_running = false;
static std::map<int, GuiWidget*> g_id_map;
static char g_temp_str[4096];

static void* g_display = nullptr;
static int g_screen;
static unsigned long g_root;

static GuiWidget* widget_new(int type, GuiWidget* parent) {
    GuiWidget* w = new GuiWidget();
    w->id = g_next_id++; w->type = type;
    w->x = w->y = w->w = w->h = 0; w->selected_idx = -1;
    w->min_val = 0; w->max_val = 100; w->group_id = 0;
    w->callback = nullptr; w->parent = parent; w->extra_data = nullptr;
    w->xwindow = 0; w->gc = nullptr; w->is_visible = 0;
    g_widgets.push_back(w); g_id_map[w->id] = w;
    return w;
}

/* ── All X11 stubs ── */
int  aurora_gui_app_init(void) { return 0; }
void aurora_gui_app_run(void) { }
void aurora_gui_app_quit(void) { }
void aurora_gui_set_enabled(AuroraWidget w, int e) { (void)w;(void)e; }
int  aurora_gui_get_enabled(AuroraWidget w) { (void)w; return 1; }
void aurora_gui_set_visible(AuroraWidget w, int v) { (void)w;(void)v; }
int  aurora_gui_get_visible(AuroraWidget w) { (void)w; return 1; }
void aurora_gui_set_focus(AuroraWidget w) { (void)w; }
void aurora_gui_move(AuroraWidget w, int x, int y, int w_, int h_) { (void)w;(void)x;(void)y;(void)w_;(void)h_; }
AuroraWidget aurora_gui_window_new(const char* t, int w, int h) { (void)t;(void)w;(void)h; return nullptr; }
void aurora_gui_window_set_title(AuroraWidget w, const char* t) { (void)w;(void)t; }
void aurora_gui_window_resize(AuroraWidget w, int w_, int h_) { (void)w;(void)w_;(void)h_; }
void aurora_gui_window_show(AuroraWidget w) { (void)w; }
void aurora_gui_window_hide(AuroraWidget w) { (void)w; }
void aurora_gui_window_destroy(AuroraWidget w) { (void)w; }
void aurora_gui_window_maximize(AuroraWidget w) { (void)w; }
void aurora_gui_window_minimize(AuroraWidget w) { (void)w; }
void aurora_gui_window_restore(AuroraWidget w) { (void)w; }
int  aurora_gui_window_get_width(AuroraWidget w) { (void)w; return 0; }
int  aurora_gui_window_get_height(AuroraWidget w) { (void)w; return 0; }
void aurora_gui_window_set_min_size(AuroraWidget w, int w_, int h_) { (void)w;(void)w_;(void)h_; }
void aurora_gui_window_set_max_size(AuroraWidget w, int w_, int h_) { (void)w;(void)w_;(void)h_; }
void aurora_gui_window_set_resizable(AuroraWidget w, int r) { (void)w;(void)r; }
AuroraWidget aurora_gui_label_new(AuroraWidget p, const char* t, int x, int y, int w, int h) { (void)p;(void)t;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_label_set_text(AuroraWidget l, const char* t) { (void)l;(void)t; }
const char* aurora_gui_label_get_text(AuroraWidget l) { (void)l; return ""; }
void aurora_gui_label_set_font_size(AuroraWidget l, int s) { (void)l;(void)s; }
void aurora_gui_label_set_color(AuroraWidget l, unsigned int c) { (void)l;(void)c; }
void aurora_gui_label_set_align(AuroraWidget l, int a) { (void)l;(void)a; }
AuroraWidget aurora_gui_text_new(AuroraWidget p, const char* t, int x, int y, int w, int h) { (void)p;(void)t;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_text_set_text(AuroraWidget t, const char* s) { (void)t;(void)s; }
const char* aurora_gui_text_get_text(AuroraWidget t) { (void)t; return ""; }
AuroraWidget aurora_gui_button_new(AuroraWidget p, const char* t, int x, int y, int w, int h) { (void)p;(void)t;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_button_set_text(AuroraWidget b, const char* t) { (void)b;(void)t; }
const char* aurora_gui_button_get_text(AuroraWidget b) { (void)b; return ""; }
AuroraWidget aurora_gui_checkbox_new(AuroraWidget p, const char* t, int x, int y, int w, int h) { (void)p;(void)t;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_checkbox_set_text(AuroraWidget c, const char* t) { (void)c;(void)t; }
const char* aurora_gui_checkbox_get_text(AuroraWidget c) { (void)c; return ""; }
int  aurora_gui_checkbox_is_checked(AuroraWidget c) { (void)c; return 0; }
void aurora_gui_checkbox_set_checked(AuroraWidget c, int v) { (void)c;(void)v; }
AuroraWidget aurora_gui_radiobutton_new(AuroraWidget p, const char* t, int x, int y, int w, int h, int g) { (void)p;(void)t;(void)x;(void)y;(void)w;(void)h;(void)g; return nullptr; }
void aurora_gui_radiobutton_set_text(AuroraWidget r, const char* t) { (void)r;(void)t; }
const char* aurora_gui_radiobutton_get_text(AuroraWidget r) { (void)r; return ""; }
int  aurora_gui_radiobutton_is_checked(AuroraWidget r) { (void)r; return 0; }
void aurora_gui_radiobutton_set_checked(AuroraWidget r, int v) { (void)r;(void)v; }
AuroraWidget aurora_gui_switch_new(AuroraWidget p, const char* t, int x, int y, int w, int h) { (void)p;(void)t;(void)x;(void)y;(void)w;(void)h; return nullptr; }
int  aurora_gui_switch_is_on(AuroraWidget s) { (void)s; return 0; }
void aurora_gui_switch_set_on(AuroraWidget s, int v) { (void)s;(void)v; }
AuroraWidget aurora_gui_textbox_new(AuroraWidget p, int x, int y, int w, int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_textbox_set_text(AuroraWidget t, const char* s) { (void)t;(void)s; }
const char* aurora_gui_textbox_get_text(AuroraWidget t) { (void)t; return ""; }
void aurora_gui_textbox_set_readonly(AuroraWidget t, int r) { (void)t;(void)r; }
void aurora_gui_textbox_set_placeholder(AuroraWidget t, const char* s) { (void)t;(void)s; }
void aurora_gui_textbox_set_multiline(AuroraWidget t, int m) { (void)t;(void)m; }
int  aurora_gui_textbox_get_line_count(AuroraWidget t) { (void)t; return 0; }
AuroraWidget aurora_gui_passwordbox_new(AuroraWidget p, int x, int y, int w, int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_passwordbox_set_text(AuroraWidget p, const char* t) { (void)p;(void)t; }
const char* aurora_gui_passwordbox_get_text(AuroraWidget p) { (void)p; return ""; }
AuroraWidget aurora_gui_slider_new(AuroraWidget p, int x, int y, int w, int h, int mn, int mx) { (void)p;(void)x;(void)y;(void)w;(void)h;(void)mn;(void)mx; return nullptr; }
int  aurora_gui_slider_get_value(AuroraWidget s) { (void)s; return 0; }
void aurora_gui_slider_set_value(AuroraWidget s, int v) { (void)s;(void)v; }
void aurora_gui_slider_set_range(AuroraWidget s, int mn, int mx) { (void)s;(void)mn;(void)mx; }
AuroraWidget aurora_gui_progressbar_new(AuroraWidget p, int x, int y, int w, int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_progressbar_set_value(AuroraWidget p, int v) { (void)p;(void)v; }
int  aurora_gui_progressbar_get_value(AuroraWidget p) { (void)p; return 0; }
void aurora_gui_progressbar_set_range(AuroraWidget p, int mn, int mx) { (void)p;(void)mn;(void)mx; }
void aurora_gui_progressbar_set_marquee(AuroraWidget p, int v) { (void)p;(void)v; }
AuroraWidget aurora_gui_combobox_new(AuroraWidget p, int x, int y, int w, int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_combobox_add_item(AuroraWidget c, const char* i) { (void)c;(void)i; }
void aurora_gui_combobox_clear(AuroraWidget c) { (void)c; }
int  aurora_gui_combobox_get_selected(AuroraWidget c) { (void)c; return -1; }
void aurora_gui_combobox_set_selected(AuroraWidget c, int i) { (void)c;(void)i; }
int  aurora_gui_combobox_count(AuroraWidget c) { (void)c; return 0; }
const char* aurora_gui_combobox_get_item(AuroraWidget c, int i) { (void)c;(void)i; return nullptr; }
AuroraWidget aurora_gui_dropdown_new(AuroraWidget p, int x, int y, int w, int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_dropdown_add_item(AuroraWidget d, const char* i) { (void)d;(void)i; }
void aurora_gui_dropdown_clear(AuroraWidget d) { (void)d; }
int  aurora_gui_dropdown_get_selected(AuroraWidget d) { (void)d; return -1; }
void aurora_gui_dropdown_set_selected(AuroraWidget d, int i) { (void)d;(void)i; }
int  aurora_gui_dropdown_count(AuroraWidget d) { (void)d; return 0; }
const char* aurora_gui_dropdown_get_item(AuroraWidget d, int i) { (void)d;(void)i; return nullptr; }
AuroraWidget aurora_gui_listbox_new(AuroraWidget p, int x, int y, int w, int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_listbox_add_item(AuroraWidget l, const char* i) { (void)l;(void)i; }
void aurora_gui_listbox_clear(AuroraWidget l) { (void)l; }
int  aurora_gui_listbox_get_selected(AuroraWidget l) { (void)l; return -1; }
const char* aurora_gui_listbox_get_item(AuroraWidget l, int i) { (void)l;(void)i; return nullptr; }
int  aurora_gui_listbox_count(AuroraWidget l) { (void)l; return 0; }
AuroraWidget aurora_gui_treeview_new(AuroraWidget p, int x, int y, int w, int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
AuroraTreeItem aurora_gui_treeview_add_item(AuroraWidget t, const char* s, AuroraTreeItem p) { (void)t;(void)s;(void)p; return nullptr; }
void aurora_gui_treeview_remove_item(AuroraWidget t, AuroraTreeItem i) { (void)t;(void)i; }
void aurora_gui_treeview_clear(AuroraWidget t) { (void)t; }
AuroraTreeItem aurora_gui_treeview_get_selected(AuroraWidget t) { (void)t; return nullptr; }
void aurora_gui_treeview_expand(AuroraWidget t, AuroraTreeItem i) { (void)t;(void)i; }
void aurora_gui_treeview_collapse(AuroraWidget t, AuroraTreeItem i) { (void)t;(void)i; }
void aurora_gui_treeview_set_item_text(AuroraWidget t, AuroraTreeItem i, const char* s) { (void)t;(void)i;(void)s; }
AuroraWidget aurora_gui_table_new(AuroraWidget p, int x, int y, int w, int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_table_add_column(AuroraWidget t, const char* s, int w) { (void)t;(void)s;(void)w; }
int  aurora_gui_table_column_count(AuroraWidget t) { (void)t; return 0; }
AuroraTableItem aurora_gui_table_add_row(AuroraWidget t) { (void)t; return nullptr; }
void aurora_gui_table_set_cell(AuroraWidget t, int r, int c, const char* s) { (void)t;(void)r;(void)c;(void)s; }
const char* aurora_gui_table_get_cell(AuroraWidget t, int r, int c) { (void)t;(void)r;(void)c; return ""; }
void aurora_gui_table_remove_row(AuroraWidget t, int r) { (void)t;(void)r; }
void aurora_gui_table_clear(AuroraWidget t) { (void)t; }
int  aurora_gui_table_get_selected(AuroraWidget t) { (void)t; return -1; }
int  aurora_gui_table_row_count(AuroraWidget t) { (void)t; return 0; }
AuroraWidget aurora_gui_tabview_new(AuroraWidget p, int x, int y, int w, int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
AuroraWidget aurora_gui_tabview_add_page(AuroraWidget t, const char* s) { (void)t;(void)s; return nullptr; }
int  aurora_gui_tabview_get_selected(AuroraWidget t) { (void)t; return -1; }
void aurora_gui_tabview_set_selected(AuroraWidget t, int i) { (void)t;(void)i; }
int  aurora_gui_tabview_page_count(AuroraWidget t) { (void)t; return 0; }
AuroraWidget aurora_gui_scrollview_new(AuroraWidget p, int x, int y, int w, int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
AuroraWidget aurora_gui_splitview_new(AuroraWidget p, int x, int y, int w, int h, int o) { (void)p;(void)x;(void)y;(void)w;(void)h;(void)o; return nullptr; }
void aurora_gui_splitview_set_position(AuroraWidget s, int p) { (void)s;(void)p; }
int  aurora_gui_splitview_get_position(AuroraWidget s) { (void)s; return 0; }
AuroraWidget aurora_gui_splitview_get_pane1(AuroraWidget s) { (void)s; return nullptr; }
AuroraWidget aurora_gui_splitview_get_pane2(AuroraWidget s) { (void)s; return nullptr; }
AuroraWidget aurora_gui_groupbox_new(AuroraWidget p, const char* s, int x, int y, int w, int h) { (void)p;(void)s;(void)x;(void)y;(void)w;(void)h; return nullptr; }
AuroraWidget aurora_gui_image_new(AuroraWidget p, const char* s, int x, int y, int w, int h) { (void)p;(void)s;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_image_load(AuroraWidget i, const char* s) { (void)i;(void)s; }
void aurora_gui_image_set_data(AuroraWidget i, const unsigned char* d, int l) { (void)i;(void)d;(void)l; }
AuroraWidget aurora_gui_canvas_new(AuroraWidget p, int x, int y, int w, int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_canvas_set_paint_callback(AuroraWidget c, AuroraPaintCallback cb, void* u) { (void)c;(void)cb;(void)u; }
void aurora_gui_canvas_repaint(AuroraWidget c) { (void)c; }
AuroraMenu aurora_gui_menu_bar_new(AuroraWidget p) { (void)p; return nullptr; }
AuroraMenu aurora_gui_menu_new(const char* s) { (void)s; return nullptr; }
void aurora_gui_menu_add_item(AuroraMenu m, const char* s, int i) { (void)m;(void)s;(void)i; }
void aurora_gui_menu_add_separator(AuroraMenu m) { (void)m; }
void aurora_gui_menu_add_submenu(AuroraMenu m, AuroraMenu s) { (void)m;(void)s; }
void aurora_gui_menu_bar_add_menu(AuroraMenu m, AuroraMenu s) { (void)m;(void)s; }
void aurora_gui_menu_set_checked(AuroraMenu m, int i, int c) { (void)m;(void)i;(void)c; }
void aurora_gui_menu_set_enabled(AuroraMenu m, int i, int e) { (void)m;(void)i;(void)e; }
AuroraWidget aurora_gui_toolbar_new(AuroraWidget p, int x, int y, int w, int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_toolbar_add_button(AuroraWidget t, const char* s, int i) { (void)t;(void)s;(void)i; }
void aurora_gui_toolbar_add_separator(AuroraWidget t) { (void)t; }
AuroraWidget aurora_gui_statusbar_new(AuroraWidget p, int x, int y, int w, int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_statusbar_set_text(AuroraWidget s, const char* t) { (void)s;(void)t; }
const char* aurora_gui_statusbar_get_text(AuroraWidget s) { (void)s; return ""; }
void aurora_gui_statusbar_set_parts(AuroraWidget s, const int* w, int c) { (void)s;(void)w;(void)c; }
AuroraWidget aurora_gui_dialog_new(AuroraWidget p, const char* s, int w, int h) { (void)p;(void)s;(void)w;(void)h; return nullptr; }
int  aurora_gui_dialog_show_modal(AuroraWidget d) { (void)d; return 0; }
void aurora_gui_dialog_close(AuroraWidget d) { (void)d; }
int aurora_gui_messagebox_show(AuroraWidget p, const char* s, const char* m, int t) { (void)p;(void)s;(void)m;(void)t; return 0; }
const char* aurora_gui_file_open_dialog(AuroraWidget p, const char* s, const char* f) { (void)p;(void)s;(void)f; return nullptr; }
const char* aurora_gui_file_save_dialog(AuroraWidget p, const char* s, const char* f) { (void)p;(void)s;(void)f; return nullptr; }
const char* aurora_gui_folder_select_dialog(AuroraWidget p, const char* s) { (void)p;(void)s; return nullptr; }
int aurora_gui_color_picker_dialog(AuroraWidget p, unsigned int c) { (void)p;(void)c; return -1; }
int aurora_gui_font_picker_dialog(AuroraWidget p, AuroraFontInfo* f) { (void)p;(void)f; return 0; }
int  aurora_gui_notification_show(AuroraWidget p, const char* s, const char* m, int i) { (void)p;(void)s;(void)m;(void)i; return -1; }
void aurora_gui_notification_remove(AuroraWidget p) { (void)p; }
int  aurora_gui_clipboard_set_text(const char* s) { (void)s; return 0; }
const char* aurora_gui_clipboard_get_text(void) { return nullptr; }
void aurora_gui_cursor_set(int c) { (void)c; }
int  aurora_gui_cursor_get(void) { return 0; }
int  aurora_gui_keyboard_is_key_down(int k) { (void)k; return 0; }
int  aurora_gui_keyboard_get_modifiers(void) { return 0; }
int  aurora_gui_mouse_get_x(void) { return 0; }
int  aurora_gui_mouse_get_y(void) { return 0; }
int  aurora_gui_mouse_button_down(int b) { (void)b; return 0; }
void aurora_gui_mouse_set_pos(int x, int y) { (void)x;(void)y; }
void aurora_gui_run() { }
void aurora_gui_quit() { }
void aurora_gui_set_callback(AuroraWidget w, AuroraEventCallback cb) { (void)w;(void)cb; }
void aurora_gui_layout_horizontal(AuroraWidget p, int m) { (void)p;(void)m; }
void aurora_gui_layout_vertical(AuroraWidget p, int m) { (void)p;(void)m; }

/* ── Layout stubs ── */
AuroraWidget aurora_gui_row_new(AuroraWidget p, int x, int y, int w, int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
AuroraWidget aurora_gui_column_new(AuroraWidget p, int x, int y, int w, int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
AuroraWidget aurora_gui_stack_new(AuroraWidget p, int x, int y, int w, int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
AuroraWidget aurora_gui_grid_new(AuroraWidget p, int x, int y, int w, int h, int c) { (void)p;(void)x;(void)y;(void)w;(void)h;(void)c; return nullptr; }
AuroraWidget aurora_gui_wrap_new(AuroraWidget p, int x, int y, int w, int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
AuroraWidget aurora_gui_flow_new(AuroraWidget p, int x, int y, int w, int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_layout_add_child(AuroraWidget l, AuroraWidget c) { (void)l;(void)c; }
void aurora_gui_layout_remove_child(AuroraWidget l, int i) { (void)l;(void)i; }
void aurora_gui_layout_clear(AuroraWidget l) { (void)l; }
int aurora_gui_layout_child_count(AuroraWidget l) { (void)l; return 0; }
void aurora_gui_layout_recalc(AuroraWidget l) { (void)l; }
void aurora_gui_layout_set_main_align(AuroraWidget l, int a) { (void)l;(void)a; }
void aurora_gui_layout_set_cross_align(AuroraWidget l, int a) { (void)l;(void)a; }
void aurora_gui_layout_set_spacing(AuroraWidget l, int s) { (void)l;(void)s; }
void aurora_gui_layout_child_set_flex(AuroraWidget l, AuroraWidget c, int f) { (void)l;(void)c;(void)f; }
void aurora_gui_layout_child_set_fit(AuroraWidget l, AuroraWidget c, int f) { (void)l;(void)c;(void)f; }
void aurora_gui_grid_set_child_pos(AuroraWidget g, AuroraWidget c, int col, int row, int cs, int rs) { (void)g;(void)c;(void)col;(void)row;(void)cs;(void)rs; }
AuroraWidget aurora_gui_spacer_new(AuroraWidget p, int f) { (void)p;(void)f; return nullptr; }
AuroraWidget aurora_gui_padding_new(AuroraWidget p, AuroraWidget c, int l, int t, int r, int b) { (void)p;(void)c;(void)l;(void)t;(void)r;(void)b; return nullptr; }
AuroraWidget aurora_gui_margin_new(AuroraWidget p, AuroraWidget c, int l, int t, int r, int b) { (void)p;(void)c;(void)l;(void)t;(void)r;(void)b; return nullptr; }
AuroraWidget aurora_gui_center_new(AuroraWidget p, AuroraWidget c) { (void)p;(void)c; return nullptr; }
AuroraWidget aurora_gui_align_new(AuroraWidget p, AuroraWidget c, int ax, int ay) { (void)p;(void)c;(void)ax;(void)ay; return nullptr; }
AuroraWidget aurora_gui_expand_new(AuroraWidget p, AuroraWidget c, int f) { (void)p;(void)c;(void)f; return nullptr; }
AuroraWidget aurora_gui_flexible_new(AuroraWidget p, AuroraWidget c, int f) { (void)p;(void)c;(void)f; return nullptr; }
AuroraWidget aurora_gui_container_new(AuroraWidget p, AuroraWidget c) { (void)p;(void)c; return nullptr; }
void aurora_gui_container_set_padding(AuroraWidget c, int l, int t, int r, int b) { (void)c;(void)l;(void)t;(void)r;(void)b; }
void aurora_gui_container_set_margin(AuroraWidget c, int l, int t, int r, int b) { (void)c;(void)l;(void)t;(void)r;(void)b; }
void aurora_gui_container_set_bg(AuroraWidget c, unsigned int clr) { (void)c;(void)clr; }
AuroraWidget aurora_gui_divider_new(AuroraWidget p, int o, int t, int x, int y, int w, int h) { (void)p;(void)o;(void)t;(void)x;(void)y;(void)w;(void)h; return nullptr; }
AuroraWidget aurora_gui_aspect_ratio_new(AuroraWidget p, AuroraWidget c, float r) { (void)p;(void)c;(void)r; return nullptr; }

#endif /* __linux__ */
