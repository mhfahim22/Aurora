#include "std/dom.hpp"

/* ── Browser DOM bindings (Phase 40) ──
   Native builds: no-op stubs (return -1 / empty). Keeps the symbol table
   complete so JIT + desktop linking never fail.
   wasm32 builds: delegating implementations that call JS-imported functions
   provided by the JS glue (aurora/src/runtime/wasm/wasm_glue.js).

   All int-typed params/returns are int64_t to match the Aurora codegen
   (Aurora `int` maps to i64 in LLVM IR). */

#if defined(__wasm__) || defined(__wasm32__) || defined(AURORA_PLATFORM_WASM)
  #define AURORA_WASM_DOM 1
#else
  #define AURORA_WASM_DOM 0
#endif

/* ── Imported JavaScript functions (resolved by the JS glue) ── */
#if AURORA_WASM_DOM
extern "C" int64_t aurora_js_dom_init(void);
extern "C" int64_t aurora_js_dom_create_element(const char* tag);
extern "C" int64_t aurora_js_dom_remove_element(int64_t elem);
extern "C" int64_t aurora_js_dom_document_body(void);
extern "C" int64_t aurora_js_dom_append_child(int64_t parent, int64_t child);
extern "C" int64_t aurora_js_dom_insert_before(int64_t parent, int64_t child, int64_t ref);
extern "C" int64_t aurora_js_dom_replace_child(int64_t parent, int64_t new_child, int64_t old_child);
extern "C" int64_t aurora_js_dom_set_text(int64_t elem, const char* text);
extern "C" const char* aurora_js_dom_get_text(int64_t elem);
extern "C" int64_t aurora_js_dom_set_html(int64_t elem, const char* html);
extern "C" int64_t aurora_js_dom_set_attr(int64_t elem, const char* name, const char* value);
extern "C" const char* aurora_js_dom_get_attr(int64_t elem, const char* name);
extern "C" int64_t aurora_js_dom_set_prop(int64_t elem, const char* name, const char* value);
extern "C" int64_t aurora_js_dom_set_value(int64_t elem, const char* value);
extern "C" const char* aurora_js_dom_get_value(int64_t elem);
extern "C" int64_t aurora_js_dom_clear_children(int64_t elem);
extern "C" int64_t aurora_js_dom_set_style(int64_t elem, const char* prop, const char* value);
extern "C" const char* aurora_js_dom_get_style(int64_t elem, const char* prop);
extern "C" int64_t aurora_js_dom_add_class(int64_t elem, const char* cls);
extern "C" int64_t aurora_js_dom_remove_class(int64_t elem, const char* cls);
extern "C" int64_t aurora_js_dom_toggle_class(int64_t elem, const char* cls, int64_t force);
extern "C" int64_t aurora_js_dom_has_class(int64_t elem, const char* cls);
extern "C" int64_t aurora_js_dom_add_event_listener(int64_t elem, const char* type, void* fn);
extern "C" int64_t aurora_js_dom_remove_event_listener(int64_t elem, const char* type, void* fn);
extern "C" int64_t aurora_js_dom_dispatch_event(int64_t elem, const char* type);
extern "C" int64_t aurora_js_dom_prevent_default(void* event);
extern "C" int64_t aurora_js_dom_event_target(void* event);
extern "C" int64_t aurora_js_dom_get_element_by_id(const char* id);
extern "C" int64_t aurora_js_dom_query_selector(const char* selector);
extern "C" int64_t aurora_js_dom_child_count(int64_t elem);
extern "C" int64_t aurora_js_dom_child_at(int64_t elem, int64_t index);
extern "C" int64_t aurora_js_dom_parent(int64_t elem);
extern "C" int64_t aurora_js_dom_window_inner_width(void);
extern "C" int64_t aurora_js_dom_window_inner_height(void);
extern "C" int64_t aurora_js_dom_set_title(const char* title);
extern "C" int64_t aurora_js_dom_set_body_style(const char* prop, const char* value);
extern "C" int64_t aurora_js_dom_focus(int64_t elem);
extern "C" void   aurora_js_dom_set_cb(void* fn);
extern "C" void*  aurora_js_dom_cb(void);
#endif

static const char kEmptyStr[1] = {0};

/* ── Public API ── */
extern "C" int64_t aurora_dom_init(void) {
#if AURORA_WASM_DOM
    return aurora_js_dom_init();
#else
    return -1;
#endif
}

