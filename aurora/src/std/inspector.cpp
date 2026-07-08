#include "std/inspector.hpp"
#include "std/gui.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#endif

static std::mutex g_ins_mtx;
static int g_ins_enabled = 0;
static void* g_ins_selected = nullptr;
static void* g_ins_highlighted = nullptr;

/* Introspection functions are declared in gui.hpp (included above) */

int aurora_inspector_init(void) {
    std::lock_guard<std::mutex> lock(g_ins_mtx);
    g_ins_enabled = 1;
    g_ins_selected = nullptr;
    g_ins_highlighted = nullptr;
    return 1;
}

int aurora_inspector_shutdown(void) {
    std::lock_guard<std::mutex> lock(g_ins_mtx);
    g_ins_enabled = 0;
    g_ins_selected = nullptr;
    g_ins_highlighted = nullptr;
    return 1;
}

void aurora_inspector_set_enabled(int enabled) {
    std::lock_guard<std::mutex> lock(g_ins_mtx);
    g_ins_enabled = enabled;
    if (!enabled) { g_ins_selected = nullptr; g_ins_highlighted = nullptr; }
}

int aurora_inspector_is_enabled(void) {
    return g_ins_enabled;
}

void aurora_inspector_highlight_widget(void* widget) {
    std::lock_guard<std::mutex> lock(g_ins_mtx);
    g_ins_highlighted = widget;
}

void aurora_inspector_select_widget(void* widget) {
    std::lock_guard<std::mutex> lock(g_ins_mtx);
    g_ins_selected = widget;
    g_ins_highlighted = widget;
}

void* aurora_inspector_get_selected(void) {
    return g_ins_selected;
}

void aurora_inspector_render_overlay(void) {
    if (!g_ins_enabled) return;
    std::lock_guard<std::mutex> lock(g_ins_mtx);

#ifdef _WIN32
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return;
    HDC hdc = GetDC(hwnd);
    if (!hdc) return;

    HPEN highlight_pen = CreatePen(PS_SOLID, 3, RGB(0, 120, 255));
    HPEN select_pen = CreatePen(PS_SOLID, 3, RGB(255, 0, 0));
    HGDIOBJ old_pen = SelectObject(hdc, highlight_pen);
    HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

    /* Draw debug overlay for all widgets */
    SetBkMode(hdc, TRANSPARENT);
    int n = aurora_gui_widget_count();
    for (int i = 0; i < n; i++) {
        void* w = aurora_gui_widget_get_by_index(i);
        if (!w) continue;
        int x = 0, y = 0, ww = 0, hh = 0;
        aurora_gui_widget_get_bounds(w, &x, &y, &ww, &hh);
        if (ww <= 0 || hh <= 0) continue;

        if (w == g_ins_selected) {
            SelectObject(hdc, select_pen);
            Rectangle(hdc, x-1, y-1, x+ww+1, y+hh+1);
        } else if (w == g_ins_highlighted) {
            SelectObject(hdc, highlight_pen);
            Rectangle(hdc, x-1, y-1, x+ww+1, y+hh+1);
        }

        /* Draw type label */
        char label[64];
        int tid = aurora_gui_widget_get_type(w);
        snprintf(label, sizeof(label), "#%d t=%d", aurora_gui_widget_get_id(w), tid);
        if (w == g_ins_selected || w == g_ins_highlighted) {
            SetTextColor(hdc, (w == g_ins_selected) ? RGB(255,0,0) : RGB(0,120,255));
            TextOutA(hdc, x+2, y+2, label, (int)strlen(label));
        }
    }

    /* Draw selected widget info panel (top-right) */
    if (g_ins_selected) {
        int sx, sy, sw, sh;
        aurora_gui_widget_get_bounds(g_ins_selected, &sx, &sy, &sw, &sh);
        int tid = aurora_gui_widget_get_type(g_ins_selected);
        const char* txt = aurora_gui_widget_get_text(g_ins_selected);
        char info[256];
        snprintf(info, sizeof(info),
                 "Selected: #%d type=%d\n"
                 "Bounds: %d,%d %dx%d\n"
                 "Text: %s\n",
                 aurora_gui_widget_get_id(g_ins_selected), tid,
                 sx, sy, sw, sh,
                 txt ? txt : "(none)");

        RECT rect = { 10, 10, 310, 150 };
        HBRUSH bg = CreateSolidBrush(RGB(255,255,230));
        FillRect(hdc, &rect, bg);
        DeleteObject(bg);
        FrameRect(hdc, &rect, (HBRUSH)GetStockObject(GRAY_BRUSH));
        DrawTextA(hdc, info, -1, &rect, DT_LEFT | DT_WORDBREAK);
    }

    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(highlight_pen);
    DeleteObject(select_pen);
    ReleaseDC(hwnd, hdc);
#endif
}

const char* aurora_inspector_get_property_str(void* widget, const char* key) {
    if (!widget || !key) return nullptr;
    static std::string result;
    result.clear();
    if (strcmp(key, "text") == 0) {
        const char* txt = aurora_gui_widget_get_text(widget);
        result = txt ? txt : "";
    } else if (strcmp(key, "type") == 0) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", aurora_gui_widget_get_type(widget));
        result = buf;
    } else if (strcmp(key, "id") == 0) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", aurora_gui_widget_get_id(widget));
        result = buf;
    }
    return result.empty() ? nullptr : result.c_str();
}

int aurora_inspector_get_property_int(void* widget, const char* key) {
    if (!widget || !key) return 0;
    if (strcmp(key, "type") == 0) return aurora_gui_widget_get_type(widget);
    if (strcmp(key, "id") == 0) return aurora_gui_widget_get_id(widget);
    return 0;
}

void aurora_inspector_set_property_str(void* widget, const char* key, const char* value) {
    if (!widget || !key || !value) return;
    (void)widget; (void)key; (void)value;
}

void aurora_inspector_set_property_int(void* widget, const char* key, int value) {
    (void)widget; (void)key; (void)value;
}
