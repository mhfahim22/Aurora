#include "runtime/orm.hpp"
#include "std/db.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <sstream>

/* ── strdup for platforms that don't have it (MSVC) ── */
#if defined(_MSC_VER) && !defined(strdup)
#define strdup _strdup
#endif

/* ── Internal structures ── */

/* ── Global schema registry ── */
/* Used by backend builtins to track model schemas across calls */

static std::vector<AuroraModelSchema*> g_orm_schemas;
static AuroraModelSchema* g_orm_last = nullptr;
void* g_orm_default_db = nullptr;

extern "C" {

void aurora_orm_register_schema(AuroraModelSchema* s) {
    if (!s) return;
    g_orm_schemas.push_back(s);
    g_orm_last = s;
}

AuroraModelSchema* aurora_orm_last_schema() {
    return g_orm_last;
}

void aurora_orm_set_default_db(void* db) {
    g_orm_default_db = db;
}

int aurora_orm_migrate_all() {
    int count = 0;
    for (auto* s : g_orm_schemas) {
        if (s && g_orm_default_db) {
            if (aurora_orm_auto_migrate(g_orm_default_db, s) == 0)
                count++;
        }
    }
    return count;
}

}

/* ── Schema definition ── */

AuroraModelSchema* aurora_orm_schema_define(const char* table_name) {
    if (!table_name) return nullptr;
    AuroraModelSchema* s = new AuroraModelSchema();
    s->table_name = table_name;
    /* Always add an 'id' primary key column by default */
    OrmColumn def;
    def.name = "id";
    def.type = AURORA_ORM_TYPE_INT;
    def.flags = AURORA_ORM_FLAG_PRIMARY | AURORA_ORM_FLAG_AUTO;
    def.default_val = "";
    s->columns.push_back(def);
    return s;
}

void aurora_orm_schema_column(AuroraModelSchema* schema, const char* name,
                               int type, int flags, const char* default_val) {
    if (!schema || !name) return;
    OrmColumn def;
    def.name = name;
    def.type = type;
    def.flags = flags;
    def.default_val = default_val ? default_val : "";
    schema->columns.push_back(def);
}

void aurora_orm_schema_free(AuroraModelSchema* schema) {
    delete schema;
}

/* ── Helpers ── */

static std::string col_type_to_sql(int type) {
    switch (type) {
        case AURORA_ORM_TYPE_INT:   return "INTEGER";
        case AURORA_ORM_TYPE_FLOAT: return "REAL";
        case AURORA_ORM_TYPE_TEXT:  return "TEXT";
        case AURORA_ORM_TYPE_BOOL:  return "INTEGER";
        case AURORA_ORM_TYPE_JSON:  return "TEXT";
        default: return "TEXT";
    }
}

static std::string escape_sql(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\'') out += "''";
        else out += c;
    }
    return out;
}

/* ── Migration ── */

int aurora_orm_auto_migrate(void* db, AuroraModelSchema* schema) {
    if (!db || !schema) return -1;
    std::string sql = "CREATE TABLE IF NOT EXISTS " + schema->table_name + " (";
    for (size_t i = 0; i < schema->columns.size(); i++) {
        if (i > 0) sql += ", ";
        const auto& col = schema->columns[i];
        sql += col.name + " " + col_type_to_sql(col.type);
        if (col.flags & AURORA_ORM_FLAG_PRIMARY)   sql += " PRIMARY KEY";
        if (col.flags & AURORA_ORM_FLAG_AUTO)       sql += " AUTOINCREMENT";
        if (col.flags & AURORA_ORM_FLAG_UNIQUE)     sql += " UNIQUE";
        if (col.flags & AURORA_ORM_FLAG_NOT_NULL)   sql += " NOT NULL";
        if (!col.default_val.empty()) {
            if (col.type == AURORA_ORM_TYPE_INT || col.type == AURORA_ORM_TYPE_FLOAT)
                sql += " DEFAULT " + col.default_val;
            else
                sql += " DEFAULT '" + escape_sql(col.default_val) + "'";
        }
    }
    sql += ")";
    int rc = aurora_db_sqlite_execute(db, sql.c_str());
    /* Create indexes for indexed columns */
    for (const auto& col : schema->columns) {
        if (col.flags & AURORA_ORM_FLAG_INDEXED) {
            std::string idx_sql = "CREATE INDEX IF NOT EXISTS idx_" +
                schema->table_name + "_" + col.name + " ON " +
                schema->table_name + "(" + col.name + ")";
            aurora_db_sqlite_execute(db, idx_sql.c_str());
        }
    }
    return rc >= 0 ? 0 : -1;
}

