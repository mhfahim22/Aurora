#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

#define MW_BUTTON       0
#define MW_TEXT         1
#define MW_IMAGE        2
#define MW_COLUMN       3
#define MW_ROW          4
#define MW_GRID         5
#define MW_LIST         6
#define MW_SCROLL       7
#define MW_INPUT        8
#define MW_DIALOG       9
#define MW_BOTTOM_SHEET 10
#define MW_NAV_BAR      11
#define MW_TAB_BAR      12
#define MW_DRAWER       13
#define MW_FAB          14
#define MW_SNACKBAR     15

#define MW_EVENT_CLICK      0
#define MW_EVENT_CHANGE     1
#define MW_EVENT_FOCUS      2
#define MW_EVENT_BLUR       3
#define MW_EVENT_SUBMIT     4
#define MW_EVENT_TAB_SELECT 5
#define MW_EVENT_SCROLL     6
#define MW_EVENT_DRAWER     7
#define MW_EVENT_DISMISS    8

#define MW_MAIN_START      0
#define MW_MAIN_CENTER     1
#define MW_MAIN_END        2
#define MW_MAIN_SPACE_BETWEEN 3
#define MW_MAIN_SPACE_AROUND  4
#define MW_CROSS_START     0
#define MW_CROSS_CENTER    1
#define MW_CROSS_END       2
#define MW_CROSS_STRETCH   3

#define MW_TOUCH_DOWN      0
#define MW_TOUCH_UP        1
#define MW_TOUCH_MOVE      2
#define MW_TOUCH_CANCEL    3

typedef void (*MwEventCallback)(void* widget, int event_type, void* data);

void  mw_init(void);
void  mw_shutdown(void);
void* mw_create(int type);
void  mw_destroy(void* widget);
void  mw_add_child(void* parent, void* child);
void  mw_remove_child(void* parent, void* child);
void  mw_set_pos(void* widget, float x, float y);
void  mw_set_size(void* widget, float w, float h);
float mw_get_width(void* widget);
float mw_get_height(void* widget);
void  mw_layout(void* widget);
void  mw_set_align(void* widget, int main_axis, int cross_axis);
void  mw_set_spacing(void* widget, float spacing);
void  mw_set_padding(void* widget, float l, float t, float r, float b);
void  mw_set_margin(void* widget, float l, float t, float r, float b);
void  mw_set_bg_color(void* widget, float r, float g, float b, float a);
void  mw_set_text_color(void* widget, float r, float g, float b, float a);
void  mw_set_font_size(void* widget, float size);
void  mw_set_text(void* widget, const char* text);
const char* mw_get_text(void* widget);
void  mw_set_enabled(void* widget, int enabled);
void  mw_set_visible(void* widget, int visible);
void  mw_set_image(void* widget, const char* path);
void  mw_set_value(void* widget, float value);
void  mw_set_selected(void* widget, int index);
int   mw_get_type(void* widget);
void  mw_add_item(void* widget, const char* text);
void  mw_remove_item(void* widget, int index);
void  mw_clear_items(void* widget);
void  mw_set_callback(void* widget, MwEventCallback callback);
int   mw_handle_touch(void* widget, float x, float y, int action);
void  mw_set_scroll_pos(void* widget, float x, float y);
void  mw_get_scroll_pos(void* widget, float* x, float* y);
void  mw_render(void* widget);

#ifdef __cplusplus
}
#endif
