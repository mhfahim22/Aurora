// allocation_strategy.cpp — Allocation Strategy Engine Implementation
// Part of the Aurora compiler pipeline — Phase 5

#include "compiler/allocation_strategy.hpp"
#include "compiler/memory_analyzer.hpp"
#include <sstream>
#include <iostream>
#include <algorithm>

/* ════════════════════════════════════════════════════════════
   AllocationStrategyEngine — Main Entry Point
   ════════════════════════════════════════════════════════════ */

void AllocationStrategyEngine::analyse(const ASTNode* root) {
    decisions_.clear();
    if (!root) return;

    auto walk = [&](const ASTNode* node, auto&& self) -> void {
        if (!node) return;

        if (node->type == NodeType::Function ||
            node->type == NodeType::PerformanceFn) {

            current_func_name_ = node->value;
            in_performance_mode_ = (node->type == NodeType::PerformanceFn);

            auto walk_vars = [&](const ASTNode* n, auto&& wv) -> void {
                if (!n) return;
                if (n->type == NodeType::Var) {
                    std::string var_name = n->value;
                    if (!var_name.empty()) {
                        MemoryMetadata meta = n->memory_meta;
                        if (meta.size_estimate <= 0) meta.size_estimate = 64;
                        if (meta.ownership_type == OwnershipType::Unknown)
                            meta.ownership_type = OwnershipType::Single;
                        if (meta.escape_status == EscapeStatus::Unknown)
                            meta.escape_status = EscapeStatus::NoEscape;

                        if (is_forced_strategy(meta.forced_strategy)) {
                            AllocationDecision d;
                            d.var_name = var_name;
                            d.strategy = meta.forced_strategy;
                            d.confidence = 100;
                            d.reason = "forced by attribute";
                            decisions_[current_func_name_ + "::" + var_name] = d;
                        } else {
                            AllocationDecision decision;
                            if (meta.size_estimate > 0 && meta.size_estimate <= 64 && !meta.has_borrows)
                                decision = decide_stack(var_name, meta);
                            else if (meta.ownership_type == OwnershipType::Single)
                                decision = decide_raii(var_name, meta);
                            else if (meta.ownership_type == OwnershipType::Shared)
                                decision = decide_arc(var_name, meta);
                            else
                                decision = decide_gc(var_name, meta);

                            if (decision.strategy == AllocStrategy::Unknown && decision.confidence == 0) {
                                decision = decide_raii(var_name, meta);
                            }
                            if (decision.strategy == AllocStrategy::Unknown && decision.confidence == 0) {
                                decision.strategy = AllocStrategy::Stack;
                                decision.confidence = 30;
                                decision.reason = "default fallback";
                            }

                            decisions_[current_func_name_ + "::" + var_name] = decision;
                        }
                    }
                }
                wv(n->left.get(), wv);
                wv(n->right.get(), wv);
                wv(n->body.get(), wv);
                wv(n->orelse.get(), wv);
                wv(n->next.get(), wv);
                wv(n->args.get(), wv);
            };
            walk_vars(node->left.get(), walk_vars);
            walk_vars(node->right.get(), walk_vars);
            walk_vars(node->body.get(), walk_vars);
            walk_vars(node->orelse.get(), walk_vars);
            walk_vars(node->args.get(), walk_vars);
        }

        self(node->left.get(), self);
        self(node->right.get(), self);
        self(node->body.get(), self);
        self(node->orelse.get(), self);
        self(node->next.get(), self);
        self(node->args.get(), self);
    };
    walk(root, walk);
}

void AllocationStrategyEngine::set_performance_mode(bool val) {
    in_performance_mode_ = val;
    if (val) {
        rules_.apply_performance_mode();
    } else {
        rules_.apply_normal_mode();
    }
}

/* ════════════════════════════════════════════════════════════
   Decision Methods
   ════════════════════════════════════════════════════════════ */

AllocationDecision AllocationStrategyEngine::decide_stack(const std::string& name,
                                                          const MemoryMetadata& meta) {
    AllocationDecision decision;
    decision.var_name = name;
    decision.strategy = AllocStrategy::Stack;
    decision.fallback = AllocStrategy::Arena;
    decision.size_known = (meta.size_estimate > 0);
    decision.size_estimate = meta.size_estimate;
    decision.is_mutable = meta.is_mutable;
    decision.has_borrows = meta.has_borrows;

    /* Check if stack is allowed */
    if (!validate_stack(decision, meta)) {
        decision.strategy = AllocStrategy::Unknown;
        decision.confidence = 0;
        decision.reason = "Stack not allowed";
        return decision;
    }

    /* Calculate confidence */
    decision.confidence = calculate_confidence(AllocStrategy::Stack, meta);
    decision.reason = "Small, local, no escape";

    return decision;
}

