#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* Callback type forward declaration needed by struct */
typedef void (*MwEventCallback)(void* widget, int event_type, void* data);

/* Internal widget structure — defined in header so renderers can access fields */
typedef struct MwWidget {
    int      type;
    float    x, y, w, h;
    char*    text;
    int      enabled;
    int      visible;
    float    padding[4];
    float    margin[4];
    float    bg_color[4];
    float    text_color[4];
    float    font_size;
    int      main_align;
    int      cross_align;
    float    spacing;
    float    value;
    int      selected_index;
    float    scroll_x, scroll_y;
    char*    image_path;
    char**   items;
    int      item_count;
    int      item_capacity;
    MwEventCallback callback;
    int      has_focus;
    struct MwWidget* parent;
    struct MwWidget** children;
    int      child_count;
    int      child_capacity;
    /* ── 37.3 Gesture tracking (long-press / swipe) ── */
    float    press_x, press_y;
    double   press_time_ms;
    int      touch_state;         /* MW_TOUCH_NONE/PRESSED/HELD */
    int      gesture_active;
    int      long_press_ms;
    float    swipe_threshold;
    /* ── 37.3 Safe area insets (notch / home indicator) ── */
    float    safe_area[4];        /* top, bottom, left, right */
    /* ── 37.3 Dark mode theming ── */
    int      dark_mode;           /* 0 = light, 1 = dark */
} MwWidget;

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
#define MW_SLIDER       16
#define MW_SWITCH       17
#define MW_CHECKBOX     18
#define MW_RADIO        19
#define MW_PROGRESS     20
/* ── Phase 9: Advanced Widgets ── */
#define MW_TABLE        21
#define MW_TREEVIEW     22
#define MW_CANVAS       23
#define MW_WEBVIEW      24
#define MW_MEDIA        25
#define MW_MAP          26

#define MW_EVENT_CLICK      0
#define MW_EVENT_CHANGE     1
#define MW_EVENT_FOCUS      2
#define MW_EVENT_BLUR       3
#define MW_EVENT_SUBMIT     4
#define MW_EVENT_TAB_SELECT 5
#define MW_EVENT_SCROLL     6
#define MW_EVENT_DRAWER     7
#define MW_EVENT_DISMISS    8
/* ── 37.3 Gesture events ── */
#define MW_EVENT_LONG_PRESS  9
#define MW_EVENT_SWIPE_LEFT  10
#define MW_EVENT_SWIPE_RIGHT 11
#define MW_EVENT_SWIPE_UP    12
#define MW_EVENT_SWIPE_DOWN  13

/* Touch state flags */
#define MW_TOUCH_NONE        0
#define MW_TOUCH_PRESSED     1
#define MW_TOUCH_HELD        2

/* Gesture tuning defaults */
#define MW_GESTURE_LONG_PRESS_MS  500.0
#define MW_GESTURE_SWIPE_THRESHOLD 60.0f

/* ── 37.3 Safe area + dark mode ── */
#define MW_THEME_LIGHT 0
#define MW_THEME_DARK  1

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

/* ── 37.3 Gesture recognition ── */
extern "C" {
void  mw_set_long_press_ms(void* widget, int ms);
void  mw_set_swipe_threshold(void* widget, float px);
int   mw_get_touch_state(void* widget);
}

/* ── 37.3 Safe area insets (notch / home indicator) ── */
void  mw_set_safe_area(void* widget, float top, float bottom, float left, float right);
void  mw_get_safe_area(void* widget, float* top, float* bottom, float* left, float* right);

/* ── 37.3 Dark mode theming ── */
void  mw_set_theme(void* widget, int theme);
int   mw_get_theme(void* widget);
int   mw_is_dark_mode(void* widget);
void  mw_set_dark_mode(void* widget, int dark);

#ifdef __cplusplus
}
#endif
