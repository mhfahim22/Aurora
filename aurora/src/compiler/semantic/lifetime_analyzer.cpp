#include "compiler/lifetime_analyzer.hpp"
#include <sstream>
#include <iostream>
#include <algorithm>
#include <functional>

/* ════════════════════════════════════════════════════════════
   LifetimeAnalyzer — Main Entry Point
   ════════════════════════════════════════════════════════════ */

void LifetimeAnalyzer::analyse(const ASTNode* root) {
    push_scope(LifetimeScope::Global, 0);
    walk(root);
    pop_scope(0);
    unify_regions();
}

/* ════════════════════════════════════════════════════════════
   AST Walker
   ════════════════════════════════════════════════════════════ */

void LifetimeAnalyzer::walk_block(const ASTNode* node) {
    while (node) {
        walk(node);
        node = node->next.get();
    }
}

void LifetimeAnalyzer::walk(const ASTNode* node) {
    if (!node) return;

    switch (node->type) {

    case NodeType::Function:
    case NodeType::PerformanceFn: {
        current_func_name_ = node->value;
        current_func_line_ = node->src_line;

        push_scope(LifetimeScope::Function, node->src_line);

        const ASTNode* param = node->args.get();
        while (param) {
            record_variable(param->value, LifetimeScope::Function,
                           node->src_line, false, true);
            add_local_var(param->value);
            param = param->next.get();
        }

        create_region(LifetimeScope::Function, node->src_line);

        walk_block(node->body.get());

        finalize_region();
        pop_scope(node->src_line);

        current_func_name_ = "";
        current_func_line_ = 0;
        break;
    }

    case NodeType::Assign: {
        if (!node->left || !node->right) break;

        const std::string& name = node->left->value;
        LifetimeScope scope = determine_lifetime(name);

        /* Determine more precise scope from right-hand side */
        switch (node->right->type) {
            case NodeType::Num:
            case NodeType::Float:
            case NodeType::Str:
            case NodeType::Call:
            case NodeType::Array: {
                record_variable(name, scope, node->src_line);
                const ASTNode* elem = node->right->args.get();
                while (elem) { walk(elem); elem = elem->next.get(); }
                break;
            }
            case NodeType::Var: {
                /* y = x — y gets the same lifetime as x if x is function-level */
                record_variable(name, scope, node->src_line);
                update_last_use(node->right->value, node->src_line);
                break;
            }
            case NodeType::New: {
                /* new object — block/function lifetime */
                record_variable(name, scope, node->src_line);
                walk(node->right->args.get());
                break;
            }
            default: {
                record_variable(name, scope, node->src_line);
                break;
            }
        }

        walk(node->right.get());
        break;
    }

    case NodeType::Return: {
        if (node->left && node->left->type == NodeType::Var) {
            const std::string& name = node->left->value;
            auto it = lifetime_info_.find(name);
            if (it != lifetime_info_.end()) {
                if (it->second.scope == LifetimeScope::Block ||
                    it->second.scope == LifetimeScope::Loop ||
                    it->second.scope == LifetimeScope::Temporary) {
                    it->second.scope = LifetimeScope::Function;
                }
            }
        }
        walk(node->left.get());
        break;
    }

    case NodeType::If: {
        walk(node->left.get());
        push_scope(LifetimeScope::Block, node->src_line);
        walk_block(node->body.get());
        pop_scope(node->src_line);

        if (node->orelse) {
            if (node->orelse->type == NodeType::If) {
                walk(node->orelse.get());
            } else {
                push_scope(LifetimeScope::Block, node->src_line);
                walk_block(node->orelse->body.get());
                pop_scope(node->src_line);
            }
        }
        break;
    }
    case NodeType::Else: {
        push_scope(LifetimeScope::Block, node->src_line);
        walk_block(node->body.get());
        pop_scope(node->src_line);
        break;
    }

    case NodeType::While: {
        walk(node->left.get());

        bool saved_in_loop = in_loop_;
        int saved_loop_depth = loop_depth_;
        in_loop_ = true;
        loop_depth_++;

        push_scope(LifetimeScope::Loop, node->src_line);
        create_region(LifetimeScope::Loop, node->src_line);
        walk_block(node->body.get());
        finalize_region();
        pop_scope(node->src_line);

        in_loop_ = saved_in_loop;
        loop_depth_ = saved_loop_depth;
        break;
    }

    case NodeType::Loop:
    case NodeType::Repeat: {
        bool saved_in_loop = in_loop_;
        int saved_loop_depth = loop_depth_;
        in_loop_ = true;
        loop_depth_++;

        push_scope(LifetimeScope::Loop, node->src_line);
        create_region(LifetimeScope::Loop, node->src_line);
        walk_block(node->body.get());
        if (node->left) walk(node->left.get());
        finalize_region();
        pop_scope(node->src_line);

        in_loop_ = saved_in_loop;
        loop_depth_ = saved_loop_depth;
        break;
    }

    case NodeType::Match: {
        walk(node->left.get());
        const ASTNode* case_ptr = node->args.get();
        while (case_ptr) {
            push_scope(LifetimeScope::Block, case_ptr->src_line);
            walk_block(case_ptr->body.get());
            pop_scope(case_ptr->src_line);
            case_ptr = case_ptr->next.get();
        }
        break;
    }

    case NodeType::For: {
        walk(node->left.get());

        bool saved_in_loop = in_loop_;
        int saved_loop_depth = loop_depth_;
        in_loop_ = true;
        loop_depth_++;

        push_scope(LifetimeScope::Loop, node->src_line);

        record_variable(node->value, LifetimeScope::Loop, node->src_line, true);
        add_local_var(node->value);

        walk_block(node->body.get());
        pop_scope(node->src_line);

        in_loop_ = saved_in_loop;
        loop_depth_ = saved_loop_depth;
        break;
    }

    case NodeType::Var: {
        update_last_use(node->value, node->src_line);
        break;
    }

    case NodeType::Call: {
        const ASTNode* arg = node->args.get();
        while (arg) {
            if (arg->type == NodeType::Var) {
                update_last_use(arg->value, node->src_line);
            }
            walk(arg);
            arg = arg->next.get();
        }
        break;
    }

    case NodeType::Array: {
        const ASTNode* elem = node->args.get();
        while (elem) {
            walk(elem);
            elem = elem->next.get();
        }
        break;
    }

    case NodeType::Index: {
        walk(node->left.get());
        break;
    }

    case NodeType::IndexAssign: {
        if (node->left) walk(node->left.get());
        if (node->right) walk(node->right.get());
        break;
    }

    case NodeType::Attribute: {
        walk(node->left.get());
        break;
    }

    case NodeType::BinOp: {
        walk(node->left.get());
        walk(node->right.get());
        break;
    }
    case NodeType::UnaryOp: {
        walk(node->left.get());
        break;
    }

    case NodeType::Output: {
        walk(node->left.get());
        break;
    }

    case NodeType::Class: {
        push_scope(LifetimeScope::Function, node->src_line);
        walk_block(node->body.get());
        pop_scope(node->src_line);
        break;
    }

    case NodeType::Try: {
        push_scope(LifetimeScope::Block, node->src_line);
        walk_block(node->body.get());
        pop_scope(node->src_line);

        if (node->orelse) {
            push_scope(LifetimeScope::Block, node->src_line);
            walk_block(node->orelse.get());
            pop_scope(node->src_line);
        }
        if (node->right) {
            push_scope(LifetimeScope::Block, node->src_line);
            walk_block(node->right->body.get());
            pop_scope(node->src_line);
        }
        break;
    }

    case NodeType::Lambda: {
        push_scope(LifetimeScope::Function, node->src_line);
        walk_block(node->body.get());
        pop_scope(node->src_line);
        break;
    }

    case NodeType::Move:
    case NodeType::Drop:
    case NodeType::SharedRef:
    case NodeType::WeakRef:
    case NodeType::Borrow:
    case NodeType::Copy:
    case NodeType::Free: {
        if (!node->value.empty()) {
            update_last_use(node->value, node->src_line);
        }
        break;
    }

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
        walk(node->left.get());
        walk(node->right.get());
        walk_block(node->body.get());
        break;
    }
}

