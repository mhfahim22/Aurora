/* ════════════════════════════════════════════════════════════
   gui_win32.cpp — Win32 native GUI backend for Aurora
   ════════════════════════════════════════════════════════════ */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")

#include "../../include/std/gui.hpp"

/* ── Forward declare TabPage struct ── */
struct TabPage {
    std::string title;
    HWND panel;
};

/* ── Forward declare MenuData ── */
struct MenuData {
    HMENU hmenu;
    std::vector<MenuData*> submenus;
};

/* ── Forward declare ToolbarData ── */
struct ToolbarData {
    int next_id;
};

/* ── Forward declare TableData ── */
struct TableData {
    int col_count;
    int row_count;
};

/* ── Forward declare SplitViewData ── */
struct SplitViewData {
    int orientation;
    int position;
};

/* ════════════════════════════════════════════════════════════
   GuiWidget — internal widget struct
   ════════════════════════════════════════════════════════════ */
struct GuiWidget {
    int id;
    int type;
    int x, y, w, h;
    std::string text;
    std::vector<std::string> items;
    int selected_idx;
    int min_val, max_val;
    int group_id;
    AuroraEventCallback callback;
    GuiWidget* parent;
    HWND hwnd;
    HWND hwnd_parent;
    void* extra_data;
};

/* ── Globals ── */
static std::vector<GuiWidget*> g_widgets;
static int g_next_id = 1;
static bool g_running = false;
static std::map<int, GuiWidget*> g_id_map;
static char g_temp_str[4096];

static const char* WINDOW_CLASS = "AuroraGUI_Window";
static bool g_class_registered = false;
static bool g_common_ctrl_inited = false;

static OPENFILENAMEA g_ofn;
static char g_ofn_file[4096];

/* ── Tray icon map ── */
static std::map<HWND, NOTIFYICONDATAA> g_tray_icons;

/* ── Cursor ID map ── */
static LPCWSTR cursor_ids[] = {
    IDC_ARROW, IDC_IBEAM, IDC_WAIT, IDC_CROSS, IDC_HAND,
    IDC_SIZENESW, IDC_SIZENS, IDC_SIZENWSE, IDC_SIZEWE, IDC_SIZEALL, IDC_NO
};

/* ── Helper: create widget ── */
static GuiWidget* widget_new(int type, GuiWidget* parent) {
    GuiWidget* w = new GuiWidget();
    w->id = g_next_id++;
    w->type = type;
    w->x = w->y = w->w = w->h = 0;
    w->selected_idx = -1;
    w->min_val = 0;
    w->max_val = 100;
    w->group_id = 0;
    w->callback = nullptr;
    w->parent = parent;
    w->extra_data = nullptr;
    w->hwnd = nullptr;
    w->hwnd_parent = parent ? parent->hwnd : nullptr;
    g_widgets.push_back(w);
    g_id_map[w->id] = w;
    return w;
}

/* ── Helper: init common controls ── */
static void ensure_common_ctrl() {
    if (g_common_ctrl_inited) return;
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES |
                 ICC_TAB_CLASSES | ICC_PROGRESS_CLASS |
                 ICC_BAR_CLASSES | ICC_STANDARD_CLASSES |
                 ICC_COOL_CLASSES | ICC_USEREX_CLASSES;
    InitCommonControlsEx(&icex);
    g_common_ctrl_inited = true;
}

/* ── Helper: get parent HWND ── */
static HWND parent_hwnd(GuiWidget* p) {
    return p ? p->hwnd : nullptr;
}

/* ── Forward declarations for layout ── */
static void layout_recalc(GuiWidget* w);
static void wrapper_reposition(GuiWidget* wp);

