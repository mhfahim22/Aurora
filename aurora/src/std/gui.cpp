/* gui.cpp -- Platform dispatcher for native GUI */

/* On Windows: all aurora_gui_* functions are in ui_win32.cpp */
/* On Linux: X11 implementation is inline here */
#if defined(_WIN32)
  #include "../../include/std/gui.hpp"
  /* Windows implementation provided by ui_win32.cpp */
#elif defined(__linux__)

#include "../../include/std/gui.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

struct GuiWidget {
    int id, type, x, y, w, h;
    std::string text;
    std::vector<std::string> items;
    int selected_idx, min_val, max_val, group_id, value;
    AuroraEventCallback callback;
    GuiWidget* parent;
    Window xwindow;
    GC gc;
    int is_visible;
    void* extra_data;
    unsigned int fg_color, bg_color;
    AuroraPaintCallback paint_cb;
    void* paint_user;
};

static std::vector<GuiWidget*> g_widgets;
static int g_next_id = 1;
static std::map<int, GuiWidget*> g_id_map;
static char g_temp_str[4096];
static Display* g_display = nullptr;
static int g_screen;
static Window g_root;
static Colormap g_cmap;
static Atom g_wm_delete_msg;
static bool g_running = false;
static int g_win_width = 800, g_win_height = 600;

static GuiWidget* widget_new(int type, GuiWidget* parent) {
    GuiWidget* w = new GuiWidget();
    w->id = g_next_id++; w->type = type;
    w->x = w->y = w->w = w->h = 0; w->selected_idx = -1;
    w->min_val = 0; w->max_val = 100; w->group_id = 0;
    w->value = 0;
    w->callback = nullptr; w->parent = parent;
    w->xwindow = 0; w->gc = nullptr; w->is_visible = 1;
    w->extra_data = nullptr;
    w->fg_color = 0x000000; w->bg_color = 0xFFFFFF;
    w->paint_cb = nullptr; w->paint_user = nullptr;
    g_widgets.push_back(w); g_id_map[w->id] = w;
    return w;
}

static void fire_event(GuiWidget* w, int event, int p1, int p2) {
    if (w && w->callback) w->callback(w->id, event, p1, p2);
}

static unsigned long alloc_color(unsigned int hex) {
    XColor xc;
    xc.red   = ((hex >> 16) & 0xFF) * 257;
    xc.green = ((hex >> 8) & 0xFF) * 257;
    xc.blue  = (hex & 0xFF) * 257;
    xc.flags = DoRed | DoGreen | DoBlue;
    XAllocColor(g_display, g_cmap, &xc);
    return xc.pixel;
}

static void draw_label(GuiWidget* w) {
    if (!w || !w->xwindow || !w->gc) return;
    XClearWindow(g_display, w->xwindow);
    XDrawString(g_display, w->xwindow, w->gc, 2, w->h - 4, w->text.c_str(), (int)w->text.size());
}

/* ── Application ── */
int aurora_gui_app_init(void) {
    g_display = XOpenDisplay(nullptr);
    if (!g_display) return -1;
    g_screen = DefaultScreen(g_display);
    g_root = RootWindow(g_display, g_screen);
    g_cmap = DefaultColormap(g_display, g_screen);
    g_wm_delete_msg = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    return 0;
}

static bool g_key_state[256] = {false};
static int g_mouse_buttons[5] = {0};
static int g_mouse_x = 0, g_mouse_y = 0;
static int g_mod_state = 0;

void aurora_gui_app_run(void) {
    g_running = true;
    XEvent ev;
    while (g_running) {
        while (XPending(g_display) > 0) {
            XNextEvent(g_display, &ev);
            if (ev.type == ClientMessage && (Atom)ev.xclient.data.l[0] == g_wm_delete_msg) {
                g_running = false; break;
            }
            if (ev.type == Expose) {
                for (auto* w : g_widgets) {
                    if (w->xwindow && w->gc) draw_label(w);
                }
            }
            if (ev.type == ButtonPress) {
                if (ev.xbutton.button < 5) g_mouse_buttons[ev.xbutton.button] = 1;
                g_mouse_x = ev.xbutton.x; g_mouse_y = ev.xbutton.y;
                for (auto* w : g_widgets) {
                    if (w->xwindow && ev.xbutton.window == w->xwindow) {
                        fire_event(w, 1, ev.xbutton.x, ev.xbutton.y);
                    }
                }
            }
            if (ev.type == ButtonRelease) {
                if (ev.xbutton.button < 5) g_mouse_buttons[ev.xbutton.button] = 0;
                g_mouse_x = ev.xbutton.x; g_mouse_y = ev.xbutton.y;
            }
            if (ev.type == MotionNotify) {
                g_mouse_x = ev.xmotion.x; g_mouse_y = ev.xmotion.y;
            }
            if (ev.type == KeyPress) {
                KeySym ks = XLookupKeysym(&ev.xkey, 0);
                if (ks < 256) g_key_state[ks] = true;
                g_mod_state = ev.xkey.state;
                for (auto* w : g_widgets) {
                    if (w->xwindow && ev.xkey.window == w->xwindow && w->type == 4) {
                        char buf[32] = {0};
                        int len = XLookupString(&ev.xkey, buf, sizeof(buf)-1, nullptr, nullptr);
                        if (len > 0) {
                            if (ks == XK_BackSpace && !w->text.empty())
                                w->text.pop_back();
                            else if (ks == XK_Return)
                                fire_event(w, 1, 0, 0);
                            else if (len == 1 && buf[0] >= 32)
                                w->text += buf[0];
                        }
                    }
                }
            }
            if (ev.type == KeyRelease) {
                KeySym ks = XLookupKeysym(&ev.xkey, 0);
                if (ks < 256) g_key_state[ks] = false;
            }
        }
        if (!g_running) break;
    }
}

void aurora_gui_app_quit(void) { g_running = false; }

void aurora_gui_set_enabled(AuroraWidget w, int e) {
    GuiWidget* gw = (GuiWidget*)w; if (gw) gw->selected_idx = e ? 1 : 0;
}
int  aurora_gui_get_enabled(AuroraWidget w) {
    GuiWidget* gw = (GuiWidget*)w; return gw ? gw->selected_idx : 0;
}

void aurora_gui_set_visible(AuroraWidget w, int v) {
    GuiWidget* gw = (GuiWidget*)w; if (!gw) return;
    gw->is_visible = v;
    if (gw->xwindow) { if (v) XMapWindow(g_display, gw->xwindow); else XUnmapWindow(g_display, gw->xwindow); }
}

int aurora_gui_get_visible(AuroraWidget w) { GuiWidget* gw = (GuiWidget*)w; return gw ? gw->is_visible : 0; }

void aurora_gui_set_focus(AuroraWidget w) {
    GuiWidget* gw = (GuiWidget*)w; if (gw && gw->xwindow) XSetInputFocus(g_display, gw->xwindow, RevertToParent, CurrentTime);
}

void aurora_gui_move(AuroraWidget w, int x, int y, int w_, int h_) {
    GuiWidget* gw = (GuiWidget*)w; if (!gw) return;
    gw->x = x; gw->y = y; gw->w = w_; gw->h = h_;
    if (gw->xwindow) XMoveResizeWindow(g_display, gw->xwindow, x, y, w_, h_);
}

/* ── Window ── */
AuroraWidget aurora_gui_window_new(const char* title, int width, int height) {
    GuiWidget* gw = widget_new(1, nullptr);
    gw->x = gw->y = 0; gw->w = width; gw->h = height;
    Window win = XCreateSimpleWindow(g_display, g_root, 0, 0, width, height, 1,
        BlackPixel(g_display, g_screen), WhitePixel(g_display, g_screen));
    XStoreName(g_display, win, title ? title : "Aurora App");
    XSetWMProtocols(g_display, win, &g_wm_delete_msg, 1);
    XSelectInput(g_display, win, ExposureMask | ButtonPressMask | KeyPressMask);
    GC gc = XCreateGC(g_display, win, 0, nullptr);
    XSetForeground(g_display, gc, BlackPixel(g_display, g_screen));
    gw->xwindow = win; gw->gc = gc;
    XMapWindow(g_display, win);
    g_win_width = width; g_win_height = height;
    return gw;
}

