#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

int         aurora_inspector_init(void);
int         aurora_inspector_shutdown(void);
void        aurora_inspector_set_enabled(int enabled);
int         aurora_inspector_is_enabled(void);
void        aurora_inspector_highlight_widget(void* widget);
void        aurora_inspector_select_widget(void* widget);
void*       aurora_inspector_get_selected(void);
void        aurora_inspector_render_overlay(void);
const char* aurora_inspector_get_property_str(void* widget, const char* key);
int         aurora_inspector_get_property_int(void* widget, const char* key);
void        aurora_inspector_set_property_str(void* widget, const char* key, const char* value);
void        aurora_inspector_set_property_int(void* widget, const char* key, int value);

#ifdef __cplusplus
}
#endif
