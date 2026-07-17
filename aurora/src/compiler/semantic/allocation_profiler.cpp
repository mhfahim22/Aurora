// allocation_profiler.cpp — Allocation Profiler & Diagnostics Implementation
// Part of the Aurora compiler pipeline — Phase 8

#include "compiler/allocation_profiler.hpp"
#include "compiler/memory_analyzer.hpp"
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cmath>

/* ════════════════════════════════════════════════════════════
   AllocationProfiler — Main Entry Point
   ════════════════════════════════════════════════════════════ */

void AllocationProfiler::analyse(const ASTNode* root,
                                 const MemoryAnalyzer& memory_analyzer) {
    /* Clear previous results */
    allocations_.clear();

    /* Get analysis results from memory analyzer */
    const auto& all_results = memory_analyzer.get_all_results();

    /* Collect allocation info for each variable */
    for (const auto& [func_name, result] : all_results) {
        for (const auto& [var_name, meta] : result.variables) {
            AllocationStats stats;
            stats.var_name = var_name;
            stats.func_name = func_name;
            stats.strategy = meta.alloc_strategy;
            stats.size_estimate = meta.size_estimate > 0 ? meta.size_estimate : 64;
            stats.line = meta.decl_line;
            stats.is_performance_mode = result.is_performance_mode;

            allocations_.push_back(stats);
        }
    }

    /* Calculate aggregated statistics */
    calculate_stats();

    /* Calculate performance metrics */
    calculate_metrics(memory_analyzer);
}

/* ════════════════════════════════════════════════════════════
   Statistics Calculation
   ════════════════════════════════════════════════════════════ */

void AllocationProfiler::calculate_stats() {
    /* Reset stats */
    stats_ = {};

    /* Count allocations by strategy */
    for (const auto& alloc : allocations_) {
        /* Convert forced strategy to base strategy for counting */
        AllocStrategy effective = is_forced_strategy(alloc.strategy)
            ? forced_to_base(alloc.strategy) : alloc.strategy;

        switch (effective) {
            case AllocStrategy::Stack:
                stats_.stack_count++;
                stats_.stack_size += alloc.size_estimate;
                break;
            case AllocStrategy::Arena:
                stats_.arena_count++;
                stats_.arena_size += alloc.size_estimate;
                break;
            case AllocStrategy::RAII:
                stats_.raii_count++;
                stats_.raii_size += alloc.size_estimate;
                break;
            case AllocStrategy::ARC:
                stats_.arc_count++;
                stats_.arc_size += alloc.size_estimate;
                break;
            case AllocStrategy::GC:
                stats_.gc_count++;
                stats_.gc_size += alloc.size_estimate;
                break;
            case AllocStrategy::Heap:
                stats_.heap_count++;
                stats_.heap_size += alloc.size_estimate;
                break;
            default:
                stats_.unknown_count++;
                break;
        }
    }

    /* Calculate totals */
    stats_.total_count = stats_.stack_count + stats_.arena_count +
                         stats_.raii_count + stats_.arc_count +
                         stats_.gc_count + stats_.heap_count;

    stats_.total_size = stats_.stack_size + stats_.arena_size +
                        stats_.raii_size + stats_.arc_size +
                        stats_.gc_size + stats_.heap_size;

    /* Calculate percentages */
    if (stats_.total_count > 0) {
        stats_.stack_percent = static_cast<double>(stats_.stack_count) / stats_.total_count * 100.0;
        stats_.arena_percent = static_cast<double>(stats_.arena_count) / stats_.total_count * 100.0;
        stats_.raii_percent = static_cast<double>(stats_.raii_count) / stats_.total_count * 100.0;
        stats_.arc_percent = static_cast<double>(stats_.arc_count) / stats_.total_count * 100.0;
        stats_.gc_percent = static_cast<double>(stats_.gc_count) / stats_.total_count * 100.0;
        stats_.heap_percent = static_cast<double>(stats_.heap_count) / stats_.total_count * 100.0;
    }
}

