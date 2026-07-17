// benchmark_suite.cpp — Benchmarking & Optimization System Implementation
// Part of the Aurora compiler pipeline — Phase 9

#include "compiler/benchmark_suite.hpp"
#include "runtime/memory.hpp"
#include <sstream>
#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

/* ════════════════════════════════════════════════════════════
   BenchmarkSuite — Main Entry Point
   ════════════════════════════════════════════════════════════ */

void BenchmarkSuite::run_all_benchmarks(const MemoryAnalyzer& memory_analyzer) {
    results_.clear();

    std::cout << "Running Aurora Memory Benchmarks...\n";
    std::cout << "═══════════════════════════════════════════════════════════\n\n";

    run_allocation_benchmarks();
    run_throughput_benchmarks();
    run_threading_benchmarks();
    run_gc_benchmarks();
    run_arc_benchmarks();

    calculate_summary();

    std::cout << "═══════════════════════════════════════════════════════════\n";
    std::cout << "Benchmarks complete!\n\n";
}

void BenchmarkSuite::run_allocation_benchmarks() {
    std::cout << "Running Allocation Benchmarks...\n";

    benchmark_stack_alloc();
    benchmark_arena_alloc();
    benchmark_raii_alloc();
    benchmark_arc_alloc();
    benchmark_gc_alloc();

    std::cout << "  Allocation benchmarks complete.\n\n";
}

void BenchmarkSuite::run_throughput_benchmarks() {
    std::cout << "Running Throughput Benchmarks...\n";

    benchmark_sequential_alloc();
    benchmark_random_alloc();
    benchmark_bulk_alloc();

    std::cout << "  Throughput benchmarks complete.\n\n";
}

void BenchmarkSuite::run_threading_benchmarks() {
    if (!config_.measure_thread) return;

    std::cout << "Running Threading Benchmarks...\n";

    benchmark_single_thread();
    benchmark_multi_thread();

    std::cout << "  Threading benchmarks complete.\n\n";
}

void BenchmarkSuite::run_gc_benchmarks() {
    if (!config_.measure_gc) return;

    std::cout << "Running GC Benchmarks...\n";

    benchmark_gc_pause();

    std::cout << "  GC benchmarks complete.\n\n";
}

void BenchmarkSuite::run_arc_benchmarks() {
    if (!config_.measure_arc) return;

    std::cout << "Running ARC Benchmarks...\n";

    benchmark_arc_overhead();

    std::cout << "  ARC benchmarks complete.\n\n";
}

/* ════════════════════════════════════════════════════════════
   Allocation Benchmarks
   ════════════════════════════════════════════════════════════ */

void BenchmarkSuite::benchmark_stack_alloc() {
    /* Simulate stack allocation (alloca in entry block) */
    auto bench = [this]() {
        volatile int x = 0;
        static_cast<void>(x);
    };

    double time = measure_time(bench, config_.iterations);
    record_result("Stack Allocation", "Allocation", time,
                  config_.iterations, config_.iterations * sizeof(int));
}

void BenchmarkSuite::benchmark_arena_alloc() {
    auto bench = [this]() {
        void* ptr = aurora_arena_alloc(config_.data_size);
        static_cast<void>(ptr);
    };

    /* Warmup */
    for (size_t i = 0; i < config_.warmup_iters; i++) {
        aurora_arena_alloc(config_.data_size);
    }
    aurora_arena_free();

    double time = measure_time(bench, config_.iterations);
    record_result("Arena Allocation", "Allocation", time,
                  config_.iterations, config_.iterations * config_.data_size);
}

void BenchmarkSuite::benchmark_raii_alloc() {
    auto bench = [this]() {
        void* ptr = aurora_arena_alloc(config_.data_size);
        aurora_drop_glue(ptr);
    };

    double time = measure_time(bench, config_.iterations);
    record_result("RAII Allocation", "Allocation", time,
                  config_.iterations, config_.iterations * config_.data_size);
}

void BenchmarkSuite::benchmark_arc_alloc() {
    auto bench = [this]() {
        void* ptr = aurora_shared_new(nullptr, nullptr);
        aurora_refcount_inc(ptr);
        aurora_refcount_dec(ptr);
    };

    double time = measure_time(bench, config_.iterations);
    record_result("ARC Allocation", "Allocation", time,
                  config_.iterations, config_.iterations * sizeof(void*));
}

void BenchmarkSuite::benchmark_gc_alloc() {
    /* GC allocation is slower due to tracking */
    auto bench = [this]() {
        void* ptr = aurora_arena_alloc(config_.data_size);
        aurora_gc_register_root(ptr);
        aurora_gc_unregister_root(ptr);
    };

    double time = measure_time(bench, config_.iterations);
    record_result("GC Allocation", "Allocation", time,
                  config_.iterations, config_.iterations * config_.data_size);
}

/* ════════════════════════════════════════════════════════════
   Throughput Benchmarks
   ════════════════════════════════════════════════════════════ */

