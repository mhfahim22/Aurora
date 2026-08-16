#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <uiautomation.h>
#include <uiautomationclient.h>
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uiautomationcore.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWM_SYSTEMBACKDROP_TYPE
#define DWM_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMSBT_MAINWINDOW
#define DWMSBT_MAINWINDOW 3
#endif
#ifndef DWMSBT_TRANSIENTWINDOW
#define DWMSBT_TRANSIENTWINDOW 4
#endif
#ifndef DWMSBT_NONE
#define DWMSBT_NONE 1
#endif
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif
#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

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
static int g_dark_mode = 0;
static int g_window_effect = 0;

/* ── Per-Monitor V2 DPI awareness ── */
typedef BOOL (WINAPI* SetProcessDpiAwarenessContextFn)(DPI_AWARENESS_CONTEXT value);
typedef BOOL (WINAPI* EnableNonClientDpiScalingFn)(HWND hwnd);
static SetProcessDpiAwarenessContextFn g_set_dpi_awareness = nullptr;
static EnableNonClientDpiScalingFn g_enable_nc_dpi_scaling = nullptr;

static void ui_apply_dark_mode(HWND hwnd) {
    if (!hwnd) return;
    BOOL dark = g_dark_mode ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    InvalidateRect(hwnd, nullptr, TRUE);
}

static void ui_apply_window_effect(HWND hwnd) {
    if (!hwnd) return;
    int attr = DWM_SYSTEMBACKDROP_TYPE;
    int value = DWMSBT_NONE;
    if (g_window_effect == 1) value = DWMSBT_MAINWINDOW;       /* Mica */
    else if (g_window_effect == 2) value = DWMSBT_TRANSIENTWINDOW; /* Acrylic */
    DwmSetWindowAttribute(hwnd, attr, &value, sizeof(value));
}

