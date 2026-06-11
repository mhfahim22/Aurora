#pragma once
#include "compiler/ast.hpp"
#include "compiler/memory_analyzer.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <numeric>

/* ════════════════════════════════════════════════════════════
   Aurora Phase 9 — Benchmarking & Optimization System
   ════════════════════════════════════════════════════════════

   Provides comprehensive benchmarking and performance analysis
   for Aurora memory management system.

   Benchmarks:
     - Allocation benchmarks (Stack/Arena/RAII/ARC/GC)
     - Memory throughput benchmarks
     - Threading benchmarks
     - GC pause measurement
     - ARC overhead measurement

   ════════════════════════════════════════════════════════════ */

/* ── Benchmark result ── */
struct BenchmarkResult {
    std::string name;
    std::string category;
    double      time_ms;
    double      ops_per_sec;
    size_t      iterations;
    size_t      bytes_processed;
    std::string notes;
};

/* ── Benchmark configuration ── */
struct BenchmarkConfig {
    size_t  iterations      { 100000 };
    size_t  warmup_iters    { 1000 };
    size_t  data_size       { 1024 };
    bool    measure_gc      { true };
    bool    measure_arc     { true };
    bool    measure_thread  { true };
    int     thread_count    { 4 };
};

/* ── Benchmark summary ── */
struct BenchmarkSummary {
    double  total_time_ms;
    int     total_benchmarks;
    int     passed;
    int     failed;
    std::vector<BenchmarkResult> results;
};

/* ════════════════════════════════════════════════════════════
   BenchmarkSuite — main benchmarking class
   ════════════════════════════════════════════════════════════ */
class BenchmarkSuite {
public:
    BenchmarkSuite() = default;

    /* ── Main entry point ── */
    void run_all_benchmarks(const MemoryAnalyzer& memory_analyzer);
    void run_allocation_benchmarks();
    void run_throughput_benchmarks();
    void run_threading_benchmarks();
    void run_gc_benchmarks();
    void run_arc_benchmarks();

    /* ── Query results ── */
    const BenchmarkSummary& get_summary() const { return summary_; }
    const std::vector<BenchmarkResult>& get_results() const { return results_; }

    /* ── Configuration ── */
    void set_config(const BenchmarkConfig& config) { config_ = config; }

    /* ── Print reports ── */
    void print_summary() const;
    void print_detailed() const;
    void print_optimization_report() const;

    /* ── Export ── */
    std::string export_json() const;

private:
    /* ── Results ── */
    std::vector<BenchmarkResult> results_;
    BenchmarkSummary summary_;
    BenchmarkConfig config_;

    /* ── Benchmark functions ── */
    void benchmark_stack_alloc();
    void benchmark_arena_alloc();
    void benchmark_raii_alloc();
    void benchmark_arc_alloc();
    void benchmark_gc_alloc();

    void benchmark_sequential_alloc();
    void benchmark_random_alloc();
    void benchmark_bulk_alloc();

    void benchmark_single_thread();
    void benchmark_multi_thread();

    void benchmark_gc_pause();
    void benchmark_arc_overhead();

    /* ── Helper functions ── */
    void record_result(const std::string& name, const std::string& category,
                       double time_ms, size_t iterations, size_t bytes);
    double measure_time(std::function<void()> func, size_t iterations);
    void calculate_summary();
};
