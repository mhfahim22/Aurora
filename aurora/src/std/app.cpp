#include "std/app.hpp"
#include <cstdlib>
#include <cstring>
#include <cmath>

/* ════════════════════════════════════════════════════════════
   Platform-conditional widget backend
   ════════════════════════════════════════════════════════════
   Desktop  → aurora_gui_*  (native Win32 / GTK / Cocoa)
   Mobile   → mw_*          (flexbox-based MwWidget tree)
   ════════════════════════════════════════════════════════════ */

#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
#  include "std/gui.hpp"

static void* make_widget(AuroraWidget parent, int gui_type,
                         const char* text, int x, int y, int w, int h)
{
    (void)gui_type;
    /* Desktop uses a single widget handle per app; parent is the app window.
       Individual aurora_gui_*_new functions create native children. */
    return nullptr; /* unused — widget-specific functions called directly */
}

#elif AURORA_PLATFORM_ANDROID || AURORA_PLATFORM_IOS
#  include "mobile/widgets.hpp"

/* Map desktop widget types to mobile MwWidget types */
static int map_widget_type(int app_type)
{
    switch (app_type) {
        case AURORA_WIDGET_BUTTON:      return MW_BUTTON;
        case AURORA_WIDGET_LABEL:       return MW_TEXT;
        case AURORA_WIDGET_TEXTBOX:     return MW_INPUT;
        case AURORA_WIDGET_LISTBOX:     return MW_LIST;
        case AURORA_WIDGET_CHECKBOX:    return MW_CHECKBOX;
        case AURORA_WIDGET_SLIDER:      return MW_SLIDER;
        case AURORA_WIDGET_PROGRESSBAR: return MW_PROGRESS;
        case AURORA_WIDGET_COMBOBOX:    return MW_LIST;
        case AURORA_WIDGET_SWITCH:      return MW_SWITCH;
        case AURORA_WIDGET_IMAGE:       return MW_IMAGE;
        case AURORA_WIDGET_ROW:         return MW_ROW;
        case AURORA_WIDGET_COLUMN:      return MW_COLUMN;
        case AURORA_WIDGET_GRID:        return MW_GRID;
        default:                        return MW_BUTTON;
    }
}

/* Mobile: create a widget via mw_create + configure it */
static void* make_widget(AuroraWidget parent, int gui_type,
                         const char* text, int x, int y, int w, int h)
{
    int mw_type = map_widget_type(gui_type);
    void* wgt = mw_create(mw_type);
    if (text) mw_set_text(wgt, text);
    if (x != 0 || y != 0) mw_set_pos(wgt, (float)x, (float)y);
    if (w > 0 && h > 0)   mw_set_size(wgt, (float)w, (float)h);
    if (parent)           mw_add_child(parent, wgt);
    return wgt;
}
#endif

/* Layout / nav / theme globals */
static void* g_app_layout = nullptr;
static void* g_app_nav = nullptr;
static void* g_app_theme = nullptr;

extern "C" {

/* ════════════════════════════════════════════════════════════
   App Lifecycle
   ════════════════════════════════════════════════════════════ */

AuroraApp aurora_app_init(const char* title, int width, int height)
{
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_app_init();
#endif
#if AURORA_PLATFORM_ANDROID || AURORA_PLATFORM_IOS
    mw_init();
#endif
    g_app_theme  = aurora_app_theme_init();
    g_app_layout = aurora_app_layout_create();
    g_app_nav    = aurora_app_nav_init();

#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    return (AuroraApp)aurora_gui_window_new(title, width, height);
#else
    void* root = mw_create(MW_COLUMN);
    mw_set_size(root, (float)width, (float)height);
    return (AuroraApp)root;
#endif
}

void* aurora_app_get_layout(AuroraApp app) { (void)app; return g_app_layout; }
void* aurora_app_get_nav(AuroraApp app)    { (void)app; return g_app_nav; }
void* aurora_app_get_theme(AuroraApp app)  { (void)app; return g_app_theme; }

void aurora_app_run(AuroraApp app)
{
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_window_show((AuroraWidget)app);
    aurora_gui_app_run();
#else
    mw_render(app);
#endif
}

void aurora_app_quit(AuroraApp app)
{
    (void)app;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_app_quit();
#endif
}

/* ════════════════════════════════════════════════════════════
   Windows
   ════════════════════════════════════════════════════════════ */

AuroraWidget aurora_app_window_new(AuroraApp app, const char* title, int x, int y, int w, int h)
{
    (void)app; (void)x; (void)y;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    return aurora_gui_window_new(title, w, h);
#else
    void* win = mw_create(MW_COLUMN);
    mw_set_size(win, (float)w, (float)h);
    return win;
#endif
}

/* ════════════════════════════════════════════════════════════
   Widget Creation
   ════════════════════════════════════════════════════════════ */

#define DEFINE_WIDGET_NEW(name, gui_type)                                        \
AuroraWidget aurora_app_ ## name ## _new(AuroraWidget parent, const char* text,  \
                                         int x, int y, int w, int h)             \
{                                                                                \
    (void)text;                                                                  \
    return (AuroraWidget)make_widget(parent, gui_type, text, x, y, w, h);        \
}

DEFINE_WIDGET_NEW(button,      AURORA_WIDGET_BUTTON)
DEFINE_WIDGET_NEW(label,       AURORA_WIDGET_LABEL)
DEFINE_WIDGET_NEW(checkbox,    AURORA_WIDGET_CHECKBOX)
DEFINE_WIDGET_NEW(switch,      AURORA_WIDGET_SWITCH)
DEFINE_WIDGET_NEW(image,       AURORA_WIDGET_IMAGE)