void AllocationProfiler::calculate_metrics(const MemoryAnalyzer& memory_analyzer) {
    /* Reset metrics */
    metrics_ = {};

    /* Check if GC is disabled */
    metrics_.gc_disabled = (stats_.gc_count == 0);

    /* Calculate estimated usages */
    metrics_.estimated_stack_usage = stats_.stack_size;
    metrics_.estimated_arena_usage = stats_.arena_size;
    metrics_.estimated_heap_usage = stats_.gc_size + stats_.heap_size;

    /* Estimate refcount operations */
    metrics_.estimated_refcount_ops = stats_.arc_count * 2;  /* inc + dec */

    /* Estimate destructor calls */
    metrics_.estimated_destructor_calls = stats_.raii_count;

    /* Check if all variables could be stack allocated */
    metrics_.all_stack_possible = (stats_.arena_count == 0 &&
                                   stats_.arc_count == 0 &&
                                   stats_.gc_count == 0);
}

/* ════════════════════════════════════════════════════════════
   Print Reports
   ════════════════════════════════════════════════════════════ */

void AllocationProfiler::print_allocation_report() const {
    print_percentage_report();
}

void AllocationProfiler::print_percentage_report() const {
    std::cerr << "\n";
    std::cerr << "--- Aurora Allocation Distribution Report ---\n";

    if (stats_.total_count == 0) {
        std::cerr << "  No allocations to report.\n";
    } else {
        /* Stack bar */
        int stack_bar = static_cast<int>(stats_.stack_percent / 5);
        fprintf(stderr, "  Stack : %5.1f%% ", stats_.stack_percent);
        for (int i = 0; i < stack_bar; i++) std::cerr << "█";
        std::cerr << "\n";

        /* Arena bar */
        int arena_bar = static_cast<int>(stats_.arena_percent / 5);
        fprintf(stderr, "  Arena : %5.1f%% ", stats_.arena_percent);
        for (int i = 0; i < arena_bar; i++) std::cerr << "█";
        std::cerr << "\n";

        /* RAII bar */
        int raii_bar = static_cast<int>(stats_.raii_percent / 5);
        fprintf(stderr, "  RAII  : %5.1f%% ", stats_.raii_percent);
        for (int i = 0; i < raii_bar; i++) std::cerr << "█";
        std::cerr << "\n";

        /* ARC bar */
        int arc_bar = static_cast<int>(stats_.arc_percent / 5);
        fprintf(stderr, "  ARC   : %5.1f%% ", stats_.arc_percent);
        for (int i = 0; i < arc_bar; i++) std::cerr << "█";
        std::cerr << "\n";

        /* GC bar */
        int gc_bar = static_cast<int>(stats_.gc_percent / 5);
        fprintf(stderr, "  GC    : %5.1f%% ", stats_.gc_percent);
        for (int i = 0; i < gc_bar; i++) std::cerr << "█";
        std::cerr << "\n";

        /* Summary */
        fprintf(stderr, "  Total: %d allocations (%s)\n",
               stats_.total_count, format_size(stats_.total_size).c_str());
    }

    std::cerr << "------------------------------------------\n";
    std::cerr << "\n";
}

void AllocationProfiler::print_detailed_report() const {
    std::cerr << "\n";
    std::cerr << "--- Aurora Detailed Allocation Report ---\n";

    if (allocations_.empty()) {
        std::cerr << "  No allocations to report.\n";
    } else {
        fprintf(stderr, "  %-18s  %-9s  %-7s  %s\n",
               "Variable", "Strategy", "Size", "Function");
        std::cerr << "  " << std::string(52, '-') << "\n";

        for (const auto& alloc : allocations_) {
            std::string strategy_name = alloc_strategy_name(alloc.strategy);
            fprintf(stderr, "  %-18s  %-9s  %7s  %s\n",
                   alloc.var_name.c_str(),
                   strategy_name.c_str(),
                   format_size(alloc.size_estimate).c_str(),
                   alloc.func_name.substr(0, 15).c_str());
        }

        std::cerr << "  " << std::string(52, '-') << "\n";
        std::cerr << "  Summary:\n";
        fprintf(stderr, "    Stack: %3d (%5.1f%%)  Arena: %3d (%5.1f%%)\n",
               stats_.stack_count, stats_.stack_percent,
               stats_.arena_count, stats_.arena_percent);
        fprintf(stderr, "    RAII:  %3d (%5.1f%%)  ARC:   %3d (%5.1f%%)\n",
               stats_.raii_count, stats_.raii_percent,
               stats_.arc_count, stats_.arc_percent);
        fprintf(stderr, "    GC:    %3d (%5.1f%%)  Other: %3d\n",
               stats_.gc_count, stats_.gc_percent,
               stats_.unknown_count);
    }

    std::cerr << "------------------------------------------\n";
    std::cerr << "\n";
}