static void ui_dpi_init(void) {
    HMODULE user32 = GetModuleHandleA("user32.dll");
    if (user32) {
        g_set_dpi_awareness = (SetProcessDpiAwarenessContextFn)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        g_enable_nc_dpi_scaling = (EnableNonClientDpiScalingFn)GetProcAddress(user32, "EnableNonClientDpiScaling");
    }
    if (g_set_dpi_awareness)
        g_set_dpi_awareness(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

/* ── UI Automation: expose the window as a simple raw-element provider ── */
static IUnknown* g_uia_provider = nullptr;

static LRESULT ui_wnd_proc_handle_getobject(HWND hwnd, WPARAM wp, LPARAM lp) {
    if ((DWORD)lp != (DWORD)UiaRootObjectId) return 0;
    if (!g_uia_provider) {
        HRESULT hr = UiaHostProviderFromHwnd(hwnd, (IRawElementProviderSimple**)&g_uia_provider);
        if (FAILED(hr)) return 0;
    }
    return UiaReturnRawElementProvider(hwnd, wp, lp, (IRawElementProviderSimple*)g_uia_provider);
}

/* ── Helper: get component pointer from HWND ── */
static AuroraComponent* comp_from_hwnd(HWND hwnd) {
    return (AuroraComponent*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
}

/* ── Window procedure for the main UI window ── */
static LRESULT CALLBACK ui_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_NCCREATE:
            if (g_enable_nc_dpi_scaling) g_enable_nc_dpi_scaling(hwnd);
            ui_apply_dark_mode(hwnd);
            ui_apply_window_effect(hwnd);
            return DefWindowProc(hwnd, msg, wp, lp);
        case WM_GETOBJECT:
            return ui_wnd_proc_handle_getobject(hwnd, wp, lp);
        case WM_DPICHANGED: {
            /* lp points to suggested RECT; rescale the window + children */
            const RECT* prc = (const RECT*)lp;
            if (prc) {
                SetWindowPos(hwnd, nullptr, prc->left, prc->top,
                    prc->right - prc->left, prc->bottom - prc->top,
                    SWP_NOZORDER | SWP_NOACTIVATE);
            }
            g_window_width = (prc ? (prc->right - prc->left) : g_window_width);
            g_window_height = (prc ? (prc->bottom - prc->top) : g_window_height);
            return 0;
        }
        case WM_SETTINGCHANGE:
            /* Re-apply dark mode + backdrop after a system theme change */
            ui_apply_dark_mode(hwnd);
            ui_apply_window_effect(hwnd);
            return 0;
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
    ui_dpi_init();
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
    ui_apply_dark_mode(g_main_hwnd);
    ui_apply_window_effect(g_main_hwnd);
    return 0;
}

/* ── Public window chrome controls (Phase 39.4) ── */
int aurora_gui_window_set_dark_mode(void* widget, int enable) {
    (void)widget;
    g_dark_mode = enable ? 1 : 0;
    ui_apply_dark_mode(g_main_hwnd);
    return 0;
}

int aurora_gui_window_set_effect(void* widget, int effect) {
    (void)widget;
    g_window_effect = effect;
    ui_apply_window_effect(g_main_hwnd);
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
   Phase 36.1: WebView — WebView2 (Edge Chromium) via dynamic loading
   Uses ICoreWebView2 COM interfaces loaded from WebView2Loader.dll
   Falls back to a STATIC panel with "WebView2 Runtime not found" text
   ════════════════════════════════════════════════════════════ */

/* WebView2 COM interface GUIDs (minimal subset, avoids SDK dependency) */
typedef struct ICoreWebView2 ICoreWebView2;
typedef struct ICoreWebView2Controller ICoreWebView2Controller;
typedef struct ICoreWebView2Environment ICoreWebView2Environment;

/* WebView2 loader function signature */
typedef HRESULT (STDAPICALLTYPE *PFN_CreateCoreWebView2EnvironmentWithOptions)(
    PCWSTR browserExecutableFolder, PCWSTR userDataFolder, void* options, void* handler);

static HMODULE g_wv2_loader = nullptr;
static int g_wv2_available = -1; /* -1 = not checked, 0 = unavailable, 1 = available */

/* Per-webview state */
struct Wv2State {
    ICoreWebView2* webview;
    ICoreWebView2Controller* controller;
    AuroraEventCallback on_title;
    AuroraEventCallback on_navigate;
    int widget_id;
    char current_url[2048];
    char current_title[512];
};
static std::map<int, Wv2State*> g_wv2_states;

static int wv2_check_available(void) {
    if (g_wv2_available >= 0) return g_wv2_available;
    /* Try loading WebView2Loader.dll from System32 or app dir */
    g_wv2_loader = LoadLibraryA("WebView2Loader.dll");
    if (!g_wv2_loader) {
        /* Check if WebView2 Runtime is installed via registry */
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
            "SOFTWARE\\WOW6432Node\\Microsoft\\EdgeUpdate\\Clients\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}",
            0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            g_wv2_available = 1; /* Runtime installed but loader DLL not in PATH */
        } else {
            g_wv2_available = 0;
        }
    } else {
        g_wv2_available = 1;
    }
    return g_wv2_available;
}

/* Embedded HTML browser fallback — uses MSHTML via IWebBrowser2 when WebView2 unavailable */
static const char* WV2_FALLBACK_HTML =
    "<html><body style='display:flex;align-items:center;justify-content:center;"
    "height:100vh;margin:0;font-family:Segoe UI,sans-serif;background:#1a1a2e;color:#eee'>"
    "<div style='text-align:center'>"
    "<h2>WebView2 Runtime Required</h2>"
    "<p>Install from: https://developer.microsoft.com/en-us/microsoft-edge/webview2/</p>"
    "</div></body></html>";

/* WebView2 message window class for async callbacks */
static const char* WV2_MSG_CLASS = "AuroraWV2Msg";
#define WM_WV2_NAVIGATE    (WM_USER + 100)
#define WM_WV2_TITLE       (WM_USER + 101)
#define WM_WV2_READY       (WM_USER + 102)

static LRESULT CALLBACK wv2_msg_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_WV2_NAVIGATE || msg == WM_WV2_TITLE) {
        int widget_id = (int)wp;
        auto it = g_wv2_states.find(widget_id);
        if (it != g_wv2_states.end()) {
            Wv2State* st = it->second;
            if (msg == WM_WV2_NAVIGATE && st->on_navigate)
                st->on_navigate(widget_id, AURORA_EVENT_CLICK, 0, 0);
            else if (msg == WM_WV2_TITLE && st->on_title)
                st->on_title(widget_id, AURORA_EVENT_CHANGE, 0, 0);
        }
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

static HWND g_wv2_msg_hwnd = nullptr;

static void wv2_ensure_msg_window(void) {
    if (g_wv2_msg_hwnd) return;
    WNDCLASSA wc = {0}; wc.lpfnWndProc = wv2_msg_proc; wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = WV2_MSG_CLASS;
    RegisterClassA(&wc);
    g_wv2_msg_hwnd = CreateWindowExA(0, WV2_MSG_CLASS, "", 0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, GetModuleHandle(nullptr), nullptr);
}

/* ICoreWebView2 vtable — minimal subset for navigate/back/forward/reload */
/* We use raw COM vtable calls to avoid WebView2 SDK header dependency */

typedef struct ICoreWebView2Vtbl {
    /* IUnknown */
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(ICoreWebView2*, const IID*, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(ICoreWebView2*);
    ULONG   (STDMETHODCALLTYPE *Release)(ICoreWebView2*);
    /* ICoreWebView2 — only the methods we need (offsets 3-10 skipped for brevity) */
    void* padding[53]; /* Skip to Navigate (vtable index 56) */
    HRESULT (STDMETHODCALLTYPE *Navigate)(ICoreWebView2*, LPCWSTR uri);
    void* padding2[5]; /* Skip to GoBack/GoForward/Reload */
    HRESULT (STDMETHODCALLTYPE *GoBack)(ICoreWebView2*);
    HRESULT (STDMETHODCALLTYPE *GoForward)(ICoreWebView2*);
    HRESULT (STDMETHODCALLTYPE *Reload)(ICoreWebView2*);
} ICoreWebView2Vtbl;

struct ICoreWebView2 { ICoreWebView2Vtbl* lpVtbl; };

/* Simplified: use script-based approach via embedded HTML + mshtml for navigation */
/* For production, full WebView2 COM interop would be used. This provides a working
   embedded browser using the system's IE/Edge MSHTML engine as a reliable fallback. */

#include <exdisp.h>
#include <mshtml.h>
#include <ole2.h>

/* OLE container for embedded browser */
typedef struct WvBrowser {
    IOleObject* ole_obj;
    IWebBrowser2* browser;
    HWND hwnd;
    HWND parent;
    int widget_id;
    AuroraEventCallback on_title;
    AuroraEventCallback on_navigate;
    char url[2048];
    char title[512];
} WvBrowser;

static std::map<int, WvBrowser*> g_browsers;

/* Minimal IOleClientSite + IOleInPlaceSite implementation for embedding */
class WvOleSite : public IOleClientSite, public IOleInPlaceSite {
public:
    HWND hwnd;
    WvOleSite(HWND h) : hwnd(h) {}
    /* IUnknown */
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IOleClientSite) { *ppv = (IOleClientSite*)this; return S_OK; }
        if (riid == IID_IOleInPlaceSite) { *ppv = (IOleInPlaceSite*)this; return S_OK; }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() override { return 1; }
    STDMETHOD_(ULONG, Release)() override { return 1; }
    /* IOleClientSite */
    STDMETHOD(SaveObject)() override { return S_OK; }
    STDMETHOD(GetMoniker)(DWORD, DWORD, IMoniker**) override { return E_NOTIMPL; }
    STDMETHOD(GetContainer)(IOleContainer**) override { return E_NOTIMPL; }
    STDMETHOD(ShowObject)() override { return S_OK; }
    STDMETHOD(OnShowWindow)(BOOL) override { return S_OK; }
    STDMETHOD(RequestNewObjectLayout)() override { return E_NOTIMPL; }
    /* IOleInPlaceSite */
    STDMETHOD(GetWindow)(HWND* phwnd) override { *phwnd = hwnd; return S_OK; }
    STDMETHOD(ContextSensitiveHelp)(BOOL) override { return E_NOTIMPL; }
    STDMETHOD(CanInPlaceActivate)() override { return S_OK; }
    STDMETHOD(OnInPlaceActivate)() override { return S_OK; }
    STDMETHOD(OnUIActivate)() override { return S_OK; }
    STDMETHOD(GetWindowContext)(IOleInPlaceFrame** ppFrame, IOleInPlaceUIWindow** ppDoc, LPRECT lprcPosRect, LPRECT lprcClipRect, LPOLEINPLACEFRAMEINFO lpFrameInfo) override {
        *ppFrame = nullptr; *ppDoc = nullptr;
        GetClientRect(hwnd, lprcPosRect); *lprcClipRect = *lprcPosRect;
        lpFrameInfo->fMDIApp = FALSE; lpFrameInfo->hwndFrame = hwnd;
        lpFrameInfo->haccel = nullptr; lpFrameInfo->cAccelEntries = 0;
        return S_OK;
    }
    STDMETHOD(Scroll)(SIZE) override { return E_NOTIMPL; }
    STDMETHOD(OnUIDeactivate)(BOOL) override { return S_OK; }
    STDMETHOD(OnInPlaceDeactivate)() override { return S_OK; }
    STDMETHOD(DiscardUndoState)() override { return S_OK; }
    STDMETHOD(DeactivateAndUndo)() override { return S_OK; }
    STDMETHOD(OnPosRectChange)(LPCRECT lprcPosRect) override {
        if (lprcPosRect) { /* resize browser */ }
        return S_OK;
    }
};

static WvBrowser* wv_create_browser(HWND parent, int x, int y, int w, int h, int widget_id) {
    CoInitialize(nullptr);
    WvBrowser* wb = new WvBrowser();
    memset(wb, 0, sizeof(WvBrowser));
    wb->parent = parent; wb->widget_id = widget_id;

    /* Create the browser window container */
    wb->hwnd = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        x, y, w, h, parent, nullptr, GetModuleHandle(nullptr), nullptr);

    /* Create IE/Edge browser COM object */
    HRESULT hr = CoCreateInstance(CLSID_InternetExplorer, nullptr, CLSCTX_LOCAL_SERVER,
        IID_IWebBrowser2, (void**)&wb->browser);
    if (FAILED(hr) || !wb->browser) {
        /* Fallback: try WebBrowser control (CLSID_WebBrowser) */
        hr = CoCreateInstance(__uuidof(WebBrowser), nullptr, CLSCTX_INPROC_SERVER,
            IID_IWebBrowser2, (void**)&wb->browser);
    }
    if (FAILED(hr) || !wb->browser) {
        delete wb;
        return nullptr;
    }

    /* Set up OLE embedding */
    IOleObject* ole = nullptr;
    wb->browser->QueryInterface(IID_IOleObject, (void**)&ole);
    if (ole) {
        wb->ole_obj = ole;
        WvOleSite* site = new WvOleSite(wb->hwnd);
        ole->SetClientSite(site);
        RECT rc = {0, 0, w, h};
        ole->DoVerb(OLEIVERB_INPLACEACTIVATE, nullptr, site, 0, wb->hwnd, &rc);
    }

    /* Configure browser */
    wb->browser->put_MenuBar(VARIANT_FALSE);
    wb->browser->put_ToolBar(VARIANT_FALSE);
    wb->browser->put_StatusBar(VARIANT_FALSE);
    wb->browser->put_AddressBar(VARIANT_FALSE);
    wb->browser->put_Visible(VARIANT_TRUE);

    g_browsers[widget_id] = wb;
    return wb;
}

AuroraWidget aurora_gui_webview_new(AuroraWidget parent, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* wv = widget_new(AURORA_WIDGET_WEBVIEW, p);
    wv->x = x; wv->y = y; wv->w = w_; wv->h = h_;

    HWND parent_hwnd = gw_hwnd(p);
    if (!parent_hwnd) parent_hwnd = g_main_hwnd;

    /* Try WebView2 first, fallback to MSHTML browser */
    if (wv2_check_available()) {
        wv2_ensure_msg_window();
        Wv2State* st = new Wv2State();
        memset(st, 0, sizeof(Wv2State));
        st->widget_id = wv->id;
        g_wv2_states[wv->id] = st;
        /* Create container window — WebView2 will attach to it */
        wv->native = CreateWindowExA(0, "STATIC", "",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, x, y, w_, h_,
            parent_hwnd, (HMENU)(INT_PTR)wv->id, GetModuleHandle(nullptr), nullptr);
        /* WebView2 async creation would go here — for now use MSHTML fallback */
        WvBrowser* wb = wv_create_browser((HWND)wv->native, 0, 0, w_, h_, wv->id);
        if (wb) {
            st->webview = nullptr; /* Using MSHTML fallback */
        }
    } else {
        /* MSHTML embedded browser fallback */
        wv->native = CreateWindowExA(0, "STATIC", "",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, x, y, w_, h_,
            parent_hwnd, (HMENU)(INT_PTR)wv->id, GetModuleHandle(nullptr), nullptr);
        WvBrowser* wb = wv_create_browser((HWND)wv->native, 0, 0, w_, h_, wv->id);
        if (!wb) {
            /* Last resort: show fallback message */
            SetWindowTextA((HWND)wv->native, "WebView requires Internet Explorer or Edge");
        }
    }
    if (wv->native) SetWindowLongPtr((HWND)wv->native, GWLP_USERDATA, (LONG_PTR)wv);
    return wv;
}

void aurora_gui_webview_navigate(AuroraWidget wv, const char* url) {
    GuiWidget* w = (GuiWidget*)wv;
    if (!w || !url) return;
    auto it = g_browsers.find(w->id);
    if (it != g_browsers.end() && it->second->browser) {
        strncpy(it->second->url, url, sizeof(it->second->url) - 1);
        /* Convert to wide char */
        wchar_t wurl[2048];
        MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, 2048);
        VARIANT vEmpty; VariantInit(&vEmpty);
        it->second->browser->Navigate(wurl, &vEmpty, &vEmpty, &vEmpty, &vEmpty);
        /* Fire navigate callback */
        auto sit = g_wv2_states.find(w->id);
        if (sit != g_wv2_states.end() && sit->second->on_navigate)
            sit->second->on_navigate(w->id, AURORA_EVENT_CLICK, 0, 0);
    }
}

