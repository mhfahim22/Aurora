#pragma once
#include <cstdint>
#include "common/platform.hpp"

#ifdef __cplusplus
extern "C" {
#endif

/* ════════════════════════════════════════════════════════════
   Phase 8: Cross-Platform Widget Component API
   
   Unified component-based widget builder that works on both
   desktop (aurora_gui_*) and mobile (mw_*) backends.
   
   Component types:
     0 = BUTTON, 1 = LABEL, 2 = TEXTBOX, 3 = IMAGE,
     4 = COLUMN, 5 = ROW, 6 = GRID, 7 = LIST,
     8 = SCROLL, 9 = SLIDER, 10 = SWITCH, 11 = CHECKBOX,
     12 = RADIO, 13 = PROGRESS, 14 = COMBOBOX
   ════════════════════════════════════════════════════════════ */

#define APP_WIDGET_BUTTON      0
#define APP_WIDGET_LABEL       1
#define APP_WIDGET_TEXTBOX     2
#define APP_WIDGET_IMAGE       3
#define APP_WIDGET_COLUMN      4
#define APP_WIDGET_ROW         5
#define APP_WIDGET_GRID        6
#define APP_WIDGET_LIST        7
#define APP_WIDGET_SCROLL      8
#define APP_WIDGET_SLIDER      9
#define APP_WIDGET_SWITCH      10
#define APP_WIDGET_CHECKBOX    11
#define APP_WIDGET_RADIO       12
#define APP_WIDGET_PROGRESS    13
#define APP_WIDGET_COMBOBOX    14

/* ── Component API ── */

AURORA_EXPORT void* aurora_widget_new(int type);
AURORA_EXPORT void  aurora_widget_free(void* w);
AURORA_EXPORT void  aurora_widget_add_child(void* parent, void* child);
AURORA_EXPORT void  aurora_widget_set_text(void* w, const char* text);
AURORA_EXPORT void  aurora_widget_set_pos(void* w, float x, float y);
AURORA_EXPORT void  aurora_widget_set_size(void* w, float width, float height);
AURORA_EXPORT void  aurora_widget_set_value(void* w, float val);
AURORA_EXPORT void  aurora_widget_set_bg(void* w, unsigned int color);
AURORA_EXPORT void  aurora_widget_set_font_size(void* w, int size);

/* ── Layout component ── */
AURORA_EXPORT void  aurora_widget_layout(void* w, float parent_w, float parent_h);

/* ── Render (mobile) / show (desktop) ── */
AURORA_EXPORT void  aurora_widget_render(void* w);

#ifdef __cplusplus
}
#endif