/* ════════════════════════════════════════════════════════════
   Lifetime Detection Methods
   ════════════════════════════════════════════════════════════ */

void LifetimeAnalyzer::detect_scope_lifetime(const std::string& name, int line) {
    auto it = lifetime_info_.find(name);
    if (it != lifetime_info_.end()) {
        it->second.end_line = line;
    }
}

void LifetimeAnalyzer::detect_function_lifetime(const std::string& name, int line) {
    auto it = lifetime_info_.find(name);
    if (it != lifetime_info_.end()) {
        it->second.scope = LifetimeScope::Function;
        it->second.end_line = line;
    }
}

void LifetimeAnalyzer::detect_loop_lifetime(const std::string& name, int line) {
    auto it = lifetime_info_.find(name);
    if (it != lifetime_info_.end()) {
        it->second.scope = LifetimeScope::Loop;
        it->second.is_loop_var = true;
        it->second.end_line = line;
    }
}

void LifetimeAnalyzer::detect_temporary_lifetime(const std::string& name, int line) {
    auto it = lifetime_info_.find(name);
    if (it != lifetime_info_.end()) {
        it->second.scope = LifetimeScope::Temporary;
        it->second.end_line = line;
    }
}

void LifetimeAnalyzer::detect_global_lifetime(const std::string& name, int line) {
    auto it = lifetime_info_.find(name);
    if (it != lifetime_info_.end()) {
        it->second.scope = LifetimeScope::Global;
        it->second.end_line = line;
    }
}

