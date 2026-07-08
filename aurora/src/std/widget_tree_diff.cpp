#include "std/widget_tree_diff.hpp"
#include "std/gui.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <mutex>

static std::mutex g_diff_mtx;
static int g_diff_initialized = 0;

struct WidgetDiffState {
    int id;
    int type;
    int x, y, w, h;
    std::string text;
    int changed;
};

static std::vector<WidgetDiffState> g_old_state;
static std::vector<WidgetDiffState> g_new_state;
static std::map<int, int> g_change_map; /* widget_id -> changed flag */

int aurora_widget_diff_init(void) {
    if (g_diff_initialized) return 0;
    g_diff_initialized = 1;
    return 1;
}

static void capture_state(std::vector<WidgetDiffState>& out) {
    out.clear();
    int n = aurora_gui_widget_count();
    for (int i = 0; i < n; i++) {
        void* w = aurora_gui_widget_get_by_index(i);
        if (!w) continue;
        WidgetDiffState s = {};
        s.id = aurora_gui_widget_get_id(w);
        s.type = aurora_gui_widget_get_type(w);
        aurora_gui_widget_get_bounds(w, &s.x, &s.y, &s.w, &s.h);
        const char* txt = aurora_gui_widget_get_text(w);
        s.text = txt ? txt : "";
        s.changed = 0;
        out.push_back(s);
    }
}

int aurora_widget_diff_snapshot(void) {
    std::lock_guard<std::mutex> lock(g_diff_mtx);
    if (!g_diff_initialized) return 0;
    capture_state(g_old_state);
    return 1;
}

int aurora_widget_diff_compute(void) {
    std::lock_guard<std::mutex> lock(g_diff_mtx);
    if (!g_diff_initialized) return 0;
    capture_state(g_new_state);
    g_change_map.clear();

    /* Build old state map by id */
    std::map<int, const WidgetDiffState*> old_by_id;
    for (auto& s : g_old_state)
        old_by_id[s.id] = &s;

    int changed_count = 0;
    for (auto& ns : g_new_state) {
        auto it = old_by_id.find(ns.id);
        if (it == old_by_id.end()) {
            /* New widget */
            ns.changed = 1;
            g_change_map[ns.id] = 1;
            changed_count++;
        } else {
            const WidgetDiffState* os = it->second;
            if (os->x != ns.x || os->y != ns.y || os->w != ns.w || os->h != ns.h ||
                os->text != ns.text || os->type != ns.type) {
                ns.changed = 1;
                g_change_map[ns.id] = 1;
                changed_count++;
            } else {
                ns.changed = 0;
                g_change_map[ns.id] = 0;
            }
        }
    }

    /* Widgets removed from old state */
    for (auto& os : g_old_state) {
        bool found = false;
        for (auto& ns : g_new_state) {
            if (ns.id == os.id) { found = true; break; }
        }
        if (!found) {
            changed_count++;
        }
    }

    return changed_count;
}

int aurora_widget_diff_get_change_count(void) {
    std::lock_guard<std::mutex> lock(g_diff_mtx);
    if (!g_diff_initialized) return 0;
    int cnt = 0;
    for (auto& kv : g_change_map)
        if (kv.second) cnt++;
    return cnt;
}

int aurora_widget_diff_has_changed(int widget_id) {
    std::lock_guard<std::mutex> lock(g_diff_mtx);
    auto it = g_change_map.find(widget_id);
    if (it == g_change_map.end()) return 0;
    return it->second;
}

void aurora_widget_diff_apply(void) {
    std::lock_guard<std::mutex> lock(g_diff_mtx);
    if (!g_diff_initialized) return;
    /* Apply old state to new widgets (preserve positions if unchanged) */
    for (auto& ns : g_new_state) {
        if (!ns.changed) continue;
        /* Redraw widget at new position */
        void* w = nullptr;
        int n = aurora_gui_widget_count();
        for (int i = 0; i < n; i++) {
            void* wi = aurora_gui_widget_get_by_index(i);
            if (wi && aurora_gui_widget_get_id(wi) == ns.id) {
                w = wi; break;
            }
        }
        if (w) {
            /* Only reposition if bounds changed */
            int ox, oy, ow, oh;
            aurora_gui_widget_get_bounds(w, &ox, &oy, &ow, &oh);
        }
    }
    g_old_state = g_new_state;
    g_change_map.clear();
}

void aurora_widget_diff_clear(void) {
    std::lock_guard<std::mutex> lock(g_diff_mtx);
    g_old_state.clear();
    g_new_state.clear();
    g_change_map.clear();
}