void aurora_gui_window_set_title(AuroraWidget w, const char* t) {
    GuiWidget* gw = (GuiWidget*)w; if (gw && gw->xwindow && t) XStoreName(g_display, gw->xwindow, t);
}

void aurora_gui_window_resize(AuroraWidget w, int w_, int h_) {
    GuiWidget* gw = (GuiWidget*)w; if (gw && gw->xwindow) XResizeWindow(g_display, gw->xwindow, w_, h_);
}

void aurora_gui_window_show(AuroraWidget w) {
    GuiWidget* gw = (GuiWidget*)w; if (gw && gw->xwindow) XMapWindow(g_display, gw->xwindow);
}

void aurora_gui_window_hide(AuroraWidget w) {
    GuiWidget* gw = (GuiWidget*)w; if (gw && gw->xwindow) XUnmapWindow(g_display, gw->xwindow);
}

void aurora_gui_window_destroy(AuroraWidget w) {
    GuiWidget* gw = (GuiWidget*)w; if (gw && gw->xwindow) { XDestroyWindow(g_display, gw->xwindow); gw->xwindow = 0; }
}

void aurora_gui_window_maximize(AuroraWidget w) {
    GuiWidget* gw = (GuiWidget*)w; if (!gw || !gw->xwindow) return;
    XMapWindow(g_display, gw->xwindow);
    Atom wm_state = XInternAtom(g_display, "_NET_WM_STATE", False);
    Atom max_h = XInternAtom(g_display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    Atom max_v = XInternAtom(g_display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    if (wm_state && max_h && max_v) {
        XEvent e; memset(&e, 0, sizeof(e));
        e.type = ClientMessage; e.xclient.window = gw->xwindow;
        e.xclient.message_type = wm_state; e.xclient.format = 32;
        e.xclient.data.l[0] = 1; e.xclient.data.l[1] = max_h; e.xclient.data.l[2] = max_v;
        XSendEvent(g_display, g_root, False, SubstructureNotifyMask, &e);
    }
}
void aurora_gui_window_minimize(AuroraWidget w) {
    GuiWidget* gw = (GuiWidget*)w; if (!gw || !gw->xwindow) return;
    XIconifyWindow(g_display, gw->xwindow, g_screen);
}
void aurora_gui_window_restore(AuroraWidget w) {
    GuiWidget* gw = (GuiWidget*)w; if (!gw || !gw->xwindow) return;
    Atom wm_state = XInternAtom(g_display, "_NET_WM_STATE", False);
    Atom max_h = XInternAtom(g_display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    Atom max_v = XInternAtom(g_display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    if (wm_state && max_h && max_v) {
        XEvent e; memset(&e, 0, sizeof(e));
        e.type = ClientMessage; e.xclient.window = gw->xwindow;
        e.xclient.message_type = wm_state; e.xclient.format = 32;
        e.xclient.data.l[0] = 0; e.xclient.data.l[1] = max_h; e.xclient.data.l[2] = max_v;
        XSendEvent(g_display, g_root, False, SubstructureNotifyMask, &e);
    }
}
int aurora_gui_window_get_width(AuroraWidget w) {
    GuiWidget* gw = (GuiWidget*)w; return gw ? gw->w : g_win_width;
}
int aurora_gui_window_get_height(AuroraWidget w) {
    GuiWidget* gw = (GuiWidget*)w; return gw ? gw->h : g_win_height;
}
void aurora_gui_window_set_min_size(AuroraWidget w, int a, int b) {
    GuiWidget* gw = (GuiWidget*)w; if (!gw || !gw->xwindow) return;
    XSizeHints hints; memset(&hints, 0, sizeof(hints));
    hints.flags = PMinSize; hints.min_width = a; hints.min_height = b;
    XSetWMNormalHints(g_display, gw->xwindow, &hints);
}
void aurora_gui_window_set_max_size(AuroraWidget w, int a, int b) {
    GuiWidget* gw = (GuiWidget*)w; if (!gw || !gw->xwindow) return;
    XSizeHints hints; memset(&hints, 0, sizeof(hints));
    hints.flags = PMaxSize; hints.max_width = a; hints.max_height = b;
    XSetWMNormalHints(g_display, gw->xwindow, &hints);
}
void aurora_gui_window_set_resizable(AuroraWidget w, int r) {
    GuiWidget* gw = (GuiWidget*)w; if (gw) gw->min_val = r ? 1 : 0;
}

/* ── Generic ── */
void aurora_gui_set_callback(AuroraWidget w, AuroraEventCallback cb) {
    GuiWidget* gw = (GuiWidget*)w; if (gw) gw->callback = cb;
}

void* aurora_gui_get_native_handle(AuroraWidget w) {
    GuiWidget* gw = (GuiWidget*)w; if (!gw) return nullptr;
    return (void*)(uintptr_t)gw->xwindow;
}

/* ── Label ── */
AuroraWidget aurora_gui_label_new(AuroraWidget parent, const char* text, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(3, (GuiWidget*)parent);
    gw->x = x; gw->y = y; gw->w = w; gw->h = h; gw->text = text ? text : "";
    Window sub = XCreateSimpleWindow(g_display, ((GuiWidget*)parent) ? ((GuiWidget*)parent)->xwindow : g_root,
        x, y, w, h, 0, 0, WhitePixel(g_display, g_screen));
    GC gc = XCreateGC(g_display, sub, 0, nullptr);
    XSetForeground(g_display, gc, BlackPixel(g_display, g_screen));
    XSelectInput(g_display, sub, ExposureMask);
    XMapWindow(g_display, sub);
    gw->xwindow = sub; gw->gc = gc;
    draw_label(gw);
    return gw;
}

void aurora_gui_label_set_text(AuroraWidget w, const char* t) {
    GuiWidget* gw = (GuiWidget*)w; if (!gw) return;
    gw->text = t ? t : ""; draw_label(gw);
}

const char* aurora_gui_label_get_text(AuroraWidget w) {
    GuiWidget* gw = (GuiWidget*)w; return gw ? gw->text.c_str() : "";
}

void aurora_gui_label_set_font_size(AuroraWidget w, int s) {
    GuiWidget* gw = (GuiWidget*)w; if (gw) gw->min_val = s;
}
void aurora_gui_label_set_color(AuroraWidget w, unsigned int c) {
    GuiWidget* gw = (GuiWidget*)w; if (gw) gw->fg_color = c;
}
void aurora_gui_label_set_align(AuroraWidget w, int a) {
    GuiWidget* gw = (GuiWidget*)w; if (gw) gw->max_val = a;
}
void aurora_gui_label_set_align(AuroraWidget w, int a) { (void)w;(void)a; }

/* ── Text ── */
AuroraWidget aurora_gui_text_new(AuroraWidget p, const char* t, int x, int y, int w, int h) {
    return aurora_gui_label_new(p, t, x, y, w, h);
}

void aurora_gui_text_set_text(AuroraWidget w, const char* t) { aurora_gui_label_set_text(w, t); }
const char* aurora_gui_text_get_text(AuroraWidget w) { return aurora_gui_label_get_text(w); }

/* ── Button ── */
AuroraWidget aurora_gui_button_new(AuroraWidget parent, const char* text, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(2, (GuiWidget*)parent);
    gw->x = x; gw->y = y; gw->w = w; gw->h = h; gw->text = text ? text : "";
    Window parent_win = ((GuiWidget*)parent) ? ((GuiWidget*)parent)->xwindow : g_root;
    Window sub = XCreateSimpleWindow(g_display, parent_win, x, y, w, h, 1,
        BlackPixel(g_display, g_screen), 0xCCCCCC);
    GC gc = XCreateGC(g_display, sub, 0, nullptr);
    XSetForeground(g_display, gc, BlackPixel(g_display, g_screen));
    XSelectInput(g_display, sub, ExposureMask | ButtonPressMask);
    XMapWindow(g_display, sub);
    gw->xwindow = sub; gw->gc = gc;
    XDrawString(g_display, sub, gc, 4, h - 6, text ? text : "", text ? (int)strlen(text) : 0);
    return gw;
}

void aurora_gui_button_set_text(AuroraWidget w, const char* t) {
    GuiWidget* gw = (GuiWidget*)w; if (!gw) return;
    gw->text = t ? t : "";
    if (gw->xwindow && gw->gc) {
        XClearWindow(g_display, gw->xwindow);
        XDrawString(g_display, gw->xwindow, gw->gc, 4, gw->h - 6, gw->text.c_str(), (int)gw->text.size());
    }
}

const char* aurora_gui_button_get_text(AuroraWidget w) {
    GuiWidget* gw = (GuiWidget*)w; return gw ? gw->text.c_str() : "";
}

/* ── CheckBox ── */
AuroraWidget aurora_gui_checkbox_new(AuroraWidget parent, const char* text, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(7, (GuiWidget*)parent);
    gw->x = x; gw->y = y; gw->w = w; gw->h = h; gw->text = text ? text : "";
    return gw;
}
void aurora_gui_checkbox_set_text(AuroraWidget w, const char* t) {
    GuiWidget* gw = (GuiWidget*)w; if (gw) gw->text = t ? t : "";
}
const char* aurora_gui_checkbox_get_text(AuroraWidget w) { return aurora_gui_label_get_text(w); }
int aurora_gui_checkbox_is_checked(AuroraWidget w) {
    GuiWidget* gw = (GuiWidget*)w; return gw ? gw->selected_idx : 0;
}
void aurora_gui_checkbox_set_checked(AuroraWidget w, int v) {
    GuiWidget* gw = (GuiWidget*)w; if (gw) gw->selected_idx = v ? 1 : 0;
}

/* ── RadioButton ── */
AuroraWidget aurora_gui_radiobutton_new(AuroraWidget p, const char* t, int x, int y, int w, int h, int g) {
    (void)g; GuiWidget* gw = widget_new(8, (GuiWidget*)p);
    gw->x = x; gw->y = y; gw->w = w; gw->h = h; gw->text = t ? t : "";
    return gw;
}
void aurora_gui_radiobutton_set_text(AuroraWidget w, const char* t) {
    GuiWidget* gw = (GuiWidget*)w; if (gw) gw->text = t ? t : "";
}
const char* aurora_gui_radiobutton_get_text(AuroraWidget w) { return aurora_gui_label_get_text(w); }
int aurora_gui_radiobutton_is_checked(AuroraWidget w) {
    GuiWidget* gw = (GuiWidget*)w; return gw ? gw->selected_idx : 0;
}
void aurora_gui_radiobutton_set_checked(AuroraWidget w, int v) {
    GuiWidget* gw = (GuiWidget*)w; if (gw) gw->selected_idx = v ? 1 : 0;
}

/* ── Switch ── */
AuroraWidget aurora_gui_switch_new(AuroraWidget parent, const char* text, int x, int y, int w, int h) {
    return aurora_gui_checkbox_new(parent, text, x, y, w, h);
}
int aurora_gui_switch_is_on(AuroraWidget w) { return aurora_gui_checkbox_is_checked(w); }
void aurora_gui_switch_set_on(AuroraWidget w, int v) { aurora_gui_checkbox_set_checked(w, v); }

/* ── TextBox ── */
AuroraWidget aurora_gui_textbox_new(AuroraWidget parent, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(4, (GuiWidget*)parent);
    gw->x = x; gw->y = y; gw->w = w; gw->h = h;
    return gw;
}
void aurora_gui_textbox_set_text(AuroraWidget w, const char* t) {
    GuiWidget* gw = (GuiWidget*)w; if (gw) gw->text = t ? t : "";
}
const char* aurora_gui_textbox_get_text(AuroraWidget w) { return aurora_gui_label_get_text(w); }
void aurora_gui_textbox_set_readonly(AuroraWidget w, int r) {
    GuiWidget* gw = (GuiWidget*)w; if (gw) gw->min_val = r ? 1 : 0;
}
void aurora_gui_textbox_set_placeholder(AuroraWidget w, const char* t) {
    GuiWidget* gw = (GuiWidget*)w; if (gw) gw->extra_data = (void*)t;
}
void aurora_gui_textbox_set_multiline(AuroraWidget w, int m) {
    GuiWidget* gw = (GuiWidget*)w; if (gw) gw->max_val = m ? 1 : 0;
}
int aurora_gui_textbox_get_line_count(AuroraWidget w) {
    GuiWidget* gw = (GuiWidget*)w; if (!gw) return 0;
    int lines = 1;
    for (char c : gw->text) if (c == '\n') lines++;
    return lines;
}

/* ── PasswordBox ── */
AuroraWidget aurora_gui_passwordbox_new(AuroraWidget parent, int x, int y, int w, int h) {
    return aurora_gui_textbox_new(parent, x, y, w, h);
}
void aurora_gui_passwordbox_set_text(AuroraWidget w, const char* t) { aurora_gui_textbox_set_text(w, t); }
const char* aurora_gui_passwordbox_get_text(AuroraWidget w) { return aurora_gui_textbox_get_text(w); }

/* ── Slider ── */
AuroraWidget aurora_gui_slider_new(AuroraWidget parent, int x, int y, int w, int h, int min, int max) {
    GuiWidget* gw = widget_new(9, (GuiWidget*)parent);
    gw->x = x; gw->y = y; gw->w = w; gw->h = h; gw->min_val = min; gw->max_val = max; gw->value = min;
    return gw;
}
int aurora_gui_slider_get_value(AuroraWidget w) {
    GuiWidget* gw = (GuiWidget*)w; return gw ? gw->value : 0;
}
void aurora_gui_slider_set_value(AuroraWidget w, int v) {
    GuiWidget* gw = (GuiWidget*)w; if (gw) gw->value = v;
}
void aurora_gui_slider_set_range(AuroraWidget w, int mn, int mx) {
    GuiWidget* gw = (GuiWidget*)w; if (gw) { gw->min_val = mn; gw->max_val = mx; }
}

/* ── ProgressBar ── */
AuroraWidget aurora_gui_progressbar_new(AuroraWidget parent, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(10, (GuiWidget*)parent);
    gw->x = x; gw->y = y; gw->w = w; gw->h = h; gw->min_val = 0; gw->max_val = 100;
    return gw;
}
void aurora_gui_progressbar_set_value(AuroraWidget w, int v) {
    GuiWidget* gw = (GuiWidget*)w; if (gw) gw->value = v;
}
int aurora_gui_progressbar_get_value(AuroraWidget w) {
    GuiWidget* gw = (GuiWidget*)w; return gw ? gw->value : 0;
}
void aurora_gui_progressbar_set_range(AuroraWidget w, int mn, int mx) {
    GuiWidget* gw = (GuiWidget*)w; if (gw) { gw->min_val = mn; gw->max_val = mx; }
}
void aurora_gui_progressbar_set_marquee(AuroraWidget w, int v) { (void)w;(void)v; }

/* ── ComboBox ── */
AuroraWidget aurora_gui_combobox_new(AuroraWidget parent, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(11, (GuiWidget*)parent);
    gw->x = x; gw->y = y; gw->w = w; gw->h = h;
    return gw;
}
void aurora_gui_combobox_add_item(AuroraWidget w, const char* i) {
    GuiWidget* gw = (GuiWidget*)w; if (gw) gw->items.push_back(i ? i : "");
}
void aurora_gui_combobox_clear(AuroraWidget w) {
    GuiWidget* gw = (GuiWidget*)w; if (gw) gw->items.clear();
}
int aurora_gui_combobox_get_selected(AuroraWidget w) {
    GuiWidget* gw = (GuiWidget*)w; return gw ? gw->selected_idx : -1;
}
void aurora_gui_combobox_set_selected(AuroraWidget w, int i) {
    GuiWidget* gw = (GuiWidget*)w; if (gw) gw->selected_idx = i;
}
int aurora_gui_combobox_count(AuroraWidget w) {
    GuiWidget* gw = (GuiWidget*)w; return gw ? (int)gw->items.size() : 0;
}
const char* aurora_gui_combobox_get_item(AuroraWidget w, int i) {
    GuiWidget* gw = (GuiWidget*)w; if (!gw || i < 0 || i >= (int)gw->items.size()) return nullptr;
    return gw->items[i].c_str();
}

/* ── DropDown ── */
AuroraWidget aurora_gui_dropdown_new(AuroraWidget p, int x, int y, int w, int h) {
    return aurora_gui_combobox_new(p, x, y, w, h);
}
void aurora_gui_dropdown_add_item(AuroraWidget w, const char* i) { aurora_gui_combobox_add_item(w, i); }
void aurora_gui_dropdown_clear(AuroraWidget w) { aurora_gui_combobox_clear(w); }
int aurora_gui_dropdown_get_selected(AuroraWidget w) { return aurora_gui_combobox_get_selected(w); }
void aurora_gui_dropdown_set_selected(AuroraWidget w, int i) { aurora_gui_combobox_set_selected(w, i); }
int aurora_gui_dropdown_count(AuroraWidget w) { return aurora_gui_combobox_count(w); }
const char* aurora_gui_dropdown_get_item(AuroraWidget w, int i) { return aurora_gui_combobox_get_item(w, i); }

/* ── ListBox ── */
AuroraWidget aurora_gui_listbox_new(AuroraWidget parent, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(5, (GuiWidget*)parent);
    gw->x = x; gw->y = y; gw->w = w; gw->h = h;
    return gw;
}
void aurora_gui_listbox_add_item(AuroraWidget w, const char* i) {
    GuiWidget* gw = (GuiWidget*)w; if (gw) gw->items.push_back(i ? i : "");
}
void aurora_gui_listbox_clear(AuroraWidget w) {
    GuiWidget* gw = (GuiWidget*)w; if (gw) gw->items.clear();
}
int aurora_gui_listbox_get_selected(AuroraWidget w) {
    GuiWidget* gw = (GuiWidget*)w; return gw ? gw->selected_idx : -1;
}
int aurora_gui_listbox_count(AuroraWidget w) {
    GuiWidget* gw = (GuiWidget*)w; return gw ? (int)gw->items.size() : 0;
}

/* ── TreeView ── */
AuroraWidget aurora_gui_treeview_new(AuroraWidget p, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(13, (GuiWidget*)p);
    gw->x = x; gw->y = y; gw->w = w; gw->h = h;
    return gw;
}
AuroraTreeItem aurora_gui_treeview_add_item(AuroraWidget w, const char* s, AuroraTreeItem p) {
    (void)w;(void)s;(void)p; return nullptr;
}
void aurora_gui_treeview_remove_item(AuroraWidget w, AuroraTreeItem i) { (void)w;(void)i; }
void aurora_gui_treeview_clear(AuroraWidget w) { (void)w; }
AuroraTreeItem aurora_gui_treeview_get_selected(AuroraWidget w) { (void)w; return nullptr; }
void aurora_gui_treeview_expand(AuroraWidget w, AuroraTreeItem i) { (void)w;(void)i; }
void aurora_gui_treeview_collapse(AuroraWidget w, AuroraTreeItem i) { (void)w;(void)i; }
void aurora_gui_treeview_set_item_text(AuroraWidget w, AuroraTreeItem i, const char* s) { (void)w;(void)i;(void)s; }

/* ── Table ── */
AuroraWidget aurora_gui_table_new(AuroraWidget p, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(14, (GuiWidget*)p);
    gw->x = x; gw->y = y; gw->w = w; gw->h = h;
    return gw;
}
void aurora_gui_table_add_column(AuroraWidget w, const char* s, int wid) { (void)w;(void)s;(void)wid; }
int aurora_gui_table_column_count(AuroraWidget w) { (void)w; return 0; }
AuroraTableItem aurora_gui_table_add_row(AuroraWidget w) { (void)w; return nullptr; }
void aurora_gui_table_set_cell(AuroraWidget w, int r, int c, const char* s) { (void)w;(void)r;(void)c;(void)s; }
const char* aurora_gui_table_get_cell(AuroraWidget w, int r, int c) { (void)w;(void)r;(void)c; return ""; }
void aurora_gui_table_remove_row(AuroraWidget w, int r) { (void)w;(void)r; }
void aurora_gui_table_clear(AuroraWidget w) { (void)w; }
int aurora_gui_table_get_selected(AuroraWidget w) { (void)w; return -1; }
int aurora_gui_table_row_count(AuroraWidget w) { (void)w; return 0; }

/* ── TabView ── */
AuroraWidget aurora_gui_tabview_new(AuroraWidget p, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(15, (GuiWidget*)p);
    gw->x = x; gw->y = y; gw->w = w; gw->h = h;
    return gw;
}
AuroraWidget aurora_gui_tabview_add_page(AuroraWidget w, const char* s) {
    GuiWidget* gw = (GuiWidget*)w; if (gw) gw->items.push_back(s ? s : "");
    return w;
}
int aurora_gui_tabview_get_selected(AuroraWidget w) {
    GuiWidget* gw = (GuiWidget*)w; return gw ? gw->selected_idx : -1;
}
void aurora_gui_tabview_set_selected(AuroraWidget w, int i) {
    GuiWidget* gw = (GuiWidget*)w; if (gw) gw->selected_idx = i;
}
int aurora_gui_tabview_page_count(AuroraWidget w) {
    GuiWidget* gw = (GuiWidget*)w; return gw ? (int)gw->items.size() : 0;
}

/* ── ScrollView ── */
AuroraWidget aurora_gui_scrollview_new(AuroraWidget p, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(16, (GuiWidget*)p);
    gw->x = x; gw->y = y; gw->w = w; gw->h = h;
    return gw;
}

/* ── SplitView ── */
AuroraWidget aurora_gui_splitview_new(AuroraWidget p, int x, int y, int w, int h, int o) {
    (void)o; GuiWidget* gw = widget_new(21, (GuiWidget*)p);
    gw->x = x; gw->y = y; gw->w = w; gw->h = h;
    return gw;
}
void aurora_gui_splitview_set_position(AuroraWidget w, int p) { (void)w;(void)p; }
int aurora_gui_splitview_get_position(AuroraWidget w) { (void)w; return 0; }
AuroraWidget aurora_gui_splitview_get_pane1(AuroraWidget w) { (void)w; return nullptr; }
AuroraWidget aurora_gui_splitview_get_pane2(AuroraWidget w) { (void)w; return nullptr; }

/* ── GroupBox ── */
AuroraWidget aurora_gui_groupbox_new(AuroraWidget p, const char* s, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(22, (GuiWidget*)p);
    gw->x = x; gw->y = y; gw->w = w; gw->h = h; gw->text = s ? s : "";
    return gw;
}

/* ── Image ── */
AuroraWidget aurora_gui_image_new(AuroraWidget p, const char* path, int x, int y, int w, int h) {
    (void)path; GuiWidget* gw = widget_new(18, (GuiWidget*)p);
    gw->x = x; gw->y = y; gw->w = w; gw->h = h;
    return gw;
}
void aurora_gui_image_load(AuroraWidget w, const char* s) { (void)w;(void)s; }
void aurora_gui_image_set_data(AuroraWidget w, const unsigned char* d, int l) { (void)w;(void)d;(void)l; }

/* ── Canvas ── */
AuroraWidget aurora_gui_canvas_new(AuroraWidget p, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(17, (GuiWidget*)p);
    gw->x = x; gw->y = y; gw->w = w; gw->h = h;
    Window sub = XCreateSimpleWindow(g_display, g_root, x, y, w, h, 0, 0, WhitePixel(g_display, g_screen));
    GC gc = XCreateGC(g_display, sub, 0, nullptr);
    XSelectInput(g_display, sub, ExposureMask | ButtonPressMask);
    XMapWindow(g_display, sub);
    gw->xwindow = sub; gw->gc = gc;
    return gw;
}
void aurora_gui_canvas_set_paint_callback(AuroraWidget w, AuroraPaintCallback cb, void* u) {
    GuiWidget* gw = (GuiWidget*)w; if (gw) { gw->paint_cb = cb; gw->paint_user = u; }
}
void aurora_gui_canvas_repaint(AuroraWidget w) {
    GuiWidget* gw = (GuiWidget*)w; if (gw && gw->paint_cb) gw->paint_cb(gw->id, gw->paint_user);
}

/* ── Menu ── */
AuroraMenu aurora_gui_menu_bar_new(AuroraWidget p) { (void)p; return nullptr; }
AuroraMenu aurora_gui_menu_new(const char* s) { (void)s; return nullptr; }
void aurora_gui_menu_add_item(AuroraMenu m, const char* s, int i) { (void)m;(void)s;(void)i; }
void aurora_gui_menu_add_separator(AuroraMenu m) { (void)m; }
void aurora_gui_menu_add_submenu(AuroraMenu m, AuroraMenu s) { (void)m;(void)s; }
void aurora_gui_menu_bar_add_menu(AuroraMenu m, AuroraMenu s) { (void)m;(void)s; }
void aurora_gui_menu_set_checked(AuroraMenu m, int i, int c) { (void)m;(void)i;(void)c; }
void aurora_gui_menu_set_enabled(AuroraMenu m, int i, int e) { (void)m;(void)i;(void)e; }

/* ── ToolBar ── */
AuroraWidget aurora_gui_toolbar_new(AuroraWidget p, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(23, (GuiWidget*)p);
    gw->x = x; gw->y = y; gw->w = w; gw->h = h;
    return gw;
}
void aurora_gui_toolbar_add_button(AuroraWidget w, const char* s, int i) { (void)w;(void)s;(void)i; }
void aurora_gui_toolbar_add_separator(AuroraWidget w) { (void)w; }

/* ── StatusBar ── */
AuroraWidget aurora_gui_statusbar_new(AuroraWidget p, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(19, (GuiWidget*)p);
    gw->x = x; gw->y = y; gw->w = w; gw->h = h;
    return gw;
}
void aurora_gui_statusbar_set_text(AuroraWidget w, const char* t) {
    GuiWidget* gw = (GuiWidget*)w; if (gw) gw->text = t ? t : "";
}
const char* aurora_gui_statusbar_get_text(AuroraWidget w) { return aurora_gui_label_get_text(w); }
void aurora_gui_statusbar_set_parts(AuroraWidget w, const int* widths, int c) { (void)w;(void)widths;(void)c; }

/* ── Dialog ── */
AuroraWidget aurora_gui_dialog_new(AuroraWidget p, const char* s, int w, int h) { (void)p;(void)s;(void)w;(void)h; return nullptr; }
int aurora_gui_dialog_show_modal(AuroraWidget d) { (void)d; return 0; }
void aurora_gui_dialog_close(AuroraWidget d) { (void)d; }

/* ── MessageBox ── */
int aurora_gui_messagebox_show(AuroraWidget parent, const char* title, const char* message, int type) {
    (void)parent;
    if (!title || !message) return 0;
    const char* icon = "";
    switch (type) {
        case 0: icon = "info"; break;
        case 1: icon = "warning"; break;
        case 2: icon = "error"; break;
        case 3: icon = "question"; break;
        default: icon = "info"; break;
    }
    std::string cmd = "zenity --" + std::string(icon) + " --title=\"" + std::string(title) + "\" --text=\"" + std::string(message) + "\" --width=400 2>/dev/null";
    int ret = system(cmd.c_str());
    if (ret != 0) {
        cmd = "xmessage -center -title \"" + std::string(title) + "\" \"" + std::string(message) + "\"";
        ret = system(cmd.c_str());
    }
    return ret == 0 ? 1 : 0;
}

/* ── FilePicker ── */
const char* aurora_gui_file_open_dialog(AuroraWidget p, const char* t, const char* f) { (void)p;(void)t;(void)f; return nullptr; }
const char* aurora_gui_file_save_dialog(AuroraWidget p, const char* t, const char* f) { (void)p;(void)t;(void)f; return nullptr; }
const char* aurora_gui_folder_select_dialog(AuroraWidget p, const char* t) { (void)p;(void)t; return nullptr; }

/* ── ColorPicker, FontPicker ── */
int aurora_gui_color_picker_dialog(AuroraWidget p, unsigned int c) { (void)p;(void)c; return -1; }
int aurora_gui_font_picker_dialog(AuroraWidget p, AuroraFontInfo* f) { (void)p;(void)f; return 0; }

/* ── Notification ── */
int aurora_gui_notification_show(AuroraWidget p, const char* t, const char* m, int i) { (void)p;(void)t;(void)m;(void)i; return -1; }
void aurora_gui_notification_remove(AuroraWidget p) { (void)p; }

/* ── Clipboard ── */
static std::string g_clipboard_text;
int aurora_gui_clipboard_set_text(const char* s) {
    if (!s) return -1;
    g_clipboard_text = s;
    if (g_display) {
        XSetSelectionOwner(g_display, XA_CLIPBOARD(g_display), g_root, CurrentTime);
    }
    return 0;
}
const char* aurora_gui_clipboard_get_text(void) {
    return g_clipboard_text.c_str();
}

/* ── Cursor ── */
void aurora_gui_cursor_set(int c) {
    if (!g_display) return;
    unsigned int shape;
    switch (c) {
        case 0: shape = XC_left_ptr; break;
        case 1: shape = XC_xterm; break;
        case 2: shape = XC_watch; break;
        case 3: shape = XC_crosshair; break;
        case 4: shape = XC_hand1; break;
        case 5: shape = XC_fleur; break;
        default: shape = XC_left_ptr; break;
    }
    Cursor cur = XCreateFontCursor(g_display, shape);
    XDefineCursor(g_display, g_root, cur);
    XFreeCursor(g_display, cur);
}
int aurora_gui_cursor_get(void) { return 0; }

/* ── Keyboard ── */
int aurora_gui_keyboard_is_key_down(int k) {
    if (k >= 0 && k < 256) return g_key_state[k] ? 1 : 0;
    return 0;
}
int aurora_gui_keyboard_get_modifiers(void) {
    return g_mod_state;
}

/* ── Mouse ── */
int aurora_gui_mouse_get_x(void) { return g_mouse_x; }
int aurora_gui_mouse_get_y(void) { return g_mouse_y; }
int aurora_gui_mouse_button_down(int b) { if (b >= 0 && b < 5) return g_mouse_buttons[b]; return 0; }
void aurora_gui_mouse_set_pos(int x, int y) { (void)x;(void)y; }

/* ── Legacy aliases ── */
void aurora_gui_run() { aurora_gui_app_run(); }
void aurora_gui_quit() { aurora_gui_app_quit(); }
void aurora_gui_layout_horizontal(AuroraWidget p, int m) { (void)p;(void)m; }
void aurora_gui_layout_vertical(AuroraWidget p, int m) { (void)p;(void)m; }

/* ── Layout stubs (handled by app_layout.cpp) ── */
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

/* ── Widget introspection (Linux) ── */
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
    if (gw) { if(x)*x=gw->x; if(y)*y=gw->y; if(w)*w=gw->w; if(h)*h=gw->h; }
}
int aurora_gui_widget_get_id(void* widget) {
    GuiWidget* w = (GuiWidget*)widget;
    return w ? w->id : -1;
}
void* aurora_gui_widget_find_at(int x, int y) {
    for (auto* w : g_widgets) {
        if (!w) continue;
        if (x >= w->x && x < w->x + w->w && y >= w->y && y < w->y + w->h) return w;
    }
    return nullptr;
}
int aurora_gui_widget_count(void) { return (int)g_widgets.size(); }
void* aurora_gui_widget_get_by_index(int idx) {
    if (idx < 0 || idx >= (int)g_widgets.size()) return nullptr;
    return g_widgets[idx];
}

/* ── WebView (Linux: WebKitGTK via dlopen, fallback stub) ── */
AuroraWidget aurora_gui_webview_new(AuroraWidget p, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(24, (GuiWidget*)p);
    gw->x = x; gw->y = y; gw->w = w; gw->h = h;
    return gw;
}
void aurora_gui_webview_navigate(AuroraWidget wv, const char* url) { (void)wv; (void)url; }
void aurora_gui_webview_go_back(AuroraWidget wv) { (void)wv; }
void aurora_gui_webview_go_forward(AuroraWidget wv) { (void)wv; }
void aurora_gui_webview_reload(AuroraWidget wv) { (void)wv; }

/* ── Media Player (Linux: FFmpeg via dlopen, fallback stub) ── */
AuroraWidget aurora_gui_media_new(AuroraWidget p, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(25, (GuiWidget*)p);
    gw->x = x; gw->y = y; gw->w = w; gw->h = h;
    return gw;
}
void aurora_gui_media_play(AuroraWidget m) { (void)m; }
void aurora_gui_media_pause(AuroraWidget m) { (void)m; }
void aurora_gui_media_stop(AuroraWidget m) { (void)m; }
void aurora_gui_media_load(AuroraWidget m, const char* src) { (void)m; (void)src; }

/* ── Map (Linux: Leaflet via headless WebKitGTK, fallback stub) ── */
AuroraWidget aurora_gui_map_new(AuroraWidget p, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(26, (GuiWidget*)p);
    gw->x = x; gw->y = y; gw->w = w; gw->h = h;
    return gw;
}
void aurora_gui_map_set_center(AuroraWidget m, double lat, double lon) { (void)m; (void)lat; (void)lon; }
void aurora_gui_map_set_zoom(AuroraWidget m, int z) { (void)m; (void)z; }
void aurora_gui_map_add_marker(AuroraWidget m, double lat, double lon, const char* label) { (void)m; (void)lat; (void)lon; (void)label; }

#elif defined(__APPLE__)
  /* macOS implementation provided by ui_mac.mm — include header only */
  #include "../../include/std/gui.hpp"
#else /* unknown platform stubs */

#include "../../include/std/gui.hpp"
#include <cstdlib>
#include <cstring>

static struct { int w, h; } g_win = {800, 600};

int aurora_gui_app_init(void) { return 0; }
void aurora_gui_app_run(void) {}
void aurora_gui_app_quit(void) {}
void aurora_gui_set_enabled(AuroraWidget w, int e) { (void)w;(void)e; }
int aurora_gui_get_enabled(AuroraWidget w) { (void)w; return 1; }
void aurora_gui_set_visible(AuroraWidget w, int v) { (void)w;(void)v; }
int aurora_gui_get_visible(AuroraWidget w) { (void)w; return 1; }
void aurora_gui_set_focus(AuroraWidget w) { (void)w; }
void aurora_gui_move(AuroraWidget w, int x, int y, int w_, int h_) { (void)w;(void)x;(void)y;(void)w_;(void)h_; }
AuroraWidget aurora_gui_window_new(const char* t, int w, int h) { (void)t; g_win.w = w; g_win.h = h; return nullptr; }
void aurora_gui_window_set_title(AuroraWidget w, const char* t) { (void)w;(void)t; }
void aurora_gui_window_resize(AuroraWidget w, int w_, int h_) { (void)w;(void)w_;(void)h_; }
void aurora_gui_window_show(AuroraWidget w) { (void)w; }
void aurora_gui_window_hide(AuroraWidget w) { (void)w; }
void aurora_gui_window_destroy(AuroraWidget w) { (void)w; }
void aurora_gui_window_maximize(AuroraWidget w) { (void)w; }
void aurora_gui_window_minimize(AuroraWidget w) { (void)w; }
void aurora_gui_window_restore(AuroraWidget w) { (void)w; }
int aurora_gui_window_get_width(AuroraWidget w) { (void)w; return g_win.w; }
int aurora_gui_window_get_height(AuroraWidget w) { (void)w; return g_win.h; }
void aurora_gui_window_set_min_size(AuroraWidget w, int a, int b) { (void)w;(void)a;(void)b; }
void aurora_gui_window_set_max_size(AuroraWidget w, int a, int b) { (void)w;(void)a;(void)b; }
void aurora_gui_window_set_resizable(AuroraWidget w, int r) { (void)w;(void)r; }
void aurora_gui_set_callback(AuroraWidget w, AuroraEventCallback cb) { (void)w;(void)cb; }
void* aurora_gui_get_native_handle(AuroraWidget w) { (void)w; return nullptr; }
AuroraWidget aurora_gui_label_new(AuroraWidget p, const char* t, int x, int y, int w, int h) { (void)p;(void)t;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_label_set_text(AuroraWidget w, const char* t) { (void)w;(void)t; }
const char* aurora_gui_label_get_text(AuroraWidget w) { (void)w; static const char* e = ""; return e; }
void aurora_gui_label_set_font_size(AuroraWidget w, int s) { (void)w;(void)s; }
void aurora_gui_label_set_color(AuroraWidget w, unsigned int c) { (void)w;(void)c; }
void aurora_gui_label_set_align(AuroraWidget w, int a) { (void)w;(void)a; }
AuroraWidget aurora_gui_text_new(AuroraWidget p, const char* t, int x, int y, int w, int h) { return aurora_gui_label_new(p, t, x, y, w, h); }
void aurora_gui_text_set_text(AuroraWidget w, const char* t) { aurora_gui_label_set_text(w, t); }
const char* aurora_gui_text_get_text(AuroraWidget w) { return aurora_gui_label_get_text(w); }
AuroraWidget aurora_gui_button_new(AuroraWidget p, const char* t, int x, int y, int w, int h) { (void)p;(void)t;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_button_set_text(AuroraWidget w, const char* t) { (void)w;(void)t; }
const char* aurora_gui_button_get_text(AuroraWidget w) { (void)w; static const char* e = ""; return e; }
AuroraWidget aurora_gui_checkbox_new(AuroraWidget p, const char* t, int x, int y, int w, int h) { (void)p;(void)t;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_checkbox_set_text(AuroraWidget w, const char* t) { (void)w;(void)t; }
const char* aurora_gui_checkbox_get_text(AuroraWidget w) { (void)w; return ""; }
int aurora_gui_checkbox_is_checked(AuroraWidget w) { (void)w; return 0; }
void aurora_gui_checkbox_set_checked(AuroraWidget w, int v) { (void)w;(void)v; }
AuroraWidget aurora_gui_radiobutton_new(AuroraWidget p, const char* t, int x, int y, int w, int h, int g) { (void)p;(void)t;(void)x;(void)y;(void)w;(void)h;(void)g; return nullptr; }
void aurora_gui_radiobutton_set_text(AuroraWidget w, const char* t) { (void)w;(void)t; }
const char* aurora_gui_radiobutton_get_text(AuroraWidget w) { (void)w; return ""; }
int aurora_gui_radiobutton_is_checked(AuroraWidget w) { (void)w; return 0; }
void aurora_gui_radiobutton_set_checked(AuroraWidget w, int v) { (void)w;(void)v; }
AuroraWidget aurora_gui_switch_new(AuroraWidget p, const char* t, int x, int y, int w, int h) { return aurora_gui_checkbox_new(p, t, x, y, w, h); }
int aurora_gui_switch_is_on(AuroraWidget w) { return aurora_gui_checkbox_is_checked(w); }
void aurora_gui_switch_set_on(AuroraWidget w, int v) { aurora_gui_checkbox_set_checked(w, v); }
AuroraWidget aurora_gui_textbox_new(AuroraWidget p, int x, int y, int w, int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_textbox_set_text(AuroraWidget w, const char* t) { (void)w;(void)t; }
const char* aurora_gui_textbox_get_text(AuroraWidget w) { (void)w; return ""; }
void aurora_gui_textbox_set_readonly(AuroraWidget w, int r) { (void)w;(void)r; }
void aurora_gui_textbox_set_placeholder(AuroraWidget w, const char* t) { (void)w;(void)t; }
void aurora_gui_textbox_set_multiline(AuroraWidget w, int m) { (void)w;(void)m; }
int aurora_gui_textbox_get_line_count(AuroraWidget w) { (void)w; return 0; }
AuroraWidget aurora_gui_passwordbox_new(AuroraWidget p, int x, int y, int w, int h) { return aurora_gui_textbox_new(p, x, y, w, h); }
void aurora_gui_passwordbox_set_text(AuroraWidget w, const char* t) { aurora_gui_textbox_set_text(w, t); }
const char* aurora_gui_passwordbox_get_text(AuroraWidget w) { return aurora_gui_textbox_get_text(w); }
AuroraWidget aurora_gui_slider_new(AuroraWidget p, int x, int y, int w, int h, int mn, int mx) { (void)p;(void)x;(void)y;(void)w;(void)h;(void)mn;(void)mx; return nullptr; }
int aurora_gui_slider_get_value(AuroraWidget w) { (void)w; return 0; }
void aurora_gui_slider_set_value(AuroraWidget w, int v) { (void)w;(void)v; }
void aurora_gui_slider_set_range(AuroraWidget w, int mn, int mx) { (void)w;(void)mn;(void)mx; }
AuroraWidget aurora_gui_progressbar_new(AuroraWidget p, int x, int y, int w, int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_progressbar_set_value(AuroraWidget w, int v) { (void)w;(void)v; }
int aurora_gui_progressbar_get_value(AuroraWidget w) { (void)w; return 0; }
void aurora_gui_progressbar_set_range(AuroraWidget w, int mn, int mx) { (void)w;(void)mn;(void)mx; }
void aurora_gui_progressbar_set_marquee(AuroraWidget w, int v) { (void)w;(void)v; }
AuroraWidget aurora_gui_combobox_new(AuroraWidget p, int x, int y, int w, int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_combobox_add_item(AuroraWidget w, const char* i) { (void)w;(void)i; }
void aurora_gui_combobox_clear(AuroraWidget w) { (void)w; }
int aurora_gui_combobox_get_selected(AuroraWidget w) { (void)w; return -1; }
void aurora_gui_combobox_set_selected(AuroraWidget w, int i) { (void)w;(void)i; }
int aurora_gui_combobox_count(AuroraWidget w) { (void)w; return 0; }
const char* aurora_gui_combobox_get_item(AuroraWidget w, int i) { (void)w;(void)i; return nullptr; }
AuroraWidget aurora_gui_dropdown_new(AuroraWidget p, int x, int y, int w, int h) { return aurora_gui_combobox_new(p, x, y, w, h); }
void aurora_gui_dropdown_add_item(AuroraWidget w, const char* i) { aurora_gui_combobox_add_item(w, i); }
void aurora_gui_dropdown_clear(AuroraWidget w) { aurora_gui_combobox_clear(w); }
int aurora_gui_dropdown_get_selected(AuroraWidget w) { return aurora_gui_combobox_get_selected(w); }
void aurora_gui_dropdown_set_selected(AuroraWidget w, int i) { aurora_gui_combobox_set_selected(w, i); }
int aurora_gui_dropdown_count(AuroraWidget w) { return aurora_gui_combobox_count(w); }
const char* aurora_gui_dropdown_get_item(AuroraWidget w, int i) { return aurora_gui_combobox_get_item(w, i); }
AuroraWidget aurora_gui_listbox_new(AuroraWidget p, int x, int y, int w, int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_listbox_add_item(AuroraWidget w, const char* i) { (void)w;(void)i; }
void aurora_gui_listbox_clear(AuroraWidget w) { (void)w; }
int aurora_gui_listbox_get_selected(AuroraWidget w) { (void)w; return -1; }
const char* aurora_gui_listbox_get_item(AuroraWidget w, int i) { (void)w;(void)i; return nullptr; }
int aurora_gui_listbox_count(AuroraWidget w) { (void)w; return 0; }
AuroraWidget aurora_gui_treeview_new(AuroraWidget p, int x, int y, int w, int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
AuroraTreeItem aurora_gui_treeview_add_item(AuroraWidget w, const char* s, AuroraTreeItem p) { (void)w;(void)s;(void)p; return nullptr; }
void aurora_gui_treeview_remove_item(AuroraWidget w, AuroraTreeItem i) { (void)w;(void)i; }
void aurora_gui_treeview_clear(AuroraWidget w) { (void)w; }
AuroraTreeItem aurora_gui_treeview_get_selected(AuroraWidget w) { (void)w; return nullptr; }
void aurora_gui_treeview_expand(AuroraWidget w, AuroraTreeItem i) { (void)w;(void)i; }
void aurora_gui_treeview_collapse(AuroraWidget w, AuroraTreeItem i) { (void)w;(void)i; }
void aurora_gui_treeview_set_item_text(AuroraWidget w, AuroraTreeItem i, const char* s) { (void)w;(void)i;(void)s; }
AuroraWidget aurora_gui_table_new(AuroraWidget p, int x, int y, int w, int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_table_add_column(AuroraWidget w, const char* s, int wid) { (void)w;(void)s;(void)wid; }
int aurora_gui_table_column_count(AuroraWidget w) { (void)w; return 0; }
AuroraTableItem aurora_gui_table_add_row(AuroraWidget w) { (void)w; return nullptr; }
void aurora_gui_table_set_cell(AuroraWidget w, int r, int c, const char* s) { (void)w;(void)r;(void)c;(void)s; }
const char* aurora_gui_table_get_cell(AuroraWidget w, int r, int c) { (void)w;(void)r;(void)c; return ""; }
void aurora_gui_table_remove_row(AuroraWidget w, int r) { (void)w;(void)r; }
void aurora_gui_table_clear(AuroraWidget w) { (void)w; }
int aurora_gui_table_get_selected(AuroraWidget w) { (void)w; return -1; }
int aurora_gui_table_row_count(AuroraWidget w) { (void)w; return 0; }
AuroraWidget aurora_gui_tabview_new(AuroraWidget p, int x, int y, int w, int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
AuroraWidget aurora_gui_tabview_add_page(AuroraWidget w, const char* s) { (void)w;(void)s; return w; }
int aurora_gui_tabview_get_selected(AuroraWidget w) { (void)w; return -1; }
void aurora_gui_tabview_set_selected(AuroraWidget w, int i) { (void)w;(void)i; }
int aurora_gui_tabview_page_count(AuroraWidget w) { (void)w; return 0; }
AuroraWidget aurora_gui_scrollview_new(AuroraWidget p, int x, int y, int w, int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
AuroraWidget aurora_gui_splitview_new(AuroraWidget p, int x, int y, int w, int h, int o) { (void)p;(void)x;(void)y;(void)w;(void)h;(void)o; return nullptr; }
void aurora_gui_splitview_set_position(AuroraWidget w, int p) { (void)w;(void)p; }
int aurora_gui_splitview_get_position(AuroraWidget w) { (void)w; return 0; }
AuroraWidget aurora_gui_splitview_get_pane1(AuroraWidget w) { (void)w; return nullptr; }
AuroraWidget aurora_gui_splitview_get_pane2(AuroraWidget w) { (void)w; return nullptr; }
AuroraWidget aurora_gui_groupbox_new(AuroraWidget p, const char* s, int x, int y, int w, int h) { (void)p;(void)s;(void)x;(void)y;(void)w;(void)h; return nullptr; }
AuroraWidget aurora_gui_image_new(AuroraWidget p, const char* path, int x, int y, int w, int h) { (void)p;(void)path;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_image_load(AuroraWidget w, const char* s) { (void)w;(void)s; }
void aurora_gui_image_set_data(AuroraWidget w, const unsigned char* d, int l) { (void)w;(void)d;(void)l; }
AuroraWidget aurora_gui_canvas_new(AuroraWidget p, int x, int y, int w, int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_canvas_set_paint_callback(AuroraWidget w, AuroraPaintCallback cb, void* u) { (void)w;(void)cb;(void)u; }
void aurora_gui_canvas_repaint(AuroraWidget w) { (void)w; }
AuroraMenu aurora_gui_menu_bar_new(AuroraWidget p) { (void)p; return nullptr; }
AuroraMenu aurora_gui_menu_new(const char* s) { (void)s; return nullptr; }
void aurora_gui_menu_add_item(AuroraMenu m, const char* s, int i) { (void)m;(void)s;(void)i; }
void aurora_gui_menu_add_separator(AuroraMenu m) { (void)m; }
void aurora_gui_menu_add_submenu(AuroraMenu m, AuroraMenu s) { (void)m;(void)s; }
void aurora_gui_menu_bar_add_menu(AuroraMenu m, AuroraMenu s) { (void)m;(void)s; }
void aurora_gui_menu_set_checked(AuroraMenu m, int i, int c) { (void)m;(void)i;(void)c; }
void aurora_gui_menu_set_enabled(AuroraMenu m, int i, int e) { (void)m;(void)i;(void)e; }
AuroraWidget aurora_gui_toolbar_new(AuroraWidget p, int x, int y, int w, int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_toolbar_add_button(AuroraWidget w, const char* s, int i) { (void)w;(void)s;(void)i; }
void aurora_gui_toolbar_add_separator(AuroraWidget w) { (void)w; }
AuroraWidget aurora_gui_statusbar_new(AuroraWidget p, int x, int y, int w, int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_statusbar_set_text(AuroraWidget w, const char* t) { (void)w;(void)t; }
const char* aurora_gui_statusbar_get_text(AuroraWidget w) { (void)w; return ""; }
void aurora_gui_statusbar_set_parts(AuroraWidget w, const int* widths, int c) { (void)w;(void)widths;(void)c; }
AuroraWidget aurora_gui_dialog_new(AuroraWidget p, const char* s, int w, int h) { (void)p;(void)s;(void)w;(void)h; return nullptr; }
int aurora_gui_dialog_show_modal(AuroraWidget d) { (void)d; return 0; }
void aurora_gui_dialog_close(AuroraWidget d) { (void)d; }
int aurora_gui_messagebox_show(AuroraWidget p, const char* t, const char* m, int ty) { (void)p;(void)t;(void)m;(void)ty; return 0; }
const char* aurora_gui_file_open_dialog(AuroraWidget p, const char* t, const char* f) { (void)p;(void)t;(void)f; return nullptr; }
const char* aurora_gui_file_save_dialog(AuroraWidget p, const char* t, const char* f) { (void)p;(void)t;(void)f; return nullptr; }
const char* aurora_gui_folder_select_dialog(AuroraWidget p, const char* t) { (void)p;(void)t; return nullptr; }
int aurora_gui_color_picker_dialog(AuroraWidget p, unsigned int c) { (void)p;(void)c; return -1; }
int aurora_gui_font_picker_dialog(AuroraWidget p, AuroraFontInfo* f) { (void)p;(void)f; return 0; }
int aurora_gui_notification_show(AuroraWidget p, const char* t, const char* m, int i) { (void)p;(void)t;(void)m;(void)i; return -1; }
void aurora_gui_notification_remove(AuroraWidget p) { (void)p; }
int aurora_gui_clipboard_set_text(const char* s) { (void)s; return 0; }
const char* aurora_gui_clipboard_get_text(void) { return nullptr; }
void aurora_gui_cursor_set(int c) { (void)c; }
int aurora_gui_cursor_get(void) { return 0; }
int aurora_gui_keyboard_is_key_down(int k) { (void)k; return 0; }
int aurora_gui_keyboard_get_modifiers(void) { return 0; }
int aurora_gui_mouse_get_x(void) { return 0; }
int aurora_gui_mouse_get_y(void) { return 0; }
int aurora_gui_mouse_button_down(int b) { (void)b; return 0; }
void aurora_gui_mouse_set_pos(int x, int y) { (void)x;(void)y; }
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
   Phase 9: WebView stubs (non-Win32)
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_webview_new(AuroraWidget p,int x,int y,int w,int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_webview_navigate(AuroraWidget w,const char* u) { (void)w;(void)u; }
void aurora_gui_webview_go_back(AuroraWidget w) { (void)w; }
void aurora_gui_webview_go_forward(AuroraWidget w) { (void)w; }
void aurora_gui_webview_reload(AuroraWidget w) { (void)w; }

/* ════════════════════════════════════════════════════════════
   Phase 9: Media Player stubs (non-Win32)
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_media_new(AuroraWidget p,int x,int y,int w,int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_media_open(AuroraWidget m,const char* s) { (void)m;(void)s; }
void aurora_gui_media_play(AuroraWidget m) { (void)m; }
void aurora_gui_media_pause(AuroraWidget m) { (void)m; }
void aurora_gui_media_stop(AuroraWidget m) { (void)m; }
void aurora_gui_media_set_volume(AuroraWidget m,float v) { (void)m;(void)v; }
void aurora_gui_media_set_looping(AuroraWidget m,int l) { (void)m;(void)l; }
int aurora_gui_media_is_playing(AuroraWidget m) { (void)m; return 0; }

/* ════════════════════════════════════════════════════════════
   Phase 9: Map stubs (non-Win32)
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_map_new(AuroraWidget p,int x,int y,int w,int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_map_set_center(AuroraWidget m,double la,double lo) { (void)m;(void)la;(void)lo; }
void aurora_gui_map_set_zoom(AuroraWidget m,int z) { (void)m;(void)z; }
void aurora_gui_map_add_marker(AuroraWidget m,double la,double lo,const char* t) { (void)m;(void)la;(void)lo;(void)t; }

/* ════════════════════════════════════════════════════════════
   Phase 10: Widget Introspection stubs (non-Win32/non-Linux)
   ════════════════════════════════════════════════════════════ */
int aurora_gui_widget_get_type(void* widget) { (void)widget; return 0; }
void* aurora_gui_widget_get_parent(void* widget) { (void)widget; return nullptr; }
const char* aurora_gui_widget_get_text(void* widget) { (void)widget; return nullptr; }
void aurora_gui_widget_get_bounds(void* widget, int* x, int* y, int* w, int* h) { (void)widget;(void)x;(void)y;(void)w;(void)h; }
int aurora_gui_widget_get_id(void* widget) { (void)widget; return -1; }
void* aurora_gui_widget_find_at(int x, int y) { (void)x;(void)y; return nullptr; }
int aurora_gui_widget_count(void) { return 0; }
void* aurora_gui_widget_get_by_index(int idx) { (void)idx; return nullptr; }

#endif /* _WIN32 / __linux__ / else */
