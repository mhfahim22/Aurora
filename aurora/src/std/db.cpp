#include "std/db.hpp"
#include "std/json.hpp"
#include "sqlite3/sqlite3.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

/* ════════════════════════════════════════════════════════════
   Connection management
   ════════════════════════════════════════════════════════════ */

void* aurora_db_sqlite_open(const char* path) {
    if (!path) return nullptr;
    sqlite3* db = nullptr;
    int rc = sqlite3_open(path, &db);
    if (rc != SQLITE_OK) {
        if (db) {
            fprintf(stderr, "[sqlite] open failed: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
        }
        return nullptr;
    }
    return db;
}

void aurora_db_sqlite_close(void* db) {
    if (!db) return;
    sqlite3_close((sqlite3*)db);
}

/* ════════════════════════════════════════════════════════════
   Query execution
   ════════════════════════════════════════════════════════════ */

static std::string build_pipe_result(sqlite3* db, const char* sql) {
    std::string result;
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[sqlite] exec error: %s\n", sqlite3_errmsg(db));
        return result;
    }
    /* Column headers */
    int ncol = sqlite3_column_count(stmt);
    if (ncol > 0) {
        for (int i = 0; i < ncol; i++) {
            if (i > 0) result += '|';
            result += sqlite3_column_name(stmt, i);
        }
        result += '\n';
    }
    /* Rows */
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        for (int i = 0; i < ncol; i++) {
            if (i > 0) result += '|';
            const char* txt = (const char*)sqlite3_column_text(stmt, i);
            if (txt) result += txt;
        }
        result += '\n';
    }
    sqlite3_finalize(stmt);
    return result;
}

char* aurora_db_sqlite_exec(void* db, const char* sql) {
    if (!db || !sql) return strdup("");
    std::string result = build_pipe_result((sqlite3*)db, sql);
    return strdup(result.c_str());
}

static JsonValue* row_to_json_value(sqlite3_stmt* stmt) {
    int ncol = sqlite3_column_count(stmt);
    JsonValue* obj = aurora_json_new_object();
    for (int i = 0; i < ncol; i++) {
        const char* name = sqlite3_column_name(stmt, i);
        int type = sqlite3_column_type(stmt, i);
        switch (type) {
            case SQLITE_INTEGER:
                aurora_json_set(obj, name, (double)sqlite3_column_int64(stmt, i));
                break;
            case SQLITE_FLOAT:
                aurora_json_set(obj, name, sqlite3_column_double(stmt, i));
                break;
            case SQLITE_TEXT: {
                const char* txt = (const char*)sqlite3_column_text(stmt, i);
                JsonValue* v = aurora_json_new_str(txt ? txt : "");
                aurora_json_set_obj(obj, name, v);
                break;
            }
            case SQLITE_NULL:
                aurora_json_set(obj, name, 0);
                break;
            case SQLITE_BLOB: {
                JsonValue* v = aurora_json_new_str("[BLOB]");
                aurora_json_set_obj(obj, name, v);
                break;
            }
            default:
                aurora_json_set(obj, name, 0);
                break;
        }
    }
    return obj;
}

void* aurora_db_sqlite_query_json(void* db, const char* sql) {
    if (!db || !sql) return aurora_json_new_array();
    sqlite3* sdb = (sqlite3*)db;
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(sdb, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[sqlite] query error: %s\n", sqlite3_errmsg(sdb));
        return aurora_json_new_array();
    }
    JsonValue* arr = aurora_json_new_array();
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        JsonValue* row = row_to_json_value(stmt);
        if (row) aurora_json_array_push(arr, row);
    }
    sqlite3_finalize(stmt);
    return arr;
}

int aurora_db_sqlite_execute(void* db, const char* sql) {
    if (!db || !sql) return -1;
    sqlite3* sdb = (sqlite3*)db;
    char* errmsg = nullptr;
    int rc = sqlite3_exec(sdb, sql, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[sqlite] execute error: %s\n", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
        return -1;
    }
    return sqlite3_changes(sdb);
}

/* ════════════════════════════════════════════════════════════
   Prepared Statements
   ════════════════════════════════════════════════════════════ */

void* aurora_db_sqlite_prepare(void* db, const char* sql) {
    if (!db || !sql) return nullptr;
    sqlite3* sdb = (sqlite3*)db;
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(sdb, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[sqlite] prepare error: %s\n", sqlite3_errmsg(sdb));
        return nullptr;
    }
    return stmt;
}

