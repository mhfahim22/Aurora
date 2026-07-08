#include "std/widget_plugin.hpp"
#include "std/plugin.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>

/* ── Constants ── */
#define MAX_WIDGET_PLUGINS 32
#define MAX_WIDGET_TYPES   16

/* ── Callback typedefs ── */
typedef void* (*WidgetCreateFn)();
typedef int   (*WidgetDestroyFn)(void*);
typedef int   (*WidgetRenderFn)(void*, void*);
typedef int   (*WidgetEventFn)(void*, void*);

/* ── Widget type descriptor ── */
struct WidgetTypeEntry {
    char            type_name[64];
    WidgetCreateFn  create_fn;
    WidgetDestroyFn destroy_fn;
    WidgetRenderFn  render_fn;
    WidgetEventFn   event_fn;
};

/* ── Widget plugin descriptor ── */
struct WidgetPluginEntry {
    char                name[64];
    char                version[32];
    int                 loaded;
    WidgetTypeEntry     types[MAX_WIDGET_TYPES];
    int                 type_count;
};

static WidgetPluginEntry g_widget_plugins[MAX_WIDGET_PLUGINS];
static int g_widget_plugin_count = 0;

static int find_widget_plugin(const char* name) {
    if (!name) return -1;
    for (int i = 0; i < g_widget_plugin_count; i++)
        if (strcmp(g_widget_plugins[i].name, name) == 0)
            return i;
    return -1;
}

static int find_widget_type(const WidgetPluginEntry* plugin, const char* type_name) {
    if (!plugin || !type_name) return -1;
    for (int i = 0; i < plugin->type_count; i++)
        if (strcmp(plugin->types[i].type_name, type_name) == 0)
            return i;
    return -1;
}

/* ── Register a widget type from a loaded native plugin ── */

int aurora_widget_plugin_register(const char* plugin_name, const char* version,
                                   const char* widget_type,
                                   void* create_fn, void* destroy_fn,
                                   void* render_fn, void* event_fn) {
    if (!plugin_name || !widget_type || !create_fn) return 0;

    int idx = find_widget_plugin(plugin_name);
    if (idx < 0) {
        if (g_widget_plugin_count >= MAX_WIDGET_PLUGINS) return 0;
        idx = g_widget_plugin_count++;
        strncpy(g_widget_plugins[idx].name, plugin_name, sizeof(g_widget_plugins[idx].name) - 1);
        g_widget_plugins[idx].name[sizeof(g_widget_plugins[idx].name) - 1] = '\0';
        g_widget_plugins[idx].version[0] = '\0';
        g_widget_plugins[idx].loaded = 1;
        g_widget_plugins[idx].type_count = 0;
    }

    if (version) {
        strncpy(g_widget_plugins[idx].version, version, sizeof(g_widget_plugins[idx].version) - 1);
        g_widget_plugins[idx].version[sizeof(g_widget_plugins[idx].version) - 1] = '\0';
    }

    if (g_widget_plugins[idx].type_count >= MAX_WIDGET_TYPES) return 0;
    int ti = g_widget_plugins[idx].type_count++;
    WidgetTypeEntry* t = &g_widget_plugins[idx].types[ti];
    strncpy(t->type_name, widget_type, sizeof(t->type_name) - 1);
    t->type_name[sizeof(t->type_name) - 1] = '\0';
    t->create_fn  = (WidgetCreateFn)create_fn;
    t->destroy_fn = (WidgetDestroyFn)destroy_fn;
    t->render_fn  = (WidgetRenderFn)render_fn;
    t->event_fn   = (WidgetEventFn)event_fn;
    return 1;
}

/* ── Load a native plugin and auto-register widget types ── */

