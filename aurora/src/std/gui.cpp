/* ════════════════════════════════════════════════════════════
   gui.cpp — Cross-platform native GUI module
   Supports: Win32 (Windows), X11 (Linux), Cocoa (macOS)
   ════════════════════════════════════════════════════════════ */

#include "../../include/std/gui.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>
#include <map>

#if defined(_WIN32)
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <commctrl.h>
  #pragma comment(lib, "comctl32.lib")
#elif defined(__linux__)
  /* Linux X11 */
  #include <X11/Xlib.h>
  #include <X11/Xutil.h>
  #include <X11/Xatom.h>
  #include <X11/keysym.h>
#endif

/* ════════════════════════════════════════════════════════════
   Internal data structures
   ════════════════════════════════════════════════════════════ */

struct GuiWidget {
    int id;
    int type;
    int x, y, w, h;
    std::string text;
    std::vector<std::string> items;
    int selected_idx;
    AuroraEventCallback callback;
    GuiWidget* parent;

#if defined(_WIN32)
    HWND hwnd;
    HWND hwnd_parent;
#else
    Window xwindow;
    GC gc;
    int is_visible;
#endif
};

static std::vector<GuiWidget*> g_widgets;
static int g_next_id = 1;
static bool g_running = false;
static std::map<int, GuiWidget*> g_id_map;

#ifndef _WIN32
static Display* g_display = nullptr;
static int g_screen;
static Window g_root;
static Atom g_wm_delete_msg;
#endif

static GuiWidget* widget_new(int type, GuiWidget* parent) {
    GuiWidget* w = new GuiWidget();
    w->id = g_next_id++;
    w->type = type;
    w->x = w->y = w->w = w->h = 0;
    w->selected_idx = -1;
    w->callback = nullptr;
    w->parent = parent;
#if defined(_WIN32)
    w->hwnd = nullptr;
    w->hwnd_parent = parent ? parent->hwnd : nullptr;
#else
    w->xwindow = 0;
    w->gc = nullptr;
    w->is_visible = 0;
#endif
    g_widgets.push_back(w);
    g_id_map[w->id] = w;
    return w;
}

/* ════════════════════════════════════════════════════════════
   Win32 Backend
   ════════════════════════════════════════════════════════════ */

#if defined(_WIN32)

static const char* WINDOW_CLASS = "AuroraGUI_Window";
static bool g_class_registered = false;

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    GuiWidget* w = (GuiWidget*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!w) return DefWindowProc(hwnd, msg, wp, lp);

    switch (msg) {
        case WM_CLOSE: {
            if (w->callback) w->callback(w->id, AURORA_EVENT_CLOSE, 0, 0);
            if (w->type == AURORA_WIDGET_WINDOW) {
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
                if (code == BN_CLICKED && it->second->callback)
                    it->second->callback(id, AURORA_EVENT_CLICK, 0, 0);
                if (code == EN_CHANGE && it->second->callback)
                    it->second->callback(id, AURORA_EVENT_CHANGE, 0, 0);
                if ((code == LBN_SELCHANGE || code == CBN_SELCHANGE) && it->second->callback)
                    it->second->callback(id, AURORA_EVENT_SELECT, it->second->selected_idx, 0);
            }
            return 0;
        }
        case WM_DESTROY:
            if (w->type == AURORA_WIDGET_WINDOW)
                PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

static void register_window_class() {
    if (g_class_registered) return;
    WNDCLASSA wc = {};
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = WINDOW_CLASS;
    RegisterClassA(&wc);
    g_class_registered = true;
}

AuroraWidget aurora_gui_window_new(const char* title, int width, int height) {
    register_window_class();
    GuiWidget* w = widget_new(AURORA_WIDGET_WINDOW, nullptr);
    HWND hwnd = CreateWindowExA(
        0, WINDOW_CLASS, title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr
    );
    w->hwnd = hwnd;
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)w);
    return (AuroraWidget)w;
}