AllocationDecision AllocationStrategyEngine::decide_arena(const std::string& name,
                                                          const MemoryMetadata& meta) {
    AllocationDecision decision;
    decision.var_name = name;
    decision.strategy = AllocStrategy::Arena;
    decision.fallback = AllocStrategy::RAII;
    decision.size_known = (meta.size_estimate > 0);
    decision.size_estimate = meta.size_estimate;
    decision.is_mutable = meta.is_mutable;

    /* Check if arena is allowed */
    if (!validate_arena(decision, meta)) {
        decision.strategy = AllocStrategy::Unknown;
        decision.confidence = 0;
        decision.reason = "Arena not allowed";
        return decision;
    }

    /* Calculate confidence */
    decision.confidence = calculate_confidence(AllocStrategy::Arena, meta);
    decision.reason = "Same lifetime group, bulk allocation";

    return decision;
}

AllocationDecision AllocationStrategyEngine::decide_raii(const std::string& name,
                                                         const MemoryMetadata& meta) {
    AllocationDecision decision;
    decision.var_name = name;
    decision.strategy = AllocStrategy::RAII;
    decision.fallback = AllocStrategy::ARC;
    decision.size_known = (meta.size_estimate > 0);
    decision.size_estimate = meta.size_estimate;
    decision.is_mutable = meta.is_mutable;

    /* Check if RAII is allowed */
    if (!validate_raii(decision, meta)) {
        decision.strategy = AllocStrategy::Unknown;
        decision.confidence = 0;
        decision.reason = "RAII not allowed";
        return decision;
    }

    /* Calculate confidence */
    decision.confidence = calculate_confidence(AllocStrategy::RAII, meta);
    decision.reason = "Single owner, predictable destruction";

    return decision;
}

AllocationDecision AllocationStrategyEngine::decide_arc(const std::string& name,
                                                        const MemoryMetadata& meta) {
    AllocationDecision decision;
    decision.var_name = name;
    decision.strategy = AllocStrategy::ARC;
    decision.fallback = AllocStrategy::GC;
    decision.size_known = (meta.size_estimate > 0);
    decision.size_estimate = meta.size_estimate;
    decision.is_mutable = meta.is_mutable;

    /* Check if ARC is allowed */
    if (!validate_arc(decision, meta)) {
        decision.strategy = AllocStrategy::Unknown;
        decision.confidence = 0;
        decision.reason = "ARC not allowed";
        return decision;
    }

    /* Calculate confidence */
    decision.confidence = calculate_confidence(AllocStrategy::ARC, meta);
    decision.reason = "Shared ownership";

    return decision;
}

AllocationDecision AllocationStrategyEngine::decide_gc(const std::string& name,
                                                       const MemoryMetadata& meta) {
    AllocationDecision decision;
    decision.var_name = name;
    decision.strategy = AllocStrategy::GC;
    decision.fallback = AllocStrategy::Unknown;
    decision.size_known = (meta.size_estimate > 0);
    decision.size_estimate = meta.size_estimate;
    decision.is_mutable = meta.is_mutable;
    decision.has_cycles = has_cycle_risk(meta);

    /* Check if GC is allowed */
    if (!validate_gc(decision, meta)) {
        decision.strategy = AllocStrategy::Unknown;
        decision.confidence = 0;
        decision.reason = "GC not allowed";
        return decision;
    }

    /* Calculate confidence */
    decision.confidence = calculate_confidence(AllocStrategy::GC, meta);
    decision.reason = "Complex cyclic graph";

    return decision;
}

/* ════════════════════════════════════════════════════════════
   Validation Methods
   ════════════════════════════════════════════════════════════ */

bool AllocationStrategyEngine::validate_stack(const AllocationDecision& decision,
                                              const MemoryMetadata& meta) const {
    if (decision.size_known && decision.size_estimate > rules_.stack_max_size) {
        return false;
    }
    return true;
}

bool AllocationStrategyEngine::validate_arena(const AllocationDecision& decision,
                                              const MemoryMetadata& meta) const {
    /* Arena is allowed if:
       - Arena is enabled
       - No return escape
       - Not global scope
       - Size is within arena limits
    */
    if (!rules_.arena_enabled) {
        return false;
    }
    if (meta.escape_status == EscapeStatus::ReturnEscape) {
        return false;
    }
    if (meta.lifetime_scope == LifetimeScope::Global) {
        return false;
    }
    if (decision.size_known) {
        if (decision.size_estimate < rules_.arena_min_size ||
            decision.size_estimate > rules_.arena_max_size) {
            return false;
        }
    }
    return true;
}

