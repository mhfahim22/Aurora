// ownership_analyzer.cpp — Ownership & Alias Analysis Engine Implementation
// Part of the Aurora compiler pipeline — Phase 4

#include "compiler/ownership_analyzer.hpp"
#include <sstream>
#include <iostream>
#include <algorithm>
#include <functional>

/* ════════════════════════════════════════════════════════════
   OwnershipAnalyzer — Main Entry Point
   ════════════════════════════════════════════════════════════ */

void OwnershipAnalyzer::analyse(const ASTNode* root) {
    push_scope("global");
    walk_block(root);
    pop_scope();

    /* Propagate ownership through alias graph */
    for (auto& [name, info] : ownership_info_) {
        propagate_ownership(name);
    }

    /* Detect reference cycles in the alias graph */
    detect_reference_cycles();

    /* Build unscoped fallback map for backward compatibility */
    ownership_info_unscoped_.clear();
    for (const auto& [scoped_name, info] : ownership_info_) {
        auto pos = scoped_name.find("::");
        std::string raw_name = (pos != std::string::npos)
            ? scoped_name.substr(pos + 2)
            : scoped_name;
        ownership_info_unscoped_[raw_name] = info;
    }
}

/* ════════════════════════════════════════════════════════════
   AST Walker
   ════════════════════════════════════════════════════════════ */

void OwnershipAnalyzer::walk_block(const ASTNode* node) {
    while (node) {
        walk(node);
        node = node->next.get();
    }
}

