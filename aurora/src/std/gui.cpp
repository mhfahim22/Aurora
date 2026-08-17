/* gui.cpp -- Platform dispatcher for native GUI */

/* On Windows: all aurora_gui_* functions are in ui_win32.cpp */
/* On Linux: X11 implementation is inline here */
#if defined(_WIN32)
  #include "../../include/std/gui.hpp"
  /* Windows implementation provided by ui_win32.cpp */
#elif defined(__linux__) && !defined(__ANDROID__)

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
#include <X11/cursorfont.h>
#include <unistd.h>

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

/* ── TreeView data model ── */
struct TreeNode {
    int id;
    int parent_id;
    std::string text;
    int expanded;
};
static std::map<int, std::vector<TreeNode>> g_tree_nodes;
static std::map<int, int> g_tree_next_node_id;
static std::map<int, std::map<int, int>> g_tree_node_index;
static std::map<int, int> g_tree_selected;

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

static void draw_treeview(GuiWidget* w) {
    if (!w || !w->xwindow || !w->gc) return;
    XClearWindow(g_display, w->xwindow);
    int wid = w->id;
    auto it = g_tree_nodes.find(wid);
    if (it == g_tree_nodes.end()) return;
    int y = 14, row = 0;
    for (const auto& node : it->second) {
        std::string label = node.text;
        if (node.parent_id != 0) label = "  " + label;
        XDrawString(g_display, w->xwindow, w->gc, 4, y, label.c_str(), (int)label.size());
        int sel = g_tree_selected[wid];
        if (sel != 0 && node.id == sel) {
            /* Draw selection underline. */
            int tw = (int)label.size() * 8;
            XDrawLine(g_display, w->xwindow, w->gc, 4, y + 2, 4 + tw, y + 2);
        }
        y += 14;
        if (++row > w->h / 14) break;
    }
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
                    if (w->xwindow && w->gc) {
                        if (w->type == 13) draw_treeview(w);
                        else draw_label(w);
                    }
                }
            }
            if (ev.type == ButtonPress) {
                if (ev.xbutton.button < 5) g_mouse_buttons[ev.xbutton.button] = 1;
                g_mouse_x = ev.xbutton.x; g_mouse_y = ev.xbutton.y;
                for (auto* w : g_widgets) {
                    if (w->xwindow && ev.xbutton.window == w->xwindow) {
                        if (w->type == 13) {
                            /* Hit-test the clicked row and select it. */
                            int row = ev.xbutton.y / 14;
                            auto it = g_tree_nodes.find(w->id);
                            if (it != g_tree_nodes.end() && row >= 0 && row < (int)it->second.size()) {
                                int nid = it->second[row].id;
                                g_tree_selected[w->id] = nid;
                                fire_event(w, 17, nid, 0); /* AURORA_EVENT_TREE_SELECT */
                                draw_treeview(w);
                            }
                        }
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

int aurora_gui_window_set_dark_mode(AuroraWidget w, int enable) {
    /* X11 has no native title-bar dark mode; set the _GTK_THEME_VARIANT
       hint honored by GNOME/GTK window managers. */
    GuiWidget* gw = (GuiWidget*)w;
    if (gw && gw->xwindow && g_display) {
        Atom variant = XInternAtom(g_display, "_GTK_THEME_VARIANT", False);
        if (variant != None) {
            const char* val = enable ? "dark" : "light";
            XChangeProperty(g_display, gw->xwindow, variant, XA_STRING, 8,
                PropModeReplace, (const unsigned char*)val, (int)strlen(val));
        }
    }
    return 0;
}

int aurora_gui_window_set_effect(AuroraWidget w, int effect) {
    /* No acrylic/mica equivalent on X11; no-op success. */
    (void)w; (void)effect;
    return 0;
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
const char* aurora_gui_listbox_get_item(AuroraWidget w, int i) {
    GuiWidget* gw = (GuiWidget*)w;
    if (gw && i >= 0 && i < (int)gw->items.size()) return gw->items[i].c_str();
    return nullptr;
}

/* ── TreeView ── */
/* In-memory tree data model (same structure as the macOS backend). */

static TreeNode* tree_find_node(int wid, int node_id) {
    auto it = g_tree_node_index.find(wid);
    if (it == g_tree_node_index.end()) return nullptr;
    auto jt = it->second.find(node_id);
    if (jt == it->second.end()) return nullptr;
    int idx = jt->second;
    if (idx < 0 || idx >= (int)g_tree_nodes[wid].size()) return nullptr;
    return &g_tree_nodes[wid][idx];
}

AuroraWidget aurora_gui_treeview_new(AuroraWidget p, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(13, (GuiWidget*)p);
    gw->x = x; gw->y = y; gw->w = w; gw->h = h;
    g_tree_nodes[gw->id] = {};
    g_tree_next_node_id[gw->id] = 1;
    g_tree_node_index[gw->id] = {};
    g_tree_selected[gw->id] = 0;
    Window parent_win = ((GuiWidget*)p) ? ((GuiWidget*)p)->xwindow : g_root;
    if (parent_win) {
        Window sub = XCreateSimpleWindow(g_display, parent_win, x, y, w, h, 1,
            BlackPixel(g_display, g_screen), 0xFFFFFF);
        GC gc = XCreateGC(g_display, sub, 0, nullptr);
        XSetForeground(g_display, gc, BlackPixel(g_display, g_screen));
        XSelectInput(g_display, sub, ExposureMask | ButtonPressMask);
        XMapWindow(g_display, sub);
        gw->xwindow = sub; gw->gc = gc;
    }
    return gw;
}
AuroraTreeItem aurora_gui_treeview_add_item(AuroraWidget w, const char* s, AuroraTreeItem p) {
    GuiWidget* gw = (GuiWidget*)w; if (!gw) return nullptr;
    int wid = gw->id;
    TreeNode node;
    node.id = g_tree_next_node_id[wid]++;
    node.parent_id = p ? (int)(intptr_t)p : 0;
    node.text = s ? s : "";
    node.expanded = 1;
    g_tree_node_index[wid][node.id] = (int)g_tree_nodes[wid].size();
    g_tree_nodes[wid].push_back(node);
    return (AuroraTreeItem)(intptr_t)node.id;
}
void aurora_gui_treeview_remove_item(AuroraWidget w, AuroraTreeItem i) {
    GuiWidget* gw = (GuiWidget*)w; if (!gw || !i) return;
    int wid = gw->id;
    int nid = (int)(intptr_t)i;
    auto& nodes = g_tree_nodes[wid];
    auto it = g_tree_node_index[wid].find(nid);
    if (it == g_tree_node_index[wid].end()) return;
    int idx = it->second;
    if (idx < 0 || idx >= (int)nodes.size()) return;
    nodes.erase(nodes.begin() + idx);
    /* Rebuild index for shifted nodes. */
    auto& nidx = g_tree_node_index[wid];
    nidx.clear();
    for (int k = 0; k < (int)nodes.size(); k++) nidx[nodes[k].id] = k;
    if (g_tree_selected[wid] == nid) g_tree_selected[wid] = 0;
}
void aurora_gui_treeview_clear(AuroraWidget w) {
    GuiWidget* gw = (GuiWidget*)w; if (!gw) return;
    int wid = gw->id;
    g_tree_nodes[wid].clear();
    g_tree_node_index[wid].clear();
    g_tree_next_node_id[wid] = 1;
    g_tree_selected[wid] = 0;
}
AuroraTreeItem aurora_gui_treeview_get_selected(AuroraWidget w) {
    GuiWidget* gw = (GuiWidget*)w; if (!gw) return nullptr;
    int sel = g_tree_selected[gw->id];
    if (sel == 0) return nullptr;
    return (AuroraTreeItem)(intptr_t)sel;
}
void aurora_gui_treeview_expand(AuroraWidget w, AuroraTreeItem i) {
    GuiWidget* gw = (GuiWidget*)w;
    TreeNode* node = gw ? tree_find_node(gw->id, (int)(intptr_t)i) : nullptr;
    if (node) node->expanded = 1;
}
void aurora_gui_treeview_collapse(AuroraWidget w, AuroraTreeItem i) {
    GuiWidget* gw = (GuiWidget*)w;
    TreeNode* node = gw ? tree_find_node(gw->id, (int)(intptr_t)i) : nullptr;
    if (node) node->expanded = 0;
}
void aurora_gui_treeview_set_item_text(AuroraWidget w, AuroraTreeItem i, const char* s) {
    GuiWidget* gw = (GuiWidget*)w;
    TreeNode* node = gw ? tree_find_node(gw->id, (int)(intptr_t)i) : nullptr;
    if (node) node->text = s ? s : "";
}

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
    GuiWidget* gw = (GuiWidget*)w;     if (gw && gw->paint_cb) gw->paint_cb(gw->paint_user, gw->x, gw->y, gw->w, gw->h);
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
        Atom clip = XInternAtom(g_display, "CLIPBOARD", False);
        XSetSelectionOwner(g_display, clip, g_root, CurrentTime);
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

/* ════════════════════════════════════════════════════════════
   Phase 36.2: WebView — WebKitGTK via dlopen (no link-time dep)
   Loads libwebkit2gtk-4.1.so at runtime. Falls back to stub
   if WebKitGTK is not installed.
   ════════════════════════════════════════════════════════════ */
#include <dlfcn.h>

/* WebKitGTK function pointers (loaded via dlopen) */
typedef void* (*PFN_webkit_web_view_new)(void);
typedef void  (*PFN_webkit_web_view_load_uri)(void* view, const char* uri);
typedef void  (*PFN_webkit_web_view_go_back)(void* view);
typedef void  (*PFN_webkit_web_view_go_forward)(void* view);
typedef void  (*PFN_webkit_web_view_reload)(void* view);
typedef const char* (*PFN_webkit_web_view_get_title)(void* view);
typedef const char* (*PFN_webkit_web_view_get_uri)(void* view);
typedef void  (*PFN_webkit_web_view_run_javascript)(void* view, const char* script, void* cancellable, void* callback, void* user_data);
typedef void* (*PFN_gtk_widget_get_window)(void* widget);
typedef unsigned long (*PFN_g_signal_connect_data)(void* instance, const char* signal, void* handler, void* data, void* destroy, int flags);

static void* g_webkit_lib = nullptr;
static int g_webkit_available = -1;

static PFN_webkit_web_view_new            p_web_view_new = nullptr;
static PFN_webkit_web_view_load_uri       p_load_uri = nullptr;
static PFN_webkit_web_view_go_back        p_go_back = nullptr;
static PFN_webkit_web_view_go_forward     p_go_forward = nullptr;
static PFN_webkit_web_view_reload         p_reload = nullptr;
static PFN_webkit_web_view_get_title      p_get_title = nullptr;
static PFN_webkit_web_view_get_uri        p_get_uri = nullptr;
static PFN_webkit_web_view_run_javascript p_run_js = nullptr;

static int webkit_init(void) {
    if (g_webkit_available >= 0) return g_webkit_available;

    /* Try WebKitGTK 4.1 first, then 4.0 */
    const char* libs[] = {
        "libwebkit2gtk-4.1.so.0",
        "libwebkit2gtk-4.0.so.37",
        "libwebkit2gtk-4.0.so",
        nullptr
    };
    for (int i = 0; libs[i]; i++) {
        g_webkit_lib = dlopen(libs[i], RTLD_NOW | RTLD_LOCAL);
        if (g_webkit_lib) break;
    }
    if (!g_webkit_lib) { g_webkit_available = 0; return 0; }

    p_web_view_new = (PFN_webkit_web_view_new)dlsym(g_webkit_lib, "webkit_web_view_new");
    p_load_uri     = (PFN_webkit_web_view_load_uri)dlsym(g_webkit_lib, "webkit_web_view_load_uri");
    p_go_back      = (PFN_webkit_web_view_go_back)dlsym(g_webkit_lib, "webkit_web_view_go_back");
    p_go_forward   = (PFN_webkit_web_view_go_forward)dlsym(g_webkit_lib, "webkit_web_view_go_forward");
    p_reload       = (PFN_webkit_web_view_reload)dlsym(g_webkit_lib, "webkit_web_view_reload");
    p_get_title    = (PFN_webkit_web_view_get_title)dlsym(g_webkit_lib, "webkit_web_view_get_title");
    p_get_uri      = (PFN_webkit_web_view_get_uri)dlsym(g_webkit_lib, "webkit_web_view_get_uri");
    p_run_js       = (PFN_webkit_web_view_run_javascript)dlsym(g_webkit_lib, "webkit_web_view_run_javascript");

    g_webkit_available = (p_web_view_new && p_load_uri) ? 1 : 0;
    return g_webkit_available;
}

/* Per-webview state for Linux */
struct LinuxWvState {
    void* web_view;       /* WebKitWebView* */
    AuroraEventCallback on_title;
    AuroraEventCallback on_navigate;
    char current_url[2048];
};
static std::map<int, LinuxWvState*> g_linux_wv;

AuroraWidget aurora_gui_webview_new(AuroraWidget p, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(24, (GuiWidget*)p);
    gw->x = x; gw->y = y; gw->w = w; gw->h = h;

    if (!webkit_init()) {
        /* WebKitGTK not available — create placeholder X11 window */
        if (g_display && gw->parent && gw->parent->xwindow) {
            gw->xwindow = XCreateSimpleWindow(g_display, gw->parent->xwindow,
                x, y, w, h, 1, 0x404040, 0x1a1a2e);
            XMapWindow(g_display, gw->xwindow);
        }
        return gw;
    }

    /* Create WebKitWebView */
    LinuxWvState* st = new LinuxWvState();
    memset(st, 0, sizeof(LinuxWvState));
    st->web_view = p_web_view_new();
    g_linux_wv[gw->id] = st;

    /* Store WebKitWebView pointer in extra_data for later use */
    gw->extra_data = st->web_view;

    /* Note: Full GTK embedding requires gtk_container_add into a GtkFixed/GtkOverlay.
       For X11-only apps, we create the WebView and position it via X11.
       The WebView renders into its own GdkWindow which we reparent. */
    if (g_display && gw->parent && gw->parent->xwindow) {
        gw->xwindow = XCreateSimpleWindow(g_display, gw->parent->xwindow,
            x, y, w, h, 0, 0, 0);
        XMapWindow(g_display, gw->xwindow);
    }

    return gw;
}

void aurora_gui_webview_navigate(AuroraWidget wv, const char* url) {
    GuiWidget* gw = (GuiWidget*)wv;
    if (!gw || !url) return;
    auto it = g_linux_wv.find(gw->id);
    if (it != g_linux_wv.end() && it->second->web_view && p_load_uri) {
        p_load_uri(it->second->web_view, url);
        strncpy(it->second->current_url, url, sizeof(it->second->current_url) - 1);
        if (it->second->on_navigate)
            it->second->on_navigate(gw->id, AURORA_EVENT_CLICK, 0, 0);
    }
}

void aurora_gui_webview_go_back(AuroraWidget wv) {
    GuiWidget* gw = (GuiWidget*)wv;
    if (!gw) return;
    auto it = g_linux_wv.find(gw->id);
    if (it != g_linux_wv.end() && it->second->web_view && p_go_back)
        p_go_back(it->second->web_view);
}

void aurora_gui_webview_go_forward(AuroraWidget wv) {
    GuiWidget* gw = (GuiWidget*)wv;
    if (!gw) return;
    auto it = g_linux_wv.find(gw->id);
    if (it != g_linux_wv.end() && it->second->web_view && p_go_forward)
        p_go_forward(it->second->web_view);
}

void aurora_gui_webview_reload(AuroraWidget wv) {
    GuiWidget* gw = (GuiWidget*)wv;
    if (!gw) return;
    auto it = g_linux_wv.find(gw->id);
    if (it != g_linux_wv.end() && it->second->web_view && p_reload)
        p_reload(it->second->web_view);
}

void aurora_gui_webview_set_on_title(AuroraWidget wv, AuroraEventCallback cb) {
    GuiWidget* gw = (GuiWidget*)wv;
    if (!gw) return;
    auto it = g_linux_wv.find(gw->id);
    if (it != g_linux_wv.end()) it->second->on_title = cb;
}

void aurora_gui_webview_set_on_navigate(AuroraWidget wv, AuroraEventCallback cb) {
    GuiWidget* gw = (GuiWidget*)wv;
    if (!gw) return;
    auto it = g_linux_wv.find(gw->id);
    if (it != g_linux_wv.end()) it->second->on_navigate = cb;
}

/* ════════════════════════════════════════════════════════════
   Phase 36.3: Media Player — GStreamer via dlopen
   Uses GStreamer playbin for audio/video playback on Linux.
   Falls back to stub if GStreamer not installed.
   ════════════════════════════════════════════════════════════ */

typedef void* (*PFN_gst_element_factory_make)(const char* factory, const char* name);
typedef int   (*PFN_gst_element_set_state)(void* element, int state);
typedef void  (*PFN_gst_object_unref)(void* obj);
typedef void  (*PFN_gst_init)(int* argc, char*** argv);
typedef void  (*PFN_g_object_set)(void* obj, const char* prop, ...);

static void* g_gst_lib = nullptr;
static int g_gst_available = -1;
static int g_gst_initialized = 0;

static PFN_gst_element_factory_make p_gst_factory = nullptr;
static PFN_gst_element_set_state    p_gst_set_state = nullptr;
static PFN_gst_object_unref         p_gst_unref = nullptr;
static PFN_gst_init                 p_gst_init = nullptr;
static PFN_g_object_set             p_g_object_set = nullptr;

/* GstState enum values */
#define GST_STATE_NULL    1
#define GST_STATE_READY   2
#define GST_STATE_PAUSED  3
#define GST_STATE_PLAYING 4

static int gst_init_check(void) {
    if (g_gst_available >= 0) return g_gst_available;

    g_gst_lib = dlopen("libgstreamer-1.0.so.0", RTLD_NOW | RTLD_LOCAL);
    if (!g_gst_lib) { g_gst_available = 0; return 0; }

    p_gst_factory   = (PFN_gst_element_factory_make)dlsym(g_gst_lib, "gst_element_factory_make");
    p_gst_set_state = (PFN_gst_element_set_state)dlsym(g_gst_lib, "gst_element_set_state");
    p_gst_unref     = (PFN_gst_object_unref)dlsym(g_gst_lib, "gst_object_unref");
    p_gst_init      = (PFN_gst_init)dlsym(g_gst_lib, "gst_init");

    /* g_object_set is in libgobject */
    void* gobj = dlopen("libgobject-2.0.so.0", RTLD_NOW | RTLD_LOCAL);
    if (gobj) p_g_object_set = (PFN_g_object_set)dlsym(gobj, "g_object_set");

    g_gst_available = (p_gst_factory && p_gst_set_state) ? 1 : 0;
    return g_gst_available;
}

struct LinuxMediaState {
    void* pipeline;  /* GstElement* (playbin) */
    int is_playing;
    int is_looping;
    float volume;
    char file_path[1024];
};
static std::map<int, LinuxMediaState*> g_linux_media;

AuroraWidget aurora_gui_media_new(AuroraWidget p, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(25, (GuiWidget*)p);
    gw->x = x; gw->y = y; gw->w = w; gw->h = h;

    /* Create X11 window for video output */
    if (g_display && gw->parent && gw->parent->xwindow) {
        gw->xwindow = XCreateSimpleWindow(g_display, gw->parent->xwindow,
            x, y, w, h, 0, 0, 0);
        XMapWindow(g_display, gw->xwindow);
    }

    LinuxMediaState* ms = new LinuxMediaState();
    memset(ms, 0, sizeof(LinuxMediaState));
    ms->volume = 1.0f;
    g_linux_media[gw->id] = ms;

    return gw;
}

void aurora_gui_media_open(AuroraWidget m, const char* src) {
    GuiWidget* gw = (GuiWidget*)m;
    if (!gw || !src) return;
    auto it = g_linux_media.find(gw->id);
    if (it == g_linux_media.end()) return;
    LinuxMediaState* ms = it->second;

    if (!gst_init_check()) return;

    /* Initialize GStreamer once */
    if (!g_gst_initialized && p_gst_init) {
        p_gst_init(nullptr, nullptr);
        g_gst_initialized = 1;
    }

    /* Stop and free previous pipeline */
    if (ms->pipeline) {
        p_gst_set_state(ms->pipeline, GST_STATE_NULL);
        if (p_gst_unref) p_gst_unref(ms->pipeline);
        ms->pipeline = nullptr;
        ms->is_playing = 0;
    }

    strncpy(ms->file_path, src, sizeof(ms->file_path) - 1);

    /* Create playbin pipeline */
    ms->pipeline = p_gst_factory("playbin", "aurora_media");
    if (!ms->pipeline) return;

    /* Set URI */
    if (p_g_object_set) {
        char uri[1200];
        if (src[0] == '/' || strstr(src, "://"))
            snprintf(uri, sizeof(uri), "%s", src);
        else
            snprintf(uri, sizeof(uri), "file://%s", src);
        p_g_object_set(ms->pipeline, "uri", uri, nullptr);
    }
}

void aurora_gui_media_play(AuroraWidget m) {
    GuiWidget* gw = (GuiWidget*)m;
    if (!gw) return;
    auto it = g_linux_media.find(gw->id);
    if (it == g_linux_media.end() || !it->second->pipeline) return;
    p_gst_set_state(it->second->pipeline, GST_STATE_PLAYING);
    it->second->is_playing = 1;
}

void aurora_gui_media_pause(AuroraWidget m) {
    GuiWidget* gw = (GuiWidget*)m;
    if (!gw) return;
    auto it = g_linux_media.find(gw->id);
    if (it == g_linux_media.end() || !it->second->pipeline) return;
    p_gst_set_state(it->second->pipeline, GST_STATE_PAUSED);
    it->second->is_playing = 0;
}

void aurora_gui_media_stop(AuroraWidget m) {
    GuiWidget* gw = (GuiWidget*)m;
    if (!gw) return;
    auto it = g_linux_media.find(gw->id);
    if (it == g_linux_media.end() || !it->second->pipeline) return;
    p_gst_set_state(it->second->pipeline, GST_STATE_NULL);
    it->second->is_playing = 0;
}

void aurora_gui_media_load(AuroraWidget m, const char* src) {
    aurora_gui_media_open(m, src);
}

void aurora_gui_media_set_volume(AuroraWidget m, float vol) {
    GuiWidget* gw = (GuiWidget*)m;
    if (!gw) return;
    auto it = g_linux_media.find(gw->id);
    if (it == g_linux_media.end()) return;
    it->second->volume = vol < 0.0f ? 0.0f : (vol > 1.0f ? 1.0f : vol);
    if (it->second->pipeline && p_g_object_set) {
        double v = (double)it->second->volume;
        p_g_object_set(it->second->pipeline, "volume", v, nullptr);
    }
}

void aurora_gui_media_set_looping(AuroraWidget m, int loop) {
    GuiWidget* gw = (GuiWidget*)m;
    if (!gw) return;
    auto it = g_linux_media.find(gw->id);
    if (it != g_linux_media.end()) it->second->is_looping = loop ? 1 : 0;
}

int aurora_gui_media_is_playing(AuroraWidget m) {
    GuiWidget* gw = (GuiWidget*)m;
    if (!gw) return 0;
    auto it = g_linux_media.find(gw->id);
    return (it != g_linux_media.end()) ? it->second->is_playing : 0;
}

/* ════════════════════════════════════════════════════════════
   Phase 36.4: Map — Leaflet.js via WebKitGTK
   Generates HTML with Leaflet and loads in WebView.
   ════════════════════════════════════════════════════════════ */

struct LinuxMapState {
    double center_lat, center_lon;
    int zoom;
    int webview_widget_id;
    char html_path[512];
};
static std::map<int, LinuxMapState*> g_linux_maps;

static void linux_map_generate_html(LinuxMapState* ms) {
    snprintf(ms->html_path, sizeof(ms->html_path),
        "/tmp/aurora_map_%d.html", (int)getpid());
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
        "  attribution:'&copy; OpenStreetMap contributors',maxZoom:19\n"
        "}).addTo(map);\n"
        "var markers=[];\n"
        "function addMarker(lat,lon,title){\n"
        "  var m=L.marker([lat,lon]).addTo(map);\n"
        "  if(title)m.bindPopup(title);\n"
        "  markers.push(m);\n"
        "}\n"
        "function setCenter(lat,lon,zoom){map.setView([lat,lon],zoom||map.getZoom());}\n"
        "function clearMarkers(){markers.forEach(function(m){map.removeLayer(m)});markers=[];}\n"
        "</script>\n"
        "</body></html>\n",
        ms->center_lat, ms->center_lon, ms->zoom);
    fclose(f);
}

static void linux_map_exec_js(LinuxMapState* ms, const char* js) {
    if (!ms || !js) return;
    auto it = g_linux_wv.find(ms->webview_widget_id);
    if (it != g_linux_wv.end() && it->second->web_view && p_run_js)
        p_run_js(it->second->web_view, js, nullptr, nullptr, nullptr);
}

AuroraWidget aurora_gui_map_new(AuroraWidget p, int x, int y, int w, int h) {
    GuiWidget* gw = widget_new(26, (GuiWidget*)p);
    gw->x = x; gw->y = y; gw->w = w; gw->h = h;

    LinuxMapState* ms = new LinuxMapState();
    memset(ms, 0, sizeof(LinuxMapState));
    ms->center_lat = 23.8103; /* Dhaka, Bangladesh */
    ms->center_lon = 90.4125;
    ms->zoom = 13;
    ms->webview_widget_id = gw->id;
    g_linux_maps[gw->id] = ms;

    /* Create internal WebView for map rendering */
    if (webkit_init()) {
        LinuxWvState* wv_st = new LinuxWvState();
        memset(wv_st, 0, sizeof(LinuxWvState));
        wv_st->web_view = p_web_view_new();
        g_linux_wv[gw->id] = wv_st;
        gw->extra_data = wv_st->web_view;

        /* Generate and load map HTML */
        linux_map_generate_html(ms);
        char uri[600];
        snprintf(uri, sizeof(uri), "file://%s", ms->html_path);
        if (p_load_uri) p_load_uri(wv_st->web_view, uri);
    }

    /* Create X11 window container */
    if (g_display && gw->parent && gw->parent->xwindow) {
        gw->xwindow = XCreateSimpleWindow(g_display, gw->parent->xwindow,
            x, y, w, h, 0, 0, 0);
        XMapWindow(g_display, gw->xwindow);
    }

    return gw;
}

void aurora_gui_map_set_center(AuroraWidget m, double lat, double lon) {
    GuiWidget* gw = (GuiWidget*)m;
    if (!gw) return;
    auto it = g_linux_maps.find(gw->id);
    if (it == g_linux_maps.end()) return;
    it->second->center_lat = lat; it->second->center_lon = lon;
    char js[256];
    snprintf(js, sizeof(js), "setCenter(%.6f,%.6f,%d);", lat, lon, it->second->zoom);
    linux_map_exec_js(it->second, js);
}

void aurora_gui_map_set_zoom(AuroraWidget m, int z) {
    GuiWidget* gw = (GuiWidget*)m;
    if (!gw) return;
    auto it = g_linux_maps.find(gw->id);
    if (it == g_linux_maps.end()) return;
    it->second->zoom = z;
    char js[256];
    snprintf(js, sizeof(js), "setCenter(%.6f,%.6f,%d);",
        it->second->center_lat, it->second->center_lon, z);
    linux_map_exec_js(it->second, js);
}

void aurora_gui_map_add_marker(AuroraWidget m, double lat, double lon, const char* label) {
    GuiWidget* gw = (GuiWidget*)m;
    if (!gw) return;
    auto it = g_linux_maps.find(gw->id);
    if (it == g_linux_maps.end()) return;
    char escaped[512] = {0};
    if (label) {
        int j = 0;
        for (int i = 0; label[i] && j < 500; i++) {
            if (label[i] == '\'') escaped[j++] = '\\';
            escaped[j++] = label[i];
        }
        escaped[j] = '\0';
    }
    char js[1024];
    snprintf(js, sizeof(js), "addMarker(%.6f,%.6f,'%s');", lat, lon, escaped);
    linux_map_exec_js(it->second, js);
}

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
int aurora_gui_window_set_dark_mode(AuroraWidget w, int e) { (void)w;(void)e; return 0; }
int aurora_gui_window_set_effect(AuroraWidget w, int e) { (void)w;(void)e; return 0; }
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