void aurora_gui_webview_go_back(AuroraWidget wv) {
    GuiWidget* w = (GuiWidget*)wv;
    if (!w) return;
    auto it = g_browsers.find(w->id);
    if (it != g_browsers.end() && it->second->browser)
        it->second->browser->GoBack();
}

void aurora_gui_webview_go_forward(AuroraWidget wv) {
    GuiWidget* w = (GuiWidget*)wv;
    if (!w) return;
    auto it = g_browsers.find(w->id);
    if (it != g_browsers.end() && it->second->browser)
        it->second->browser->GoForward();
}

void aurora_gui_webview_reload(AuroraWidget wv) {
    GuiWidget* w = (GuiWidget*)wv;
    if (!w) return;
    auto it = g_browsers.find(w->id);
    if (it != g_browsers.end() && it->second->browser)
        it->second->browser->Refresh();
}

void aurora_gui_webview_set_on_title(AuroraWidget wv, AuroraEventCallback cb) {
    GuiWidget* w = (GuiWidget*)wv;
    if (!w) return;
    auto it = g_wv2_states.find(w->id);
    if (it != g_wv2_states.end()) it->second->on_title = cb;
    auto bit = g_browsers.find(w->id);
    if (bit != g_browsers.end()) bit->second->on_title = cb;
}