int aurora_widget_plugin_load(const char* path) {
    if (!path || !*path) return 0;

    /* Load via Phase 20 native plugin system */
    if (!aurora_plugin_load(path)) return 0;

    /* Get the plugin name from the Phase 20 system */
    int count = aurora_plugin_get_count();
    if (count <= 0) return 0;
    const char* name = aurora_plugin_get_name(count - 1);
    if (!name) return 0;

    /* Check if it exposes widget plugin symbols */
    typedef int (*RegFn)(int (*)(const char*, const char*, const char*,
                                  void*, void*, void*, void*));
    RegFn reg_fn = (RegFn)aurora_plugin_get_function(name, "aurora_widget_plugin_register_types");
    if (reg_fn) {
        reg_fn(aurora_widget_plugin_register);
    }

    /* Also try to register via explicit symbol lookup */
    typedef const char* (*WTypeFn)(int);
    WTypeFn type_name_fn = (WTypeFn)aurora_plugin_get_function(name, "aurora_widget_plugin_type_name");
    WidgetCreateFn create_fn = (WidgetCreateFn)aurora_plugin_get_function(name, "aurora_widget_create");
    WidgetDestroyFn destroy_fn = (WidgetDestroyFn)aurora_plugin_get_function(name, "aurora_widget_destroy");
    WidgetRenderFn render_fn = (WidgetRenderFn)aurora_plugin_get_function(name, "aurora_widget_render");
    WidgetEventFn event_fn = (WidgetEventFn)aurora_plugin_get_function(name, "aurora_widget_event");

    /* Get plugin version */
    const char* ver = (const char*)aurora_plugin_get_function(name, "aurora_plugin_version");
    const char* version = ver ? ver : "1.0.0";

    /* Register each widget type the plugin provides */
    if (type_name_fn && create_fn) {
        int ti = 0;
        while (true) {
            const char* tname = type_name_fn(ti);
            if (!tname || !*tname) break;
            aurora_widget_plugin_register(name, version, tname,
                                           (void*)create_fn,
                                           (void*)destroy_fn,
                                           (void*)render_fn,
                                           (void*)event_fn);
            ti++;
        }
    }

    /* If no types were registered via symbols, try default type name */
    int idx = find_widget_plugin(name);
    if (idx < 0 || g_widget_plugins[idx].type_count == 0) {
        /* Register a default widget type for this plugin */
        aurora_widget_plugin_register(name, version, "default",
                                       (void*)create_fn ? (void*)create_fn : nullptr,
                                       (void*)destroy_fn,
                                       (void*)render_fn,
                                       (void*)event_fn);
    }

    return 1;
}

/* ── Widget lifecycle ── */

void* aurora_widget_plugin_create(const char* plugin_name, const char* widget_type) {
    if (!plugin_name || !widget_type) return nullptr;
    int idx = find_widget_plugin(plugin_name);
    if (idx < 0) return nullptr;

    int ti = find_widget_type(&g_widget_plugins[idx], widget_type);
    if (ti < 0) return nullptr;

    if (!g_widget_plugins[idx].types[ti].create_fn) return nullptr;
    return g_widget_plugins[idx].types[ti].create_fn();
}

int aurora_widget_plugin_destroy(void* widget) {
    if (!widget) return 0;
    /* Walk all registered plugins to find a destroy function */
    for (int i = 0; i < g_widget_plugin_count; i++) {
        for (int j = 0; j < g_widget_plugins[i].type_count; j++) {
            if (g_widget_plugins[i].types[j].destroy_fn) {
                if (g_widget_plugins[i].types[j].destroy_fn(widget))
                    return 1;
            }
        }
    }
    return 0;
}

int aurora_widget_plugin_render(void* widget, void* ctx) {
    if (!widget) return 0;
    for (int i = 0; i < g_widget_plugin_count; i++) {
        for (int j = 0; j < g_widget_plugins[i].type_count; j++) {
            if (g_widget_plugins[i].types[j].render_fn) {
                if (g_widget_plugins[i].types[j].render_fn(widget, ctx))
                    return 1;
            }
        }
    }
    return 0;
}

int aurora_widget_plugin_handle_event(void* widget, void* event) {
    if (!widget) return 0;
    for (int i = 0; i < g_widget_plugin_count; i++) {
        for (int j = 0; j < g_widget_plugins[i].type_count; j++) {
            if (g_widget_plugins[i].types[j].event_fn) {
                if (g_widget_plugins[i].types[j].event_fn(widget, event))
                    return 1;
            }
        }
    }
    return 0;
}

/* ── Query ── */

int aurora_widget_plugin_list(void** out_plugins, int* out_count) {
    if (!out_plugins || !out_count) return 0;
    *out_count = g_widget_plugin_count;
    if (g_widget_plugin_count > 0) {
        *out_plugins = (void*)g_widget_plugins;
    } else {
        *out_plugins = nullptr;
    }
    return 1;
}

const char* aurora_widget_plugin_get_name(int index) {
    if (index < 0 || index >= g_widget_plugin_count) return nullptr;
    return g_widget_plugins[index].name;
}

const char* aurora_widget_plugin_get_version(const char* plugin_name) {
    int idx = find_widget_plugin(plugin_name);
    if (idx < 0) return nullptr;
    return g_widget_plugins[idx].version;
}

int aurora_widget_plugin_get_type_count(const char* plugin_name) {
    int idx = find_widget_plugin(plugin_name);
    if (idx < 0) return 0;
    return g_widget_plugins[idx].type_count;
}

const char* aurora_widget_plugin_get_type_name(const char* plugin_name, int type_index) {
    int idx = find_widget_plugin(plugin_name);
    if (idx < 0) return nullptr;
    if (type_index < 0 || type_index >= g_widget_plugins[idx].type_count) return nullptr;
    return g_widget_plugins[idx].types[type_index].type_name;
}

int aurora_widget_plugin_is_loaded(const char* plugin_name) {
    int idx = find_widget_plugin(plugin_name);
    return (idx >= 0 && g_widget_plugins[idx].loaded) ? 1 : 0;
}