void aurora_gui_window_set_title(AuroraWidget win, const char* title) {
    GuiWidget* w = (GuiWidget*)win;
    if (w->hwnd) SetWindowTextA(w->hwnd, title);
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

AuroraWidget aurora_gui_button_new(AuroraWidget parent, const char* text, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* btn = widget_new(AURORA_WIDGET_BUTTON, p);
    btn->x = x; btn->y = y; btn->w = w_; btn->h = h_;
    btn->text = text;
    btn->hwnd = CreateWindowExA(
        0, "BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, w_, h_,
        p->hwnd, (HMENU)(INT_PTR)btn->id,
        GetModuleHandle(nullptr), nullptr
    );
    SetWindowLongPtr(btn->hwnd, GWLP_USERDATA, (LONG_PTR)btn);
    return (AuroraWidget)btn;
}

void aurora_gui_button_set_text(AuroraWidget btn, const char* text) {
    GuiWidget* w = (GuiWidget*)btn;
    w->text = text;
    if (w->hwnd) SetWindowTextA(w->hwnd, text);
}

const char* aurora_gui_button_get_text(AuroraWidget btn) {
    return ((GuiWidget*)btn)->text.c_str();
}

AuroraWidget aurora_gui_label_new(AuroraWidget parent, const char* text, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* lbl = widget_new(AURORA_WIDGET_LABEL, p);
    lbl->x = x; lbl->y = y; lbl->w = w_; lbl->h = h_;
    lbl->text = text;
    lbl->hwnd = CreateWindowExA(
        0, "STATIC", text,
        WS_CHILD | WS_VISIBLE,
        x, y, w_, h_,
        p->hwnd, nullptr, GetModuleHandle(nullptr), nullptr
    );
    return (AuroraWidget)lbl;
}

void aurora_gui_label_set_text(AuroraWidget lbl, const char* text) {
    GuiWidget* w = (GuiWidget*)lbl;
    w->text = text;
    if (w->hwnd) SetWindowTextA(w->hwnd, text);
}

const char* aurora_gui_label_get_text(AuroraWidget lbl) {
    return ((GuiWidget*)lbl)->text.c_str();
}

AuroraWidget aurora_gui_textbox_new(AuroraWidget parent, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* tb = widget_new(AURORA_WIDGET_TEXTBOX, p);
    tb->x = x; tb->y = y; tb->w = w_; tb->h = h_;
    tb->hwnd = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL,
        x, y, w_, h_,
        p->hwnd, (HMENU)(INT_PTR)tb->id,
        GetModuleHandle(nullptr), nullptr
    );
    SetWindowLongPtr(tb->hwnd, GWLP_USERDATA, (LONG_PTR)tb);
    return (AuroraWidget)tb;
}

void aurora_gui_textbox_set_text(AuroraWidget tb, const char* text) {
    GuiWidget* w = (GuiWidget*)tb;
    w->text = text;
    if (w->hwnd) SetWindowTextA(w->hwnd, text);
}

const char* aurora_gui_textbox_get_text(AuroraWidget tb) {
    GuiWidget* w = (GuiWidget*)tb;
    if (w->hwnd) {
        char buf[4096];
        GetWindowTextA(w->hwnd, buf, sizeof(buf));
        w->text = buf;
    }
    return w->text.c_str();
}

AuroraWidget aurora_gui_listbox_new(AuroraWidget parent, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* lb = widget_new(AURORA_WIDGET_LISTBOX, p);
    lb->x = x; lb->y = y; lb->w = w_; lb->h = h_;
    lb->hwnd = CreateWindowExA(
        WS_EX_CLIENTEDGE, "LISTBOX", "",
        WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL,
        x, y, w_, h_,
        p->hwnd, (HMENU)(INT_PTR)lb->id,
        GetModuleHandle(nullptr), nullptr
    );
    SetWindowLongPtr(lb->hwnd, GWLP_USERDATA, (LONG_PTR)lb);
    return (AuroraWidget)lb;
}

void aurora_gui_listbox_add_item(AuroraWidget lb, const char* item) {
    GuiWidget* w = (GuiWidget*)lb;
    w->items.push_back(item);
    if (w->hwnd) SendMessageA(w->hwnd, LB_ADDSTRING, 0, (LPARAM)item);
}

void aurora_gui_listbox_clear(AuroraWidget lb) {
    GuiWidget* w = (GuiWidget*)lb;
    w->items.clear();
    if (w->hwnd) SendMessage(w->hwnd, LB_RESETCONTENT, 0, 0);
}

