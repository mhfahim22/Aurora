// escape_analyzer.cpp — Escape Analysis Engine Implementation
// Part of the Aurora compiler pipeline — Phase 2

#include "compiler/escape_analyzer.hpp"
#include <sstream>
#include <algorithm>

/* ════════════════════════════════════════════════════════════
   EscapeAnalyzer — Main Entry Point
   ════════════════════════════════════════════════════════════ */

void EscapeAnalyzer::analyse(const ASTNode* root) {
    push_scope("global");
    walk(root);
    pop_scope();
}

/* ════════════════════════════════════════════════════════════
   AST Walker
   ════════════════════════════════════════════════════════════ */

void EscapeAnalyzer::walk_block(const ASTNode* node) {
    while (node) {
        walk(node);
        node = node->next.get();
    }
}

void EscapeAnalyzer::walk(const ASTNode* node) {
    if (!node) return;

    switch (node->type) {

    /* ── Function definition ── */
    case NodeType::Function:
    case NodeType::PerformanceFn: {
        current_func_name_ = node->value;
        bool saved_perf = in_performance_mode_;
        in_performance_mode_ = (node->type == NodeType::PerformanceFn);

        /* Push new scope for function */
        push_scope(node->value);

        /* Add parameters as local variables */
        const ASTNode* param = node->args.get();
        while (param) {
            add_local_var(param->value);
            param = param->next.get();
        }

        /* Walk function body */
        walk_block(node->body.get());

        /* Pop scope */
        pop_scope();

        in_performance_mode_ = saved_perf;
        current_func_name_ = "";
        break;
    }

    /* ── Assignment ── */
    case NodeType::Assign: {
        if (!node->left || !node->right) break;

        const std::string& lhs_name = node->left->value;

        /* Check if LHS is a global variable */
        detect_global_assignment(node);

        /* Check if LHS is already tracked */
        auto it = escape_info_.find(lhs_name);
        if (it == escape_info_.end()) {
            /* New variable declaration */
            EscapeInfo info;
            info.var_name = lhs_name;
            info.decl_line = node->src_line;
            info.status = EscapeStatus::NoEscape;  /* initially no escape */
            escape_info_[lhs_name] = info;
            add_local_var(lhs_name);
        }

        /* Analyze RHS for escape */
        walk(node->right.get());

        /* If RHS is a variable, it might escape */
        if (node->right->type == NodeType::Var) {
            const std::string& rhs_name = node->right->value;
            auto rhs_it = escape_info_.find(rhs_name);
            if (rhs_it != escape_info_.end()) {
                /* RHS variable escapes through assignment to LHS */
                record_escape(rhs_name, EscapeStatus::ArgEscape,
                              node->src_line, "assigned to another variable");
            }
        }

        /* If RHS is a function call, check for return escape */
        if (node->right->type == NodeType::Call) {
            walk(node->right.get());
        }

        /* If RHS is array, check elements */
        if (node->right->type == NodeType::Array) {
            walk(node->right.get());
        }

        break;
    }

    /* ── Return statement ── */
    case NodeType::Return: {
        detect_return_escape(node);
        if (node->left) walk(node->left.get());
        break;
    }

    /* ── Function call ── */
    case NodeType::Call: {
        detect_argument_escape(node);
        /* Walk arguments */
        const ASTNode* arg = node->args.get();
        while (arg) {
            walk(arg);
            arg = arg->next.get();
        }
        break;
    }

    /* ── Lambda (closure) ── */
    case NodeType::Lambda: {
        detect_closure_capture(node);
        break;
    }

    /* ── Variable reference ── */
    case NodeType::Var: {
        /* Track variable usage */
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

    /* ── While ── */
    case NodeType::While: {
        walk(node->left.get());   /* condition */
        push_scope(current_func_name_);
        walk_block(node->body.get());
        pop_scope();
        break;
    }

    /* ── Loop ── */
    case NodeType::Loop:
    case NodeType::Repeat: {
        push_scope(current_func_name_);
        walk_block(node->body.get());
        if (node->left) walk(node->left.get());
        pop_scope();
        break;
    }

    /* ── Match ── */
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

    /* ── For ── */
    case NodeType::For: {
        walk(node->left.get());   /* iterable */
        push_scope(current_func_name_);
        /* Loop variable is local to the loop scope */
        add_local_var(node->value);
        walk_block(node->body.get());
        pop_scope();
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

    /* ── IndexAssign ── */
    case NodeType::IndexAssign: {
        /* arr[idx] = expr — arr might escape */
        auto it = escape_info_.find(node->value);
        if (it != escape_info_.end()) {
            it->second.ref_count++;
        }
        if (node->left) walk(node->left.get());
        if (node->right) walk(node->right.get());
        break;
    }

    /* ── Attribute access ── */
    case NodeType::Attribute: {
        walk(node->left.get());
        break;
    }

    /* ── Memory management nodes ── */
    case NodeType::Move: {
        /* move x — x becomes invalid, no escape */
        break;
    }
    case NodeType::Drop: {
        /* drop x — explicit destruction, no escape */
        break;
    }
    case NodeType::SharedRef: {
        /* shared x — x is now shared, escape through refcount */
        detect_reference_escape(node);
        break;
    }
    case NodeType::WeakRef: {
        /* weak x — weak reference, escape through refcount */
        detect_reference_escape(node);
        break;
    }
    case NodeType::Borrow: {
        /* borrow x — temporary reference, no escape */
        break;
    }
    case NodeType::Copy: {
        /* copy x — deep copy, source stays */
        break;
    }
    case NodeType::Free: {
        /* free x — explicit free, no escape */
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

    /* ── Struct literal: walk field values ── */
    case NodeType::StructLiteral: {
        const ASTNode* f = node->args.get();
        while (f) { if (f->left) walk(f->left.get()); f = f->next.get(); }
        break;
    }
    /* ── Leaf nodes ── */
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
   Escape Detection Methods
   ════════════════════════════════════════════════════════════ */

void EscapeAnalyzer::detect_return_escape(const ASTNode* node) {
    if (!node->left) return;

    /* If returning a variable, it escapes */
    if (node->left->type == NodeType::Var) {
        const std::string& name = node->left->value;
        record_escape(name, EscapeStatus::ReturnEscape,
                      node->src_line, "returned from function");
    }
    /* If returning an array, check elements */
    else if (node->left->type == NodeType::Array) {
        const ASTNode* elem = node->left->args.get();
        while (elem) {
            if (elem->type == NodeType::Var) {
                record_escape(elem->value, EscapeStatus::ReturnEscape,
                              node->src_line, "returned in array");
            }
            elem = elem->next.get();
        }
    }
    /* If returning a new object, it doesn't escape (new allocation) */
}

void EscapeAnalyzer::detect_closure_capture(const ASTNode* node) {
    /* HIGH 6 fix: use recursive helper to find ALL Var references at any depth */
    ClosureCaptureInfo capture_info;
    capture_info.closure_line = node->src_line;

    std::unordered_set<std::string> captured;
    find_all_var_refs(node->body.get(), captured);

    if (!captured.empty()) {
        for (const auto& var_name : captured) {
            capture_info.captured_vars.push_back(var_name);
            record_escape(var_name, EscapeStatus::ClosureEscape,
                          node->src_line, "captured by closure");
        }
        closure_captures_.push_back(capture_info);
    }
}

/* HIGH 6 fix: recursively walk the AST subtree to find all Var references */
void EscapeAnalyzer::find_all_var_refs(const ASTNode* node, std::unordered_set<std::string>& out_vars) const {
    if (!node) return;

    if (node->type == NodeType::Var) {
        /* Check if this variable is local to the lambda (i.e., declared in current scope) */
        bool is_local = false;
        for (const auto& scope : scope_stack_) {
            if (scope.local_vars.count(node->value) > 0) {
                is_local = true;
                break;
            }
        }
        if (!is_local) {
            /* This variable is captured from outer scope */
            out_vars.insert(node->value);
        }
        return;
    }

    /* Recurse into all children of the node */
    find_all_var_refs(node->left.get(), out_vars);
    find_all_var_refs(node->right.get(), out_vars);
    find_all_var_refs(node->body.get(), out_vars);
    find_all_var_refs(node->orelse.get(), out_vars);
    find_all_var_refs(node->args.get(), out_vars);
    find_all_var_refs(node->next.get(), out_vars);
}

void EscapeAnalyzer::detect_global_assignment(const ASTNode* node) {
    if (!node->left) return;

    /* Check if LHS is a global variable */
    bool is_global = false;
    for (const auto& gv : global_vars_) {
        if (gv.var_name == node->left->value) {
            is_global = true;
            break;
        }
    }

    if (is_global) {
        /* Assigning to a global variable */
        if (node->right && node->right->type == NodeType::Var) {
            const std::string& rhs_name = node->right->value;
            record_escape(rhs_name, EscapeStatus::GlobalEscape,
                          node->src_line, "assigned to global variable");
        }
    }
}

void EscapeAnalyzer::detect_reference_escape(const ASTNode* node) {
    /* shared/weak/borrow x — x is referenced, might escape */
    if (node->type == NodeType::SharedRef ||
        node->type == NodeType::WeakRef) {
        /* These create references that outlive the original scope */
        record_escape(node->value, EscapeStatus::ArgEscape,
                      node->src_line, "referenced by shared/weak");
    }
}

void EscapeAnalyzer::detect_argument_escape(const ASTNode* node) {
    /* If passing variable to function, it might escape */
    const ASTNode* arg = node->args.get();
    while (arg) {
        if (arg->type == NodeType::Var) {
            record_escape(arg->value, EscapeStatus::ArgEscape,
                          node->src_line, "passed as function argument");
        }
        /* Check array elements */
        else if (arg->type == NodeType::Array) {
            const ASTNode* elem = arg->args.get();
            while (elem) {
                if (elem->type == NodeType::Var) {
                    record_escape(elem->value, EscapeStatus::ArgEscape,
                                  node->src_line, "passed in array argument");
                }
                elem = elem->next.get();
            }
        }
        arg = arg->next.get();
    }
}

/* ════════════════════════════════════════════════════════════
   Helper Methods
   ════════════════════════════════════════════════════════════ */

void EscapeAnalyzer::record_escape(const std::string& var_name,
                                   EscapeStatus status,
                                   int line,
                                   const std::string& context) {
    /* Find or create escape info */
    auto it = escape_info_.find(var_name);
    if (it == escape_info_.end()) {
        EscapeInfo info;
        info.var_name = var_name;
        info.decl_line = line;
        info.status = status;
        it = escape_info_.insert({var_name, info}).first;
    }

    /* Update escape status (keep the most severe) */
    if (status > it->second.status) {
        it->second.status = status;
    }

    /* Record escape point */
    EscapeInfo::EscapePoint point;
    point.type = status;
    point.line = line;
    point.context = current_func_name_;
    it->second.escape_points.push_back(point);
}

void EscapeAnalyzer::add_local_var(const std::string& name) {
    if (!scope_stack_.empty()) {
        scope_stack_.back().local_vars.insert(name);
    }
}

void EscapeAnalyzer::push_scope(const std::string& func_name) {
    Scope scope;
    scope.func_name = func_name;
    scope_stack_.push_back(std::move(scope));
}

void EscapeAnalyzer::pop_scope() {
    if (!scope_stack_.empty()) {
        scope_stack_.pop_back();
    }
}

bool EscapeAnalyzer::is_local_var(const std::string& name) const {
    for (int i = (int)scope_stack_.size() - 1; i >= 0; i--) {
        if (scope_stack_[i].local_vars.count(name) > 0) {
            return true;
        }
    }
    return false;
}

bool EscapeAnalyzer::is_global_var(const std::string& name) const {
    for (const auto& gv : global_vars_) {
        if (gv.var_name == name) {
            return true;
        }
    }
    return false;
}

/* ════════════════════════════════════════════════════════════
   Query Methods
   ════════════════════════════════════════════════════════════ */

EscapeStatus EscapeAnalyzer::get_escape_status(const std::string& var_name) const {
    auto it = escape_info_.find(var_name);
    return (it != escape_info_.end()) ? it->second.status : EscapeStatus::Unknown;
}

bool EscapeAnalyzer::does_escape(const std::string& var_name) const {
    EscapeStatus status = get_escape_status(var_name);
    return status != EscapeStatus::NoEscape && status != EscapeStatus::Unknown;
}

int EscapeAnalyzer::count_no_escape() const {
    int count = 0;
    for (const auto& [name, info] : escape_info_) {
        if (info.status == EscapeStatus::NoEscape) count++;
    }
    return count;
}

int EscapeAnalyzer::count_arg_escape() const {
    int count = 0;
    for (const auto& [name, info] : escape_info_) {
        if (info.status == EscapeStatus::ArgEscape) count++;
    }
    return count;
}

int EscapeAnalyzer::count_return_escape() const {
    int count = 0;
    for (const auto& [name, info] : escape_info_) {
        if (info.status == EscapeStatus::ReturnEscape) count++;
    }
    return count;
}

int EscapeAnalyzer::count_global_escape() const {
    int count = 0;
    for (const auto& [name, info] : escape_info_) {
        if (info.status == EscapeStatus::GlobalEscape) count++;
    }
    return count;
}

int EscapeAnalyzer::count_closure_escape() const {
    int count = 0;
    for (const auto& [name, info] : escape_info_) {
        if (info.status == EscapeStatus::ClosureEscape) count++;
    }
    return count;
}