/* ── Window procedure ── */
static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    GuiWidget* w = (GuiWidget*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!w) return DefWindowProc(hwnd, msg, wp, lp);
    switch (msg) {
        case WM_CLOSE: {
            if (w->callback) w->callback(w->id, AURORA_EVENT_CLOSE, 0, 0);
            if (w->type == AURORA_WIDGET_WINDOW || w->type == AURORA_WIDGET_DIALOG) {
                DestroyWindow(hwnd);
                PostQuitMessage(0);
            }
            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(wp);
            int code = HIWORD(wp);
            auto it = g_id_map.find(id);
            if (it != g_id_map.end()) {
                if (code == BN_CLICKED) it->second->callback(id, AURORA_EVENT_CLICK, 0, 0);
                else if (code == EN_CHANGE) it->second->callback(id, AURORA_EVENT_CHANGE, 0, 0);
                else if (code == CBN_SELCHANGE) {
                    int idx = (int)SendMessage((HWND)lp, CB_GETCURSEL, 0, 0);
                    it->second->selected_idx = idx;
                    it->second->callback(id, AURORA_EVENT_SELECT, idx, 0);
                }
                else if (code == LBN_SELCHANGE) {
                    int idx = (int)SendMessage((HWND)lp, LB_GETCURSEL, 0, 0);
                    it->second->selected_idx = idx;
                    it->second->callback(id, AURORA_EVENT_SELECT, idx, 0);
                }
            }
            return 0;
        }
        case WM_HSCROLL: case WM_VSCROLL: {
            if (w->callback) {
                int code2 = LOWORD(wp);
                if (code2 == SB_THUMBTRACK || code2 == SB_ENDSCROLL) {
                    int pos = (int)SendMessage(hwnd, TBM_GETPOS, 0, 0);
                    w->callback(w->id, AURORA_EVENT_SCROLL, pos, code2);
                }
            }
            return 0;
        }
        case WM_NOTIFY: {
            LPNMHDR nmhdr = (LPNMHDR)lp;
            auto it = g_id_map.find(nmhdr->idFrom);
            if (it != g_id_map.end() && it->second->callback) {
                GuiWidget* src = it->second;
                if (nmhdr->code == TVN_SELCHANGED)
                    src->callback(src->id, AURORA_EVENT_TREE_SELECT, 0, 0);
                else if (nmhdr->code == TCN_SELCHANGE)
                    src->callback(src->id, AURORA_EVENT_TAB_CHANGE, TabCtrl_GetCurSel(nmhdr->hwndFrom), 0);
            }
            return 0;
        }
        case WM_DESTROY:
            if (w->type == AURORA_WIDGET_WINDOW) PostQuitMessage(0);
            return 0;
        case WM_PAINT: {
            if (w->type == AURORA_WIDGET_CANVAS) {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                HBRUSH br = CreateSolidBrush(RGB(255, 255, 255));
                FillRect(hdc, &ps.rcPaint, br);
                DeleteObject(br);
                if (w->extra_data) {
                    AuroraPaintCallback pcb = (AuroraPaintCallback)w->extra_data;
                    pcb(nullptr, 0, 0, w->w, w->h);
                }
                EndPaint(hwnd, &ps);
                return 0;
            }
            break;
        }
        case WM_CTLCOLORSTATIC: {
            if (w->extra_data && (w->type == AURORA_WIDGET_LABEL || w->type == AURORA_WIDGET_CHECKBOX)) {
                HDC hdc = (HDC)wp;
                unsigned int color = (unsigned int)(uintptr_t)w->extra_data;
                SetTextColor(hdc, RGB(GetRValue(color), GetGValue(color), GetBValue(color)));
                SetBkMode(hdc, TRANSPARENT);
                return (LRESULT)GetStockObject(NULL_BRUSH);
            }
            break;
        }
        case WM_SIZE: {
            if (w->type >= AURORA_WIDGET_ROW && w->type <= AURORA_WIDGET_GRID) {
                layout_recalc(w);
                return 0;
            }
            if (w->type == AURORA_WIDGET_PADDING || w->type == AURORA_WIDGET_MARGIN ||
                w->type == AURORA_WIDGET_CENTER || w->type == AURORA_WIDGET_ALIGN ||
                w->type == AURORA_WIDGET_CONTAINER) {
                wrapper_reposition(w);
                return 0;
            }
            break;
        }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

static void register_window_class() {
    if (g_class_registered) return;
    ensure_common_ctrl();
    WNDCLASSA wc = {};
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = WINDOW_CLASS;
    RegisterClassA(&wc);
    g_class_registered = true;
}

/* ── Forward declarations for functions defined later ── */
extern "C" void aurora_gui_run(void);
extern "C" void aurora_gui_quit(void);

/* ════════════════════════════════════════════════════════════
   Application
   ════════════════════════════════════════════════════════════ */
int aurora_gui_app_init(void) { register_window_class(); return 0; }
void aurora_gui_app_run(void) { aurora_gui_run(); }
void aurora_gui_app_quit(void) { aurora_gui_quit(); }

/* ════════════════════════════════════════════════════════════
   Generic widget operations
   ════════════════════════════════════════════════════════════ */
void aurora_gui_set_enabled(AuroraWidget widget, int enabled) {
    GuiWidget* w = (GuiWidget*)widget;
    if (w && w->hwnd) EnableWindow(w->hwnd, enabled ? TRUE : FALSE);
}
int aurora_gui_get_enabled(AuroraWidget widget) {
    GuiWidget* w = (GuiWidget*)widget;
    return (w && w->hwnd) ? IsWindowEnabled(w->hwnd) : 0;
}
void aurora_gui_set_visible(AuroraWidget widget, int visible) {
    GuiWidget* w = (GuiWidget*)widget;
    if (w && w->hwnd) ShowWindow(w->hwnd, visible ? SW_SHOW : SW_HIDE);
}
int aurora_gui_get_visible(AuroraWidget widget) {
    GuiWidget* w = (GuiWidget*)widget;
    return (w && w->hwnd) ? IsWindowVisible(w->hwnd) : 0;
}
void aurora_gui_set_focus(AuroraWidget widget) {
    GuiWidget* w = (GuiWidget*)widget;
    if (w && w->hwnd) SetFocus(w->hwnd);
}
void aurora_gui_move(AuroraWidget widget, int x, int y, int w_, int h_) {
    GuiWidget* gw = (GuiWidget*)widget;
    if (!gw) return;
    gw->x = x; gw->y = y; gw->w = w_; gw->h = h_;
    if (gw->hwnd) MoveWindow(gw->hwnd, x, y, w_, h_, TRUE);
}

void* aurora_gui_get_native_handle(AuroraWidget widget) {
    GuiWidget* gw = (GuiWidget*)widget;
    if (!gw) return nullptr;
    return (void*)gw->hwnd;
}

/* ════════════════════════════════════════════════════════════
   Window
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_window_new(const char* title, int width, int height) {
    register_window_class();
    GuiWidget* w = widget_new(AURORA_WIDGET_WINDOW, nullptr);
    w->hwnd = CreateWindowExA(0, WINDOW_CLASS, title ? title : "",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    if (w->hwnd) SetWindowLongPtr(w->hwnd, GWLP_USERDATA, (LONG_PTR)w);
    return (AuroraWidget)w;
}
void aurora_gui_window_set_title(AuroraWidget win, const char* title) {
    GuiWidget* w = (GuiWidget*)win;
    if (w->hwnd) SetWindowTextA(w->hwnd, title ? title : "");
}
void aurora_gui_window_resize(AuroraWidget win, int w_, int h_) {
    GuiWidget* w = (GuiWidget*)win;
    if (w->hwnd) SetWindowPos(w->hwnd, nullptr, 0, 0, w_, h_, SWP_NOMOVE | SWP_NOZORDER);
}
void aurora_gui_window_show(AuroraWidget win) {
    GuiWidget* w = (GuiWidget*)win;
    if (w->hwnd) ShowWindow(w->hwnd, SW_SHOW);
}
void aurora_gui_window_hide(AuroraWidget win) {
    GuiWidget* w = (GuiWidget*)win;
    if (w->hwnd) ShowWindow(w->hwnd, SW_HIDE);
}
void aurora_gui_window_destroy(AuroraWidget win) {
    GuiWidget* w = (GuiWidget*)win;
    if (w->hwnd) DestroyWindow(w->hwnd);
}
void aurora_gui_window_maximize(AuroraWidget win) {
    GuiWidget* w = (GuiWidget*)win;
    if (w->hwnd) ShowWindow(w->hwnd, SW_MAXIMIZE);
}
void aurora_gui_window_minimize(AuroraWidget win) {
    GuiWidget* w = (GuiWidget*)win;
    if (w->hwnd) ShowWindow(w->hwnd, SW_MINIMIZE);
}
void aurora_gui_window_restore(AuroraWidget win) {
    GuiWidget* w = (GuiWidget*)win;
    if (w->hwnd) ShowWindow(w->hwnd, SW_RESTORE);
}
int aurora_gui_window_get_width(AuroraWidget win) {
    GuiWidget* w = (GuiWidget*)win;
    if (!w || !w->hwnd) return 0;
    RECT r; GetClientRect(w->hwnd, &r);
    return r.right - r.left;
}
int aurora_gui_window_get_height(AuroraWidget win) {
    GuiWidget* w = (GuiWidget*)win;
    if (!w || !w->hwnd) return 0;
    RECT r; GetClientRect(w->hwnd, &r);
    return r.bottom - r.top;
}
void aurora_gui_window_set_min_size(AuroraWidget win, int w_, int h_) {
    (void)win; (void)w_; (void)h_;
}
void aurora_gui_window_set_max_size(AuroraWidget win, int w_, int h_) {
    (void)win; (void)w_; (void)h_;
}
void aurora_gui_window_set_resizable(AuroraWidget win, int resizable) {
    GuiWidget* w = (GuiWidget*)win;
    if (!w->hwnd) return;
    LONG style = GetWindowLong(w->hwnd, GWL_STYLE);
    if (resizable) style |= WS_THICKFRAME;
    else style &= ~WS_THICKFRAME;
    SetWindowLong(w->hwnd, GWL_STYLE, style);
}

/* ════════════════════════════════════════════════════════════
   Label
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_label_new(AuroraWidget parent, const char* text, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* lbl = widget_new(AURORA_WIDGET_LABEL, p);
    lbl->x = x; lbl->y = y; lbl->w = w_; lbl->h = h_;
    lbl->text = text ? text : "";
    lbl->hwnd = CreateWindowExA(0, "STATIC", text ? text : "",
        WS_CHILD | WS_VISIBLE, x, y, w_, h_,
        parent_hwnd(p), nullptr, GetModuleHandle(nullptr), nullptr);
    if (lbl->hwnd) {
        SetWindowLongPtr(lbl->hwnd, GWLP_USERDATA, (LONG_PTR)lbl);
        SendMessage(lbl->hwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    }
    return (AuroraWidget)lbl;
}
void aurora_gui_label_set_text(AuroraWidget lbl, const char* text) {
    GuiWidget* w = (GuiWidget*)lbl;
    w->text = text ? text : "";
    if (w->hwnd) SetWindowTextA(w->hwnd, w->text.c_str());
}
const char* aurora_gui_label_get_text(AuroraWidget lbl) {
    return ((GuiWidget*)lbl)->text.c_str();
}
void aurora_gui_label_set_font_size(AuroraWidget lbl, int size) {
    GuiWidget* w = (GuiWidget*)lbl;
    if (!w->hwnd) return;
    HFONT hFont = CreateFontA(size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH, "Segoe UI");
    SendMessage(w->hwnd, WM_SETFONT, (WPARAM)hFont, TRUE);
}
void aurora_gui_label_set_color(AuroraWidget lbl, unsigned int color) {
    GuiWidget* w = (GuiWidget*)lbl;
    w->extra_data = (void*)(uintptr_t)color;
    if (w->hwnd) InvalidateRect(w->hwnd, nullptr, TRUE);
}
void aurora_gui_label_set_align(AuroraWidget lbl, int align) {
    GuiWidget* w = (GuiWidget*)lbl;
    if (!w->hwnd) return;
    LONG style = GetWindowLong(w->hwnd, GWL_STYLE);
    style &= ~(SS_LEFT | SS_CENTER | SS_RIGHT);
    if (align == AURORA_ALIGN_CENTER) style |= SS_CENTER;
    else if (align == AURORA_ALIGN_RIGHT) style |= SS_RIGHT;
    else style |= SS_LEFT;
    SetWindowLong(w->hwnd, GWL_STYLE, style);
    InvalidateRect(w->hwnd, nullptr, TRUE);
}

/* ════════════════════════════════════════════════════════════
   Text (read-only selectable)
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_text_new(AuroraWidget parent, const char* text, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* txt = widget_new(AURORA_WIDGET_LABEL, p);
    txt->x = x; txt->y = y; txt->w = w_; txt->h = h_;
    txt->text = text ? text : "";
    txt->hwnd = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", text ? text : "",
        WS_CHILD | WS_VISIBLE | ES_READONLY | ES_MULTILINE | ES_AUTOHSCROLL | WS_VSCROLL,
        x, y, w_, h_, parent_hwnd(p), nullptr, GetModuleHandle(nullptr), nullptr);
    if (txt->hwnd) {
        SetWindowLongPtr(txt->hwnd, GWLP_USERDATA, (LONG_PTR)txt);
        SendMessage(txt->hwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    }
    return (AuroraWidget)txt;
}
void aurora_gui_text_set_text(AuroraWidget txt, const char* text) {
    GuiWidget* w = (GuiWidget*)txt;
    w->text = text ? text : "";
    if (w->hwnd) SetWindowTextA(w->hwnd, w->text.c_str());
}
const char* aurora_gui_text_get_text(AuroraWidget txt) {
    return ((GuiWidget*)txt)->text.c_str();
}

/* ════════════════════════════════════════════════════════════
   Button
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_button_new(AuroraWidget parent, const char* text, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* btn = widget_new(AURORA_WIDGET_BUTTON, p);
    btn->x = x; btn->y = y; btn->w = w_; btn->h = h_;
    btn->text = text ? text : "";
    btn->hwnd = CreateWindowExA(0, "BUTTON", text ? text : "",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, x, y, w_, h_,
        parent_hwnd(p), (HMENU)(INT_PTR)btn->id, GetModuleHandle(nullptr), nullptr);
    if (btn->hwnd) {
        SetWindowLongPtr(btn->hwnd, GWLP_USERDATA, (LONG_PTR)btn);
        SendMessage(btn->hwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    }
    return (AuroraWidget)btn;
}
void aurora_gui_button_set_text(AuroraWidget btn, const char* text) {
    GuiWidget* w = (GuiWidget*)btn;
    w->text = text ? text : "";
    if (w->hwnd) SetWindowTextA(w->hwnd, w->text.c_str());
}
const char* aurora_gui_button_get_text(AuroraWidget btn) {
    return ((GuiWidget*)btn)->text.c_str();
}

/* ════════════════════════════════════════════════════════════
   CheckBox
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_checkbox_new(AuroraWidget parent, const char* text, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* cb = widget_new(AURORA_WIDGET_CHECKBOX, p);
    cb->x = x; cb->y = y; cb->w = w_; cb->h = h_;
    cb->text = text ? text : "";
    cb->hwnd = CreateWindowExA(0, "BUTTON", text ? text : "",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, x, y, w_, h_,
        parent_hwnd(p), (HMENU)(INT_PTR)cb->id, GetModuleHandle(nullptr), nullptr);
    if (cb->hwnd) {
        SetWindowLongPtr(cb->hwnd, GWLP_USERDATA, (LONG_PTR)cb);
        SendMessage(cb->hwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    }
    return (AuroraWidget)cb;
}
void aurora_gui_checkbox_set_text(AuroraWidget cb, const char* text) {
    GuiWidget* w = (GuiWidget*)cb;
    w->text = text ? text : "";
    if (w->hwnd) SetWindowTextA(w->hwnd, w->text.c_str());
}
const char* aurora_gui_checkbox_get_text(AuroraWidget cb) { return ((GuiWidget*)cb)->text.c_str(); }
int aurora_gui_checkbox_is_checked(AuroraWidget cb) {
    GuiWidget* w = (GuiWidget*)cb;
    return (w->hwnd && SendMessage(w->hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
}
void aurora_gui_checkbox_set_checked(AuroraWidget cb, int checked) {
    GuiWidget* w = (GuiWidget*)cb;
    if (w->hwnd) SendMessage(w->hwnd, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
}

/* ════════════════════════════════════════════════════════════
   RadioButton
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_radiobutton_new(AuroraWidget parent, const char* text, int x, int y, int w_, int h_, int group_id) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* rb = widget_new(AURORA_WIDGET_RADIOBUTTON, p);
    rb->x = x; rb->y = y; rb->w = w_; rb->h = h_;
    rb->text = text ? text : ""; rb->group_id = group_id;
    rb->hwnd = CreateWindowExA(0, "BUTTON", text ? text : "",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, x, y, w_, h_,
        parent_hwnd(p), (HMENU)(INT_PTR)rb->id, GetModuleHandle(nullptr), nullptr);
    if (rb->hwnd) {
        SetWindowLongPtr(rb->hwnd, GWLP_USERDATA, (LONG_PTR)rb);
        SendMessage(rb->hwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    }
    return (AuroraWidget)rb;
}
void aurora_gui_radiobutton_set_text(AuroraWidget rb, const char* text) {
    GuiWidget* w = (GuiWidget*)rb;
    w->text = text ? text : "";
    if (w->hwnd) SetWindowTextA(w->hwnd, w->text.c_str());
}
const char* aurora_gui_radiobutton_get_text(AuroraWidget rb) { return ((GuiWidget*)rb)->text.c_str(); }
int aurora_gui_radiobutton_is_checked(AuroraWidget rb) {
    GuiWidget* w = (GuiWidget*)rb;
    return (w->hwnd && SendMessage(w->hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
}
void aurora_gui_radiobutton_set_checked(AuroraWidget rb, int checked) {
    GuiWidget* w = (GuiWidget*)rb;
    if (w->hwnd) SendMessage(w->hwnd, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
}

/* ════════════════════════════════════════════════════════════
   Switch (toggle — checkbox with pushlike style)
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_switch_new(AuroraWidget parent, const char* text, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* sw = widget_new(AURORA_WIDGET_SWITCH, p);
    sw->x = x; sw->y = y; sw->w = w_; sw->h = h_;
    sw->text = text ? text : "";
    sw->hwnd = CreateWindowExA(0, "BUTTON", text ? text : "",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE, x, y, w_, h_,
        parent_hwnd(p), (HMENU)(INT_PTR)sw->id, GetModuleHandle(nullptr), nullptr);
    if (sw->hwnd) {
        SetWindowLongPtr(sw->hwnd, GWLP_USERDATA, (LONG_PTR)sw);
        SendMessage(sw->hwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    }
    return (AuroraWidget)sw;
}
int aurora_gui_switch_is_on(AuroraWidget sw) {
    GuiWidget* w = (GuiWidget*)sw;
    return (w->hwnd && SendMessage(w->hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
}
void aurora_gui_switch_set_on(AuroraWidget sw, int on) {
    GuiWidget* w = (GuiWidget*)sw;
    if (w->hwnd) SendMessage(w->hwnd, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
}

/* ════════════════════════════════════════════════════════════
   TextBox
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_textbox_new(AuroraWidget parent, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* tb = widget_new(AURORA_WIDGET_TEXTBOX, p);
    tb->x = x; tb->y = y; tb->w = w_; tb->h = h_;
    tb->hwnd = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL, x, y, w_, h_,
        parent_hwnd(p), (HMENU)(INT_PTR)tb->id, GetModuleHandle(nullptr), nullptr);
    if (tb->hwnd) {
        SetWindowLongPtr(tb->hwnd, GWLP_USERDATA, (LONG_PTR)tb);
        SendMessage(tb->hwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    }
    return (AuroraWidget)tb;
}
void aurora_gui_textbox_set_text(AuroraWidget tb, const char* text) {
    GuiWidget* w = (GuiWidget*)tb;
    w->text = text ? text : "";
    if (w->hwnd) SetWindowTextA(w->hwnd, w->text.c_str());
}
const char* aurora_gui_textbox_get_text(AuroraWidget tb) {
    GuiWidget* w = (GuiWidget*)tb;
    if (w->hwnd) { GetWindowTextA(w->hwnd, g_temp_str, sizeof(g_temp_str)); w->text = g_temp_str; }
    return w->text.c_str();
}
void aurora_gui_textbox_set_readonly(AuroraWidget tb, int readonly) {
    GuiWidget* w = (GuiWidget*)tb;
    if (w->hwnd) SendMessage(w->hwnd, EM_SETREADONLY, readonly ? TRUE : FALSE, 0);
}
void aurora_gui_textbox_set_placeholder(AuroraWidget tb, const char* text) {
    GuiWidget* w = (GuiWidget*)tb;
    if (w->hwnd && w->text.empty()) SetWindowTextA(w->hwnd, text ? text : "");
}
void aurora_gui_textbox_set_multiline(AuroraWidget tb, int multiline) {
    GuiWidget* w = (GuiWidget*)tb;
    if (!w->hwnd) return;
    LONG style = GetWindowLong(w->hwnd, GWL_STYLE);
    if (multiline) style |= ES_MULTILINE | WS_VSCROLL | ES_AUTOVSCROLL;
    else style &= ~(ES_MULTILINE | WS_VSCROLL | ES_AUTOVSCROLL);
    SetWindowLong(w->hwnd, GWL_STYLE, style);
}
int aurora_gui_textbox_get_line_count(AuroraWidget tb) {
    GuiWidget* w = (GuiWidget*)tb;
    return w->hwnd ? (int)SendMessage(w->hwnd, EM_GETLINECOUNT, 0, 0) : 0;
}

/* ════════════════════════════════════════════════════════════
   PasswordBox
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_passwordbox_new(AuroraWidget parent, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* pb = widget_new(AURORA_WIDGET_PASSWORDBOX, p);
    pb->x = x; pb->y = y; pb->w = w_; pb->h = h_;
    pb->hwnd = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL | ES_PASSWORD, x, y, w_, h_,
        parent_hwnd(p), (HMENU)(INT_PTR)pb->id, GetModuleHandle(nullptr), nullptr);
    if (pb->hwnd) {
        SetWindowLongPtr(pb->hwnd, GWLP_USERDATA, (LONG_PTR)pb);
        SendMessage(pb->hwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    }
    return (AuroraWidget)pb;
}
void aurora_gui_passwordbox_set_text(AuroraWidget pb, const char* text) {
    GuiWidget* w = (GuiWidget*)pb; w->text = text ? text : "";
    if (w->hwnd) SetWindowTextA(w->hwnd, w->text.c_str());
}
const char* aurora_gui_passwordbox_get_text(AuroraWidget pb) {
    GuiWidget* w = (GuiWidget*)pb;
    if (w->hwnd) { GetWindowTextA(w->hwnd, g_temp_str, sizeof(g_temp_str)); w->text = g_temp_str; }
    return w->text.c_str();
}

/* ════════════════════════════════════════════════════════════
   Slider (Trackbar)
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_slider_new(AuroraWidget parent, int x, int y, int w_, int h_, int min, int max) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* sl = widget_new(AURORA_WIDGET_SLIDER, p);
    sl->x = x; sl->y = y; sl->w = w_; sl->h = h_; sl->min_val = min; sl->max_val = max;
    ensure_common_ctrl();
    sl->hwnd = CreateWindowExA(0, TRACKBAR_CLASSA, "",
        WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_ENABLESELRANGE, x, y, w_, h_,
        parent_hwnd(p), nullptr, GetModuleHandle(nullptr), nullptr);
    if (sl->hwnd) {
        SetWindowLongPtr(sl->hwnd, GWLP_USERDATA, (LONG_PTR)sl);
        SendMessage(sl->hwnd, TBM_SETRANGE, TRUE, MAKELONG(min, max));
        SendMessage(sl->hwnd, TBM_SETPOS, TRUE, min);
    }
    return (AuroraWidget)sl;
}
int aurora_gui_slider_get_value(AuroraWidget sl) {
    GuiWidget* w = (GuiWidget*)sl;
    return w->hwnd ? (int)SendMessage(w->hwnd, TBM_GETPOS, 0, 0) : 0;
}
void aurora_gui_slider_set_value(AuroraWidget sl, int value) {
    GuiWidget* w = (GuiWidget*)sl;
    if (w->hwnd) SendMessage(w->hwnd, TBM_SETPOS, TRUE, value);
}
void aurora_gui_slider_set_range(AuroraWidget sl, int min, int max) {
    GuiWidget* w = (GuiWidget*)sl; w->min_val = min; w->max_val = max;
    if (w->hwnd) SendMessage(w->hwnd, TBM_SETRANGE, TRUE, MAKELONG(min, max));
}

/* ════════════════════════════════════════════════════════════
   ProgressBar
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_progressbar_new(AuroraWidget parent, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* pb = widget_new(AURORA_WIDGET_PROGRESSBAR, p);
    pb->x = x; pb->y = y; pb->w = w_; pb->h = h_; pb->min_val = 0; pb->max_val = 100;
    ensure_common_ctrl();
    pb->hwnd = CreateWindowExA(0, PROGRESS_CLASSA, "",
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH, x, y, w_, h_,
        parent_hwnd(p), nullptr, GetModuleHandle(nullptr), nullptr);
    if (pb->hwnd) {
        SetWindowLongPtr(pb->hwnd, GWLP_USERDATA, (LONG_PTR)pb);
        SendMessage(pb->hwnd, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    }
    return (AuroraWidget)pb;
}
void aurora_gui_progressbar_set_value(AuroraWidget pb, int value) {
    GuiWidget* w = (GuiWidget*)pb;
    if (w->hwnd) SendMessage(w->hwnd, PBM_SETPOS, value, 0);
}
int aurora_gui_progressbar_get_value(AuroraWidget pb) {
    GuiWidget* w = (GuiWidget*)pb;
    return w->hwnd ? (int)SendMessage(w->hwnd, PBM_GETPOS, 0, 0) : 0;
}
void aurora_gui_progressbar_set_range(AuroraWidget pb, int min, int max) {
    GuiWidget* w = (GuiWidget*)pb; w->min_val = min; w->max_val = max;
    if (w->hwnd) SendMessage(w->hwnd, PBM_SETRANGE, 0, MAKELPARAM(min, max));
}
void aurora_gui_progressbar_set_marquee(AuroraWidget pb, int on) {
    GuiWidget* w = (GuiWidget*)pb;
    if (w->hwnd) SendMessage(w->hwnd, PBM_SETMARQUEE, on ? TRUE : FALSE, 0);
}

/* ════════════════════════════════════════════════════════════
   ComboBox
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_combobox_new(AuroraWidget parent, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* cb = widget_new(AURORA_WIDGET_COMBOBOX, p);
    cb->x = x; cb->y = y; cb->w = w_; cb->h = h_;
    cb->hwnd = CreateWindowExA(0, "COMBOBOX", "",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | CBS_HASSTRINGS | WS_VSCROLL, x, y, w_, h_,
        parent_hwnd(p), (HMENU)(INT_PTR)cb->id, GetModuleHandle(nullptr), nullptr);
    if (cb->hwnd) {
        SetWindowLongPtr(cb->hwnd, GWLP_USERDATA, (LONG_PTR)cb);
        SendMessage(cb->hwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    }
    return (AuroraWidget)cb;
}
void aurora_gui_combobox_add_item(AuroraWidget cb, const char* item) {
    GuiWidget* w = (GuiWidget*)cb;
    w->items.push_back(item ? item : "");
    if (w->hwnd) SendMessageA(w->hwnd, CB_ADDSTRING, 0, (LPARAM)(item ? item : ""));
}
void aurora_gui_combobox_clear(AuroraWidget cb) {
    GuiWidget* w = (GuiWidget*)cb; w->items.clear();
    if (w->hwnd) SendMessage(w->hwnd, CB_RESETCONTENT, 0, 0);
}
int aurora_gui_combobox_get_selected(AuroraWidget cb) {
    GuiWidget* w = (GuiWidget*)cb;
    if (w->hwnd) { LRESULT sel = SendMessage(w->hwnd, CB_GETCURSEL, 0, 0); w->selected_idx = (sel == CB_ERR) ? -1 : (int)sel; }
    return w->selected_idx;
}
void aurora_gui_combobox_set_selected(AuroraWidget cb, int idx) {
    GuiWidget* w = (GuiWidget*)cb; w->selected_idx = idx;
    if (w->hwnd) SendMessage(w->hwnd, CB_SETCURSEL, idx, 0);
}
int aurora_gui_combobox_count(AuroraWidget cb) {
    GuiWidget* w = (GuiWidget*)cb;
    return w->hwnd ? (int)SendMessage(w->hwnd, CB_GETCOUNT, 0, 0) : (int)w->items.size();
}
const char* aurora_gui_combobox_get_item(AuroraWidget cb, int idx) {
    GuiWidget* w = (GuiWidget*)cb;
    return (idx >= 0 && idx < (int)w->items.size()) ? w->items[idx].c_str() : nullptr;
}

/* ════════════════════════════════════════════════════════════
   DropDown (read-only combobox)
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_dropdown_new(AuroraWidget parent, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* dd = widget_new(AURORA_WIDGET_DROPDOWN, p);
    dd->x = x; dd->y = y; dd->w = w_; dd->h = h_;
    dd->hwnd = CreateWindowExA(0, "COMBOBOX", "",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL, x, y, w_, h_,
        parent_hwnd(p), (HMENU)(INT_PTR)dd->id, GetModuleHandle(nullptr), nullptr);
    if (dd->hwnd) {
        SetWindowLongPtr(dd->hwnd, GWLP_USERDATA, (LONG_PTR)dd);
        SendMessage(dd->hwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    }
    return (AuroraWidget)dd;
}
void aurora_gui_dropdown_add_item(AuroraWidget dd, const char* item) {
    GuiWidget* w = (GuiWidget*)dd; w->items.push_back(item ? item : "");
    if (w->hwnd) SendMessageA(w->hwnd, CB_ADDSTRING, 0, (LPARAM)(item ? item : ""));
}
void aurora_gui_dropdown_clear(AuroraWidget dd) {
    GuiWidget* w = (GuiWidget*)dd; w->items.clear();
    if (w->hwnd) SendMessage(w->hwnd, CB_RESETCONTENT, 0, 0);
}
int aurora_gui_dropdown_get_selected(AuroraWidget dd) {
    GuiWidget* w = (GuiWidget*)dd;
    if (w->hwnd) { LRESULT sel = SendMessage(w->hwnd, CB_GETCURSEL, 0, 0); w->selected_idx = (sel == CB_ERR) ? -1 : (int)sel; }
    return w->selected_idx;
}
void aurora_gui_dropdown_set_selected(AuroraWidget dd, int idx) {
    GuiWidget* w = (GuiWidget*)dd; w->selected_idx = idx;
    if (w->hwnd) SendMessage(w->hwnd, CB_SETCURSEL, idx, 0);
}
int aurora_gui_dropdown_count(AuroraWidget dd) {
    GuiWidget* w = (GuiWidget*)dd;
    return w->hwnd ? (int)SendMessage(w->hwnd, CB_GETCOUNT, 0, 0) : (int)w->items.size();
}
const char* aurora_gui_dropdown_get_item(AuroraWidget dd, int idx) {
    GuiWidget* w = (GuiWidget*)dd;
    return (idx >= 0 && idx < (int)w->items.size()) ? w->items[idx].c_str() : nullptr;
}

/* ════════════════════════════════════════════════════════════
   ListBox
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_listbox_new(AuroraWidget parent, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* lb = widget_new(AURORA_WIDGET_LISTBOX, p);
    lb->x = x; lb->y = y; lb->w = w_; lb->h = h_;
    lb->hwnd = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "",
        WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL, x, y, w_, h_,
        parent_hwnd(p), (HMENU)(INT_PTR)lb->id, GetModuleHandle(nullptr), nullptr);
    if (lb->hwnd) {
        SetWindowLongPtr(lb->hwnd, GWLP_USERDATA, (LONG_PTR)lb);
        SendMessage(lb->hwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    }
    return (AuroraWidget)lb;
}
void aurora_gui_listbox_add_item(AuroraWidget lb, const char* item) {
    GuiWidget* w = (GuiWidget*)lb; w->items.push_back(item ? item : "");
    if (w->hwnd) SendMessageA(w->hwnd, LB_ADDSTRING, 0, (LPARAM)(item ? item : ""));
}
void aurora_gui_listbox_clear(AuroraWidget lb) {
    GuiWidget* w = (GuiWidget*)lb; w->items.clear();
    if (w->hwnd) SendMessage(w->hwnd, LB_RESETCONTENT, 0, 0);
}
int aurora_gui_listbox_get_selected(AuroraWidget lb) {
    GuiWidget* w = (GuiWidget*)lb;
    if (w->hwnd) { LRESULT sel = SendMessage(w->hwnd, LB_GETCURSEL, 0, 0); w->selected_idx = (sel == LB_ERR) ? -1 : (int)sel; }
    return w->selected_idx;
}
const char* aurora_gui_listbox_get_item(AuroraWidget lb, int idx) {
    GuiWidget* w = (GuiWidget*)lb;
    return (idx >= 0 && idx < (int)w->items.size()) ? w->items[idx].c_str() : nullptr;
}
int aurora_gui_listbox_count(AuroraWidget lb) { return (int)((GuiWidget*)lb)->items.size(); }

/* ════════════════════════════════════════════════════════════
   TreeView
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_treeview_new(AuroraWidget parent, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* tv = widget_new(AURORA_WIDGET_TREEVIEW, p);
    tv->x = x; tv->y = y; tv->w = w_; tv->h = h_;
    ensure_common_ctrl();
    tv->hwnd = CreateWindowExA(0, WC_TREEVIEWA, "",
        WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | WS_BORDER,
        x, y, w_, h_, parent_hwnd(p), (HMENU)(INT_PTR)tv->id, GetModuleHandle(nullptr), nullptr);
    if (tv->hwnd) SetWindowLongPtr(tv->hwnd, GWLP_USERDATA, (LONG_PTR)tv);
    return (AuroraWidget)tv;
}
AuroraTreeItem aurora_gui_treeview_add_item(AuroraWidget tv, const char* text, AuroraTreeItem parent_item) {
    GuiWidget* w = (GuiWidget*)tv;
    if (!w->hwnd) return nullptr;
    TVINSERTSTRUCTA tvis;
    tvis.hParent = (HTREEITEM)parent_item;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT;
    tvis.item.pszText = (char*)(text ? text : "");
    return (AuroraTreeItem)SendMessageA(w->hwnd, TVM_INSERTITEMA, 0, (LPARAM)&tvis);
}
void aurora_gui_treeview_remove_item(AuroraWidget tv, AuroraTreeItem item) {
    GuiWidget* w = (GuiWidget*)tv;
    if (w->hwnd) SendMessage(w->hwnd, TVM_DELETEITEM, 0, (LPARAM)item);
}
void aurora_gui_treeview_clear(AuroraWidget tv) {
    GuiWidget* w = (GuiWidget*)tv;
    if (w->hwnd) SendMessage(w->hwnd, TVM_DELETEITEM, 0, (LPARAM)TVI_ROOT);
}
AuroraTreeItem aurora_gui_treeview_get_selected(AuroraWidget tv) {
    GuiWidget* w = (GuiWidget*)tv;
    return w->hwnd ? (AuroraTreeItem)SendMessage(w->hwnd, TVM_GETNEXTITEM, TVGN_CARET, 0) : nullptr;
}
void aurora_gui_treeview_expand(AuroraWidget tv, AuroraTreeItem item) {
    GuiWidget* w = (GuiWidget*)tv;
    if (w->hwnd) SendMessage(w->hwnd, TVM_EXPAND, TVE_EXPAND, (LPARAM)item);
}
void aurora_gui_treeview_collapse(AuroraWidget tv, AuroraTreeItem item) {
    GuiWidget* w = (GuiWidget*)tv;
    if (w->hwnd) SendMessage(w->hwnd, TVM_EXPAND, TVE_COLLAPSE, (LPARAM)item);
}
void aurora_gui_treeview_set_item_text(AuroraWidget tv, AuroraTreeItem item, const char* text) {
    GuiWidget* w = (GuiWidget*)tv;
    if (!w->hwnd) return;
    TVITEMA tvi; tvi.hItem = (HTREEITEM)item; tvi.mask = TVIF_TEXT;
    tvi.pszText = (char*)(text ? text : "");
    SendMessageA(w->hwnd, TVM_SETITEMA, 0, (LPARAM)&tvi);
}

/* ════════════════════════════════════════════════════════════
   Table (ListView report mode)
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_table_new(AuroraWidget parent, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* tbl = widget_new(AURORA_WIDGET_TABLE, p);
    tbl->x = x; tbl->y = y; tbl->w = w_; tbl->h = h_;
    ensure_common_ctrl();
    tbl->hwnd = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL, x, y, w_, h_,
        parent_hwnd(p), (HMENU)(INT_PTR)tbl->id, GetModuleHandle(nullptr), nullptr);
    if (tbl->hwnd) {
        SetWindowLongPtr(tbl->hwnd, GWLP_USERDATA, (LONG_PTR)tbl);
        ListView_SetExtendedListViewStyle(tbl->hwnd, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        TableData* td = new TableData(); td->col_count = 0; td->row_count = 0;
        tbl->extra_data = td;
    }
    return (AuroraWidget)tbl;
}
void aurora_gui_table_add_column(AuroraWidget tbl, const char* title, int width) {
    GuiWidget* w = (GuiWidget*)tbl;
    if (!w->hwnd) return;
    TableData* td = (TableData*)w->extra_data;
    LVCOLUMNA lvc; lvc.mask = LVCF_TEXT | LVCF_WIDTH;
    lvc.pszText = (char*)(title ? title : ""); lvc.cx = width;
    SendMessageA(w->hwnd, LVM_INSERTCOLUMNA, td->col_count++, (LPARAM)&lvc);
}
int aurora_gui_table_column_count(AuroraWidget tbl) {
    GuiWidget* w = (GuiWidget*)tbl;
    TableData* td = (TableData*)w->extra_data;
    return td ? td->col_count : 0;
}
AuroraTableItem aurora_gui_table_add_row(AuroraWidget tbl) {
    GuiWidget* w = (GuiWidget*)tbl;
    if (!w->hwnd) return nullptr;
    TableData* td = (TableData*)w->extra_data;
    LVITEMA lvi; lvi.mask = LVIF_TEXT; lvi.iItem = td->row_count; lvi.iSubItem = 0; lvi.pszText = (char*)"";
    SendMessageA(w->hwnd, LVM_INSERTITEMA, 0, (LPARAM)&lvi);
    return (AuroraTableItem)(uintptr_t)(td->row_count++);
}
void aurora_gui_table_set_cell(AuroraWidget tbl, int row, int col, const char* text) {
    GuiWidget* w = (GuiWidget*)tbl; if (!w->hwnd) return;
    LVITEMA lvi; lvi.iSubItem = col; lvi.pszText = (char*)(text ? text : ""); lvi.mask = LVIF_TEXT;
    SendMessageA(w->hwnd, LVM_SETITEMA, 0, (LPARAM)&lvi);
}
const char* aurora_gui_table_get_cell(AuroraWidget tbl, int row, int col) {
    GuiWidget* w = (GuiWidget*)tbl; if (!w->hwnd) return "";
    static char buf[512]; LVITEMA lvi; lvi.iSubItem = col; lvi.pszText = buf; lvi.cchTextMax = sizeof(buf); lvi.mask = LVIF_TEXT;
    SendMessageA(w->hwnd, LVM_GETITEMA, 0, (LPARAM)&lvi); return buf;
}
void aurora_gui_table_remove_row(AuroraWidget tbl, int row) {
    GuiWidget* w = (GuiWidget*)tbl; if (!w->hwnd) return;
    ListView_DeleteItem(w->hwnd, row);
    TableData* td = (TableData*)w->extra_data; if (td && td->row_count > 0) td->row_count--;
}
void aurora_gui_table_clear(AuroraWidget tbl) {
    GuiWidget* w = (GuiWidget*)tbl; if (!w->hwnd) return;
    ListView_DeleteAllItems(w->hwnd);
    TableData* td = (TableData*)w->extra_data; if (td) td->row_count = 0;
}
int aurora_gui_table_get_selected(AuroraWidget tbl) {
    GuiWidget* w = (GuiWidget*)tbl;
    return w->hwnd ? (int)SendMessage(w->hwnd, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED) : -1;
}
int aurora_gui_table_row_count(AuroraWidget tbl) {
    GuiWidget* w = (GuiWidget*)tbl;
    return w->hwnd ? (int)SendMessage(w->hwnd, LVM_GETITEMCOUNT, 0, 0) : 0;
}

/* ════════════════════════════════════════════════════════════
   TabView
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_tabview_new(AuroraWidget parent, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* tv = widget_new(AURORA_WIDGET_TABVIEW, p);
    tv->x = x; tv->y = y; tv->w = w_; tv->h = h_;
    ensure_common_ctrl();
    tv->hwnd = CreateWindowExA(0, WC_TABCONTROLA, "",
        WS_CHILD | WS_VISIBLE | TCS_MULTILINE, x, y, w_, h_,
        parent_hwnd(p), (HMENU)(INT_PTR)tv->id, GetModuleHandle(nullptr), nullptr);
    if (tv->hwnd) {
        SetWindowLongPtr(tv->hwnd, GWLP_USERDATA, (LONG_PTR)tv);
        tv->extra_data = new std::vector<TabPage>();
    }
    return (AuroraWidget)tv;
}
AuroraWidget aurora_gui_tabview_add_page(AuroraWidget tv, const char* title) {
    GuiWidget* w = (GuiWidget*)tv;
    if (!w->hwnd) return nullptr;
    auto* pages = (std::vector<TabPage>*)w->extra_data;
    TCITEMA tci; tci.mask = TCIF_TEXT; tci.pszText = (char*)(title ? title : "");
    int idx = TabCtrl_GetItemCount(w->hwnd);
    SendMessageA(w->hwnd, TCM_INSERTITEMA, idx, (LPARAM)&tci);
    TabPage page; page.title = title ? title : "";
    page.panel = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
        parent_hwnd(w), nullptr, GetModuleHandle(nullptr), nullptr);
    pages->push_back(page);
    return (AuroraWidget)(uintptr_t)idx;
}
int aurora_gui_tabview_get_selected(AuroraWidget tv) {
    GuiWidget* w = (GuiWidget*)tv;
    return w->hwnd ? TabCtrl_GetCurSel(w->hwnd) : -1;
}
void aurora_gui_tabview_set_selected(AuroraWidget tv, int idx) {
    GuiWidget* w = (GuiWidget*)tv;
    if (w->hwnd) TabCtrl_SetCurSel(w->hwnd, idx);
}
int aurora_gui_tabview_page_count(AuroraWidget tv) {
    GuiWidget* w = (GuiWidget*)tv;
    return w->hwnd ? TabCtrl_GetItemCount(w->hwnd) : 0;
}

/* ════════════════════════════════════════════════════════════
   ScrollView
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_scrollview_new(AuroraWidget parent, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* sv = widget_new(AURORA_WIDGET_SCROLLVIEW, p);
    sv->x = x; sv->y = y; sv->w = w_; sv->h = h_;
    sv->hwnd = CreateWindowExA(WS_EX_CLIENTEDGE, "STATIC", "",
        WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL, x, y, w_, h_,
        parent_hwnd(p), nullptr, GetModuleHandle(nullptr), nullptr);
    if (sv->hwnd) SetWindowLongPtr(sv->hwnd, GWLP_USERDATA, (LONG_PTR)sv);
    return (AuroraWidget)sv;
}

/* ════════════════════════════════════════════════════════════
   SplitView
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_splitview_new(AuroraWidget parent, int x, int y, int w_, int h_, int orientation) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* sv = widget_new(AURORA_WIDGET_SPLITVIEW, p);
    sv->x = x; sv->y = y; sv->w = w_; sv->h = h_;
    SplitViewData* data = new SplitViewData();
    data->orientation = orientation;
    data->position = (orientation == AURORA_ORIENT_HORIZONTAL) ? w_ / 2 : h_ / 2;
    sv->extra_data = data;
    sv->hwnd = CreateWindowExA(0, "STATIC", "",
        WS_CHILD | WS_VISIBLE, x, y, w_, h_,
        parent_hwnd(p), nullptr, GetModuleHandle(nullptr), nullptr);
    if (sv->hwnd) SetWindowLongPtr(sv->hwnd, GWLP_USERDATA, (LONG_PTR)sv);
    return (AuroraWidget)sv;
}
void aurora_gui_splitview_set_position(AuroraWidget sv, int pos) {
    GuiWidget* w = (GuiWidget*)sv;
    SplitViewData* d = (SplitViewData*)w->extra_data;
    if (d) d->position = pos;
}
int aurora_gui_splitview_get_position(AuroraWidget sv) {
    GuiWidget* w = (GuiWidget*)sv;
    SplitViewData* d = (SplitViewData*)w->extra_data;
    return d ? d->position : 0;
}
AuroraWidget aurora_gui_splitview_get_pane1(AuroraWidget sv) { (void)sv; return nullptr; }
AuroraWidget aurora_gui_splitview_get_pane2(AuroraWidget sv) { (void)sv; return nullptr; }

/* ════════════════════════════════════════════════════════════
   GroupBox
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_groupbox_new(AuroraWidget parent, const char* title, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* gb = widget_new(AURORA_WIDGET_GROUPBOX, p);
    gb->x = x; gb->y = y; gb->w = w_; gb->h = h_; gb->text = title ? title : "";
    gb->hwnd = CreateWindowExA(0, "BUTTON", title ? title : "",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX, x, y, w_, h_,
        parent_hwnd(p), nullptr, GetModuleHandle(nullptr), nullptr);
    if (gb->hwnd) {
        SetWindowLongPtr(gb->hwnd, GWLP_USERDATA, (LONG_PTR)gb);
        SendMessage(gb->hwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    }
    return (AuroraWidget)gb;
}

/* ════════════════════════════════════════════════════════════
   Image
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_image_new(AuroraWidget parent, const char* path, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* img = widget_new(AURORA_WIDGET_IMAGE, p);
    img->x = x; img->y = y; img->w = w_; img->h = h_;
    img->hwnd = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE | SS_BITMAP,
        x, y, w_, h_, parent_hwnd(p), nullptr, GetModuleHandle(nullptr), nullptr);
    if (img->hwnd) {
        SetWindowLongPtr(img->hwnd, GWLP_USERDATA, (LONG_PTR)img);
        if (path) {
            HBITMAP hbm = (HBITMAP)LoadImageA(nullptr, path, IMAGE_BITMAP, w_, h_, LR_LOADFROMFILE);
            if (hbm) SendMessage(img->hwnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hbm);
        }
    }
    return (AuroraWidget)img;
}
void aurora_gui_image_load(AuroraWidget img, const char* path) {
    GuiWidget* w = (GuiWidget*)img; if (!w->hwnd || !path) return;
    HBITMAP hbm = (HBITMAP)LoadImageA(nullptr, path, IMAGE_BITMAP, w->w, w->h, LR_LOADFROMFILE);
    if (hbm) SendMessage(w->hwnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hbm);
}
void aurora_gui_image_set_data(AuroraWidget img, const unsigned char* data, int len) { (void)img; (void)data; (void)len; }

/* ════════════════════════════════════════════════════════════
   Canvas
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_canvas_new(AuroraWidget parent, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* cv = widget_new(AURORA_WIDGET_CANVAS, p);
    cv->x = x; cv->y = y; cv->w = w_; cv->h = h_;
    cv->hwnd = CreateWindowExA(0, WINDOW_CLASS, "", WS_CHILD | WS_VISIBLE,
        x, y, w_, h_, parent_hwnd(p), nullptr, GetModuleHandle(nullptr), nullptr);
    if (cv->hwnd) SetWindowLongPtr(cv->hwnd, GWLP_USERDATA, (LONG_PTR)cv);
    return (AuroraWidget)cv;
}
void aurora_gui_canvas_set_paint_callback(AuroraWidget cv, AuroraPaintCallback cb, void* user_data) {
    (void)user_data; ((GuiWidget*)cv)->extra_data = (void*)cb;
}
void aurora_gui_canvas_repaint(AuroraWidget cv) {
    GuiWidget* w = (GuiWidget*)cv; if (w->hwnd) InvalidateRect(w->hwnd, nullptr, TRUE);
}

/* ════════════════════════════════════════════════════════════
   Menu / MenuBar
   ════════════════════════════════════════════════════════════ */
AuroraMenu aurora_gui_menu_bar_new(AuroraWidget parent) {
    GuiWidget* p = (GuiWidget*)parent; if (!p || !p->hwnd) return nullptr;
    HMENU hmenu = CreateMenu(); MenuData* md = new MenuData(); md->hmenu = hmenu;
    GuiWidget* w = widget_new(AURORA_WIDGET_MENUBAR, p); w->extra_data = md;
    SetMenu(p->hwnd, hmenu); return (AuroraMenu)w;
}
AuroraMenu aurora_gui_menu_new(const char* text) {
    HMENU hmenu = CreatePopupMenu(); MenuData* md = new MenuData(); md->hmenu = hmenu;
    GuiWidget* w = widget_new(AURORA_WIDGET_MENUBAR, nullptr); w->extra_data = md;
    w->text = text ? text : ""; return (AuroraMenu)w;
}
void aurora_gui_menu_add_item(AuroraMenu menu, const char* text, int id) {
    MenuData* md = (MenuData*)((GuiWidget*)menu)->extra_data;
    if (md) AppendMenuA(md->hmenu, MF_STRING, id, text ? text : "");
}
void aurora_gui_menu_add_separator(AuroraMenu menu) {
    MenuData* md = (MenuData*)((GuiWidget*)menu)->extra_data;
    if (md) AppendMenuA(md->hmenu, MF_SEPARATOR, 0, nullptr);
}
void aurora_gui_menu_add_submenu(AuroraMenu menu, AuroraMenu submenu) {
    GuiWidget* sw = (GuiWidget*)submenu;
    MenuData* md = (MenuData*)((GuiWidget*)menu)->extra_data;
    MenuData* sd = (MenuData*)sw->extra_data;
    if (md && sd) { AppendMenuA(md->hmenu, MF_POPUP, (UINT_PTR)sd->hmenu, sw->text.c_str()); md->submenus.push_back(sd); }
}
void aurora_gui_menu_bar_add_menu(AuroraMenu menubar, AuroraMenu menu) { aurora_gui_menu_add_submenu(menubar, menu); }
void aurora_gui_menu_set_checked(AuroraMenu menu, int id, int checked) {
    MenuData* md = (MenuData*)((GuiWidget*)menu)->extra_data;
    if (md) CheckMenuItem(md->hmenu, id, MF_BYCOMMAND | (checked ? MF_CHECKED : MF_UNCHECKED));
}
void aurora_gui_menu_set_enabled(AuroraMenu menu, int id, int enabled) {
    MenuData* md = (MenuData*)((GuiWidget*)menu)->extra_data;
    if (md) EnableMenuItem(md->hmenu, id, MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_GRAYED));
}