int aurora_gui_listbox_get_selected(AuroraWidget lb) {
    GuiWidget* w = (GuiWidget*)lb;
    if (w->hwnd) {
        LRESULT sel = SendMessage(w->hwnd, LB_GETCURSEL, 0, 0);
        w->selected_idx = (sel == LB_ERR) ? -1 : (int)sel;
    }
    return w->selected_idx;
}

const char* aurora_gui_listbox_get_item(AuroraWidget lb, int idx) {
    GuiWidget* w = (GuiWidget*)lb;
    if (idx >= 0 && idx < (int)w->items.size()) return w->items[idx].c_str();
    return nullptr;
}

int aurora_gui_listbox_count(AuroraWidget lb) {
    return (int)((GuiWidget*)lb)->items.size();
}

void aurora_gui_run() {
    g_running = true;
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    g_running = false;
}

void aurora_gui_quit() {
    PostQuitMessage(0);
}

void aurora_gui_layout_horizontal(AuroraWidget parent, int margin) {
    GuiWidget* p = (GuiWidget*)parent;
    if (!p || !p->hwnd) return;
    int x = margin;
    for (auto* w : g_widgets) {
        if (w->parent == p && w->hwnd) {
            MoveWindow(w->hwnd, x, margin, w->w, w->h, TRUE);
            x += w->w + margin;
        }
    }
}
void aurora_gui_layout_vertical(AuroraWidget parent, int margin) {
    GuiWidget* p = (GuiWidget*)parent;
    if (!p || !p->hwnd) return;
    int y = margin;
    for (auto* w : g_widgets) {
        if (w->parent == p && w->hwnd) {
            MoveWindow(w->hwnd, margin, y, w->w, w->h, TRUE);
            y += w->h + margin;
        }
    }
}
void aurora_gui_set_callback(AuroraWidget widget, AuroraEventCallback cb) {
    ((GuiWidget*)widget)->callback = cb;
}

/* ════════════════════════════════════════════════════════════
   X11 Backend (Linux)
   ════════════════════════════════════════════════════════════ */

#else

/* ── Platform data for X11 widgets ── */
struct X11WidgetData {
    Window xwin;
    GC gc;
    bool visible;
};

static std::map<Window, GuiWidget*> g_xwindow_map;

AuroraWidget aurora_gui_window_new(const char* title, int width, int height) {
    if (!g_display) {
        g_display = XOpenDisplay(nullptr);
        if (!g_display) { fprintf(stderr, "gui: cannot open X display\n"); return nullptr; }
        g_screen = DefaultScreen(g_display);
        g_root = RootWindow(g_display, g_screen);
        g_wm_delete_msg = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    }

    GuiWidget* w = widget_new(AURORA_WIDGET_WINDOW, nullptr);
    XSetWindowAttributes attr;
    attr.event_mask = ExposureMask | KeyPressMask | ButtonPressMask |
                      StructureNotifyMask | FocusChangeMask;
    Window xwin = XCreateWindow(g_display, g_root, 0, 0, width, height, 0,
                                CopyFromParent, InputOutput, CopyFromParent,
                                CWEventMask, &attr);
    XStoreName(g_display, xwin, title);
    XSetWMProtocols(g_display, xwin, &g_wm_delete_msg, 1);

    /* Set min size hints */
    XSizeHints hints;
    hints.flags = PSize | PMinSize;
    hints.width = width; hints.height = height;
    hints.min_width = 100; hints.min_height = 50;
    XSetWMNormalHints(g_display, xwin, &hints);

    w->xwindow = xwin;
    w->gc = XCreateGC(g_display, xwin, 0, nullptr);
    w->is_visible = 0;
    g_xwindow_map[xwin] = w;
    return (AuroraWidget)w;
}

void aurora_gui_window_set_title(AuroraWidget win, const char* title) {
    GuiWidget* w = (GuiWidget*)win;
    if (g_display && w->xwindow) XStoreName(g_display, w->xwindow, title);
}

void aurora_gui_window_resize(AuroraWidget win, int w_, int h_) {
    GuiWidget* w = (GuiWidget*)win;
    if (g_display && w->xwindow) XResizeWindow(g_display, w->xwindow, w_, h_);
}

