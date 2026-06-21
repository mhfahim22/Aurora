// memory_analyzer.cpp — Memory Analysis Implementation
// Part of the Aurora compiler pipeline — Phase 1-8

#include "compiler/memory_analyzer.hpp"
#include <sstream>
#include <iostream>
#include <functional>

/* ════════════════════════════════════════════════════════════
   MemoryAnalyzer — Main Entry Point
   ════════════════════════════════════════════════════════════ */

void MemoryAnalyzer::analyse(const ASTNode* root) {
    /* ── Phase 1: Register all functions to set up contexts ── */
    walk_and_register_functions(root);

    /* ── Phase 2: Run escape analysis ── */
    escape_analyzer_.analyse(root);

    /* ── Phase 3: Run lifetime analysis ── */
    lifetime_analyzer_.analyse(root);

    /* ── Phase 4: Run ownership analysis ── */
    ownership_analyzer_.analyse(root);

    /* ── Phase 5: Walk AST for integrated memory analysis ── */
    walk_block(root);

    /* ── Phase 6: Cross-analyzer data integration ── */
    integrate_escape_with_lifetime();
    integrate_lifetime_with_ownership();
    integrate_ownership_with_escape();

    /* ── Phase 7: Apply analysis results to variables ── */
    apply_escape_results();
    apply_lifetime_results();
    apply_ownership_results();

    /* ── Phase 8: Run allocation strategy engine ── */
    apply_allocation_strategy(root);

    /* ── Phase 9: Run allocation profiler ── */
    allocation_profiler_.analyse(root, *this);
}

/* ════════════════════════════════════════════════════════════
   Phase 1: Function Pre-registration
   ════════════════════════════════════════════════════════════ */

void MemoryAnalyzer::walk_and_register_functions(const ASTNode* node) {
    while (node) {
        if (node->type == NodeType::Function || node->type == NodeType::PerformanceFn) {
            FunctionContext ctx;
            ctx.func_name = node->value;
            ctx.func_line = node->src_line;
            ctx.is_performance_mode = (node->type == NodeType::PerformanceFn);

            MemoryAnalysisResult result;
            result.is_performance_mode = ctx.is_performance_mode;
            result.gc_disabled = ctx.is_performance_mode;
            ctx.result = result;

            const ASTNode* param = node->args.get();
            while (param) {
                VariableInfo var;
                var.name = param->value;
                var.decl_line = param->src_line;
                var.is_param = true;
                var.is_local = true;
                ctx.variables[var.name] = var;
                param = param->next.get();
            }

            /* Store result immediately so later phases can use it */
            /* Use emplace with check to avoid overwriting on re-visit */
            if (results_.find(ctx.func_name) == results_.end()) {
                results_[ctx.func_name] = std::move(result);
            }
        }
        node = node->next.get();
    }
}

/* ════════════════════════════════════════════════════════════
   Cross-Analyzer Integration
   ════════════════════════════════════════════════════════════ */

void MemoryAnalyzer::integrate_escape_with_lifetime() {
    /* Variables that escape through return get Function lifetime */
    const auto& escape_info = escape_analyzer_.get_all_escape_info();
    for (const auto& [name, info] : escape_info) {
        if (info.status == EscapeStatus::ReturnEscape) {
            for (auto& [func_name, result] : results_) {
                auto vit = result.variables.find(name);
                if (vit != result.variables.end()) {
                    vit->second.lifetime_scope = LifetimeScope::Function;
                }
            }
        }
    }
}

void MemoryAnalyzer::integrate_lifetime_with_ownership() {
    /* Variables with Global lifetime need Shared ownership */
    for (auto& [func_name, result] : results_) {
        for (auto& [var_name, meta] : result.variables) {
            if (meta.lifetime_scope == LifetimeScope::Global &&
                meta.ownership_type == OwnershipType::Single) {
                meta.ownership_type = OwnershipType::Shared;
            }
        }
    }
}

void MemoryAnalyzer::integrate_ownership_with_escape() {
    /* Escaped shared variables need ARC */
    for (auto& [func_name, result] : results_) {
        for (auto& [var_name, meta] : result.variables) {
            if (meta.ownership_type == OwnershipType::Shared &&
                meta.escape_status != EscapeStatus::NoEscape &&
                meta.escape_status != EscapeStatus::Unknown) {
                /* Already handled by apply_ownership_results */
            }
        }
    }
}

