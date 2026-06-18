#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include "runtime/memory.hpp"
#include "runtime/ui/component.h"

extern "C" {

/* ── Global component root ── */
static AuroraComponent* root_component = nullptr;

/* ── Create a new component ── */
AuroraComponent* aurora_component_create(const char* name, int x, int y, int w, int h) {
    AuroraComponent* c = (AuroraComponent*)calloc(1, sizeof(AuroraComponent));
    c->name = strdup(name ? name : "unnamed");
    c->x = x; c->y = y; c->w = w; c->h = h;
    c->visible = 1;
    c->state = nullptr;
    c->render_fn = nullptr;
    c->update_fn = nullptr;
    c->parent = nullptr;
    c->children = nullptr;
    c->child_count = 0;
    return c;
}

/* ── Destroy a component and all children ── */
void aurora_component_destroy(AuroraComponent* c) {
    if (!c) return;
    for (int i = 0; i < c->child_count; i++)
        aurora_component_destroy(c->children[i]);
    free(c->children);
    free(c->name);
    free(c->state);
    free(c);
}

/* ── Add a child component ── */
void aurora_component_add_child(AuroraComponent* parent, AuroraComponent* child) {
    if (!parent || !child) return;
    child->parent = parent;
    parent->child_count++;
    parent->children = (AuroraComponent**)aurora_safe_realloc(parent->children,
        (size_t)parent->child_count * sizeof(AuroraComponent*));
    parent->children[parent->child_count - 1] = child;
}

/* ── Set component position ── */
void aurora_component_set_pos(AuroraComponent* c, int x, int y) {
    if (!c) return;
    c->x = x; c->y = y;
}

/* ── Set component size ── */
void aurora_component_set_size(AuroraComponent* c, int w, int h) {
    if (!c) return;
    c->w = w; c->h = h;
}

/* ── Set component state ── */
void aurora_component_set_state(AuroraComponent* c, void* state) {
    if (!c) return;
    c->state = state;
}

/* ── Set render callback ── */
void aurora_component_set_render_fn(AuroraComponent* c, void (*fn)(void*, int, int, int, int)) {
    if (!c) return;
    c->render_fn = fn;
}

/* ── Set update callback ── */
void aurora_component_set_update_fn(AuroraComponent* c, void (*fn)(void*, double)) {
    if (!c) return;
    c->update_fn = fn;
}

/* ── Show/hide component ── */
void aurora_component_show(AuroraComponent* c) { if (c) c->visible = 1; }
void aurora_component_hide(AuroraComponent* c) { if (c) c->visible = 0; }

/* ── Set widget type (0=container,1=button,2=label,3=textbox,4=listbox) ── */
void aurora_component_set_widget_type(AuroraComponent* c, int type) {
    if (c) c->widget_type = type;
}

/* ── Mount component tree ── */
void aurora_component_mount(AuroraComponent* c) {
    if (!c) return;
    root_component = c;
}

/* ── Render component tree ── */
void aurora_component_render_tree(AuroraComponent* c) {
    if (!c || !c->visible) return;
    if (c->render_fn) c->render_fn(c->state, c->x, c->y, c->w, c->h);
    for (int i = 0; i < c->child_count; i++)
        aurora_component_render_tree(c->children[i]);
}

/* ── Update component tree ── */
void aurora_component_update_tree(AuroraComponent* c, double dt) {
    if (!c || !c->visible) return;
    if (c->update_fn) c->update_fn(c->state, dt);
    for (int i = 0; i < c->child_count; i++)
        aurora_component_update_tree(c->children[i], dt);
}

/* ── Route registration ── */
typedef struct { char path[256]; void* handler; } Route;
static Route routes[64];
static int route_count = 0;

void aurora_ui_route_register(const char* path, void* handler) {
    if (!path || route_count >= 64) return;
    strncpy(routes[route_count].path, path, sizeof(routes[route_count].path) - 1);
    routes[route_count].handler = handler;
    route_count++;
}

/* ── Style application (simple key-value pairs) ── */
typedef struct { char key[64]; char val[256]; } StyleRule;
static StyleRule style_rules[128];
static int style_rule_count = 0;

void aurora_style_apply(const char* target, const char* rules) {
    if (!target || !rules || style_rule_count >= 128) return;
    strncpy(style_rules[style_rule_count].key, target, sizeof(style_rules[style_rule_count].key) - 1);
    strncpy(style_rules[style_rule_count].val, rules, sizeof(style_rules[style_rule_count].val) - 1);
    style_rule_count++;
}

}
