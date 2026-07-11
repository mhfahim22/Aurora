#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

#include "runtime/ui/component.h"
#pragma warning(push)
#pragma warning(disable : 4005)

extern "C" {

/* ── Constants (matching gui.hpp) ── */
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
#define AURORA_WIDGET_DIALOG       25

#define AURORA_WIDGET_WEBVIEW      42
#define AURORA_WIDGET_MEDIA        43
#define AURORA_WIDGET_MAP          44

#ifndef AURORA_EVENT_NONE
#define AURORA_EVENT_NONE      0
#define AURORA_EVENT_CLICK     1
#define AURORA_EVENT_CLOSE     2
#endif
#ifndef AURORA_EVENT_SELECT
#define AURORA_EVENT_SELECT    3
#define AURORA_EVENT_CHANGE    4
#define AURORA_EVENT_SCROLL    5
#define AURORA_EVENT_VALUE     6
#define AURORA_EVENT_ACTIVATE  7
#define AURORA_EVENT_KEY       8
#endif

/* ── Win32 globals ── */
static const char* WIN32_UI_CLASS = "AuroraUI_Window";
static HWND g_main_hwnd = nullptr;
static int g_ui_running = 0;
static int g_window_width = 800;
static int g_window_height = 600;
static AuroraComponent* g_root_comp = nullptr;
static void* g_last_event_source = nullptr;
static int   g_last_event_type = AURORA_EVENT_NONE;
static int   g_last_event_data = 0;
static HINSTANCE g_hinst = nullptr;
static HWND g_tooltip_hwnd = nullptr;

/* ── Helper: get component pointer from HWND ── */
static AuroraComponent* comp_from_hwnd(HWND hwnd) {
    return (AuroraComponent*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
}

/* ── Window procedure for the main UI window ── */
static LRESULT CALLBACK ui_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_DESTROY:
            g_ui_running = 0;
            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            g_window_width = (int)(short)LOWORD(lp);
            g_window_height = (int)(short)HIWORD(lp);
            if (g_root_comp) { g_root_comp->w = g_window_width; g_root_comp->h = g_window_height; }
            return 0;
        case WM_COMMAND: {
            HWND child = (HWND)lp;
            if (!child) return 0;
            AuroraComponent* comp = comp_from_hwnd(child);
            if (!comp) return 0;
            int code = HIWORD(wp);
            if (code == BN_CLICKED) {
                g_last_event_source = comp; g_last_event_type = AURORA_EVENT_CLICK; g_last_event_data = 0;
                if (comp->widget_type == AURORA_WIDGET_CHECKBOX || comp->widget_type == AURORA_WIDGET_SWITCH)
                    g_last_event_data = (int)SendMessage(child, BM_GETCHECK, 0, 0);
            } else if (code == LBN_SELCHANGE) {
                g_last_event_source = comp; g_last_event_type = AURORA_EVENT_SELECT;
                g_last_event_data = (int)SendMessage(child, LB_GETCURSEL, 0, 0);
            } else if (code == CBN_SELCHANGE) {
                g_last_event_source = comp; g_last_event_type = AURORA_EVENT_SELECT;
                g_last_event_data = (int)SendMessage(child, CB_GETCURSEL, 0, 0);
            } else if (code == EN_CHANGE) {
                g_last_event_source = comp; g_last_event_type = AURORA_EVENT_CHANGE; g_last_event_data = 0;
            }
            return 0;
        }
        case WM_HSCROLL: case WM_VSCROLL: {
            HWND child = (HWND)lp;
            if (!child) return 0;
            AuroraComponent* comp = comp_from_hwnd(child);
            if (!comp) return 0;
            g_last_event_source = comp; g_last_event_type = AURORA_EVENT_VALUE;
            g_last_event_data = (int)SendMessage(child, TBM_GETPOS, 0, 0);
            return 0;
        }
        case WM_NOTIFY: {
            LPNMHDR nm = (LPNMHDR)lp;
            if (nm->code == TVN_SELCHANGED || nm->code == TCN_SELCHANGE) {
                AuroraComponent* comp = comp_from_hwnd(nm->hwndFrom);
                if (comp) { g_last_event_source = comp; g_last_event_type = AURORA_EVENT_SELECT; g_last_event_data = 0; }
            }
            return 0;
        }
        case WM_CLOSE:
            g_last_event_source = nullptr; g_last_event_type = AURORA_EVENT_CLOSE; g_last_event_data = 0;
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ── Initialize Win32 UI ── */
int aurora_ui_win32_init(const char* title, int width, int height) {
    if (g_main_hwnd) return 0;
    g_window_width = width; g_window_height = height;
    g_hinst = GetModuleHandleA(nullptr);
    INITCOMMONCONTROLSEX icex = {sizeof(icex), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_TAB_CLASSES | ICC_TREEVIEW_CLASSES | ICC_PROGRESS_CLASS | ICC_BAR_CLASSES};
    InitCommonControlsEx(&icex);
    WNDCLASSA wc = {0}; wc.lpfnWndProc = ui_wnd_proc; wc.hInstance = g_hinst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = WIN32_UI_CLASS;
    if (!RegisterClassA(&wc)) return -1;
    RECT r = {0, 0, width, height}; AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    g_main_hwnd = CreateWindowExA(0, WIN32_UI_CLASS, title ? title : "Aurora UI",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top, nullptr, nullptr, g_hinst, nullptr);
    if (!g_main_hwnd) return -1;
    return 0;
}

/* ── Create native control ── */
int aurora_ui_win32_create_control(AuroraComponent* comp) {
    if (!comp || !g_main_hwnd) return -1;
    const char* win_class = nullptr; DWORD style = WS_CHILD | WS_VISIBLE; DWORD ex_style = 0;
    switch (comp->widget_type) {
        case AURORA_WIDGET_BUTTON: win_class = "BUTTON"; style |= BS_PUSHBUTTON; break;
        case AURORA_WIDGET_LABEL: win_class = "STATIC"; style |= SS_LEFT; break;
        case AURORA_WIDGET_TEXTBOX: win_class = "EDIT"; style |= ES_LEFT | ES_AUTOHSCROLL | WS_BORDER; break;
        case AURORA_WIDGET_PASSWORDBOX: win_class = "EDIT"; style |= ES_LEFT | ES_AUTOHSCROLL | WS_BORDER | ES_PASSWORD; break;
        case AURORA_WIDGET_LISTBOX: win_class = "LISTBOX"; style |= WS_BORDER | WS_VSCROLL | LBS_NOTIFY; break;
        case AURORA_WIDGET_CHECKBOX: win_class = "BUTTON"; style |= BS_AUTOCHECKBOX | WS_TABSTOP; break;
        case AURORA_WIDGET_RADIOBUTTON: win_class = "BUTTON"; style |= BS_AUTORADIOBUTTON | WS_TABSTOP; break;
        case AURORA_WIDGET_SLIDER: win_class = "msctls_trackbar32"; style |= TBS_AUTOTICKS | TBS_HORZ | WS_TABSTOP; break;
        case AURORA_WIDGET_PROGRESSBAR: win_class = "msctls_progress32"; style |= PBS_SMOOTH; break;
        case AURORA_WIDGET_COMBOBOX: win_class = "COMBOBOX"; style |= CBS_DROPDOWN | WS_VSCROLL | CBS_AUTOHSCROLL; break;
        case AURORA_WIDGET_DROPDOWN: win_class = "COMBOBOX"; style |= CBS_DROPDOWNLIST | WS_VSCROLL; break;
        case AURORA_WIDGET_IMAGE: win_class = "STATIC"; style |= SS_BITMAP; break;
        case AURORA_WIDGET_SWITCH: win_class = "BUTTON"; style |= BS_AUTOCHECKBOX | WS_TABSTOP; break;
        case AURORA_WIDGET_TREEVIEW: win_class = WC_TREEVIEWA; style |= WS_BORDER | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS; break;
        case AURORA_WIDGET_TABLE: win_class = WC_LISTVIEWA; style |= WS_BORDER | LVS_REPORT | LVS_SINGLESEL; break;
        case AURORA_WIDGET_TABVIEW: win_class = WC_TABCONTROLA; style |= WS_BORDER | TCS_FIXEDWIDTH; break;
        case AURORA_WIDGET_SCROLLVIEW: win_class = "EDIT"; style |= ES_MULTILINE | WS_VSCROLL | WS_HSCROLL | WS_BORDER | ES_READONLY; break;
        case AURORA_WIDGET_GROUPBOX: win_class = "BUTTON"; style |= BS_GROUPBOX; break;
        case AURORA_WIDGET_TOOLBAR: win_class = TOOLBARCLASSNAMEA; style |= TBSTYLE_TOOLTIPS | TBSTYLE_FLAT | WS_BORDER; break;
        case AURORA_WIDGET_STATUSBAR: win_class = STATUSCLASSNAMEA; style |= SBARS_SIZEGRIP; break;
        default: return -1;
    }
    HWND hwnd = CreateWindowExA(ex_style, win_class, "", style, comp->x, comp->y, comp->w, comp->h, g_main_hwnd, nullptr, g_hinst, nullptr);
    if (!hwnd) return -1;
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)comp);
    SendMessage(hwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    comp->native_handle = (void*)hwnd;
    return 0;
}

void aurora_ui_win32_destroy_control(AuroraComponent* comp) {
    if (!comp || !comp->native_handle) return;
    DestroyWindow((HWND)comp->native_handle); comp->native_handle = nullptr;
}

void aurora_ui_win32_set_text(AuroraComponent* comp, const char* text) {
    if (!comp || !comp->native_handle || !text) return;
    SetWindowTextA((HWND)comp->native_handle, text);
}

const char* aurora_ui_win32_get_text(AuroraComponent* comp) {
    if (!comp || !comp->native_handle) return ""; static char buf[4096];
    GetWindowTextA((HWND)comp->native_handle, buf, sizeof(buf)); return buf;
}

void aurora_ui_win32_listbox_add(AuroraComponent* comp, const char* item) { if (comp && comp->native_handle) SendMessageA((HWND)comp->native_handle, LB_ADDSTRING, 0, (LPARAM)item); }
void aurora_ui_win32_listbox_clear(AuroraComponent* comp) { if (comp && comp->native_handle) SendMessage((HWND)comp->native_handle, LB_RESETCONTENT, 0, 0); }
int aurora_ui_win32_listbox_selected(AuroraComponent* comp) { if (!comp || !comp->native_handle) return -1; return (int)SendMessage((HWND)comp->native_handle, LB_GETCURSEL, 0, 0); }
int aurora_ui_win32_listbox_count(AuroraComponent* comp) { if (!comp || !comp->native_handle) return 0; return (int)SendMessage((HWND)comp->native_handle, LB_GETCOUNT, 0, 0); }

void aurora_ui_win32_combobox_add(AuroraComponent* comp, const char* item) { if (comp && comp->native_handle) SendMessageA((HWND)comp->native_handle, CB_ADDSTRING, 0, (LPARAM)item); }
void aurora_ui_win32_combobox_clear(AuroraComponent* comp) { if (comp && comp->native_handle) SendMessage((HWND)comp->native_handle, CB_RESETCONTENT, 0, 0); }
int aurora_ui_win32_combobox_selected(AuroraComponent* comp) { if (!comp || !comp->native_handle) return -1; return (int)SendMessage((HWND)comp->native_handle, CB_GETCURSEL, 0, 0); }
void aurora_ui_win32_combobox_set_selected(AuroraComponent* comp, int idx) { if (comp && comp->native_handle) SendMessage((HWND)comp->native_handle, CB_SETCURSEL, idx, 0); }
int aurora_ui_win32_combobox_count(AuroraComponent* comp) { if (!comp || !comp->native_handle) return 0; return (int)SendMessage((HWND)comp->native_handle, CB_GETCOUNT, 0, 0); }

void aurora_ui_win32_checkbox_set(AuroraComponent* comp, int checked) { if (comp && comp->native_handle) SendMessage((HWND)comp->native_handle, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0); }
int aurora_ui_win32_checkbox_get(AuroraComponent* comp) { if (!comp || !comp->native_handle) return 0; return (int)SendMessage((HWND)comp->native_handle, BM_GETCHECK, 0, 0) == BST_CHECKED; }

void aurora_ui_win32_slider_set_range(AuroraComponent* comp, int min, int max) { if (comp && comp->native_handle) SendMessage((HWND)comp->native_handle, TBM_SETRANGE, TRUE, MAKELONG(min, max)); }
void aurora_ui_win32_slider_set_value(AuroraComponent* comp, int value) { if (comp && comp->native_handle) SendMessage((HWND)comp->native_handle, TBM_SETPOS, TRUE, value); }
int aurora_ui_win32_slider_get_value(AuroraComponent* comp) { if (!comp || !comp->native_handle) return 0; return (int)SendMessage((HWND)comp->native_handle, TBM_GETPOS, 0, 0); }

void aurora_ui_win32_progress_set_range(AuroraComponent* comp, int min, int max) { if (comp && comp->native_handle) SendMessage((HWND)comp->native_handle, PBM_SETRANGE, 0, MAKELONG(min, max)); }
void aurora_ui_win32_progress_set_value(AuroraComponent* comp, int value) { if (comp && comp->native_handle) SendMessage((HWND)comp->native_handle, PBM_SETPOS, value, 0); }
int aurora_ui_win32_progress_get_value(AuroraComponent* comp) { if (!comp || !comp->native_handle) return 0; return (int)SendMessage((HWND)comp->native_handle, PBM_GETPOS, 0, 0); }

HTREEITEM aurora_ui_win32_tree_add(AuroraComponent* comp, const char* text, HTREEITEM parent) {
    if (!comp || !comp->native_handle) return nullptr;
    TV_INSERTSTRUCTA tvins = {}; tvins.hParent = parent ? parent : TVI_ROOT; tvins.hInsertAfter = TVI_LAST;
    tvins.item.mask = TVIF_TEXT; tvins.item.pszText = (char*)text;
    return (HTREEITEM)SendMessageA((HWND)comp->native_handle, TVM_INSERTITEM, 0, (LPARAM)&tvins);
}
void aurora_ui_win32_tree_clear(AuroraComponent* comp) { if (comp && comp->native_handle) SendMessage((HWND)comp->native_handle, TVM_DELETEITEM, 0, (LPARAM)TVI_ROOT); }

void aurora_ui_win32_table_add_column(AuroraComponent* comp, const char* title, int width) {
    if (!comp || !comp->native_handle) return;
    LV_COLUMNA col = {}; col.mask = LVCF_TEXT | LVCF_WIDTH; col.pszText = (char*)title; col.cx = width;
    ListView_InsertColumn((HWND)comp->native_handle, (int)SendMessageA((HWND)comp->native_handle, LVM_GETCOLUMNORDERARRAY, 0, 0), &col);
}
int aurora_ui_win32_table_add_row(AuroraComponent* comp) {
    if (!comp || !comp->native_handle) return -1;
    int idx = ListView_GetItemCount((HWND)comp->native_handle);
    LV_ITEMA item = {}; item.mask = LVIF_TEXT; item.pszText = (char*)"";
    ListView_InsertItem((HWND)comp->native_handle, &item); return idx;
}
void aurora_ui_win32_table_set_cell(AuroraComponent* comp, int row, int col, const char* text) {
    if (!comp || !comp->native_handle) return;
    HWND hwnd = (HWND)comp->native_handle;
    if (col == 0) { LV_ITEMA item = {}; item.mask = LVIF_TEXT; item.iItem = row; item.iSubItem = 0; item.pszText = (char*)text; ListView_SetItem(hwnd, &item); }
    else { LV_ITEMA li = {}; li.iSubItem = col; li.pszText = (char*)text; SendMessageA(hwnd, LVM_SETITEMTEXT, row, (LPARAM)&li); }
}
void aurora_ui_win32_table_clear(AuroraComponent* comp) { if (comp && comp->native_handle) ListView_DeleteAllItems((HWND)comp->native_handle); }
int aurora_ui_win32_table_row_count(AuroraComponent* comp) { if (!comp || !comp->native_handle) return 0; return ListView_GetItemCount((HWND)comp->native_handle); }

void aurora_ui_win32_tab_add_page(AuroraComponent* comp, const char* title) {
    if (!comp || !comp->native_handle) return;
    TC_ITEMA tc = {}; tc.mask = TCIF_TEXT; tc.pszText = (char*)title;
    TabCtrl_InsertItem((HWND)comp->native_handle, TabCtrl_GetItemCount((HWND)comp->native_handle), &tc);
}
int aurora_ui_win32_tab_get_selected(AuroraComponent* comp) { return comp ? TabCtrl_GetCurSel((HWND)comp->native_handle) : -1; }
void aurora_ui_win32_tab_set_selected(AuroraComponent* comp, int idx) { if (comp && comp->native_handle) TabCtrl_SetCurSel((HWND)comp->native_handle, idx); }

void aurora_ui_win32_image_load(AuroraComponent* comp, const char* path) {
    if (!comp || !comp->native_handle || !path) return;
    HBITMAP hbm = (HBITMAP)LoadImageA(nullptr, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    if (hbm) SendMessage((HWND)comp->native_handle, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hbm);
}

void aurora_ui_win32_toolbar_add_button(AuroraComponent* comp, const char* text, int id) {
    if (!comp || !comp->native_handle) return;
    TBBUTTON tbb = {}; tbb.idCommand = id; tbb.fsState = TBSTATE_ENABLED; tbb.fsStyle = BTNS_BUTTON;
    tbb.iString = (int)SendMessageA((HWND)comp->native_handle, TB_ADDSTRING, 0, (LPARAM)text);
    SendMessage((HWND)comp->native_handle, TB_ADDBUTTONS, 1, (LPARAM)&tbb);
}

void aurora_ui_win32_statusbar_set_text(AuroraComponent* comp, const char* text) {
    if (comp && comp->native_handle) SendMessageA((HWND)comp->native_handle, SB_SETTEXT, 0, (LPARAM)text);
}

void aurora_ui_win32_sync_tree(AuroraComponent* c) {
    if (!c || !g_main_hwnd) return;
    if (c->native_handle) SetWindowPos((HWND)c->native_handle, nullptr, c->x, c->y, c->w, c->h, SWP_NOZORDER | (c->visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
    for (int i = 0; i < c->child_count; i++) aurora_ui_win32_sync_tree(c->children[i]);
}

void aurora_ui_win32_create_tree(AuroraComponent* c) {
    if (!c) return;
    if (c->widget_type != 0 && !c->native_handle) aurora_ui_win32_create_control(c);
    for (int i = 0; i < c->child_count; i++) aurora_ui_win32_create_tree(c->children[i]);
}

void aurora_ui_win32_destroy_tree(AuroraComponent* c) {
    if (!c) return;
    for (int i = 0; i < c->child_count; i++) aurora_ui_win32_destroy_tree(c->children[i]);
    aurora_ui_win32_destroy_control(c);
}

void aurora_ui_win32_mount(AuroraComponent* root) {
    g_root_comp = root;
    if (root && !root->native_handle) { root->w = g_window_width; root->h = g_window_height; }
    aurora_ui_win32_create_tree(root); aurora_ui_win32_sync_tree(root);
}

int aurora_ui_win32_run() {
    if (!g_main_hwnd) return -1; g_ui_running = 1; MSG msg;
    while (g_ui_running && GetMessage(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return 0;
}

int aurora_ui_win32_pump() {
    if (!g_main_hwnd) return -1; MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); if (msg.message == WM_QUIT) { g_ui_running = 0; return 1; } }
    return 0;
}

int aurora_ui_win32_event_type() { int t = g_last_event_type; g_last_event_type = AURORA_EVENT_NONE; return t; }
void* aurora_ui_win32_event_source() { return g_last_event_source; }
int aurora_ui_win32_event_data() { return g_last_event_data; }

void aurora_ui_win32_shutdown() {
    g_ui_running = 0;
    if (g_root_comp) { aurora_ui_win32_destroy_tree(g_root_comp); g_root_comp = nullptr; }
    if (g_main_hwnd) { DestroyWindow(g_main_hwnd); g_main_hwnd = nullptr; }
    UnregisterClassA(WIN32_UI_CLASS, g_hinst);
}

/* ── Standard GUI API (aurora_gui_* layer) ── */

} /* extern "C" */

#include "../../../include/std/gui.hpp"
#include <vector>
#include <string>
#include <map>

extern "C" {

struct GuiWidget {
    int id, type, x, y, w, h;
    std::string text;
    std::vector<std::string> items;
    int selected_idx, min_val, max_val, group_id;
    void* native;
    GuiWidget* parent;
    int is_visible;
    AuroraEventCallback callback;
    void* extra_data;
    AuroraPaintCallback paint_cb;
    void* paint_user;
};

static std::vector<GuiWidget*> g_widgets;
static int g_next_id = 1;
static std::map<int, GuiWidget*> g_id_map;
static char g_temp_str[4096];
static bool g_running = false;

static GuiWidget* widget_new(int type, GuiWidget* parent) {
    GuiWidget* w = new GuiWidget();
    w->id = g_next_id++; w->type = type; w->x = w->y = w->w = w->h = 0;
    w->native = nullptr; w->text = ""; w->selected_idx = -1; w->min_val = 0; w->max_val = 100; w->group_id = 0;
    w->callback = nullptr; w->parent = parent; w->is_visible = 1; w->extra_data = nullptr; w->paint_cb = nullptr; w->paint_user = nullptr;
    g_widgets.push_back(w); g_id_map[w->id] = w; return w;
}

static void fire_event(GuiWidget* w, int event, int p1, int p2) { if (w && w->callback) w->callback(w->id, event, p1, p2); }

static HWND gw_hwnd(GuiWidget* w) { return w ? (HWND)w->native : nullptr; }

/* ── Application ── */
int aurora_gui_app_init(void) {
    g_running = false;
    if (aurora_ui_win32_init("Aurora App", 800, 600) != 0) return -1;
    return 0;
}
void aurora_gui_app_run(void) { g_running = true; aurora_ui_win32_run(); }
void aurora_gui_app_quit(void) { g_running = false; PostQuitMessage(0); }

AuroraWidget aurora_gui_window_new(const char* title, int width, int height) { (void)title;(void)width;(void)height; return widget_new(1, nullptr); }
void aurora_gui_window_set_title(AuroraWidget widget, const char* title) { if (g_main_hwnd && title) SetWindowTextA(g_main_hwnd, title); }
void aurora_gui_window_resize(AuroraWidget widget, int w, int h) { (void)widget; if (g_main_hwnd) SetWindowPos(g_main_hwnd, 0, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER); }
void aurora_gui_window_show(AuroraWidget widget) { (void)widget; if (g_main_hwnd) ShowWindow(g_main_hwnd, SW_SHOW); }
void aurora_gui_window_hide(AuroraWidget widget) { (void)widget; if (g_main_hwnd) ShowWindow(g_main_hwnd, SW_HIDE); }
void aurora_gui_window_destroy(AuroraWidget widget) { (void)widget; if (g_main_hwnd) DestroyWindow(g_main_hwnd); }
void aurora_gui_window_maximize(AuroraWidget widget) { (void)widget; if (g_main_hwnd) ShowWindow(g_main_hwnd, SW_MAXIMIZE); }
void aurora_gui_window_minimize(AuroraWidget widget) { (void)widget; if (g_main_hwnd) ShowWindow(g_main_hwnd, SW_MINIMIZE); }
void aurora_gui_window_restore(AuroraWidget widget) { (void)widget; if (g_main_hwnd) ShowWindow(g_main_hwnd, SW_RESTORE); }
int aurora_gui_window_get_width(AuroraWidget widget) { (void)widget; return g_window_width; }
int aurora_gui_window_get_height(AuroraWidget widget) { (void)widget; return g_window_height; }
void aurora_gui_window_set_min_size(AuroraWidget widget, int w, int h) { (void)widget;(void)w;(void)h; }
void aurora_gui_window_set_max_size(AuroraWidget widget, int w, int h) { (void)widget;(void)w;(void)h; }
void aurora_gui_window_set_resizable(AuroraWidget widget, int resizable) { (void)widget;(void)resizable; }

/* ── Generic ── */
void aurora_gui_set_callback(AuroraWidget widget, AuroraEventCallback cb) { GuiWidget* w = (GuiWidget*)widget; if (w) w->callback = cb; }
void aurora_gui_set_enabled(AuroraWidget widget, int e) { (void)widget;(void)e; }
int aurora_gui_get_enabled(AuroraWidget widget) { (void)widget; return 1; }
void aurora_gui_set_visible(AuroraWidget widget, int v) { GuiWidget* w = (GuiWidget*)widget; if (!w) return; w->is_visible = v; if (w->native) ShowWindow((HWND)w->native, v ? SW_SHOW : SW_HIDE); }
int aurora_gui_get_visible(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; return w ? w->is_visible : 0; }
void aurora_gui_set_focus(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; if (w && w->native) SetFocus((HWND)w->native); }
void aurora_gui_move(AuroraWidget widget, int x_, int y_, int w_, int h_) { GuiWidget* w = (GuiWidget*)widget; if (!w) return; w->x = x_; w->y = y_; w->w = w_; w->h = h_; if (w->native) SetWindowPos((HWND)w->native, 0, x_, y_, w_, h_, SWP_NOZORDER); }
void* aurora_gui_get_native_handle(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; return w ? w->native : nullptr; }

/* ── Label ── */
AuroraWidget aurora_gui_label_new(AuroraWidget parent, const char* text, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(3, (GuiWidget*)parent); gw->x = x; gw->y = y; gw->w = w; gw->h = h; gw->text = text ? text : "";
    AuroraComponent dummy = {}; dummy.widget_type = AURORA_WIDGET_LABEL; dummy.x = x; dummy.y = y; dummy.w = w; dummy.h = h; dummy.name = (char*)"";
    if (aurora_ui_win32_create_control(&dummy) == 0) gw->native = dummy.native_handle;
    if (gw->native) SetWindowTextA((HWND)gw->native, gw->text.c_str());
    return gw;
}
void aurora_gui_label_set_text(AuroraWidget widget, const char* text) { GuiWidget* w = (GuiWidget*)widget; if (!w) return; w->text = text ? text : ""; if (w->native) SetWindowTextA((HWND)w->native, w->text.c_str()); }
const char* aurora_gui_label_get_text(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; return w ? w->text.c_str() : ""; }
void aurora_gui_label_set_font_size(AuroraWidget widget, int size) { (void)widget;(void)size; }
void aurora_gui_label_set_color(AuroraWidget widget, unsigned int color) { (void)widget;(void)color; }
void aurora_gui_label_set_align(AuroraWidget widget, int align) { (void)widget;(void)align; }

/* ── Text ── */
AuroraWidget aurora_gui_text_new(AuroraWidget parent, const char* text, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(3, (GuiWidget*)parent); gw->x = x; gw->y = y; gw->w = w; gw->h = h; gw->text = text ? text : "";
    AuroraComponent dummy = {}; dummy.widget_type = AURORA_WIDGET_TEXTBOX; dummy.x = x; dummy.y = y; dummy.w = w; dummy.h = h; dummy.name = (char*)"EDIT";
    if (aurora_ui_win32_create_control(&dummy) == 0) gw->native = dummy.native_handle;
    if (gw->native) { SetWindowTextA((HWND)gw->native, gw->text.c_str()); SendMessage((HWND)gw->native, EM_SETREADONLY, TRUE, 0); }
    return gw;
}
void aurora_gui_text_set_text(AuroraWidget widget, const char* t) { GuiWidget* w = (GuiWidget*)widget; if (w && w->native) SetWindowTextA((HWND)w->native, t ? t : ""); }
const char* aurora_gui_text_get_text(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; if (!w || !w->native) return ""; GetWindowTextA((HWND)w->native, g_temp_str, sizeof(g_temp_str)); return g_temp_str; }

/* ── Button ── */
AuroraWidget aurora_gui_button_new(AuroraWidget parent, const char* text, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(2, (GuiWidget*)parent); gw->x = x; gw->y = y; gw->w = w; gw->h = h; gw->text = text ? text : "";
    AuroraComponent dummy = {}; dummy.widget_type = AURORA_WIDGET_BUTTON; dummy.x = x; dummy.y = y; dummy.w = w; dummy.h = h; dummy.name = (char*)"";
    if (aurora_ui_win32_create_control(&dummy) == 0) gw->native = dummy.native_handle;
    if (gw->native) SetWindowTextA((HWND)gw->native, gw->text.c_str());
    return gw;
}
void aurora_gui_button_set_text(AuroraWidget widget, const char* t) { GuiWidget* w = (GuiWidget*)widget; if (w && w->native) SetWindowTextA((HWND)w->native, t ? t : ""); }
const char* aurora_gui_button_get_text(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; return w ? w->text.c_str() : ""; }

/* ── CheckBox ── */
AuroraWidget aurora_gui_checkbox_new(AuroraWidget parent, const char* text, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(7, (GuiWidget*)parent); gw->x = x; gw->y = y; gw->w = w; gw->h = h; gw->text = text ? text : "";
    AuroraComponent dummy = {}; dummy.widget_type = AURORA_WIDGET_CHECKBOX; dummy.x = x; dummy.y = y; dummy.w = w; dummy.h = h; dummy.name = (char*)"";
    if (aurora_ui_win32_create_control(&dummy) == 0) gw->native = dummy.native_handle;
    if (gw->native) SetWindowTextA((HWND)gw->native, gw->text.c_str());
    return gw;
}
void aurora_gui_checkbox_set_text(AuroraWidget widget, const char* t) { GuiWidget* w = (GuiWidget*)widget; if (w && w->native) SetWindowTextA((HWND)w->native, t ? t : ""); }
const char* aurora_gui_checkbox_get_text(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; return w ? w->text.c_str() : ""; }
int aurora_gui_checkbox_is_checked(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; return w ? (int)SendMessage((HWND)w->native, BM_GETCHECK, 0, 0) == BST_CHECKED : 0; }
void aurora_gui_checkbox_set_checked(AuroraWidget widget, int checked) { GuiWidget* w = (GuiWidget*)widget; if (w && w->native) SendMessage((HWND)w->native, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0); }

/* ── RadioButton ── */
AuroraWidget aurora_gui_radiobutton_new(AuroraWidget parent, const char* text, int x, int y, int w, int h, int group_id) {
    GuiWidget* gw = widget_new(8, (GuiWidget*)parent); gw->x = x; gw->y = y; gw->w = w; gw->h = h; gw->text = text ? text : ""; gw->group_id = group_id;
    AuroraComponent dummy = {}; dummy.widget_type = AURORA_WIDGET_RADIOBUTTON; dummy.x = x; dummy.y = y; dummy.w = w; dummy.h = h; dummy.name = (char*)"";
    if (aurora_ui_win32_create_control(&dummy) == 0) gw->native = dummy.native_handle;
    if (gw->native) SetWindowTextA((HWND)gw->native, gw->text.c_str());
    return gw;
}
void aurora_gui_radiobutton_set_text(AuroraWidget widget, const char* t) { GuiWidget* w = (GuiWidget*)widget; if (w && w->native) SetWindowTextA((HWND)w->native, t ? t : ""); }
const char* aurora_gui_radiobutton_get_text(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; return w ? w->text.c_str() : ""; }
int aurora_gui_radiobutton_is_checked(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; return w ? (int)SendMessage((HWND)w->native, BM_GETCHECK, 0, 0) == BST_CHECKED : 0; }
void aurora_gui_radiobutton_set_checked(AuroraWidget widget, int checked) { GuiWidget* w = (GuiWidget*)widget; if (w && w->native) SendMessage((HWND)w->native, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0); }

AuroraWidget aurora_gui_switch_new(AuroraWidget parent, const char* text, int x, int y, int w, int h) { return aurora_gui_checkbox_new(parent, text, x, y, w, h); }
int aurora_gui_switch_is_on(AuroraWidget widget) { return aurora_gui_checkbox_is_checked(widget); }
void aurora_gui_switch_set_on(AuroraWidget widget, int on) { aurora_gui_checkbox_set_checked(widget, on); }

/* ── TextBox ── */
AuroraWidget aurora_gui_textbox_new(AuroraWidget parent, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(4, (GuiWidget*)parent); gw->x = x; gw->y = y; gw->w = w; gw->h = h;
    AuroraComponent dummy = {}; dummy.widget_type = AURORA_WIDGET_TEXTBOX; dummy.x = x; dummy.y = y; dummy.w = w; dummy.h = h; dummy.name = (char*)"";
    if (aurora_ui_win32_create_control(&dummy) == 0) gw->native = dummy.native_handle;
    return gw;
}
void aurora_gui_textbox_set_text(AuroraWidget widget, const char* t) { GuiWidget* w = (GuiWidget*)widget; if (w && w->native) SetWindowTextA((HWND)w->native, t ? t : ""); }
const char* aurora_gui_textbox_get_text(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; if (!w || !w->native) return ""; GetWindowTextA((HWND)w->native, g_temp_str, sizeof(g_temp_str)); return g_temp_str; }
void aurora_gui_textbox_set_readonly(AuroraWidget widget, int r) { GuiWidget* w = (GuiWidget*)widget; if (w && w->native) SendMessage((HWND)w->native, EM_SETREADONLY, r, 0); }
void aurora_gui_textbox_set_placeholder(AuroraWidget widget, const char* t) { GuiWidget* w = (GuiWidget*)widget; if (w && w->native) SendMessageA((HWND)w->native, EM_SETCUEBANNER, 0, (LPARAM)(t ? t : "")); }
void aurora_gui_textbox_set_multiline(AuroraWidget widget, int m) { (void)widget;(void)m; }
int aurora_gui_textbox_get_line_count(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; return (w && w->native) ? (int)SendMessage((HWND)w->native, EM_GETLINECOUNT, 0, 0) : 0; }

AuroraWidget aurora_gui_passwordbox_new(AuroraWidget parent, int x, int y, int w, int h) { return aurora_gui_textbox_new(parent, x, y, w, h); }
void aurora_gui_passwordbox_set_text(AuroraWidget widget, const char* t) { aurora_gui_textbox_set_text(widget, t); }
const char* aurora_gui_passwordbox_get_text(AuroraWidget widget) { return aurora_gui_textbox_get_text(widget); }

/* ── Slider ── */
AuroraWidget aurora_gui_slider_new(AuroraWidget parent, int x, int y, int w, int h, int min, int max) {
    GuiWidget* gw = widget_new(9, (GuiWidget*)parent); gw->x = x; gw->y = y; gw->w = w; gw->h = h; gw->min_val = min; gw->max_val = max;
    AuroraComponent dummy = {}; dummy.widget_type = AURORA_WIDGET_SLIDER; dummy.x = x; dummy.y = y; dummy.w = w; dummy.h = h; dummy.name = (char*)"";
    if (aurora_ui_win32_create_control(&dummy) == 0) {
        gw->native = dummy.native_handle;
        SendMessage((HWND)gw->native, TBM_SETRANGE, TRUE, MAKELONG(min, max));
        SendMessage((HWND)gw->native, TBM_SETPOS, TRUE, min);
    }
    return gw;
}
int aurora_gui_slider_get_value(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; return (w && w->native) ? (int)SendMessage((HWND)w->native, TBM_GETPOS, 0, 0) : 0; }
void aurora_gui_slider_set_value(AuroraWidget widget, int value) { GuiWidget* w = (GuiWidget*)widget; if (w && w->native) SendMessage((HWND)w->native, TBM_SETPOS, TRUE, value); }
void aurora_gui_slider_set_range(AuroraWidget widget, int min, int max) { GuiWidget* w = (GuiWidget*)widget; if (w && w->native) { w->min_val = min; w->max_val = max; SendMessage((HWND)w->native, TBM_SETRANGE, TRUE, MAKELONG(min, max)); } }

/* ── ProgressBar ── */
AuroraWidget aurora_gui_progressbar_new(AuroraWidget parent, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(10, (GuiWidget*)parent); gw->x = x; gw->y = y; gw->w = w; gw->h = h;
    AuroraComponent dummy = {}; dummy.widget_type = AURORA_WIDGET_PROGRESSBAR; dummy.x = x; dummy.y = y; dummy.w = w; dummy.h = h; dummy.name = (char*)"";
    if (aurora_ui_win32_create_control(&dummy) == 0) gw->native = dummy.native_handle;
    return gw;
}
void aurora_gui_progressbar_set_value(AuroraWidget widget, int value) { GuiWidget* w = (GuiWidget*)widget; if (w && w->native) SendMessage((HWND)w->native, PBM_SETPOS, value, 0); }
int aurora_gui_progressbar_get_value(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; return (w && w->native) ? (int)SendMessage((HWND)w->native, PBM_GETPOS, 0, 0) : 0; }
void aurora_gui_progressbar_set_range(AuroraWidget widget, int min, int max) { GuiWidget* w = (GuiWidget*)widget; if (w && w->native) { w->min_val = min; w->max_val = max; SendMessage((HWND)w->native, PBM_SETRANGE, 0, MAKELONG(min, max)); } }
void aurora_gui_progressbar_set_marquee(AuroraWidget widget, int on) { GuiWidget* w = (GuiWidget*)widget; if (w && w->native) SendMessage((HWND)w->native, PBM_SETMARQUEE, on, 0); }

/* ── ComboBox ── */
AuroraWidget aurora_gui_combobox_new(AuroraWidget parent, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(11, (GuiWidget*)parent); gw->x = x; gw->y = y; gw->w = w; gw->h = h;
    AuroraComponent dummy = {}; dummy.widget_type = AURORA_WIDGET_COMBOBOX; dummy.x = x; dummy.y = y; dummy.w = w; dummy.h = h; dummy.name = (char*)"";
    if (aurora_ui_win32_create_control(&dummy) == 0) gw->native = dummy.native_handle;
    return gw;
}
void aurora_gui_combobox_add_item(AuroraWidget widget, const char* item) { GuiWidget* w = (GuiWidget*)widget; if (w) { w->items.push_back(item ? item : ""); if (w->native) SendMessageA((HWND)w->native, CB_ADDSTRING, 0, (LPARAM)(item ? item : "")); } }
void aurora_gui_combobox_clear(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; if (w) { w->items.clear(); if (w->native) SendMessage((HWND)w->native, CB_RESETCONTENT, 0, 0); } }
int aurora_gui_combobox_get_selected(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; return (w && w->native) ? (int)SendMessage((HWND)w->native, CB_GETCURSEL, 0, 0) : -1; }
void aurora_gui_combobox_set_selected(AuroraWidget widget, int idx) { GuiWidget* w = (GuiWidget*)widget; if (w && w->native) { w->selected_idx = idx; SendMessage((HWND)w->native, CB_SETCURSEL, idx, 0); } }
int aurora_gui_combobox_count(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; return (w && w->native) ? (int)SendMessage((HWND)w->native, CB_GETCOUNT, 0, 0) : 0; }
const char* aurora_gui_combobox_get_item(AuroraWidget widget, int idx) { GuiWidget* w = (GuiWidget*)widget; return (w && idx >= 0 && idx < (int)w->items.size()) ? w->items[idx].c_str() : nullptr; }

AuroraWidget aurora_gui_dropdown_new(AuroraWidget parent, int x, int y, int w, int h) { return aurora_gui_combobox_new(parent, x, y, w, h); }
void aurora_gui_dropdown_add_item(AuroraWidget widget, const char* item) { aurora_gui_combobox_add_item(widget, item); }
void aurora_gui_dropdown_clear(AuroraWidget widget) { aurora_gui_combobox_clear(widget); }
int aurora_gui_dropdown_get_selected(AuroraWidget widget) { return aurora_gui_combobox_get_selected(widget); }
void aurora_gui_dropdown_set_selected(AuroraWidget widget, int idx) { aurora_gui_combobox_set_selected(widget, idx); }
int aurora_gui_dropdown_count(AuroraWidget widget) { return aurora_gui_combobox_count(widget); }
const char* aurora_gui_dropdown_get_item(AuroraWidget widget, int idx) { return aurora_gui_combobox_get_item(widget, idx); }

/* ── ListBox ── */
AuroraWidget aurora_gui_listbox_new(AuroraWidget parent, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(5, (GuiWidget*)parent); gw->x = x; gw->y = y; gw->w = w; gw->h = h;
    AuroraComponent dummy = {}; dummy.widget_type = AURORA_WIDGET_LISTBOX; dummy.x = x; dummy.y = y; dummy.w = w; dummy.h = h; dummy.name = (char*)"";
    if (aurora_ui_win32_create_control(&dummy) == 0) gw->native = dummy.native_handle;
    return gw;
}
void aurora_gui_listbox_add_item(AuroraWidget widget, const char* item) { GuiWidget* w = (GuiWidget*)widget; if (w) { w->items.push_back(item ? item : ""); if (w->native) SendMessageA((HWND)w->native, LB_ADDSTRING, 0, (LPARAM)(item ? item : "")); } }
void aurora_gui_listbox_clear(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; if (w) { w->items.clear(); if (w->native) SendMessage((HWND)w->native, LB_RESETCONTENT, 0, 0); } }
int aurora_gui_listbox_get_selected(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; return (w && w->native) ? (int)SendMessage((HWND)w->native, LB_GETCURSEL, 0, 0) : -1; }
const char* aurora_gui_listbox_get_item(AuroraWidget widget, int idx) { GuiWidget* w = (GuiWidget*)widget; return (w && idx >= 0 && idx < (int)w->items.size()) ? w->items[idx].c_str() : nullptr; }
int aurora_gui_listbox_count(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; return (w && w->native) ? (int)SendMessage((HWND)w->native, LB_GETCOUNT, 0, 0) : 0; }

/* ── TreeView ── */
AuroraWidget aurora_gui_treeview_new(AuroraWidget parent, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(13, (GuiWidget*)parent); gw->x = x; gw->y = y; gw->w = w; gw->h = h;
    AuroraComponent dummy = {}; dummy.widget_type = AURORA_WIDGET_TREEVIEW; dummy.x = x; dummy.y = y; dummy.w = w; dummy.h = h; dummy.name = (char*)"";
    if (aurora_ui_win32_create_control(&dummy) == 0) gw->native = dummy.native_handle;
    return gw;
}
AuroraTreeItem aurora_gui_treeview_add_item(AuroraWidget widget, const char* text, AuroraTreeItem parent_item) {
    GuiWidget* w = (GuiWidget*)widget; if (!w || !w->native) return nullptr;
    TV_INSERTSTRUCTA tvins = {}; tvins.hParent = (HTREEITEM)(parent_item ? parent_item : TVI_ROOT); tvins.hInsertAfter = TVI_LAST;
    tvins.item.mask = TVIF_TEXT; tvins.item.pszText = (char*)(text ? text : "");
    return (AuroraTreeItem)SendMessageA((HWND)w->native, TVM_INSERTITEM, 0, (LPARAM)&tvins);
}
void aurora_gui_treeview_remove_item(AuroraWidget widget, AuroraTreeItem item) { GuiWidget* w = (GuiWidget*)widget; if (w && w->native) SendMessage((HWND)w->native, TVM_DELETEITEM, 0, (LPARAM)item); }
void aurora_gui_treeview_clear(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; if (w && w->native) SendMessage((HWND)w->native, TVM_DELETEITEM, 0, (LPARAM)TVI_ROOT); }
AuroraTreeItem aurora_gui_treeview_get_selected(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; return (w && w->native) ? (AuroraTreeItem)SendMessage((HWND)w->native, TVM_GETNEXTITEM, TVGN_CARET, 0) : nullptr; }
void aurora_gui_treeview_expand(AuroraWidget widget, AuroraTreeItem item) { GuiWidget* w = (GuiWidget*)widget; if (w && w->native) SendMessage((HWND)w->native, TVM_EXPAND, TVE_EXPAND, (LPARAM)item); }
void aurora_gui_treeview_collapse(AuroraWidget widget, AuroraTreeItem item) { GuiWidget* w = (GuiWidget*)widget; if (w && w->native) SendMessage((HWND)w->native, TVM_EXPAND, TVE_COLLAPSE, (LPARAM)item); }
void aurora_gui_treeview_set_item_text(AuroraWidget widget, AuroraTreeItem item, const char* text) {
    GuiWidget* w = (GuiWidget*)widget; if (!w || !w->native) return;
    TV_ITEMA tvi = {}; tvi.mask = TVIF_TEXT; tvi.hItem = (HTREEITEM)item; tvi.pszText = (char*)(text ? text : ""); tvi.cchTextMax = text ? (int)strlen(text) : 0;
    SendMessageA((HWND)w->native, TVM_SETITEM, 0, (LPARAM)&tvi);
}

/* ── Table ── */
AuroraWidget aurora_gui_table_new(AuroraWidget parent, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(14, (GuiWidget*)parent); gw->x = x; gw->y = y; gw->w = w; gw->h = h;
    AuroraComponent dummy = {}; dummy.widget_type = AURORA_WIDGET_TABLE; dummy.x = x; dummy.y = y; dummy.w = w; dummy.h = h; dummy.name = (char*)"";
    if (aurora_ui_win32_create_control(&dummy) == 0) gw->native = dummy.native_handle;
    return gw;
}
void aurora_gui_table_add_column(AuroraWidget widget, const char* title, int width) {
    GuiWidget* w = (GuiWidget*)widget; if (!w || !w->native) return;
    LV_COLUMNA col = {}; col.mask = LVCF_TEXT | LVCF_WIDTH; col.pszText = (char*)(title ? title : ""); col.cx = width;
    ListView_InsertColumn((HWND)w->native, (int)SendMessageA((HWND)w->native, LVM_GETCOLUMNORDERARRAY, 0, 0), &col);
}
int aurora_gui_table_column_count(AuroraWidget widget) {
    GuiWidget* w = (GuiWidget*)widget; if (!w || !w->native) return 0;
    HWND hdr = (HWND)SendMessage((HWND)w->native, LVM_GETHEADER, 0, 0);
    return hdr ? (int)SendMessage(hdr, HDM_GETITEMCOUNT, 0, 0) : 0;
}
AuroraTableItem aurora_gui_table_add_row(AuroraWidget widget) {
    GuiWidget* w = (GuiWidget*)widget; if (!w || !w->native) return nullptr;
    LV_ITEMA item = {}; item.mask = LVIF_TEXT; item.pszText = (char*)"";
    ListView_InsertItem((HWND)w->native, &item);
    return (AuroraTableItem)(uintptr_t)(ListView_GetItemCount((HWND)w->native) - 1);
}
void aurora_gui_table_set_cell(AuroraWidget widget, int row, int col, const char* text) {
    GuiWidget* w = (GuiWidget*)widget; if (!w || !w->native) return;
    if (col == 0) { LV_ITEMA item = {}; item.mask = LVIF_TEXT; item.iItem = row; item.iSubItem = 0; item.pszText = (char*)(text ? text : ""); ListView_SetItem((HWND)w->native, &item); }
    else { LV_ITEMA li = {}; li.iSubItem = col; li.pszText = (char*)(text ? text : ""); SendMessageA((HWND)w->native, LVM_SETITEMTEXT, row, (LPARAM)&li); }
}
const char* aurora_gui_table_get_cell(AuroraWidget w, int r, int c) { (void)w;(void)r;(void)c; return ""; }
void aurora_gui_table_remove_row(AuroraWidget widget, int row) { GuiWidget* w = (GuiWidget*)widget; if (w && w->native) SendMessage((HWND)w->native, LVM_DELETEITEM, row, 0); }
void aurora_gui_table_clear(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; if (w && w->native) ListView_DeleteAllItems((HWND)w->native); }
int aurora_gui_table_get_selected(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; return (w && w->native) ? (int)SendMessage((HWND)w->native, LVM_GETNEXTITEM, -1, LVNI_SELECTED) : -1; }
int aurora_gui_table_row_count(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; return (w && w->native) ? ListView_GetItemCount((HWND)w->native) : 0; }

/* ── TabView ── */
AuroraWidget aurora_gui_tabview_new(AuroraWidget parent, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(15, (GuiWidget*)parent); gw->x = x; gw->y = y; gw->w = w; gw->h = h;
    AuroraComponent dummy = {}; dummy.widget_type = AURORA_WIDGET_TABVIEW; dummy.x = x; dummy.y = y; dummy.w = w; dummy.h = h; dummy.name = (char*)"";
    if (aurora_ui_win32_create_control(&dummy) == 0) gw->native = dummy.native_handle;
    return gw;
}
AuroraWidget aurora_gui_tabview_add_page(AuroraWidget widget, const char* title) {
    GuiWidget* w = (GuiWidget*)widget; if (!w || !w->native) return widget;
    TC_ITEMA tc = {}; tc.mask = TCIF_TEXT; tc.pszText = (char*)(title ? title : "");
    TabCtrl_InsertItem((HWND)w->native, TabCtrl_GetItemCount((HWND)w->native), &tc);
    return widget;
}
int aurora_gui_tabview_get_selected(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; return (w && w->native) ? TabCtrl_GetCurSel((HWND)w->native) : -1; }
void aurora_gui_tabview_set_selected(AuroraWidget widget, int idx) { GuiWidget* w = (GuiWidget*)widget; if (w && w->native) TabCtrl_SetCurSel((HWND)w->native, idx); }
int aurora_gui_tabview_page_count(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; return (w && w->native) ? TabCtrl_GetItemCount((HWND)w->native) : 0; }

AuroraWidget aurora_gui_scrollview_new(AuroraWidget parent, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(16, (GuiWidget*)parent); gw->x = x; gw->y = y; gw->w = w; gw->h = h;
    AuroraComponent dummy = {}; dummy.widget_type = AURORA_WIDGET_SCROLLVIEW; dummy.x = x; dummy.y = y; dummy.w = w; dummy.h = h; dummy.name = (char*)"";
    if (aurora_ui_win32_create_control(&dummy) == 0) gw->native = dummy.native_handle;
    return gw;
}

AuroraWidget aurora_gui_splitview_new(AuroraWidget parent, int x, int y, int w, int h, int orientation) { (void)orientation; GuiWidget* gw = widget_new(22, (GuiWidget*)parent); gw->x = x; gw->y = y; gw->w = w; gw->h = h; return gw; }
void aurora_gui_splitview_set_position(AuroraWidget w, int p) { (void)w;(void)p; }
int aurora_gui_splitview_get_position(AuroraWidget w) { (void)w; return 0; }
AuroraWidget aurora_gui_splitview_get_pane1(AuroraWidget w) { (void)w; return nullptr; }
AuroraWidget aurora_gui_splitview_get_pane2(AuroraWidget w) { (void)w; return nullptr; }

AuroraWidget aurora_gui_groupbox_new(AuroraWidget parent, const char* title, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(24, (GuiWidget*)parent); gw->x = x; gw->y = y; gw->w = w; gw->h = h; gw->text = title ? title : "";
    AuroraComponent dummy = {}; dummy.widget_type = AURORA_WIDGET_GROUPBOX; dummy.x = x; dummy.y = y; dummy.w = w; dummy.h = h; dummy.name = (char*)"";
    if (aurora_ui_win32_create_control(&dummy) == 0) gw->native = dummy.native_handle;
    if (gw->native) SetWindowTextA((HWND)gw->native, gw->text.c_str());
    return gw;
}

AuroraWidget aurora_gui_image_new(AuroraWidget parent, const char* path, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(18, (GuiWidget*)parent); gw->x = x; gw->y = y; gw->w = w; gw->h = h;
    AuroraComponent dummy = {}; dummy.widget_type = AURORA_WIDGET_IMAGE; dummy.x = x; dummy.y = y; dummy.w = w; dummy.h = h; dummy.name = (char*)"";
    if (aurora_ui_win32_create_control(&dummy) == 0) gw->native = dummy.native_handle;
    if (gw->native && path) { HBITMAP hbm = (HBITMAP)LoadImageA(nullptr, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE); if (hbm) SendMessage((HWND)gw->native, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hbm); }
    return gw;
}
void aurora_gui_image_load(AuroraWidget widget, const char* path) {
    GuiWidget* w = (GuiWidget*)widget; if (w && w->native && path) { HBITMAP hbm = (HBITMAP)LoadImageA(nullptr, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE); if (hbm) SendMessage((HWND)w->native, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hbm); }
}
void aurora_gui_image_set_data(AuroraWidget widget, const unsigned char* data, int len) { (void)widget;(void)data;(void)len; }

AuroraWidget aurora_gui_canvas_new(AuroraWidget parent, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(17, (GuiWidget*)parent); gw->x = x; gw->y = y; gw->w = w; gw->h = h;
    AuroraComponent dummy = {}; dummy.widget_type = AURORA_WIDGET_CANVAS; dummy.x = x; dummy.y = y; dummy.w = w; dummy.h = h; dummy.name = (char*)"";
    if (aurora_ui_win32_create_control(&dummy) == 0) gw->native = dummy.native_handle;
    return gw;
}
void aurora_gui_canvas_set_paint_callback(AuroraWidget widget, AuroraPaintCallback cb, void* user_data) { GuiWidget* w = (GuiWidget*)widget; if (w) { w->paint_cb = cb; w->paint_user = user_data; } }
void aurora_gui_canvas_repaint(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; if (w && w->native) InvalidateRect((HWND)w->native, nullptr, TRUE); }

AuroraMenu aurora_gui_menu_bar_new(AuroraWidget parent) { (void)parent; return (AuroraMenu)CreateMenu(); }
AuroraMenu aurora_gui_menu_new(const char* text) { (void)text; return (AuroraMenu)CreatePopupMenu(); }
void aurora_gui_menu_add_item(AuroraMenu menu, const char* text, int id) { if (menu) AppendMenuA((HMENU)menu, MF_STRING, id, text ? text : ""); }
void aurora_gui_menu_add_separator(AuroraMenu menu) { if (menu) AppendMenuA((HMENU)menu, MF_SEPARATOR, 0, nullptr); }
void aurora_gui_menu_add_submenu(AuroraMenu menu, AuroraMenu submenu) { if (menu && submenu) AppendMenuA((HMENU)menu, MF_POPUP, (UINT_PTR)submenu, ""); }
void aurora_gui_menu_bar_add_menu(AuroraMenu menubar, AuroraMenu menu) { aurora_gui_menu_add_submenu(menubar, menu); }
void aurora_gui_menu_set_checked(AuroraMenu menu, int id, int checked) { if (menu) CheckMenuItem((HMENU)menu, id, checked ? MF_CHECKED : MF_UNCHECKED); }
void aurora_gui_menu_set_enabled(AuroraMenu menu, int id, int enabled) { if (menu) EnableMenuItem((HMENU)menu, id, enabled ? MF_ENABLED : MF_GRAYED); }

AuroraWidget aurora_gui_toolbar_new(AuroraWidget parent, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(19, (GuiWidget*)parent); gw->x = x; gw->y = y; gw->w = w; gw->h = h;
    AuroraComponent dummy = {}; dummy.widget_type = AURORA_WIDGET_TOOLBAR; dummy.x = x; dummy.y = y; dummy.w = w; dummy.h = h; dummy.name = (char*)"";
    if (aurora_ui_win32_create_control(&dummy) == 0) gw->native = dummy.native_handle;
    return gw;
}
void aurora_gui_toolbar_add_button(AuroraWidget widget, const char* text, int id) {
    GuiWidget* w = (GuiWidget*)widget; if (!w || !w->native) return;
    TBBUTTON tbb = {}; tbb.idCommand = id; tbb.fsState = TBSTATE_ENABLED; tbb.fsStyle = BTNS_BUTTON;
    tbb.iString = (int)SendMessageA((HWND)w->native, TB_ADDSTRING, 0, (LPARAM)(text ? text : ""));
    SendMessage((HWND)w->native, TB_ADDBUTTONS, 1, (LPARAM)&tbb);
}
void aurora_gui_toolbar_add_separator(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; if (w && w->native) { TBBUTTON tbb = {}; tbb.fsStyle = BTNS_SEP; SendMessage((HWND)w->native, TB_ADDBUTTONS, 1, (LPARAM)&tbb); } }

AuroraWidget aurora_gui_statusbar_new(AuroraWidget parent, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(20, (GuiWidget*)parent); gw->x = x; gw->y = y; gw->w = w; gw->h = h;
    AuroraComponent dummy = {}; dummy.widget_type = AURORA_WIDGET_STATUSBAR; dummy.x = x; dummy.y = y; dummy.w = w; dummy.h = h; dummy.name = (char*)"";
    if (aurora_ui_win32_create_control(&dummy) == 0) gw->native = dummy.native_handle;
    return gw;
}
void aurora_gui_statusbar_set_text(AuroraWidget widget, const char* text) { GuiWidget* w = (GuiWidget*)widget; if (w && w->native) SendMessageA((HWND)w->native, SB_SETTEXT, 0, (LPARAM)(text ? text : "")); }
const char* aurora_gui_statusbar_get_text(AuroraWidget widget) { GuiWidget* w = (GuiWidget*)widget; if (!w || !w->native) return ""; SendMessageA((HWND)w->native, SB_GETTEXT, 0, (LPARAM)g_temp_str); return g_temp_str; }
void aurora_gui_statusbar_set_parts(AuroraWidget widget, const int* widths, int count) { GuiWidget* w = (GuiWidget*)widget; if (w && w->native) SendMessage((HWND)w->native, SB_SETPARTS, count, (LPARAM)widths); }

AuroraWidget aurora_gui_dialog_new(AuroraWidget parent, const char* title, int width, int height) { (void)parent;(void)title;(void)width;(void)height; return nullptr; }
int aurora_gui_dialog_show_modal(AuroraWidget dlg) { (void)dlg; return 0; }
void aurora_gui_dialog_close(AuroraWidget dlg) { (void)dlg; }

int aurora_gui_messagebox_show(AuroraWidget parent, const char* title, const char* message, int type) {
    UINT flags = MB_OK;
    switch (type) { case 0: flags = MB_OK; break; case 1: flags = MB_OKCANCEL; break; case 2: flags = MB_YESNO; break; case 3: flags = MB_YESNOCANCEL; break; case 4: flags = MB_RETRYCANCEL; break; default: break; }
    return MessageBoxA((HWND)(parent ? ((GuiWidget*)parent)->native : nullptr), message ? message : "", title ? title : "", flags);
}

const char* aurora_gui_file_open_dialog(AuroraWidget parent, const char* title, const char* filter) {
    (void)parent;(void)title; static char path[MAX_PATH]; OPENFILENAMEA ofn = {}; ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = g_main_hwnd;
    ofn.lpstrFilter = filter ? filter : "All Files\0*.*\0"; ofn.lpstrFile = path; ofn.nMaxFile = MAX_PATH; ofn.Flags = OFN_FILEMUSTEXIST;
    return GetOpenFileNameA(&ofn) ? path : nullptr;
}
const char* aurora_gui_file_save_dialog(AuroraWidget parent, const char* title, const char* filter) {
    (void)parent;(void)title; static char path[MAX_PATH]; OPENFILENAMEA ofn = {}; ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = g_main_hwnd;
    ofn.lpstrFilter = filter ? filter : "All Files\0*.*\0"; ofn.lpstrFile = path; ofn.nMaxFile = MAX_PATH;
    return GetSaveFileNameA(&ofn) ? path : nullptr;
}
const char* aurora_gui_folder_select_dialog(AuroraWidget parent, const char* title) { (void)parent;(void)title; return nullptr; }

int aurora_gui_color_picker_dialog(AuroraWidget parent, unsigned int initial_color) { (void)parent;(void)initial_color; return -1; }
int aurora_gui_font_picker_dialog(AuroraWidget parent, AuroraFontInfo* font_info) { (void)parent;(void)font_info; return 0; }

int aurora_gui_notification_show(AuroraWidget parent, const char* title, const char* message, int icon_type) {
    (void)parent;(void)icon_type; NOTIFYICONDATAA nid = {}; nid.cbSize = sizeof(nid); nid.hWnd = g_main_hwnd; nid.uID = 1; nid.uFlags = NIF_INFO; nid.dwInfoFlags = NIIF_INFO;
    if (title) strncpy_s(nid.szInfoTitle, sizeof(nid.szInfoTitle), title, _TRUNCATE);
    if (message) strncpy_s(nid.szInfo, sizeof(nid.szInfo), message, _TRUNCATE);
    Shell_NotifyIconA(NIM_ADD, &nid); Shell_NotifyIconA(NIM_DELETE, &nid);
    return 0;
}
void aurora_gui_notification_remove(AuroraWidget parent) { (void)parent; }

int aurora_gui_clipboard_set_text(const char* text) {
    if (!text || !OpenClipboard(g_main_hwnd)) return 0; EmptyClipboard();
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, strlen(text) + 1);
    if (h) { memcpy(GlobalLock(h), text, strlen(text) + 1); GlobalUnlock(h); SetClipboardData(CF_TEXT, h); }
    CloseClipboard(); return 1;
}
const char* aurora_gui_clipboard_get_text(void) {
    if (!OpenClipboard(g_main_hwnd)) return nullptr;
    HANDLE h = GetClipboardData(CF_TEXT);
    if (h) { const char* p = (const char*)GlobalLock(h); if (p) { strncpy_s(g_temp_str, sizeof(g_temp_str), p, _TRUNCATE); GlobalUnlock(h); } }
    CloseClipboard(); return g_temp_str;
}

void aurora_gui_cursor_set(int cursor_type) {
    LPCSTR id;
    switch (cursor_type) { case 0: id = (LPCSTR)IDC_ARROW; break; case 1: id = (LPCSTR)IDC_IBEAM; break; case 2: id = (LPCSTR)IDC_WAIT; break; case 3: id = (LPCSTR)IDC_CROSS; break; case 4: id = (LPCSTR)IDC_HAND; break; case 5: id = (LPCSTR)IDC_SIZENESW; break; case 6: id = (LPCSTR)IDC_SIZENS; break; case 7: id = (LPCSTR)IDC_SIZENWSE; break; case 8: id = (LPCSTR)IDC_SIZEWE; break; case 9: id = (LPCSTR)IDC_SIZEALL; break; case 10: id = (LPCSTR)IDC_NO; break; default: id = (LPCSTR)IDC_ARROW; break; }
    SetCursor(LoadCursorA(nullptr, id));
}
int aurora_gui_cursor_get(void) { return 0; }

int aurora_gui_keyboard_is_key_down(int virtual_key) { return (GetAsyncKeyState(virtual_key) & 0x8000) ? 1 : 0; }
int aurora_gui_keyboard_get_modifiers(void) { int m = 0; if (GetAsyncKeyState(VK_SHIFT) & 0x8000) m |= 1; if (GetAsyncKeyState(VK_CONTROL) & 0x8000) m |= 2; if (GetAsyncKeyState(VK_MENU) & 0x8000) m |= 4; return m; }
int aurora_gui_mouse_get_x(void) { POINT p; GetCursorPos(&p); return p.x; }
int aurora_gui_mouse_get_y(void) { POINT p; GetCursorPos(&p); return p.y; }
int aurora_gui_mouse_button_down(int button) { return (GetAsyncKeyState(button) & 0x8000) ? 1 : 0; }
void aurora_gui_mouse_set_pos(int x, int y) { SetCursorPos(x, y); }
void aurora_gui_run(void) { aurora_gui_app_run(); }
void aurora_gui_quit(void) { aurora_gui_app_quit(); }
void aurora_gui_layout_horizontal(AuroraWidget p, int m) { (void)p;(void)m; }
void aurora_gui_layout_vertical(AuroraWidget p, int m) { (void)p;(void)m; }
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

/* ════════════════════════════════════════════════════════════
   Phase 9: WebView (placeholder panel)
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_webview_new(AuroraWidget parent, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* wv = widget_new(AURORA_WIDGET_WEBVIEW, p);
    wv->x = x; wv->y = y; wv->w = w_; wv->h = h_;
    wv->native = CreateWindowExA(0, "STATIC", "WebView2",
        WS_CHILD | WS_VISIBLE | SS_CENTER, x, y, w_, h_,
        gw_hwnd(p), (HMENU)(INT_PTR)wv->id, GetModuleHandle(nullptr), nullptr);
    if (wv->native) SetWindowLongPtr((HWND)wv->native, GWLP_USERDATA, (LONG_PTR)wv);
    return wv;
}
void aurora_gui_webview_navigate(AuroraWidget wv, const char* url) { (void)wv; (void)url; }
void aurora_gui_webview_go_back(AuroraWidget wv) { (void)wv; }
void aurora_gui_webview_go_forward(AuroraWidget wv) { (void)wv; }
void aurora_gui_webview_reload(AuroraWidget wv) { (void)wv; }
void aurora_gui_webview_set_on_title(AuroraWidget wv, AuroraEventCallback cb) { (void)wv; (void)cb; }
void aurora_gui_webview_set_on_navigate(AuroraWidget wv, AuroraEventCallback cb) { (void)wv; (void)cb; }

/* ════════════════════════════════════════════════════════════
   Phase 9: Media Player (placeholder panel)
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_media_new(AuroraWidget parent, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* mw = widget_new(AURORA_WIDGET_MEDIA, p);
    mw->x = x; mw->y = y; mw->w = w_; mw->h = h_;
    mw->native = CreateWindowExA(0, "STATIC", "Media Player",
        WS_CHILD | WS_VISIBLE | SS_CENTER, x, y, w_, h_,
        gw_hwnd(p), (HMENU)(INT_PTR)mw->id, GetModuleHandle(nullptr), nullptr);
    if (mw->native) SetWindowLongPtr((HWND)mw->native, GWLP_USERDATA, (LONG_PTR)mw);
    return mw;
}
void aurora_gui_media_open(AuroraWidget mw, const char* src) { (void)mw; (void)src; }
void aurora_gui_media_play(AuroraWidget mw) { (void)mw; }
void aurora_gui_media_pause(AuroraWidget mw) { (void)mw; }
void aurora_gui_media_stop(AuroraWidget mw) { (void)mw; }
void aurora_gui_media_set_volume(AuroraWidget mw, float vol) { (void)mw; (void)vol; }
void aurora_gui_media_set_looping(AuroraWidget mw, int loop) { (void)mw; (void)loop; }
int aurora_gui_media_is_playing(AuroraWidget mw) { (void)mw; return 0; }

/* ════════════════════════════════════════════════════════════
   Phase 9: Map (placeholder panel)
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_map_new(AuroraWidget parent, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* mp = widget_new(AURORA_WIDGET_MAP, p);
    mp->x = x; mp->y = y; mp->w = w_; mp->h = h_;
    mp->native = CreateWindowExA(0, "STATIC", "Map",
        WS_CHILD | WS_VISIBLE | SS_CENTER, x, y, w_, h_,
        gw_hwnd(p), (HMENU)(INT_PTR)mp->id, GetModuleHandle(nullptr), nullptr);
    if (mp->native) SetWindowLongPtr((HWND)mp->native, GWLP_USERDATA, (LONG_PTR)mp);
    return mp;
}
void aurora_gui_map_set_center(AuroraWidget mp, double lat, double lon) { (void)mp; (void)lat; (void)lon; }
void aurora_gui_map_set_zoom(AuroraWidget mp, int zoom) { (void)mp; (void)zoom; }
void aurora_gui_map_add_marker(AuroraWidget mp, double lat, double lon, const char* title) { (void)mp; (void)lat; (void)lon; (void)title; }

/* ════════════════════════════════════════════════════════════
   Phase 10: Widget Introspection (Inspector support)
   ════════════════════════════════════════════════════════════ */
int aurora_gui_widget_get_type(void* widget) {
    GuiWidget* w = (GuiWidget*)widget;
    return w ? w->type : 0;
}

void* aurora_gui_widget_get_parent(void* widget) {
    GuiWidget* w = (GuiWidget*)widget;
    return w ? w->parent : nullptr;
}

const char* aurora_gui_widget_get_text(void* widget) {
    if (!widget) return nullptr;
    GuiWidget* w = (GuiWidget*)widget;
    static std::string result;
    result = w->text;
    return result.c_str();
}

void aurora_gui_widget_get_bounds(void* widget, int* x, int* y, int* w, int* h) {
    GuiWidget* gw = (GuiWidget*)widget;
    if (gw) {
        if (x) *x = gw->x; if (y) *y = gw->y; if (w) *w = gw->w; if (h) *h = gw->h;
    }
}

int aurora_gui_widget_get_id(void* widget) {
    GuiWidget* w = (GuiWidget*)widget;
    return w ? w->id : -1;
}

void* aurora_gui_widget_find_at(int x, int y) {
    for (auto* w : g_widgets) {
        if (!w) continue;
        if (x >= w->x && x < w->x + w->w && y >= w->y && y < w->y + w->h)
            return w;
    }
    return nullptr;
}

int aurora_gui_widget_count(void) {
    return (int)g_widgets.size();
}

void* aurora_gui_widget_get_by_index(int idx) {
    if (idx < 0 || idx >= (int)g_widgets.size()) return nullptr;
    return g_widgets[idx];
}

}
#pragma warning(pop)