/* ════════════════════════════════════════════════════════════
   AST Walker
   ════════════════════════════════════════════════════════════ */

void MemoryAnalyzer::walk_block(const ASTNode* node) {
    while (node) {
        walk(node);
        node = node->next.get();
    }
}

void MemoryAnalyzer::walk(const ASTNode* node) {
    if (!node) return;

    switch (node->type) {

    /* ── Performance Function ── */
    case NodeType::PerformanceFn: {
        analyse_function(node);
        break;
    }

    /* ── Function / Lambda ── */
    case NodeType::Function:
    case NodeType::Lambda: {
        analyse_function(node);
        break;
    }

    /* ── Assignment ── */
    case NodeType::Assign: {
        analyse_assignment(node);
        break;
    }

    /* ── Return ── */
    case NodeType::Return: {
        analyse_return(node);
        break;
    }

    /* ── Variable read ── */
    case NodeType::Var: {
        /* Variable usage - track for escape analysis */
        break;
    }

    /* ── If / Else ── */
    case NodeType::If: {
        walk(node->left.get());   /* condition */
        walk_block(node->body.get());
        if (node->orelse) walk(node->orelse.get());
        break;
    }
    case NodeType::Else: {
        walk_block(node->body.get());
        break;
    }

    /* ── While / For / Loop ── */
    case NodeType::While: {
        walk(node->left.get());   /* condition */
        walk_block(node->body.get());
        break;
    }
    case NodeType::Loop:
    case NodeType::Repeat: {
        walk_block(node->body.get());
        break;
    }
    case NodeType::For: {
        walk(node->left.get());   /* iterable */
        /* loop variable is temporary */
        walk_block(node->body.get());
        break;
    }

    /* ── Match ── */
    case NodeType::Match: {
        walk(node->left.get());
        const ASTNode* case_ptr = node->args.get();
        while (case_ptr) {
            walk_block(case_ptr->body.get());
            case_ptr = case_ptr->next.get();
        }
        break;
    }

    /* ── Function call ── */
    case NodeType::Call: {
        analyse_call(node);
        break;
    }

    /* ── BinOp / UnaryOp ── */
    case NodeType::BinOp: {
        walk(node->left.get());
        walk(node->right.get());
        break;
    }
    case NodeType::UnaryOp: {
        walk(node->left.get());
        break;
    }

    /* ── Output ── */
    case NodeType::Output: {
        walk(node->left.get());
        break;
    }

    /* ── Class ── */
    case NodeType::Class: {
        walk_block(node->body.get());
        break;
    }

    /* ── Try / Catch ── */
    case NodeType::Try: {
        walk_block(node->body.get());
        if (node->orelse) walk_block(node->orelse.get());
        break;
    }

    /* ── Array ── */
    case NodeType::Array: {
        const ASTNode* elem = node->args.get();
        while (elem) {
            walk(elem);
            elem = elem->next.get();
        }
        break;
    }

    /* ── Index ── */
    case NodeType::Index: {
        walk(node->left.get());
        break;
    }

    /* ── Memory management nodes ── */
    case NodeType::Move:
    case NodeType::Drop:
    case NodeType::SharedRef:
    case NodeType::WeakRef:
    case NodeType::Borrow:
    case NodeType::Copy:
    case NodeType::Free: {
        /* These are handled in later phases */
        break;
    }

    /* ── Leaf nodes ── */
    case NodeType::StructLiteral: {
        const ASTNode* f = node->args.get();
        while (f) { if (f->left) walk(f->left.get()); f = f->next.get(); }
        break;
    }
    case NodeType::Num:
    case NodeType::Float:
    case NodeType::Str:
    case NodeType::Import:
    case NodeType::Break:
    case NodeType::Continue:
    case NodeType::Skip:
    case NodeType::Pass:
    case NodeType::Yield:
    case NodeType::ExternFn:
    case NodeType::ExternStruct:
    case NodeType::ExternUnion:
    case NodeType::FunctionType:
    case NodeType::Throw:
    case NodeType::Delete:
    case NodeType::IndexAssign:
    case NodeType::Attribute:
        break;

    default:
        /* Walk children defensively */
        walk(node->left.get());
        walk(node->right.get());
        walk_block(node->body.get());
        break;
    }
}

