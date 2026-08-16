#pragma once

#if defined(__wasm__) || defined(__wasm32__)
  typedef long long int64_t;
  typedef unsigned long long uint64_t;
  typedef unsigned int uint32_t;
  typedef unsigned short uint16_t;
  typedef unsigned char uint8_t;
  typedef __SIZE_TYPE__ size_t;
  typedef __PTRDIFF_TYPE__ ptrdiff_t;
#else
  #include <cstdint>
  #include <cstddef>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ── Browser DOM bindings (Phase 40) ──
   On native (desktop/server) builds these are no-op stubs returning -1.
   On wasm32 builds they delegate to JavaScript through imported functions
   wired up by the JS glue (see aurora/src/runtime/wasm/dom.cpp).

   NOTE: int-typed params/returns are int64_t to match the Aurora codegen
   (Aurora `int` maps to i64 in LLVM IR). Element handles are int64_t ids. */

/* Document / element creation */
int64_t     aurora_dom_init(void);
int64_t     aurora_dom_create_element(const char* tag);
int64_t     aurora_dom_remove_element(int64_t elem);
int64_t     aurora_dom_document_body(void);
int64_t     aurora_dom_append_child(int64_t parent, int64_t child);
int64_t     aurora_dom_insert_before(int64_t parent, int64_t child, int64_t ref);
int64_t     aurora_dom_replace_child(int64_t parent, int64_t new_child, int64_t old_child);

/* Content */
int64_t     aurora_dom_set_text(int64_t elem, const char* text);
const char* aurora_dom_get_text(int64_t elem);
int64_t     aurora_dom_set_html(int64_t elem, const char* html);
int64_t     aurora_dom_set_attr(int64_t elem, const char* name, const char* value);
const char* aurora_dom_get_attr(int64_t elem, const char* name);
int64_t     aurora_dom_set_prop(int64_t elem, const char* name, const char* value);
int64_t     aurora_dom_set_value(int64_t elem, const char* value);
const char* aurora_dom_get_value(int64_t elem);
int64_t     aurora_dom_clear_children(int64_t elem);

/* Style / class */
int64_t     aurora_dom_set_style(int64_t elem, const char* prop, const char* value);
const char* aurora_dom_get_style(int64_t elem, const char* prop);
int64_t     aurora_dom_add_class(int64_t elem, const char* cls);
int64_t     aurora_dom_remove_class(int64_t elem, const char* cls);
int64_t     aurora_dom_toggle_class(int64_t elem, const char* cls, int64_t force);
int64_t     aurora_dom_has_class(int64_t elem, const char* cls);

/* Events */
int64_t     aurora_dom_add_event_listener(int64_t elem, const char* type, void (*fn)(void* event));
int64_t     aurora_dom_remove_event_listener(int64_t elem, const char* type, void (*fn)(void* event));
int64_t     aurora_dom_dispatch_event(int64_t elem, const char* type);
int64_t     aurora_dom_prevent_default(void* event);
int64_t     aurora_dom_event_target(void* event);

/* Query */
int64_t     aurora_dom_get_element_by_id(const char* id);
int64_t     aurora_dom_query_selector(const char* selector);
int64_t     aurora_dom_child_count(int64_t elem);
int64_t     aurora_dom_child_at(int64_t elem, int64_t index);
int64_t     aurora_dom_parent(int64_t elem);

/* Window / document state */
int64_t     aurora_dom_window_inner_width(void);
int64_t     aurora_dom_window_inner_height(void);
int64_t     aurora_dom_set_title(const char* title);
int64_t     aurora_dom_set_body_style(const char* prop, const char* value);
int64_t     aurora_dom_focus(int64_t elem);

/* Event-callback bridge */
void        aurora_dom_set_event_cb(void (*fn)(void* event));
void*       aurora_dom_get_event_cb(void);

#ifdef __cplusplus
}
#endif