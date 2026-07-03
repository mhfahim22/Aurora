/* ════════════════════════════════════════════════════════════
   reactive.cpp — Reactive UI System
   Provides: State, Computed, Binding, Effect, ReactiveList,
             Lifecycle hooks
   ════════════════════════════════════════════════════════════ */

#include "../../include/std/reactive.hpp"
#include <cstdlib>
#include <cstring>
#include <vector>
#include <map>

/* ════════════════════════════════════════════════════════════
   Internal structures
   ════════════════════════════════════════════════════════════ */

struct Subscriber {
    int id;
    AuroraReactiveCallback cb;
    void* user_data;
};

struct AuroraReactiveState {
    void* value;
    std::vector<Subscriber> subs;
    bool is_computed;
    void* (*compute_fn)(void*);
    void* compute_user_data;
    std::vector<AuroraReactiveState*> deps;
    int next_sub_id;
};

struct AuroraReactiveList {
    std::vector<void*> items;
    std::vector<Subscriber> subs;
    int next_sub_id;
};

/* ── Binding entry ── */
enum BindProp {
    BIND_TEXT, BIND_ENABLED, BIND_VISIBLE,
    BIND_CHECKED, BIND_VALUE, BIND_LABEL
};

struct BindingEntry {
    int id;
    AuroraWidget widget;
    BindProp prop;
    AuroraReactiveState* state;
};

/* ── Lifecycle entry ── */
struct LifecycleEntry {
    int id;
    AuroraLifecycleFn fn;
    void* user_data;
};

/* ════════════════════════════════════════════════════════════
   Globals
   ════════════════════════════════════════════════════════════ */
static std::vector<AuroraReactiveState*> g_tracking_stack;
static std::vector<BindingEntry> g_bindings;
static std::vector<LifecycleEntry> g_mount_hooks;
static std::vector<LifecycleEntry> g_unmount_hooks;
static std::map<AuroraWidget, std::vector<int>> g_widget_bindings;
static std::map<AuroraWidget, std::vector<int>> g_widget_mounts;
static std::map<AuroraWidget, std::vector<int>> g_widget_unmounts;
static int g_next_binding_id = 1;
static int g_next_lifecycle_id = 1;
static int g_next_effect_id = 1;

/* ── Track dependency during computed/effect evaluation ── */
static void track_dep(AuroraReactiveState* state) {
    if (!g_tracking_stack.empty()) {
        AuroraReactiveState* tracker = g_tracking_stack.back();
        tracker->deps.push_back(state);
    }
}

/* ════════════════════════════════════════════════════════════
   State implementation
   ════════════════════════════════════════════════════════════ */
AuroraReactiveState* aurora_reactive_state_new(void* initial) {
    AuroraReactiveState* s = new AuroraReactiveState();
    s->value = initial;
    s->is_computed = false;
    s->compute_fn = nullptr;
    s->compute_user_data = nullptr;
    s->next_sub_id = 1;
    return s;
}

void aurora_reactive_state_set(AuroraReactiveState* state, void* value) {
    if (!state || state->is_computed) return;
    state->value = value;
    for (auto& sub : state->subs) {
        if (sub.cb) sub.cb(value, sub.user_data);
    }
}

void* aurora_reactive_state_get(AuroraReactiveState* state) {
    if (!state) return nullptr;
    track_dep(state);
    return state->value;
}

void aurora_reactive_state_delete(AuroraReactiveState* state) {
    if (!state) return;
    for (auto& sub : state->subs) {
        sub.cb = nullptr;
    }
    state->subs.clear();
    state->deps.clear();
    delete state;
}

/* ════════════════════════════════════════════════════════════
   Subscription
   ════════════════════════════════════════════════════════════ */
int aurora_reactive_subscribe(AuroraReactiveState* state, AuroraReactiveCallback cb, void* user_data) {
    if (!state || !cb) return 0;
    int id = state->next_sub_id++;
    Subscriber sub; sub.id = id; sub.cb = cb; sub.user_data = user_data;
    state->subs.push_back(sub);
    return id;
}

void aurora_reactive_unsubscribe(AuroraReactiveState* state, int id) {
    if (!state) return;
    for (size_t i = 0; i < state->subs.size(); i++) {
        if (state->subs[i].id == id) {
            state->subs.erase(state->subs.begin() + i);
            return;
        }
    }
}

void aurora_reactive_notify(AuroraReactiveState* state) {
    if (!state) return;
    for (auto& sub : state->subs) {
        if (sub.cb) sub.cb(state->value, sub.user_data);
    }
}

/* ════════════════════════════════════════════════════════════
   Computed
   ════════════════════════════════════════════════════════════ */
AuroraReactiveState* aurora_reactive_computed_new(AuroraComputeFn fn, void* user_data) {
    if (!fn) return nullptr;
    AuroraReactiveState* s = new AuroraReactiveState();
    s->is_computed = true;
    s->compute_fn = fn;
    s->compute_user_data = user_data;
    s->next_sub_id = 1;
    /* Evaluate with tracking */
    g_tracking_stack.push_back(s);
    s->value = fn(user_data);
    g_tracking_stack.pop_back();
    return s;
}

