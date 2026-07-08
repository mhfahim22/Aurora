#include "std/widgets_advanced.hpp"
#include "std/gui.hpp"
#include "common/platform.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>

/* Platform dispatch */
#if AURORA_PLATFORM_ANDROID || AURORA_PLATFORM_IOS
#  include "mobile/widgets.hpp"
#endif

/* ── Forward declarations for desktop backends ── */
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
extern "C" {
    /* These are implemented in gui_win32.cpp / gui.cpp for the new widget types */
    AURORA_EXPORT AuroraWidget aurora_gui_webview_new(AuroraWidget, int, int, int, int);
    AURORA_EXPORT void aurora_gui_webview_navigate(AuroraWidget, const char*);
    AURORA_EXPORT void aurora_gui_webview_go_back(AuroraWidget);
    AURORA_EXPORT void aurora_gui_webview_go_forward(AuroraWidget);
    AURORA_EXPORT void aurora_gui_webview_reload(AuroraWidget);
    AURORA_EXPORT AuroraWidget aurora_gui_media_new(AuroraWidget, int, int, int, int);
    AURORA_EXPORT void aurora_gui_media_open(AuroraWidget, const char*);
    AURORA_EXPORT void aurora_gui_media_play(AuroraWidget);
    AURORA_EXPORT void aurora_gui_media_pause(AuroraWidget);
    AURORA_EXPORT void aurora_gui_media_stop(AuroraWidget);
    AURORA_EXPORT void aurora_gui_media_set_volume(AuroraWidget, float);
    AURORA_EXPORT void aurora_gui_media_set_looping(AuroraWidget, int);
    AURORA_EXPORT int aurora_gui_media_is_playing(AuroraWidget);
    AURORA_EXPORT AuroraWidget aurora_gui_map_new(AuroraWidget, int, int, int, int);
    AURORA_EXPORT void aurora_gui_map_set_center(AuroraWidget, double, double);
    AURORA_EXPORT void aurora_gui_map_set_zoom(AuroraWidget, int);
    AURORA_EXPORT void aurora_gui_map_add_marker(AuroraWidget, double, double, const char*);
}
#endif

/* ── Canvas widget — wraps 2D canvas engine on desktop, MwWidget on mobile ── */

struct AdvCanvas {
    int type;
    int x, y, w, h;
    void* native;
    AuroraWidgetPaintFn paint_fn;
    void* paint_user;
    float dpi;
};

