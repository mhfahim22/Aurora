#include "mobile/widgets.hpp"
#include "mobile/android.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <jni.h>

/* ════════════════════════════════════════════════════════════
   Android Canvas renderer for mobile widgets (Phase 7)
   Walks the MwWidget tree and issues Canvas draw commands
   for all 16 widget types.
   ════════════════════════════════════════════════════════════ */

/* Cached JNI method IDs */
static JavaVM*    g_vm   = nullptr;
static jclass    g_canvas_class = nullptr;
static jmethodID g_draw_rect      = nullptr;
static jmethodID g_draw_text      = nullptr;
static jmethodID g_draw_round_rect = nullptr;
static jmethodID g_draw_circle    = nullptr;
static jmethodID g_draw_line      = nullptr;
static jmethodID g_draw_oval      = nullptr;
static jmethodID g_draw_arc       = nullptr;
static jmethodID g_draw_bitmap    = nullptr;
static jmethodID g_clip_rect      = nullptr;
static jmethodID g_save           = nullptr;
static jmethodID g_restore        = nullptr;

/* Paint method IDs */
static jmethodID g_paint_set_color    = nullptr;
static jmethodID g_paint_set_text_size = nullptr;
static jmethodID g_paint_set_style    = nullptr;
static jmethodID g_paint_set_stroke_width = nullptr;
static jmethodID g_paint_set_anti_alias = nullptr;
static jmethodID g_paint_measure_text = nullptr;

/* Color helper */
static jmethodID g_color_argb = nullptr;

/* Bitmap helper */
static jmethodID g_bitmap_factory_decode = nullptr;
static jmethodID g_bitmap_create = nullptr;

/* Style constants */
static jint g_style_fill  = 0;
static jint g_style_stroke = 0;

