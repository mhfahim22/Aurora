#pragma once
#include <cstdint>
#include "common/platform.hpp"
#include "gui.hpp"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Opaque types ── */
typedef struct AuroraReactiveState AuroraReactiveState;
typedef struct AuroraReactiveList  AuroraReactiveList;

/* ── Callback types ── */
typedef void  (*AuroraReactiveCallback)(void* value, void* user_data);
typedef void* (*AuroraComputeFn)(void* user_data);
typedef void  (*AuroraEffectFn)(void* user_data);
typedef void  (*AuroraLifecycleFn)(void* user_data);

/* ════════════════════════════════════════════════════════════
   State
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraReactiveState* aurora_reactive_state_new(void* initial);
AURORA_EXPORT void                 aurora_reactive_state_set(AuroraReactiveState* state, void* value);
AURORA_EXPORT void*                aurora_reactive_state_get(AuroraReactiveState* state);
AURORA_EXPORT void                 aurora_reactive_state_delete(AuroraReactiveState* state);

/* ════════════════════════════════════════════════════════════
   Subscription
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT int  aurora_reactive_subscribe(AuroraReactiveState* state, AuroraReactiveCallback cb, void* user_data);
AURORA_EXPORT void aurora_reactive_unsubscribe(AuroraReactiveState* state, int id);
AURORA_EXPORT void aurora_reactive_notify(AuroraReactiveState* state);

/* ════════════════════════════════════════════════════════════
   Computed
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraReactiveState* aurora_reactive_computed_new(AuroraComputeFn fn, void* user_data);
AURORA_EXPORT void*                aurora_reactive_computed_get(AuroraReactiveState* computed);

/* ════════════════════════════════════════════════════════════
   Effect
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT int  aurora_reactive_effect(AuroraEffectFn fn, void* user_data);
AURORA_EXPORT void aurora_reactive_effect_clear(int id);

/* ════════════════════════════════════════════════════════════
   Binding (connect state → widget property)
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT int  aurora_reactive_bind_text(AuroraWidget widget, AuroraReactiveState* state);
AURORA_EXPORT int  aurora_reactive_bind_enabled(AuroraWidget widget, AuroraReactiveState* state);
AURORA_EXPORT int  aurora_reactive_bind_visible(AuroraWidget widget, AuroraReactiveState* state);
AURORA_EXPORT int  aurora_reactive_bind_checked(AuroraWidget widget, AuroraReactiveState* state);
AURORA_EXPORT int  aurora_reactive_bind_value(AuroraWidget widget, AuroraReactiveState* state);
AURORA_EXPORT int  aurora_reactive_bind_label(AuroraWidget widget, AuroraReactiveState* state);
AURORA_EXPORT void aurora_reactive_unbind(AuroraWidget widget, int id);
AURORA_EXPORT void aurora_reactive_unbind_all(AuroraWidget widget);

/* ════════════════════════════════════════════════════════════
   Reactive List
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraReactiveList* aurora_reactive_list_new(void);
AURORA_EXPORT void                aurora_reactive_list_add(AuroraReactiveList* list, void* item);
AURORA_EXPORT void                aurora_reactive_list_insert(AuroraReactiveList* list, int index, void* item);
AURORA_EXPORT void*               aurora_reactive_list_get(AuroraReactiveList* list, int index);
AURORA_EXPORT void                aurora_reactive_list_set(AuroraReactiveList* list, int index, void* item);
AURORA_EXPORT void                aurora_reactive_list_remove(AuroraReactiveList* list, int index);
AURORA_EXPORT int                 aurora_reactive_list_size(AuroraReactiveList* list);
AURORA_EXPORT void                aurora_reactive_list_clear(AuroraReactiveList* list);
AURORA_EXPORT void                aurora_reactive_list_delete(AuroraReactiveList* list);
AURORA_EXPORT int                 aurora_reactive_list_subscribe(AuroraReactiveList* list, AuroraReactiveCallback cb, void* user_data);
AURORA_EXPORT void                aurora_reactive_list_unsubscribe(AuroraReactiveList* list, int id);

/* ════════════════════════════════════════════════════════════
   Lifecycle hooks
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT int  aurora_reactive_on_change(AuroraReactiveState* state, AuroraReactiveCallback cb, void* user_data);
AURORA_EXPORT void aurora_reactive_on_mount(AuroraWidget widget, AuroraLifecycleFn fn, void* user_data);
AURORA_EXPORT void aurora_reactive_on_unmount(AuroraWidget widget, AuroraLifecycleFn fn, void* user_data);

#ifdef __cplusplus
}
#endif