/* ════════════════════════════════════════════════════════════
   Toolbar
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_toolbar_new(AuroraWidget parent, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* tb = widget_new(AURORA_WIDGET_TOOLBAR, p);
    tb->x = x; tb->y = y; tb->w = w_; tb->h = h_; ensure_common_ctrl();
    tb->hwnd = CreateWindowExA(0, TOOLBARCLASSNAMEA, "",
        WS_CHILD | WS_VISIBLE | TBSTYLE_TOOLTIPS | TBSTYLE_FLAT | CCS_ADJUSTABLE,
        x, y, w_, h_, parent_hwnd(p), nullptr, GetModuleHandle(nullptr), nullptr);
    if (tb->hwnd) {
        SetWindowLongPtr(tb->hwnd, GWLP_USERDATA, (LONG_PTR)tb);
        SendMessage(tb->hwnd, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
        ToolbarData* tbd = new ToolbarData(); tbd->next_id = 1; tb->extra_data = tbd;
    }
    return (AuroraWidget)tb;
}
void aurora_gui_toolbar_add_button(AuroraWidget tb, const char* text, int id) {
    GuiWidget* w = (GuiWidget*)tb; if (!w->hwnd) return;
    ToolbarData* tbd = (ToolbarData*)w->extra_data;
    TBBUTTON tbb = {}; tbb.idCommand = id ? id : (tbd ? tbd->next_id++ : 1);
    tbb.fsState = TBSTATE_ENABLED; tbb.fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
    tbb.iString = (INT_PTR)(text ? text : "");
    SendMessage(w->hwnd, TB_ADDBUTTONS, 1, (LPARAM)&tbb);
}
void aurora_gui_toolbar_add_separator(AuroraWidget tb) {
    GuiWidget* w = (GuiWidget*)tb; if (!w->hwnd) return;
    TBBUTTON tbb = {}; tbb.fsState = TBSTATE_ENABLED; tbb.fsStyle = BTNS_SEP;
    SendMessage(w->hwnd, TB_ADDBUTTONS, 1, (LPARAM)&tbb);
}

/* ════════════════════════════════════════════════════════════
   StatusBar
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_statusbar_new(AuroraWidget parent, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* sb = widget_new(AURORA_WIDGET_STATUSBAR, p);
    sb->x = x; sb->y = y; sb->w = w_; sb->h = h_; ensure_common_ctrl();
    sb->hwnd = CreateWindowExA(0, STATUSCLASSNAMEA, "",
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, x, y, w_, h_,
        parent_hwnd(p), nullptr, GetModuleHandle(nullptr), nullptr);
    if (sb->hwnd) {
        SetWindowLongPtr(sb->hwnd, GWLP_USERDATA, (LONG_PTR)sb);
        SendMessage(sb->hwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    }
    return (AuroraWidget)sb;
}
void aurora_gui_statusbar_set_text(AuroraWidget sb, const char* text) {
    GuiWidget* w = (GuiWidget*)sb; w->text = text ? text : "";
    if (w->hwnd) SendMessageA(w->hwnd, SB_SETTEXTA, 0, (LPARAM)(text ? text : ""));
}
const char* aurora_gui_statusbar_get_text(AuroraWidget sb) { return ((GuiWidget*)sb)->text.c_str(); }
void aurora_gui_statusbar_set_parts(AuroraWidget sb, const int* widths, int count) {
    GuiWidget* w = (GuiWidget*)sb;
    if (w->hwnd) SendMessage(w->hwnd, SB_SETPARTS, count, (LPARAM)widths);
}

/* ════════════════════════════════════════════════════════════
   Dialog
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_dialog_new(AuroraWidget parent, const char* title, int width, int height) {
    GuiWidget* p = (GuiWidget*)parent; register_window_class();
    GuiWidget* dlg = widget_new(AURORA_WIDGET_DIALOG, p);
    dlg->hwnd = CreateWindowExA(0, WINDOW_CLASS, title ? title : "Dialog",
        WS_CAPTION | WS_SYSMENU | WS_SIZEBOX | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        p ? p->hwnd : nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    if (dlg->hwnd) SetWindowLongPtr(dlg->hwnd, GWLP_USERDATA, (LONG_PTR)dlg);
    return (AuroraWidget)dlg;
}
int aurora_gui_dialog_show_modal(AuroraWidget dlg) {
    GuiWidget* w = (GuiWidget*)dlg; if (!w || !w->hwnd) return 0;
    ShowWindow(w->hwnd, SW_SHOW); MSG msg;
    while (IsWindow(w->hwnd) && GetMessage(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessage(w->hwnd, &msg)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    }
    return 0;
}
void aurora_gui_dialog_close(AuroraWidget dlg) {
    GuiWidget* w = (GuiWidget*)dlg; if (w->hwnd) DestroyWindow(w->hwnd);
}

/* ════════════════════════════════════════════════════════════
   MessageBox
   ════════════════════════════════════════════════════════════ */
