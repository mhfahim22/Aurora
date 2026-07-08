#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Widget Plugin — registration, lifecycle, query ── */

/* Register a widget type from a loaded native plugin */
int  aurora_widget_plugin_register(const char* plugin_name, const char* version,
                                    const char* widget_type,
                                    void* create_fn, void* destroy_fn,
                                    void* render_fn, void* event_fn);

/* Load a native plugin and auto-register its widget types */
int  aurora_widget_plugin_load(const char* path);

/* Widget lifecycle */
void* aurora_widget_plugin_create(const char* plugin_name, const char* widget_type);
int   aurora_widget_plugin_destroy(void* widget);
int   aurora_widget_plugin_render(void* widget, void* ctx);
int   aurora_widget_plugin_handle_event(void* widget, void* event);

/* Query registered widget plugins */
int         aurora_widget_plugin_list(void** out_plugins, int* out_count);
const char* aurora_widget_plugin_get_name(int index);
const char* aurora_widget_plugin_get_version(const char* plugin_name);
int         aurora_widget_plugin_get_type_count(const char* plugin_name);
const char* aurora_widget_plugin_get_type_name(const char* plugin_name, int type_index);
int         aurora_widget_plugin_is_loaded(const char* plugin_name);

#ifdef __cplusplus
}
#endif
