#pragma once
#include "compiler/ast.hpp"
#include "compiler/escape_analyzer.hpp"
#include "compiler/lifetime_analyzer.hpp"
#include "compiler/ownership_analyzer.hpp"
#include "compiler/allocation_strategy.hpp"
#include "compiler/allocation_profiler.hpp"
#include <string>
#include <vector>
#include <unordered_map>

/* ════════════════════════════════════════════════════════════
   Aurora Phase 1-8 — Memory Analysis System
   ════════════════════════════════════════════════════════════

   This module analyzes the AST to determine optimal memory
   allocation strategies for each variable.

   Analysis Pipeline:
     1. Escape Analysis     — does the object escape its scope?
     2. Lifetime Analysis   — how long does the object live?
     3. Ownership Analysis  — how many owners does it have?
     4. Allocation Decision — Stack/Arena/RAII/ARC/GC?
     5. Allocation Profiling — statistics and diagnostics

   ════════════════════════════════════════════════════════════ */

/* ── Variable info for tracking ── */
struct VariableInfo {
    std::string name;
    int         decl_line    { 0 };
    int         size_estimate { -1 };  /* -1 = unknown */
    bool        is_mutable   { true };
    bool        is_param     { false };
    bool        is_local     { false };
    bool        is_global    { false };

    /* Analysis results */
    EscapeStatus   escape_status  { EscapeStatus::Unknown };
    LifetimeScope  lifetime_scope { LifetimeScope::Unknown };
    OwnershipType  ownership_type { OwnershipType::Single };
    AllocStrategy  alloc_strategy { AllocStrategy::Unknown };
    AllocStrategy  forced_strategy { AllocStrategy::Unknown };
};

/* ── Function analysis context ── */
struct FunctionContext {
    std::string func_name;
    bool        is_performance_mode { false };
    int         func_line           { 0 };

    /* Variables in this function */
    std::unordered_map<std::string, VariableInfo> variables;

    /* Nested scopes */
    struct Scope {
        int scope_id;
        int parent_id;
        std::vector<std::string> var_names;
    };
    std::vector<Scope> scopes;

    /* Analysis results */
    MemoryAnalysisResult result;
};

/* ════════════════════════════════════════════════════════════
   MemoryAnalyzer — main analysis entry point
   ════════════════════════════════════════════════════════════ */
class MemoryAnalyzer {
public:
    /* Analyze the entire AST */
    void analyse(const ASTNode* root);

    /* Get analysis result for a specific function */
    const MemoryAnalysisResult& get_result(const std::string& func_name) const;

    /* Get all analysis results */
    const std::unordered_map<std::string, MemoryAnalysisResult>& get_all_results() const {
        return results_;
    }

    /* Check if a function has @performance attribute */
    bool is_performance_mode(const std::string& func_name) const;

    /* Get escape analyzer */
    const EscapeAnalyzer& get_escape_analyzer() const {
        return escape_analyzer_;
    }

    /* Get lifetime analyzer */
    const LifetimeAnalyzer& get_lifetime_analyzer() const {
        return lifetime_analyzer_;
    }

    /* Get ownership analyzer */
    const OwnershipAnalyzer& get_ownership_analyzer() const {
        return ownership_analyzer_;
    }

    /* Get allocation strategy engine */
    const AllocationStrategyEngine& get_allocation_engine() const {
        return allocation_engine_;
    }

    /* Get allocation profiler */
    const AllocationProfiler& get_allocation_profiler() const {
        return allocation_profiler_;
    }

    /* Get escape analysis report */
    void print_escape_report() const;

    /* Get lifetime analysis report */
    void print_lifetime_report() const;

    /* Get ownership analysis report */
    void print_ownership_report() const;
    void print_alias_graph() const;

    /* Get allocation strategy report */
    void print_allocation_report() const;

    /* Get allocation profiler reports */
    void print_profiler_report() const;
    void print_detailed_report() const;
    void print_performance_report() const;
    void print_percentage_report() const;

    /* Check if any compilation errors were encountered */
    bool has_errors() const { return has_errors_; }

    /* Propagate analysis results back to AST nodes for codegen */
    void apply_to_ast(ASTNode* root);

    /* Export reports */
    std::string export_profiler_json() const;
    std::string export_profiler_csv() const;

private:
    /* Analysis results per function */
    std::unordered_map<std::string, MemoryAnalysisResult> results_;

    /* Current function context */
    FunctionContext current_func_;

    /* Escape analyzer (Phase 2) */
    EscapeAnalyzer escape_analyzer_;

    /* Lifetime analyzer (Phase 3) */
    LifetimeAnalyzer lifetime_analyzer_;

    /* Ownership analyzer (Phase 4) */
    OwnershipAnalyzer ownership_analyzer_;

    /* Allocation strategy engine (Phase 5) */
    AllocationStrategyEngine allocation_engine_;

    /* Allocation profiler (Phase 8) */
    AllocationProfiler allocation_profiler_;

    /* Compilation error flag */
    bool has_errors_ = false;

    /* ── AST Walker ── */
    void walk(const ASTNode* node);
    void walk_block(const ASTNode* node);
    void walk_and_register_functions(const ASTNode* node);

    /* ── Analysis passes ── */
    void analyse_function(const ASTNode* node);
    void analyse_variable_decl(const std::string& name, int line);
    void analyse_assignment(const ASTNode* node);
    void analyse_return(const ASTNode* node);
    void analyse_call(const ASTNode* node);

    /* ── Cross-analyzer integration ── */
    void integrate_escape_with_lifetime();
    void integrate_lifetime_with_ownership();
    void integrate_ownership_with_escape();

    /* ── Escape Analysis (Phase 2) ── */
    void apply_escape_results();

    /* ── Lifetime Analysis (Phase 3) ── */
    void apply_lifetime_results();

    /* ── Ownership Analysis (Phase 4) ── */
    void apply_ownership_results();

    /* ── Allocation Strategy (Phase 5) ── */
    void apply_allocation_strategy(const ASTNode* root);
    bool can_use_stack_direct(const MemoryMetadata& meta) const;
    bool can_use_arena_direct(const MemoryMetadata& meta) const;

    /* ── Allocation Decision ── */
    void analyse_ownership(const std::string& name, const ASTNode* node);

    /* ── Allocation Decision ── */
    void decide_allocation(const std::string& name, VariableInfo& var);
    bool can_use_stack(const VariableInfo& var) const;
    bool can_use_arena(const VariableInfo& var) const;
    /* TODO: separate analysis-phase structs from decision-phase structs
       to improve clarity and reduce accidental coupling. */
};