int aurora_gui_messagebox_show(AuroraWidget parent, const char* title, const char* message, int type) {
    GuiWidget* p = (GuiWidget*)parent;
    UINT flags = MB_OK;
    switch (type) {
        case AURORA_MSGBOX_OK_CANCEL:      flags = MB_OKCANCEL; break;
        case AURORA_MSGBOX_YES_NO:         flags = MB_YESNO; break;
        case AURORA_MSGBOX_YES_NO_CANCEL:  flags = MB_YESNOCANCEL; break;
        case AURORA_MSGBOX_RETRY_CANCEL:   flags = MB_RETRYCANCEL; break;
    }
    int ret = MessageBoxA(p ? p->hwnd : nullptr, message ? message : "", title ? title : "", flags);
    switch (ret) {
        case IDOK: return AURORA_MSGBOX_RESULT_OK; case IDCANCEL: return AURORA_MSGBOX_RESULT_CANCEL;
        case IDYES: return AURORA_MSGBOX_RESULT_YES; case IDNO: return AURORA_MSGBOX_RESULT_NO;
        case IDRETRY: return AURORA_MSGBOX_RESULT_RETRY; default: return 0;
    }
}

/* ════════════════════════════════════════════════════════════
   FilePicker
   ════════════════════════════════════════════════════════════ */