void* aurora_reactive_computed_get(AuroraReactiveState* computed) {
    if (!computed || !computed->is_computed) return nullptr;
    track_dep(computed);
    /* Re-evaluate if dependencies changed */
    computed->value = computed->compute_fn(computed->compute_user_data);
    return computed->value;
}

/* ════════════════════════════════════════════════════════════
   Effect
   ════════════════════════════════════════════════════════════ */
struct EffectEntry {
    int id;
    AuroraEffectFn fn;
    void* user_data;
    std::vector<AuroraReactiveState*> deps;
};

static std::vector<EffectEntry> g_effects;

int aurora_reactive_effect(AuroraEffectFn fn, void* user_data) {
    if (!fn) return 0;
    int id = g_next_effect_id++;
    EffectEntry e; e.id = id; e.fn = fn; e.user_data = user_data;
    g_tracking_stack.push_back(nullptr);
    fn(user_data);
    g_tracking_stack.pop_back();
    g_effects.push_back(e);
    return id;
}

void aurora_reactive_effect_clear(int id) {
    for (size_t i = 0; i < g_effects.size(); i++) {
        if (g_effects[i].id == id) {
            g_effects.erase(g_effects.begin() + i);
            return;
        }
    }
}

/* ════════════════════════════════════════════════════════════
   Binding — connects state to widget property
   ════════════════════════════════════════════════════════════ */

static void apply_binding(BindingEntry& b, void* value) {
    if (!b.widget || !value) return;
    switch (b.prop) {
        case BIND_TEXT: {
            const char* s = (const char*)value;
            if (s) { AuroraWidget w = b.widget;
                aurora_gui_label_set_text(w, s);
                aurora_gui_button_set_text(w, s);
                aurora_gui_checkbox_set_text(w, s);
                aurora_gui_textbox_set_text(w, s);
            }
            break;
        }
        case BIND_ENABLED: {
            int v = (int)(uintptr_t)value;
            aurora_gui_set_enabled(b.widget, v);
            break;
        }
        case BIND_VISIBLE: {
            int v = (int)(uintptr_t)value;
            aurora_gui_set_visible(b.widget, v);
            break;
        }
        case BIND_CHECKED: {
            int v = (int)(uintptr_t)value;
            aurora_gui_checkbox_set_checked(b.widget, v);
            aurora_gui_radiobutton_set_checked(b.widget, v);
            break;
        }
        case BIND_VALUE: {
            /* Slider/ProgressBar value */
            int v = (int)(uintptr_t)value;
            aurora_gui_slider_set_value(b.widget, v);
            aurora_gui_progressbar_set_value(b.widget, v);
            break;
        }
        case BIND_LABEL: {
            const char* s = (const char*)value;
            if (s) aurora_gui_label_set_text(b.widget, s);
            break;
        }
    }
}

static void binding_callback(void* value, void* user_data) {
    int bid = (int)(uintptr_t)user_data;
    for (size_t i = 0; i < g_bindings.size(); i++) {
        if (g_bindings[i].id == bid) {
            apply_binding(g_bindings[i], value);
            return;
        }
    }
}

static int do_bind(AuroraWidget widget, AuroraReactiveState* state, BindProp prop) {
    if (!widget || !state) return 0;
    int id = g_next_binding_id++;
    BindingEntry b; b.id = id; b.widget = widget; b.prop = prop; b.state = state;
    g_bindings.push_back(b);
    g_widget_bindings[widget].push_back(id);
    /* Use binding ID as lookup key instead of pointer */
    for (size_t i = 0; i < g_bindings.size(); i++) {
        if (g_bindings[i].id == id) {
            aurora_reactive_subscribe(state, binding_callback, (void*)(uintptr_t)id);
            apply_binding(g_bindings[i], state->value);
            break;
        }
    }
    return id;
}

int aurora_reactive_bind_text(AuroraWidget widget, AuroraReactiveState* state) { return do_bind(widget, state, BIND_TEXT); }
int aurora_reactive_bind_enabled(AuroraWidget widget, AuroraReactiveState* state) { return do_bind(widget, state, BIND_ENABLED); }
int aurora_reactive_bind_visible(AuroraWidget widget, AuroraReactiveState* state) { return do_bind(widget, state, BIND_VISIBLE); }
int aurora_reactive_bind_checked(AuroraWidget widget, AuroraReactiveState* state) { return do_bind(widget, state, BIND_CHECKED); }
int aurora_reactive_bind_value(AuroraWidget widget, AuroraReactiveState* state) { return do_bind(widget, state, BIND_VALUE); }
int aurora_reactive_bind_label(AuroraWidget widget, AuroraReactiveState* state) { return do_bind(widget, state, BIND_LABEL); }

