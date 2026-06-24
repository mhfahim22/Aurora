#pragma once
#include "compiler/ast.hpp"
#include <string>
#include <vector>
#include <unordered_map>

/* ════════════════════════════════════════════════════════════
   Aurora Phase 5 — Allocation Strategy Engine
   ════════════════════════════════════════════════════════════

   Determines the optimal memory allocation strategy for each
   variable based on analysis results.

   Allocation Priority (Normal Mode):
     1. Stack    — small, local, no escape
     2. Arena    — same lifetime group, bulk allocation
     3. RAII     — single owner, predictable destruction
     4. ARC      — shared ownership
     5. GC       — complex cyclic graph

   Allocation Priority (@performance Mode):
     1. Stack    — small, local, no escape
     2. Arena    — same lifetime group, bulk allocation
     3. RAII     — single owner, predictable destruction
     4. ARC      — shared ownership
     5. GC       — DISABLED (error if needed)

   Rules:
     Stack:
       - Small object (size known and < threshold)
       - Local scope
       - No escape
       - Single ownership

     Arena:
       - Same lifetime group
       - Bulk allocation
       - No return escape

     RAII:
       - Single owner
       - Predictable destruction
       - No complex reference cycles

     ARC:
       - Shared ownership
       - Reference counting overhead acceptable

     GC:
       - Complex cyclic graph
       - Not allowed in @performance mode

   ════════════════════════════════════════════════════════════ */

/* ── Allocation decision for a single variable ── */
struct AllocationDecision {
    std::string var_name;
    AllocStrategy strategy      { AllocStrategy::Unknown };
    AllocStrategy fallback      { AllocStrategy::Unknown };  /* fallback if primary fails */

    /* Decision reasoning */
    std::string reason;
    int         confidence      { 0 };   /* 0-100 confidence score */

    /* Constraints */
    bool        size_known      { false };
    int         size_estimate   { 0 };
    bool        is_mutable      { true };
    bool        has_borrows     { false };
    bool        has_cycles      { false };

    /* Timing info */
    int         decision_line   { 0 };
};

/* ── Allocation rules configuration ── */
struct AllocationRules {
    /* Size thresholds */
    int stack_max_size          { 1024 };    /* 1KB max for stack */
    int arena_min_size          { 64 };      /* 64 bytes min for arena */
    int arena_max_size          { 65536 };   /* 64KB max for arena */

    /* Mode flags */
    bool gc_enabled             { true };
    bool arena_enabled          { true };
    bool arc_enabled            { true };

    /* @performance mode overrides */
    void apply_performance_mode() {
        gc_enabled = false;
        arena_enabled = true;
        arc_enabled = true;
    }

    /* Normal mode */
    void apply_normal_mode() {
        gc_enabled = true;
        arena_enabled = true;
        arc_enabled = true;
    }
};

/* ════════════════════════════════════════════════════════════
   AllocationStrategyEngine — main allocation decision class
   ════════════════════════════════════════════════════════════ */
class AllocationStrategyEngine {
public:
    AllocationStrategyEngine() = default;

    /* ── Main entry point ── */
    void analyse(const ASTNode* root);

    /* ── Configure rules ── */
    void set_rules(const AllocationRules& rules) { rules_ = rules; }
    void set_performance_mode(bool val);

    /* ── Query results ── */
    AllocStrategy get_strategy(const std::string& var_name) const;
    const AllocationDecision& get_decision(const std::string& var_name) const;
    bool is_gc_disabled() const { return !rules_.gc_enabled; }

    /* ── Get analysis reports ── */
    const std::unordered_map<std::string, AllocationDecision>& get_all_decisions() const {
        return decisions_;
    }

    /* ── Statistics ── */
    int count_stack() const;
    int count_arena() const;
    int count_raii() const;
    int count_arc() const;
    int count_gc() const;
    int count_unknown() const;

    /* ── Print report ── */
    void print_allocation_report() const;

private:
    /* ── Allocation decisions per variable ── */
    std::unordered_map<std::string, AllocationDecision> decisions_;

    /* ── Rules configuration ── */
    AllocationRules rules_;

    /* ── Current function context ── */
    std::string current_func_name_;
    bool        in_performance_mode_ { false };

    /* ── Decision methods ── */
    AllocationDecision decide_stack(const std::string& name,
                                   const MemoryMetadata& meta);
    AllocationDecision decide_arena(const std::string& name,
                                    const MemoryMetadata& meta);
    AllocationDecision decide_raii(const std::string& name,
                                   const MemoryMetadata& meta);
    AllocationDecision decide_arc(const std::string& name,
                                  const MemoryMetadata& meta);
    AllocationDecision decide_gc(const std::string& name,
                                 const MemoryMetadata& meta);

    /* ── Validation methods ── */
    bool validate_stack(const AllocationDecision& decision,
                        const MemoryMetadata& meta) const;
    bool validate_arena(const AllocationDecision& decision,
                        const MemoryMetadata& meta) const;
    bool validate_raii(const AllocationDecision& decision,
                       const MemoryMetadata& meta) const;
    bool validate_arc(const AllocationDecision& decision,
                      const MemoryMetadata& meta) const;
    bool validate_gc(const AllocationDecision& decision,
                     const MemoryMetadata& meta) const;

    /* ── Phase 3: Hybrid & Coordination ── */
    struct HybridStrategy {
        AllocStrategy primary;
        AllocStrategy secondary;
        std::string   coordination_note;
    };

    HybridStrategy get_hybrid_strategy(const std::string& var_name) const;
    bool should_use_gc_coordination(const MemoryMetadata& meta) const;
    bool should_use_arena_coordination(const MemoryMetadata& meta) const;

    /* ── Helper methods ── */
    int estimate_size(const MemoryMetadata& meta) const;
    bool has_cycle_risk(const MemoryMetadata& meta) const;
    int calculate_confidence(AllocStrategy strategy,
                             const MemoryMetadata& meta) const;
};