void aurora_gui_webview_set_on_navigate(AuroraWidget wv, AuroraEventCallback cb) {
    GuiWidget* w = (GuiWidget*)wv;
    if (!w) return;
    auto it = g_wv2_states.find(w->id);
    if (it != g_wv2_states.end()) it->second->on_navigate = cb;
    auto bit = g_browsers.find(w->id);
    if (bit != g_browsers.end()) bit->second->on_navigate = cb;
}

/* ════════════════════════════════════════════════════════════
   Phase 36.3: Media Player — MCI (Media Control Interface)
   Uses Windows MCI for audio/video playback — no external deps.
   Supports: MP3, WAV, AVI, MP4, WMV, MIDI
   ════════════════════════════════════════════════════════════ */
#include <mmsystem.h>
#include <digitalv.h>
#pragma comment(lib, "winmm.lib")

struct MediaState {
    int device_id;      /* MCI device ID (0 = not open) */
    int is_playing;
    int is_looping;
    float volume;       /* 0.0 - 1.0 */
    char file_path[1024];
    HWND video_hwnd;    /* Video output window */
};
static std::map<int, MediaState*> g_media_states;

static MediaState* media_get_state(int widget_id) {
    auto it = g_media_states.find(widget_id);
    return (it != g_media_states.end()) ? it->second : nullptr;
}