int aurora_orm_drop_table(void* db, AuroraModelSchema* schema) {
    if (!db || !schema) return -1;
    std::string sql = "DROP TABLE IF EXISTS " + schema->table_name;
    return aurora_db_sqlite_execute(db, sql.c_str()) >= 0 ? 0 : -1;
}

/* ── JSON helper: build JSON object from SQLite row ── */

static std::string row_to_json(AuroraModelSchema* schema,
                                const char** col_names, const char** col_values, int col_count) {
    std::string json = "{";
    for (int i = 0; i < col_count; i++) {
        if (i > 0) json += ", ";
        json += "\"" + std::string(col_names[i]) + "\": ";
        if (col_values[i]) {
            /* Find column type */
            int ctype = AURORA_ORM_TYPE_TEXT;
            for (const auto& c : schema->columns) {
                if (c.name == col_names[i]) { ctype = c.type; break; }
            }
            if (ctype == AURORA_ORM_TYPE_INT || ctype == AURORA_ORM_TYPE_FLOAT || ctype == AURORA_ORM_TYPE_BOOL) {
                json += col_values[i];
            } else {
                std::string val = col_values[i];
                /* Escape JSON string */
                std::string escaped;
                for (char ch : val) {
                    switch (ch) {
                        case '"': escaped += "\\\""; break;
                        case '\\': escaped += "\\\\"; break;
                        case '\n': escaped += "\\n"; break;
                        case '\r': escaped += "\\r"; break;
                        case '\t': escaped += "\\t"; break;
                        default: escaped += ch;
                    }
                }
                json += "\"" + escaped + "\"";
            }
        } else {
            json += "null";
        }
    }
    json += "}";
    return json;
}

/* ── CRUD operations ── */

