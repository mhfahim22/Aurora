#include "mobile/widgets.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>

/* ════════════════════════════════════════════════════════════
   Internal structures
   ════════════════════════════════════════════════════════════ */

struct MwWidget {
    int      type;
    float    x, y, w, h;
    char*    text;
    int      enabled;
    int      visible;
    float    padding[4];
    float    margin[4];
    float    bg_color[4];
    float    text_color[4];
    float    font_size;
    int      main_align;
    int      cross_align;
    float    spacing;
    float    value;
    int      selected_index;
    float    scroll_x, scroll_y;
    char*    image_path;
    char**   items;
    int      item_count;
    int      item_capacity;
    MwEventCallback callback;
    int      has_focus;
    MwWidget* parent;
    MwWidget** children;
    int      child_count;
    int      child_capacity;
};

/* ════════════════════════════════════════════════════════════
   Helpers
   ════════════════════════════════════════════════════════════ */

static char* dup_str(const char* s) {
    if (!s) return nullptr;
    size_t n = strlen(s) + 1;
    char* d = (char*)malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

static void expand_children(MwWidget* w) {
    if (w->child_count >= w->child_capacity) {
        int new_cap = w->child_capacity ? w->child_capacity * 2 : 4;
        auto** newc = (MwWidget**)realloc(w->children, new_cap * sizeof(MwWidget*));
        if (newc) { w->children = newc; w->child_capacity = new_cap; }
    }
}

static void expand_items(MwWidget* w) {
    if (w->item_count >= w->item_capacity) {
        int new_cap = w->item_capacity ? w->item_capacity * 2 : 4;
        char** newi = (char**)realloc(w->items, new_cap * sizeof(char*));
        if (newi) { w->items = newi; w->item_capacity = new_cap; }
    }
}

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* ════════════════════════════════════════════════════════════
   Widget core
   ════════════════════════════════════════════════════════════ */

void mw_init(void) {}

void mw_shutdown(void) {}

void* mw_create(int type) {
    MwWidget* w = (MwWidget*)calloc(1, sizeof(MwWidget));
    if (!w) return nullptr;
    w->type = type;
    w->enabled = 1;
    w->visible = 1;
    w->font_size = 16.0f;
    w->spacing = 0.0f;
    w->bg_color[3] = 1.0f;
    w->text_color[3] = 1.0f;
    w->main_align = MW_MAIN_START;
    w->cross_align = MW_CROSS_STRETCH;
    if (type == MW_TEXT) {
        w->cross_align = MW_CROSS_START;
    }
    return w;
}

static void mw_destroy_recursive(MwWidget* w) {
    if (!w) return;
    for (int i = 0; i < w->child_count; i++)
        mw_destroy_recursive(w->children[i]);
    free(w->text);
    free(w->image_path);
    for (int i = 0; i < w->item_count; i++)
        free(w->items[i]);
    free(w->items);
    free(w->children);
    free(w);
}

void mw_destroy(void* widget) {
    MwWidget* w = (MwWidget*)widget;
    if (w && w->parent)
        mw_remove_child(w->parent, w);
    else
        mw_destroy_recursive(w);
}

void mw_add_child(void* parent, void* child) {
    if (!parent || !child) return;
    MwWidget* p = (MwWidget*)parent;
    MwWidget* c = (MwWidget*)child;
    if (c->parent) {
        mw_remove_child(c->parent, c);
    }
    expand_children(p);
    p->children[p->child_count++] = c;
    c->parent = p;
}

void mw_remove_child(void* parent, void* child) {
    if (!parent || !child) return;
    MwWidget* p = (MwWidget*)parent;
    MwWidget* c = (MwWidget*)child;
    int found = 0;
    for (int i = 0; i < p->child_count; i++) {
        if (p->children[i] == c) found = 1;
        if (found && i + 1 < p->child_count)
            p->children[i] = p->children[i + 1];
    }
    if (found) {
        p->child_count--;
        c->parent = nullptr;
    }
}

/* ════════════════════════════════════════════════════════════
   Layout engine — flexbox-like for Column/Row/Grid
   ════════════════════════════════════════════════════════════ */

void mw_set_pos(void* widget, float x, float y) {
    if (!widget) return;
    MwWidget* w = (MwWidget*)widget;
    w->x = x; w->y = y;
}

void mw_set_size(void* widget, float w_, float h_) {
    if (!widget) return;
    MwWidget* w = (MwWidget*)widget;
    w->w = w_; w->h = h_;
}

float mw_get_width(void* widget) {
    if (!widget) return 0;
    return ((MwWidget*)widget)->w;
}

float mw_get_height(void* widget) {
    if (!widget) return 0;
    return ((MwWidget*)widget)->h;
}

static void layout_widget(MwWidget* w) {
    if (!w || !w->visible || w->child_count == 0) return;

    /* First, layout children recursively */
    for (int i = 0; i < w->child_count; i++)
        layout_widget(w->children[i]);

    float px = w->padding[0];
    float py = w->padding[1];
    float cw = w->w - px - w->padding[2];
    float ch = w->h - py - w->padding[3];
    if (cw < 0) cw = 0;
    if (ch < 0) ch = 0;

    if (w->type == MW_COLUMN) {
        float total_h = 0;
        for (int i = 0; i < w->child_count; i++) {
            MwWidget* c = w->children[i];
            if (!c->visible) continue;
            total_h += c->h;
        }
        total_h += (w->child_count - 1) * w->spacing;
        float start_y = py;
        if (w->main_align == MW_MAIN_CENTER)
            start_y = py + (ch - total_h) / 2;
        else if (w->main_align == MW_MAIN_END)
            start_y = py + (ch - total_h);

        float yy = start_y;
        for (int i = 0; i < w->child_count; i++) {
            MwWidget* c = w->children[i];
            if (!c->visible) continue;
            c->x = px + c->margin[0];
            if (w->cross_align == MW_CROSS_CENTER)
                c->x = px + (cw - c->w) / 2;
            else if (w->cross_align == MW_CROSS_END)
                c->x = px + cw - c->w - c->margin[2];
            else if (w->cross_align == MW_CROSS_STRETCH)
                c->w = cw - c->margin[0] - c->margin[2];
            c->y = yy + c->margin[1];
            yy += c->h + c->margin[1] + c->margin[3] + w->spacing;
        }
    } else if (w->type == MW_ROW) {
        float total_w = 0;
        for (int i = 0; i < w->child_count; i++) {
            MwWidget* c = w->children[i];
            if (!c->visible) continue;
            total_w += c->w;
        }
        total_w += (w->child_count - 1) * w->spacing;
        float start_x = px;
        if (w->main_align == MW_MAIN_CENTER)
            start_x = px + (cw - total_w) / 2;
        else if (w->main_align == MW_MAIN_END)
            start_x = px + (cw - total_w);

        float xx = start_x;
        for (int i = 0; i < w->child_count; i++) {
            MwWidget* c = w->children[i];
            if (!c->visible) continue;
            c->x = xx + c->margin[0];
            c->y = py + c->margin[1];
            if (w->cross_align == MW_CROSS_CENTER)
                c->y = py + (ch - c->h) / 2;
            else if (w->cross_align == MW_CROSS_END)
                c->y = py + ch - c->h - c->margin[3];
            else if (w->cross_align == MW_CROSS_STRETCH)
                c->h = ch - c->margin[1] - c->margin[3];
            xx += c->w + c->margin[0] + c->margin[2] + w->spacing;
        }
    } else if (w->type == MW_GRID) {
        int cols = (w->selected_index > 0) ? w->selected_index : 2;
        if (cols < 1) cols = 1;
        if (cols > w->child_count) cols = w->child_count;
        float cell_w = (cw - (cols - 1) * w->spacing) / cols;
        int idx = 0;
        for (int i = 0; i < w->child_count; i++) {
            MwWidget* c = w->children[i];
            if (!c->visible) continue;
            int row = idx / cols;
            int col = idx % cols;
            c->w = cell_w - c->margin[0] - c->margin[2];
            c->x = px + col * (cell_w + w->spacing) + c->margin[0];
            c->y = py + row * (c->h + w->spacing) + c->margin[1];
            idx++;
        }
    }
}

void mw_layout(void* widget) {
    if (!widget) return;
    layout_widget((MwWidget*)widget);
}

void mw_set_align(void* widget, int main_axis, int cross_axis) {
    if (!widget) return;
    MwWidget* w = (MwWidget*)widget;
    w->main_align = main_axis;
    w->cross_align = cross_axis;
}

void mw_set_spacing(void* widget, float spacing) {
    if (!widget) return;
    ((MwWidget*)widget)->spacing = spacing;
}

void mw_set_padding(void* widget, float l, float t, float r, float b) {
    if (!widget) return;
    MwWidget* w = (MwWidget*)widget;
    w->padding[0] = l; w->padding[1] = t;
    w->padding[2] = r; w->padding[3] = b;
}

void mw_set_margin(void* widget, float l, float t, float r, float b) {
    if (!widget) return;
    MwWidget* w = (MwWidget*)widget;
    w->margin[0] = l; w->margin[1] = t;
    w->margin[2] = r; w->margin[3] = b;
}

/* ════════════════════════════════════════════════════════════
   Properties
   ════════════════════════════════════════════════════════════ */

void mw_set_bg_color(void* widget, float r, float g, float b, float a) {
    if (!widget) return;
    MwWidget* w = (MwWidget*)widget;
    w->bg_color[0] = clampf(r,0,1);
    w->bg_color[1] = clampf(g,0,1);
    w->bg_color[2] = clampf(b,0,1);
    w->bg_color[3] = clampf(a,0,1);
}

void mw_set_text_color(void* widget, float r, float g, float b, float a) {
    if (!widget) return;
    MwWidget* w = (MwWidget*)widget;
    w->text_color[0] = clampf(r,0,1);
    w->text_color[1] = clampf(g,0,1);
    w->text_color[2] = clampf(b,0,1);
    w->text_color[3] = clampf(a,0,1);
}

void mw_set_font_size(void* widget, float size) {
    if (!widget) return;
    ((MwWidget*)widget)->font_size = size > 0 ? size : 16.0f;
}

void mw_set_text(void* widget, const char* text) {
    if (!widget) return;
    MwWidget* w = (MwWidget*)widget;
    free(w->text);
    w->text = dup_str(text);
}

const char* mw_get_text(void* widget) {
    if (!widget) return "";
    MwWidget* w = (MwWidget*)widget;
    return w->text ? w->text : "";
}

void mw_set_enabled(void* widget, int enabled) {
    if (!widget) return;
    ((MwWidget*)widget)->enabled = enabled;
}

void mw_set_visible(void* widget, int visible) {
    if (!widget) return;
    ((MwWidget*)widget)->visible = visible;
}

void mw_set_image(void* widget, const char* path) {
    if (!widget) return;
    MwWidget* w = (MwWidget*)widget;
    free(w->image_path);
    w->image_path = dup_str(path);
}

void mw_set_value(void* widget, float value) {
    if (!widget) return;
    ((MwWidget*)widget)->value = value;
}

void mw_set_selected(void* widget, int index) {
    if (!widget) return;
    ((MwWidget*)widget)->selected_index = index;
}

int mw_get_type(void* widget) {
    if (!widget) return -1;
    return ((MwWidget*)widget)->type;
}

/* ════════════════════════════════════════════════════════════
   Items
   ════════════════════════════════════════════════════════════ */

void mw_add_item(void* widget, const char* text) {
    if (!widget) return;
    MwWidget* w = (MwWidget*)widget;
    expand_items(w);
    w->items[w->item_count++] = dup_str(text);
}

void mw_remove_item(void* widget, int index) {
    if (!widget) return;
    MwWidget* w = (MwWidget*)widget;
    if (index < 0 || index >= w->item_count) return;
    free(w->items[index]);
    for (int i = index; i + 1 < w->item_count; i++)
        w->items[i] = w->items[i + 1];
    w->item_count--;
}

void mw_clear_items(void* widget) {
    if (!widget) return;
    MwWidget* w = (MwWidget*)widget;
    for (int i = 0; i < w->item_count; i++)
        free(w->items[i]);
    w->item_count = 0;
}

/* ════════════════════════════════════════════════════════════
   Events — hit-testing + dispatch
   ════════════════════════════════════════════════════════════ */

void mw_set_callback(void* widget, MwEventCallback callback) {
    if (!widget) return;
    ((MwWidget*)widget)->callback = callback;
}

static MwWidget* hit_test(MwWidget* w, float x, float y) {
    if (!w || !w->visible) return nullptr;
    if (x < w->x || x > w->x + w->w || y < w->y || y > w->y + w->h)
        return nullptr;
    for (int i = w->child_count - 1; i >= 0; i--) {
        MwWidget* hit = hit_test(w->children[i], x, y);
        if (hit) return hit;
    }
    if (w->type == MW_COLUMN || w->type == MW_ROW || w->type == MW_GRID ||
        w->type == MW_SCROLL || w->type == MW_NAV_BAR || w->type == MW_DRAWER)
        return w;
    return w;
}

int mw_handle_touch(void* widget, float x, float y, int action) {
    if (!widget) return 0;
    MwWidget* target = hit_test((MwWidget*)widget, x, y);
    if (!target || !target->enabled) return 0;
    if (action == MW_TOUCH_DOWN && target->callback) {
        int evt = MW_EVENT_CLICK;
        if (target->type == MW_INPUT) evt = MW_EVENT_FOCUS;
        else if (target->type == MW_DRAWER) evt = MW_EVENT_DRAWER;
        target->callback(target, evt, nullptr);
    }
    return 1;
}

/* ════════════════════════════════════════════════════════════
   Scroll
   ════════════════════════════════════════════════════════════ */

void mw_set_scroll_pos(void* widget, float x, float y) {
    if (!widget) return;
    MwWidget* w = (MwWidget*)widget;
    w->scroll_x = x; w->scroll_y = y;
}

void mw_get_scroll_pos(void* widget, float* x, float* y) {
    if (!widget) return;
    MwWidget* w = (MwWidget*)widget;
    if (x) *x = w->scroll_x;
    if (y) *y = w->scroll_y;
}

/* ════════════════════════════════════════════════════════════
   Render — platform-specific; stubbed for now
   ════════════════════════════════════════════════════════════ */

void mw_render(void* widget) {
    (void)widget;
}