/* ════════════════════════════════════════════════════════════
   Function Analysis
   ════════════════════════════════════════════════════════════ */

void MemoryAnalyzer::analyse_function(const ASTNode* node) {
    /* Save current context if nested */
    FunctionContext saved = current_func_;

    /* Initialize new function context */
    current_func_ = FunctionContext();
    current_func_.func_name = node->value;
    current_func_.func_line = node->src_line;
    current_func_.is_performance_mode = (node->type == NodeType::PerformanceFn);

    /* Create analysis result */
    MemoryAnalysisResult result;
    result.is_performance_mode = current_func_.is_performance_mode;
    result.gc_disabled = current_func_.is_performance_mode;
    current_func_.result = result;

    /* Analyze parameters */
    const ASTNode* param = node->args.get();
    while (param) {
        VariableInfo var;
        var.name = param->value;
        var.decl_line = param->src_line;
        var.is_param = true;
        var.is_local = true;
        var.ownership_type = OwnershipType::Single;

        /* Default allocation for params */
        if (current_func_.is_performance_mode) {
            var.alloc_strategy = AllocStrategy::Stack;
        } else {
            var.alloc_strategy = AllocStrategy::RAII;
        }

        current_func_.variables[var.name] = var;
        param = param->next.get();
    }

    /* Analyze function body */
    walk_block(node->body.get());

    /* Finalize analysis results */
    for (auto& [name, var] : current_func_.variables) {
        decide_allocation(name, var);

        MemoryMetadata meta;
        meta.alloc_strategy = var.alloc_strategy;
        meta.forced_strategy = var.forced_strategy;
        meta.escape_status = var.escape_status;
        meta.lifetime_scope = var.lifetime_scope;
        meta.ownership_type = var.ownership_type;
        meta.decl_line = var.decl_line;
        meta.size_estimate = var.size_estimate;
        meta.is_mutable = var.is_mutable;

        current_func_.result.variables[name] = meta;
    }

    current_func_.result.update_stats();

    /* Store result */
    results_[current_func_.func_name] = current_func_.result;

    /* Restore previous context */
    current_func_ = saved;
}

/* ════════════════════════════════════════════════════════════
   Variable Declaration Analysis
   ════════════════════════════════════════════════════════════ */

void MemoryAnalyzer::analyse_variable_decl(const std::string& name, int line) {
    VariableInfo var;
    var.name = name;
    var.decl_line = line;
    var.is_local = true;
    var.ownership_type = OwnershipType::Single;
    var.lifetime_scope = LifetimeScope::Function;

    current_func_.variables[name] = var;
}

/* ════════════════════════════════════════════════════════════
   Assignment Analysis
   ════════════════════════════════════════════════════════════ */

void MemoryAnalyzer::analyse_assignment(const ASTNode* node) {
    if (!node->left || !node->right) return;

    const std::string& name = node->left->value;

    /* Check if variable already exists */
    auto it = current_func_.variables.find(name);
    if (it == current_func_.variables.end()) {
        /* New variable declaration */
        analyse_variable_decl(name, node->src_line);
    }

    /* ═══════════════════════════════════════════════════
       Propagate forced allocation strategy from node
       ═══════════════════════════════════════════════════ */
    if (is_forced_strategy(node->memory_meta.forced_strategy)) {
        auto var_it = current_func_.variables.find(name);
        if (var_it != current_func_.variables.end()) {
            var_it->second.forced_strategy = node->memory_meta.forced_strategy;
        }
    }

    /* Analyze RHS */
    walk(node->right.get());

    /* Check for escape in RHS */
    if (node->right->type == NodeType::Var) {
        /* y = x → x might escape if y outlives it */
        const std::string& src_name = node->right->value;
        auto src_it = current_func_.variables.find(src_name);
        if (src_it != current_func_.variables.end()) {
            src_it->second.escape_status = EscapeStatus::ArgEscape;
        }
    }
}

/* ════════════════════════════════════════════════════════════
   Return Analysis
   ════════════════════════════════════════════════════════════ */

void MemoryAnalyzer::analyse_return(const ASTNode* node) {
    if (!node->left) return;

    /* If returning a variable, it escapes */
    if (node->left->type == NodeType::Var) {
        const std::string& name = node->left->value;
        auto it = current_func_.variables.find(name);
        if (it != current_func_.variables.end()) {
            it->second.escape_status = EscapeStatus::ReturnEscape;
        }
    }

    walk(node->left.get());
}