bool AllocationStrategyEngine::validate_raii(const AllocationDecision& decision,
                                             const MemoryMetadata& meta) const {
    /* RAII is allowed if:
       - Single ownership (or unknown)
       - No complex reference cycles
       - Predictable destruction
    */
    if (meta.ownership_type == OwnershipType::Shared ||
        meta.ownership_type == OwnershipType::Weak) {
        return false;
    }
    if (decision.has_cycles) {
        return false;
    }
    return true;
}

bool AllocationStrategyEngine::validate_arc(const AllocationDecision& decision,
                                            const MemoryMetadata& meta) const {
    /* ARC is allowed if:
       - ARC is enabled
       - Shared ownership
    */
    if (!rules_.arc_enabled) {
        return false;
    }
    if (meta.ownership_type != OwnershipType::Shared &&
        meta.ownership_type != OwnershipType::Weak) {
        return false;
    }
    return true;
}

bool AllocationStrategyEngine::validate_gc(const AllocationDecision& decision,
                                           const MemoryMetadata& meta) const {
    if (!rules_.gc_enabled) {
        return false;
    }
    return true;
}

/* ════════════════════════════════════════════════════════════
   Phase 3: Hybrid Strategy & Coordination
   ════════════════════════════════════════════════════════════ */

bool AllocationStrategyEngine::should_use_gc_coordination(const MemoryMetadata& meta) const {
    if (meta.ownership_type == OwnershipType::Shared && meta.ref_count > 1)
        return true;
    if (meta.ownership_type == OwnershipType::Weak)
        return true;
    return false;
}

bool AllocationStrategyEngine::should_use_arena_coordination(const MemoryMetadata& meta) const {
    if (meta.lifetime_scope == LifetimeScope::Function && meta.size_estimate > 256)
        return true;
    if (meta.is_gc_root && meta.escape_status == EscapeStatus::NoEscape)
        return true;
    return false;
}

AllocationStrategyEngine::HybridStrategy
AllocationStrategyEngine::get_hybrid_strategy(const std::string& var_name) const {
    HybridStrategy result;
    auto it = decisions_.find(var_name);
    if (it == decisions_.end()) {
        result.primary = AllocStrategy::Unknown;
        result.secondary = AllocStrategy::Stack;
        result.coordination_note = "No decision made";
        return result;
    }

    const auto& decision = it->second;
    result.primary = decision.strategy;
    result.secondary = decision.fallback;

    switch (decision.strategy) {
        case AllocStrategy::Stack:
            result.coordination_note = "Stack allocation (fastest)";
            break;
        case AllocStrategy::Arena:
            if (decision.has_cycles || decision.confidence < 50) {
                result.coordination_note = "Arena + GC coordination recommended";
                result.secondary = AllocStrategy::GC;
            } else {
                result.coordination_note = "Pure arena allocation";
            }
            break;
        case AllocStrategy::RAII:
            result.coordination_note = "RAII with deterministic cleanup";
            break;
        case AllocStrategy::ARC:
            if (decision.has_cycles) {
                result.coordination_note = "ARC + GC coordination for cycle safety";
                result.secondary = AllocStrategy::GC;
            } else {
                result.coordination_note = "ARC reference counting";
            }
            break;
        case AllocStrategy::GC:
            result.coordination_note = "GC managed (with arena root scanning)";
            break;
        default:
            result.coordination_note = "Unknown strategy";
            break;
    }

    return result;
}

/* ════════════════════════════════════════════════════════════
   Helper Methods
   ════════════════════════════════════════════════════════════ */

int AllocationStrategyEngine::estimate_size(const MemoryMetadata& meta) const {
    if (meta.size_estimate > 0) {
        return meta.size_estimate;
    }
    /* Default estimates based on type hints */
    return 64;  /* conservative default */
}

bool AllocationStrategyEngine::has_cycle_risk(const MemoryMetadata& meta) const {
    /* Cycle risk exists if:
       - Shared ownership with multiple references
       - Complex reference graph
    */
    return (meta.ownership_type == OwnershipType::Shared &&
            meta.ref_count > 2);
}