void AllocationProfiler::print_performance_report() const {
    std::cerr << "\n";
    std::cerr << "--- Aurora Performance Metrics Report ---\n";
    std::cerr << "  Estimated Memory Usage:\n";
    fprintf(stderr, "    Stack:  %s\n", format_size(metrics_.estimated_stack_usage).c_str());
    fprintf(stderr, "    Arena:  %s\n", format_size(metrics_.estimated_arena_usage).c_str());
    fprintf(stderr, "    Heap:   %s\n", format_size(metrics_.estimated_heap_usage).c_str());
    std::cerr << "  Estimated Overhead:\n";
    fprintf(stderr, "    Refcount Ops:   %d\n", metrics_.estimated_refcount_ops);
    fprintf(stderr, "    Destructors:    %d\n", metrics_.estimated_destructor_calls);
    std::cerr << "  Optimizations:\n";
    fprintf(stderr, "    GC Disabled:         %s\n", metrics_.gc_disabled ? "Yes" : "No");
    fprintf(stderr, "    All Stack Possible:  %s\n", metrics_.all_stack_possible ? "Yes" : "No");
    std::cerr << "------------------------------------------\n";
    std::cerr << "\n";
}

/* ════════════════════════════════════════════════════════════
   Export Reports
   ════════════════════════════════════════════════════════════ */

std::string AllocationProfiler::export_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"total_allocations\": " << stats_.total_count << ",\n";
    ss << "  \"total_size\": " << stats_.total_size << ",\n";
    ss << "  \"distribution\": {\n";
    ss << "    \"stack\": {\"count\": " << stats_.stack_count
       << ", \"percent\": " << stats_.stack_percent << "},\n";
    ss << "    \"arena\": {\"count\": " << stats_.arena_count
       << ", \"percent\": " << stats_.arena_percent << "},\n";
    ss << "    \"raii\": {\"count\": " << stats_.raii_count
       << ", \"percent\": " << stats_.raii_percent << "},\n";
    ss << "    \"arc\": {\"count\": " << stats_.arc_count
       << ", \"percent\": " << stats_.arc_percent << "},\n";
    ss << "    \"gc\": {\"count\": " << stats_.gc_count
       << ", \"percent\": " << stats_.gc_percent << "}\n";
    ss << "  },\n";
    ss << "  \"performance\": {\n";
    ss << "    \"gc_disabled\": " << (metrics_.gc_disabled ? "true" : "false") << ",\n";
    ss << "    \"all_stack_possible\": " << (metrics_.all_stack_possible ? "true" : "false") << "\n";
    ss << "  }\n";
    ss << "}";
    return ss.str();
}

std::string AllocationProfiler::export_csv() const {
    std::ostringstream ss;
    ss << "Variable,Function,Strategy,Size,Line\n";
    for (const auto& alloc : allocations_) {
        ss << alloc.var_name << ","
           << alloc.func_name << ","
           << alloc_strategy_name(alloc.strategy) << ","
           << alloc.size_estimate << ","
           << alloc.line << "\n";
    }
    return ss.str();
}

/* ════════════════════════════════════════════════════════════
   Helper Methods
   ════════════════════════════════════════════════════════════ */

std::string AllocationProfiler::format_size(int bytes) const {
    if (bytes < 1024) {
        return std::to_string(bytes) + " B";
    } else if (bytes < 1024 * 1024) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << (bytes / 1024.0) << " KB";
        return ss.str();
    } else {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << (bytes / (1024.0 * 1024.0)) << " MB";
        return ss.str();
    }
}

std::string AllocationProfiler::format_percent(double percent) const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << percent << "%";
    return ss.str();
}