int64_t aurora_orm_create(void* db, AuroraModelSchema* schema, const char* json_data) {
    if (!db || !schema || !json_data) return -1;
    /* Parse JSON-like key:value pairs from a simple format.
       We support: {"col": val, "col2": "val2"} */
    std::string cols, vals;
    /* Very simple JSON object parser: skips { } and extracts key:value */
    const char* p = json_data;
    bool first = true;
    while (*p) {
        /* Skip whitespace and braces */
        while (*p && (*p == '{' || *p == '}' || *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        if (!*p) break;
        /* Expect key string */
        if (*p == '"') {
            p++;
            std::string key;
            while (*p && *p != '"') {
                if (*p == '\\') { p++; if (*p) { key += *p; p++; } }
                else { key += *p; p++; }
            }
            if (*p == '"') p++;
            /* Skip : */
            while (*p && (*p == ' ' || *p == ':')) p++;
            /* Skip value */
            if (*p == '"') {
                p++;
                std::string val;
                while (*p && *p != '"') {
                    if (*p == '\\') { p++; if (*p) { val += *p; p++; } }
                    else { val += *p; p++; }
                }
                if (*p == '"') p++;
                if (!first) { cols += ", "; vals += ", "; }
                cols += key;
                vals += "'" + escape_sql(val) + "'";
                first = false;
            } else if (*p == 't' || *p == 'f' || *p == 'n') {
                std::string lit;
                while (*p && *p != ',' && *p != '}' && *p != ' ' && *p != '\t') {
                    lit += *p; p++;
                }
                if (!first) { cols += ", "; vals += ", "; }
                cols += key;
                vals += lit;
                first = false;
            } else {
                std::string num;
                while (*p && *p != ',' && *p != '}' && *p != ' ' && *p != '\t') {
                    num += *p; p++;
                }
                if (!first) { cols += ", "; vals += ", "; }
                cols += key;
                vals += num;
                first = false;
            }
        }
        while (*p && *p != ',' && *p != '}') p++;
        if (*p == ',') p++;
    }
    if (cols.empty()) return -1;
    std::string sql = "INSERT INTO " + schema->table_name + " (" + cols + ") VALUES (" + vals + ")";
    int rc = aurora_db_sqlite_execute(db, sql.c_str());
    if (rc < 0) return -1;
    return aurora_db_sqlite_last_insert_rowid(db);
}

char* aurora_orm_find(void* db, AuroraModelSchema* schema, int64_t id) {
    if (!db || !schema) return nullptr;
    std::string sql = "SELECT * FROM " + schema->table_name + " WHERE id = " + std::to_string(id);
    void* stmt = aurora_db_sqlite_prepare(db, sql.c_str());
    if (!stmt) return nullptr;
    int rc = aurora_db_sqlite_step(stmt);
    if (rc != 0) { aurora_db_sqlite_finalize(stmt); return nullptr; }
    int cc = aurora_db_sqlite_column_count(stmt);
    std::vector<const char*> names(cc), values(cc);
    for (int i = 0; i < cc; i++) {
        names[i] = aurora_db_sqlite_column_name(stmt, i);
        values[i] = aurora_db_sqlite_column_text(stmt, i);
    }
    std::string json = row_to_json(schema, names.data(), values.data(), cc);
    aurora_db_sqlite_finalize(stmt);
    return strdup(json.c_str());
}

char* aurora_orm_find_by(void* db, AuroraModelSchema* schema,
                          const char* field, const char* value) {
    if (!db || !schema || !field) return nullptr;
    std::string sql = "SELECT * FROM " + schema->table_name + " WHERE " +
        std::string(field) + " = '" + escape_sql(value ? value : "") + "'";
    void* stmt = aurora_db_sqlite_prepare(db, sql.c_str());
    if (!stmt) return nullptr;
    std::string json_arr = "[";
    bool first = true;
    while (aurora_db_sqlite_step(stmt) == 0) {
        int cc = aurora_db_sqlite_column_count(stmt);
        std::vector<const char*> names(cc), values(cc);
        for (int i = 0; i < cc; i++) {
            names[i] = aurora_db_sqlite_column_name(stmt, i);
            values[i] = aurora_db_sqlite_column_text(stmt, i);
        }
        if (!first) json_arr += ", ";
        json_arr += row_to_json(schema, names.data(), values.data(), cc);
        first = false;
    }
    json_arr += "]";
    aurora_db_sqlite_finalize(stmt);
    return strdup(json_arr.c_str());
}

char* aurora_orm_all(void* db, AuroraModelSchema* schema) {
    if (!db || !schema) return nullptr;
    return aurora_orm_find_by(db, schema, "1", "1");
}

char* aurora_orm_where(void* db, AuroraModelSchema* schema, const char* conditions_json) {
    if (!db || !schema) return aurora_orm_all(db, schema);
    if (!conditions_json || !*conditions_json) return aurora_orm_all(db, schema);
    /* Parse simple conditions: [["field","op","value"], ...] */
    std::string where_clause;
    const char* p = conditions_json;
    bool first = true;
    while (*p) {
        while (*p && (*p == '[' || *p == ' ' || *p == '\t' || *p == '\n')) p++;
        if (!*p || *p == ']') break;
        if (*p == '"') {
            p++;
            std::string field, op, value;
            while (*p && *p != '"') { field += *p; p++; }
            if (*p == '"') p++;
            while (*p && (*p == ' ' || *p == ',')) p++;
            if (*p == '"') { p++; while (*p && *p != '"') { op += *p; p++; } if (*p == '"') p++; }
            while (*p && (*p == ' ' || *p == ',')) p++;
            if (*p == '"') { p++; while (*p && *p != '"') { value += *p; p++; } if (*p == '"') p++; }
            if (!field.empty() && !op.empty()) {
                if (!first) where_clause += " AND ";
                where_clause += field + " " + op + " '" + escape_sql(value) + "'";
                first = false;
            }
        }
        while (*p && *p != ',' && *p != ']') p++;
        if (*p == ',') p++;
    }
    std::string sql = "SELECT * FROM " + schema->table_name;
    if (!where_clause.empty()) sql += " WHERE " + where_clause;
    void* stmt = aurora_db_sqlite_prepare(db, sql.c_str());
    if (!stmt) return nullptr;
    std::string json_arr = "[";
    first = true;
    while (aurora_db_sqlite_step(stmt) == 0) {
        int cc = aurora_db_sqlite_column_count(stmt);
        std::vector<const char*> names(cc), values(cc);
        for (int i = 0; i < cc; i++) {
            names[i] = aurora_db_sqlite_column_name(stmt, i);
            values[i] = aurora_db_sqlite_column_text(stmt, i);
        }
        if (!first) json_arr += ", ";
        json_arr += row_to_json(schema, names.data(), values.data(), cc);
        first = false;
    }
    json_arr += "]";
    aurora_db_sqlite_finalize(stmt);
    return strdup(json_arr.c_str());
}

int aurora_orm_update(void* db, AuroraModelSchema* schema, int64_t id, const char* json_data) {
    if (!db || !schema || !json_data) return -1;
    std::string set_clause;
    const char* p = json_data;
    bool first = true;
    while (*p) {
        while (*p && (*p == '{' || *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        if (!*p || *p == '}') break;
        if (*p == '"') {
            p++;
            std::string key;
            while (*p && *p != '"') {
                if (*p == '\\') { p++; if (*p) { key += *p; p++; } }
                else { key += *p; p++; }
            }
            if (*p == '"') p++;
            while (*p && (*p == ' ' || *p == ':')) p++;
            if (!first) set_clause += ", ";
            if (*p == '"') {
                p++;
                std::string val;
                while (*p && *p != '"') {
                    if (*p == '\\') { p++; if (*p) { val += *p; p++; } }
                    else { val += *p; p++; }
                }
                if (*p == '"') p++;
                set_clause += key + " = '" + escape_sql(val) + "'";
            } else {
                std::string val;
                while (*p && *p != ',' && *p != '}' && *p != ' ' && *p != '\t') {
                    val += *p; p++;
                }
                set_clause += key + " = " + val;
            }
            first = false;
        }
        while (*p && *p != ',' && *p != '}') p++;
        if (*p == ',') p++;
    }
    if (set_clause.empty()) return -1;
    std::string sql = "UPDATE " + schema->table_name + " SET " + set_clause + " WHERE id = " + std::to_string(id);
    int rc = aurora_db_sqlite_execute(db, sql.c_str());
    return rc;
}

int aurora_orm_delete(void* db, AuroraModelSchema* schema, int64_t id) {
    if (!db || !schema) return -1;
    std::string sql = "DELETE FROM " + schema->table_name + " WHERE id = " + std::to_string(id);
    return aurora_db_sqlite_execute(db, sql.c_str());
}

int64_t aurora_orm_count(void* db, AuroraModelSchema* schema) {
    if (!db || !schema) return -1;
    std::string sql = "SELECT COUNT(*) FROM " + schema->table_name;
    void* stmt = aurora_db_sqlite_prepare(db, sql.c_str());
    if (!stmt) return -1;
    int64_t count = 0;
    if (aurora_db_sqlite_step(stmt) == 0) {
        count = aurora_db_sqlite_column_int(stmt, 0);
    }
    aurora_db_sqlite_finalize(stmt);
    return count;
}

/* ── Schema query helpers ── */

const char* aurora_orm_schema_table_name(AuroraModelSchema* schema) {
    if (!schema) return nullptr;
    return schema->table_name.c_str();
}

int aurora_orm_schema_column_count(AuroraModelSchema* schema) {
    if (!schema) return 0;
    return (int)schema->columns.size();
}

const char* aurora_orm_schema_column_name(AuroraModelSchema* schema, int index) {
    if (!schema || index < 0 || index >= (int)schema->columns.size()) return nullptr;
    return schema->columns[index].name.c_str();
}

int aurora_orm_schema_column_type(AuroraModelSchema* schema, int index) {
    if (!schema || index < 0 || index >= (int)schema->columns.size()) return 99;
    return schema->columns[index].type;
}

int aurora_orm_schema_column_flags(AuroraModelSchema* schema, int index) {
    if (!schema || index < 0 || index >= (int)schema->columns.size()) return 0;
    return schema->columns[index].flags;
}