int AllocationStrategyEngine::calculate_confidence(AllocStrategy strategy,
                                                    const MemoryMetadata& meta) const {
    int confidence = 50;  /* base confidence */

    switch (strategy) {
        case AllocStrategy::Stack:
            /* Higher confidence for stack if:
               - Size is known and small
               - No escape
               - Single ownership
            */
            if (meta.size_estimate > 0 && meta.size_estimate < 256) {
                confidence += 20;
            }
            if (meta.escape_status == EscapeStatus::NoEscape) {
                confidence += 15;
            }
            if (meta.ownership_type == OwnershipType::Single) {
                confidence += 15;
            }
            break;

        case AllocStrategy::Arena:
            /* Higher confidence for arena if:
               - Same lifetime group
               - Bulk allocation likely
            */
            if (meta.lifetime_scope == LifetimeScope::Function ||
                meta.lifetime_scope == LifetimeScope::Block) {
                confidence += 20;
            }
            if (meta.size_estimate > 64) {
                confidence += 10;
            }
            break;

        case AllocStrategy::RAII:
            /* Higher confidence for RAII if:
               - Single ownership
               - Predictable destruction
            */
            if (meta.ownership_type == OwnershipType::Single) {
                confidence += 25;
            }
            if (meta.escape_status == EscapeStatus::NoEscape) {
                confidence += 10;
            }
            break;

        case AllocStrategy::ARC:
            /* Higher confidence for ARC if:
               - Shared ownership
               - Multiple references
            */
            if (meta.ownership_type == OwnershipType::Shared) {
                confidence += 25;
            }
            if (meta.ref_count > 1) {
                confidence += 10;
            }
            break;

        case AllocStrategy::GC:
            /* Higher confidence for GC if:
               - Complex cycles
               - Shared with cycles
            */
            if (meta.ref_count > 2) {
                confidence += 20;
            }
            break;

        default:
            break;
    }

    /* Cap at 100 */
    return std::min(confidence, 100);
}

/* ════════════════════════════════════════════════════════════
   Query Methods
   ════════════════════════════════════════════════════════════ */

AllocStrategy AllocationStrategyEngine::get_strategy(const std::string& var_name) const {
    auto it = decisions_.find(var_name);
    return (it != decisions_.end()) ? it->second.strategy : AllocStrategy::Unknown;
}

const AllocationDecision& AllocationStrategyEngine::get_decision(const std::string& var_name) const {
    static AllocationDecision empty;
    auto it = decisions_.find(var_name);
    return (it != decisions_.end()) ? it->second : empty;
}

int AllocationStrategyEngine::count_stack() const {
    int count = 0;
    for (const auto& [name, decision] : decisions_) {
        if (decision.strategy == AllocStrategy::Stack) count++;
    }
    return count;
}

int AllocationStrategyEngine::count_arena() const {
    int count = 0;
    for (const auto& [name, decision] : decisions_) {
        if (decision.strategy == AllocStrategy::Arena) count++;
    }
    return count;
}

int AllocationStrategyEngine::count_raii() const {
    int count = 0;
    for (const auto& [name, decision] : decisions_) {
        if (decision.strategy == AllocStrategy::RAII) count++;
    }
    return count;
}

int AllocationStrategyEngine::count_arc() const {
    int count = 0;
    for (const auto& [name, decision] : decisions_) {
        if (decision.strategy == AllocStrategy::ARC) count++;
    }
    return count;
}

int AllocationStrategyEngine::count_gc() const {
    int count = 0;
    for (const auto& [name, decision] : decisions_) {
        if (decision.strategy == AllocStrategy::GC) count++;
    }
    return count;
}

int AllocationStrategyEngine::count_unknown() const {
    int count = 0;
    for (const auto& [name, decision] : decisions_) {
        if (decision.strategy == AllocStrategy::Unknown) count++;
    }
    return count;
}

/* ════════════════════════════════════════════════════════════
   Print Report
   ════════════════════════════════════════════════════════════ */

void AllocationStrategyEngine::print_allocation_report() const {
    std::cerr << "\n";
    std::cerr << "╔══════════════════════════════════════════════════════════╗\n";
    std::cerr << "║      Aurora Allocation Strategy Report                   ║\n";
    std::cerr << "╠══════════════════════════════════════════════════════════╣\n";

    if (decisions_.empty()) {
        std::cerr << "║  No allocation decisions made.                          ║\n";
    } else {
        std::cerr << "║  Variable          │ Strategy     │ Confidence │ Reason ║\n";
        std::cerr << "╠══════════════════════════════════════════════════════════╣\n";

        for (const auto& [name, decision] : decisions_) {
            std::string strategy_name = alloc_strategy_name(decision.strategy);
            fprintf(stderr, "║  %-18s│ %-13s│ %3d%%      │ %-24s║\n",
                   name.c_str(), strategy_name.c_str(),
                   decision.confidence, decision.reason.substr(0, 24).c_str());
        }

        std::cerr << "╠══════════════════════════════════════════════════════════╣\n";
        std::cerr << "║  Statistics:                                             ║\n";
        std::cerr << "║    Stack:  " << count_stack() << "  Arena: " << count_arena()
                  << "  RAII: " << count_raii() << "                      ║\n";
        std::cerr << "║    ARC:    " << count_arc() << "  GC:    " << count_gc()
                  << "  Unknown: " << count_unknown() << "                  ║\n";
        std::cerr << "║                                                          ║\n";
        std::cerr << "║  Mode: " << (rules_.gc_enabled ? "Normal" : "@performance") << "                                        ║\n";
    }

    std::cerr << "╚══════════════════════════════════════════════════════════╝\n";
    std::cerr << "\n";
}
