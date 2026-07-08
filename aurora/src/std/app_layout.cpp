#include "std/app.hpp"
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>

/* ════════════════════════════════════════════════════════════
   Flexbox-style layout engine
   ════════════════════════════════════════════════════════════ */

#define APP_LAYOUT_COLUMN 0
#define APP_LAYOUT_ROW    1

struct AppLayoutNode {
    int type;
    int direction;
    int justify;
    int align;
    int wrap;
    float gap;
    float w, h;
    float min_w, min_h;
    float max_w, max_h;
    float pad_l, pad_t, pad_r, pad_b;
    float mar_l, mar_t, mar_r, mar_b;
    int grow;
    int shrink;
    int basis;
    float out_x, out_y, out_w, out_h;
    AppLayoutNode* parent;
    std::vector<AppLayoutNode*> children;
};

static AppLayoutNode* node_new(AppLayoutNode* parent) {
    auto* n = new AppLayoutNode();
    std::memset(n, 0, sizeof(AppLayoutNode));
    n->type = 0;
    n->direction = APP_LAYOUT_COLUMN;
    n->max_w = n->max_h = 1e9f;
    n->grow = 0;
    n->shrink = 1;
    n->parent = parent;
    if (parent) parent->children.push_back(n);
    return n;
}

static float clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static void layout_column(AppLayoutNode* n, float cw, float ch) {
    float avail_w = cw - n->pad_l - n->pad_r;
    float avail_h = ch - n->pad_t - n->pad_b;
    float y = n->pad_t;
    float total_grow = 0, total_shrink = 0;
    float used_h = 0;
    for (auto* c : n->children) {
        c->out_x = n->mar_l + n->pad_l;
        c->out_y = y + n->mar_t;
        float child_w = c->w > 0 ? c->w : avail_w - c->mar_l - c->mar_r;
        float child_h = c->h > 0 ? c->h : 0;
        c->out_w = clamp(child_w, c->min_w, c->max_w);
        c->out_h = clamp(child_h, c->min_h, c->max_h);
        y += c->out_h + c->mar_t + c->mar_b + n->gap;
        used_h += c->out_h + c->mar_t + c->mar_b + n->gap;
        if (c->grow > 0) total_grow += c->grow;
        if (c->shrink > 0) total_shrink += c->shrink;
    }
    used_h -= n->gap;
    if (used_h < 0) used_h = 0;
    float extra = avail_h - used_h;
    if (extra > 0 && total_grow > 0) {
        float remain = extra;
        for (auto* c : n->children) {
            if (c->grow > 0) {
                float add = extra * c->grow / total_grow;
                c->out_h += add;
                remain -= add;
            }
        }
    } else if (extra < 0 && total_shrink > 0) {
        float shrink_total = total_shrink;
        for (auto* c : n->children) {
            if (c->shrink > 0) {
                float sub = (-extra) * c->shrink / shrink_total;
                c->out_h = clamp(c->out_h - sub, c->min_h, c->max_h);
            }
        }
    }
    y = n->pad_t;
    if (n->justify == 1) {
        y += (avail_h - used_h) / 2;
    } else if (n->justify == 2) {
        y += avail_h - used_h;
    } else if (n->justify == 3 && n->children.size() > 1) {
        float space = (avail_h - used_h) / (n->children.size() - 1);
        for (auto* c : n->children) {
            c->out_y = n->pad_t + y;
            y += c->out_h + c->mar_t + c->mar_b + space;
        }
        y = n->pad_t;
    }
    for (auto* c : n->children) {
        c->out_y = y + n->mar_t;
        if (n->align == 3) c->out_w = avail_w - c->mar_l - c->mar_r;
        else if (n->align == 1) c->out_x = n->pad_l + (avail_w - c->out_w) / 2;
        else if (n->align == 2) c->out_x = n->pad_l + avail_w - c->out_w - c->mar_r;
        else c->out_x = n->pad_l + c->mar_l;
        y += c->out_h + c->mar_t + c->mar_b + n->gap;
        layout_column(c, c->out_w, c->out_h);
    }
}