void aurora_gui_window_show(AuroraWidget win) {
    GuiWidget* w = (GuiWidget*)win;
    if (g_display && w->xwindow) {
        XMapWindow(g_display, w->xwindow);
        w->is_visible = 1;
    }
}

void aurora_gui_window_hide(AuroraWidget win) {
    GuiWidget* w = (GuiWidget*)win;
    if (g_display && w->xwindow) {
        XUnmapWindow(g_display, w->xwindow);
        w->is_visible = 0;
    }
}

void aurora_gui_window_destroy(AuroraWidget win) {
    GuiWidget* w = (GuiWidget*)win;
    if (g_display && w->xwindow) {
        XDestroyWindow(g_display, w->xwindow);
        XFreeGC(g_display, w->gc);
        g_xwindow_map.erase(w->xwindow);
    }
}

/* Helper: create a child X window for controls */
static Window create_child(Window parent, int x, int y, int w_, int h_) {
    if (!g_display) return 0;
    XSetWindowAttributes attr;
    attr.event_mask = ExposureMask | ButtonPressMask | KeyPressMask;
    attr.background_pixel = WhitePixel(g_display, g_screen);
    attr.border_pixel = BlackPixel(g_display, g_screen);
    return XCreateWindow(g_display, parent, x, y, w_, h_, 1,
                         CopyFromParent, InputOutput, CopyFromParent,
                         CWEventMask | CWBackPixel | CWBorderPixel, &attr);
}

AuroraWidget aurora_gui_button_new(AuroraWidget parent, const char* text, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* btn = widget_new(AURORA_WIDGET_BUTTON, p);
    btn->x = x; btn->y = y; btn->w = w_; btn->h = h_; btn->text = text;
    if (g_display && p->xwindow) {
        btn->xwindow = create_child(p->xwindow, x, y, w_, h_);
        btn->gc = XCreateGC(g_display, btn->xwindow, 0, nullptr);
        XMapWindow(g_display, btn->xwindow);
        g_xwindow_map[btn->xwindow] = btn;
    }
    return (AuroraWidget)btn;
}

void aurora_gui_button_set_text(AuroraWidget btn, const char* text) {
    ((GuiWidget*)btn)->text = text;
}

const char* aurora_gui_button_get_text(AuroraWidget btn) {
    return ((GuiWidget*)btn)->text.c_str();
}

AuroraWidget aurora_gui_label_new(AuroraWidget parent, const char* text, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* lbl = widget_new(AURORA_WIDGET_LABEL, p);
    lbl->x = x; lbl->y = y; lbl->w = w_; lbl->h = h_; lbl->text = text;
    if (g_display && p->xwindow) {
        lbl->xwindow = create_child(p->xwindow, x, y, w_, h_);
        lbl->gc = XCreateGC(g_display, lbl->xwindow, 0, nullptr);
        XMapWindow(g_display, lbl->xwindow);
        g_xwindow_map[lbl->xwindow] = lbl;
    }
    return (AuroraWidget)lbl;
}

void aurora_gui_label_set_text(AuroraWidget lbl, const char* text) {
    ((GuiWidget*)lbl)->text = text;
}

const char* aurora_gui_label_get_text(AuroraWidget lbl) {
    return ((GuiWidget*)lbl)->text.c_str();
}

AuroraWidget aurora_gui_textbox_new(AuroraWidget parent, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* tb = widget_new(AURORA_WIDGET_TEXTBOX, p);
    tb->x = x; tb->y = y; tb->w = w_; tb->h = h_;
    if (g_display && p->xwindow) {
        tb->xwindow = create_child(p->xwindow, x, y, w_, h_);
        tb->gc = XCreateGC(g_display, tb->xwindow, 0, nullptr);
        XMapWindow(g_display, tb->xwindow);
        g_xwindow_map[tb->xwindow] = tb;
    }
    return (AuroraWidget)tb;
}

void aurora_gui_textbox_set_text(AuroraWidget tb, const char* text) {
    ((GuiWidget*)tb)->text = text;
}

const char* aurora_gui_textbox_get_text(AuroraWidget tb) {
    return ((GuiWidget*)tb)->text.c_str();
}

