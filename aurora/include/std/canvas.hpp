#pragma once
#include <cstdint>
#include "common/platform.hpp"
#include "gui.hpp"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Opaque type ── */
typedef struct AuroraCanvas AuroraCanvas;

/* ════════════════════════════════════════════════════════════
   Lifecycle
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraCanvas* aurora_canvas_create(AuroraWidget widget);
AURORA_EXPORT void          aurora_canvas_destroy(AuroraCanvas* ctx);
AURORA_EXPORT void          aurora_canvas_begin(AuroraCanvas* ctx, unsigned int bg_color);
AURORA_EXPORT void          aurora_canvas_end(AuroraCanvas* ctx);
AURORA_EXPORT void          aurora_canvas_clear(AuroraCanvas* ctx, unsigned int color);
AURORA_EXPORT void          aurora_canvas_resize(AuroraCanvas* ctx, int w, int h);

/* ════════════════════════════════════════════════════════════
   Shapes
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT void aurora_canvas_rect(AuroraCanvas* ctx, float x, float y, float w, float h);
AURORA_EXPORT void aurora_canvas_rounded_rect(AuroraCanvas* ctx, float x, float y, float w, float h, float r);
AURORA_EXPORT void aurora_canvas_circle(AuroraCanvas* ctx, float cx, float cy, float r);
AURORA_EXPORT void aurora_canvas_ellipse(AuroraCanvas* ctx, float cx, float cy, float rx, float ry);
AURORA_EXPORT void aurora_canvas_line(AuroraCanvas* ctx, float x1, float y1, float x2, float y2);
AURORA_EXPORT void aurora_canvas_polygon(AuroraCanvas* ctx, const float* points, int count);
AURORA_EXPORT void aurora_canvas_bezier(AuroraCanvas* ctx, float x1, float y1, float cx1, float cy1, float cx2, float cy2, float x2, float y2);
AURORA_EXPORT void aurora_canvas_arc(AuroraCanvas* ctx, float cx, float cy, float r, float start_angle, float end_angle);

/* ════════════════════════════════════════════════════════════
   Style
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT void aurora_canvas_set_fill(AuroraCanvas* ctx, unsigned int color);
AURORA_EXPORT void aurora_canvas_set_stroke(AuroraCanvas* ctx, unsigned int color);
AURORA_EXPORT void aurora_canvas_set_stroke_width(AuroraCanvas* ctx, float w);
AURORA_EXPORT void aurora_canvas_set_alpha(AuroraCanvas* ctx, float a);

/* ════════════════════════════════════════════════════════════
   Transform
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT void aurora_canvas_translate(AuroraCanvas* ctx, float dx, float dy);
AURORA_EXPORT void aurora_canvas_rotate(AuroraCanvas* ctx, float angle);
AURORA_EXPORT void aurora_canvas_scale(AuroraCanvas* ctx, float sx, float sy);
AURORA_EXPORT void aurora_canvas_save(AuroraCanvas* ctx);
AURORA_EXPORT void aurora_canvas_restore(AuroraCanvas* ctx);
AURORA_EXPORT void aurora_canvas_reset_transform(AuroraCanvas* ctx);

/* ════════════════════════════════════════════════════════════
   Path
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT void aurora_canvas_begin_path(AuroraCanvas* ctx);
AURORA_EXPORT void aurora_canvas_move_to(AuroraCanvas* ctx, float x, float y);
AURORA_EXPORT void aurora_canvas_line_to(AuroraCanvas* ctx, float x, float y);
AURORA_EXPORT void aurora_canvas_bezier_to(AuroraCanvas* ctx, float cp1x, float cp1y, float cp2x, float cp2y, float x, float y);
AURORA_EXPORT void aurora_canvas_close_path(AuroraCanvas* ctx);
AURORA_EXPORT void aurora_canvas_stroke(AuroraCanvas* ctx);
AURORA_EXPORT void aurora_canvas_fill(AuroraCanvas* ctx);

/* ════════════════════════════════════════════════════════════
   Text
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT void aurora_canvas_set_font(AuroraCanvas* ctx, const char* name, float size);
AURORA_EXPORT void aurora_canvas_draw_text(AuroraCanvas* ctx, const char* text, float x, float y);
AURORA_EXPORT void aurora_canvas_measure_text(AuroraCanvas* ctx, const char* text, float* out_w, float* out_h);

/* ════════════════════════════════════════════════════════════
   Image
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT void aurora_canvas_draw_image(AuroraCanvas* ctx, const char* path, float x, float y, float w, float h);
AURORA_EXPORT void aurora_canvas_draw_rgba(AuroraCanvas* ctx, const void* rgba, int img_w, int img_h, float x, float y, float w, float h);

/* ════════════════════════════════════════════════════════════
   Gradient
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT void aurora_canvas_set_linear_gradient(AuroraCanvas* ctx, float x1, float y1, float x2, float y2, unsigned int c1, unsigned int c2);
AURORA_EXPORT void aurora_canvas_clear_gradient(AuroraCanvas* ctx);

/* ════════════════════════════════════════════════════════════
   Shadow
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT void aurora_canvas_set_shadow(AuroraCanvas* ctx, float blur, float dx, float dy, unsigned int color);
AURORA_EXPORT void aurora_canvas_clear_shadow(AuroraCanvas* ctx);

#ifdef __cplusplus
}
#endif