const char* aurora_gui_file_open_dialog(AuroraWidget parent, const char* title, const char* filter) {
    GuiWidget* p = (GuiWidget*)parent;
    ZeroMemory(&g_ofn, sizeof(g_ofn)); g_ofn.lStructSize = sizeof(g_ofn);
    g_ofn.hwndOwner = p ? p->hwnd : nullptr; g_ofn.lpstrFile = g_ofn_file; g_ofn.nMaxFile = sizeof(g_ofn_file);
    g_ofn.lpstrFile[0] = '\0'; g_ofn.lpstrTitle = title;
    g_ofn.lpstrFilter = filter ? filter : "All Files\0*.*\0";
    g_ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    return GetOpenFileNameA(&g_ofn) ? g_ofn.lpstrFile : nullptr;
}
const char* aurora_gui_file_save_dialog(AuroraWidget parent, const char* title, const char* filter) {
    GuiWidget* p = (GuiWidget*)parent;
    ZeroMemory(&g_ofn, sizeof(g_ofn)); g_ofn.lStructSize = sizeof(g_ofn);
    g_ofn.hwndOwner = p ? p->hwnd : nullptr; g_ofn.lpstrFile = g_ofn_file; g_ofn.nMaxFile = sizeof(g_ofn_file);
    g_ofn.lpstrFile[0] = '\0'; g_ofn.lpstrTitle = title;
    g_ofn.lpstrFilter = filter ? filter : "All Files\0*.*\0";
    g_ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
    return GetSaveFileNameA(&g_ofn) ? g_ofn.lpstrFile : nullptr;
}
const char* aurora_gui_folder_select_dialog(AuroraWidget parent, const char* title) {
    static char folder[4096];
    BROWSEINFOA bi = {0}; bi.hwndOwner = parent ? ((GuiWidget*)parent)->hwnd : nullptr;
    bi.lpszTitle = title; bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl && SHGetPathFromIDListA(pidl, folder)) {
        IMalloc* imalloc = nullptr;
        if (SUCCEEDED(SHGetMalloc(&imalloc))) { imalloc->Free(pidl); imalloc->Release(); }
        return folder;
    }
    return nullptr;
}

