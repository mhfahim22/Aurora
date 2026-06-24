#include "runtime/backend.hpp"
#include "runtime/database.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <algorithm>

/* ── Connection Pool ── */

struct PooledConnection {
    AuroraDB* db = nullptr;
    bool in_use = false;
    std::chrono::steady_clock::time_point last_used;
};

struct AuroraDBPool {
    std::string conn_str;
    int min_size;
    int max_size;
    std::vector<PooledConnection> connections;
    std::mutex mtx;
    std::condition_variable cv;
    size_t next_scan_pos = 0;
};

extern "C" {

AuroraDBPool* aurora_db_pool_create(const char* conn_str, int min_size, int max_size) {
    if (!conn_str || min_size < 0 || max_size < 1 || min_size > max_size)
        return nullptr;
    AuroraDBPool* pool = new AuroraDBPool();
    pool->conn_str = conn_str;
    pool->min_size = min_size;
    pool->max_size = max_size;
    /* Pre-create min_size connections */
    for (int i = 0; i < min_size; i++) {
        AuroraDB* db = aurora_db_connect(conn_str);
        if (db) {
            PooledConnection pc;
            pc.db = db;
            pc.in_use = false;
            pc.last_used = std::chrono::steady_clock::now();
            pool->connections.push_back(std::move(pc));
        }
    }
    printf("[db-pool] created pool: conn_str=%s min=%d max=%d created=%zu\n",
           pool->conn_str.c_str(), min_size, max_size, pool->connections.size());
    return pool;
}

static AuroraDB* pool_create_new_connection(AuroraDBPool* pool) {
    return aurora_db_connect(pool->conn_str.c_str());
}

static size_t find_idle_connection(AuroraDBPool* pool) {
    size_t n = pool->connections.size();
    if (n == 0) return (size_t)-1;
    for (size_t i = 0; i < n; i++) {
        size_t idx = (pool->next_scan_pos + i) % n;
        if (!pool->connections[idx].in_use) {
            pool->next_scan_pos = (idx + 1) % n;
            return idx;
        }
    }
    return (size_t)-1;
}

AuroraDB* aurora_db_pool_acquire(AuroraDBPool* pool, int timeout_ms) {
    if (!pool) return nullptr;
    std::unique_lock<std::mutex> lock(pool->mtx);

    size_t idx = find_idle_connection(pool);
    if (idx != (size_t)-1) {
        pool->connections[idx].in_use = true;
        pool->connections[idx].last_used = std::chrono::steady_clock::now();
        printf("[db-pool] acquired existing connection %zu\n", idx);
        return pool->connections[idx].db;
    }

    if ((int)pool->connections.size() < pool->max_size) {
        AuroraDB* db = pool_create_new_connection(pool);
        if (db) {
            PooledConnection pc;
            pc.db = db;
            pc.in_use = true;
            pc.last_used = std::chrono::steady_clock::now();
            pool->connections.push_back(std::move(pc));
            printf("[db-pool] created new connection %zu\n", pool->connections.size() - 1);
            return db;
        }
        return nullptr;
    }

    auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (true) {
        if (pool->cv.wait_until(lock, until) == std::cv_status::timeout) {
            printf("[db-pool] acquire timeout after %d ms\n", timeout_ms);
            return nullptr;
        }
        idx = find_idle_connection(pool);
        if (idx != (size_t)-1) {
            pool->connections[idx].in_use = true;
            pool->connections[idx].last_used = std::chrono::steady_clock::now();
            printf("[db-pool] acquired after wait: connection %zu\n", idx);
            return pool->connections[idx].db;
        }
    }
}

void aurora_db_pool_release(AuroraDBPool* pool, AuroraDB* db) {
    if (!pool || !db) return;
    std::lock_guard<std::mutex> lock(pool->mtx);
    for (size_t i = 0; i < pool->connections.size(); i++) {
        if (pool->connections[i].db == db) {
            pool->connections[i].in_use = false;
            pool->connections[i].last_used = std::chrono::steady_clock::now();
            printf("[db-pool] released connection %zu\n", i);
            pool->cv.notify_one();
            return;
        }
    }
    printf("[db-pool] warning: releasing unknown connection\n");
}

void* aurora_db_pool_query(AuroraDBPool* pool, const char* sql) {
    if (!pool || !sql) return nullptr;
    AuroraDB* db = aurora_db_pool_acquire(pool, 5000);
    if (!db) {
        fprintf(stderr, "[db-pool] failed to acquire connection for query\n");
        return nullptr;
    }
    void* result = aurora_db_query(db, sql);
    aurora_db_pool_release(pool, db);
    return result;
}

void aurora_db_pool_query_free(void* result) {
    aurora_db_query_free(result);
}

void aurora_db_pool_destroy(AuroraDBPool* pool) {
    if (!pool) return;
    std::vector<AuroraDB*> to_close;
    {
        std::lock_guard<std::mutex> lock(pool->mtx);
        for (auto& pc : pool->connections) {
            if (pc.db) to_close.push_back(pc.db);
        }
        pool->connections.clear();
    }
    for (auto* db : to_close) {
        aurora_db_close(db);
    }
    printf("[db-pool] pool destroyed (%zu connections closed)\n", to_close.size());
    delete pool;
}

int aurora_db_pool_active_count(AuroraDBPool* pool) {
    if (!pool) return 0;
    std::lock_guard<std::mutex> lock(pool->mtx);
    int count = 0;
    for (auto& pc : pool->connections) {
        if (pc.in_use) count++;
    }
    return count;
}

int aurora_db_pool_idle_count(AuroraDBPool* pool) {
    if (!pool) return 0;
    std::lock_guard<std::mutex> lock(pool->mtx);
    int total = (int)pool->connections.size();
    int active = 0;
    for (auto& pc : pool->connections) {
        if (pc.in_use) active++;
    }
    return total - active;
}

} /* extern "C" */