extern "C" {

void* aurora_advanced_canvas_new(void* parent, int x, int y, int w, int h)
{
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    AuroraWidget gw = aurora_gui_canvas_new((AuroraWidget)parent, x, y, w, h);
    return gw;
#else
    void* c = mw_create(MW_CANVAS);
    if (parent) mw_add_child(parent, c);
    mw_set_pos(c, (float)x, (float)y);
    mw_set_size(c, (float)w, (float)h);
    return c;
#endif
}

void aurora_advanced_canvas_set_paint(void* w, AuroraWidgetPaintFn fn, void* user)
{
    if (!w) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_canvas_set_paint_callback((AuroraWidget)w, (AuroraPaintCallback)fn, user);
#else
    (void)fn; (void)user;
#endif
}

void aurora_advanced_canvas_repaint(void* w)
{
    if (!w) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_canvas_repaint((AuroraWidget)w);
#endif
}

void aurora_advanced_canvas_set_dpi(void* w, float dpi)
{
    (void)w; (void)dpi;
}

/* ── Table Widget ── */

void* aurora_advanced_table_new(void* parent, int x, int y, int w, int h)
{
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    return aurora_gui_table_new((AuroraWidget)parent, x, y, w, h);
#else
    void* t = mw_create(MW_TABLE);
    if (parent) mw_add_child(parent, t);
    mw_set_pos(t, (float)x); mw_set_size(t, (float)w, (float)h);
    return t;
#endif
}

void aurora_advanced_table_add_column(void* tbl, const char* heading)
{
    if (!tbl) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_table_add_column((AuroraWidget)tbl, heading, 100);
#else
    mw_add_item(tbl, heading);
#endif
}

void aurora_advanced_table_add_row(void* tbl)
{
    if (!tbl) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_table_add_row((AuroraWidget)tbl);
#endif
}

void aurora_advanced_table_set_cell(void* tbl, int row, int col, const char* text)
{
    if (!tbl) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_table_set_cell((AuroraWidget)tbl, row, col, text);
#endif
}

int aurora_advanced_table_selected_row(void* tbl)
{
    if (!tbl) return -1;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    return aurora_gui_table_get_selected((AuroraWidget)tbl);
#else
    return 0;
#endif
}

void aurora_advanced_table_clear(void* tbl)
{
    if (!tbl) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_table_clear((AuroraWidget)tbl);
#endif
}

/* ── TreeView Widget ── */

void* aurora_advanced_treeview_new(void* parent, int x, int y, int w, int h)
{
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    return aurora_gui_treeview_new((AuroraWidget)parent, x, y, w, h);
#else
    void* tv = mw_create(MW_TREEVIEW);
    if (parent) mw_add_child(parent, tv);
    mw_set_pos(tv, (float)x); mw_set_size(tv, (float)w, (float)h);
    return tv;
#endif
}

void* aurora_advanced_treeview_add_item(void* tv, void* parent_item, const char* text)
{
    if (!tv) return nullptr;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    return aurora_gui_treeview_add_item((AuroraWidget)tv, text, (AuroraTreeItem)parent_item);
#else
    mw_add_item(tv, text);
    return tv;
#endif
}

void aurora_advanced_treeview_remove_item(void* tv, void* item)
{
    if (!tv) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_treeview_remove_item((AuroraWidget)tv, (AuroraWidget)item);
#endif
}

void aurora_advanced_treeview_expand(void* tv, void* item)
{
    if (!tv) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_treeview_expand((AuroraWidget)tv, (AuroraWidget)item);
#endif
}

void aurora_advanced_treeview_collapse(void* tv, void* item)
{
    if (!tv) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_treeview_collapse((AuroraWidget)tv, (AuroraWidget)item);
#endif
}

void* aurora_advanced_treeview_selected(void* tv)
{
    if (!tv) return nullptr;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    return aurora_gui_treeview_get_selected((AuroraWidget)tv);
#else
    return tv;
#endif
}

void aurora_advanced_treeview_set_on_select(void* tv, void (*cb)(void*, const char*))
{
    if (!tv) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_set_callback((AuroraWidget)tv, (AuroraEventCallback)cb);
#else
    (void)cb;
#endif
}

/* ── WebView Widget ── */

void* aurora_advanced_webview_new(void* parent, int x, int y, int w, int h)
{
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    return aurora_gui_webview_new((AuroraWidget)parent, x, y, w, h);
#else
    void* wv = mw_create(MW_WEBVIEW);
    if (parent) mw_add_child(parent, wv);
    mw_set_pos(wv, (float)x); mw_set_size(wv, (float)w, (float)h);
    return wv;
#endif
}

void aurora_advanced_webview_navigate(void* wv, const char* url)
{
    if (!wv) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_webview_navigate((AuroraWidget)wv, url);
#endif
}

void aurora_advanced_webview_go_back(void* wv)
{
    if (!wv) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_webview_go_back((AuroraWidget)wv);
#endif
}

void aurora_advanced_webview_go_forward(void* wv)
{
    if (!wv) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_webview_go_forward((AuroraWidget)wv);
#endif
}

void aurora_advanced_webview_reload(void* wv)
{
    if (!wv) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_webview_reload((AuroraWidget)wv);
#endif
}

void aurora_advanced_webview_set_on_title(void* wv, void (*cb)(const char*))
{
    if (!wv) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    /* delegate to gui layer */
    (void)cb;
#endif
}

/* ── Media Player Widget ── */

void* aurora_advanced_media_new(void* parent, int x, int y, int w, int h)
{
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    return aurora_gui_media_new((AuroraWidget)parent, x, y, w, h);
#else
    void* mw = mw_create(MW_MEDIA);
    if (parent) mw_add_child(parent, mw);
    mw_set_pos(mw, (float)x); mw_set_size(mw, (float)w, (float)h);
    return mw;
#endif
}

void aurora_advanced_media_open(void* mw, const char* src)
{
    if (!mw) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_media_open((AuroraWidget)mw, src);
#endif
}

void aurora_advanced_media_play(void* mw)
{
    if (!mw) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_media_play((AuroraWidget)mw);
#endif
}

void aurora_advanced_media_pause(void* mw)
{
    if (!mw) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_media_pause((AuroraWidget)mw);
#endif
}

void aurora_advanced_media_stop(void* mw)
{
    if (!mw) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_media_stop((AuroraWidget)mw);
#endif
}

void aurora_advanced_media_set_volume(void* mw, float vol)
{
    if (!mw) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_media_set_volume((AuroraWidget)mw, vol);
#endif
}

void aurora_advanced_media_set_looping(void* mw, int loop)
{
    if (!mw) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_media_set_looping((AuroraWidget)mw, loop);
#endif
}

int aurora_advanced_media_is_playing(void* mw)
{
    if (!mw) return 0;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    return aurora_gui_media_is_playing((AuroraWidget)mw);
#else
    return 0;
#endif
}

/* ── Map Widget ── */

void* aurora_advanced_map_new(void* parent, int x, int y, int w, int h)
{
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    return aurora_gui_map_new((AuroraWidget)parent, x, y, w, h);
#else
    void* mp = mw_create(MW_MAP);
    if (parent) mw_add_child(parent, mp);
    mw_set_pos(mp, (float)x); mw_set_size(mp, (float)w, (float)h);
    return mp;
#endif
}

void aurora_advanced_map_set_center(void* mp, double lat, double lon)
{
    if (!mp) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_map_set_center((AuroraWidget)mp, lat, lon);
#endif
}

void aurora_advanced_map_set_zoom(void* mp, int zoom)
{
    if (!mp) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_map_set_zoom((AuroraWidget)mp, zoom);
#endif
}

void aurora_advanced_map_add_marker(void* mp, double lat, double lon, const char* title)
{
    if (!mp) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    aurora_gui_map_add_marker((AuroraWidget)mp, lat, lon, title);
#endif
}

void aurora_advanced_map_clear_markers(void* mp)
{
    if (!mp) return;
#if AURORA_PLATFORM_WINDOWS || AURORA_PLATFORM_LINUX || AURORA_PLATFORM_MACOS
    (void)mp;
#endif
}

} /* extern "C" */