/* ════════════════════════════════════════════════════════════
   ColorPicker
   ════════════════════════════════════════════════════════════ */
int aurora_gui_color_picker_dialog(AuroraWidget parent, unsigned int initial_color) {
    GuiWidget* p = (GuiWidget*)parent;
    static COLORREF cust_colors[16] = {0};
    CHOOSECOLORA cc; ZeroMemory(&cc, sizeof(cc)); cc.lStructSize = sizeof(cc);
    cc.hwndOwner = p ? p->hwnd : nullptr;
    cc.rgbResult = RGB(GetRValue(initial_color), GetGValue(initial_color), GetBValue(initial_color));
    cc.lpCustColors = cust_colors; cc.Flags = CC_RGBINIT | CC_FULLOPEN;
    return ChooseColorA(&cc) ? (int)RGB(GetRValue(cc.rgbResult), GetGValue(cc.rgbResult), GetBValue(cc.rgbResult)) : -1;
}

/* ════════════════════════════════════════════════════════════
   FontPicker
   ════════════════════════════════════════════════════════════ */
int aurora_gui_font_picker_dialog(AuroraWidget parent, AuroraFontInfo* font_info) {
    if (!font_info) return 0;
    LOGFONTA lf = {0}; lf.lfHeight = -12; lf.lfWeight = FW_NORMAL;
    strcpy_s(lf.lfFaceName, sizeof(lf.lfFaceName), "Segoe UI");
    CHOOSEFONTA cf; ZeroMemory(&cf, sizeof(cf)); cf.lStructSize = sizeof(cf);
    cf.hwndOwner = parent ? ((GuiWidget*)parent)->hwnd : nullptr;
    cf.lpLogFont = &lf; cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_EFFECTS | CF_SCREENFONTS;
    if (ChooseFontA(&cf)) {
        strncpy_s(font_info->name, sizeof(font_info->name), lf.lfFaceName, _TRUNCATE);
        font_info->size = abs(lf.lfHeight); font_info->bold = lf.lfWeight >= FW_BOLD ? 1 : 0;
        font_info->italic = lf.lfItalic ? 1 : 0; font_info->underline = lf.lfUnderline ? 1 : 0;
        font_info->strikethrough = lf.lfStrikeOut ? 1 : 0; font_info->color = (unsigned int)cf.rgbColors;
        return 1;
    }
    return 0;
}

/* ════════════════════════════════════════════════════════════
   Notification (system tray)
   ════════════════════════════════════════════════════════════ */
int aurora_gui_notification_show(AuroraWidget parent, const char* title, const char* message, int icon_type) {
    GuiWidget* p = (GuiWidget*)parent; HWND hwnd = p ? p->hwnd : nullptr;
    if (!hwnd) return -1;
    NOTIFYICONDATAA nid = {0}; nid.cbSize = sizeof(nid); nid.hWnd = hwnd; nid.uID = 1; nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = icon_type == 0 ? NIIF_INFO : (icon_type == 1 ? NIIF_WARNING : NIIF_ERROR);
    strncpy_s(nid.szInfoTitle, sizeof(nid.szInfoTitle), title ? title : "", _TRUNCATE);
    strncpy_s(nid.szInfo, sizeof(nid.szInfo), message ? message : "", _TRUNCATE);
    Shell_NotifyIconA(NIM_ADD, &nid); g_tray_icons[hwnd] = nid;
    return 0;
}
void aurora_gui_notification_remove(AuroraWidget parent) {
    GuiWidget* p = (GuiWidget*)parent; HWND hwnd = p ? p->hwnd : nullptr;
    if (!hwnd) return;
    auto it = g_tray_icons.find(hwnd);
    if (it != g_tray_icons.end()) { Shell_NotifyIconA(NIM_DELETE, &it->second); g_tray_icons.erase(it); }
}

/* ════════════════════════════════════════════════════════════
   Clipboard
   ════════════════════════════════════════════════════════════ */
int aurora_gui_clipboard_set_text(const char* text) {
    if (!text || !OpenClipboard(nullptr)) return 0;
    EmptyClipboard();
    HGLOBAL hglb = GlobalAlloc(GMEM_MOVEABLE, (strlen(text) + 1) * sizeof(char));
    if (!hglb) { CloseClipboard(); return 0; }
    char* buf = (char*)GlobalLock(hglb); strcpy(buf, text);
    GlobalUnlock(hglb); SetClipboardData(CF_TEXT, hglb); CloseClipboard();
    return 1;
}
const char* aurora_gui_clipboard_get_text(void) {
    if (!OpenClipboard(nullptr)) return nullptr;
    HANDLE hdata = GetClipboardData(CF_TEXT);
    if (!hdata) { CloseClipboard(); return nullptr; }
    char* buf = (char*)GlobalLock(hdata);
    if (buf) strncpy_s(g_temp_str, sizeof(g_temp_str), buf, _TRUNCATE);
    GlobalUnlock(hdata); CloseClipboard();
    return g_temp_str;
}

/* ════════════════════════════════════════════════════════════
   Cursor
   ════════════════════════════════════════════════════════════ */
void aurora_gui_cursor_set(int cursor_type) {
    if (cursor_type >= 0 && cursor_type < 11) SetCursor(LoadCursor(nullptr, cursor_ids[cursor_type]));
}
int aurora_gui_cursor_get(void) { return AURORA_CURSOR_ARROW; }

/* ════════════════════════════════════════════════════════════
   Keyboard
   ════════════════════════════════════════════════════════════ */
int aurora_gui_keyboard_is_key_down(int virtual_key) { return (GetAsyncKeyState(virtual_key) & 0x8000) ? 1 : 0; }
int aurora_gui_keyboard_get_modifiers(void) {
    int mods = 0;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) mods |= 1;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) mods |= 2;
    if (GetAsyncKeyState(VK_MENU) & 0x8000) mods |= 4;
    return mods;
}

/* ════════════════════════════════════════════════════════════
   Mouse
   ════════════════════════════════════════════════════════════ */
int aurora_gui_mouse_get_x(void) { POINT pt; GetCursorPos(&pt); return pt.x; }
int aurora_gui_mouse_get_y(void) { POINT pt; GetCursorPos(&pt); return pt.y; }
int aurora_gui_mouse_button_down(int button) {
    if (button == 0) return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) ? 1 : 0;
    if (button == 1) return (GetAsyncKeyState(VK_RBUTTON) & 0x8000) ? 1 : 0;
    if (button == 2) return (GetAsyncKeyState(VK_MBUTTON) & 0x8000) ? 1 : 0;
    return 0;
}
void aurora_gui_mouse_set_pos(int x, int y) { SetCursorPos(x, y); }

/* ════════════════════════════════════════════════════════════
   Layout System — data structures
   ════════════════════════════════════════════════════════════ */
struct LayoutChildInfo {
    GuiWidget* widget;
    int flex;
    int fit;
    int grid_col, grid_row, colspan, rowspan;
};

struct LayoutData {
    int orientation;
    int main_align;
    int cross_align;
    int spacing;
    int columns;
    std::vector<LayoutChildInfo> children;
};