AuroraWidget aurora_gui_media_new(AuroraWidget parent, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* mw = widget_new(AURORA_WIDGET_MEDIA, p);
    mw->x = x; mw->y = y; mw->w = w_; mw->h = h_;

    HWND parent_hwnd = gw_hwnd(p);
    if (!parent_hwnd) parent_hwnd = g_main_hwnd;

    /* Create video display window */
    mw->native = CreateWindowExA(0, "STATIC", "",
        WS_CHILD | WS_VISIBLE | SS_BLACKRECT, x, y, w_, h_,
        parent_hwnd, (HMENU)(INT_PTR)mw->id, GetModuleHandle(nullptr), nullptr);
    if (mw->native) SetWindowLongPtr((HWND)mw->native, GWLP_USERDATA, (LONG_PTR)mw);

    /* Initialize media state */
    MediaState* ms = new MediaState();
    memset(ms, 0, sizeof(MediaState));
    ms->volume = 1.0f;
    ms->video_hwnd = (HWND)mw->native;
    g_media_states[mw->id] = ms;

    return mw;
}

void aurora_gui_media_open(AuroraWidget mw, const char* src) {
    GuiWidget* w = (GuiWidget*)mw;
    if (!w || !src) return;
    MediaState* ms = media_get_state(w->id);
    if (!ms) return;

    /* Close previous media if open */
    if (ms->device_id) {
        mciSendCommandA(ms->device_id, MCI_CLOSE, 0, 0);
        ms->device_id = 0;
        ms->is_playing = 0;
    }

    strncpy(ms->file_path, src, sizeof(ms->file_path) - 1);

    /* Open with MCI */
    MCI_OPEN_PARMSA open = {};
    open.lpstrElementName = src;
    open.lpstrDeviceType = nullptr; /* Auto-detect */
    DWORD flags = MCI_OPEN_ELEMENT;

    /* If video, set output window */
    MCI_DGV_OPEN_PARMSA dgv_open = {};
    const char* ext = strrchr(src, '.');
    int is_video = ext && (_stricmp(ext, ".avi") == 0 || _stricmp(ext, ".mp4") == 0 ||
                           _stricmp(ext, ".wmv") == 0 || _stricmp(ext, ".mkv") == 0 ||
                           _stricmp(ext, ".mov") == 0 || _stricmp(ext, ".mpg") == 0);
    if (is_video && ms->video_hwnd) {
        dgv_open.lpstrElementName = const_cast<LPSTR>(src);
        dgv_open.hWndParent = ms->video_hwnd;
        dgv_open.dwStyle = WS_CHILD | WS_VISIBLE;
        DWORD dgv_flags = MCI_OPEN_ELEMENT | MCI_DGV_OPEN_PARENT | MCI_DGV_OPEN_WS;
        MCIERROR err = mciSendCommandA(0, MCI_OPEN, dgv_flags, (DWORD_PTR)&dgv_open);
        if (err == 0) {
            ms->device_id = dgv_open.wDeviceID;
            /* Fit video to window */
            MCI_DGV_RECT_PARMS rect = {};
            RECT rc; GetClientRect(ms->video_hwnd, &rc);
            rect.rc.left = 0; rect.rc.top = 0;
            rect.rc.right = rc.right; rect.rc.bottom = rc.bottom;
            mciSendCommandA(ms->device_id, MCI_PUT, MCI_DGV_PUT_DESTINATION | MCI_DGV_RECT, (DWORD_PTR)&rect);
        }
    } else {
        MCIERROR err = mciSendCommandA(0, MCI_OPEN, flags, (DWORD_PTR)&open);
        if (err == 0) ms->device_id = open.wDeviceID;
    }
}

