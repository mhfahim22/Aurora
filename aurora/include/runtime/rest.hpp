#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Opaque types ── */
typedef struct AuroraModelSchema AuroraModelSchema;
typedef struct AuroraRouter AuroraRouter;

/* ── Hook types ── */
#define AURORA_REST_HOOK_BEFORE_CREATE 0
#define AURORA_REST_HOOK_AFTER_CREATE  1
#define AURORA_REST_HOOK_BEFORE_UPDATE 2
#define AURORA_REST_HOOK_AFTER_UPDATE  3
#define AURORA_REST_HOOK_BEFORE_DELETE 4
#define AURORA_REST_HOOK_AFTER_DELETE  5
#define AURORA_REST_HOOK_BEFORE_FIND   6
#define AURORA_REST_HOOK_AFTER_FIND    7
#define AURORA_REST_HOOK_BEFORE_LIST   8
#define AURORA_REST_HOOK_AFTER_LIST    9

/* ── Auto-REST resource generation ── */

/* Auto-register CRUD routes for a model schema on the given router.
   Routes: GET    /{table}          → list all
           GET    /{table}/:id      → find by id
           POST   /{table}          → create
           PUT    /{table}/:id      → update
           DELETE /{table}/:id      → delete
   A db connection handle must be provided for queries.
   Returns 0 on success, -1 on failure. */
int aurora_rest_register(AuroraRouter* router, void* db,
                          AuroraModelSchema* schema, const char* prefix);

/* Unregister REST routes for the schema (removes them from the router).
   Returns 0 on success, -1 on failure. */
int aurora_rest_unregister(AuroraRouter* router, AuroraModelSchema* schema);

/* Set a hook callback for a model. The callback receives
   (AuroraHttpRequest*, AuroraHttpResponse*, void* user_data)
   and returns 0 to continue or non-zero to abort.
   Returns 0 on success. */
int aurora_rest_set_hook(AuroraRouter* router, AuroraModelSchema* schema,
                          int hook_type, void* callback, void* user_data);

#ifdef __cplusplus
}
#endif
