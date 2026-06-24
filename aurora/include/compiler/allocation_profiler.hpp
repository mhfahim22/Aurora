#pragma once
#include "compiler/ast.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>

/* Forward declaration */
class MemoryAnalyzer;

/* ════════════════════════════════════════════════════════════
   Aurora Phase 8 — Allocation Profiler & Diagnostics
   ════════════════════════════════════════════════════════════

   Provides detailed memory allocation profiling and diagnostics
   for Aurora programs.

   Features:
     - Track allocation decisions per variable
     - Calculate allocation percentages
     - Generate detailed reports
     - Performance metrics

   ════════════════════════════════════════════════════════════ */

/* ── Allocation statistics for a single variable ── */
struct AllocationStats {
    std::string var_name;
    std::string func_name;
    AllocStrategy strategy;
    int         size_estimate;
    int         line;
    bool        is_performance_mode;
};

/* ── Aggregated statistics ── */
struct AllocationAggregatedStats {
    int stack_count;
    int stack_size;
    int arena_count;
    int arena_size;
    int raii_count;
    int raii_size;
    int arc_count;
    int arc_size;
    int gc_count;
    int gc_size;
    int heap_count;
    int heap_size;
    int unknown_count;

    int total_count;
    int total_size;

    double stack_percent;
    double arena_percent;
    double raii_percent;
    double arc_percent;
    double gc_percent;
    double heap_percent;
};

/* ── Performance metrics ── */
struct PerformanceMetrics {
    double estimated_stack_usage;
    double estimated_arena_usage;
    double estimated_heap_usage;
    int    estimated_refcount_ops;
    int    estimated_destructor_calls;
    bool   gc_disabled;
    bool   all_stack_possible;
};

/* ════════════════════════════════════════════════════════════
   AllocationProfiler — main profiling class
   ════════════════════════════════════════════════════════════ */
class AllocationProfiler {
public:
    AllocationProfiler() = default;

    /* ── Main entry point ── */
    void analyse(const ASTNode* root, const MemoryAnalyzer& memory_analyzer);

    /* ── Query results ── */
    const AllocationAggregatedStats& get_stats() const { return stats_; }
    const PerformanceMetrics& get_metrics() const { return metrics_; }
    const std::vector<AllocationStats>& get_all_allocations() const {
        return allocations_;
    }

    /* ── Statistics ── */
    int get_stack_count() const { return stats_.stack_count; }
    int get_arena_count() const { return stats_.arena_count; }
    int get_raii_count() const { return stats_.raii_count; }
    int get_arc_count() const { return stats_.arc_count; }
    int get_gc_count() const { return stats_.gc_count; }

    /* ── Print reports ── */
    void print_allocation_report() const;
    void print_detailed_report() const;
    void print_performance_report() const;
    void print_percentage_report() const;

    /* ── Export reports ── */
    std::string export_json() const;
    std::string export_csv() const;

private:
    /* ── Allocation tracking ── */
    std::vector<AllocationStats> allocations_;
    AllocationAggregatedStats stats_;
    PerformanceMetrics metrics_;

    /* ── Helper methods ── */
    void calculate_stats();
    void calculate_metrics(const MemoryAnalyzer& memory_analyzer);
    std::string format_size(int bytes) const;
    std::string format_percent(double percent) const;
};