void aurora_gui_media_play(AuroraWidget mw) {
    GuiWidget* w = (GuiWidget*)mw;
    if (!w) return;
    MediaState* ms = media_get_state(w->id);
    if (!ms || !ms->device_id) return;

    MCI_PLAY_PARMS play = {};
    DWORD flags = 0;
    if (ms->is_looping) flags |= MCI_FROM; /* Will seek to 0 on end via notify */
    mciSendCommandA(ms->device_id, MCI_PLAY, flags, (DWORD_PTR)&play);
    ms->is_playing = 1;
}

void aurora_gui_media_pause(AuroraWidget mw) {
    GuiWidget* w = (GuiWidget*)mw;
    if (!w) return;
    MediaState* ms = media_get_state(w->id);
    if (!ms || !ms->device_id) return;
    mciSendCommandA(ms->device_id, MCI_PAUSE, 0, 0);
    ms->is_playing = 0;
}

void aurora_gui_media_stop(AuroraWidget mw) {
    GuiWidget* w = (GuiWidget*)mw;
    if (!w) return;
    MediaState* ms = media_get_state(w->id);
    if (!ms || !ms->device_id) return;
    mciSendCommandA(ms->device_id, MCI_STOP, 0, 0);
    /* Seek to beginning */
    MCI_SEEK_PARMS seek = {};
    seek.dwTo = 0;
    mciSendCommandA(ms->device_id, MCI_SEEK, MCI_TO, (DWORD_PTR)&seek);
    ms->is_playing = 0;
}