/* ════════════════════════════════════════════════════════════
   Call Analysis
   ════════════════════════════════════════════════════════════ */

void MemoryAnalyzer::analyse_call(const ASTNode* node) {
    /* Analyze arguments */
    const ASTNode* arg = node->args.get();
    while (arg) {
        /* If passing variable to function, it might escape */
        if (arg->type == NodeType::Var) {
            const std::string& name = arg->value;
            auto it = current_func_.variables.find(name);
            if (it != current_func_.variables.end()) {
                it->second.escape_status = EscapeStatus::ArgEscape;
            }
        }
        walk(arg);
        arg = arg->next.get();
    }
}

/* ════════════════════════════════════════════════════════════
   Allocation Decision Engine
   ════════════════════════════════════════════════════════════ */

bool MemoryAnalyzer::can_use_stack(const VariableInfo& var) const {
    /* Stack is good for:
       - Small objects
       - Local scope
       - No escape
       - Single ownership
    */
    if (var.escape_status != EscapeStatus::NoEscape &&
        var.escape_status != EscapeStatus::Unknown) {
        return false;
    }
    if (var.ownership_type != OwnershipType::Single) {
        return false;
    }
    /* Size check: if known and > 1KB, don't stack */
    if (var.size_estimate > 1024) {
        return false;
    }
    return true;
}

bool MemoryAnalyzer::can_use_arena(const VariableInfo& var) const {
    /* Arena is good for:
       - Same lifetime group
       - Bulk allocation
       - Local scope
    */
    if (var.escape_status == EscapeStatus::ReturnEscape) {
        return false;
    }
    if (var.lifetime_scope == LifetimeScope::Function ||
        var.lifetime_scope == LifetimeScope::Block) {
        return true;
    }
    return false;
}

void MemoryAnalyzer::decide_allocation(const std::string& name, VariableInfo& var) {
    /* If already decided, skip */
    if (var.alloc_strategy != AllocStrategy::Unknown) {
        return;
    }

    /* Performance mode: Stack → Arena → RAII → ARC */
    if (current_func_.is_performance_mode) {
        if (can_use_stack(var)) {
            var.alloc_strategy = AllocStrategy::Stack;
        } else if (can_use_arena(var)) {
            var.alloc_strategy = AllocStrategy::Arena;
        } else if (var.ownership_type == OwnershipType::Single) {
            var.alloc_strategy = AllocStrategy::RAII;
        } else {
            var.alloc_strategy = AllocStrategy::ARC;
        }
    }
    /* Normal mode: Stack → Arena → RAII → ARC → GC */
    else {
        if (can_use_stack(var)) {
            var.alloc_strategy = AllocStrategy::Stack;
        } else if (can_use_arena(var)) {
            var.alloc_strategy = AllocStrategy::Arena;
        } else if (var.ownership_type == OwnershipType::Single) {
            var.alloc_strategy = AllocStrategy::RAII;
        } else if (var.ownership_type == OwnershipType::Shared) {
            var.alloc_strategy = AllocStrategy::ARC;
        } else {
            var.alloc_strategy = AllocStrategy::GC;
        }
    }
}

/* ════════════════════════════════════════════════════════════
   Query Helpers
   ════════════════════════════════════════════════════════════ */

const MemoryAnalysisResult& MemoryAnalyzer::get_result(const std::string& func_name) const {
    static MemoryAnalysisResult empty;
    auto it = results_.find(func_name);
    return (it != results_.end()) ? it->second : empty;
}

bool MemoryAnalyzer::is_performance_mode(const std::string& func_name) const {
    auto it = results_.find(func_name);
    return (it != results_.end()) ? it->second.is_performance_mode : false;
}

/* ════════════════════════════════════════════════════════════
   Apply Escape Analysis Results (Phase 2)
   ════════════════════════════════════════════════════════════ */