static void layout_row(AppLayoutNode* n, float cw, float ch) {
    float avail_w = cw - n->pad_l - n->pad_r;
    float avail_h = ch - n->pad_t - n->pad_b;
    float x = n->pad_l;
    float total_grow = 0, total_shrink = 0;
    float used_w = 0;
    for (auto* c : n->children) {
        c->out_y = n->pad_t + n->mar_t;
        float child_w = c->w > 0 ? c->w : 0;
        float child_h = c->h > 0 ? c->h : avail_h - c->mar_t - c->mar_b;
        c->out_w = clamp(child_w, c->min_w, c->max_w);
        c->out_h = clamp(child_h, c->min_h, c->max_h);
        x += c->out_w + c->mar_l + c->mar_r + n->gap;
        used_w += c->out_w + c->mar_l + c->mar_r + n->gap;
        if (c->grow > 0) total_grow += c->grow;
        if (c->shrink > 0) total_shrink += c->shrink;
    }
    used_w -= n->gap;
    if (used_w < 0) used_w = 0;
    float extra = avail_w - used_w;
    if (extra > 0 && total_grow > 0) {
        float remain = extra;
        for (auto* c : n->children) {
            if (c->grow > 0) {
                float add = extra * c->grow / total_grow;
                c->out_w += add;
                remain -= add;
            }
        }
    } else if (extra < 0 && total_shrink > 0) {
        for (auto* c : n->children) {
            if (c->shrink > 0) {
                float sub = (-extra) * c->shrink / total_shrink;
                c->out_w = clamp(c->out_w - sub, c->min_w, c->max_w);
            }
        }
    }
    x = n->pad_l;
    if (n->justify == 1) {
        x += (avail_w - used_w) / 2;
    } else if (n->justify == 2) {
        x += avail_w - used_w;
    } else if (n->justify == 3 && n->children.size() > 1) {
        float space = (avail_w - used_w) / (n->children.size() - 1);
        for (auto* c : n->children) {
            c->out_x = x;
            x += c->out_w + c->mar_l + c->mar_r + space;
        }
        x = n->pad_l;
    }
    for (auto* c : n->children) {
        c->out_x = x + c->mar_l;
        if (n->align == 3) c->out_h = avail_h - c->mar_t - c->mar_b;
        else if (n->align == 1) c->out_y = n->pad_t + (avail_h - c->out_h) / 2;
        else if (n->align == 2) c->out_y = n->pad_t + avail_h - c->out_h - c->mar_b;
        else c->out_y = n->pad_t + c->mar_t;
        x += c->out_w + c->mar_l + c->mar_r + n->gap;
        layout_row(c, c->out_w, c->out_h);
    }
}

extern "C" {

void* aurora_app_layout_create(void) {
    return node_new(nullptr);
}

void aurora_app_layout_destroy(void* layout) {
    delete (AppLayoutNode*)layout;
}

void* aurora_app_layout_node_new(void* parent, int type) {
    auto* n = node_new((AppLayoutNode*)parent);
    n->type = type;
    return n;
}

void aurora_app_layout_node_set_flex(void* node, int grow, int shrink, int basis) {
    auto* n = (AppLayoutNode*)node;
    n->grow = grow;
    n->shrink = shrink;
    n->basis = basis;
}

void aurora_app_layout_node_set_direction(void* node, int dir) {
    ((AppLayoutNode*)node)->direction = dir ? APP_LAYOUT_ROW : APP_LAYOUT_COLUMN;
}

void aurora_app_layout_node_set_justify(void* node, int justify) {
    ((AppLayoutNode*)node)->justify = justify;
}

void aurora_app_layout_node_set_align(void* node, int align) {
    ((AppLayoutNode*)node)->align = align;
}

void aurora_app_layout_node_set_wrap(void* node, int wrap) {
    ((AppLayoutNode*)node)->wrap = wrap;
}

void aurora_app_layout_node_set_gap(void* node, float gap) {
    ((AppLayoutNode*)node)->gap = gap;
}

void aurora_app_layout_node_set_size(void* node, float w, float h) {
    auto* n = (AppLayoutNode*)node;
    n->w = w; n->h = h;
}

void aurora_app_layout_node_set_min_size(void* node, float w, float h) {
    auto* n = (AppLayoutNode*)node;
    n->min_w = w; n->min_h = h;
}

void aurora_app_layout_node_set_max_size(void* node, float w, float h) {
    auto* n = (AppLayoutNode*)node;
    n->max_w = w; n->max_h = h;
}

void aurora_app_layout_node_set_padding(void* node, float l, float t, float r, float b) {
    auto* n = (AppLayoutNode*)node;
    n->pad_l = l; n->pad_t = t; n->pad_r = r; n->pad_b = b;
}

void aurora_app_layout_node_set_margin(void* node, float l, float t, float r, float b) {
    auto* n = (AppLayoutNode*)node;
    n->mar_l = l; n->mar_t = t; n->mar_r = r; n->mar_b = b;
}

void aurora_app_layout_calculate(void* root, float width, float height) {
    auto* n = (AppLayoutNode*)root;
    n->out_w = width; n->out_h = height;
    if (n->direction == APP_LAYOUT_ROW)
        layout_row(n, width, height);
    else
        layout_column(n, width, height);
}

float aurora_app_layout_node_get_x(void* node) {
    return ((AppLayoutNode*)node)->out_x;
}

float aurora_app_layout_node_get_y(void* node) {
    return ((AppLayoutNode*)node)->out_y;
}

float aurora_app_layout_node_get_w(void* node) {
    return ((AppLayoutNode*)node)->out_w;
}

float aurora_app_layout_node_get_h(void* node) {
    return ((AppLayoutNode*)node)->out_h;
}

} // extern "C"
