#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/* ── SQLite connection management ── */

/* Open a SQLite database file. Returns opaque handle or NULL. */
void* aurora_db_sqlite_open(const char* path);

/* Close a SQLite database. */
void aurora_db_sqlite_close(void* db);

/* ── Query execution ── */

/* Execute SQL and return result as pipe-delimited string (like mock engine). */
/* For SELECT: "col1|col2\nval1|val2\n"  For non-SELECT: empty string. */
/* Returns malloc'd string. Caller must free. */
char* aurora_db_sqlite_exec(void* db, const char* sql);

/* Execute SQL and return result as JsonValue* (array of objects). */
void* aurora_db_sqlite_query_json(void* db, const char* sql);

/* Execute INSERT/UPDATE/DELETE and return affected rows count. */
int aurora_db_sqlite_execute(void* db, const char* sql);

/* ── Prepared Statements ── */

/* Prepare a SQL statement. Returns statement handle or NULL. */
void* aurora_db_sqlite_prepare(void* db, const char* sql);

/* Bind parameters */
void aurora_db_sqlite_bind_int(void* stmt, int index, int value);
void aurora_db_sqlite_bind_double(void* stmt, int index, double value);
void aurora_db_sqlite_bind_text(void* stmt, int index, const char* value);
void aurora_db_sqlite_bind_null(void* stmt, int index);

/* Step through results. Returns 0 (SQLITE_ROW) if row available, 1 (SQLITE_DONE) if done. */
int aurora_db_sqlite_step(void* stmt);

/* Get column count from prepared statement result */
int aurora_db_sqlite_column_count(void* stmt);

/* Get column name */
const char* aurora_db_sqlite_column_name(void* stmt, int col);

/* Get column type: 1=INT, 2=FLOAT, 3=TEXT, 4=BLOB, 5=NULL */
int aurora_db_sqlite_column_type(void* stmt, int col);

/* Get column value as text (returns temporary pointer, do not free) */
const char* aurora_db_sqlite_column_text(void* stmt, int col);

/* Get column value as integer */
int aurora_db_sqlite_column_int(void* stmt, int col);

/* Get column value as double */
double aurora_db_sqlite_column_double(void* stmt, int col);

/* Reset statement for re-execution */
void aurora_db_sqlite_reset(void* stmt);

/* Finalize (destroy) a prepared statement */
void aurora_db_sqlite_finalize(void* stmt);

/* ── Transactions ── */

int aurora_db_sqlite_begin(void* db);
int aurora_db_sqlite_commit(void* db);
int aurora_db_sqlite_rollback(void* db);

/* ── Utility ── */

/* Returns last inserted row id */
int64_t aurora_db_sqlite_last_insert_rowid(void* db);

/* Returns number of rows changed by last statement */
int aurora_db_sqlite_changes(void* db);

/* Returns error code for last operation */
int aurora_db_sqlite_errcode(void* db);

/* Returns error message for last operation (temporary, do not free) */
const char* aurora_db_sqlite_errmsg(void* db);

/* Execute a prepared statement and return result as JsonValue* */
void* aurora_db_sqlite_prep_query_json(void* stmt);

#ifdef __cplusplus
}
#endif