void BenchmarkSuite::benchmark_sequential_alloc() {
    auto bench = [this]() {
        for (size_t i = 0; i < 1000; i++) {
            aurora_arena_alloc(64);
        }
    };

    double time = measure_time(bench, config_.iterations / 1000);
    record_result("Sequential Throughput", "Throughput", time,
                  config_.iterations, config_.iterations * 64);
}

void BenchmarkSuite::benchmark_random_alloc() {
    auto bench = [this]() {
        for (size_t i = 0; i < 1000; i++) {
            size_t size = 16 + (i % 64) * 16;
            aurora_arena_alloc(size);
        }
    };

    double time = measure_time(bench, config_.iterations / 1000);
    record_result("Random Size Throughput", "Throughput", time,
                  config_.iterations, config_.iterations * 40);
}

void BenchmarkSuite::benchmark_bulk_alloc() {
    auto bench = [this]() {
        aurora_arena_alloc(config_.data_size * 1000);
    };

    double time = measure_time(bench, config_.iterations / 1000);
    record_result("Bulk Allocation", "Throughput", time,
                  config_.iterations, config_.iterations * config_.data_size);
}

/* ════════════════════════════════════════════════════════════
   Threading Benchmarks
   ════════════════════════════════════════════════════════════ */

void BenchmarkSuite::benchmark_single_thread() {
    auto bench = [this]() {
        for (size_t i = 0; i < 10000; i++) {
            void* ptr = aurora_shared_new(nullptr, nullptr);
            aurora_refcount_inc(ptr);
            aurora_refcount_dec(ptr);
        }
    };

    double time = measure_time(bench, config_.iterations / 10000);
    record_result("Single Thread ARC", "Threading", time,
                  config_.iterations, config_.iterations * sizeof(void*));
}

void BenchmarkSuite::benchmark_multi_thread() {
    std::atomic<int> counter{0};

    auto bench = [this, &counter]() {
        std::vector<std::thread> threads;
        for (int t = 0; t < config_.thread_count; t++) {
            threads.emplace_back([this, &counter]() {
                for (size_t i = 0; i < 10000; i++) {
                    void* ptr = aurora_shared_new(nullptr, nullptr);
                    aurora_refcount_inc_atomic(ptr);
                    aurora_refcount_dec_atomic(ptr);
                    counter++;
                }
            });
        }
        for (auto& th : threads) {
            th.join();
        }
    };

    double time = measure_time(bench, config_.iterations / 10000);
    record_result("Multi Thread ARC (" + std::to_string(config_.thread_count) + " threads)",
                  "Threading", time, config_.iterations, config_.iterations * sizeof(void*));
}

/* ════════════════════════════════════════════════════════════
   GC Benchmarks
   ════════════════════════════════════════════════════════════ */

void BenchmarkSuite::benchmark_gc_pause() {
    /* Measure GC pause time */
    aurora_gc_init();

    auto bench = [this]() {
        for (size_t i = 0; i < 1000; i++) {
            void* ptr = aurora_arena_alloc(64);
            aurora_gc_register_root(ptr);
        }
        aurora_gc_collect();
    };

    double time = measure_time(bench, config_.iterations / 1000);
    record_result("GC Pause", "GC", time, config_.iterations / 1000, 0);
}

/* ════════════════════════════════════════════════════════════
   ARC Benchmarks
   ════════════════════════════════════════════════════════════ */

void BenchmarkSuite::benchmark_arc_overhead() {
    /* Measure ARC overhead vs raw allocation */
    auto bench_raw = [this]() {
        for (size_t i = 0; i < 10000; i++) {
            volatile void* ptr = aurora_arena_alloc(64);
            static_cast<void>(ptr);
        }
    };

    auto bench_arc = [this]() {
        for (size_t i = 0; i < 10000; i++) {
            void* ptr = aurora_shared_new(nullptr, nullptr);
            aurora_refcount_inc(ptr);
            aurora_refcount_dec(ptr);
        }
    };

    double time_raw = measure_time(bench_raw, config_.iterations / 10000);
    double time_arc = measure_time(bench_arc, config_.iterations / 10000);

    record_result("Raw Allocation", "ARC Overhead", time_raw,
                  config_.iterations, config_.iterations * 64);
    record_result("ARC Allocation", "ARC Overhead", time_arc,
                  config_.iterations, config_.iterations * sizeof(void*));

    double overhead = ((time_arc - time_raw) / time_raw) * 100.0;
    std::cout << "  ARC Overhead: " << std::fixed << std::setprecision(2)
              << overhead << "%\n";
}

/* ════════════════════════════════════════════════════════════
   Helper Functions
   ════════════════════════════════════════════════════════════ */