/* ════════════════════════════════════════════════════════════
   Region Grouping — improved with unification
   ════════════════════════════════════════════════════════════ */

void LifetimeAnalyzer::create_region(LifetimeScope scope_type, int start_line) {
    RegionInfo region;
    region.region_id = (int)regions_.size();
    region.scope_type = scope_type;
    region.start_line = start_line;
    regions_.push_back(region);
}

void LifetimeAnalyzer::add_to_region(const std::string& var_name) {
    if (regions_.empty()) return;

    auto it = lifetime_info_.find(var_name);
    if (it != lifetime_info_.end()) {
        int region_id = find_or_create_region(it->second.scope, it->second.decl_line);
        it->second.region_id = region_id;
        it->second.can_share_arena = true;

        /* Add to the right region */
        for (auto& r : regions_) {
            if (r.region_id == region_id) {
                if (std::find(r.var_names.begin(), r.var_names.end(), var_name) == r.var_names.end()) {
                    r.var_names.push_back(var_name);
                }
                break;
            }
        }
    }
}

void LifetimeAnalyzer::finalize_region() {
    if (regions_.empty()) return;

    RegionInfo& region = regions_.back();
    if (!region.var_names.empty()) {
        int total_size = 0;
        for (const auto& var_name : region.var_names) {
            auto it = lifetime_info_.find(var_name);
            if (it != lifetime_info_.end()) {
                int estimated_size = 64;
                /* Better estimates based on scope type */
                if (it->second.is_param) estimated_size = 128;
                if (it->second.is_loop_var) estimated_size = 32;
                total_size += estimated_size;
                region.total_size_est = total_size;
            }
        }
        region.end_line = region.start_line;
        for (const auto& var_name : region.var_names) {
            auto it = lifetime_info_.find(var_name);
            if (it != lifetime_info_.end() && it->second.end_line > region.end_line) {
                region.end_line = it->second.end_line;
            }
        }
    }
}

int LifetimeAnalyzer::find_or_create_region(LifetimeScope scope_type, int line) {
    /* Look for an existing region with the same scope type that overlaps at this line */
    for (auto& r : regions_) {
        if (r.scope_type == scope_type) {
            return r.region_id;
        }
    }
    /* Create new region if none found */
    create_region(scope_type, line);
    return regions_.back().region_id;
}