void MemoryAnalyzer::apply_escape_results() {
    /* For each function's variables, update escape status from EscapeAnalyzer */
    for (auto& [func_name, result] : results_) {
        for (auto& [var_name, meta] : result.variables) {
            /* Get escape status from escape analyzer */
            EscapeStatus escape_status = escape_analyzer_.get_escape_status(var_name);

            /* Update if escape analyzer has better info */
            if (escape_status != EscapeStatus::Unknown) {
                meta.escape_status = escape_status;
            }

            /* Update ownership type based on escape status */
            if (meta.ownership_type == OwnershipType::Single) {
                switch (escape_status) {
                    case EscapeStatus::GlobalEscape:
                    case EscapeStatus::ClosureEscape:
                        /* These need shared ownership */
                        meta.ownership_type = OwnershipType::Shared;
                        break;
                    case EscapeStatus::ArgEscape:
                        /* Passed as argument - might need shared */
                        meta.ownership_type = OwnershipType::Shared;
                        break;
                    case EscapeStatus::ReturnEscape:
                        /* Return values are transferred via inttoptr/ptrtoint,
                           no SharedBox wrapper needed */
                        meta.ownership_type = OwnershipType::Single;
                        break;
                    default:
                        break;
                }
            }
        }
    }
}

/* ════════════════════════════════════════════════════════════
   Print Escape Analysis Report
   ════════════════════════════════════════════════════════════ */

void MemoryAnalyzer::print_escape_report() const {
    std::cerr << "\n";
    std::cerr << "╔══════════════════════════════════════════════════════════╗\n";
    std::cerr << "║           Aurora Escape Analysis Report                  ║\n";
    std::cerr << "╠══════════════════════════════════════════════════════════╣\n";

    const auto& all_info = escape_analyzer_.get_all_escape_info();

    if (all_info.empty()) {
        std::cerr << "║  No variables analyzed.                                 ║\n";
    } else {
        std::cerr << "║  Variable          │ Escape Status     │ Count          ║\n";
        std::cerr << "╠══════════════════════════════════════════════════════════╣\n";

        for (const auto& [name, info] : all_info) {
            std::string status_name = escape_status_name(info.status);
            fprintf(stderr, "║  %-18s│ %-18s│                ║\n",
                   name.c_str(), status_name.c_str());
        }

        std::cerr << "╠══════════════════════════════════════════════════════════╣\n";
        std::cerr << "║  Statistics:                                             ║\n";
        std::cerr << "║    NoEscape:       " << escape_analyzer_.count_no_escape() << "                                ║\n";
        std::cerr << "║    ArgEscape:      " << escape_analyzer_.count_arg_escape() << "                                ║\n";
        std::cerr << "║    ReturnEscape:   " << escape_analyzer_.count_return_escape() << "                                ║\n";
        std::cerr << "║    GlobalEscape:   " << escape_analyzer_.count_global_escape() << "                                ║\n";
        std::cerr << "║    ClosureEscape:  " << escape_analyzer_.count_closure_escape() << "                                ║\n";
    }

    std::cerr << "╚══════════════════════════════════════════════════════════╝\n";
    std::cerr << "\n";
}

/* ════════════════════════════════════════════════════════════
   Apply Lifetime Analysis Results (Phase 3)
   ════════════════════════════════════════════════════════════ */

void MemoryAnalyzer::apply_lifetime_results() {
    /* For each function's variables, update lifetime scope from LifetimeAnalyzer */
    for (auto& [func_name, result] : results_) {
        for (auto& [var_name, meta] : result.variables) {
            /* Get lifetime scope from lifetime analyzer */
            LifetimeScope lifetime_scope = lifetime_analyzer_.get_lifetime_scope(var_name);

            /* Update if lifetime analyzer has better info */
            if (lifetime_scope != LifetimeScope::Unknown) {
                meta.lifetime_scope = lifetime_scope;
            }

            /* Update allocation strategy based on lifetime */
            if (meta.alloc_strategy == AllocStrategy::Unknown) {
                switch (lifetime_scope) {
                    case LifetimeScope::Function:
                    case LifetimeScope::Block:
                        /* Function/Block lifetime - can use stack or arena */
                        if (meta.escape_status == EscapeStatus::NoEscape) {
                            meta.alloc_strategy = AllocStrategy::Stack;
                        } else {
                            meta.alloc_strategy = AllocStrategy::Arena;
                        }
                        break;
                    case LifetimeScope::Loop:
                        /* Loop lifetime - stack per iteration */
                        meta.alloc_strategy = AllocStrategy::Stack;
                        break;
                    case LifetimeScope::Temporary:
                        /* Temporary - stack only */
                        meta.alloc_strategy = AllocStrategy::Stack;
                        break;
                    case LifetimeScope::Global:
                        /* Global - heap allocation */
                        meta.alloc_strategy = AllocStrategy::Heap;
                        break;
                    default:
                        break;
                }
            }
        }
    }
}