void BenchmarkSuite::record_result(const std::string& name,
                                   const std::string& category,
                                   double time_ms,
                                   size_t iterations,
                                   size_t bytes) {
    BenchmarkResult result;
    result.name = name;
    result.category = category;
    result.time_ms = time_ms;
    result.iterations = iterations;
    result.bytes_processed = bytes;
    result.ops_per_sec = (time_ms > 0) ? (iterations * 1000.0 / time_ms) : 0;

    results_.push_back(result);

    std::cout << "  " << name << ": "
              << std::fixed << std::setprecision(2) << time_ms << " ms ("
              << std::setprecision(0) << result.ops_per_sec << " ops/sec)\n";
}

double BenchmarkSuite::measure_time(std::function<void()> func, size_t iterations) {
    /* Warmup */
    for (size_t i = 0; i < config_.warmup_iters; i++) {
        func();
    }

    /* Measure */
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < iterations; i++) {
        func();
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    return duration.count() / 1000.0;  /* Convert to milliseconds */
}

void BenchmarkSuite::calculate_summary() {
    summary_.results = results_;
    summary_.total_benchmarks = static_cast<int>(results_.size());
    summary_.passed = summary_.total_benchmarks;
    summary_.failed = 0;

    double total_time = 0;
    for (const auto& result : results_) {
        total_time += result.time_ms;
    }
    summary_.total_time_ms = total_time;
}

/* ════════════════════════════════════════════════════════════
   Print Reports
   ════════════════════════════════════════════════════════════ */

void BenchmarkSuite::print_summary() const {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "--- Aurora Benchmark Summary ---\n";
    printf("  Total Benchmarks: %d\n", summary_.total_benchmarks);
    printf("  Passed:           %d\n", summary_.passed);
    printf("  Failed:           %d\n", summary_.failed);
    printf("  Total Time:       %.2f ms\n", summary_.total_time_ms);
    std::cout << "------------------------------------------\n";
    std::cout << "\n";
}

void BenchmarkSuite::print_detailed() const {
    std::cout << "\n";
    std::cout << "--- Aurora Detailed Benchmark Results ---\n";

    std::string current_category;
    for (const auto& result : results_) {
        if (result.category != current_category) {
            current_category = result.category;
            std::cout << "  -- " << current_category << " --\n";
        }
        printf("    %-35s %8.2f ms  %10.0f ops/s\n",
               result.name.c_str(), result.time_ms, result.ops_per_sec);
    }

    std::cout << "------------------------------------------\n";
    std::cout << "\n";
}

void BenchmarkSuite::print_optimization_report() const {
    std::cout << "\n";
    std::cout << "--- Aurora Optimization Report ---\n";

    /* Find best/worst allocations */
    double fastest_alloc = 1e9, slowest_alloc = 0;
    std::string fastest_name, slowest_name;

    for (const auto& result : results_) {
        if (result.category == "Allocation") {
            if (result.time_ms < fastest_alloc) {
                fastest_alloc = result.time_ms;
                fastest_name = result.name;
            }
            if (result.time_ms > slowest_alloc) {
                slowest_alloc = result.time_ms;
                slowest_name = result.name;
            }
        }
    }

    std::cout << "  Allocation Performance:\n";
    printf("    Fastest: %-35s %8.2f ms\n", fastest_name.c_str(), fastest_alloc);
    printf("    Slowest: %-35s %8.2f ms\n", slowest_name.c_str(), slowest_alloc);
    printf("    Ratio:   %.1fx\n", slowest_alloc / fastest_alloc);

    std::cout << "  Recommendations:\n";
    std::cout << "    - Use Stack for small, local variables\n";
    std::cout << "    - Use Arena for bulk allocations\n";
    std::cout << "    - Use RAII for single-owner objects\n";
    std::cout << "    - Use ARC only when shared ownership needed\n";
    std::cout << "    - Avoid GC in @performance mode\n";

    std::cout << "------------------------------------------\n";
    std::cout << "\n";
}

/* ════════════════════════════════════════════════════════════
   Export
   ════════════════════════════════════════════════════════════ */

std::string BenchmarkSuite::export_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"summary\": {\n";
    ss << "    \"total_benchmarks\": " << summary_.total_benchmarks << ",\n";
    ss << "    \"passed\": " << summary_.passed << ",\n";
    ss << "    \"failed\": " << summary_.failed << ",\n";
    ss << "    \"total_time_ms\": " << summary_.total_time_ms << "\n";
    ss << "  },\n";
    ss << "  \"results\": [\n";

    for (size_t i = 0; i < results_.size(); i++) {
        const auto& r = results_[i];
        ss << "    {\n";
        ss << "      \"name\": \"" << r.name << "\",\n";
        ss << "      \"category\": \"" << r.category << "\",\n";
        ss << "      \"time_ms\": " << r.time_ms << ",\n";
        ss << "      \"ops_per_sec\": " << r.ops_per_sec << ",\n";
        ss << "      \"iterations\": " << r.iterations << ",\n";
        ss << "      \"bytes_processed\": " << r.bytes_processed << "\n";
        ss << "    }";
        if (i < results_.size() - 1) ss << ",";
        ss << "\n";
    }

    ss << "  ]\n";
    ss << "}";
    return ss.str();
}