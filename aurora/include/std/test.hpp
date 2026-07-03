#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Test Registration (3) ── */
int         aurora_test_describe(const char* suite, void (*fn)(void));
int         aurora_test_it(const char* name, void (*fn)(void));
int         aurora_test_run(void);

/* ── Assertions (7) ── */
void        aurora_test_assert(int condition, const char* msg);
void        aurora_test_assert_eq_int(int a, int b, const char* msg);
void        aurora_test_assert_eq_float(double a, double b, double epsilon, const char* msg);
void        aurora_test_assert_eq_str(const char* a, const char* b, const char* msg);
void        aurora_test_assert_true(int condition, const char* msg);
void        aurora_test_assert_false(int condition, const char* msg);
void        aurora_test_assert_null(const void* ptr, const char* msg);

/* ── Integration Tests (3) ── */
void        aurora_test_setup(void (*fn)(void));
void        aurora_test_teardown(void (*fn)(void));
int         aurora_test_integration(const char* name, void (*fn)(void));

/* ── Widget Tests (3) ── */
int         aurora_test_widget(const char* name, void (*fn)(void));
void*       aurora_test_find_widget(const char* id);
int         aurora_test_click(const char* id);

/* ── Benchmark Tests (4) ── */
int         aurora_test_bench(const char* name, void (*fn)(void), int iterations);
void        aurora_test_bench_start(void);
void        aurora_test_bench_end(void);
double      aurora_test_bench_result(void);

/* ── Snapshot Tests (3) ── */
int         aurora_test_snapshot(const char* name, const char* value);
int         aurora_test_snapshot_update(const char* name, const char* value);
int         aurora_test_snapshot_delete(const char* name);

/* ── Coverage (3) ── */
int         aurora_test_coverage_start(void);
int         aurora_test_coverage_stop(void);
const char* aurora_test_coverage_report(void);

/* ── Results (4) ── */
int         aurora_test_pass_count(void);
int         aurora_test_fail_count(void);
int         aurora_test_total_count(void);
const char* aurora_test_results(void);

#ifdef __cplusplus
}
#endif
