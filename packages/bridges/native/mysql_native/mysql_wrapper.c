/* ════════════════════════════════════════════════════════════
   MySQL Native Bridge — C Helper Wrapper
   Provides field-level access to MYSQL_ROW (char**) results
   ════════════════════════════════════════════════════════════ */

#include <stdint.h>

/* ── Get field value from a MYSQL_ROW at given column index ── */
const char* mysql_native_get_field(const char** row, int col)
{
    if (!row) return 0;
    return row[col];
}

/* ── Get field length from lengths array at given column ── */
unsigned long mysql_native_get_length(const unsigned long* lengths, int col)
{
    if (!lengths) return 0;
    return lengths[col];
}

/* ── Check if a field at column is NULL (row pointer itself is null) ── */
int mysql_native_field_is_null(const char** row, int col)
{
    if (!row) return 1;
    return row[col] == 0;
}
