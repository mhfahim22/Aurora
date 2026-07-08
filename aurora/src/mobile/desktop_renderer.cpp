#include "mobile/widgets.hpp"
#include <cstdio>
#include <cmath>
#include <cstring>

/* ════════════════════════════════════════════════════════════
   Desktop mobile widget emulator (Phase 7)
   Renders MwWidget tree onto a desktop GDI device context.

   Usage (Win32):
       HDC hdc = GetDC(hwnd);
       mw_desktop_set_hdc(hdc);
       mw_render(root_widget);
       ReleaseDC(hwnd, hdc);

   On non-Windows platforms, provides stubs.
   ════════════════════════════════════════════════════════════ */

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static HDC g_render_dc = nullptr;

void mw_desktop_set_hdc(void* hdc) {
    g_render_dc = (HDC)hdc;
}

void* mw_desktop_get_hdc(void) {
    return (void*)g_render_dc;
}

static void draw_rect(HDC dc, float x, float y, float w, float h, int color, int filled) {
    RECT r = { (int)x, (int)y, (int)(x + w), (int)(y + h) };
    if (filled) {
        HBRUSH brush = CreateSolidBrush((COLORREF)color);
        FillRect(dc, &r, brush);
        DeleteObject(brush);
    } else {
        HPEN pen = CreatePen(PS_SOLID, 1, (COLORREF)color);
        HGDIOBJ old_pen = SelectObject(dc, pen);
        HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        Rectangle(dc, r.left, r.top, r.right, r.bottom);
        SelectObject(dc, old_pen);
        SelectObject(dc, old_brush);
        DeleteObject(pen);
    }
}

static void draw_round_rect(HDC dc, float x, float y, float w, float h, float r, int color, int filled) {
    int ix = (int)x, iy = (int)y, iw = (int)(x + w), ih = (int)(y + h);
    int ir = (int)r;
    HBRUSH brush = filled ? CreateSolidBrush((COLORREF)color) : (HBRUSH)GetStockObject(NULL_BRUSH);
    HPEN pen = CreatePen(PS_SOLID, 1, (COLORREF)color);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, brush);
    RoundRect(dc, ix, iy, iw, ih, ir * 2, ir * 2);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(pen);
    if (filled) DeleteObject(brush);
}

static void draw_circle(HDC dc, float cx, float cy, float r, int color, int filled) {
    int ix = (int)(cx - r), iy = (int)(cy - r);
    int iw = (int)(r * 2), ih = (int)(r * 2);
    HBRUSH brush = filled ? CreateSolidBrush((COLORREF)color) : (HBRUSH)GetStockObject(NULL_BRUSH);
    HPEN pen = CreatePen(PS_SOLID, 1, (COLORREF)color);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, brush);
    Ellipse(dc, ix, iy, ix + iw, iy + ih);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(pen);
    if (filled) DeleteObject(brush);
}

static void draw_line(HDC dc, float x1, float y1, float x2, float y2, int color) {
    HPEN pen = CreatePen(PS_SOLID, 1, (COLORREF)color);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    MoveToEx(dc, (int)x1, (int)y1, nullptr);
    LineTo(dc, (int)x2, (int)y2);
    SelectObject(dc, old_pen);
    DeleteObject(pen);
}