/* ════════════════════════════════════════════════════════════
   Print Lifetime Analysis Report
   ════════════════════════════════════════════════════════════ */

void MemoryAnalyzer::print_lifetime_report() const {
    lifetime_analyzer_.print_lifetime_report();
}

/* ════════════════════════════════════════════════════════════
   Apply Ownership Analysis Results (Phase 4)
   ════════════════════════════════════════════════════════════ */

void MemoryAnalyzer::apply_ownership_results() {
    /* Get all ownership info from analyzer */
    const auto& all_ownership = ownership_analyzer_.get_all_ownership_info();

    /* For each function's variables, update ownership type from OwnershipAnalyzer */
    for (auto& [func_name, result] : results_) {
        for (auto& [var_name, meta] : result.variables) {
            /* Use scoped name to avoid cross-function collisions */
            std::string scoped = func_name + "::" + var_name;

            /* Get ownership type from ownership analyzer */
            OwnershipType ownership_type = ownership_analyzer_.get_ownership_type(scoped);

            /* Update if ownership analyzer has better info */
            if (ownership_type != OwnershipType::Unknown) {
                meta.ownership_type = ownership_type;
            }

            /* Copy ref_count from ownership analyzer — try scoped first, then raw */
            auto oa_it = all_ownership.find(scoped);
            bool used_unscoped = false;
            if (oa_it == all_ownership.end()) {
                oa_it = all_ownership.find(var_name);
                used_unscoped = true;
            }
            if (oa_it != all_ownership.end()) {
                meta.ref_count = oa_it->second.owner_count;
            }

            /* MEDIUM 8 fix: use the same name for cycle check that was used for lookup */
            std::string cycle_check_name = used_unscoped ? var_name : scoped;
            bool in_cycle = ownership_analyzer_.is_in_reference_cycle(cycle_check_name);
            /* Also check the scoped name if unscoped was used */
            if (used_unscoped && !in_cycle) {
                in_cycle = ownership_analyzer_.is_in_reference_cycle(scoped);
            }

            /* Override allocation strategy based on ownership + cycle info */
            switch (ownership_type) {
                case OwnershipType::Shared: {
                    /* owner_count=1→RAII handled elsewhere; here 2+ → default ARC */
                    if (in_cycle) {
                        if (result.gc_disabled) {
                            /* @performance mode cannot tolerate GC */
                            std::cerr << "ERROR: Reference cycle detected for '"
                                      << var_name << "' in @performance function '"
                                      << func_name << "' at line "
                                      << meta.decl_line
                                      << " — use @gc attribute or restructure\n";
                            meta.alloc_strategy = AllocStrategy::ARC;
                            has_errors_ = true;
                        } else {
                            meta.alloc_strategy = AllocStrategy::GC;
                        }
                    } else {
                        meta.alloc_strategy = AllocStrategy::ARC;
                    }
                    break;
                }
                case OwnershipType::Weak:
                    meta.alloc_strategy = AllocStrategy::ARC;
                    break;
                case OwnershipType::Borrowed:
                    if (meta.alloc_strategy == AllocStrategy::Unknown)
                        meta.alloc_strategy = AllocStrategy::Stack;
                    break;
                default:
                    break;
            }
        }
    }
}

/* ════════════════════════════════════════════════════════════
   Print Ownership Analysis Reports
   ════════════════════════════════════════════════════════════ */

void MemoryAnalyzer::print_ownership_report() const {
    ownership_analyzer_.print_ownership_report();
}

void MemoryAnalyzer::print_alias_graph() const {
    ownership_analyzer_.print_alias_graph();
}

/* ════════════════════════════════════════════════════════════
   Apply Allocation Strategy (Phase 5)
   ════════════════════════════════════════════════════════════ */

