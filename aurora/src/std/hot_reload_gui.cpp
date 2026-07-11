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
#include <algorithm>

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
static std::vector<WidgetSnapshot> g_previous_state;
static std::vector<WidgetSnapshot> g_current_state;

enum DiffOp { DIFF_NONE, DIFF_ADDED, DIFF_REMOVED, DIFF_CHANGED };
struct DiffEntry {
    DiffOp op;
    int id;
    int type;
    int old_x, old_y, old_w, old_h;
    int new_x, new_y, new_w, new_h;
    std::string old_text;
    std::string new_text;
};
static std::vector<DiffEntry> g_diff_result;

static void snapshot_all(std::vector<WidgetSnapshot>& out) {
    out.clear();
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
        out.push_back(s);
    }
}

static void compute_diff() {
    g_diff_result.clear();
    std::map<int, const WidgetSnapshot*> prev_map;
    std::map<int, const WidgetSnapshot*> curr_map;
    for (auto& s : g_previous_state) prev_map[s.id] = &s;
    for (auto& s : g_current_state) curr_map[s.id] = &s;

    for (auto& [id, prev] : prev_map) {
        auto cit = curr_map.find(id);
        if (cit == curr_map.end()) {
            DiffEntry d;
            d.op = DIFF_REMOVED;
            d.id = id;
            d.type = prev->type;
            d.old_x = prev->x; d.old_y = prev->y;
            d.old_w = prev->w; d.old_h = prev->h;
            d.old_text = prev->text;
            g_diff_result.push_back(d);
        } else {
            const WidgetSnapshot* cur = cit->second;
            if (prev->x != cur->x || prev->y != cur->y ||
                prev->w != cur->w || prev->h != cur->h ||
                prev->text != cur->text || prev->visible != cur->visible) {
                DiffEntry d;
                d.op = DIFF_CHANGED;
                d.id = id;
                d.type = prev->type;
                d.old_x = prev->x; d.old_y = prev->y;
                d.old_w = prev->w; d.old_h = prev->h;
                d.new_x = cur->x; d.new_y = cur->y;
                d.new_w = cur->w; d.new_h = cur->h;
                d.old_text = prev->text;
                d.new_text = cur->text;
                g_diff_result.push_back(d);
            }
        }
    }
    for (auto& [id, cur] : curr_map) {
        if (prev_map.find(id) == prev_map.end()) {
            DiffEntry d;
            d.op = DIFF_ADDED;
            d.id = id;
            d.type = cur->type;
            d.new_x = cur->x; d.new_y = cur->y;
            d.new_w = cur->w; d.new_h = cur->h;
            d.new_text = cur->text;
            g_diff_result.push_back(d);
        }
    }
}

static void apply_diff_to_widget(const DiffEntry& d) {
    void* w = (void*)(intptr_t)d.id;
    switch (d.op) {
        case DIFF_CHANGED:
            if (d.old_x != d.new_x || d.old_y != d.new_y ||
                d.old_w != d.new_w || d.old_h != d.new_h)
                aurora_gui_move(w, d.new_x, d.new_y, d.new_w, d.new_h);
            break;
        default:
            break;
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
    g_previous_state.clear();
    g_current_state.clear();
    g_diff_result.clear();
    return 1;
}

int aurora_hot_reload_gui_diff(void) {
    std::lock_guard<std::mutex> lock(g_hr_mtx);
    if (!g_hr_initialized) return 0;
    g_previous_state = g_current_state;
    snapshot_all(g_current_state);
    compute_diff();
    return (int)g_diff_result.size();
}

int aurora_hot_reload_gui_apply(void) {
    std::lock_guard<std::mutex> lock(g_hr_mtx);
    if (!g_hr_initialized) return 0;
    for (auto& d : g_diff_result)
        apply_diff_to_widget(d);
    return 1;
}

const char* aurora_hot_reload_gui_diff_tree(void) {
    std::lock_guard<std::mutex> lock(g_hr_mtx);
    static std::string result;
    result.clear();
    result += "=== Widget Tree ===\n";
    int n = aurora_gui_widget_count();
    for (int i = 0; i < n; i++) {
        void* w = aurora_gui_widget_get_by_index(i);
        if (!w) continue;
        char buf[256];
        int tx, ty, tw, th;
        aurora_gui_widget_get_bounds(w, &tx, &ty, &tw, &th);
        snprintf(buf, sizeof(buf), "  [%d] id=%d type=%d pos=%d,%d size=%dx%d text=%s\n",
                 i, aurora_gui_widget_get_id(w), aurora_gui_widget_get_type(w),
                 tx, ty, tw, th, aurora_gui_widget_get_text(w));
        result += buf;
    }
    result += "=== Diff (" + std::to_string(g_diff_result.size()) + " changes) ===\n";
    for (auto& d : g_diff_result) {
        char buf[256];
        const char* op_str = d.op == DIFF_ADDED ? "ADD" : (d.op == DIFF_REMOVED ? "REMOVE" : "CHANGE");
        snprintf(buf, sizeof(buf), "  %s id=%d type=%d\n", op_str, d.id, d.type);
        result += buf;
    }
    return result.c_str();
}

int aurora_hot_reload_gui_preserve_state(void) {
    std::lock_guard<std::mutex> lock(g_hr_mtx);
    if (!g_hr_initialized) return 0;
    snapshot_all(g_saved_state);
    return (int)g_saved_state.size();
}

int aurora_hot_reload_gui_restore_state(void) {
    std::lock_guard<std::mutex> lock(g_hr_mtx);
    if (!g_hr_initialized) return 0;
    std::map<int, const WidgetSnapshot*> saved_map;
    for (auto& s : g_saved_state) saved_map[s.id] = &s;
    int n = aurora_gui_widget_count();
    for (int i = 0; i < n; i++) {
        void* w = aurora_gui_widget_get_by_index(i);
        if (!w) continue;
        int id = aurora_gui_widget_get_id(w);
        auto it = saved_map.find(id);
        if (it == saved_map.end()) continue;
        const WidgetSnapshot& s = *it->second;
        int cx, cy, cw, ch;
        aurora_gui_widget_get_bounds(w, &cx, &cy, &cw, &ch);
        if (cx != s.x || cy != s.y || cw != s.w || ch != s.h)
            aurora_gui_move(w, s.x, s.y, s.w, s.h);
    }
    g_saved_state.clear();
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
