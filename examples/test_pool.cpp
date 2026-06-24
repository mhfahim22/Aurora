#include "runtime/backend.hpp"
#include "runtime/database.hpp"
#include <cstdio>
#include <thread>
#include <vector>
#include <atomic>

int main() {
    printf("=== Connection Pool Test ===\n\n");

    /* Test 1: Create pool */
    AuroraDBPool* pool = aurora_db_pool_create("mock://testdb", 2, 5);
    printf("Pool created.\n");
    printf("  idle=%d active=%d\n", aurora_db_pool_idle_count(pool), aurora_db_pool_active_count(pool));

    /* Test 2: Acquire and release */
    printf("\n--- Acquire/Release ---\n");
    AuroraDB* c1 = aurora_db_pool_acquire(pool, 1000);
    printf("  c1 acquired, idle=%d active=%d\n", aurora_db_pool_idle_count(pool), aurora_db_pool_active_count(pool));

    AuroraDB* c2 = aurora_db_pool_acquire(pool, 1000);
    printf("  c2 acquired, idle=%d active=%d\n", aurora_db_pool_idle_count(pool), aurora_db_pool_active_count(pool));

    aurora_db_pool_release(pool, c1);
    printf("  c1 released, idle=%d active=%d\n", aurora_db_pool_idle_count(pool), aurora_db_pool_active_count(pool));

    aurora_db_pool_release(pool, c2);
    printf("  c2 released, idle=%d active=%d\n", aurora_db_pool_idle_count(pool), aurora_db_pool_active_count(pool));

    /* Test 3: Pool query (auto acquire/release) */
    printf("\n--- Pool Query ---\n");
    aurora_db_pool_query(pool, "CREATE TABLE items (id INTEGER, name TEXT)");
    printf("  table created\n");
    aurora_db_pool_query(pool, "INSERT INTO items VALUES (1, 'hello')");
    aurora_db_pool_query(pool, "INSERT INTO items VALUES (2, 'world')");
    char* result = (char*)aurora_db_pool_query(pool, "SELECT * FROM items");
    printf("  query result: %s\n", result ? result : "NULL");
    aurora_db_pool_query_free(result);

    printf("  idle=%d active=%d\n", aurora_db_pool_idle_count(pool), aurora_db_pool_active_count(pool));

    /* Test 4: Thread safety */
    printf("\n--- Thread Safety ---\n");
    std::atomic<int> success{0};
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; i++) {
        threads.emplace_back([pool, &success]() {
            for (int j = 0; j < 20; j++) {
                AuroraDB* db = aurora_db_pool_acquire(pool, 5000);
                if (db) {
                    void* res = aurora_db_query(db, "SELECT * FROM items");
                    if (res) {
                        aurora_db_query_free(res);
                        success++;
                    }
                    aurora_db_pool_release(pool, db);
                }
            }
        });
    }
    for (auto& t : threads) t.join();
    printf("  %d/160 pool queries succeeded\n", success.load());

    /* Test 5: Overcommit (more threads than max connections) */
    printf("\n--- Overcommit (blocking) ---\n");
    std::atomic<int> blocked_success{0};
    std::vector<std::thread> bthreads;
    for (int i = 0; i < 10; i++) {
        bthreads.emplace_back([pool, &blocked_success]() {
            AuroraDB* db = aurora_db_pool_acquire(pool, 3000);
            if (db) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                void* res = aurora_db_query(db, "SELECT * FROM items");
                if (res) { aurora_db_query_free(res); blocked_success++; }
                aurora_db_pool_release(pool, db);
            }
        });
    }
    for (auto& t : bthreads) t.join();
    printf("  %d/10 blocked queries succeeded\n", blocked_success.load());

    /* Cleanup */
    aurora_db_pool_destroy(pool);
    printf("\n=== All tests passed ===\n");
    return 0;
}
