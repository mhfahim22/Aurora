/* ════════════════════════════════════════════════════════════
   canvas.cpp — 2D Graphics Engine (Win32 GDI)
   Shapes, paths, style, transform, text, images, double-buffering
   ════════════════════════════════════════════════════════════ */

#include "../../include/std/canvas.hpp"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifdef _WIN32

/* ════════════════════════════════════════════════════════════
   Internal structures
   ════════════════════════════════════════════════════════════ */

struct XForm {
    float dx, dy;
    float sx, sy;
    float angle; /* radians */
};

struct PathPoint {
    float x, y;
    int type; /* 0=move, 1=line, 2=bezier */
    float cp1x, cp1y, cp2x, cp2y;
};

struct AuroraCanvas {
    HWND hwnd;
    HDC  hdc;        /* window DC (used in end() ) */
    HDC  mem_dc;     /* off-screen DC for double buffering */
    HBITMAP mem_bmp; /* backing bitmap */
    int w, h;

    /* current style */
    COLORREF fill_color;
    COLORREF stroke_color;
    int stroke_width;
    int alpha; /* 0-255 */

    /* font */
    HFONT font;

    /* transform stack */
    XForm xform;
    std::vector<XForm> xform_stack;

    /* path */
    std::vector<PathPoint> path;
    bool in_path;

    /* gradient */
    bool use_gradient;
    float gx1, gy1, gx2, gy2;
    unsigned int gc1, gc2;

    /* shadow */
    bool use_shadow;
    float shadow_blur;
    float shadow_dx, shadow_dy;
    unsigned int shadow_color;

    /* memory for path rendering */
    std::vector<POINT> gdi_pts;
    std::vector<BYTE>  gdi_types;
};

/* ════════════════════════════════════════════════════════════
   Helper: apply current transform to point
   ════════════════════════════════════════════════════════════ */
static void apply_xform(AuroraCanvas* ctx, float& x, float& y) {
    XForm& t = ctx->xform;
    float rx = t.sx * x;
    float ry = t.sy * y;
    float c = (float)cos(t.angle);
    float s = (float)sin(t.angle);
    x = c * rx - s * ry + t.dx;
    y = s * rx + c * ry + t.dy;
}

/* ════════════════════════════════════════════════════════════
   Helper: set GDI pen/brush from current style
   ════════════════════════════════════════════════════════════ */
static HPEN make_pen(COLORREF color, int width) {
    return CreatePen(PS_SOLID, width, color);
}

static HBRUSH make_brush(COLORREF color) {
    return CreateSolidBrush(color);
}

/* ════════════════════════════════════════════════════════════
   Lifecycle
   ════════════════════════════════════════════════════════════ */