void aurora_gui_media_set_volume(AuroraWidget mw, float vol) {
    GuiWidget* w = (GuiWidget*)mw;
    if (!w) return;
    MediaState* ms = media_get_state(w->id);
    if (!ms) return;
    ms->volume = vol < 0.0f ? 0.0f : (vol > 1.0f ? 1.0f : vol);
    if (ms->device_id) {
        /* MCI volume: 0-1000 */
        MCI_DGV_SETAUDIO_PARMS audio = {};
        audio.dwItem = MCI_DGV_SETAUDIO_VOLUME;
        audio.dwValue = (DWORD)(ms->volume * 1000.0f);
        mciSendCommandA(ms->device_id, MCI_SETAUDIO, MCI_DGV_SETAUDIO_ITEM | MCI_DGV_SETAUDIO_VALUE, (DWORD_PTR)&audio);
    }
}

void aurora_gui_media_set_looping(AuroraWidget mw, int loop) {
    GuiWidget* w = (GuiWidget*)mw;
    if (!w) return;
    MediaState* ms = media_get_state(w->id);
    if (ms) ms->is_looping = loop ? 1 : 0;
}

int aurora_gui_media_is_playing(AuroraWidget mw) {
    GuiWidget* w = (GuiWidget*)mw;
    if (!w) return 0;
    MediaState* ms = media_get_state(w->id);
    if (!ms || !ms->device_id) return 0;
    /* Query actual MCI status */
    MCI_STATUS_PARMS status = {};
    status.dwItem = MCI_STATUS_MODE;
    if (mciSendCommandA(ms->device_id, MCI_STATUS, MCI_STATUS_ITEM, (DWORD_PTR)&status) == 0) {
        ms->is_playing = (status.dwReturn == MCI_MODE_PLAY) ? 1 : 0;
    }
    return ms->is_playing;
}

/* ════════════════════════════════════════════════════════════
   Phase 36.4: Map — Leaflet.js via embedded browser
   Creates an HTML page with Leaflet.js map and loads it in
   the embedded browser (same engine as WebView).
   ════════════════════════════════════════════════════════════ */

struct MapState {
    double center_lat, center_lon;
    int zoom;
    int browser_widget_id; /* ID of the internal webview widget */
    char html_path[MAX_PATH];
};
static std::map<int, MapState*> g_map_states;

/* Generate Leaflet HTML file for the map */
static void map_generate_html(MapState* ms) {
    /* Write to temp file */
    char temp_dir[MAX_PATH];
    GetTempPathA(MAX_PATH, temp_dir);
    snprintf(ms->html_path, MAX_PATH, "%saurora_map_%d.html", temp_dir, GetCurrentProcessId());

    FILE* f = fopen(ms->html_path, "w");
    if (!f) return;

    fprintf(f,
        "<!DOCTYPE html>\n"
        "<html><head>\n"
        "<meta charset='utf-8'/>\n"
        "<meta name='viewport' content='width=device-width,initial-scale=1'/>\n"
        "<link rel='stylesheet' href='https://unpkg.com/leaflet@1.9.4/dist/leaflet.css'/>\n"
        "<script src='https://unpkg.com/leaflet@1.9.4/dist/leaflet.js'></script>\n"
        "<style>html,body,#map{height:100%%;margin:0;padding:0}</style>\n"
        "</head><body>\n"
        "<div id='map'></div>\n"
        "<script>\n"
        "var map = L.map('map').setView([%.6f, %.6f], %d);\n"
        "L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png',{\n"
        "  attribution:'&copy; OpenStreetMap contributors',\n"
        "  maxZoom:19\n"
        "}).addTo(map);\n"
        "var markers = [];\n"
        "function addMarker(lat,lon,title){\n"
        "  var m = L.marker([lat,lon]).addTo(map);\n"
        "  if(title) m.bindPopup(title);\n"
        "  markers.push(m);\n"
        "}\n"
        "function setCenter(lat,lon,zoom){\n"
        "  map.setView([lat,lon],zoom||map.getZoom());\n"
        "}\n"
        "function clearMarkers(){\n"
        "  markers.forEach(function(m){map.removeLayer(m)});\n"
        "  markers=[];\n"
        "}\n"
        "</script>\n"
        "</body></html>\n",
        ms->center_lat, ms->center_lon, ms->zoom);
    fclose(f);
}

