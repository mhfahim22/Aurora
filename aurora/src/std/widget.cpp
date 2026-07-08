#include "std/widget.hpp"
#include <cstdlib>
#include <cstring>
#include <cmath>

/* Platform dispatch */
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
#  include "std/gui.hpp"

/* On desktop we use a lightweight wrapper around native widgets.
   Each widget is a MwWidget-compatible struct so the API is consistent. */
typedef struct CompWidget {
    int      type;
    float    x, y, w, h;
    char*    text;
    float    value;
    unsigned int bg_color;
    int      font_size;
    int      child_count;
    struct CompWidget** children;
    struct CompWidget*  parent;
} CompWidget;

static CompWidget* comp_alloc(int type)
{
    CompWidget* cw = (CompWidget*)calloc(1, sizeof(CompWidget));
    cw->type = type;
    cw->font_size = 14;
    return cw;
}

/* Map APP_WIDGET_* to AURORA_WIDGET_* */
static int map_to_gui(int app_type)
{
    switch (app_type) {
        case APP_WIDGET_BUTTON:   return AURORA_WIDGET_BUTTON;
        case APP_WIDGET_LABEL:    return AURORA_WIDGET_LABEL;
        case APP_WIDGET_TEXTBOX:  return AURORA_WIDGET_TEXTBOX;
        case APP_WIDGET_IMAGE:    return AURORA_WIDGET_IMAGE;
        case APP_WIDGET_COLUMN:   return AURORA_WIDGET_COLUMN;
        case APP_WIDGET_ROW:      return AURORA_WIDGET_ROW;
        case APP_WIDGET_GRID:     return AURORA_WIDGET_GRID;
        case APP_WIDGET_LIST:     return AURORA_WIDGET_LISTBOX;
        case APP_WIDGET_SLIDER:   return AURORA_WIDGET_SLIDER;
        case APP_WIDGET_CHECKBOX: return AURORA_WIDGET_CHECKBOX;
        case APP_WIDGET_PROGRESS: return AURORA_WIDGET_PROGRESSBAR;
        case APP_WIDGET_COMBOBOX: return AURORA_WIDGET_COMBOBOX;
        default:                  return AURORA_WIDGET_BUTTON;
    }
}

#elif AURORA_PLATFORM_ANDROID || AURORA_PLATFORM_IOS
#  include "mobile/widgets.hpp"

/* Mobile: MwWidget* is the handle — no translation needed */
typedef MwWidget CompWidget;

static CompWidget* comp_alloc(int app_type)
{
    int mw_type;
    switch (app_type) {
        case APP_WIDGET_BUTTON:   mw_type = MW_BUTTON;   break;
        case APP_WIDGET_LABEL:    mw_type = MW_TEXT;     break;
        case APP_WIDGET_TEXTBOX:  mw_type = MW_INPUT;    break;
        case APP_WIDGET_IMAGE:    mw_type = MW_IMAGE;    break;
        case APP_WIDGET_COLUMN:   mw_type = MW_COLUMN;   break;
        case APP_WIDGET_ROW:      mw_type = MW_ROW;      break;
        case APP_WIDGET_GRID:     mw_type = MW_GRID;     break;
        case APP_WIDGET_LIST:     mw_type = MW_LIST;     break;
        case APP_WIDGET_SLIDER:   mw_type = MW_SLIDER;   break;
        case APP_WIDGET_CHECKBOX: mw_type = MW_CHECKBOX; break;
        case APP_WIDGET_PROGRESS: mw_type = MW_PROGRESS; break;
        case APP_WIDGET_COMBOBOX: mw_type = MW_LIST;     break;
        default:                  mw_type = MW_BUTTON;    break;
    }
    return (CompWidget*)mw_create(mw_type);
}
#endif

/* ════════════════════════════════════════════════════════════
   Public API
   ════════════════════════════════════════════════════════════ */

extern "C" {

void* aurora_widget_new(int type)
{
    return (void*)comp_alloc(type);
}

void aurora_widget_free(void* w)
{
    if (!w) return;
    CompWidget* cw = (CompWidget*)w;
    free(cw->text);
    free(cw->children);
#if AURORA_PLATFORM_ANDROID || AURORA_PLATFORM_IOS
    mw_destroy(w);
#else
    free(cw);
#endif
}

void aurora_widget_add_child(void* parent, void* child)
{
    if (!parent || !child) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    CompWidget* p = (CompWidget*)parent;
    CompWidget** newc = (CompWidget**)realloc(p->children,
                         sizeof(CompWidget*) * (p->child_count + 1));
    if (newc) {
        p->children = newc;
        p->children[p->child_count++] = (CompWidget*)child;
        ((CompWidget*)child)->parent = p;
    }
#else
    mw_add_child(parent, child);
#endif
}

void aurora_widget_set_text(void* w, const char* text)
{
    if (!w || !text) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    CompWidget* cw = (CompWidget*)w;
    free(cw->text);
    cw->text = (char*)malloc(strlen(text) + 1);
    if (cw->text) strcpy(cw->text, text);
#else
    mw_set_text(w, text);
#endif
}

void aurora_widget_set_pos(void* w, float x, float y)
{
    if (!w) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    CompWidget* cw = (CompWidget*)w;
    cw->x = x; cw->y = y;
#else
    mw_set_pos(w, x, y);
#endif
}

void aurora_widget_set_size(void* w, float width, float height)
{
    if (!w) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    CompWidget* cw = (CompWidget*)w;
    cw->w = width; cw->h = height;
#else
    mw_set_size(w, width, height);
#endif
}

void aurora_widget_set_value(void* w, float val)
{
    if (!w) return;
#if AURORA_PLATFORM_ANDROID || AURORA_PLATFORM_IOS
    mw_set_value(w, val);
#else
    ((CompWidget*)w)->value = val;
#endif
}

void aurora_widget_set_bg(void* w, unsigned int color)
{
    if (!w) return;
#if AURORA_PLATFORM_ANDROID || AURORA_PLATFORM_IOS
    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >>  8) & 0xFF) / 255.0f;
    float b = ((color      ) & 0xFF) / 255.0f;
    float a = ((color >> 24) & 0xFF) / 255.0f;
    mw_set_bg_color(w, r, g, b, a);
#else
    ((CompWidget*)w)->bg_color = color;
#endif
}

void aurora_widget_set_font_size(void* w, int size)
{
    if (!w) return;
#if AURORA_PLATFORM_ANDROID || AURORA_PLATFORM_IOS
    mw_set_font_size(w, (float)size);
#else
    ((CompWidget*)w)->font_size = size;
#endif
}

void aurora_widget_layout(void* w, float parent_w, float parent_h)
{
    if (!w) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    (void)parent_w; (void)parent_h;
    /* Desktop native widgets are positioned by the OS */
#else
    mw_layout(w);
#endif
}

void aurora_widget_render(void* w)
{
    if (!w) return;
#if AURORA_PLATFORM_ANDROID || AURORA_PLATFORM_IOS
    mw_render(w);
#endif
}

} /* extern "C" */