/* Unified regions — merge compatible regions */
void LifetimeAnalyzer::unify_regions() {
    if (regions_.size() < 2) return;

    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; i < regions_.size(); i++) {
            for (size_t j = i + 1; j < regions_.size(); j++) {
                RegionInfo& a = regions_[i];
                RegionInfo& b = regions_[j];

                /* HIGH 7 fix: only merge regions with the same scope type
                   if their lifetime intervals actually overlap */
                if (a.scope_type == b.scope_type) {
                    /* Check if intervals overlap */
                    bool overlap = (a.start_line <= b.end_line && b.start_line <= a.end_line);

                    if (!overlap) {
                        /* Non-overlapping regions — keep separate */
                        continue;
                    }

                    /* Merge b into a */
                    for (const auto& var : b.var_names) {
                        if (std::find(a.var_names.begin(), a.var_names.end(), var) == a.var_names.end()) {
                            a.var_names.push_back(var);
                        }
                        auto it = lifetime_info_.find(var);
                        if (it != lifetime_info_.end()) {
                            it->second.region_id = a.region_id;
                        }
                    }
                    a.start_line = std::min(a.start_line, b.start_line);
                    a.end_line = std::max(a.end_line, b.end_line);
                    a.total_size_est += b.total_size_est;

                    regions_.erase(regions_.begin() + j);
                    changed = true;
                    break;
                }
            }
            if (changed) break;
        }
    }
}

/* ════════════════════════════════════════════════════════════
   Scope Management
   ════════════════════════════════════════════════════════════ */

void LifetimeAnalyzer::push_scope(LifetimeScope scope_type, int line) {
    ScopeInfo3 scope;
    scope.scope_id = next_scope_id_++;
    scope.parent_id = scopes_.empty() ? -1 : scopes_.back().scope_id;
    scope.scope_type = scope_type;
    scope.depth = current_depth_++;
    scope.start_line = line;
    scopes_.push_back(std::move(scope));
}

void LifetimeAnalyzer::pop_scope(int line) {
    if (!scopes_.empty()) {
        scopes_.back().end_line = line;
        /* Update end_line for all variables in this scope */
        for (const auto& var_name : scopes_.back().local_vars) {
            auto it = lifetime_info_.find(var_name);
            if (it != lifetime_info_.end()) {
                it->second.end_line = line;
            }
        }
        scopes_.pop_back();
        current_depth_--;
    }
}

void LifetimeAnalyzer::add_local_var(const std::string& name) {
    if (!scopes_.empty()) {
        scopes_.back().local_vars.push_back(name);
    }
}

/* ════════════════════════════════════════════════════════════
   Helper Methods
   ════════════════════════════════════════════════════════════ */

void LifetimeAnalyzer::update_last_use(const std::string& name, int line) {
    auto it = lifetime_info_.find(name);
    if (it != lifetime_info_.end()) {
        if (line > it->second.end_line) {
            it->second.end_line = line;
        }
    }
}

void LifetimeAnalyzer::record_variable(const std::string& name,
                                        LifetimeScope scope,
                                        int line,
                                        bool is_loop,
                                        bool is_param) {
    auto it = lifetime_info_.find(name);
    if (it != lifetime_info_.end()) {
        it->second.end_line = line;
        /* If in a more nested scope, keep the outer scope */
        if (scope < it->second.scope) {
            it->second.scope = scope;
        }
        return;
    }

    LifetimeInfo info;
    info.var_name = name;
    info.decl_line = line;
    info.scope = scope;
    info.scope_depth = current_depth_;
    info.start_line = line;
    info.end_line = line;
    info.is_loop_var = is_loop;
    info.is_param = is_param;

    lifetime_info_[name] = info;

    /* Assign to best matching region */
    add_to_region(name);
}

LifetimeScope LifetimeAnalyzer::determine_lifetime(const std::string& name) const {
    if (in_loop_) {
        return LifetimeScope::Loop;
    }
    for (int i = (int)scopes_.size() - 1; i >= 0; i--) {
        if (scopes_[i].scope_type == LifetimeScope::Function) {
            return LifetimeScope::Function;
        }
        if (scopes_[i].scope_type == LifetimeScope::Block) {
            return LifetimeScope::Block;
        }
    }
    return LifetimeScope::Function;
}

/* ════════════════════════════════════════════════════════════
   Query Methods
   ════════════════════════════════════════════════════════════ */

LifetimeScope LifetimeAnalyzer::get_lifetime_scope(const std::string& var_name) const {
    auto it = lifetime_info_.find(var_name);
    return (it != lifetime_info_.end()) ? it->second.scope : LifetimeScope::Unknown;
}