extern "C" int64_t aurora_dom_create_element(const char* tag) {
#if AURORA_WASM_DOM
    return aurora_js_dom_create_element(tag ? tag : "div");
#else
    return -1;
#endif
}

extern "C" int64_t aurora_dom_remove_element(int64_t elem) {
#if AURORA_WASM_DOM
    return aurora_js_dom_remove_element(elem);
#else
    return -1;
#endif
}

extern "C" int64_t aurora_dom_document_body(void) {
#if AURORA_WASM_DOM
    return aurora_js_dom_document_body();
#else
    return -1;
#endif
}

extern "C" int64_t aurora_dom_append_child(int64_t parent, int64_t child) {
#if AURORA_WASM_DOM
    return aurora_js_dom_append_child(parent, child);
#else
    return -1;
#endif
}

extern "C" int64_t aurora_dom_insert_before(int64_t parent, int64_t child, int64_t ref) {
#if AURORA_WASM_DOM
    return aurora_js_dom_insert_before(parent, child, ref);
#else
    return -1;
#endif
}

extern "C" int64_t aurora_dom_replace_child(int64_t parent, int64_t new_child, int64_t old_child) {
#if AURORA_WASM_DOM
    return aurora_js_dom_replace_child(parent, new_child, old_child);
#else
    return -1;
#endif
}

extern "C" int64_t aurora_dom_set_text(int64_t elem, const char* text) {
#if AURORA_WASM_DOM
    return aurora_js_dom_set_text(elem, text ? text : "");
#else
    return -1;
#endif
}

extern "C" const char* aurora_dom_get_text(int64_t elem) {
#if AURORA_WASM_DOM
    const char* s = aurora_js_dom_get_text(elem);
    return s ? s : kEmptyStr;
#else
    return kEmptyStr;
#endif
}

extern "C" int64_t aurora_dom_set_html(int64_t elem, const char* html) {
#if AURORA_WASM_DOM
    return aurora_js_dom_set_html(elem, html ? html : "");
#else
    return -1;
#endif
}

extern "C" int64_t aurora_dom_set_attr(int64_t elem, const char* name, const char* value) {
#if AURORA_WASM_DOM
    return aurora_js_dom_set_attr(elem, name ? name : "", value ? value : "");
#else
    return -1;
#endif
}

extern "C" const char* aurora_dom_get_attr(int64_t elem, const char* name) {
#if AURORA_WASM_DOM
    const char* s = aurora_js_dom_get_attr(elem, name ? name : "");
    return s ? s : kEmptyStr;
#else
    return kEmptyStr;
#endif
}

extern "C" int64_t aurora_dom_set_prop(int64_t elem, const char* name, const char* value) {
#if AURORA_WASM_DOM
    return aurora_js_dom_set_prop(elem, name ? name : "", value ? value : "");
#else
    return -1;
#endif
}

extern "C" int64_t aurora_dom_set_value(int64_t elem, const char* value) {
#if AURORA_WASM_DOM
    return aurora_js_dom_set_value(elem, value ? value : "");
#else
    return -1;
#endif
}

extern "C" const char* aurora_dom_get_value(int64_t elem) {
#if AURORA_WASM_DOM
    const char* s = aurora_js_dom_get_value(elem);
    return s ? s : kEmptyStr;
#else
    return kEmptyStr;
#endif
}

extern "C" int64_t aurora_dom_clear_children(int64_t elem) {
#if AURORA_WASM_DOM
    return aurora_js_dom_clear_children(elem);
#else
    return -1;
#endif
}

extern "C" int64_t aurora_dom_set_style(int64_t elem, const char* prop, const char* value) {
#if AURORA_WASM_DOM
    return aurora_js_dom_set_style(elem, prop ? prop : "", value ? value : "");
#else
    return -1;
#endif
}

extern "C" const char* aurora_dom_get_style(int64_t elem, const char* prop) {
#if AURORA_WASM_DOM
    const char* s = aurora_js_dom_get_style(elem, prop ? prop : "");
    return s ? s : kEmptyStr;
#else
    return kEmptyStr;
#endif
}

extern "C" int64_t aurora_dom_add_class(int64_t elem, const char* cls) {
#if AURORA_WASM_DOM
    return aurora_js_dom_add_class(elem, cls ? cls : "");
#else
    return -1;
#endif
}

extern "C" int64_t aurora_dom_remove_class(int64_t elem, const char* cls) {
#if AURORA_WASM_DOM
    return aurora_js_dom_remove_class(elem, cls ? cls : "");
#else
    return -1;
#endif
}

