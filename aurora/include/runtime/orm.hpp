#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

#ifdef __cplusplus

struct OrmColumn {
    std::string name;
    int type;
    int flags;
    std::string default_val;
};

struct AuroraModelSchema {
    std::string table_name;
    std::vector<OrmColumn> columns;
};

extern "C" {
#endif

/* ── Column type constants ── */
#define AURORA_ORM_TYPE_INT    0
#define AURORA_ORM_TYPE_FLOAT  1
#define AURORA_ORM_TYPE_TEXT   2
#define AURORA_ORM_TYPE_BOOL   3
#define AURORA_ORM_TYPE_JSON   4

/* ── Column flags ── */
#define AURORA_ORM_FLAG_NONE      0
#define AURORA_ORM_FLAG_PRIMARY   1
#define AURORA_ORM_FLAG_AUTO      2
#define AURORA_ORM_FLAG_UNIQUE    4
#define AURORA_ORM_FLAG_NOT_NULL  8
#define AURORA_ORM_FLAG_INDEXED   16

/* ── Opaque types ── */
typedef struct AuroraModelSchema AuroraModelSchema;
typedef struct AuroraModelResult AuroraModelResult;

/* ── Schema definition ── */

/* Define a new model schema with the given table name */
AuroraModelSchema* aurora_orm_schema_define(const char* table_name);

/* Add a column to the schema:
   type: AURORA_ORM_TYPE_*, flags: bitmask of AURORA_ORM_FLAG_*
   default_val: NULL or string representation of default value */
void aurora_orm_schema_column(AuroraModelSchema* schema, const char* name,
                               int type, int flags, const char* default_val);

/* Free a schema (does not drop table) */
void aurora_orm_schema_free(AuroraModelSchema* schema);

/* ── CRUD operations ── */
/* All take the schema and a SQLite database handle (void* db) */

/* INSERT: returns last_insert_rowid, or -1 on failure */
int64_t aurora_orm_create(void* db, AuroraModelSchema* schema, const char* json_data);

/* SELECT by id: returns JSON string (caller must free), or NULL */
char* aurora_orm_find(void* db, AuroraModelSchema* schema, int64_t id);

/* SELECT by field=value: returns JSON array string (caller must free), or NULL */
char* aurora_orm_find_by(void* db, AuroraModelSchema* schema,
                          const char* field, const char* value);

/* SELECT all: returns JSON array string (caller must free), or NULL */
char* aurora_orm_all(void* db, AuroraModelSchema* schema);

/* SELECT with conditions: conditions_json is like [["field","op","value"],...]
   Returns JSON array string (caller must free), or NULL */
char* aurora_orm_where(void* db, AuroraModelSchema* schema, const char* conditions_json);

/* UPDATE by id with json_data (partial update supported).
   Returns number of rows affected, or -1 on failure */
int aurora_orm_update(void* db, AuroraModelSchema* schema, int64_t id, const char* json_data);

/* DELETE by id. Returns number of rows affected, or -1 on failure */
int aurora_orm_delete(void* db, AuroraModelSchema* schema, int64_t id);

/* SELECT COUNT(*) for this table. Returns count, or -1 on failure */
int64_t aurora_orm_count(void* db, AuroraModelSchema* schema);

/* ── Migration ── */

/* Auto-create/update table from schema definition.
   Returns 0 on success, -1 on failure */
int aurora_orm_auto_migrate(void* db, AuroraModelSchema* schema);

/* Drop the table associated with the schema.
   Returns 0 on success, -1 on failure */
int aurora_orm_drop_table(void* db, AuroraModelSchema* schema);

/* ── Schema query helpers ── */

/* Get the table name from a schema */
const char* aurora_orm_schema_table_name(AuroraModelSchema* schema);

/* Get column count */
int aurora_orm_schema_column_count(AuroraModelSchema* schema);

/* Get column info by index */
const char* aurora_orm_schema_column_name(AuroraModelSchema* schema, int index);
int aurora_orm_schema_column_type(AuroraModelSchema* schema, int index);
int aurora_orm_schema_column_flags(AuroraModelSchema* schema, int index);

#ifdef __cplusplus
}
#endif