void aurora_reactive_unbind(AuroraWidget widget, int id) {
    for (size_t i = 0; i < g_bindings.size(); i++) {
        if (g_bindings[i].widget == widget && g_bindings[i].id == id) {
            aurora_reactive_unsubscribe(g_bindings[i].state, g_bindings[i].id);
            g_bindings.erase(g_bindings.begin() + i);
            break;
        }
    }
    auto it = g_widget_bindings.find(widget);
    if (it != g_widget_bindings.end()) {
        auto& ids = it->second;
        for (size_t i = 0; i < ids.size(); i++) {
            if (ids[i] == id) { ids.erase(ids.begin() + i); break; }
        }
    }
}

void aurora_reactive_unbind_all(AuroraWidget widget) {
    auto it = g_widget_bindings.find(widget);
    if (it == g_widget_bindings.end()) return;
    for (int bid : it->second) {
        for (size_t i = 0; i < g_bindings.size(); i++) {
            if (g_bindings[i].id == bid) {
                aurora_reactive_unsubscribe(g_bindings[i].state, g_bindings[i].id);
                g_bindings.erase(g_bindings.begin() + i);
                break;
            }
        }
    }
    g_widget_bindings.erase(it);
}

/* ════════════════════════════════════════════════════════════
   Reactive List
   ════════════════════════════════════════════════════════════ */
AuroraReactiveList* aurora_reactive_list_new(void) {
    AuroraReactiveList* l = new AuroraReactiveList();
    l->next_sub_id = 1;
    return l;
}

void aurora_reactive_list_add(AuroraReactiveList* list, void* item) {
    if (!list) return;
    list->items.push_back(item);
    for (auto& sub : list->subs) {
        if (sub.cb) sub.cb(item, sub.user_data);
    }
}

void aurora_reactive_list_insert(AuroraReactiveList* list, int index, void* item) {
    if (!list) return;
    if (index < 0) index = 0;
    if ((size_t)index >= list->items.size()) list->items.push_back(item);
    else list->items.insert(list->items.begin() + index, item);
    for (auto& sub : list->subs) {
        if (sub.cb) sub.cb(item, sub.user_data);
    }
}

void* aurora_reactive_list_get(AuroraReactiveList* list, int index) {
    if (!list || index < 0 || (size_t)index >= list->items.size()) return nullptr;
    return list->items[index];
}

void aurora_reactive_list_set(AuroraReactiveList* list, int index, void* item) {
    if (!list || index < 0 || (size_t)index >= list->items.size()) return;
    list->items[index] = item;
    for (auto& sub : list->subs) {
        if (sub.cb) sub.cb(item, sub.user_data);
    }
}

void aurora_reactive_list_remove(AuroraReactiveList* list, int index) {
    if (!list || index < 0 || (size_t)index >= list->items.size()) return;
    list->items.erase(list->items.begin() + index);
    for (auto& sub : list->subs) {
        if (sub.cb) sub.cb(nullptr, sub.user_data);
    }
}

int aurora_reactive_list_size(AuroraReactiveList* list) {
    return list ? (int)list->items.size() : 0;
}

void aurora_reactive_list_clear(AuroraReactiveList* list) {
    if (!list) return;
    list->items.clear();
    for (auto& sub : list->subs) {
        if (sub.cb) sub.cb(nullptr, sub.user_data);
    }
}

void aurora_reactive_list_delete(AuroraReactiveList* list) {
    if (!list) return;
    list->subs.clear();
    list->items.clear();
    delete list;
}

int aurora_reactive_list_subscribe(AuroraReactiveList* list, AuroraReactiveCallback cb, void* user_data) {
    if (!list || !cb) return 0;
    int id = list->next_sub_id++;
    Subscriber sub; sub.id = id; sub.cb = cb; sub.user_data = user_data;
    list->subs.push_back(sub);
    return id;
}

void aurora_reactive_list_unsubscribe(AuroraReactiveList* list, int id) {
    if (!list) return;
    for (size_t i = 0; i < list->subs.size(); i++) {
        if (list->subs[i].id == id) {
            list->subs.erase(list->subs.begin() + i);
            return;
        }
    }
}

/* ════════════════════════════════════════════════════════════
   Lifecycle hooks
   ════════════════════════════════════════════════════════════ */
int aurora_reactive_on_change(AuroraReactiveState* state, AuroraReactiveCallback cb, void* user_data) {
    return aurora_reactive_subscribe(state, cb, user_data);
}

void aurora_reactive_on_mount(AuroraWidget widget, AuroraLifecycleFn fn, void* user_data) {
    if (!widget || !fn) return;
    LifecycleEntry e; e.id = g_next_lifecycle_id++; e.fn = fn; e.user_data = user_data;
    g_mount_hooks.push_back(e);
    g_widget_mounts[widget].push_back(e.id);
}

void aurora_reactive_on_unmount(AuroraWidget widget, AuroraLifecycleFn fn, void* user_data) {
    if (!widget || !fn) return;
    LifecycleEntry e; e.id = g_next_lifecycle_id++; e.fn = fn; e.user_data = user_data;
    g_unmount_hooks.push_back(e);
    g_widget_unmounts[widget].push_back(e.id);
}