AuroraCanvas* aurora_canvas_create(AuroraWidget widget) {
    if (!widget) return nullptr;
    HWND hwnd = (HWND)aurora_gui_get_native_handle(widget);
    if (!hwnd) return nullptr;

    AuroraCanvas* ctx = new AuroraCanvas();
    ctx->hwnd = hwnd;
    ctx->hdc = nullptr;
    ctx->mem_dc = nullptr;
    ctx->mem_bmp = nullptr;
    ctx->w = 100;
    ctx->h = 100;

    ctx->fill_color = RGB(255, 255, 255);
    ctx->stroke_color = RGB(0, 0, 0);
    ctx->stroke_width = 1;
    ctx->alpha = 255;

    ctx->font = CreateFontA(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

    ctx->xform.dx = 0; ctx->xform.dy = 0;
    ctx->xform.sx = 1; ctx->xform.sy = 1;
    ctx->xform.angle = 0;

    ctx->in_path = false;
    ctx->use_gradient = false;
    ctx->use_shadow = false;

    /* Create double buffer */
    HDC dc = GetDC(ctx->hwnd);
    ctx->mem_dc = CreateCompatibleDC(dc);
    ctx->mem_bmp = CreateCompatibleBitmap(dc, ctx->w, ctx->h);
    SelectObject(ctx->mem_dc, ctx->mem_bmp);
    ReleaseDC(ctx->hwnd, dc);

    return ctx;
}

void aurora_canvas_destroy(AuroraCanvas* ctx) {
    if (!ctx) return;
    if (ctx->mem_bmp) DeleteObject(ctx->mem_bmp);
    if (ctx->mem_dc) DeleteDC(ctx->mem_dc);
    if (ctx->font) DeleteObject(ctx->font);
    delete ctx;
}

void aurora_canvas_begin(AuroraCanvas* ctx, unsigned int bg_color) {
    if (!ctx || !ctx->mem_dc) return;
    RECT r = { 0, 0, ctx->w, ctx->h };
    HBRUSH bg = CreateSolidBrush(RGB(GetRValue(bg_color), GetGValue(bg_color), GetBValue(bg_color)));
    FillRect(ctx->mem_dc, &r, bg);
    DeleteObject(bg);
    SetBkMode(ctx->mem_dc, TRANSPARENT);
    SetTextColor(ctx->mem_dc, ctx->stroke_color);
}

void aurora_canvas_end(AuroraCanvas* ctx) {
    if (!ctx || !ctx->hwnd || !ctx->mem_dc) return;
    HDC dc = GetDC(ctx->hwnd);
    BitBlt(dc, 0, 0, ctx->w, ctx->h, ctx->mem_dc, 0, 0, SRCCOPY);
    ReleaseDC(ctx->hwnd, dc);
}

void aurora_canvas_clear(AuroraCanvas* ctx, unsigned int color) {
    if (!ctx || !ctx->mem_dc) return;
    RECT r = { 0, 0, ctx->w, ctx->h };
    HBRUSH br = CreateSolidBrush(RGB(GetRValue(color), GetGValue(color), GetBValue(color)));
    FillRect(ctx->mem_dc, &r, br);
    DeleteObject(br);
}

void aurora_canvas_resize(AuroraCanvas* ctx, int w, int h) {
    if (!ctx) return;
    if (ctx->mem_bmp) DeleteObject(ctx->mem_bmp);
    HDC dc = GetDC(ctx->hwnd);
    ctx->mem_bmp = CreateCompatibleBitmap(dc, w, h);
    SelectObject(ctx->mem_dc, ctx->mem_bmp);
    ReleaseDC(ctx->hwnd, dc);
    ctx->w = w; ctx->h = h;
}

/* ════════════════════════════════════════════════════════════
   Helper: draw a shadow shape
   ════════════════════════════════════════════════════════════ */
static void prepare_shadow(AuroraCanvas* ctx) {
    if (!ctx->use_shadow) return;
    /* We apply shadow offset to existing transform later in each draw call */
}

static void do_shadow_fill(AuroraCanvas* ctx, HDC dc, const RECT* area) {
    if (!ctx->use_shadow) return;
    /* draw shadow: offset the area and fill with shadow color */
    int dx = (int)ctx->shadow_dx;
    int dy = (int)ctx->shadow_dy;
    RECT sr = { area->left + dx, area->top + dy, area->right + dx, area->bottom + dy };
    HBRUSH sb = CreateSolidBrush(RGB(GetRValue(ctx->shadow_color), GetGValue(ctx->shadow_color), GetBValue(ctx->shadow_color)));
    FillRect(dc, &sr, sb);
    DeleteObject(sb);
}

/* ════════════════════════════════════════════════════════════
   Shapes
   ════════════════════════════════════════════════════════════ */
void aurora_canvas_rect(AuroraCanvas* ctx, float x, float y, float w, float h) {
    if (!ctx || !ctx->mem_dc) return;
    apply_xform(ctx, x, y);
    RECT r = { (int)x, (int)y, (int)(x + w), (int)(y + h) };

    /* shadow */
    if (ctx->use_shadow) do_shadow_fill(ctx, ctx->mem_dc, &r);

    /* fill */
    HBRUSH fb = make_brush(ctx->fill_color);
    FillRect(ctx->mem_dc, &r, fb);
    DeleteObject(fb);

    /* stroke */
    HPEN p = make_pen(ctx->stroke_color, ctx->stroke_width);
    HPEN old_pen = (HPEN)SelectObject(ctx->mem_dc, p);
    HBRUSH old_br = (HBRUSH)SelectObject(ctx->mem_dc, GetStockObject(NULL_BRUSH));
    Rectangle(ctx->mem_dc, r.left, r.top, r.right, r.bottom);
    SelectObject(ctx->mem_dc, old_pen);
    SelectObject(ctx->mem_dc, old_br);
    DeleteObject(p);
}

void aurora_canvas_rounded_rect(AuroraCanvas* ctx, float x, float y, float w, float h, float r) {
    if (!ctx || !ctx->mem_dc) return;
    apply_xform(ctx, x, y);
    int ir = (int)r;

    /* shadow */
    RECT sr = { (int)x, (int)y, (int)(x + w), (int)(y + h) };
    if (ctx->use_shadow) do_shadow_fill(ctx, ctx->mem_dc, &sr);

    HPEN p = make_pen(ctx->stroke_color, ctx->stroke_width);
    HBRUSH fb = make_brush(ctx->fill_color);
    HPEN old_pen = (HPEN)SelectObject(ctx->mem_dc, p);
    HBRUSH old_br = (HBRUSH)SelectObject(ctx->mem_dc, fb);
    RoundRect(ctx->mem_dc, (int)x, (int)y, (int)(x + w), (int)(y + h), ir, ir);
    SelectObject(ctx->mem_dc, old_pen);
    SelectObject(ctx->mem_dc, old_br);
    DeleteObject(p); DeleteObject(fb);
}

void aurora_canvas_circle(AuroraCanvas* ctx, float cx, float cy, float r) {
    aurora_canvas_ellipse(ctx, cx, cy, r, r);
}

void aurora_canvas_ellipse(AuroraCanvas* ctx, float cx, float cy, float rx, float ry) {
    if (!ctx || !ctx->mem_dc) return;
    apply_xform(ctx, cx, cy);
    int l = (int)(cx - rx), t = (int)(cy - ry);
    int ri = (int)(cx + rx), bi = (int)(cy + ry);

    RECT sr = { l, t, ri, bi };
    if (ctx->use_shadow) do_shadow_fill(ctx, ctx->mem_dc, &sr);

    HPEN p = make_pen(ctx->stroke_color, ctx->stroke_width);
    HBRUSH fb = make_brush(ctx->fill_color);
    HPEN old_pen = (HPEN)SelectObject(ctx->mem_dc, p);
    HBRUSH old_br = (HBRUSH)SelectObject(ctx->mem_dc, fb);
    Ellipse(ctx->mem_dc, l, t, ri, bi);
    SelectObject(ctx->mem_dc, old_pen);
    SelectObject(ctx->mem_dc, old_br);
    DeleteObject(p); DeleteObject(fb);
}

void aurora_canvas_line(AuroraCanvas* ctx, float x1, float y1, float x2, float y2) {
    if (!ctx || !ctx->mem_dc) return;
    apply_xform(ctx, x1, y1);
    apply_xform(ctx, x2, y2);

    POINT pts[2] = { {(int)x1, (int)y1}, {(int)x2, (int)y2} };

    HPEN p = make_pen(ctx->stroke_color, ctx->stroke_width);
    HPEN old_pen = (HPEN)SelectObject(ctx->mem_dc, p);
    MoveToEx(ctx->mem_dc, pts[0].x, pts[0].y, nullptr);
    LineTo(ctx->mem_dc, pts[1].x, pts[1].y);
    SelectObject(ctx->mem_dc, old_pen);
    DeleteObject(p);
}

void aurora_canvas_polygon(AuroraCanvas* ctx, const float* points, int count) {
    if (!ctx || !ctx->mem_dc || !points || count < 3) return;
    std::vector<POINT> pts(count);
    for (int i = 0; i < count; i++) {
        float x = points[i * 2], y = points[i * 2 + 1];
        apply_xform(ctx, x, y);
        pts[i].x = (int)x; pts[i].y = (int)y;
    }

    HPEN p = make_pen(ctx->stroke_color, ctx->stroke_width);
    HBRUSH fb = make_brush(ctx->fill_color);
    HPEN old_pen = (HPEN)SelectObject(ctx->mem_dc, p);
    HBRUSH old_br = (HBRUSH)SelectObject(ctx->mem_dc, fb);
    Polygon(ctx->mem_dc, pts.data(), count);
    SelectObject(ctx->mem_dc, old_pen);
    SelectObject(ctx->mem_dc, old_br);
    DeleteObject(p); DeleteObject(fb);
}

void aurora_canvas_bezier(AuroraCanvas* ctx, float x1, float y1, float cx1, float cy1, float cx2, float cy2, float x2, float y2) {
    if (!ctx || !ctx->mem_dc) return;
    apply_xform(ctx, x1, y1); apply_xform(ctx, cx1, cy1);
    apply_xform(ctx, cx2, cy2); apply_xform(ctx, x2, y2);

    POINT pts[4] = { {(int)x1, (int)y1}, {(int)cx1, (int)cy1}, {(int)cx2, (int)cy2}, {(int)x2, (int)y2} };

    HPEN p = make_pen(ctx->stroke_color, ctx->stroke_width);
    HPEN old_pen = (HPEN)SelectObject(ctx->mem_dc, p);
    PolyBezier(ctx->mem_dc, pts, 4);
    SelectObject(ctx->mem_dc, old_pen);
    DeleteObject(p);
}

void aurora_canvas_arc(AuroraCanvas* ctx, float cx, float cy, float r, float start_angle, float end_angle) {
    if (!ctx || !ctx->mem_dc) return;
    apply_xform(ctx, cx, cy);
    int l = (int)(cx - r), t = (int)(cy - r);
    int ri = (int)(cx + r), bi = (int)(cy + r);

    HPEN p = make_pen(ctx->stroke_color, ctx->stroke_width);
    HPEN old_pen = (HPEN)SelectObject(ctx->mem_dc, p);
    HBRUSH old_br = (HBRUSH)SelectObject(ctx->mem_dc, GetStockObject(NULL_BRUSH));
    Arc(ctx->mem_dc, l, t, ri, bi, 
        (int)(cx + r * (float)cos(start_angle)), (int)(cy + r * (float)sin(start_angle)),
        (int)(cx + r * (float)cos(end_angle)), (int)(cy + r * (float)sin(end_angle)));
    SelectObject(ctx->mem_dc, old_pen);
    SelectObject(ctx->mem_dc, old_br);
    DeleteObject(p);
}

/* ════════════════════════════════════════════════════════════
   Style
   ════════════════════════════════════════════════════════════ */
void aurora_canvas_set_fill(AuroraCanvas* ctx, unsigned int color) {
    if (!ctx) return;
    ctx->fill_color = RGB(GetRValue(color), GetGValue(color), GetBValue(color));
}

void aurora_canvas_set_stroke(AuroraCanvas* ctx, unsigned int color) {
    if (!ctx) return;
    ctx->stroke_color = RGB(GetRValue(color), GetGValue(color), GetBValue(color));
}

void aurora_canvas_set_stroke_width(AuroraCanvas* ctx, float w) {
    if (!ctx) return;
    ctx->stroke_width = w > 0 ? (int)w : 1;
}

void aurora_canvas_set_alpha(AuroraCanvas* ctx, float a) {
    if (!ctx) return;
    ctx->alpha = (int)(a * 255.0f);
    if (ctx->alpha < 0) ctx->alpha = 0;
    if (ctx->alpha > 255) ctx->alpha = 255;
}

/* ════════════════════════════════════════════════════════════
   Transform
   ════════════════════════════════════════════════════════════ */
void aurora_canvas_translate(AuroraCanvas* ctx, float dx, float dy) {
    if (!ctx) return;
    ctx->xform.dx += dx;
    ctx->xform.dy += dy;
}

void aurora_canvas_rotate(AuroraCanvas* ctx, float angle) {
    if (!ctx) return;
    ctx->xform.angle += angle;
}

void aurora_canvas_scale(AuroraCanvas* ctx, float sx, float sy) {
    if (!ctx) return;
    ctx->xform.sx *= sx;
    ctx->xform.sy *= sy;
}

void aurora_canvas_save(AuroraCanvas* ctx) {
    if (!ctx) return;
    ctx->xform_stack.push_back(ctx->xform);
}

void aurora_canvas_restore(AuroraCanvas* ctx) {
    if (!ctx || ctx->xform_stack.empty()) return;
    ctx->xform = ctx->xform_stack.back();
    ctx->xform_stack.pop_back();
}

void aurora_canvas_reset_transform(AuroraCanvas* ctx) {
    if (!ctx) return;
    ctx->xform.dx = 0; ctx->xform.dy = 0;
    ctx->xform.sx = 1; ctx->xform.sy = 1;
    ctx->xform.angle = 0;
}

/* ════════════════════════════════════════════════════════════
   Path
   ════════════════════════════════════════════════════════════ */
void aurora_canvas_begin_path(AuroraCanvas* ctx) {
    if (!ctx) return;
    ctx->path.clear();
    ctx->in_path = true;
}

void aurora_canvas_move_to(AuroraCanvas* ctx, float x, float y) {
    if (!ctx) return;
    apply_xform(ctx, x, y);
    PathPoint pp; pp.x = x; pp.y = y; pp.type = 0;
    ctx->path.push_back(pp);
}

void aurora_canvas_line_to(AuroraCanvas* ctx, float x, float y) {
    if (!ctx) return;
    apply_xform(ctx, x, y);
    PathPoint pp; pp.x = x; pp.y = y; pp.type = 1;
    ctx->path.push_back(pp);
}

void aurora_canvas_bezier_to(AuroraCanvas* ctx, float cp1x, float cp1y, float cp2x, float cp2y, float x, float y) {
    if (!ctx) return;
    apply_xform(ctx, cp1x, cp1y); apply_xform(ctx, cp2x, cp2y);
    apply_xform(ctx, x, y);
    PathPoint pp; pp.x = x; pp.y = y; pp.type = 2;
    pp.cp1x = cp1x; pp.cp1y = cp1y; pp.cp2x = cp2x; pp.cp2y = cp2y;
    ctx->path.push_back(pp);
}

void aurora_canvas_close_path(AuroraCanvas* ctx) {
    if (!ctx || ctx->path.empty()) return;
    /* Add a line back to the first point if not already closed */
    const PathPoint& first = ctx->path[0];
    PathPoint pp; pp.x = first.x; pp.y = first.y; pp.type = 1;
    ctx->path.push_back(pp);
}

void aurora_canvas_stroke(AuroraCanvas* ctx) {
    if (!ctx || !ctx->mem_dc || ctx->path.size() < 2) return;

    HPEN p = make_pen(ctx->stroke_color, ctx->stroke_width);
    HPEN old_pen = (HPEN)SelectObject(ctx->mem_dc, p);

    BeginPath(ctx->mem_dc);
    MoveToEx(ctx->mem_dc, (int)ctx->path[0].x, (int)ctx->path[0].y, nullptr);
    for (size_t i = 1; i < ctx->path.size(); i++) {
        PathPoint& pt = ctx->path[i];
        if (pt.type == 1) {
            LineTo(ctx->mem_dc, (int)pt.x, (int)pt.y);
        } else if (pt.type == 2 && i > 0) {
            PathPoint& prev = ctx->path[i - 1];
            POINT cpts[3] = { {(int)pt.cp1x, (int)pt.cp1y}, {(int)pt.cp2x, (int)pt.cp2y}, {(int)pt.x, (int)pt.y} };
            PolyBezierTo(ctx->mem_dc, cpts, 3);
        }
    }
    EndPath(ctx->mem_dc);
    StrokePath(ctx->mem_dc);

    SelectObject(ctx->mem_dc, old_pen);
    DeleteObject(p);
}

void aurora_canvas_fill(AuroraCanvas* ctx) {
    if (!ctx || !ctx->mem_dc || ctx->path.size() < 2) return;

    HBRUSH fb = make_brush(ctx->fill_color);
    HBRUSH old_br = (HBRUSH)SelectObject(ctx->mem_dc, fb);

    BeginPath(ctx->mem_dc);
    MoveToEx(ctx->mem_dc, (int)ctx->path[0].x, (int)ctx->path[0].y, nullptr);
    for (size_t i = 1; i < ctx->path.size(); i++) {
        PathPoint& pt = ctx->path[i];
        if (pt.type == 1) {
            LineTo(ctx->mem_dc, (int)pt.x, (int)pt.y);
        } else if (pt.type == 2 && i > 0) {
            PathPoint& prev = ctx->path[i - 1];
            POINT cpts[3] = { {(int)pt.cp1x, (int)pt.cp1y}, {(int)pt.cp2x, (int)pt.cp2y}, {(int)pt.x, (int)pt.y} };
            PolyBezierTo(ctx->mem_dc, cpts, 3);
        }
    }
    CloseFigure(ctx->mem_dc);
    EndPath(ctx->mem_dc);
    FillPath(ctx->mem_dc);

    SelectObject(ctx->mem_dc, old_br);
    DeleteObject(fb);
}

/* ════════════════════════════════════════════════════════════
   Text
   ════════════════════════════════════════════════════════════ */
void aurora_canvas_set_font(AuroraCanvas* ctx, const char* name, float size) {
    if (!ctx || !name) return;
    if (ctx->font) DeleteObject(ctx->font);
    ctx->font = CreateFontA(-(int)size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, name);
}

void aurora_canvas_draw_text(AuroraCanvas* ctx, const char* text, float x, float y) {
    if (!ctx || !ctx->mem_dc || !text) return;
    apply_xform(ctx, x, y);
    HFONT old_font = (HFONT)SelectObject(ctx->mem_dc, ctx->font);
    SetTextColor(ctx->mem_dc, ctx->stroke_color);
    TextOutA(ctx->mem_dc, (int)x, (int)y, text, (int)strlen(text));
    SelectObject(ctx->mem_dc, old_font);
}

void aurora_canvas_measure_text(AuroraCanvas* ctx, const char* text, float* out_w, float* out_h) {
    if (!ctx || !ctx->mem_dc) { if (out_w) *out_w = 0; if (out_h) *out_h = 0; return; }
    SIZE sz = {0};
    HFONT old_font = (HFONT)SelectObject(ctx->mem_dc, ctx->font);
    GetTextExtentPoint32A(ctx->mem_dc, text ? text : "", text ? (int)strlen(text) : 0, &sz);
    SelectObject(ctx->mem_dc, old_font);
    if (out_w) *out_w = (float)sz.cx;
    if (out_h) *out_h = (float)sz.cy;
}

/* ════════════════════════════════════════════════════════════
   Image
   ════════════════════════════════════════════════════════════ */
void aurora_canvas_draw_image(AuroraCanvas* ctx, const char* path, float x, float y, float w, float h) {
    if (!ctx || !ctx->mem_dc || !path) return;
    apply_xform(ctx, x, y);

    HBITMAP hbm = (HBITMAP)LoadImageA(nullptr, path, IMAGE_BITMAP, (int)w, (int)h, LR_LOADFROMFILE);
    if (!hbm) return;

    HDC img_dc = CreateCompatibleDC(ctx->mem_dc);
    HBITMAP old_bmp = (HBITMAP)SelectObject(img_dc, hbm);
    BITMAP bm; GetObject(hbm, sizeof(bm), &bm);

    HBRUSH fb = make_brush(ctx->fill_color);
    HBRUSH old_br = (HBRUSH)SelectObject(ctx->mem_dc, fb);

    /* shadow */
    if (ctx->use_shadow) {
        RECT sr = { (int)x + (int)ctx->shadow_dx, (int)y + (int)ctx->shadow_dy,
                    (int)(x + w) + (int)ctx->shadow_dx, (int)(y + h) + (int)ctx->shadow_dy };
        HBRUSH sb = CreateSolidBrush(RGB(GetRValue(ctx->shadow_color), GetGValue(ctx->shadow_color), GetBValue(ctx->shadow_color)));
        FillRect(ctx->mem_dc, &sr, sb);
        DeleteObject(sb);
    }

    StretchBlt(ctx->mem_dc, (int)x, (int)y, (int)w, (int)h, img_dc, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);

    SelectObject(ctx->mem_dc, old_br);
    SelectObject(img_dc, old_bmp);
    DeleteDC(img_dc);
    DeleteObject(fb);
    DeleteObject(hbm);
}

void aurora_canvas_draw_rgba(AuroraCanvas* ctx, const void* rgba, int img_w, int img_h, float x, float y, float w, float h) {
    if (!ctx || !ctx->mem_dc || !rgba || img_w <= 0 || img_h <= 0) return;
    apply_xform(ctx, x, y);

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize   = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth  = img_w;
    bmi.bmiHeader.biHeight = -img_h;  /* top-down */
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HBRUSH fb = make_brush(ctx->fill_color);
    HBRUSH old_br = (HBRUSH)SelectObject(ctx->mem_dc, fb);

    if (ctx->use_shadow) {
        RECT sr = { (int)x + (int)ctx->shadow_dx, (int)y + (int)ctx->shadow_dy,
                    (int)(x + w) + (int)ctx->shadow_dx, (int)(y + h) + (int)ctx->shadow_dy };
        HBRUSH sb = CreateSolidBrush(RGB(GetRValue(ctx->shadow_color), GetGValue(ctx->shadow_color), GetBValue(ctx->shadow_color)));
        FillRect(ctx->mem_dc, &sr, sb);
        DeleteObject(sb);
    }

    StretchDIBits(ctx->mem_dc, (int)x, (int)y, (int)w, (int)h,
                  0, 0, img_w, img_h, rgba, &bmi, DIB_RGB_COLORS, SRCCOPY);

    SelectObject(ctx->mem_dc, old_br);
    DeleteObject(fb);
}

/* ════════════════════════════════════════════════════════════
   Gradient
   ════════════════════════════════════════════════════════════ */
void aurora_canvas_set_linear_gradient(AuroraCanvas* ctx, float x1, float y1, float x2, float y2, unsigned int c1, unsigned int c2) {
    if (!ctx) return;
    ctx->use_gradient = true;
    ctx->gx1 = x1; ctx->gy1 = y1; ctx->gx2 = x2; ctx->gy2 = y2;
    ctx->gc1 = c1; ctx->gc2 = c2;
}

void aurora_canvas_clear_gradient(AuroraCanvas* ctx) {
    if (!ctx) return;
    ctx->use_gradient = false;
}

/* ════════════════════════════════════════════════════════════
   Shadow
   ════════════════════════════════════════════════════════════ */
void aurora_canvas_set_shadow(AuroraCanvas* ctx, float blur, float dx, float dy, unsigned int color) {
    if (!ctx) return;
    ctx->use_shadow = true;
    ctx->shadow_blur = blur;
    ctx->shadow_dx = dx;
    ctx->shadow_dy = dy;
    ctx->shadow_color = color;
}

void aurora_canvas_clear_shadow(AuroraCanvas* ctx) {
    if (!ctx) return;
    ctx->use_shadow = false;
}

#else  // !_WIN32

AuroraCanvas* aurora_canvas_create(AuroraWidget widget) { (void)widget; return nullptr; }
void aurora_canvas_destroy(AuroraCanvas* ctx) { (void)ctx; }
void aurora_canvas_begin(AuroraCanvas* ctx, unsigned int bg) { (void)ctx; (void)bg; }
void aurora_canvas_end(AuroraCanvas* ctx) { (void)ctx; }
void aurora_canvas_clear(AuroraCanvas* ctx, unsigned int c) { (void)ctx; (void)c; }
void aurora_canvas_resize(AuroraCanvas* ctx, int w, int h) { (void)ctx; (void)w; (void)h; }
void aurora_canvas_rect(AuroraCanvas* ctx, float x, float y, float w, float h) { (void)ctx; (void)x; (void)y; (void)w; (void)h; }
void aurora_canvas_rounded_rect(AuroraCanvas* ctx, float x, float y, float w, float h, float r) { (void)ctx; (void)x; (void)y; (void)w; (void)h; (void)r; }
void aurora_canvas_circle(AuroraCanvas* ctx, float cx, float cy, float r) { (void)ctx; (void)cx; (void)cy; (void)r; }
void aurora_canvas_ellipse(AuroraCanvas* ctx, float cx, float cy, float rx, float ry) { (void)ctx; (void)cx; (void)cy; (void)rx; (void)ry; }
void aurora_canvas_line(AuroraCanvas* ctx, float x1, float y1, float x2, float y2) { (void)ctx; (void)x1; (void)y1; (void)x2; (void)y2; }
void aurora_canvas_polygon(AuroraCanvas* ctx, const float* pts, int n) { (void)ctx; (void)pts; (void)n; }
void aurora_canvas_bezier(AuroraCanvas* ctx, float x1, float y1, float cx1, float cy1, float cx2, float cy2, float x2, float y2) { (void)ctx; (void)x1; (void)y1; (void)cx1; (void)cy1; (void)cx2; (void)cy2; (void)x2; (void)y2; }
void aurora_canvas_arc(AuroraCanvas* ctx, float cx, float cy, float r, float sa, float ea) { (void)ctx; (void)cx; (void)cy; (void)r; (void)sa; (void)ea; }
void aurora_canvas_set_fill(AuroraCanvas* ctx, unsigned int c) { (void)ctx; (void)c; }
void aurora_canvas_set_stroke(AuroraCanvas* ctx, unsigned int c) { (void)ctx; (void)c; }
void aurora_canvas_set_stroke_width(AuroraCanvas* ctx, float w) { (void)ctx; (void)w; }
void aurora_canvas_set_alpha(AuroraCanvas* ctx, float a) { (void)ctx; (void)a; }
void aurora_canvas_translate(AuroraCanvas* ctx, float dx, float dy) { (void)ctx; (void)dx; (void)dy; }
void aurora_canvas_rotate(AuroraCanvas* ctx, float a) { (void)ctx; (void)a; }
void aurora_canvas_scale(AuroraCanvas* ctx, float sx, float sy) { (void)ctx; (void)sx; (void)sy; }
void aurora_canvas_save(AuroraCanvas* ctx) { (void)ctx; }
void aurora_canvas_restore(AuroraCanvas* ctx) { (void)ctx; }
void aurora_canvas_reset_transform(AuroraCanvas* ctx) { (void)ctx; }
void aurora_canvas_begin_path(AuroraCanvas* ctx) { (void)ctx; }
void aurora_canvas_move_to(AuroraCanvas* ctx, float x, float y) { (void)ctx; (void)x; (void)y; }
void aurora_canvas_line_to(AuroraCanvas* ctx, float x, float y) { (void)ctx; (void)x; (void)y; }
void aurora_canvas_bezier_to(AuroraCanvas* ctx, float cp1x, float cp1y, float cp2x, float cp2y, float x, float y) { (void)ctx; (void)cp1x; (void)cp1y; (void)cp2x; (void)cp2y; (void)x; (void)y; }
void aurora_canvas_close_path(AuroraCanvas* ctx) { (void)ctx; }
void aurora_canvas_stroke(AuroraCanvas* ctx) { (void)ctx; }
void aurora_canvas_fill(AuroraCanvas* ctx) { (void)ctx; }
void aurora_canvas_set_font(AuroraCanvas* ctx, const char* name, float sz) { (void)ctx; (void)name; (void)sz; }
void aurora_canvas_draw_text(AuroraCanvas* ctx, const char* txt, float x, float y) { (void)ctx; (void)txt; (void)x; (void)y; }
void aurora_canvas_measure_text(AuroraCanvas* ctx, const char* txt, float* out_w, float* out_h) { (void)ctx; (void)txt; if (out_w) *out_w = 0; if (out_h) *out_h = 0; }
void aurora_canvas_draw_image(AuroraCanvas* ctx, const char* p, float x, float y, float w, float h) { (void)ctx; (void)p; (void)x; (void)y; (void)w; (void)h; }
void aurora_canvas_draw_rgba(AuroraCanvas* ctx, const void* rgba, int iw, int ih, float x, float y, float w, float h) { (void)ctx; (void)rgba; (void)iw; (void)ih; (void)x; (void)y; (void)w; (void)h; }
void aurora_canvas_set_linear_gradient(AuroraCanvas* ctx, float x1, float y1, float x2, float y2, unsigned int c1, unsigned int c2) { (void)ctx; (void)x1; (void)y1; (void)x2; (void)y2; (void)c1; (void)c2; }
void aurora_canvas_clear_gradient(AuroraCanvas* ctx) { (void)ctx; }
void aurora_canvas_set_shadow(AuroraCanvas* ctx, float bl, float dx, float dy, unsigned int c) { (void)ctx; (void)bl; (void)dx; (void)dy; (void)c; }
void aurora_canvas_clear_shadow(AuroraCanvas* ctx) { (void)ctx; }

#endif  // _WIN32
