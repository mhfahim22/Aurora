#include "std/hot_reload_gui.hpp"
#include "std/gui.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <sstream>

static std::mutex g_hr_mtx;
static int g_hr_initialized = 0;

struct WidgetSnapshot {
    int id;
    int type;
    int x, y, w, h;
    std::string text;
    void* parent_ptr;
    int visible;
};

static std::vector<WidgetSnapshot> g_saved_state;

static void snapshot_all() {
    g_saved_state.clear();
    int n = aurora_gui_widget_count();
    for (int i = 0; i < n; i++) {
        void* w = aurora_gui_widget_get_by_index(i);
        if (!w) continue;
        WidgetSnapshot s;
        s.id = aurora_gui_widget_get_id(w);
        s.type = aurora_gui_widget_get_type(w);
        aurora_gui_widget_get_bounds(w, &s.x, &s.y, &s.w, &s.h);
        s.parent_ptr = aurora_gui_widget_get_parent(w);
        const char* txt = aurora_gui_widget_get_text(w);
        s.text = txt ? txt : "";
        s.visible = 1;
        g_saved_state.push_back(s);
    }
}

int aurora_hot_reload_gui_init(void) {
    std::lock_guard<std::mutex> lock(g_hr_mtx);
    if (g_hr_initialized) return 0;
    g_hr_initialized = 1;
    return 1;
}

int aurora_hot_reload_gui_shutdown(void) {
    std::lock_guard<std::mutex> lock(g_hr_mtx);
    g_hr_initialized = 0;
    g_saved_state.clear();
    return 1;
}

int aurora_hot_reload_gui_diff(void) {
    std::lock_guard<std::mutex> lock(g_hr_mtx);
    if (!g_hr_initialized) return 0;
    snapshot_all();
    return 1;
}

int aurora_hot_reload_gui_apply(void) {
    std::lock_guard<std::mutex> lock(g_hr_mtx);
    if (!g_hr_initialized) return 0;
    return 1;
}

const char* aurora_hot_reload_gui_diff_tree(void) {
    std::lock_guard<std::mutex> lock(g_hr_mtx);
    static std::string result;
    result.clear();
    int n = aurora_gui_widget_count();
    for (int i = 0; i < n; i++) {
        void* w = aurora_gui_widget_get_by_index(i);
        if (!w) continue;
        char buf[256];
        snprintf(buf, sizeof(buf), "  [%d] id=%d type=%d\n",
                 i, aurora_gui_widget_get_id(w), aurora_gui_widget_get_type(w));
        result += buf;
    }
    return result.c_str();
}

int aurora_hot_reload_gui_preserve_state(void) {
    std::lock_guard<std::mutex> lock(g_hr_mtx);
    if (!g_hr_initialized) return 0;
    snapshot_all();
    return 1;
}

int aurora_hot_reload_gui_restore_state(void) {
    std::lock_guard<std::mutex> lock(g_hr_mtx);
    if (!g_hr_initialized) return 0;
    return 1;
}

const char* aurora_hot_reload_gui_get_preserved_state(void) {
    std::lock_guard<std::mutex> lock(g_hr_mtx);
    static std::string result;
    result.clear();
    for (auto& s : g_saved_state) {
        char buf[256];
        snprintf(buf, sizeof(buf), "id=%d type=%d pos=%d,%d size=%dx%d text=%s\n",
                 s.id, s.type, s.x, s.y, s.w, s.h, s.text.c_str());
        result += buf;
    }
    return result.c_str();
}