bool LifetimeAnalyzer::is_function_lifetime(const std::string& var_name) const {
    return get_lifetime_scope(var_name) == LifetimeScope::Function;
}

bool LifetimeAnalyzer::is_block_lifetime(const std::string& var_name) const {
    return get_lifetime_scope(var_name) == LifetimeScope::Block;
}

bool LifetimeAnalyzer::is_loop_lifetime(const std::string& var_name) const {
    return get_lifetime_scope(var_name) == LifetimeScope::Loop;
}

bool LifetimeAnalyzer::is_temporary(const std::string& var_name) const {
    return get_lifetime_scope(var_name) == LifetimeScope::Temporary;
}

int LifetimeAnalyzer::count_function_lifetime() const {
    int count = 0;
    for (const auto& [name, info] : lifetime_info_) {
        if (info.scope == LifetimeScope::Function) count++;
    }
    return count;
}

int LifetimeAnalyzer::count_block_lifetime() const {
    int count = 0;
    for (const auto& [name, info] : lifetime_info_) {
        if (info.scope == LifetimeScope::Block) count++;
    }
    return count;
}

int LifetimeAnalyzer::count_loop_lifetime() const {
    int count = 0;
    for (const auto& [name, info] : lifetime_info_) {
        if (info.scope == LifetimeScope::Loop) count++;
    }
    return count;
}

int LifetimeAnalyzer::count_temporary() const {
    int count = 0;
    for (const auto& [name, info] : lifetime_info_) {
        if (info.scope == LifetimeScope::Temporary) count++;
    }
    return count;
}

int LifetimeAnalyzer::count_global_lifetime() const {
    int count = 0;
    for (const auto& [name, info] : lifetime_info_) {
        if (info.scope == LifetimeScope::Global) count++;
    }
    return count;
}

int LifetimeAnalyzer::count_regions() const {
    return (int)regions_.size();
}

/* ════════════════════════════════════════════════════════════
   Print Report
   ════════════════════════════════════════════════════════════ */

void LifetimeAnalyzer::print_lifetime_report() const {
    std::cerr << "\n";
    std::cerr << "╔══════════════════════════════════════════════════════════╗\n";
    std::cerr << "║          Aurora Lifetime Analysis Report                 ║\n";
    std::cerr << "╠══════════════════════════════════════════════════════════╣\n";

    if (lifetime_info_.empty()) {
        std::cerr << "║  No variables analyzed.                                 ║\n";
    } else {
        std::cerr << "║  Variable          │ Lifetime Scope   │ Region ID       ║\n";
        std::cerr << "╠══════════════════════════════════════════════════════════╣\n";

        for (const auto& [name, info] : lifetime_info_) {
            std::string scope_name = lifetime_scope_name(info.scope);
            fprintf(stderr, "║  %-18s│ %-17s│ %d               ║\n",
                   name.c_str(), scope_name.c_str(), info.region_id);
        }

        std::cerr << "╠══════════════════════════════════════════════════════════╣\n";
        std::cerr << "║  Statistics:                                             ║\n";
        std::cerr << "║    Function:      " << count_function_lifetime() << "                                ║\n";
        std::cerr << "║    Block:         " << count_block_lifetime() << "                                ║\n";
        std::cerr << "║    Loop:          " << count_loop_lifetime() << "                                ║\n";
        std::cerr << "║    Temporary:     " << count_temporary() << "                                ║\n";
        std::cerr << "║    Global:        " << count_global_lifetime() << "                                ║\n";
        std::cerr << "║    Regions:       " << count_regions() << "                                ║\n";

        std::cerr << "╠══════════════════════════════════════════════════════════╣\n";
        std::cerr << "║  Region Details:                                         ║\n";
        for (const auto& r : regions_) {
            fprintf(stderr, "║    R%d (%s): %zu vars, est %d bytes, lines %d-%d   ║\n",
                   r.region_id, lifetime_scope_name(r.scope_type),
                   r.var_names.size(), r.total_size_est,
                   r.start_line, r.end_line);
        }
    }

    std::cerr << "╚══════════════════════════════════════════════════════════╝\n";
    std::cerr << "\n";
}