void aurora_android_renderer_init(JNIEnv* env) {
    jclass canvas = env->FindClass("android/graphics/Canvas");
    if (!canvas) { fprintf(stderr, "[android-renderer] Canvas class not found\n"); return; }
    g_canvas_class = (jclass)env->NewGlobalRef(canvas);

    g_draw_rect       = env->GetMethodID(g_canvas_class, "drawRect", "(FFFFLandroid/graphics/Paint;)V");
    g_draw_round_rect = env->GetMethodID(g_canvas_class, "drawRoundRect", "(FFFFFFLandroid/graphics/Paint;)V");
    g_draw_text       = env->GetMethodID(g_canvas_class, "drawText", "(Ljava/lang/String;FFLandroid/graphics/Paint;)V");
    g_draw_circle     = env->GetMethodID(g_canvas_class, "drawCircle", "(FFFLandroid/graphics/Paint;)V");
    g_draw_line       = env->GetMethodID(g_canvas_class, "drawLine", "(FFFFLandroid/graphics/Paint;)V");
    g_draw_oval       = env->GetMethodID(g_canvas_class, "drawOval", "(FFFFLandroid/graphics/Paint;)V");
    g_draw_arc        = env->GetMethodID(g_canvas_class, "drawArc", "(FFFFFFZLandroid/graphics/Paint;)V");
    g_draw_bitmap     = env->GetMethodID(g_canvas_class, "drawBitmap", "(Landroid/graphics/Bitmap;FFLandroid/graphics/Paint;)V");
    g_clip_rect        = env->GetMethodID(g_canvas_class, "clipRect", "(FFFF)Z");
    g_save             = env->GetMethodID(g_canvas_class, "save", "()I");
    g_restore          = env->GetMethodID(g_canvas_class, "restore", "()V");

    jclass paint = env->FindClass("android/graphics/Paint");
    if (paint) {
        g_paint_set_color       = env->GetMethodID(paint, "setColor", "(I)V");
        g_paint_set_text_size   = env->GetMethodID(paint, "setTextSize", "(F)V");
        g_paint_set_style       = env->GetMethodID(paint, "setStyle", "(Landroid/graphics/Paint$Style;)V");
        g_paint_set_stroke_width = env->GetMethodID(paint, "setStrokeWidth", "(F)V");
        g_paint_set_anti_alias  = env->GetMethodID(paint, "setAntiAlias", "(Z)V");
        g_paint_measure_text    = env->GetMethodID(paint, "measureText", "(Ljava/lang/String;)F");

        jclass style_enum = env->FindClass("android/graphics/Paint$Style");
        if (style_enum) {
            jfieldID fill_fid   = env->GetStaticFieldID(style_enum, "FILL", "Landroid/graphics/Paint$Style;");
            jfieldID stroke_fid = env->GetStaticFieldID(style_enum, "STROKE", "Landroid/graphics/Paint$Style;");
            if (fill_fid)   g_style_fill   = 0;
            if (stroke_fid) g_style_stroke = 0;
        }

        jclass color = env->FindClass("android/graphics/Color");
        if (color)
            g_color_argb = env->GetStaticMethodID(color, "argb", "(IIII)I");
    }

    jclass bmp_factory = env->FindClass("android/graphics/BitmapFactory");
    if (bmp_factory)
        g_bitmap_factory_decode = env->GetStaticMethodID(bmp_factory, "decodeFile",
            "(Ljava/lang/String;)Landroid/graphics/Bitmap;");

    jclass bmp_class = env->FindClass("android/graphics/Bitmap");
    if (bmp_class)
        g_bitmap_create = env->GetStaticMethodID(bmp_class, "createBitmap",
            "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
}

static int rgba_to_argb(float r, float g, float b, float a) {
    int ir = (int)(r * 255.0f);
    int ig = (int)(g * 255.0f);
    int ib = (int)(b * 255.0f);
    int ia = (int)(a * 255.0f);
    return (ia << 24) | (ir << 16) | (ig << 8) | ib;
}

static void set_paint_color(JNIEnv* env, jobject paint, float r, float g, float b, float a) {
    int argb = rgba_to_argb(r, g, b, a);
    env->CallVoidMethod(paint, g_paint_set_color, argb);
}

static void set_paint_style(JNIEnv* env, jobject paint, int style) {
    /* style: 0=fill, 1=stroke, 2=fill_and_stroke */
}

static void draw_text_centered(JNIEnv* env, jobject canvas, jobject paint,
    const char* text, float cx, float cy, float font_size) {
    if (!text || !text[0]) return;
    env->CallVoidMethod(paint, g_paint_set_text_size, font_size);
    jstring jtext = env->NewStringUTF(text);
    float tx = cx;
    float ty = cy + font_size / 3;
    env->CallVoidMethod(canvas, g_draw_text, jtext, tx, ty, paint);
    env->DeleteLocalRef(jtext);
}

static void render_widget_android(MwWidget* w, JNIEnv* env, jobject canvas, jobject paint,
    float offset_x, float offset_y, float alpha) {
    if (!w || !w->visible) return;

    float abs_x = w->x + offset_x;
    float abs_y = w->y + offset_y;
    float child_alpha = alpha;

    /* ── Clipping for scroll containers ── */
    int need_clip = (w->type == MW_SCROLL);
    if (need_clip)
        env->CallIntMethod(canvas, g_save);

    /* ── Background ── */
    if (w->bg_color[3] > 0) {
        float a = w->bg_color[3] * alpha;
        set_paint_color(env, paint, w->bg_color[0], w->bg_color[1], w->bg_color[2], a);
        env->CallVoidMethod(paint, g_paint_set_anti_alias, 1);

        switch (w->type) {
            case MW_BUTTON:
            case MW_FAB: {
                float r = w->h < 30 ? w->h / 4 : 8;
                env->CallVoidMethod(canvas, g_draw_round_rect,
                    abs_x, abs_y, abs_x + w->w, abs_y + w->h, r, r, paint);
                break;
            }
            case MW_IMAGE:
            case MW_DIALOG:
            case MW_DRAWER:
            case MW_BOTTOM_SHEET:
            case MW_NAV_BAR:
            case MW_TAB_BAR:
            case MW_SNACKBAR:
                env->CallVoidMethod(canvas, g_draw_rect,
                    abs_x, abs_y, abs_x + w->w, abs_y + w->h, paint);
                break;
            default:
                break;
        }
    }

    /* ── Widget-specific rendering ── */
    switch (w->type) {
        case MW_BUTTON: {
            /* Rounded bg already drawn. Draw centered text. */
            if (w->text && w->text[0]) {
                set_paint_color(env, paint, w->text_color[0], w->text_color[1], w->text_color[2], w->text_color[3] * alpha);
                env->CallVoidMethod(paint, g_paint_set_text_size, w->font_size);
                jstring jtext = env->NewStringUTF(w->text);
                float tx = abs_x + 8;
                float ty = abs_y + w->h / 2 + w->font_size / 3;
                env->CallVoidMethod(canvas, g_draw_text, jtext, tx, ty, paint);
                env->DeleteLocalRef(jtext);
            }
            break;
        }

        case MW_TEXT: {
            if (w->text && w->text[0]) {
                set_paint_color(env, paint, w->text_color[0], w->text_color[1], w->text_color[2], w->text_color[3] * alpha);
                env->CallVoidMethod(paint, g_paint_set_text_size, w->font_size);
                jstring jtext = env->NewStringUTF(w->text);
                env->CallVoidMethod(canvas, g_draw_text, jtext, abs_x, abs_y + w->font_size, paint);
                env->DeleteLocalRef(jtext);
            }
            break;
        }

        case MW_INPUT: {
            /* Draw input background */
            set_paint_color(env, paint, 1, 1, 1, alpha);
            env->CallVoidMethod(canvas, g_draw_rect, abs_x, abs_y, abs_x + w->w, abs_y + w->h, paint);
            /* Draw border */
            set_paint_color(env, paint, 0.7f, 0.7f, 0.7f, alpha);
            float bw = 1.0f;
            env->CallVoidMethod(canvas, g_draw_rect, abs_x, abs_y, abs_x + w->w, abs_y + bw, paint);
            env->CallVoidMethod(canvas, g_draw_rect, abs_x, abs_y + w->h - bw, abs_x + w->w, abs_y + w->h, paint);
            env->CallVoidMethod(canvas, g_draw_rect, abs_x, abs_y, abs_x + bw, abs_y + w->h, paint);
            env->CallVoidMethod(canvas, g_draw_rect, abs_x + w->w - bw, abs_y, abs_x + w->w, abs_y + w->h, paint);
            /* Draw text */
            if (w->text && w->text[0]) {
                set_paint_color(env, paint, 0, 0, 0, alpha);
                env->CallVoidMethod(paint, g_paint_set_text_size, w->font_size);
                jstring jtext = env->NewStringUTF(w->text);
                env->CallVoidMethod(canvas, g_draw_text, jtext, abs_x + 4, abs_y + w->h / 2 + w->font_size / 3, paint);
                env->DeleteLocalRef(jtext);
            }
            break;
        }

        case MW_IMAGE: {
            /* Draw placeholder or actual bitmap */
            if (w->image_path && w->image_path[0]) {
                jstring jpath = env->NewStringUTF(w->image_path);
                if (g_bitmap_factory_decode) {
                    jobject bmp = env->CallStaticObjectMethod(
                        env->FindClass("android/graphics/BitmapFactory"),
                        g_bitmap_factory_decode, jpath);
                    if (bmp) {
                        env->CallVoidMethod(canvas, g_draw_bitmap, bmp, abs_x, abs_y, paint);
                    }
                }
                env->DeleteLocalRef(jpath);
            } else {
                /* Placeholder: gray rect */
                set_paint_color(env, paint, 0.8f, 0.8f, 0.8f, alpha);
                env->CallVoidMethod(canvas, g_draw_rect, abs_x, abs_y, abs_x + w->w, abs_y + w->h, paint);
                set_paint_color(env, paint, 0.5f, 0.5f, 0.5f, alpha);
                env->CallVoidMethod(canvas, g_draw_line, abs_x, abs_y, abs_x + w->w, abs_y + w->h, paint);
                env->CallVoidMethod(canvas, g_draw_line, abs_x + w->w, abs_y, abs_x, abs_y + w->h, paint);
            }
            break;
        }

        case MW_LIST: {
            /* Draw items as rows */
            float row_h = 40;
            set_paint_color(env, paint, 0, 0, 0, 0.1f * alpha);
            for (int i = 0; i < w->item_count; i++) {
                float ry = abs_y + i * row_h;
                if (i % 2 == 1)
                    env->CallVoidMethod(canvas, g_draw_rect, abs_x, ry, abs_x + w->w, ry + row_h, paint);
                if (w->items[i]) {
                    set_paint_color(env, paint, 0, 0, 0, alpha);
                    env->CallVoidMethod(paint, g_paint_set_text_size, 14);
                    jstring jitem = env->NewStringUTF(w->items[i]);
                    env->CallVoidMethod(canvas, g_draw_text, jitem, abs_x + 8, ry + row_h / 2 + 5, paint);
                    env->DeleteLocalRef(jitem);
                }
                /* Divider line */
                set_paint_color(env, paint, 0.8f, 0.8f, 0.8f, alpha);
                env->CallVoidMethod(canvas, g_draw_line, abs_x, ry + row_h, abs_x + w->w, ry + row_h, paint);
            }
            break;
        }

        case MW_GRID: {
            /* Draw grid lines */
            int cols = (w->selected_index > 0) ? w->selected_index : 2;
            if (cols < 1) cols = 1;
            float cell_w = w->w / cols;
            set_paint_color(env, paint, 0.8f, 0.8f, 0.8f, alpha);
            for (int c = 1; c < cols; c++) {
                float gx = abs_x + c * cell_w;
                env->CallVoidMethod(canvas, g_draw_line, gx, abs_y, gx, abs_y + w->h, paint);
            }
            float total_h = 0;
            int rows = (w->child_count + cols - 1) / cols;
            if (rows < 1) rows = 1;
            float row_h2 = w->h / rows;
            for (int r = 1; r < rows; r++) {
                float gy = abs_y + r * row_h2;
                env->CallVoidMethod(canvas, g_draw_line, abs_x, gy, abs_x + w->w, gy, paint);
            }
            break;
        }

        case MW_SCROLL: {
            /* Clip children to content area */
            env->CallBooleanMethod(canvas, g_clip_rect, abs_x, abs_y, abs_x + w->w, abs_y + w->h);
            /* Draw scroll indicator if content overflows */
            if (w->child_count > 0) {
                float content_h = w->child_count * 60;
                float visible_ratio = w->h / content_h;
                if (visible_ratio < 1.0f) {
                    float scrollbar_h = w->h * visible_ratio;
                    float scrollbar_y = abs_y + (w->scroll_y / (content_h - w->h)) * (w->h - scrollbar_h);
                    set_paint_color(env, paint, 0.5f, 0.5f, 0.5f, 0.5f * alpha);
                    float sb_x = abs_x + w->w - 6;
                    env->CallVoidMethod(canvas, g_draw_round_rect,
                        sb_x, scrollbar_y, sb_x + 4, scrollbar_y + scrollbar_h, 2, 2, paint);
                }
            }
            break;
        }

        case MW_SLIDER: {
            /* Track */
            float track_y = abs_y + w->h / 2 - 2;
            set_paint_color(env, paint, 0.7f, 0.7f, 0.7f, alpha);
            env->CallVoidMethod(canvas, g_draw_round_rect,
                abs_x, track_y, abs_x + w->w, track_y + 4, 2, 2, paint);
            /* Filled portion */
            float fill = (w->value > 0 && w->value <= 100) ? w->value / 100.0f : 0.5f;
            float fill_w = w->w * fill;
            set_paint_color(env, paint, 0.3f, 0.6f, 1.0f, alpha);
            env->CallVoidMethod(canvas, g_draw_round_rect,
                abs_x, track_y, abs_x + fill_w, track_y + 4, 2, 2, paint);
            /* Thumb circle */
            float thumb_x = abs_x + fill_w;
            float thumb_y = abs_y + w->h / 2;
            set_paint_color(env, paint, 1, 1, 1, alpha);
            env->CallVoidMethod(canvas, g_draw_circle, thumb_x, thumb_y, 8, paint);
            set_paint_color(env, paint, 0.3f, 0.6f, 1.0f, alpha);
            env->CallVoidMethod(canvas, g_draw_circle, thumb_x, thumb_y, 8, paint);
            env->CallVoidMethod(canvas, g_draw_circle, thumb_x, thumb_y, 5, paint);
            break;
        }

        case MW_SWITCH: {
            /* Track */
            float track_w = w->w;
            float track_h = w->h;
            float t_r = track_h / 2;
            set_paint_color(env, paint, 0.6f, 0.6f, 0.6f, alpha);
            env->CallVoidMethod(canvas, g_draw_round_rect,
                abs_x, abs_y, abs_x + track_w, abs_y + track_h, t_r, t_r, paint);
            /* On state: filled track */
            if (w->value > 0) {
                set_paint_color(env, paint, 0.3f, 0.8f, 0.3f, alpha);
                env->CallVoidMethod(canvas, g_draw_round_rect,
                    abs_x, abs_y, abs_x + track_w, abs_y + track_h, t_r, t_r, paint);
            }
            /* Knob circle */
            float knob_x = (w->value > 0) ? abs_x + track_w - track_h : abs_x;
            set_paint_color(env, paint, 1, 1, 1, alpha);
            env->CallVoidMethod(canvas, g_draw_circle, knob_x + t_r, abs_y + t_r, t_r - 2, paint);
            break;
        }

        case MW_CHECKBOX: {
            float box_size = (w->h < 24) ? w->h : 24;
            float box_x = abs_x;
            float box_y = abs_y + (w->h - box_size) / 2;
            /* Box border */
            set_paint_color(env, paint, 0.5f, 0.5f, 0.5f, alpha);
            env->CallVoidMethod(canvas, g_draw_round_rect,
                box_x, box_y, box_x + box_size, box_y + box_size, 3, 3, paint);
            /* Filled if checked */
            if (w->value > 0) {
                set_paint_color(env, paint, 0.3f, 0.6f, 1.0f, alpha);
                env->CallVoidMethod(canvas, g_draw_round_rect,
                    box_x + 2, box_y + 2, box_x + box_size - 2, box_y + box_size - 2, 2, 2, paint);
                /* Checkmark */
                set_paint_color(env, paint, 1, 1, 1, alpha);
                env->CallVoidMethod(canvas, g_draw_line,
                    box_x + 4, box_y + box_size / 2, box_x + box_size / 2 - 2, box_y + box_size - 5, paint);
                env->CallVoidMethod(canvas, g_draw_line,
                    box_x + box_size / 2 - 2, box_y + box_size - 5, box_x + box_size - 4, box_y + 4, paint);
            }
            /* Label */
            if (w->text && w->text[0]) {
                set_paint_color(env, paint, w->text_color[0], w->text_color[1], w->text_color[2], w->text_color[3] * alpha);
                env->CallVoidMethod(paint, g_paint_set_text_size, w->font_size);
                jstring jtext = env->NewStringUTF(w->text);
                env->CallVoidMethod(canvas, g_draw_text, jtext, box_x + box_size + 6, abs_y + w->h / 2 + w->font_size / 3, paint);
                env->DeleteLocalRef(jtext);
            }
            break;
        }

        case MW_RADIO: {
            float r_size = (w->h < 24) ? w->h : 24;
            float r_x = abs_x + r_size / 2;
            float r_y = abs_y + w->h / 2;
            /* Outer circle */
            set_paint_color(env, paint, 0.5f, 0.5f, 0.5f, alpha);
            env->CallVoidMethod(canvas, g_draw_circle, r_x, r_y, r_size / 2, paint);
            /* Filled if selected */
            set_paint_color(env, paint, 1, 1, 1, alpha);
            env->CallVoidMethod(canvas, g_draw_circle, r_x, r_y, r_size / 2 - 2, paint);
            if (w->value > 0) {
                set_paint_color(env, paint, 0.3f, 0.6f, 1.0f, alpha);
                env->CallVoidMethod(canvas, g_draw_circle, r_x, r_y, r_size / 2 - 4, paint);
            }
            /* Label */
            if (w->text && w->text[0]) {
                set_paint_color(env, paint, w->text_color[0], w->text_color[1], w->text_color[2], w->text_color[3] * alpha);
                env->CallVoidMethod(paint, g_paint_set_text_size, w->font_size);
                jstring jtext = env->NewStringUTF(w->text);
                env->CallVoidMethod(canvas, g_draw_text, jtext, r_x + r_size / 2 + 6, abs_y + w->h / 2 + w->font_size / 3, paint);
                env->DeleteLocalRef(jtext);
            }
            break;
        }

        case MW_PROGRESS: {
            float prog_y = abs_y + w->h / 2 - 4;
            float prog_h = 8;
            /* Track */
            set_paint_color(env, paint, 0.8f, 0.8f, 0.8f, alpha);
            env->CallVoidMethod(canvas, g_draw_round_rect,
                abs_x, prog_y, abs_x + w->w, prog_y + prog_h, 4, 4, paint);
            /* Fill */
            float pct = (w->value > 0) ? w->value / 100.0f : 0.0f;
            if (pct > 0) {
                float fill_w = (w->w - 2) * pct;
                set_paint_color(env, paint, 0.3f, 0.6f, 1.0f, alpha);
                env->CallVoidMethod(canvas, g_draw_round_rect,
                    abs_x + 1, prog_y + 1, abs_x + 1 + fill_w, prog_y + prog_h - 1, 3, 3, paint);
            }
            break;
        }

        case MW_DIALOG: {
            /* Semi-transparent overlay */
            set_paint_color(env, paint, 0, 0, 0, 0.4f * alpha);
            env->CallVoidMethod(canvas, g_draw_rect,
                offset_x, offset_y, offset_x + 10000, offset_y + 10000, paint);
            /* Dialog box centered */
            float dlg_w = w->w > 0 ? w->w : 300;
            float dlg_h = w->h > 0 ? w->h : 200;
            float dlg_x = abs_x;
            float dlg_y = abs_y;
            set_paint_color(env, paint, 1, 1, 1, alpha);
            env->CallVoidMethod(canvas, g_draw_round_rect,
                dlg_x, dlg_y, dlg_x + dlg_w, dlg_y + dlg_h, 12, 12, paint);
            /* Title */
            if (w->text && w->text[0]) {
                set_paint_color(env, paint, 0, 0, 0, alpha);
                env->CallVoidMethod(paint, g_paint_set_text_size, w->font_size + 4);
                jstring jtitle = env->NewStringUTF(w->text);
                env->CallVoidMethod(canvas, g_draw_text, jtitle, dlg_x + 16, dlg_y + 32, paint);
                env->DeleteLocalRef(jtitle);
            }
            break;
        }

        case MW_SNACKBAR: {
            /* Rounded pill at bottom */
            set_paint_color(env, paint, 0.2f, 0.2f, 0.2f, 0.9f * alpha);
            float sb_r = w->h / 2;
            env->CallVoidMethod(canvas, g_draw_round_rect,
                abs_x, abs_y, abs_x + w->w, abs_y + w->h, sb_r, sb_r, paint);
            /* Text */
            if (w->text && w->text[0]) {
                set_paint_color(env, paint, 1, 1, 1, alpha);
                env->CallVoidMethod(paint, g_paint_set_text_size, 14);
                jstring jtext = env->NewStringUTF(w->text);
                env->CallVoidMethod(canvas, g_draw_text, jtext, abs_x + 16, abs_y + w->h / 2 + 5, paint);
                env->DeleteLocalRef(jtext);
            }
            break;
        }

        case MW_BOTTOM_SHEET: {
            /* Sheet overlay + rounded top panel */
            set_paint_color(env, paint, 0, 0, 0, 0.3f * alpha);
            env->CallVoidMethod(canvas, g_draw_rect,
                offset_x, offset_y, offset_x + 10000, offset_y + 10000, paint);
            set_paint_color(env, paint, 1, 1, 1, alpha);
            int top_r = 16;
            env->CallVoidMethod(canvas, g_draw_round_rect,
                abs_x, abs_y, abs_x + w->w, abs_y + w->h, top_r, top_r, paint);
            break;
        }

        case MW_NAV_BAR: {
            /* Top bar */
            set_paint_color(env, paint, 0.2f, 0.2f, 0.2f, alpha);
            env->CallVoidMethod(canvas, g_draw_rect, abs_x, abs_y, abs_x + w->w, abs_y + w->h, paint);
            if (w->text && w->text[0]) {
                set_paint_color(env, paint, 1, 1, 1, alpha);
                env->CallVoidMethod(paint, g_paint_set_text_size, 18);
                jstring jtitle = env->NewStringUTF(w->text);
                float tx = abs_x + w->w / 2;
                float ty = abs_y + w->h / 2 + 6;
                env->CallVoidMethod(canvas, g_draw_text, jtitle, tx, ty, paint);
                env->DeleteLocalRef(jtitle);
            }
            break;
        }

        case MW_TAB_BAR: {
            /* Bottom tab bar with items */
            int tab_count = w->item_count > 0 ? w->item_count : 1;
            float tab_w = w->w / tab_count;
            /* Background */
            set_paint_color(env, paint, 0.95f, 0.95f, 0.95f, alpha);
            env->CallVoidMethod(canvas, g_draw_rect, abs_x, abs_y, abs_x + w->w, abs_y + w->h, paint);
            /* Divider */
            set_paint_color(env, paint, 0.8f, 0.8f, 0.8f, alpha);
            env->CallVoidMethod(canvas, g_draw_line, abs_x, abs_y, abs_x + w->w, abs_y, paint);
            /* Tabs */
            set_paint_color(env, paint, 0, 0, 0, alpha);
            env->CallVoidMethod(paint, g_paint_set_text_size, 12);
            for (int i = 0; i < tab_count; i++) {
                float tx = abs_x + i * tab_w + tab_w / 2;
                float ty = abs_y + w->h / 2 + 4;
                const char* label = (i < w->item_count && w->items[i]) ? w->items[i] : "Tab";
                jstring jtab = env->NewStringUTF(label);
                env->CallVoidMethod(canvas, g_draw_text, jtab, tx, ty, paint);
                env->DeleteLocalRef(jtab);
                /* Active tab indicator */
                if (i == w->selected_index) {
                    set_paint_color(env, paint, 0.3f, 0.6f, 1.0f, alpha);
                    env->CallVoidMethod(canvas, g_draw_rect,
                        abs_x + i * tab_w + 8, abs_y + w->h - 3,
                        abs_x + (i + 1) * tab_w - 8, abs_y + w->h, paint);
                    set_paint_color(env, paint, 0, 0, 0, alpha);
                }
            }
            break;
        }

        case MW_DRAWER: {
            /* Side drawer: semi-transparent left panel */
            set_paint_color(env, paint, 0, 0, 0, 0.3f * alpha);
            env->CallVoidMethod(canvas, g_draw_rect,
                offset_x, offset_y, offset_x + 10000, offset_y + 10000, paint);
            set_paint_color(env, paint, 1, 1, 1, alpha);
            env->CallVoidMethod(canvas, g_draw_rect, abs_x, abs_y, abs_x + w->w, abs_y + w->h, paint);
            break;
        }

        case MW_COLUMN:
        case MW_ROW:
        default:
            break;
    }

    /* ── Render children ── */
    for (int i = 0; i < w->child_count; i++)
        render_widget_android(w->children[i], env, canvas, paint, abs_x, abs_y, child_alpha);

    /* ── Restore clipping ── */
    if (need_clip)
        env->CallVoidMethod(canvas, g_restore);
}

void aurora_android_render_mw(JNIEnv* env, jobject canvas, void* root_widget) {
    if (!env || !canvas || !root_widget) return;

    jclass paint_class = env->FindClass("android/graphics/Paint");
    jmethodID paint_init = env->GetMethodID(paint_class, "<init>", "()V");
    jobject paint = env->NewObject(paint_class, paint_init);

    if (paint) {
        render_widget_android((MwWidget*)root_widget, env, canvas, paint, 0, 0, 1.0f);
        env->DeleteLocalRef(paint);
    }
}
