#pragma once
#include <cstdint>
#include "common/platform.hpp"

#ifdef __cplusplus
extern "C" {
#endif

/* ════════════════════════════════════════════════════════════
   Phase 9: Advanced Widgets — Cross-Platform API
   
   Canvas, Table, TreeView, WebView, Media Player, Map
   ════════════════════════════════════════════════════════════ */

/* ── Canvas Widget — paint-callback based, backed by 2D canvas engine ── */

typedef void (*AuroraWidgetPaintFn)(void* user_data, int x, int y, int w, int h);

AURORA_EXPORT void* aurora_advanced_canvas_new(void* parent, int x, int y, int w, int h);
AURORA_EXPORT void  aurora_advanced_canvas_set_paint(void* w, AuroraWidgetPaintFn fn, void* user);
AURORA_EXPORT void  aurora_advanced_canvas_repaint(void* w);
AURORA_EXPORT void  aurora_advanced_canvas_set_dpi(void* w, float dpi);

/* ── Table Widget ── */

AURORA_EXPORT void* aurora_advanced_table_new(void* parent, int x, int y, int w, int h);
AURORA_EXPORT void  aurora_advanced_table_add_column(void* tbl, const char* heading);
AURORA_EXPORT void  aurora_advanced_table_add_row(void* tbl);
AURORA_EXPORT void  aurora_advanced_table_set_cell(void* tbl, int row, int col, const char* text);
AURORA_EXPORT int   aurora_advanced_table_selected_row(void* tbl);
AURORA_EXPORT void  aurora_advanced_table_clear(void* tbl);

/* ── TreeView Widget ── */

AURORA_EXPORT void* aurora_advanced_treeview_new(void* parent, int x, int y, int w, int h);
AURORA_EXPORT void* aurora_advanced_treeview_add_item(void* tv, void* parent_item, const char* text);
AURORA_EXPORT void  aurora_advanced_treeview_remove_item(void* tv, void* item);
AURORA_EXPORT void  aurora_advanced_treeview_expand(void* tv, void* item);
AURORA_EXPORT void  aurora_advanced_treeview_collapse(void* tv, void* item);
AURORA_EXPORT void* aurora_advanced_treeview_selected(void* tv);
AURORA_EXPORT void  aurora_advanced_treeview_set_on_select(void* tv, void (*cb)(void* item, const char* text));

/* ── WebView Widget ── */

AURORA_EXPORT void* aurora_advanced_webview_new(void* parent, int x, int y, int w, int h);
AURORA_EXPORT void  aurora_advanced_webview_navigate(void* wv, const char* url);
AURORA_EXPORT void  aurora_advanced_webview_go_back(void* wv);
AURORA_EXPORT void  aurora_advanced_webview_go_forward(void* wv);
AURORA_EXPORT void  aurora_advanced_webview_reload(void* wv);
AURORA_EXPORT void  aurora_advanced_webview_set_on_title(void* wv, void (*cb)(const char* title));

/* ── Media Player Widget ── */

AURORA_EXPORT void* aurora_advanced_media_new(void* parent, int x, int y, int w, int h);
AURORA_EXPORT void  aurora_advanced_media_open(void* mw, const char* src);
AURORA_EXPORT void  aurora_advanced_media_play(void* mw);
AURORA_EXPORT void  aurora_advanced_media_pause(void* mw);
AURORA_EXPORT void  aurora_advanced_media_stop(void* mw);
AURORA_EXPORT void  aurora_advanced_media_set_volume(void* mw, float vol);
AURORA_EXPORT void  aurora_advanced_media_set_looping(void* mw, int loop);
AURORA_EXPORT int   aurora_advanced_media_is_playing(void* mw);

/* ── Map Widget ── */

AURORA_EXPORT void* aurora_advanced_map_new(void* parent, int x, int y, int w, int h);
AURORA_EXPORT void  aurora_advanced_map_set_center(void* mp, double lat, double lon);
AURORA_EXPORT void  aurora_advanced_map_set_zoom(void* mp, int zoom);
AURORA_EXPORT void  aurora_advanced_map_add_marker(void* mp, double lat, double lon, const char* title);
AURORA_EXPORT void  aurora_advanced_map_clear_markers(void* mp);

#ifdef __cplusplus
}
#endif