/* ── Layout recalculation ── */
static void layout_recalc(GuiWidget* w) {
    if (!w || !w->hwnd || !w->extra_data) return;
    LayoutData* ld = (LayoutData*)w->extra_data;
    auto& ch = ld->children;
    if (ch.empty()) return;

    RECT rc; GetClientRect(w->hwnd, &rc);
    int cw = rc.right - rc.left;
    int ch_ = rc.bottom - rc.top;

    if (w->type == AURORA_WIDGET_STACK) {
        for (auto& c : ch) {
            if (c.widget->hwnd)
                MoveWindow(c.widget->hwnd, 0, 0, cw, ch_, TRUE);
        }
        return;
    }

    if (w->type == AURORA_WIDGET_GRID && ld->columns > 0) {
        int cols = ld->columns;
        int gap = ld->spacing;
        int cell_w = (cw - gap * (cols - 1)) / cols;
        int cell_h = ch_;
        for (auto& c : ch) {
            int col = c.grid_col;
            int row = c.grid_row;
            int cs = c.colspan > 0 ? c.colspan : 1;
            int rs = c.rowspan > 0 ? c.rowspan : 1;
            int x = col * (cell_w + gap);
            int y = row * (cell_h + gap);
            if (c.widget->hwnd)
                MoveWindow(c.widget->hwnd, x, y, cell_w * cs + gap * (cs - 1), cell_h * rs + gap * (rs - 1), TRUE);
        }
        return;
    }

    /* Row / Column / Wrap / Flow — flexbox-like */
    bool horz = (w->type == AURORA_WIDGET_ROW || w->type == AURORA_WIDGET_WRAP || w->type == AURORA_WIDGET_FLOW);
    int gap = ld->spacing;

    /* Calculate total flex and fixed size */
    int total_flex = 0;
    int fixed = 0;
    for (auto& c : ch) {
        if (c.flex > 0) total_flex += c.flex;
        else fixed += (horz ? c.widget->w : c.widget->h);
    }
    int gaps = gap * ((std::max)((int)ch.size() - 1, 0));
    int available = (horz ? cw : ch_) - gaps;
    int remaining = available - fixed;
    float flex_unit = (total_flex > 0 && remaining > 0) ? (float)remaining / (float)total_flex : 0;

    int pos = 0;
    for (auto& c : ch) {
        int ws, hs;
        if (c.flex > 0) {
            ws = horz ? (int)(flex_unit * c.flex) : (cw / (int)ch.size());
            hs = horz ? ch_ : (int)(flex_unit * c.flex);
            if (ws < 0) ws = 0;
            if (hs < 0) hs = 0;
        } else {
            ws = horz ? c.widget->w : cw;
            hs = horz ? ch_ : c.widget->h;
        }
        int x = horz ? pos : 0;
        int y = horz ? 0 : pos;
        if (c.widget->hwnd)
            MoveWindow(c.widget->hwnd, x, y, ws, hs, TRUE);
        pos += (horz ? ws : hs) + gap;
    }
}

/* ════════════════════════════════════════════════════════════
   Layout containers
   ════════════════════════════════════════════════════════════ */
static GuiWidget* layout_container_new(int type, GuiWidget* parent, int x, int y, int w, int h) {
    GuiWidget* lw = widget_new(type, parent);
    lw->x = x; lw->y = y; lw->w = w; lw->h = h;
    lw->hwnd = CreateWindowExA(0, "STATIC", "",
        WS_CHILD | WS_VISIBLE, x, y, w, h,
        parent_hwnd(parent), nullptr, GetModuleHandle(nullptr), nullptr);
    if (lw->hwnd) {
        SetWindowLongPtr(lw->hwnd, GWLP_USERDATA, (LONG_PTR)lw);
        LayoutData* ld = new LayoutData();
        ld->orientation = (type == AURORA_WIDGET_COLUMN) ? AURORA_ORIENT_VERTICAL : AURORA_ORIENT_HORIZONTAL;
        ld->main_align = AURORA_MAIN_START;
        ld->cross_align = AURORA_CROSS_STRETCH;
        ld->spacing = 0;
        ld->columns = 1;
        lw->extra_data = ld;
    }
    return lw;
}

AuroraWidget aurora_gui_row_new(AuroraWidget parent, int x, int y, int w, int h) {
    return (AuroraWidget)layout_container_new(AURORA_WIDGET_ROW, (GuiWidget*)parent, x, y, w, h);
}
AuroraWidget aurora_gui_column_new(AuroraWidget parent, int x, int y, int w, int h) {
    return (AuroraWidget)layout_container_new(AURORA_WIDGET_COLUMN, (GuiWidget*)parent, x, y, w, h);
}
AuroraWidget aurora_gui_stack_new(AuroraWidget parent, int x, int y, int w, int h) {
    return (AuroraWidget)layout_container_new(AURORA_WIDGET_STACK, (GuiWidget*)parent, x, y, w, h);
}
AuroraWidget aurora_gui_grid_new(AuroraWidget parent, int x, int y, int w, int h, int columns) {
    GuiWidget* g = layout_container_new(AURORA_WIDGET_GRID, (GuiWidget*)parent, x, y, w, h);
    if (g && g->extra_data) ((LayoutData*)g->extra_data)->columns = columns > 0 ? columns : 1;
    return (AuroraWidget)g;
}
AuroraWidget aurora_gui_wrap_new(AuroraWidget parent, int x, int y, int w, int h) {
    return (AuroraWidget)layout_container_new(AURORA_WIDGET_WRAP, (GuiWidget*)parent, x, y, w, h);
}
AuroraWidget aurora_gui_flow_new(AuroraWidget parent, int x, int y, int w, int h) {
    return (AuroraWidget)layout_container_new(AURORA_WIDGET_FLOW, (GuiWidget*)parent, x, y, w, h);
}

/* ── Child management ── */
void aurora_gui_layout_add_child(AuroraWidget layout, AuroraWidget child) {
    GuiWidget* lw = (GuiWidget*)layout;
    GuiWidget* cw = (GuiWidget*)child;
    if (!lw || !lw->extra_data || !cw) return;
    LayoutData* ld = (LayoutData*)lw->extra_data;
    LayoutChildInfo ci; ci.widget = cw; ci.flex = 0; ci.fit = AURORA_FIT_LOOSE;
    ci.grid_col = ci.grid_row = 0; ci.colspan = 1; ci.rowspan = 1;
    ld->children.push_back(ci);
    cw->parent = lw;
    if (cw->hwnd) SetParent(cw->hwnd, lw->hwnd);
    layout_recalc(lw);
}
void aurora_gui_layout_remove_child(AuroraWidget layout, int index) {
    GuiWidget* lw = (GuiWidget*)layout;
    if (!lw || !lw->extra_data) return;
    LayoutData* ld = (LayoutData*)lw->extra_data;
    if (index < 0 || index >= (int)ld->children.size()) return;
    ld->children.erase(ld->children.begin() + index);
    layout_recalc(lw);
}
void aurora_gui_layout_clear(AuroraWidget layout) {
    GuiWidget* lw = (GuiWidget*)layout;
    if (!lw || !lw->extra_data) return;
    ((LayoutData*)lw->extra_data)->children.clear();
}
int aurora_gui_layout_child_count(AuroraWidget layout) {
    GuiWidget* lw = (GuiWidget*)layout;
    if (!lw || !lw->extra_data) return 0;
    return (int)((LayoutData*)lw->extra_data)->children.size();
}
void aurora_gui_layout_recalc(AuroraWidget layout) {
    layout_recalc((GuiWidget*)layout);
}

/* ── Layout properties ── */
void aurora_gui_layout_set_main_align(AuroraWidget layout, int align) {
    GuiWidget* lw = (GuiWidget*)layout;
    if (lw && lw->extra_data) { ((LayoutData*)lw->extra_data)->main_align = align; layout_recalc(lw); }
}
void aurora_gui_layout_set_cross_align(AuroraWidget layout, int align) {
    GuiWidget* lw = (GuiWidget*)layout;
    if (lw && lw->extra_data) { ((LayoutData*)lw->extra_data)->cross_align = align; layout_recalc(lw); }
}
void aurora_gui_layout_set_spacing(AuroraWidget layout, int spacing) {
    GuiWidget* lw = (GuiWidget*)layout;
    if (lw && lw->extra_data) { ((LayoutData*)lw->extra_data)->spacing = spacing; layout_recalc(lw); }
}
void aurora_gui_layout_child_set_flex(AuroraWidget layout, AuroraWidget child, int flex) {
    GuiWidget* lw = (GuiWidget*)layout;
    if (!lw || !lw->extra_data) return;
    LayoutData* ld = (LayoutData*)lw->extra_data;
    for (auto& c : ld->children) { if (c.widget == child) { c.flex = flex; break; } }
    layout_recalc(lw);
}
void aurora_gui_layout_child_set_fit(AuroraWidget layout, AuroraWidget child, int fit) {
    GuiWidget* lw = (GuiWidget*)layout;
    if (!lw || !lw->extra_data) return;
    LayoutData* ld = (LayoutData*)lw->extra_data;
    for (auto& c : ld->children) { if (c.widget == child) { c.fit = fit; break; } }
    layout_recalc(lw);
}

/* ── Grid child position ── */
void aurora_gui_grid_set_child_pos(AuroraWidget grid, AuroraWidget child, int col, int row, int colspan, int rowspan) {
    GuiWidget* gw = (GuiWidget*)grid;
    if (!gw || !gw->extra_data) return;
    LayoutData* ld = (LayoutData*)gw->extra_data;
    for (auto& c : ld->children) {
        if (c.widget == child) {
            c.grid_col = col; c.grid_row = row; c.colspan = colspan; c.rowspan = rowspan;
            break;
        }
    }
    layout_recalc(gw);
}