static void draw_text(HDC dc, float x, float y, const char* text, int color, float font_size) {
    if (!text || !text[0]) return;
    SetTextColor(dc, (COLORREF)color);
    SetBkMode(dc, TRANSPARENT);
    HFONT font = CreateFontA((int)font_size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    HGDIOBJ old_font = SelectObject(dc, font);
    TextOutA(dc, (int)x, (int)y, text, (int)strlen(text));
    SelectObject(dc, old_font);
    DeleteObject(font);
}

static void draw_text_centered(HDC dc, float x, float y, float w, float h,
    const char* text, int color, float font_size) {
    if (!text || !text[0]) return;
    SetTextColor(dc, (COLORREF)color);
    SetBkMode(dc, TRANSPARENT);
    HFONT font = CreateFontA((int)font_size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    HGDIOBJ old_font = SelectObject(dc, font);
    RECT r = { (int)x, (int)y, (int)(x + w), (int)(y + h) };
    DrawTextA(dc, text, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, old_font);
    DeleteObject(font);
}

static void save_clip(HDC dc) { SaveDC(dc); }
static void restore_clip(HDC dc) { RestoreDC(dc, -1); }

static void clip_rect(HDC dc, float x, float y, float w, float h) {
    HRGN rgn = CreateRectRgn((int)x, (int)y, (int)(x + w), (int)(y + h));
    SelectClipRgn(dc, rgn);
    DeleteObject(rgn);
}

static void render_widget_desktop(MwWidget* w, float offset_x, float offset_y) {
    if (!w || !w->visible) return;
    HDC dc = g_render_dc;
    if (!dc) return;

    float abs_x = w->x + offset_x;
    float abs_y = w->y + offset_y;

    int need_clip = (w->type == MW_SCROLL);
    if (need_clip) save_clip(dc);

    if (w->bg_color[3] > 0) {
        int a = (int)(w->bg_color[3] * 255.0f);
        if (a >= 128) {
            int bg = RGB(
                (int)(w->bg_color[0] * 255.0f),
                (int)(w->bg_color[1] * 255.0f),
                (int)(w->bg_color[2] * 255.0f));
            switch (w->type) {
                case MW_BUTTON:
                case MW_FAB:
                    draw_round_rect(dc, abs_x, abs_y, w->w, w->h,
                        w->h < 30 ? w->h / 4 : 8, bg, 1);
                    break;
                case MW_IMAGE: case MW_DIALOG: case MW_DRAWER:
                case MW_BOTTOM_SHEET: case MW_NAV_BAR: case MW_TAB_BAR:
                case MW_SNACKBAR:
                    draw_rect(dc, abs_x, abs_y, w->w, w->h, bg, 1);
                    break;
                default: break;
            }
        }
    }

    switch (w->type) {
        case MW_BUTTON: {
            int tc = RGB((int)(w->text_color[0]*255), (int)(w->text_color[1]*255), (int)(w->text_color[2]*255));
            draw_text_centered(dc, abs_x, abs_y, w->w, w->h, w->text, tc, w->font_size);
            break;
        }
        case MW_TEXT: {
            int tc = RGB((int)(w->text_color[0]*255), (int)(w->text_color[1]*255), (int)(w->text_color[2]*255));
            draw_text(dc, abs_x, abs_y, w->text, tc, w->font_size);
            break;
        }
        case MW_INPUT: {
            draw_rect(dc, abs_x, abs_y, w->w, w->h, RGB(255,255,255), 1);
            draw_rect(dc, abs_x, abs_y, w->w, w->h, RGB(180,180,180), 0);
            if (w->text && w->text[0])
                draw_text(dc, abs_x+4, abs_y + w->h/2 - w->font_size/3, w->text, RGB(0,0,0), w->font_size);
            break;
        }
        case MW_IMAGE: {
            draw_rect(dc, abs_x, abs_y, w->w, w->h, RGB(200,200,200), 1);
            draw_line(dc, abs_x, abs_y, abs_x+w->w, abs_y+w->h, RGB(128,128,128));
            draw_line(dc, abs_x+w->w, abs_y, abs_x, abs_y+w->h, RGB(128,128,128));
            break;
        }
        case MW_LIST: {
            float rh = 40;
            for (int i = 0; i < w->item_count; i++) {
                float ry = abs_y + i*rh;
                if (i%2==1) draw_rect(dc, abs_x, ry, w->w, rh, RGB(245,245,245), 1);
                if (w->items[i]) draw_text(dc, abs_x+8, ry+rh/2-7, w->items[i], RGB(0,0,0), 14);
                draw_line(dc, abs_x, ry+rh, abs_x+w->w, ry+rh, RGB(200,200,200));
            }
            break;
        }
        case MW_GRID: {
            int cols = w->selected_index > 0 ? w->selected_index : 2;
            if (cols < 1) cols = 1;
            float cw = w->w / cols;
            int rows = (w->child_count + cols - 1) / cols;
            if (rows < 1) rows = 1;
            float rh2 = w->h / rows;
            int gc = RGB(200,200,200);
            for (int c = 1; c < cols; c++)
                draw_line(dc, abs_x + c*cw, abs_y, abs_x + c*cw, abs_y + w->h, gc);
            for (int r = 1; r < rows; r++)
                draw_line(dc, abs_x, abs_y + r*rh2, abs_x + w->w, abs_y + r*rh2, gc);
            break;
        }
        case MW_SCROLL: {
            clip_rect(dc, abs_x, abs_y, w->w, w->h);
            if (w->child_count > 0) {
                float ch = w->child_count * 60;
                float vr = w->h / ch;
                if (vr < 1.0f) {
                    float sb_h = w->h * vr;
                    float sb_y = abs_y + (w->scroll_y/(ch-w->h))*(w->h-sb_h);
                    draw_round_rect(dc, abs_x+w->w-6, sb_y, 4, sb_h, 2, RGB(128,128,128), 1);
                }
            }
            break;
        }
        case MW_SLIDER: {
            float ty = abs_y + w->h/2 - 2;
            draw_round_rect(dc, abs_x, ty, w->w, 4, 2, RGB(180,180,180), 1);
            float fill = (w->value > 0 && w->value <= 100) ? w->value/100.0f : 0.5f;
            if (fill > 0) draw_round_rect(dc, abs_x, ty, w->w*fill, 4, 2, RGB(76,153,255), 1);
            draw_circle(dc, abs_x+w->w*fill, abs_y+w->h/2, 8, RGB(255,255,255), 1);
            draw_circle(dc, abs_x+w->w*fill, abs_y+w->h/2, 5, RGB(76,153,255), 1);
            break;
        }
        case MW_SWITCH: {
            float tr = w->h/2;
            int track = w->value > 0 ? RGB(76,191,76) : RGB(153,153,153);
            draw_round_rect(dc, abs_x, abs_y, w->w, w->h, tr, track, 1);
            float kx = w->value > 0 ? abs_x + w->w - w->h : abs_x;
            draw_circle(dc, kx + tr, abs_y + tr, tr-2, RGB(255,255,255), 1);
            break;
        }
        case MW_CHECKBOX: {
            float box = w->h < 24 ? w->h : 24;
            float bx = abs_x, by = abs_y + (w->h-box)/2;
            draw_round_rect(dc, bx, by, box, box, 3, RGB(128,128,128), 0);
            if (w->value > 0) {
                draw_round_rect(dc, bx+2, by+2, box-4, box-4, 2, RGB(76,153,255), 1);
                draw_line(dc, bx+4, by+box/2, bx+box/2-2, by+box-5, RGB(255,255,255));
                draw_line(dc, bx+box/2-2, by+box-5, bx+box-4, by+4, RGB(255,255,255));
            }
            if (w->text && w->text[0]) {
                int tc = RGB((int)(w->text_color[0]*255), (int)(w->text_color[1]*255), (int)(w->text_color[2]*255));
                draw_text(dc, bx+box+6, abs_y + w->h/2 - w->font_size/3, w->text, tc, w->font_size);
            }
            break;
        }
        case MW_RADIO: {
            float rs = w->h < 24 ? w->h : 24;
            float cx = abs_x + rs/2, cy = abs_y + w->h/2;
            draw_circle(dc, cx, cy, rs/2, RGB(128,128,128), 0);
            draw_circle(dc, cx, cy, rs/2-2, RGB(255,255,255), 1);
            if (w->value > 0) draw_circle(dc, cx, cy, rs/2-4, RGB(76,153,255), 1);
            if (w->text && w->text[0]) {
                int tc = RGB((int)(w->text_color[0]*255), (int)(w->text_color[1]*255), (int)(w->text_color[2]*255));
                draw_text(dc, cx+rs/2+6, abs_y + w->h/2 - w->font_size/3, w->text, tc, w->font_size);
            }
            break;
        }
        case MW_PROGRESS: {
            float py = abs_y + w->h/2 - 4;
            draw_round_rect(dc, abs_x, py, w->w, 8, 4, RGB(200,200,200), 1);
            float pct = w->value > 0 ? w->value/100.0f : 0.0f;
            if (pct > 0) draw_round_rect(dc, abs_x+1, py+1, (w->w-2)*pct, 6, 3, RGB(76,153,255), 1);
            break;
        }
        case MW_DIALOG: {
            draw_rect(dc, (int)offset_x, (int)offset_y, 10000, 10000, RGB(0,0,0), 1);
            float dw = w->w > 0 ? w->w : 300;
            float dh = w->h > 0 ? w->h : 200;
            draw_round_rect(dc, abs_x, abs_y, dw, dh, 12, RGB(255,255,255), 1);
            if (w->text && w->text[0])
                draw_text(dc, abs_x+16, abs_y+16, w->text, RGB(0,0,0), w->font_size+4);
            break;
        }
        case MW_SNACKBAR: {
            float sbr = w->h/2;
            draw_round_rect(dc, abs_x, abs_y, w->w, w->h, sbr, RGB(51,51,51), 1);
            draw_text_centered(dc, abs_x+16, abs_y, w->w-32, w->h, w->text, RGB(255,255,255), 14);
            break;
        }
        case MW_BOTTOM_SHEET: {
            draw_rect(dc, (int)offset_x, (int)offset_y, 10000, 10000, RGB(0,0,0), 1);
            draw_round_rect(dc, abs_x, abs_y, w->w, w->h, 16, RGB(255,255,255), 1);
            break;
        }
        case MW_NAV_BAR: {
            draw_rect(dc, abs_x, abs_y, w->w, w->h, RGB(51,51,51), 1);
            if (w->text && w->text[0])
                draw_text_centered(dc, abs_x, abs_y, w->w, w->h, w->text, RGB(255,255,255), 18);
            break;
        }
        case MW_TAB_BAR: {
            int tc2 = w->item_count > 0 ? w->item_count : 1;
            float tw = w->w / tc2;
            draw_rect(dc, abs_x, abs_y, w->w, w->h, RGB(240,240,240), 1);
            draw_line(dc, abs_x, abs_y, abs_x+w->w, abs_y, RGB(200,200,200));
            for (int i = 0; i < tc2; i++) {
                const char* lb = (i < w->item_count && w->items[i]) ? w->items[i] : "Tab";
                draw_text_centered(dc, abs_x+i*tw, abs_y, tw, w->h, lb, RGB(0,0,0), 12);
                if (i == w->selected_index)
                    draw_rect(dc, abs_x+i*tw+8, abs_y+w->h-3, tw-16, 3, RGB(76,153,255), 1);
            }
            break;
        }
        case MW_DRAWER: {
            draw_rect(dc, (int)offset_x, (int)offset_y, 10000, 10000, RGB(0,0,0), 1);
            draw_rect(dc, abs_x, abs_y, w->w, w->h, RGB(255,255,255), 1);
            break;
        }
        case MW_COLUMN: case MW_ROW: default: break;
    }

    for (int i = 0; i < w->child_count; i++)
        render_widget_desktop((MwWidget*)w->children[i], abs_x, abs_y);

    if (need_clip) restore_clip(dc);
}

void mw_desktop_render(void* widget) {
    if (!widget || !g_render_dc) return;
    render_widget_desktop((MwWidget*)widget, 0, 0);
}

#else /* !_WIN32 */

void mw_desktop_set_hdc(void* hdc) { (void)hdc; }
void* mw_desktop_get_hdc(void) { return nullptr; }
void mw_desktop_render(void* widget) { (void)widget; }

#endif /* _WIN32 */
