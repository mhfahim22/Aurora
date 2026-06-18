#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Connection Pool ── */
typedef struct AuroraDBPool AuroraDBPool;
typedef struct AuroraDB AuroraDB;

/* Create a connection pool. conn_str: "mock://dbname" or "postgresql://user:pass@host:port/db"
   min_size: connections kept alive minimum, max_size: hard limit. */
AuroraDBPool* aurora_db_pool_create(const char* conn_str, int min_size, int max_size);

/* Acquire a connection from the pool (blocks if all busy & at max_size). Returns NULL on error. */
AuroraDB* aurora_db_pool_acquire(AuroraDBPool* pool, int timeout_ms);

/* Return a connection to the pool. */
void aurora_db_pool_release(AuroraDBPool* pool, AuroraDB* db);

/* Execute a query via the pool (auto acquire → query → release). Returns result or NULL. */
void* aurora_db_pool_query(AuroraDBPool* pool, const char* sql);

/* Free a pool query result. */
void aurora_db_pool_query_free(void* result);

/* Close all connections and destroy the pool. */
void aurora_db_pool_destroy(AuroraDBPool* pool);

/* Get pool stats for monitoring. */
int aurora_db_pool_active_count(AuroraDBPool* pool);
int aurora_db_pool_idle_count(AuroraDBPool* pool);

#ifdef __cplusplus
}
#endif
