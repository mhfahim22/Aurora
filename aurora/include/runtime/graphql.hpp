#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Opaque types ── */
typedef struct AuroraGQLSchema   AuroraGQLSchema;
typedef struct AuroraGQLType     AuroraGQLType;
typedef struct AuroraGQLField    AuroraGQLField;
typedef struct AuroraGQLResult   AuroraGQLResult;

/* ── Resolver signature: char* resolver(const char* parent_json, const char* args_json, void* ctx) ── */
typedef char* (*AuroraGQLResolver)(const char*, const char*, void*);

/* ── Schema ── */
AuroraGQLSchema* aurora_gql_schema_new(void);
void             aurora_gql_schema_free(AuroraGQLSchema* schema);

/* ── Type registration ── */
int  aurora_gql_type_add_object(AuroraGQLSchema* schema, const char* name, const char* description);
int  aurora_gql_type_add_enum(AuroraGQLSchema* schema, const char* name, const char* values_csv);
int  aurora_gql_type_add_scalar(AuroraGQLSchema* schema, const char* name);
int  aurora_gql_type_add_input(AuroraGQLSchema* schema, const char* name);

/* ── Field registration ── */
int aurora_gql_field_add(AuroraGQLSchema* schema, const char* type_name,
                         const char* field_name, const char* field_type,
                         const char* description, AuroraGQLResolver resolver, void* ctx);

/* ── Query type shortcuts ── */
int aurora_gql_query_add(AuroraGQLSchema* schema, const char* field_name,
                         const char* field_type, const char* description,
                         AuroraGQLResolver resolver, void* ctx);
int aurora_gql_mutation_add(AuroraGQLSchema* schema, const char* field_name,
                            const char* field_type, const char* description,
                            AuroraGQLResolver resolver, void* ctx);

/* ── Execution ── */
AuroraGQLResult* aurora_gql_execute(AuroraGQLSchema* schema, const char* query,
                                    const char* variables_json);
const char* aurora_gql_result_json(AuroraGQLResult* result);
const char* aurora_gql_result_errors(AuroraGQLResult* result);
int         aurora_gql_result_has_errors(AuroraGQLResult* result);
void        aurora_gql_result_free(AuroraGQLResult* result);

/* ── Parse SDL (Schema Definition Language) ── */
int aurora_gql_parse_sdl(AuroraGQLSchema* schema, const char* sdl);

/* ── Schema introspection ── */
char* aurora_gql_introspect(AuroraGQLSchema* schema);

/* ── GraphQL endpoint helper (call from route handler) ── */
void aurora_gql_handle_request(AuroraGQLSchema* schema, const char* body_json,
                               char* out_buffer, int out_size);

#ifdef __cplusplus
}
#endif