/* Widgets without text */
AuroraWidget aurora_app_textbox_new(AuroraWidget parent, int x, int y, int w, int h)
{
    return (AuroraWidget)make_widget(parent, AURORA_WIDGET_TEXTBOX, nullptr, x, y, w, h);
}

AuroraWidget aurora_app_listbox_new(AuroraWidget parent, int x, int y, int w, int h)
{
    return (AuroraWidget)make_widget(parent, AURORA_WIDGET_LISTBOX, nullptr, x, y, w, h);
}

AuroraWidget aurora_app_progressbar_new(AuroraWidget parent, int x, int y, int w, int h)
{
    return (AuroraWidget)make_widget(parent, AURORA_WIDGET_PROGRESSBAR, nullptr, x, y, w, h);
}

AuroraWidget aurora_app_combobox_new(AuroraWidget parent, int x, int y, int w, int h)
{
    return (AuroraWidget)make_widget(parent, AURORA_WIDGET_COMBOBOX, nullptr, x, y, w, h);
}

AuroraWidget aurora_app_slider_new(AuroraWidget parent, int x, int y, int w, int h, int min, int max)
{
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    return aurora_gui_slider_new(parent, x, y, w, h, min, max);
#else
    void* s = make_widget(parent, AURORA_WIDGET_SLIDER, nullptr, x, y, w, h);
    (void)min; (void)max;
    return (AuroraWidget)s;
#endif
}

/* ════════════════════════════════════════════════════════════
   Widget Properties
   ════════════════════════════════════════════════════════════ */

void aurora_app_set_text(AuroraWidget widget, const char* text)
{
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_label_set_text(widget, text);
#else
    mw_set_text(widget, text);
#endif
}

const char* aurora_app_get_text(AuroraWidget widget)
{
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    return aurora_gui_textbox_get_text(widget);
#else
    return mw_get_text(widget);
#endif
}

/* ListBox */
void aurora_app_listbox_add(AuroraWidget listbox, const char* item)
{
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_listbox_add_item(listbox, item);
#else
    mw_add_item(listbox, item);
#endif
}

void aurora_app_listbox_clear(AuroraWidget listbox)
{
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_listbox_clear(listbox);
#else
    mw_clear_items(listbox);
#endif
}

int aurora_app_listbox_count(AuroraWidget listbox)
{
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    return aurora_gui_listbox_count(listbox);
#else
    /* mw_* doesn't expose item count directly; track via items array */
    MwWidget* w = (MwWidget*)listbox;
    return w ? w->item_count : 0;
#endif
}

int aurora_app_listbox_selected(AuroraWidget listbox)
{
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    return aurora_gui_listbox_get_selected(listbox);
#else
    MwWidget* w = (MwWidget*)listbox;
    return w ? w->selected_index : -1;
#endif
}

/* ComboBox */
void aurora_app_combobox_add(AuroraWidget combobox, const char* item)
{
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_combobox_add_item(combobox, item);
#else
    mw_add_item(combobox, item);
#endif
}

/* Slider */
int aurora_app_slider_value(AuroraWidget slider)
{
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    return aurora_gui_slider_get_value(slider);
#else
    MwWidget* w = (MwWidget*)slider;
    return w ? (int)w->value : 0;
#endif
}

void aurora_app_slider_set(AuroraWidget slider, int value)
{
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_slider_set_value(slider, value);
#else
    mw_set_value(slider, (float)value);
#endif
}

/* ProgressBar */
void aurora_app_progress_set(AuroraWidget progress, int value)
{
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_progressbar_set_value(progress, value);
#else
    mw_set_value(progress, (float)value);
#endif
}

/* CheckBox */
int aurora_app_checkbox_checked(AuroraWidget checkbox)
{
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    return aurora_gui_checkbox_is_checked(checkbox);
#else
    MwWidget* w = (MwWidget*)checkbox;
    return w ? (int)w->value : 0;
#endif
}

void aurora_app_checkbox_set(AuroraWidget checkbox, int checked)
{
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_checkbox_set_checked(checkbox, checked);
#else
    mw_set_value(checkbox, (float)checked);
#endif
}

/* Switch */
int aurora_app_switch_on(AuroraWidget sw)
{
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    return aurora_gui_switch_is_on(sw);
#else
    MwWidget* w = (MwWidget*)sw;
    return w ? (int)w->value : 0;
#endif
}

void aurora_app_switch_set(AuroraWidget sw, int on)
{
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_switch_set_on(sw, on);
#else
    mw_set_value(sw, (float)on);
#endif
}

/* Style */
void aurora_app_set_bg(AuroraWidget widget, unsigned int color)
{
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_label_set_color(widget, color);
#else
    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >>  8) & 0xFF) / 255.0f;
    float b = ((color      ) & 0xFF) / 255.0f;
    float a = ((color >> 24) & 0xFF) / 255.0f;
    mw_set_bg_color(widget, r, g, b, a);
#endif
}

void aurora_app_set_font_size(AuroraWidget widget, int size)
{
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_label_set_font_size(widget, size);
#else
    mw_set_font_size(widget, (float)size);
#endif
}

void aurora_app_set_text_align(AuroraWidget widget, int align)
{
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_label_set_align(widget, align);
#else
    (void)widget; (void)align;
#endif
}

/* ════════════════════════════════════════════════════════════
   Events
   ════════════════════════════════════════════════════════════ */

void aurora_app_set_callback(AuroraWidget widget, AuroraAppEventCallback cb)
{
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_set_callback(widget, cb);
#endif
}

void aurora_app_set_on_click(AuroraWidget widget, AuroraAppEventCallback cb)
{
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_set_callback(widget, cb);
#endif
}

void aurora_app_set_on_change(AuroraWidget widget, AuroraAppEventCallback cb)
{
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_set_callback(widget, cb);
#endif
}

} /* extern "C" */