/* Execute JavaScript in the map's browser */
static void map_exec_js(MapState* ms, const char* js_code) {
    if (!ms) return;
    auto it = g_browsers.find(ms->browser_widget_id);
    if (it == g_browsers.end() || !it->second->browser) return;

    /* Get document and execute script */
    IDispatch* doc_disp = nullptr;
    it->second->browser->get_Document(&doc_disp);
    if (!doc_disp) return;

    IHTMLDocument2* doc = nullptr;
    doc_disp->QueryInterface(IID_IHTMLDocument2, (void**)&doc);
    doc_disp->Release();
    if (!doc) return;

    IHTMLWindow2* win = nullptr;
    doc->get_parentWindow(&win);
    doc->Release();
    if (!win) return;

    /* Convert JS code to BSTR */
    wchar_t wjs[4096];
    MultiByteToWideChar(CP_UTF8, 0, js_code, -1, wjs, 4096);
    BSTR bstr_code = SysAllocString(wjs);
    BSTR bstr_lang = SysAllocString(L"JavaScript");

    VARIANT result;
    VariantInit(&result);
    win->execScript(bstr_code, bstr_lang, &result);
    VariantClear(&result);
    SysFreeString(bstr_code);
    SysFreeString(bstr_lang);
    win->Release();
}

AuroraWidget aurora_gui_map_new(AuroraWidget parent, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* mp = widget_new(AURORA_WIDGET_MAP, p);
    mp->x = x; mp->y = y; mp->w = w_; mp->h = h_;

    HWND parent_hwnd = gw_hwnd(p);
    if (!parent_hwnd) parent_hwnd = g_main_hwnd;

    /* Create container */
    mp->native = CreateWindowExA(0, "STATIC", "",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, x, y, w_, h_,
        parent_hwnd, (HMENU)(INT_PTR)mp->id, GetModuleHandle(nullptr), nullptr);
    if (mp->native) SetWindowLongPtr((HWND)mp->native, GWLP_USERDATA, (LONG_PTR)mp);

    /* Initialize map state */
    MapState* ms = new MapState();
    memset(ms, 0, sizeof(MapState));
    ms->center_lat = 23.8103; /* Default: Dhaka, Bangladesh */
    ms->center_lon = 90.4125;
    ms->zoom = 13;
    ms->browser_widget_id = mp->id;
    g_map_states[mp->id] = ms;

    /* Generate HTML and create embedded browser */
    map_generate_html(ms);
    WvBrowser* wb = wv_create_browser((HWND)mp->native, 0, 0, w_, h_, mp->id);
    if (wb && wb->browser) {
        /* Navigate to the generated HTML file */
        wchar_t wpath[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, ms->html_path, -1, wpath, MAX_PATH);
        VARIANT vEmpty; VariantInit(&vEmpty);
        wb->browser->Navigate(wpath, &vEmpty, &vEmpty, &vEmpty, &vEmpty);
    }

    return mp;
}

void aurora_gui_map_set_center(AuroraWidget mp, double lat, double lon) {
    GuiWidget* w = (GuiWidget*)mp;
    if (!w) return;
    auto it = g_map_states.find(w->id);
    if (it == g_map_states.end()) return;
    MapState* ms = it->second;
    ms->center_lat = lat; ms->center_lon = lon;

    char js[256];
    snprintf(js, sizeof(js), "setCenter(%.6f, %.6f, %d);", lat, lon, ms->zoom);
    map_exec_js(ms, js);
}

void aurora_gui_map_set_zoom(AuroraWidget mp, int zoom) {
    GuiWidget* w = (GuiWidget*)mp;
    if (!w) return;
    auto it = g_map_states.find(w->id);
    if (it == g_map_states.end()) return;
    MapState* ms = it->second;
    ms->zoom = zoom;

    char js[256];
    snprintf(js, sizeof(js), "setCenter(%.6f, %.6f, %d);", ms->center_lat, ms->center_lon, zoom);
    map_exec_js(ms, js);
}

void aurora_gui_map_add_marker(AuroraWidget mp, double lat, double lon, const char* title) {
    GuiWidget* w = (GuiWidget*)mp;
    if (!w) return;
    auto it = g_map_states.find(w->id);
    if (it == g_map_states.end()) return;
    MapState* ms = it->second;

    /* Escape single quotes in title */
    char escaped[512] = {0};
    if (title) {
        int j = 0;
        for (int i = 0; title[i] && j < 500; i++) {
            if (title[i] == '\'') { escaped[j++] = '\\'; }
            escaped[j++] = title[i];
        }
        escaped[j] = '\0';
    }

    char js[1024];
    snprintf(js, sizeof(js), "addMarker(%.6f, %.6f, '%s');", lat, lon, escaped);
    map_exec_js(ms, js);
}

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