extern "C" int64_t aurora_dom_toggle_class(int64_t elem, const char* cls, int64_t force) {
#if AURORA_WASM_DOM
    return aurora_js_dom_toggle_class(elem, cls ? cls : "", force);
#else
    return -1;
#endif
}

extern "C" int64_t aurora_dom_has_class(int64_t elem, const char* cls) {
#if AURORA_WASM_DOM
    return aurora_js_dom_has_class(elem, cls ? cls : "");
#else
    return 0;
#endif
}

extern "C" int64_t aurora_dom_add_event_listener(int64_t elem, const char* type, void (*fn)(void* event)) {
#if AURORA_WASM_DOM
    return aurora_js_dom_add_event_listener(elem, type ? type : "", (void*)fn);
#else
    return -1;
#endif
}

extern "C" int64_t aurora_dom_remove_event_listener(int64_t elem, const char* type, void (*fn)(void* event)) {
#if AURORA_WASM_DOM
    return aurora_js_dom_remove_event_listener(elem, type ? type : "", (void*)fn);
#else
    return -1;
#endif
}

extern "C" int64_t aurora_dom_dispatch_event(int64_t elem, const char* type) {
#if AURORA_WASM_DOM
    return aurora_js_dom_dispatch_event(elem, type ? type : "");
#else
    return -1;
#endif
}

extern "C" int64_t aurora_dom_prevent_default(void* event) {
#if AURORA_WASM_DOM
    return aurora_js_dom_prevent_default(event);
#else
    return -1;
#endif
}

extern "C" int64_t aurora_dom_event_target(void* event) {
#if AURORA_WASM_DOM
    return aurora_js_dom_event_target(event);
#else
    return -1;
#endif
}

extern "C" int64_t aurora_dom_get_element_by_id(const char* id) {
#if AURORA_WASM_DOM
    return aurora_js_dom_get_element_by_id(id ? id : "");
#else
    return -1;
#endif
}

extern "C" int64_t aurora_dom_query_selector(const char* selector) {
#if AURORA_WASM_DOM
    return aurora_js_dom_query_selector(selector ? selector : "");
#else
    return -1;
#endif
}

extern "C" int64_t aurora_dom_child_count(int64_t elem) {
#if AURORA_WASM_DOM
    return aurora_js_dom_child_count(elem);
#else
    return -1;
#endif
}

extern "C" int64_t aurora_dom_child_at(int64_t elem, int64_t index) {
#if AURORA_WASM_DOM
    return aurora_js_dom_child_at(elem, index);
#else
    return -1;
#endif
}

extern "C" int64_t aurora_dom_parent(int64_t elem) {
#if AURORA_WASM_DOM
    return aurora_js_dom_parent(elem);
#else
    return -1;
#endif
}

extern "C" int64_t aurora_dom_window_inner_width(void) {
#if AURORA_WASM_DOM
    return aurora_js_dom_window_inner_width();
#else
    return 0;
#endif
}

extern "C" int64_t aurora_dom_window_inner_height(void) {
#if AURORA_WASM_DOM
    return aurora_js_dom_window_inner_height();
#else
    return 0;
#endif
}

extern "C" int64_t aurora_dom_set_title(const char* title) {
#if AURORA_WASM_DOM
    return aurora_js_dom_set_title(title ? title : "");
#else
    return -1;
#endif
}

extern "C" int64_t aurora_dom_set_body_style(const char* prop, const char* value) {
#if AURORA_WASM_DOM
    return aurora_js_dom_set_body_style(prop ? prop : "", value ? value : "");
#else
    return -1;
#endif
}

extern "C" int64_t aurora_dom_focus(int64_t elem) {
#if AURORA_WASM_DOM
    return aurora_js_dom_focus(elem);
#else
    return -1;
#endif
}

extern "C" void aurora_dom_set_event_cb(void (*fn)(void* event)) {
#if AURORA_WASM_DOM
    aurora_js_dom_set_cb((void*)fn);
#else
    (void)fn;
#endif
}

extern "C" void* aurora_dom_get_event_cb(void) {
#if AURORA_WASM_DOM
    return aurora_js_dom_cb();
#else
    return nullptr;
#endif
}

#if AURORA_WASM_DOM
/* Force the empty string into .rodata so it is exported for JS reads. */
extern "C" const char* aurora_wasm_empty_string(void) { return kEmptyStr; }
#endif