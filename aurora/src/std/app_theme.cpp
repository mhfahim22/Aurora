#include "std/app.hpp"
#include <cstdlib>
#include <cstring>
#include <map>

/* ════════════════════════════════════════════════════════════
   Cross-platform theme system
   ════════════════════════════════════════════════════════════ */

/* Theme color keys */
#define APP_THEME_PRIMARY        0
#define APP_THEME_SECONDARY      1
#define APP_THEME_BACKGROUND     2
#define APP_THEME_SURFACE        3
#define APP_THEME_ERROR          4
#define APP_THEME_ON_PRIMARY     5
#define APP_THEME_ON_SECONDARY   6
#define APP_THEME_ON_BACKGROUND  7
#define APP_THEME_ON_SURFACE     8
#define APP_THEME_ON_ERROR       9
#define APP_THEME_OUTLINE        10

/* Theme font keys */
#define APP_THEME_FONT_HEADLINE  0
#define APP_THEME_FONT_TITLE     1
#define APP_THEME_FONT_BODY      2
#define APP_THEME_FONT_LABEL     3
#define APP_THEME_FONT_CAPTION   4

struct AppTheme {
    int mode;
    std::map<int, unsigned int> colors_light;
    std::map<int, unsigned int> colors_dark;
    std::map<int, int> fonts;
    float spacings[5];
};

static void init_default_light(AppTheme* t) {
    t->colors_light[APP_THEME_PRIMARY]        = 0xFF6750A4;
    t->colors_light[APP_THEME_SECONDARY]      = 0xFF625B71;
    t->colors_light[APP_THEME_BACKGROUND]     = 0xFFFFFBFE;
    t->colors_light[APP_THEME_SURFACE]        = 0xFFFFFBFE;
    t->colors_light[APP_THEME_ERROR]          = 0xFFB3261E;
    t->colors_light[APP_THEME_ON_PRIMARY]     = 0xFFFFFFFF;
    t->colors_light[APP_THEME_ON_SECONDARY]   = 0xFFFFFFFF;
    t->colors_light[APP_THEME_ON_BACKGROUND]  = 0xFF1C1B1F;
    t->colors_light[APP_THEME_ON_SURFACE]     = 0xFF1C1B1F;
    t->colors_light[APP_THEME_ON_ERROR]       = 0xFFFFFFFF;
    t->colors_light[APP_THEME_OUTLINE]        = 0xFF79747E;
}

static void init_default_dark(AppTheme* t) {
    t->colors_dark[APP_THEME_PRIMARY]         = 0xFFD0BCFF;
    t->colors_dark[APP_THEME_SECONDARY]       = 0xFFCCC2DC;
    t->colors_dark[APP_THEME_BACKGROUND]      = 0xFF1C1B1F;
    t->colors_dark[APP_THEME_SURFACE]         = 0xFF1C1B1F;
    t->colors_dark[APP_THEME_ERROR]           = 0xFFF2B8B5;
    t->colors_dark[APP_THEME_ON_PRIMARY]      = 0xFF381E72;
    t->colors_dark[APP_THEME_ON_SECONDARY]    = 0xFF332D41;
    t->colors_dark[APP_THEME_ON_BACKGROUND]   = 0xFFE6E1E5;
    t->colors_dark[APP_THEME_ON_SURFACE]      = 0xFFE6E1E5;
    t->colors_dark[APP_THEME_ON_ERROR]        = 0xFF601410;
    t->colors_dark[APP_THEME_OUTLINE]         = 0xFF938F99;
}

static void init_default_fonts(AppTheme* t) {
    t->fonts[APP_THEME_FONT_HEADLINE] = 32;
    t->fonts[APP_THEME_FONT_TITLE]    = 22;
    t->fonts[APP_THEME_FONT_BODY]     = 16;
    t->fonts[APP_THEME_FONT_LABEL]    = 14;
    t->fonts[APP_THEME_FONT_CAPTION]  = 12;
    t->spacings[0] = 4;
    t->spacings[1] = 8;
    t->spacings[2] = 16;
    t->spacings[3] = 24;
    t->spacings[4] = 48;
}

extern "C" {

void* aurora_app_theme_init(void) {
    auto* t = new AppTheme();
    t->mode = 0;
    init_default_light(t);
    init_default_dark(t);
    init_default_fonts(t);
    return t;
}

void aurora_app_theme_destroy(void* theme) {
    delete (AppTheme*)theme;
}

void aurora_app_theme_set_mode(void* theme, int mode) {
    if (theme) ((AppTheme*)theme)->mode = mode ? 1 : 0;
}

int aurora_app_theme_get_mode(void* theme) {
    return theme ? ((AppTheme*)theme)->mode : 0;
}

void aurora_app_theme_set_color(void* theme, int key, unsigned int color) {
    if (theme) {
        AppTheme* t = (AppTheme*)theme;
        if (t->mode)
            t->colors_dark[key] = color;
        else
            t->colors_light[key] = color;
    }
}

unsigned int aurora_app_theme_get_color(void* theme, int key) {
    if (!theme) return 0;
    AppTheme* t = (AppTheme*)theme;
    auto& map = t->mode ? t->colors_dark : t->colors_light;
    auto it = map.find(key);
    return it != map.end() ? it->second : 0;
}

void aurora_app_theme_set_font(void* theme, int key, int size) {
    if (theme) ((AppTheme*)theme)->fonts[key] = size;
}

int aurora_app_theme_get_font(void* theme, int key) {
    if (!theme) return 16;
    AppTheme* t = (AppTheme*)theme;
    auto it = t->fonts.find(key);
    return it != t->fonts.end() ? it->second : 16;
}

float aurora_app_theme_get_spacing(void* theme, int level) {
    if (!theme || level < 0 || level > 4) return 0;
    return ((AppTheme*)theme)->spacings[level];
}

} // extern "C"
