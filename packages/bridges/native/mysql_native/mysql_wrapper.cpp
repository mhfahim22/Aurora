/* ════════════════════════════════════════════════════════════
   MySQL Native Bridge — C++ Helper Wrapper
   Provides field-level access to MYSQL_ROW (char**) results
   and MYSQL_FIELD metadata
   ════════════════════════════════════════════════════════════ */

#include <stdint.h>

/* MySQL C API struct definitions (avoid requiring mysql.h) */
struct MYSQL_FIELD {
    char* name;        /* Name of column */
    char* org_name;    /* Original column name */
    char* table;       /* Table of column if applicable */
    char* org_table;   /* Original table name */
    char* db;          /* Database for column */
    char* catalog;     /* Catalog for column */
    char* def;         /* Default value (set by mysql_list_fields) */
    unsigned long length;      /* Width of column (create length) */
    unsigned long max_length;  /* Max width for selected set */
    unsigned int name_length;
    unsigned int org_name_length;
    unsigned int table_length;
    unsigned int org_table_length;
    unsigned int db_length;
    unsigned int catalog_length;
    unsigned int def_length;
    unsigned int flags;         /* Flags (enum_field_flags) */
    unsigned int decimals;      /* Number of decimals in field */
    unsigned int charsetnr;     /* Character set */
    unsigned char* extension;
};

/* ── Get field value from a MYSQL_ROW at given column index ── */
extern "C" const char* mysql_native_get_field(const char** row, int col)
{
    if (!row) return 0;
    return row[col];
}

/* ── Get field length from lengths array at given column ── */
extern "C" unsigned long mysql_native_get_length(const unsigned long* lengths, int col)
{
    if (!lengths) return 0;
    return lengths[col];
}

/* ── Check if a field at column is NULL ── */
extern "C" int mysql_native_field_is_null(const char** row, int col)
{
    if (!row) return 1;
    return row[col] == 0;
}

/* ── Get field name from MYSQL_FIELD struct pointer ── */
extern "C" const char* mysql_native_field_name(const void* field_ptr)
{
    if (!field_ptr) return 0;
    const MYSQL_FIELD* field = (const MYSQL_FIELD*)field_ptr;
    return field->name;
}
