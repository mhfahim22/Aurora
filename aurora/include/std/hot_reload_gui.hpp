#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

int         aurora_hot_reload_gui_init(void);
int         aurora_hot_reload_gui_shutdown(void);
int         aurora_hot_reload_gui_diff(void);
int         aurora_hot_reload_gui_apply(void);
const char* aurora_hot_reload_gui_diff_tree(void);
int         aurora_hot_reload_gui_preserve_state(void);
int         aurora_hot_reload_gui_restore_state(void);
const char* aurora_hot_reload_gui_get_preserved_state(void);

#ifdef __cplusplus
}
#endif