void OwnershipAnalyzer::walk(const ASTNode* node) {
    if (!node) return;

    switch (node->type) {

    /* ── Function definition ── */
    case NodeType::Function:
    case NodeType::PerformanceFn: {
        current_func_name_ = node->value;

        push_scope(node->value);

        /* Add parameters as single-owned */
        const ASTNode* param = node->args.get();
        while (param) {
            record_variable(param->value, OwnershipType::Single,
                           node->src_line, true);
            add_local_var(param->value);
            param = param->next.get();
        }

        /* Walk function body */
        walk_block(node->body.get());

        pop_scope();
        current_func_name_ = "";
        break;
    }

    /* ── Assignment ── */
    case NodeType::Assign: {
        if (!node->left || !node->right) break;

        const std::string& lhs_raw = node->left->value;
        const std::string  lhs_scoped = key(lhs_raw);

        /* Check if LHS already exists */
        auto it = ownership_info_.find(lhs_scoped);
        if (it == ownership_info_.end()) {
            /* New variable - single owner */
            record_variable(lhs_raw, OwnershipType::Single,
                           node->src_line, true);
            add_local_var(lhs_raw);
        }

        /* Analyze RHS */
        if (node->right->type == NodeType::Var) {
            /* y = x — alias created */
            const std::string& rhs_raw = node->right->value;
            add_alias_edge(key(rhs_raw), lhs_scoped, true, node->src_line);
            merge_aliases(key(rhs_raw), lhs_scoped);
        }
        else if (node->right->type == NodeType::SharedRef) {
            /* y = shared x — shared ownership */
            const std::string& src_raw = node->right->value;
            record_variable(lhs_raw, OwnershipType::Shared,
                           node->src_line, true);
            add_alias_edge(key(src_raw), lhs_scoped, true, node->src_line);
            detect_shared_owner(src_raw, node->src_line);
        }
        else if (node->right->type == NodeType::WeakRef) {
            /* y = weak x — weak reference */
            const std::string& src_raw = node->right->value;
            record_variable(lhs_raw, OwnershipType::Weak,
                           node->src_line, true);
            add_alias_edge(key(src_raw), lhs_scoped, false, node->src_line);
        }
        else if (node->right->type == NodeType::Borrow) {
            /* y = borrow x — borrowed reference */
            const std::string& src_raw = node->right->value;
            record_variable(lhs_raw, OwnershipType::Borrowed,
                           node->src_line, false);
            /* HIGH 3 fix: check if this is a mutable borrow */
            if (node->right->is_mutable) {
                detect_mutable_borrow(lhs_raw, src_raw, node->src_line);
            } else {
                detect_immutable_borrow(lhs_raw, src_raw, node->src_line);
            }
        }
        else if (node->right->type == NodeType::Copy) {
            /* y = copy x — deep copy, separate ownership */
            const std::string& src_raw = node->right->value;
            record_variable(lhs_raw, OwnershipType::Single,
                           node->src_line, true);
            detect_copy(lhs_raw, node->src_line);
        }
        else if (node->right->type == NodeType::Move) {
            /* y = move x — ownership transfer */
            const std::string& src_raw = node->right->value;
            record_variable(lhs_raw, OwnershipType::Single,
                           node->src_line, true);
            detect_move(src_raw, node->src_line);
        }
        else {
            /* Other expressions - single owner */
            auto lhs_it = ownership_info_.find(lhs_scoped);
            if (lhs_it != ownership_info_.end()) {
                lhs_it->second.type = OwnershipType::Single;
                lhs_it->second.owner_count = 1;
            }
        }

        /* Walk RHS for nested expressions */
        walk(node->right.get());
        break;
    }

    /* ── Return statement ── */
    case NodeType::Return: {
        if (node->left && node->left->type == NodeType::Var) {
            /* Returning a variable - it escapes, might need shared */
            const std::string& name = key(node->left->value);
            auto it = ownership_info_.find(name);
            if (it != ownership_info_.end()) {
                /* If escaping, consider shared ownership */
                if (it->second.type == OwnershipType::Single) {
                    it->second.type = OwnershipType::Shared;
                    it->second.owner_count++;
                }
            }
        }
        if (node->left) walk(node->left.get());
        break;
    }

    /* ── Function call ── */
    case NodeType::Call: {
        /* Passing arguments - might create aliases or be consumed */
        /* Check if this call consumes (moves) its arguments (CRITICAL 1 fix) */
        bool consumes = node->consumes_args;
        auto consumed_it = consumed_args_map_.find(node->value);
        if (consumed_it != consumed_args_map_.end()) {
            consumes = true;
        }

        int arg_idx = 0;
        const ASTNode* arg = node->args.get();
        while (arg) {
            if (arg->type == NodeType::Var) {
                const std::string& name = key(arg->value);
                auto it = ownership_info_.find(name);
                if (it != ownership_info_.end()) {
                    /* Check if this specific argument is consumed (moved) */
                    bool is_consumed = false;
                    if (node->consumes_args) {
                        is_consumed = true;
                    } else if (consumed_it != consumed_args_map_.end()) {
                        const auto& indices = consumed_it->second;
                        is_consumed = std::find(indices.begin(), indices.end(), arg_idx) != indices.end();
                    }

                    if (is_consumed) {
                        /* Argument is moved into the function - decrement owner count */
                        it->second.owner_count--;
                        if (it->second.owner_count <= 0) {
                            it->second.type = OwnershipType::Unknown;
                        }
                    } else {
                        /* Increase ref count for function calls (not consumed) */
                        it->second.strong_ref_count++;
                    }
                }
            }
            walk(arg);
            arg = arg->next.get();
            arg_idx++;
        }
        break;
    }

    /* ── Memory management nodes ── */
    case NodeType::Move: {
        /* move x — ownership transfer, x becomes invalid */
        detect_move(node->value, node->src_line);
        break;
    }
    case NodeType::Drop: {
        /* drop x — explicit destruction */
        auto it = ownership_info_.find(key(node->value));
        if (it != ownership_info_.end()) {
            it->second.owner_count--;
            if (it->second.owner_count <= 0) {
                it->second.type = OwnershipType::Unknown;
            }
        }
        break;
    }
    case NodeType::SharedRef: {
        /* shared x — create shared reference */
        detect_shared_owner(node->value, node->src_line);
        break;
    }
    case NodeType::WeakRef: {
        /* weak x — create weak reference */
        auto it = ownership_info_.find(key(node->value));
        if (it != ownership_info_.end()) {
            it->second.weak_ref_count++;
        }
        break;
    }
    case NodeType::Borrow: {
        /* borrow x — create immutable borrow */
        /* Find the most recent variable being assigned to */
        /* This is a simplified version - in reality we'd track the assignment context */
        break;
    }
    case NodeType::Copy: {
        /* copy x — deep copy, no ownership change */
        detect_copy(node->value, node->src_line);
        break;
    }
    case NodeType::Free: {
        /* free x — explicit free */
        auto it = ownership_info_.find(key(node->value));
        if (it != ownership_info_.end()) {
            it->second.owner_count = 0;
            it->second.type = OwnershipType::Unknown;
        }
        break;
    }

    /* ── If / Else ── */
    case NodeType::If: {
        walk(node->left.get());   /* condition */
        push_scope(current_func_name_);
        walk_block(node->body.get());
        pop_scope();
        if (node->orelse) {
            if (node->orelse->type == NodeType::If) {
                walk(node->orelse.get());
            } else {
                push_scope(current_func_name_);
                walk_block(node->orelse->body.get());
                pop_scope();
            }
        }
        break;
    }
    case NodeType::Else: {
        push_scope(current_func_name_);
        walk_block(node->body.get());
        pop_scope();
        break;
    }

    /* ── While / For / Loop ── */
    case NodeType::While: {
        walk(node->left.get());   /* condition */
        push_scope(current_func_name_);
        walk_block(node->body.get());
        pop_scope();
        break;
    }
    case NodeType::Loop:
    case NodeType::Repeat: {
        push_scope(current_func_name_);
        walk_block(node->body.get());
        if (node->left) walk(node->left.get());
        pop_scope();
        break;
    }
    case NodeType::Match: {
        walk(node->left.get());
        const ASTNode* case_ptr = node->args.get();
        while (case_ptr) {
            push_scope(current_func_name_);
            walk_block(case_ptr->body.get());
            pop_scope();
            case_ptr = case_ptr->next.get();
        }
        break;
    }

    case NodeType::For: {
        walk(node->left.get());   /* iterable */
        push_scope(current_func_name_);
        /* Loop variable is single-owned per iteration */
        record_variable(node->value, OwnershipType::Single,
                       node->src_line, true);
        add_local_var(node->value);
        walk_block(node->body.get());
        pop_scope();
        break;
    }

    /* ── Variable reference ── */
    case NodeType::Var: {
        /* Track variable usage */
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

    /* ── Index / IndexAssign ── */
    case NodeType::Index: {
        walk(node->left.get());
        break;
    }
    case NodeType::IndexAssign: {
        if (node->left) walk(node->left.get());
        if (node->right) walk(node->right.get());
        break;
    }

    /* ── Attribute access ── */
    case NodeType::Attribute: {
        walk(node->left.get());
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
        push_scope(current_func_name_);
        walk_block(node->body.get());
        pop_scope();
        break;
    }

    /* ── Try / Catch ── */
    case NodeType::Try: {
        push_scope(current_func_name_);
        walk_block(node->body.get());
        pop_scope();
        if (node->orelse) {
            push_scope(current_func_name_);
            walk_block(node->orelse.get());
            pop_scope();
        }
        if (node->right) {
            push_scope(current_func_name_);
            walk_block(node->right->body.get());
            pop_scope();
        }
        break;
    }

    /* ── Lambda (closure) ── */
    case NodeType::Lambda: {
        push_scope(current_func_name_);
        walk_block(node->body.get());
        pop_scope();
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
   Ownership Detection Methods
   ════════════════════════════════════════════════════════════ */

void OwnershipAnalyzer::detect_single_owner(const std::string& name, int line) {
    auto it = ownership_info_.find(key(name));
    if (it != ownership_info_.end()) {
        it->second.type = OwnershipType::Single;
        it->second.owner_count = 1;
    }
}

void OwnershipAnalyzer::detect_shared_owner(const std::string& name, int line) {
    auto it = ownership_info_.find(key(name));
    if (it != ownership_info_.end()) {
        if (it->second.type == OwnershipType::Single) {
            it->second.type = OwnershipType::Shared;
            it->second.owner_count = 2;
        } else {
            it->second.owner_count++;
        }
        it->second.strong_ref_count++;
    }
}

void OwnershipAnalyzer::detect_mutable_borrow(const std::string& borrower,
                                               const std::string& lender,
                                               int line) {
    /* Record mutable borrow */
    BorrowInfo info;
    info.borrower = key(borrower);
    info.lender = key(lender);
    info.is_mutable = true;
    info.start_line = line;
    borrows_.push_back(info);

    /* Update lender */
    auto lender_it = ownership_info_.find(key(lender));
    if (lender_it != ownership_info_.end()) {
        lender_it->second.has_mutable_borrow = true;
    }

    /* Update borrower */
    auto borrower_it = ownership_info_.find(key(borrower));
    if (borrower_it != ownership_info_.end()) {
        borrower_it->second.type = OwnershipType::Borrowed;
        borrower_it->second.borrowed_from = key(lender);
    }
}

void OwnershipAnalyzer::detect_immutable_borrow(const std::string& borrower,
                                                 const std::string& lender,
                                                 int line) {
    /* Record immutable borrow */
    BorrowInfo info;
    info.borrower = key(borrower);
    info.lender = key(lender);
    info.is_mutable = false;
    info.start_line = line;
    borrows_.push_back(info);

    /* Update lender */
    auto lender_it = ownership_info_.find(key(lender));
    if (lender_it != ownership_info_.end()) {
        lender_it->second.immutable_borrow_count++;
    }

    /* Update borrower */
    auto borrower_it = ownership_info_.find(key(borrower));
    if (borrower_it != ownership_info_.end()) {
        borrower_it->second.type = OwnershipType::Borrowed;
        borrower_it->second.borrowed_from = key(lender);
    }
}

void OwnershipAnalyzer::detect_move(const std::string& name, int line) {
    auto it = ownership_info_.find(key(name));
    if (it != ownership_info_.end()) {
        /* Source becomes invalid */
        it->second.owner_count--;
        if (it->second.owner_count <= 0) {
            it->second.type = OwnershipType::Unknown;
        }
    }
}

void OwnershipAnalyzer::detect_copy(const std::string& name, int line) {
    auto it = ownership_info_.find(key(name));
    if (it != ownership_info_.end()) {
        /* Copy doesn't change ownership, just creates a new owner */
        it->second.owner_count++;
    }
}

/* ════════════════════════════════════════════════════════════
   Alias Graph Methods
   ════════════════════════════════════════════════════════════ */

void OwnershipAnalyzer::add_alias_edge(const std::string& from,
                                        const std::string& to,
                                        bool is_strong, int line) {
    /* Both from/to are already scoped by caller */
    AliasEdge edge;
    edge.from = from;
    edge.to = to;
    edge.is_strong = is_strong;
    edge.line = line;
    alias_edges_.push_back(edge);

    /* Add to alias lists */
    auto from_it = ownership_info_.find(from);
    if (from_it != ownership_info_.end()) {
        from_it->second.aliases.push_back(to);
    }
    auto to_it = ownership_info_.find(to);
    if (to_it != ownership_info_.end()) {
        to_it->second.aliases.push_back(from);
    }
}

void OwnershipAnalyzer::merge_aliases(const std::string& a, const std::string& b) {
    /* Both a/b are already scoped by caller */
    auto a_it = ownership_info_.find(a);
    auto b_it = ownership_info_.find(b);
    if (a_it == ownership_info_.end() || b_it == ownership_info_.end()) return;

    /* Merge alias lists */
    for (const auto& alias : b_it->second.aliases) {
        bool found = false;
        for (const auto& existing : a_it->second.aliases) {
            if (existing == alias) { found = true; break; }
        }
        if (!found) a_it->second.aliases.push_back(alias);
    }
    for (const auto& alias : a_it->second.aliases) {
        bool found = false;
        for (const auto& existing : b_it->second.aliases) {
            if (existing == alias) { found = true; break; }
        }
        if (!found) b_it->second.aliases.push_back(alias);
    }
}

void OwnershipAnalyzer::propagate_ownership(const std::string& name) {
    /* name is already scoped (from ownership_info_ iteration) */
    auto it = ownership_info_.find(name);
    if (it == ownership_info_.end()) return;

    /* If has aliases, might need shared ownership */
    if (!it->second.aliases.empty() && it->second.type == OwnershipType::Single) {
        it->second.type = OwnershipType::Shared;
        it->second.owner_count = static_cast<int>(it->second.aliases.size()) + 1;
    }
}

/* ════════════════════════════════════════════════════════════
   Reference Cycle Detection
   ════════════════════════════════════════════════════════════ */

void OwnershipAnalyzer::detect_reference_cycles() {
    cycle_vars_.clear();

    /* Build directed adjacency list from alias edges */
    std::unordered_map<std::string, std::vector<std::string>> adj;
    for (const auto& edge : alias_edges_) {
        /* Only strong edges contribute to reference cycles */
        if (!edge.is_strong) continue;
        if (!ownership_info_.count(edge.from) ||
            !ownership_info_.count(edge.to)) continue;
        adj[edge.from].push_back(edge.to);
    }

    if (adj.empty()) return;

    /* DFS with 3-color marking to find back-edges (cycles) */
    enum Color { WHITE, GRAY, BLACK };
    std::unordered_map<std::string, Color> color;
    std::vector<std::string> path;

    std::function<void(const std::string&)> dfs =
        [&](const std::string& node) {
            color[node] = GRAY;
            path.push_back(node);

            for (const auto& neighbor : adj[node]) {
                if (color[neighbor] == GRAY) {
                    /* Back-edge found — mark all nodes in the cycle */
                    auto start = std::find(path.begin(), path.end(), neighbor);
                    if (start != path.end()) {
                        for (auto it = start; it != path.end(); ++it) {
                            cycle_vars_.insert(*it);
                        }
                    }
                } else if (color[neighbor] == WHITE) {
                    dfs(neighbor);
                }
            }

            path.pop_back();
            color[node] = BLACK;
        };

    for (const auto& [node, _] : adj) {
        if (color[node] == WHITE) {
            dfs(node);
        }
    }
}

bool OwnershipAnalyzer::is_in_reference_cycle(const std::string& var_name) const {
    return cycle_vars_.count(var_name) > 0;
}

/* ════════════════════════════════════════════════════════════
   Helper Methods
   ════════════════════════════════════════════════════════════ */

void OwnershipAnalyzer::record_variable(const std::string& raw_name,
                                         OwnershipType type,
                                         int line,
                                         bool is_mutable) {
    const std::string scoped = key(raw_name);
    auto it = ownership_info_.find(scoped);
    if (it != ownership_info_.end()) {
        /* Update existing */
        it->second.decl_line = line;
        return;
    }

    /* Create new ownership info */
    OwnershipInfo info;
    info.var_name = scoped;
    info.decl_line = line;
    info.type = type;
    info.is_mutable = is_mutable;
    info.owner_count = 1;

    ownership_info_[scoped] = info;
}

void OwnershipAnalyzer::add_local_var(const std::string& name) {
    if (!scope_stack_.empty()) {
        scope_stack_.back().local_vars.insert(name);
    }
}

void OwnershipAnalyzer::push_scope(const std::string& func_name) {
    Scope scope;
    scope.func_name = func_name;
    scope_stack_.push_back(std::move(scope));
}

void OwnershipAnalyzer::pop_scope() {
    if (!scope_stack_.empty()) {
        scope_stack_.pop_back();
    }
}

bool OwnershipAnalyzer::is_local_var(const std::string& name) const {
    for (int i = static_cast<int>(scope_stack_.size()) - 1; i >= 0; i--) {
        if (scope_stack_[i].local_vars.count(name) > 0) {
            return true;
        }
    }
    return false;
}

/* ════════════════════════════════════════════════════════════
   Query Methods
   ════════════════════════════════════════════════════════════ */

OwnershipType OwnershipAnalyzer::get_ownership_type(const std::string& var_name) const {
    /* Try scoped name first, then unscoped fallback */
    auto it = ownership_info_.find(var_name);
    if (it != ownership_info_.end()) return it->second.type;
    auto uit = ownership_info_unscoped_.find(var_name);
    return (uit != ownership_info_unscoped_.end()) ? uit->second.type : OwnershipType::Unknown;
}

bool OwnershipAnalyzer::is_single_owner(const std::string& var_name) const {
    return get_ownership_type(var_name) == OwnershipType::Single;
}

bool OwnershipAnalyzer::is_shared(const std::string& var_name) const {
    return get_ownership_type(var_name) == OwnershipType::Shared;
}

bool OwnershipAnalyzer::is_borrowed(const std::string& var_name) const {
    return get_ownership_type(var_name) == OwnershipType::Borrowed;
}

bool OwnershipAnalyzer::has_aliases(const std::string& var_name) const {
    auto it = ownership_info_.find(var_name);
    if (it != ownership_info_.end()) return !it->second.aliases.empty();
    auto uit = ownership_info_unscoped_.find(var_name);
    return (uit != ownership_info_unscoped_.end()) && !uit->second.aliases.empty();
}

int OwnershipAnalyzer::count_single() const {
    int count = 0;
    for (const auto& [name, info] : ownership_info_) {
        if (info.type == OwnershipType::Single) count++;
    }
    return count;
}

int OwnershipAnalyzer::count_shared() const {
    int count = 0;
    for (const auto& [name, info] : ownership_info_) {
        if (info.type == OwnershipType::Shared) count++;
    }
    return count;
}

int OwnershipAnalyzer::count_weak() const {
    int count = 0;
    for (const auto& [name, info] : ownership_info_) {
        if (info.type == OwnershipType::Weak) count++;
    }
    return count;
}

int OwnershipAnalyzer::count_borrowed() const {
    int count = 0;
    for (const auto& [name, info] : ownership_info_) {
        if (info.type == OwnershipType::Borrowed) count++;
    }
    return count;
}

int OwnershipAnalyzer::count_aliases() const {
    int count = 0;
    for (const auto& [name, info] : ownership_info_) {
        if (!info.aliases.empty()) count++;
    }
    return count;
}

/* ════════════════════════════════════════════════════════════
   Print Reports
   ════════════════════════════════════════════════════════════ */

void OwnershipAnalyzer::print_ownership_report() const {
    std::cerr << "\n";
    std::cerr << "╔══════════════════════════════════════════════════════════╗\n";
    std::cerr << "║        Aurora Ownership Analysis Report                  ║\n";
    std::cerr << "╠══════════════════════════════════════════════════════════╣\n";

    if (ownership_info_.empty()) {
        std::cerr << "║  No variables analyzed.                                 ║\n";
    } else {
        std::cerr << "║  Variable          │ Ownership Type   │ Owner Count     ║\n";
        std::cerr << "╠══════════════════════════════════════════════════════════╣\n";

        for (const auto& [name, info] : ownership_info_) {
            std::string type_name = ownership_type_name(info.type);
            /* Strip function prefix for display (use last :: for safety) */
            auto pos = name.rfind("::");
            std::string display = (pos != std::string::npos) ? name.substr(pos + 2) : name;
            fprintf(stderr, "║  %-18s│ %-17s│ %d               ║\n",
                   display.c_str(), type_name.c_str(), info.owner_count);
        }

        std::cerr << "╠══════════════════════════════════════════════════════════╣\n";
        std::cerr << "║  Statistics:                                             ║\n";
        std::cerr << "║    Single:       " << count_single() << "                                ║\n";
        std::cerr << "║    Shared:       " << count_shared() << "                                ║\n";
        std::cerr << "║    Weak:         " << count_weak() << "                                ║\n";
        std::cerr << "║    Borrowed:     " << count_borrowed() << "                                ║\n";
        std::cerr << "║    With Aliases: " << count_aliases() << "                                ║\n";
    }

    std::cerr << "╚══════════════════════════════════════════════════════════╝\n";
    std::cerr << "\n";
}

void OwnershipAnalyzer::print_alias_graph() const {
    std::cerr << "\n";
    std::cerr << "╔══════════════════════════════════════════════════════════╗\n";
    std::cerr << "║           Aurora Alias Graph                             ║\n";
    std::cerr << "╠══════════════════════════════════════════════════════════╣\n";

    if (alias_edges_.empty()) {
        std::cerr << "║  No alias relationships.                                ║\n";
    } else {
        std::cerr << "║  From              → To                │ Type            ║\n";
        std::cerr << "╠══════════════════════════════════════════════════════════╣\n";

        for (const auto& edge : alias_edges_) {
            std::string type_str = edge.is_strong ? "Strong" : "Weak";
            fprintf(stderr, "║  %-18s→ %-18s│ %-16s║\n",
                   edge.from.c_str(), edge.to.c_str(), type_str.c_str());
        }
    }

    std::cerr << "╚══════════════════════════════════════════════════════════╝\n";
    std::cerr << "\n";
}