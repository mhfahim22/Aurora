#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

int         aurora_a11y_set_label(void* widget, const char* label);
const char* aurora_a11y_get_label(void* widget);
int         aurora_a11y_set_role(void* widget, const char* role);
const char* aurora_a11y_get_role(void* widget);
int         aurora_a11y_set_focusable(void* widget, int focusable);
int         aurora_a11y_is_focusable(void* widget);
int         aurora_a11y_set_tab_index(void* widget, int index);
int         aurora_a11y_get_tab_index(void* widget);
int         aurora_a11y_focus_next(void);
int         aurora_a11y_focus_prev(void);
void*       aurora_a11y_get_focused(void);
int         aurora_a11y_announce(const char* text);
int         aurora_a11y_set_hint(void* widget, const char* hint);
int         aurora_a11y_register_shortcut(const char* shortcut, void (*fn)(void));
int         aurora_a11y_unregister_shortcut(const char* shortcut);
int         aurora_a11y_screen_reader_active(void);

#ifdef __cplusplus
}
#endif
