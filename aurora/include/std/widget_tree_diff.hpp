#ifndef AURORA_WIDGET_TREE_DIFF_HPP
#define AURORA_WIDGET_TREE_DIFF_HPP

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* Widget tree diffing: only redraw changed widgets */
int  aurora_widget_diff_init(void);
int  aurora_widget_diff_snapshot(void);
int  aurora_widget_diff_compute(void);
int  aurora_widget_diff_get_change_count(void);
int  aurora_widget_diff_has_changed(int widget_id);
void aurora_widget_diff_apply(void);
void aurora_widget_diff_clear(void);

#ifdef __cplusplus
}
#endif

#endif
