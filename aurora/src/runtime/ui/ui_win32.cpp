#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

#include "runtime/ui/component.h"

extern "C" {

/* Component struct is defined in component.cpp with these fields:
   name, x, y, w, h, visible, state, render_fn, update_fn,
   parent, children, child_count, native_handle, widget_type */

/* ── Constants ── */
#define AURORA_WIDGET_CONTAINER 0
#define AURORA_WIDGET_BUTTON    1
#define AURORA_WIDGET_LABEL     2
#define AURORA_WIDGET_TEXTBOX   3
#define AURORA_WIDGET_LISTBOX   4

/* ── Event types ── */
#define AURORA_EVENT_NONE      0
#define AURORA_EVENT_CLICK     1
#define AURORA_EVENT_SELECT    2
#define AURORA_EVENT_CLOSE     4

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

/* ── Forward declarations of component functions from component.cpp ── */
extern AuroraComponent* aurora_component_create(const char* name, int x, int y, int w, int h);
extern void aurora_component_destroy(AuroraComponent* c);
extern void aurora_component_add_child(AuroraComponent* parent, AuroraComponent* child);
extern void aurora_component_set_pos(AuroraComponent* c, int x, int y);
extern void aurora_component_set_size(AuroraComponent* c, int w, int h);
extern void aurora_component_show(AuroraComponent* c);
extern void aurora_component_hide(AuroraComponent* c);
extern void aurora_component_render_tree(AuroraComponent* c);
extern void aurora_component_update_tree(AuroraComponent* c, double dt);

/* ── Window procedure for the main UI window ── */
static LRESULT CALLBACK ui_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_DESTROY:
            g_ui_running = 0;
            PostQuitMessage(0);
            return 0;
        case WM_SIZE: {
            g_window_width = (int)(short)LOWORD(lp);
            g_window_height = (int)(short)HIWORD(lp);
            /* Resize root component to match window */
            if (g_root_comp) {
                g_root_comp->w = g_window_width;
                g_root_comp->h = g_window_height;
            }
            return 0;
        }
        case WM_COMMAND: {
            HWND child = (HWND)lp;
            if (!child) return 0;
            AuroraComponent* comp = (AuroraComponent*)GetWindowLongPtr(child, GWLP_USERDATA);
            if (!comp) return 0;
            if (HIWORD(wp) == BN_CLICKED) {
                g_last_event_source = comp;
                g_last_event_type   = AURORA_EVENT_CLICK;
                g_last_event_data   = 0;
            } else if (HIWORD(wp) == LBN_SELCHANGE) {
                int idx = (int)SendMessage(child, LB_GETCURSEL, 0, 0);
                g_last_event_source = comp;
                g_last_event_type   = AURORA_EVENT_SELECT;
                g_last_event_data   = idx;
            }
            return 0;
        }
        case WM_CLOSE: {
            g_last_event_source = nullptr;
            g_last_event_type   = AURORA_EVENT_CLOSE;
            g_last_event_data   = 0;
            DestroyWindow(hwnd);
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ── Initialize the Win32 UI system — creates main window ── */
int aurora_ui_win32_init(const char* title, int width, int height) {
    if (g_main_hwnd) return 0;

    g_window_width = width;
    g_window_height = height;

    /* Initialize common controls for listbox, etc. */
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    /* Register window class */
    WNDCLASSA wc = {0};
    wc.lpfnWndProc   = ui_wnd_proc;
    wc.hInstance     = GetModuleHandleA(nullptr);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = WIN32_UI_CLASS;
    if (!RegisterClassA(&wc)) return -1;

    /* Create main window */
    RECT r = {0, 0, width, height};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    g_main_hwnd = CreateWindowExA(
        0, WIN32_UI_CLASS, title ? title : "Aurora UI",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top,
        nullptr, nullptr, wc.hInstance, nullptr);
    if (!g_main_hwnd) return -1;

    return 0;
}

/* ── Create a native child control for a component ── */
int aurora_ui_win32_create_control(AuroraComponent* comp) {
    if (!comp || !g_main_hwnd) return -1;

    const char* win_class = nullptr;
    DWORD style = WS_CHILD | WS_VISIBLE;
    const char* text = comp->name ? comp->name : "";

    switch (comp->widget_type) {
        case AURORA_WIDGET_BUTTON:
            win_class = "BUTTON";
            style |= BS_PUSHBUTTON;
            break;
        case AURORA_WIDGET_LABEL:
            win_class = "STATIC";
            style |= SS_LEFT;
            break;
        case AURORA_WIDGET_TEXTBOX:
            win_class = "EDIT";
            style |= ES_LEFT | ES_AUTOHSCROLL | WS_BORDER;
            break;
        case AURORA_WIDGET_LISTBOX:
            win_class = "LISTBOX";
            style |= WS_BORDER | WS_VSCROLL | LBS_NOTIFY;
            break;
        default:
            return -1;
    }

    HWND hwnd = CreateWindowExA(
        0, win_class, text, style,
        comp->x, comp->y, comp->w, comp->h,
        g_main_hwnd, nullptr, GetModuleHandleA(nullptr), nullptr);
    if (!hwnd) return -1;

    /* Store the component pointer in the HWND's user data */
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)comp);

    /* Set font to system default */
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SendMessage(hwnd, WM_SETFONT, (WPARAM)hFont, TRUE);

    comp->native_handle = (void*)hwnd;
    return 0;
}