void aurora_db_sqlite_bind_int(void* stmt, int index, int value) {
    if (!stmt) return;
    sqlite3_bind_int((sqlite3_stmt*)stmt, index, value);
}

void aurora_db_sqlite_bind_double(void* stmt, int index, double value) {
    if (!stmt) return;
    sqlite3_bind_double((sqlite3_stmt*)stmt, index, value);
}

void aurora_db_sqlite_bind_text(void* stmt, int index, const char* value) {
    if (!stmt) return;
    sqlite3_bind_text((sqlite3_stmt*)stmt, index, value, -1, SQLITE_TRANSIENT);
}

void aurora_db_sqlite_bind_null(void* stmt, int index) {
    if (!stmt) return;
    sqlite3_bind_null((sqlite3_stmt*)stmt, index);
}

int aurora_db_sqlite_step(void* stmt) {
    if (!stmt) return 1;
    int rc = sqlite3_step((sqlite3_stmt*)stmt);
    if (rc == SQLITE_ROW) return 0;
    return 1;
}

int aurora_db_sqlite_column_count(void* stmt) {
    if (!stmt) return 0;
    return sqlite3_column_count((sqlite3_stmt*)stmt);
}

const char* aurora_db_sqlite_column_name(void* stmt, int col) {
    if (!stmt) return "";
    return sqlite3_column_name((sqlite3_stmt*)stmt, col);
}

int aurora_db_sqlite_column_type(void* stmt, int col) {
    if (!stmt) return 5;
    return sqlite3_column_type((sqlite3_stmt*)stmt, col);
}

const char* aurora_db_sqlite_column_text(void* stmt, int col) {
    if (!stmt) return "";
    const char* txt = (const char*)sqlite3_column_text((sqlite3_stmt*)stmt, col);
    return txt ? txt : "";
}

int aurora_db_sqlite_column_int(void* stmt, int col) {
    if (!stmt) return 0;
    return sqlite3_column_int((sqlite3_stmt*)stmt, col);
}

double aurora_db_sqlite_column_double(void* stmt, int col) {
    if (!stmt) return 0.0;
    return sqlite3_column_double((sqlite3_stmt*)stmt, col);
}

void aurora_db_sqlite_reset(void* stmt) {
    if (!stmt) return;
    sqlite3_reset((sqlite3_stmt*)stmt);
}

void aurora_db_sqlite_finalize(void* stmt) {
    if (!stmt) return;
    sqlite3_finalize((sqlite3_stmt*)stmt);
}

/* ════════════════════════════════════════════════════════════
   Transactions
   ════════════════════════════════════════════════════════════ */

int aurora_db_sqlite_begin(void* db) {
    if (!db) return -1;
    char* errmsg = nullptr;
    int rc = sqlite3_exec((sqlite3*)db, "BEGIN", nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        sqlite3_free(errmsg);
        return -1;
    }
    return 0;
}

int aurora_db_sqlite_commit(void* db) {
    if (!db) return -1;
    char* errmsg = nullptr;
    int rc = sqlite3_exec((sqlite3*)db, "COMMIT", nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        sqlite3_free(errmsg);
        return -1;
    }
    return 0;
}

int aurora_db_sqlite_rollback(void* db) {
    if (!db) return -1;
    char* errmsg = nullptr;
    int rc = sqlite3_exec((sqlite3*)db, "ROLLBACK", nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        sqlite3_free(errmsg);
        return -1;
    }
    return 0;
}

/* ════════════════════════════════════════════════════════════
   Utility
   ════════════════════════════════════════════════════════════ */

int64_t aurora_db_sqlite_last_insert_rowid(void* db) {
    if (!db) return 0;
    return sqlite3_last_insert_rowid((sqlite3*)db);
}

int aurora_db_sqlite_changes(void* db) {
    if (!db) return 0;
    return sqlite3_changes((sqlite3*)db);
}

int aurora_db_sqlite_errcode(void* db) {
    if (!db) return 0;
    return sqlite3_errcode((sqlite3*)db);
}

const char* aurora_db_sqlite_errmsg(void* db) {
    if (!db) return "";
    return sqlite3_errmsg((sqlite3*)db);
}

void* aurora_db_sqlite_prep_query_json(void* stmt) {
    if (!stmt) return aurora_json_new_array();
    sqlite3_stmt* s = (sqlite3_stmt*)stmt;
    JsonValue* arr = aurora_json_new_array();
    while (sqlite3_step(s) == SQLITE_ROW) {
        JsonValue* row = row_to_json_value(s);
        if (row) aurora_json_array_push(arr, row);
    }
    sqlite3_reset(s);
    return arr;
}