void MemoryAnalyzer::apply_allocation_strategy(const ASTNode* root) {
    /* Run the AllocationStrategyEngine for sophisticated decision logic */
    allocation_engine_.analyse(root);

    /* Check if any function is in performance mode */
    bool any_perf_mode = false;
    for (const auto& [func_name, result] : results_) {
        if (result.is_performance_mode) {
            any_perf_mode = true;
            break;
        }
    }

    /* Set allocation engine mode */
    allocation_engine_.set_performance_mode(any_perf_mode);

    /* For each function's variables, make allocation decisions */
    for (auto& [func_name, result] : results_) {
        for (auto& [var_name, meta] : result.variables) {
            /* ═══════════════════════════════════════════════════
               Step 1: Check for forced allocation strategy
               ═══════════════════════════════════════════════════ */
            if (is_forced_strategy(meta.forced_strategy)) {
                meta.alloc_strategy = meta.forced_strategy;
                continue;
            }

            /* ═══════════════════════════════════════════════════
               Step 2: Auto-determine allocation strategy
               ═══════════════════════════════════════════════════ */
            if (can_use_stack_direct(meta)) {
                meta.alloc_strategy = AllocStrategy::Stack;
            } else if (can_use_arena_direct(meta)) {
                meta.alloc_strategy = AllocStrategy::Arena;
            } else if (meta.ownership_type == OwnershipType::Single) {
                meta.alloc_strategy = AllocStrategy::RAII;
            } else if (meta.ownership_type == OwnershipType::Shared) {
                /* ARC vs GC — GC only when a reference cycle is detected */
                /* MEDIUM 8 fix: also try unscoped name for cycle check */
                std::string scoped = func_name + "::" + var_name;
                bool in_cycle = ownership_analyzer_.is_in_reference_cycle(scoped);
                if (!in_cycle) {
                    in_cycle = ownership_analyzer_.is_in_reference_cycle(var_name);
                }
                if (in_cycle && !result.gc_disabled) {
                    meta.alloc_strategy = AllocStrategy::GC;
                } else {
                    meta.alloc_strategy = AllocStrategy::ARC;
                }
            } else if (!result.gc_disabled) {
                meta.alloc_strategy = AllocStrategy::GC;
            } else {
                meta.alloc_strategy = AllocStrategy::ARC;
            }
        }
    }
}

bool MemoryAnalyzer::can_use_stack_direct(const MemoryMetadata& meta) const {
    /* Stack is good for:
       - Small objects
       - Local scope
       - No escape
       - Single ownership
    */
    if (meta.escape_status != EscapeStatus::NoEscape &&
        meta.escape_status != EscapeStatus::Unknown) {
        return false;
    }
    if (meta.ownership_type != OwnershipType::Single &&
        meta.ownership_type != OwnershipType::Unknown) {
        return false;
    }
    if (meta.size_estimate > 1024) {
        return false;
    }
    /* HIGH 9 fix: global lifetime variables cannot be stack-allocated (use-after-return) */
    if (meta.lifetime_scope == LifetimeScope::Global) {
        return false;
    }
    return true;
}

bool MemoryAnalyzer::can_use_arena_direct(const MemoryMetadata& meta) const {
    /* Arena is good for:
       - Same lifetime group
       - Bulk allocation
       - Local scope
       - Single ownership only
    */
    if (meta.escape_status == EscapeStatus::ReturnEscape) {
        return false;
    }
    if (meta.ownership_type == OwnershipType::Shared ||
        meta.ownership_type == OwnershipType::Weak ||
        meta.ownership_type == OwnershipType::Borrowed) {
        return false;
    }
    if (meta.lifetime_scope == LifetimeScope::Global) {
        return false;
    }
    if (meta.lifetime_scope == LifetimeScope::Function ||
        meta.lifetime_scope == LifetimeScope::Block) {
        return true;
    }
    return false;
}

/* ════════════════════════════════════════════════════════════
   Propagate Results to AST Nodes for Codegen
   ════════════════════════════════════════════════════════════ */