/* ── Destroy a component's native control ── */
void aurora_ui_win32_destroy_control(AuroraComponent* comp) {
    if (!comp || !comp->native_handle) return;
    HWND hwnd = (HWND)comp->native_handle;
    DestroyWindow(hwnd);
    comp->native_handle = nullptr;
}

/* ── Set component text (for button/label/textbox) ── */
void aurora_ui_win32_set_text(AuroraComponent* comp, const char* text) {
    if (!comp || !comp->native_handle || !text) return;
    SetWindowTextA((HWND)comp->native_handle, text);
}

/* ── Get component text (for textbox) ── */
const char* aurora_ui_win32_get_text(AuroraComponent* comp) {
    if (!comp || !comp->native_handle) return "";
    static char buf[4096];
    GetWindowTextA((HWND)comp->native_handle, buf, sizeof(buf));
    return buf;
}

/* ── Add item to listbox ── */
void aurora_ui_win32_listbox_add(AuroraComponent* comp, const char* item) {
    if (!comp || !comp->native_handle) return;
    SendMessageA((HWND)comp->native_handle, LB_ADDSTRING, 0, (LPARAM)item);
}

/* ── Clear listbox ── */
void aurora_ui_win32_listbox_clear(AuroraComponent* comp) {
    if (!comp || !comp->native_handle) return;
    SendMessage((HWND)comp->native_handle, LB_RESETCONTENT, 0, 0);
}

/* ── Get listbox selected index ── */
int aurora_ui_win32_listbox_selected(AuroraComponent* comp) {
    if (!comp || !comp->native_handle) return -1;
    return (int)SendMessage((HWND)comp->native_handle, LB_GETCURSEL, 0, 0);
}

/* ── Get listbox item count ── */
int aurora_ui_win32_listbox_count(AuroraComponent* comp) {
    if (!comp || !comp->native_handle) return 0;
    return (int)SendMessage((HWND)comp->native_handle, LB_GETCOUNT, 0, 0);
}

/* ── Sync component tree to native window positions ── */
void aurora_ui_win32_sync_tree(AuroraComponent* c) {
    if (!c || !g_main_hwnd) return;
    if (c->native_handle) {
        SetWindowPos((HWND)c->native_handle, nullptr,
            c->x, c->y, c->w, c->h,
            SWP_NOZORDER | (c->visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
    }
    for (int i = 0; i < c->child_count; i++)
        aurora_ui_win32_sync_tree(c->children[i]);
}

/* ── Create native controls recursively for a component tree ── */
void aurora_ui_win32_create_tree(AuroraComponent* c) {
    if (!c) return;
    if (c->widget_type != AURORA_WIDGET_CONTAINER && !c->native_handle) {
        aurora_ui_win32_create_control(c);
    }
    for (int i = 0; i < c->child_count; i++)
        aurora_ui_win32_create_tree(c->children[i]);
}

/* ── Destroy native controls recursively ── */
void aurora_ui_win32_destroy_tree(AuroraComponent* c) {
    if (!c) return;
    for (int i = 0; i < c->child_count; i++)
        aurora_ui_win32_destroy_tree(c->children[i]);
    aurora_ui_win32_destroy_control(c);
}

/* ── Mount a component tree to the Win32 window ── */
void aurora_ui_win32_mount(AuroraComponent* root) {
    g_root_comp = root;
    if (root && !root->native_handle) {
        /* Root component is the main window itself — set its size */
        root->w = g_window_width;
        root->h = g_window_height;
    }
    /* Create native controls for all children */
    aurora_ui_win32_create_tree(root);
    /* Sync positions */
    aurora_ui_win32_sync_tree(root);
}

/* ── Run the Win32 message loop ── */
int aurora_ui_win32_run() {
    if (!g_main_hwnd) return -1;
    g_ui_running = 1;
    MSG msg;
    while (g_ui_running && GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

/* ── Process pending Win32 messages (non-blocking) ── */
int aurora_ui_win32_pump() {
    if (!g_main_hwnd) return -1;
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (msg.message == WM_QUIT) { g_ui_running = 0; return 1; }
    }
    return 0;
}

/* ── Event polling (simple getter/clear ── */
/* Returns last event type (0=none) and clears it */
int aurora_ui_win32_event_type() {
    int t = g_last_event_type;
    g_last_event_type = AURORA_EVENT_NONE;
    return t;
}
/* Returns last event source component pointer */
void* aurora_ui_win32_event_source() {
    return g_last_event_source;
}
/* Returns last event data (e.g. listbox selection index) */
int aurora_ui_win32_event_data() {
    return g_last_event_data;
}

/* ── Shutdown Win32 UI ── */
void aurora_ui_win32_shutdown() {
    g_ui_running = 0;
    if (g_root_comp) {
        aurora_ui_win32_destroy_tree(g_root_comp);
        g_root_comp = nullptr;
    }
    if (g_main_hwnd) {
        DestroyWindow(g_main_hwnd);
        g_main_hwnd = nullptr;
    }
    UnregisterClassA(WIN32_UI_CLASS, GetModuleHandleA(nullptr));
}

}
