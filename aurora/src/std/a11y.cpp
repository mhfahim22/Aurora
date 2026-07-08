#include "std/a11y.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

static std::mutex g_a11y_mtx;

struct A11yInfo {
    std::string label;
    std::string role;
    std::string hint;
    int focusable;
    int tab_index;
};

extern "C" void aurora_gui_set_focus(void* widget);

static std::map<void*, A11yInfo> g_a11y_map;
static std::vector<void*> g_focus_order;
static int g_focus_index = -1;

int aurora_a11y_set_label(void* widget, const char* label) {
    if (!widget) return 0;
    std::lock_guard<std::mutex> lock(g_a11y_mtx);
    g_a11y_map[widget].label = label ? label : "";
    return 1;
}

const char* aurora_a11y_get_label(void* widget) {
    if (!widget) return nullptr;
    std::lock_guard<std::mutex> lock(g_a11y_mtx);
    auto it = g_a11y_map.find(widget);
    if (it == g_a11y_map.end()) return nullptr;
    static std::string result;
    result = it->second.label;
    return result.c_str();
}

int aurora_a11y_set_role(void* widget, const char* role) {
    if (!widget) return 0;
    std::lock_guard<std::mutex> lock(g_a11y_mtx);
    g_a11y_map[widget].role = role ? role : "";
    return 1;
}

const char* aurora_a11y_get_role(void* widget) {
    if (!widget) return nullptr;
    std::lock_guard<std::mutex> lock(g_a11y_mtx);
    auto it = g_a11y_map.find(widget);
    if (it == g_a11y_map.end()) return nullptr;
    static std::string result;
    result = it->second.role;
    return result.c_str();
}

int aurora_a11y_set_focusable(void* widget, int focusable) {
    if (!widget) return 0;
    std::lock_guard<std::mutex> lock(g_a11y_mtx);
    g_a11y_map[widget].focusable = focusable;
    if (focusable) {
        if (std::find(g_focus_order.begin(), g_focus_order.end(), widget) == g_focus_order.end())
            g_focus_order.push_back(widget);
    } else {
        g_focus_order.erase(std::remove(g_focus_order.begin(), g_focus_order.end(), widget), g_focus_order.end());
    }
    return 1;
}

int aurora_a11y_is_focusable(void* widget) {
    if (!widget) return 0;
    std::lock_guard<std::mutex> lock(g_a11y_mtx);
    auto it = g_a11y_map.find(widget);
    return (it != g_a11y_map.end() && it->second.focusable) ? 1 : 0;
}

int aurora_a11y_set_tab_index(void* widget, int index) {
    if (!widget) return 0;
    std::lock_guard<std::mutex> lock(g_a11y_mtx);
    g_a11y_map[widget].tab_index = index;
    return 1;
}

int aurora_a11y_get_tab_index(void* widget) {
    if (!widget) return -1;
    std::lock_guard<std::mutex> lock(g_a11y_mtx);
    auto it = g_a11y_map.find(widget);
    return (it != g_a11y_map.end()) ? it->second.tab_index : -1;
}

int aurora_a11y_focus_next(void) {
    std::lock_guard<std::mutex> lock(g_a11y_mtx);
    if (g_focus_order.empty()) return 0;
    g_focus_index = (g_focus_index + 1) % (int)g_focus_order.size();
    void* w = g_focus_order[g_focus_index];
    if (w) {
        aurora_gui_set_focus(w);
    }
    return 1;
}

int aurora_a11y_focus_prev(void) {
    std::lock_guard<std::mutex> lock(g_a11y_mtx);
    if (g_focus_order.empty()) return 0;
    g_focus_index = (g_focus_index - 1 + (int)g_focus_order.size()) % (int)g_focus_order.size();
    void* w = g_focus_order[g_focus_index];
    if (w) {
        aurora_gui_set_focus(w);
    }
    return 1;
}

void* aurora_a11y_get_focused(void) {
    std::lock_guard<std::mutex> lock(g_a11y_mtx);
    if (g_focus_order.empty() || g_focus_index < 0) return nullptr;
    return g_focus_order[g_focus_index];
}

int aurora_a11y_announce(const char* text) {
    if (!text) return 0;
    printf("[a11y] %s\n", text);
#ifdef _WIN32
    HWND hwnd = GetForegroundWindow();
    if (hwnd) {
        NotifyWinEvent(EVENT_OBJECT_NAMECHANGE, hwnd, OBJID_CLIENT, CHILDID_SELF);
    }
#endif
    return 1;
}

int aurora_a11y_set_hint(void* widget, const char* hint) {
    if (!widget) return 0;
    std::lock_guard<std::mutex> lock(g_a11y_mtx);
    g_a11y_map[widget].hint = hint ? hint : "";
    return 1;
}

struct ShortcutEntry {
    std::string shortcut;
    void (*fn)(void);
};
static std::vector<ShortcutEntry> g_shortcuts;

int aurora_a11y_register_shortcut(const char* shortcut, void (*fn)(void)) {
    if (!shortcut || !fn) return 0;
    std::lock_guard<std::mutex> lock(g_a11y_mtx);
    for (auto& s : g_shortcuts) {
        if (s.shortcut == shortcut) {
            s.fn = fn;
            return 1;
        }
    }
    g_shortcuts.push_back({shortcut, fn});
    return 1;
}

int aurora_a11y_unregister_shortcut(const char* shortcut) {
    if (!shortcut) return 0;
    std::lock_guard<std::mutex> lock(g_a11y_mtx);
    g_shortcuts.erase(std::remove_if(g_shortcuts.begin(), g_shortcuts.end(),
        [&](const ShortcutEntry& e) { return e.shortcut == shortcut; }), g_shortcuts.end());
    return 1;
}

int aurora_a11y_screen_reader_active(void) {
#ifdef _WIN32
    BOOL active = FALSE;
    SystemParametersInfoA(SPI_GETSCREENREADER, 0, &active, 0);
    return active ? 1 : 0;
#else
    const char* env = getenv("SCREEN_READER");
    return (env && env[0] == '1') ? 1 : 0;
#endif
}