void MemoryAnalyzer::apply_to_ast(ASTNode* root) {
    if (!root) return;

    /* Walk through Function/PerformanceFn nodes */
    if (root->type == NodeType::Function || root->type == NodeType::PerformanceFn) {
        const std::string& func_name = root->value;
        auto func_it = results_.find(func_name);
        if (func_it != results_.end()) {
            const auto& vars = func_it->second.variables;
            /* Walk function body assignments */
            std::function<void(ASTNode*)> walk_body = [&](ASTNode* node) {
                while (node) {
                    if (node->type == NodeType::Assign) {
                        const std::string& lhs = node->left->value;
                        auto var_it = vars.find(lhs);
                        if (var_it != vars.end()) {
                            node->memory_meta.ownership_type = var_it->second.ownership_type;
                            node->memory_meta.alloc_strategy  = var_it->second.alloc_strategy;
                            node->memory_meta.ref_count       = var_it->second.ref_count;
                        }
                    }
                    /* Recurse into nested bodies */
                    if (node->body) walk_body(node->body.get());
                    if (node->orelse) {
                        if (node->orelse->type == NodeType::If ||
                            node->orelse->type == NodeType::Else)
                            walk_body(node->orelse.get());
                        else if (node->orelse->body)
                            walk_body(node->orelse->body.get());
                    }
                    node = node->next.get();
                }
            };
            walk_body(root->body.get());
        }
    }

    /* Recurse into all children */
    if (root->left) apply_to_ast(root->left.get());
    if (root->right) apply_to_ast(root->right.get());
    if (root->body) apply_to_ast(root->body.get());
    if (root->orelse) apply_to_ast(root->orelse.get());
    if (root->args) apply_to_ast(root->args.get());
    if (root->next) apply_to_ast(root->next.get());
}

/* ════════════════════════════════════════════════════════════
   Print Allocation Strategy Report
   ════════════════════════════════════════════════════════════ */

void MemoryAnalyzer::print_allocation_report() const {
    std::cerr << "\n";
    std::cerr << "╔══════════════════════════════════════════════════════════╗\n";
    std::cerr << "║      Aurora Allocation Strategy Report                   ║\n";
    std::cerr << "╠══════════════════════════════════════════════════════════╣\n";

    /* Count strategies */
    int stack_count = 0, arena_count = 0, raii_count = 0;
    int arc_count = 0, gc_count = 0;

    for (const auto& [func_name, result] : results_) {
        for (const auto& [var_name, meta] : result.variables) {
            switch (meta.alloc_strategy) {
                case AllocStrategy::Stack:  stack_count++;  break;
                case AllocStrategy::Arena:  arena_count++;  break;
                case AllocStrategy::RAII:   raii_count++;   break;
                case AllocStrategy::ARC:    arc_count++;    break;
                case AllocStrategy::GC:     gc_count++;     break;
                default: break;
            }
        }
    }

    std::cerr << "║  Allocation Distribution:                                ║\n";
    std::cerr << "╠══════════════════════════════════════════════════════════╣\n";

    int total = stack_count + arena_count + raii_count + arc_count + gc_count;
    if (total > 0) {
        fprintf(stderr, "║  Stack: %3d (%3d%%)  Arena: %3d (%3d%%)  RAII: %3d (%3d%%)    ║\n",
               stack_count, (stack_count * 100 / total),
               arena_count, (arena_count * 100 / total),
               raii_count, (raii_count * 100 / total));
        fprintf(stderr, "║  ARC:   %3d (%3d%%)  GC:    %3d (%3d%%)                      ║\n",
               arc_count, (arc_count * 100 / total),
               gc_count, (gc_count * 100 / total));
    } else {
        std::cerr << "║  No allocation decisions made.                          ║\n";
    }

    std::cerr << "╠══════════════════════════════════════════════════════════╣\n";
    std::cerr << "║  Mode: " << (results_.empty() ? "Normal" :
        (results_.begin()->second.is_performance_mode ? "@performance" : "Normal")) << "                                        ║\n";
    std::cerr << "╚══════════════════════════════════════════════════════════╝\n";
    std::cerr << "\n";
}

/* ════════════════════════════════════════════════════════════
   Allocation Profiler Reports (Phase 8)
   ════════════════════════════════════════════════════════════ */

void MemoryAnalyzer::print_profiler_report() const {
    allocation_profiler_.print_allocation_report();
}

void MemoryAnalyzer::print_detailed_report() const {
    allocation_profiler_.print_detailed_report();
}

void MemoryAnalyzer::print_performance_report() const {
    allocation_profiler_.print_performance_report();
}

void MemoryAnalyzer::print_percentage_report() const {
    allocation_profiler_.print_percentage_report();
}

std::string MemoryAnalyzer::export_profiler_json() const {
    return allocation_profiler_.export_json();
}

std::string MemoryAnalyzer::export_profiler_csv() const {
    return allocation_profiler_.export_csv();
}