AuroraWidget aurora_gui_listbox_new(AuroraWidget parent, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    GuiWidget* lb = widget_new(AURORA_WIDGET_LISTBOX, p);
    lb->x = x; lb->y = y; lb->w = w_; lb->h = h_;
    if (g_display && p->xwindow) {
        lb->xwindow = create_child(p->xwindow, x, y, w_, h_);
        lb->gc = XCreateGC(g_display, lb->xwindow, 0, nullptr);
        XMapWindow(g_display, lb->xwindow);
        g_xwindow_map[lb->xwindow] = lb;
    }
    return (AuroraWidget)lb;
}

void aurora_gui_listbox_add_item(AuroraWidget lb, const char* item) {
    ((GuiWidget*)lb)->items.push_back(item);
}

void aurora_gui_listbox_clear(AuroraWidget lb) {
    ((GuiWidget*)lb)->items.clear();
}

int aurora_gui_listbox_get_selected(AuroraWidget lb) {
    return ((GuiWidget*)lb)->selected_idx;
}

const char* aurora_gui_listbox_get_item(AuroraWidget lb, int idx) {
    GuiWidget* w = (GuiWidget*)lb;
    if (idx >= 0 && idx < (int)w->items.size()) return w->items[idx].c_str();
    return nullptr;
}

int aurora_gui_listbox_count(AuroraWidget lb) {
    return (int)((GuiWidget*)lb)->items.size();
}

void aurora_gui_run() {
    if (!g_display) return;
    g_running = true;
    while (g_running) {
        while (XPending(g_display)) {
            XEvent ev;
            XNextEvent(g_display, &ev);
            auto it = g_xwindow_map.find(ev.xany.window);
            GuiWidget* w = (it != g_xwindow_map.end()) ? it->second : nullptr;

            switch (ev.type) {
                case Expose:
                    if (w && w->gc) {
                        /* Draw button/label backgrounds */
                        if (w->type == AURORA_WIDGET_BUTTON) {
                            XSetForeground(g_display, w->gc, 0xCCCCCC);
                            XFillRectangle(g_display, w->xwindow, w->gc, 0, 0, w->w, w->h);
                            XSetForeground(g_display, w->gc, 0);
                            XDrawString(g_display, w->xwindow, w->gc, 5, 15,
                                        w->text.c_str(), (int)w->text.size());
                        } else if (w->type == AURORA_WIDGET_LABEL) {
                            XSetForeground(g_display, w->gc, 0);
                            XDrawString(g_display, w->xwindow, w->gc, 2, 12,
                                        w->text.c_str(), (int)w->text.size());
                        }
                    }
                    break;
                case ButtonPress:
                    if (w && w->callback && w->type == AURORA_WIDGET_BUTTON)
                        w->callback(w->id, AURORA_EVENT_CLICK, 0, 0);
                    break;
                case ClientMessage:
                    if ((Atom)ev.xclient.data.l[0] == g_wm_delete_msg) {
                        if (w && w->callback) w->callback(w->id, AURORA_EVENT_CLOSE, 0, 0);
                        g_running = false;
                    }
                    break;
                case DestroyNotify:
                    g_running = false;
                    break;
            }
        }
    }
    if (g_display) {
        XCloseDisplay(g_display);
        g_display = nullptr;
    }
}

void aurora_gui_quit() {
    g_running = false;
}

void aurora_gui_set_callback(AuroraWidget widget, AuroraEventCallback cb) {
    if (widget) ((GuiWidget*)widget)->callback = cb;
}

void aurora_gui_layout_horizontal(AuroraWidget parent, int margin) {
    GuiWidget* p = (GuiWidget*)parent;
    if (!g_display || !p || !p->xwindow) return;
    int x = margin;
    for (auto* w : g_widgets) {
        if (w->parent == p && w->xwindow) {
            XMoveResizeWindow(g_display, w->xwindow, x, margin, w->w, w->h);
            x += w->w + margin;
        }
    }
}
void aurora_gui_layout_vertical(AuroraWidget parent, int margin) {
    GuiWidget* p = (GuiWidget*)parent;
    if (!g_display || !p || !p->xwindow) return;
    int y = margin;
    for (auto* w : g_widgets) {
        if (w->parent == p && w->xwindow) {
            XMoveResizeWindow(g_display, w->xwindow, margin, y, w->w, w->h);
            y += w->h + margin;
        }
    }
}

#endif