/* ════════════════════════════════════════════════════════════
   Layout single-child wrappers
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_spacer_new(AuroraWidget parent, int flex) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* sp = widget_new(AURORA_WIDGET_SPACER, p);
    sp->x = 0; sp->y = 0; sp->w = 0; sp->h = 0;
    sp->hwnd = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
        parent_hwnd(p), nullptr, GetModuleHandle(nullptr), nullptr);
    if (sp->hwnd) SetWindowLongPtr(sp->hwnd, GWLP_USERDATA, (LONG_PTR)sp);
    if (p && p->extra_data) {
        LayoutData* ld = (LayoutData*)p->extra_data;
        LayoutChildInfo ci; ci.widget = sp; ci.flex = flex; ci.fit = AURORA_FIT_LOOSE;
        ci.grid_col = ci.grid_row = 0; ci.colspan = 1; ci.rowspan = 1;
        ld->children.push_back(ci);
        sp->parent = p;
        if (sp->hwnd) SetParent(sp->hwnd, p->hwnd);
        layout_recalc(p);
    }
    return (AuroraWidget)sp;
}

struct WrapperData {
    GuiWidget* child;
    int pad_l, pad_t, pad_r, pad_b;
    int align_x, align_y;
    int flex;
    float ratio;
    unsigned int bg;
};

static GuiWidget* wrapper_new(int type, GuiWidget* parent, GuiWidget* child, int w, int h) {
    GuiWidget* wp = widget_new(type, parent);
    wp->x = 0; wp->y = 0; wp->w = w; wp->h = h;
    wp->hwnd = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 0, 0, w, h,
        parent_hwnd(parent), nullptr, GetModuleHandle(nullptr), nullptr);
    if (wp->hwnd) SetWindowLongPtr(wp->hwnd, GWLP_USERDATA, (LONG_PTR)wp);
    WrapperData* wd = new WrapperData();
    wd->child = child; wd->pad_l = wd->pad_t = wd->pad_r = wd->pad_b = 0;
    wd->align_x = AURORA_ALIGN_CENTER; wd->align_y = AURORA_ALIGN_CENTER;
    wd->flex = 0; wd->ratio = 1.0f; wd->bg = 0xFFFFFFFF;
    wp->extra_data = wd;
    if (child) { child->parent = wp; if (child->hwnd) SetParent(child->hwnd, wp->hwnd); }
    return wp;
}

static void wrapper_reposition(GuiWidget* wp) {
    if (!wp || !wp->extra_data) return;
    WrapperData* wd = (WrapperData*)wp->extra_data;
    if (!wd->child || !wd->child->hwnd) return;
    RECT rc; GetClientRect(wp->hwnd, &rc);
    int cw = rc.right - rc.left;
    int ch_ = rc.bottom - rc.top;

    if (wp->type == AURORA_WIDGET_PADDING || wp->type == AURORA_WIDGET_MARGIN) {
        int l = wd->pad_l, t = wd->pad_t, r = wd->pad_r, b = wd->pad_b;
        int wc = cw - l - r; int hc = ch_ - t - b;
        if (wc < 0) wc = 0; if (hc < 0) hc = 0;
        MoveWindow(wd->child->hwnd, l, t, wc, hc, TRUE);
    } else if (wp->type == AURORA_WIDGET_CENTER || wp->type == AURORA_WIDGET_ALIGN) {
        int cw_child = wd->child->w; int ch_child = wd->child->h;
        int cx = (cw > cw_child) ? (cw - cw_child) / 2 : 0;
        int cy = (ch_ > ch_child) ? (ch_ - ch_child) / 2 : 0;
        MoveWindow(wd->child->hwnd, cx, cy, cw_child, ch_child, TRUE);
    }
}

AuroraWidget aurora_gui_padding_new(AuroraWidget parent, AuroraWidget child, int left, int top, int right, int bottom) {
    GuiWidget* wp = wrapper_new(AURORA_WIDGET_PADDING, (GuiWidget*)parent, (GuiWidget*)child, 100, 100);
    WrapperData* wd = (WrapperData*)wp->extra_data;
    wd->pad_l = left; wd->pad_t = top; wd->pad_r = right; wd->pad_b = bottom;
    wrapper_reposition(wp);
    return (AuroraWidget)wp;
}
AuroraWidget aurora_gui_margin_new(AuroraWidget parent, AuroraWidget child, int left, int top, int right, int bottom) {
    GuiWidget* wp = wrapper_new(AURORA_WIDGET_MARGIN, (GuiWidget*)parent, (GuiWidget*)child, 100, 100);
    WrapperData* wd = (WrapperData*)wp->extra_data;
    wd->pad_l = left; wd->pad_t = top; wd->pad_r = right; wd->pad_b = bottom;
    wrapper_reposition(wp);
    return (AuroraWidget)wp;
}
AuroraWidget aurora_gui_center_new(AuroraWidget parent, AuroraWidget child) {
    GuiWidget* wp = wrapper_new(AURORA_WIDGET_CENTER, (GuiWidget*)parent, (GuiWidget*)child, 100, 100);
    wrapper_reposition(wp);
    return (AuroraWidget)wp;
}
AuroraWidget aurora_gui_align_new(AuroraWidget parent, AuroraWidget child, int align_x, int align_y) {
    GuiWidget* wp = wrapper_new(AURORA_WIDGET_ALIGN, (GuiWidget*)parent, (GuiWidget*)child, 100, 100);
    WrapperData* wd = (WrapperData*)wp->extra_data;
    wd->align_x = align_x; wd->align_y = align_y;
    wrapper_reposition(wp);
    return (AuroraWidget)wp;
}
AuroraWidget aurora_gui_expand_new(AuroraWidget parent, AuroraWidget child, int flex) {
    GuiWidget* wp = wrapper_new(AURORA_WIDGET_EXPAND, (GuiWidget*)parent, (GuiWidget*)child, 100, 100);
    WrapperData* wd = (WrapperData*)wp->extra_data;
    wd->flex = flex;
    if (parent && ((GuiWidget*)parent)->extra_data) {
        LayoutData* ld = (LayoutData*)((GuiWidget*)parent)->extra_data;
        LayoutChildInfo ci; ci.widget = wp; ci.flex = flex; ci.fit = AURORA_FIT_TIGHT;
        ci.grid_col = ci.grid_row = 0; ci.colspan = 1; ci.rowspan = 1;
        ld->children.push_back(ci);
        layout_recalc((GuiWidget*)parent);
    }
    return (AuroraWidget)wp;
}
AuroraWidget aurora_gui_flexible_new(AuroraWidget parent, AuroraWidget child, int flex) {
    return aurora_gui_expand_new(parent, child, flex);
}
AuroraWidget aurora_gui_container_new(AuroraWidget parent, AuroraWidget child) {
    GuiWidget* wp = wrapper_new(AURORA_WIDGET_CONTAINER, (GuiWidget*)parent, (GuiWidget*)child, 100, 100);
    wrapper_reposition(wp);
    return (AuroraWidget)wp;
}
void aurora_gui_container_set_padding(AuroraWidget c, int l, int t, int r, int b) {
    GuiWidget* wp = (GuiWidget*)c;
    if (wp && wp->extra_data) {
        WrapperData* wd = (WrapperData*)wp->extra_data;
        wd->pad_l = l; wd->pad_t = t; wd->pad_r = r; wd->pad_b = b;
        wrapper_reposition(wp);
    }
}
void aurora_gui_container_set_margin(AuroraWidget c, int l, int t, int r, int b) {
    aurora_gui_container_set_padding(c, l, t, r, b);
}
void aurora_gui_container_set_bg(AuroraWidget c, unsigned int color) {
    GuiWidget* wp = (GuiWidget*)c;
    if (wp && wp->hwnd && wp->extra_data) {
        ((WrapperData*)wp->extra_data)->bg = color;
        HBRUSH brush = CreateSolidBrush(RGB(GetRValue(color), GetGValue(color), GetBValue(color)));
        SetClassLongPtr(wp->hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)brush);
        InvalidateRect(wp->hwnd, nullptr, TRUE);
    }
}
AuroraWidget aurora_gui_divider_new(AuroraWidget parent, int orientation, int thickness, int x, int y, int w, int h) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* dv = widget_new(AURORA_WIDGET_DIVIDER, p);
    dv->x = x; dv->y = y; dv->w = w; dv->h = h;
    dv->hwnd = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
        x, y, w, h, parent_hwnd(p), nullptr, GetModuleHandle(nullptr), nullptr);
    if (dv->hwnd) SetWindowLongPtr(dv->hwnd, GWLP_USERDATA, (LONG_PTR)dv);
    return (AuroraWidget)dv;
}
AuroraWidget aurora_gui_aspect_ratio_new(AuroraWidget parent, AuroraWidget child, float ratio) {
    GuiWidget* wp = wrapper_new(AURORA_WIDGET_ASPECT_RATIO, (GuiWidget*)parent, (GuiWidget*)child, 100, 100);
    WrapperData* wd = (WrapperData*)wp->extra_data;
    wd->ratio = ratio > 0.01f ? ratio : 1.0f;
    return (AuroraWidget)wp;
}

/* ════════════════════════════════════════════════════════════
   Event loop + layout
   ════════════════════════════════════════════════════════════ */
extern "C" void aurora_gui_run() {
    g_running = true; MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg); DispatchMessage(&msg);
        if (msg.message == WM_QUIT) { g_running = false; break; }
    }
    g_running = false;
}
extern "C" void aurora_gui_quit() { PostQuitMessage(0); }
void aurora_gui_set_callback(AuroraWidget widget, AuroraEventCallback cb) {
    if (widget) ((GuiWidget*)widget)->callback = cb;
}
void aurora_gui_layout_horizontal(AuroraWidget parent, int margin) {
    GuiWidget* p = (GuiWidget*)parent; if (!p || !p->hwnd) return;
    int x = margin;
    for (auto* w : g_widgets) {
        if (w->parent == p && w->hwnd) { MoveWindow(w->hwnd, x, margin, w->w, w->h, TRUE); x += w->w + margin; }
    }
}
void aurora_gui_layout_vertical(AuroraWidget parent, int margin) {
    GuiWidget* p = (GuiWidget*)parent; if (!p || !p->hwnd) return;
    int y = margin;
    for (auto* w : g_widgets) {
        if (w->parent == p && w->hwnd) { MoveWindow(w->hwnd, margin, y, w->w, w->h, TRUE); y += w->h + margin; }
    }
}

/* ════════════════════════════════════════════════════════════
   Phase 9: WebView (Win32 — placeholder panel; WebView2 integration requires webview2.h)
   ════════════════════════════════════════════════════════════ */

extern "C" AuroraWidget aurora_gui_webview_new(AuroraWidget parent, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* wv = widget_new(AURORA_WIDGET_WEBVIEW, p);
    wv->x = x; wv->y = y; wv->w = w_; wv->h = h_;
    wv->hwnd = CreateWindowExA(0, "STATIC", "WebView2",
        WS_CHILD | WS_VISIBLE | SS_CENTER, x, y, w_, h_,
        parent_hwnd(p), (HMENU)(INT_PTR)wv->id, GetModuleHandle(nullptr), nullptr);
    if (wv->hwnd) SetWindowLongPtr(wv->hwnd, GWLP_USERDATA, (LONG_PTR)wv);
    return wv;
}

extern "C" void aurora_gui_webview_navigate(AuroraWidget wv, const char* url) {
    (void)wv; (void)url;
}

extern "C" void aurora_gui_webview_go_back(AuroraWidget wv) {
    (void)wv;
}

extern "C" void aurora_gui_webview_go_forward(AuroraWidget wv) {
    (void)wv;
}

extern "C" void aurora_gui_webview_reload(AuroraWidget wv) {
    (void)wv;
}

/* ════════════════════════════════════════════════════════════
   Phase 9: Media Player (Win32 — placeholder panel)
   ════════════════════════════════════════════════════════════ */

extern "C" AuroraWidget aurora_gui_media_new(AuroraWidget parent, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* mw = widget_new(AURORA_WIDGET_MEDIA, p);
    mw->x = x; mw->y = y; mw->w = w_; mw->h = h_;
    mw->hwnd = CreateWindowExA(0, "STATIC", "Media Player",
        WS_CHILD | WS_VISIBLE | SS_CENTER, x, y, w_, h_,
        parent_hwnd(p), (HMENU)(INT_PTR)mw->id, GetModuleHandle(nullptr), nullptr);
    if (mw->hwnd) SetWindowLongPtr(mw->hwnd, GWLP_USERDATA, (LONG_PTR)mw);
    return mw;
}

extern "C" void aurora_gui_media_open(AuroraWidget mw, const char* src) { (void)mw; (void)src; }
extern "C" void aurora_gui_media_play(AuroraWidget mw) { (void)mw; }
extern "C" void aurora_gui_media_pause(AuroraWidget mw) { (void)mw; }
extern "C" void aurora_gui_media_stop(AuroraWidget mw) { (void)mw; }
extern "C" void aurora_gui_media_set_volume(AuroraWidget mw, float vol) { (void)mw; (void)vol; }
extern "C" void aurora_gui_media_set_looping(AuroraWidget mw, int loop) { (void)mw; (void)loop; }
extern "C" int aurora_gui_media_is_playing(AuroraWidget mw) { (void)mw; return 0; }

/* ════════════════════════════════════════════════════════════
   Phase 9: Map (Win32 — placeholder panel)
   ════════════════════════════════════════════════════════════ */

extern "C" AuroraWidget aurora_gui_map_new(AuroraWidget parent, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* mp = widget_new(AURORA_WIDGET_MAP, p);
    mp->x = x; mp->y = y; mp->w = w_; mp->h = h_;
    mp->hwnd = CreateWindowExA(0, "STATIC", "Map",
        WS_CHILD | WS_VISIBLE | SS_CENTER, x, y, w_, h_,
        parent_hwnd(p), (HMENU)(INT_PTR)mp->id, GetModuleHandle(nullptr), nullptr);
    if (mp->hwnd) SetWindowLongPtr(mp->hwnd, GWLP_USERDATA, (LONG_PTR)mp);
    return mp;
}

extern "C" void aurora_gui_map_set_center(AuroraWidget mp, double lat, double lon) { (void)mp; (void)lat; (void)lon; }
extern "C" void aurora_gui_map_set_zoom(AuroraWidget mp, int zoom) { (void)mp; (void)zoom; }
extern "C" void aurora_gui_map_add_marker(AuroraWidget mp, double lat, double lon, const char* title) { (void)mp; (void)lat; (void)lon; (void)title; }
