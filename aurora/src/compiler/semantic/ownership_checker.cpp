#include "compiler/ownership.hpp"
#include <algorithm>
#include <set>

/* ════════════════════════════════════════════════════════════
   Scope management
   ════════════════════════════════════════════════════════════ */

void OwnershipTracker::push_scope() {
    Scope s;
    s.scope_id = next_scope_id_++;
    s.parent_id = scopes_.empty() ? -1 : scopes_.back().scope_id;
    scopes_.push_back(std::move(s));
}

std::vector<std::string> OwnershipTracker::pop_scope() {
    if (scopes_.empty()) return {};
    auto owned = scopes_.back().owned_vars();

    /* End any borrows involving variables in the outgoing scope */
    for (const auto& [name, _] : scopes_.back().vars) {
        end_borrows_for(name);
        /* Also end all borrows where this var is a lender */
        end_all_borrows(name, -1);
        /* Remove from lifetime map if present */
        lifetime_map_.erase(name);
    }

    scopes_.pop_back();
    return owned;
}

void OwnershipTracker::declare(const std::string& name,
                                OwnershipState     state,
                                int                ln) {
    if (scopes_.empty()) push_scope();
    VarInfo info;
    info.state = state;
    info.src_line = ln;
    info.lifetime_id = next_lifetime_id_++;
    scopes_.back().vars[name] = info;
    lifetime_map_[name] = info.lifetime_id;
}

VarInfo* OwnershipTracker::lookup(const std::string& name) {
    for (int i = static_cast<int>(scopes_.size()) - 1; i >= 0; i--) {
        auto* v = scopes_[i].find(name);
        if (v) return v;
    }
    return nullptr;
}

int OwnershipTracker::current_scope_id() const {
    return scopes_.empty() ? -1 : scopes_.back().scope_id;
}

/* ════════════════════════════════════════════════════════════
   Borrow tracking
   ════════════════════════════════════════════════════════════ */

void OwnershipTracker::end_borrows_for(const std::string& borrower_name) {
    for (auto it = active_borrows_.begin(); it != active_borrows_.end(); ) {
        auto& borrows = it->second;
        /* MEDIUM 5 fix: don't erase borrows immediately - just mark as ended */
        /* Only end borrows whose last_use indicates they're truly done */
        bool has_active = false;
        for (auto& br : borrows) {
            if (br.borrower == borrower_name && !br.ended) {
                /* Keep last_use_line intact for NLL analysis */
                br.ended = true;
            }
            if (!br.ended) has_active = true;
        }
        /* Remove ended borrows only when the lender also has no active borrows */
        borrows.erase(
            std::remove_if(borrows.begin(), borrows.end(),
                [](const BorrowRecord& br) { return br.ended; }),
            borrows.end());
        if (borrows.empty()) {
            it = active_borrows_.erase(it);
        } else {
            ++it;
        }
    }
}

/* MEDIUM 5 fix: NLL — auto-end borrows whose last use is before current_line */
void OwnershipTracker::try_end_borrows(const std::string& lender_name, int current_line) {
    auto it = active_borrows_.find(lender_name);
    if (it == active_borrows_.end()) return;
    for (auto& br : it->second) {
        if (!br.ended && br.last_use_line >= 0 && br.last_use_line < current_line) {
            br.ended = true;
        }
    }
    /* Clean up fully ended entries */
    it->second.erase(
        std::remove_if(it->second.begin(), it->second.end(),
            [](const BorrowRecord& br) { return br.ended; }),
        it->second.end());
    if (it->second.empty()) {
        active_borrows_.erase(it);
    }
}

bool OwnershipTracker::has_active_borrows(const std::string& lender_name) const {
    auto it = active_borrows_.find(lender_name);
    if (it == active_borrows_.end()) return false;
    for (const auto& br : it->second)
        if (!br.ended) return true;
    return false;
}

bool OwnershipTracker::has_mut_borrow(const std::string& lender_name) const {
    auto it = active_borrows_.find(lender_name);
    if (it == active_borrows_.end()) return false;
    for (const auto& br : it->second)
        if (!br.ended && br.is_mutable) return true;
    return false;
}

void OwnershipTracker::end_all_borrows(const std::string& lender_name, int end_line) {
    auto it = active_borrows_.find(lender_name);
    if (it == active_borrows_.end()) return;
    for (auto& br : it->second) {
        br.ended = true;
        br.last_use_line = end_line;
    }
    active_borrows_.erase(it);
}

/* ════════════════════════════════════════════════════════════
   Query helpers
   ════════════════════════════════════════════════════════════ */

OwnershipState OwnershipTracker::state_of(const std::string& name) const {
    for (int i = static_cast<int>(scopes_.size()) - 1; i >= 0; i--) {
        const Scope& s = scopes_[i];
        auto it = s.vars.find(name);
        if (it != s.vars.end()) return it->second.state;
    }
    return OwnershipState::Owned;
}

bool OwnershipTracker::is_readable(const std::string& name) const {
    OwnershipState s = state_of(name);
    return s != OwnershipState::Moved;
}

bool OwnershipTracker::is_writable(const std::string& name) const {
    OwnershipState s = state_of(name);
    return s != OwnershipState::Moved &&
           s != OwnershipState::Borrowed &&
           s != OwnershipState::MutBorrowed;
}

/* ════════════════════════════════════════════════════════════
   NLL: record last use of a variable (for borrow end tracking)
   ════════════════════════════════════════════════════════════ */

void OwnershipTracker::record_use(const std::string& name, int ln) {
    /* Update last_use_line for any borrow where this var is the borrower */
    for (auto& [lender, borrows] : active_borrows_) {
        for (auto& br : borrows) {
            if (br.borrower == name && !br.ended) {
                br.last_use_line = ln;
            }
        }
    }
}

/* ════════════════════════════════════════════════════════════
   State transition helpers
   ════════════════════════════════════════════════════════════ */

void OwnershipTracker::assert_not_moved(const std::string& name, int ln) {
    VarInfo* v = lookup(name);
    if (!v) return;
    if (v->state == OwnershipState::Moved) {
        std::ostringstream msg;
        msg << "use of moved variable '" << name << "'";
        throw OwnershipError(msg.str(), name, ln);
    }
}

void OwnershipTracker::assert_readable(const std::string& name, int ln) {
    VarInfo* v = lookup(name);
    if (!v) return;
    if (v->state == OwnershipState::Moved) {
        std::ostringstream msg;
        msg << "use of moved variable '" << name
            << "' (ownership was transferred at line " << v->src_line << ")";
        throw OwnershipError(msg.str(), name, ln);
    }
    record_use(name, ln);
}

void OwnershipTracker::assert_writable(const std::string& name, int ln) {
    VarInfo* v = lookup(name);
    if (!v) return;
    if (v->state == OwnershipState::Moved) {
        std::ostringstream msg;
        msg << "cannot write to moved variable '" << name
            << "' (moved at line " << v->src_line << ")";
        throw OwnershipError(msg.str(), name, ln);
    }
    if (v->state == OwnershipState::Borrowed || v->state == OwnershipState::MutBorrowed) {
        std::ostringstream msg;
        msg << "cannot write to '" << name
            << "' because it is currently borrowed"
            << " (state=" << state_name(v->state) << ")";
        throw OwnershipError(msg.str(), name, ln);
    }
    /* MEDIUM 5 fix: auto-end borrows whose last use was before this line */
    try_end_borrows(name, ln);
    /* NLL: check active borrows */
    if (has_active_borrows(name)) {
        std::ostringstream msg;
        auto& borrows = active_borrows_.at(name);
        int still_active = 0;
        for (auto& br : borrows) if (!br.ended) still_active++;
        if (still_active > 0) {
            msg << "cannot write to '" << name
                << "' because it has " << still_active
                << " active borrow(s)";
            for (auto& br : borrows) {
                if (!br.ended) {
                    msg << " (borrow by '" << br.borrower
                        << "' at line " << br.start_line << ")";
                }
            }
            throw OwnershipError(msg.str(), name, ln);
        }
    }
}

void OwnershipTracker::check_mutable_borrow_allowed(const std::string& name, int ln) {
    auto it = active_borrows_.find(name);
    if (it != active_borrows_.end()) {
        for (const auto& br : it->second) {
            if (!br.ended) {
                std::ostringstream msg;
                msg << "cannot mutably borrow '" << name
                    << "' because it is already "
                    << (br.is_mutable ? "mutably" : "immutably")
                    << " borrowed by '" << br.borrower
                    << "' (at line " << br.start_line << ")";
                throw OwnershipError(msg.str(), name, ln);
            }
        }
    }
}

void OwnershipTracker::assign(const std::string& name, int ln,
                               OwnershipState new_state) {
    VarInfo* v = lookup(name);
    if (v) {
        assert_writable(name, ln);
        v->state = new_state;
        v->src_line = ln;
    } else {
        declare(name, new_state, ln);
    }
}

/* Update alias map after assignment — called from AST walker */
void OwnershipTracker::record_alias(const std::string& lhs, const std::string& rhs) {
    /* CRITICAL 2 fix: track aliases so moves can invalidate all aliases */
    alias_map_[lhs].insert(rhs);
    alias_map_[rhs].insert(lhs);
}

void OwnershipTracker::do_move(const std::string& name, int ln) {
    VarInfo* v = lookup(name);
    if (!v) {
        declare(name, OwnershipState::Moved, ln);
        return;
    }
    if (v->state == OwnershipState::Moved) {
        std::ostringstream msg;
        msg << "cannot move '" << name
            << "' — already moved at line " << v->src_line;
        throw OwnershipError(msg.str(), name, ln);
    }
    if (v->state == OwnershipState::Borrowed || v->state == OwnershipState::MutBorrowed) {
        std::ostringstream msg;
        msg << "cannot move '" << name
            << "' while it is borrowed"
            << " (borrow at line " << v->src_line << ")";
        throw OwnershipError(msg.str(), name, ln);
    }
    /* MEDIUM 5 fix: auto-end borrows whose last use was before this line */
    try_end_borrows(name, ln);
    /* NLL: check no active borrows */
    if (has_active_borrows(name)) {
        std::ostringstream msg;
        msg << "cannot move '" << name
            << "' while it has active borrows";
        throw OwnershipError(msg.str(), name, ln);
    }
    v->state    = OwnershipState::Moved;
    v->src_line = ln;
    end_all_borrows(name, ln);

    /* CRITICAL 2 fix: Invalidate all aliases of the moved variable */
    auto alias_it = alias_map_.find(name);
    if (alias_it != alias_map_.end()) {
        for (const std::string& alias : alias_it->second) {
            VarInfo* alias_v = lookup(alias);
            if (alias_v && alias_v->state != OwnershipState::Moved) {
                alias_v->state = OwnershipState::Moved;
                alias_v->src_line = ln;
                end_all_borrows(alias, ln);
            }
        }
    }
}

void OwnershipTracker::do_shared(const std::string& name, int ln) {
    VarInfo* v = lookup(name);
    if (!v) { declare(name, OwnershipState::Shared, ln); return; }
    assert_readable(name, ln);
    /* Cannot create shared ref while mutably borrowed */
    if (has_mut_borrow(name)) {
        std::ostringstream msg;
        msg << "cannot share '" << name
            << "' while it is mutably borrowed";
        throw OwnershipError(msg.str(), name, ln);
    }
    v->state    = OwnershipState::Shared;
    v->src_line = ln;
}

void OwnershipTracker::do_weak(const std::string& name, int ln) {
    VarInfo* v = lookup(name);
    if (!v) { declare(name, OwnershipState::Weak, ln); return; }
    assert_readable(name, ln);
    v->state    = OwnershipState::Weak;
    v->src_line = ln;
}

void OwnershipTracker::do_borrow(const std::string& name, int ln, bool is_mutable) {
    VarInfo* v = lookup(name);
    if (!v) { declare(name,
        is_mutable ? OwnershipState::MutBorrowed : OwnershipState::Borrowed, ln);
        return;
    }
    if (v->state == OwnershipState::Moved) {
        std::ostringstream msg;
        msg << "cannot borrow moved variable '" << name
            << "' (moved at line " << v->src_line << ")";
        throw OwnershipError(msg.str(), name, ln);
    }

    if (is_mutable) {
        check_mutable_borrow_allowed(name, ln);
    } else {
        /* Immutable borrow disallowed if there's an active mutable borrow */
        if (has_mut_borrow(name)) {
            std::ostringstream msg;
            msg << "cannot immutably borrow '" << name
                << "' because it is already mutably borrowed";
            throw OwnershipError(msg.str(), name, ln);
        }
    }

    BorrowRecord br;
    br.borrower = "(temp)"; /* will be set when the borrow is assigned to a var */
    br.lender = name;
    br.is_mutable = is_mutable;
    br.start_line = ln;
    active_borrows_[name].push_back(br);

    v->src_line = ln;
}

void OwnershipTracker::do_reborrow(const std::string& borrower,
                                    const std::string& new_borrower,
                                    int ln, bool is_mutable) {
    /* Reborrow: new_borrower borrows from borrower, who already has a borrow */
    /* Update the existing borrow record's borrower name if it was temp */
    for (auto& [lender, borrows] : active_borrows_) {
        for (auto& br : borrows) {
            if (br.borrower == "(temp)" && br.start_line == ln) {
                br.borrower = new_borrower;
                return;
            }
        }
    }
    /* If not found, create a new borrow from the borrower variable's lender */
    for (auto& [lender, borrows] : active_borrows_) {
        for (auto& br : borrows) {
            if (br.borrower == borrower && !br.ended) {
                do_borrow(lender, ln, is_mutable && br.is_mutable);
                /* Update the borrower name */
                for (auto& [l, bs] : active_borrows_) {
                    for (auto& b : bs) {
                        if (b.borrower == "(temp)" && b.start_line == ln) {
                            b.borrower = new_borrower;
                            return;
                        }
                    }
                }
                return;
            }
        }
    }
    /* Fall back: direct borrow */
    do_borrow(borrower, ln, is_mutable);
}

void OwnershipTracker::do_drop(const std::string& name, int ln) {
    VarInfo* v = lookup(name);
    if (!v) return;
    if (v->state == OwnershipState::Moved) {
        std::ostringstream msg;
        msg << "cannot drop '" << name
            << "' — already moved at line " << v->src_line;
        throw OwnershipError(msg.str(), name, ln);
    }
    /* MEDIUM 5 fix: auto-end borrows whose last use was before this line */
    try_end_borrows(name, ln);
    /* NLL: check no active borrows */
    if (has_active_borrows(name)) {
        auto& borrows = active_borrows_.at(name);
        int still_active = 0;
        for (auto& br : borrows) if (!br.ended) still_active++;
        if (still_active > 0) {
            std::ostringstream msg;
            msg << "cannot drop '" << name
                << "' while it has " << still_active << " active borrow(s)";
            throw OwnershipError(msg.str(), name, ln);
        }
    }

    for (int i = static_cast<int>(scopes_.size()) - 1; i >= 0; i--) {
        auto it = scopes_[i].vars.find(name);
        if (it != scopes_[i].vars.end()) {
            scopes_[i].vars.erase(it);
            end_all_borrows(name, ln);
            lifetime_map_.erase(name);
            return;
        }
    }
}

/* ════════════════════════════════════════════════════════════
   Return reference validation
   ════════════════════════════════════════════════════════════ */

void OwnershipTracker::validate_return(const ASTNode* node) {
    if (!node || !node->left) return;

    /* HIGH 4 fix: actually emit diagnostics for returning borrows of locals */
    const ASTNode* ret_val = node->left.get();
    if (ret_val->type == NodeType::Var) {
        VarInfo* v = lookup(ret_val->value);
        if (v && (v->state == OwnershipState::Borrowed ||
                  v->state == OwnershipState::MutBorrowed)) {
            /* The borrow originates from somewhere - check if it's a local */
            auto it = active_borrows_.find(ret_val->value);
            if (it != active_borrows_.end()) {
                for (const auto& br : it->second) {
                    if (!br.ended) {
                        VarInfo* lender = lookup(br.lender);
                        if (lender) {
                            /* Check if lender is a function parameter
                               (we allow returning references to params/statics) */
                            bool is_param = false;
                            /* Check if the lender is a parameter (lives as long as function) */
                            for (size_t i = 0; i < scopes_.size(); i++) {
                                auto* pv = scopes_[i].find(br.lender);
                                if (pv) {
                                    /* Variables in the outermost parameter scope are params */
                                    if (i == 0) {
                                        is_param = true;
                                    }
                                    break;
                                }
                            }
                            if (!is_param) {
                                /* Returning a borrow of a local variable - this is an error */
                                std::ostringstream msg;
                                msg << "cannot return a borrow of local variable '"
                                    << br.lender << "' — the borrow's lifetime does not "
                                    << "outlive the function";
                                throw OwnershipError(msg.str(), ret_val->value, node->src_line);
                            }
                        }
                    }
                }
            }
        }
    }
}

/* ════════════════════════════════════════════════════════════
   Debug: dump active borrows
   ════════════════════════════════════════════════════════════ */

void OwnershipTracker::dump_borrows() const {
    std::cerr << "--- Active Borrows ---\n";
    for (const auto& [lender, borrows] : active_borrows_) {
        for (const auto& br : borrows) {
            if (!br.ended) {
                std::cerr << "  " << lender << " <- "
                          << (br.is_mutable ? "&mut " : "& ")
                          << br.borrower << " (line " << br.start_line << ")\n";
            }
        }
    }
    std::cerr << "----------------------\n";
}

/* ════════════════════════════════════════════════════════════
   AST Walker
   ════════════════════════════════════════════════════════════ */

void OwnershipTracker::analyse(const ASTNode* root) {
    push_scope();
    walk_block(root);
    pop_scope();
}

void OwnershipTracker::walk_block(const ASTNode* node) {
    while (node) {
        walk(node);
        node = node->next.get();
    }
}

void OwnershipTracker::walk(const ASTNode* node) {
    if (!node) return;

    switch (node->type) {

    /* ── Assign: x = expr ── */
    case NodeType::Assign: {
        walk(node->right.get());
        if (node->left && node->left->type == NodeType::Var) {
            const std::string& name = node->left->value;
            OwnershipState new_state = OwnershipState::Owned;

            if (node->right) {
                switch (node->right->type) {
                    case NodeType::Move:
                        new_state = OwnershipState::Owned;
                        if (node->right->left)
                            do_move(node->right->left->value, node->right->src_line);
                        break;
                    case NodeType::SharedRef: new_state = OwnershipState::Shared;   break;
                    case NodeType::WeakRef:   new_state = OwnershipState::Weak;     break;
                    case NodeType::Borrow:
                        new_state = OwnershipState::Borrowed;
                        do_borrow(node->right->value, node->right->src_line,
                                  node->right->is_mutable);
                        break;
                    default:                  new_state = OwnershipState::Owned;    break;
                }

                /* CRITICAL 2 fix: track aliases when y = x */
                if (node->right->type == NodeType::Var) {
                    record_alias(name, node->right->value);
                }
            }

            /* Handle reborrow: if RHS is a borrow of a borrow */
            if (node->right && node->right->type == NodeType::Borrow) {
                VarInfo* rhs_v = lookup(node->right->value);
                if (rhs_v && (rhs_v->state == OwnershipState::Borrowed ||
                              rhs_v->state == OwnershipState::MutBorrowed)) {
                    do_reborrow(node->right->value, name, node->src_line,
                                rhs_v->state == OwnershipState::MutBorrowed);
                } else {
                    do_borrow(node->right->value, node->src_line, false);
                    /* Update the temp borrower name */
                    for (auto& [lender, borrows] : active_borrows_) {
                        for (auto& br : borrows) {
                            if (br.borrower == "(temp)" && br.start_line == node->src_line) {
                                br.borrower = name;
                            }
                        }
                    }
                }
            }

            assign(name, node->src_line, new_state);
        }
        break;
    }

    case NodeType::Move: {
        do_move(node->value, node->src_line);
        break;
    }

    case NodeType::Drop: {
        do_drop(node->value, node->src_line);
        break;
    }

    case NodeType::Delete: {
        if (node->left)
            do_drop(node->left->value, node->src_line);
        break;
    }

    case NodeType::SharedRef: {
        do_shared(node->value, node->src_line);
        break;
    }

    case NodeType::WeakRef: {
        do_weak(node->value, node->src_line);
        break;
    }

    case NodeType::Borrow: {
        do_borrow(node->value, node->src_line, false);
        break;
    }

    /* ── Copy: copy x ── */
    case NodeType::Copy: {
        assert_readable(node->value, node->src_line);
        break;
    }

    /* ── Free: free x ── */
    case NodeType::Free: {
        VarInfo* v = lookup(node->value);
        if (v) {
            if (has_active_borrows(node->value)) {
                std::ostringstream msg;
                msg << "cannot free '" << node->value
                    << "' while it has active borrows";
                throw OwnershipError(msg.str(), node->value, node->src_line);
            }
        }
        break;
    }

    /* ── Variable read ── */
    case NodeType::Var: {
        assert_readable(node->value, node->src_line);
        break;
    }

    /* ── Function call ── */
    case NodeType::Call: {
        const ASTNode* arg = node->args.get();
        while (arg) {
            walk(arg);
            arg = arg->next.get();
        }
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

    case NodeType::Return: {
        walk(node->left.get());
        validate_return(node);
        break;
    }

    case NodeType::If: {
        walk(node->left.get());
        push_scope();
        walk_block(node->body.get());
        pop_scope();
        if (node->orelse) walk(node->orelse.get());
        break;
    }

    case NodeType::Else: {
        push_scope();
        walk_block(node->body.get());
        pop_scope();
        break;
    }

    case NodeType::While: {
        walk(node->left.get());
        push_scope();
        walk_block(node->body.get());
        pop_scope();
        break;
    }

    case NodeType::Loop:
    case NodeType::Repeat: {
        push_scope();
        walk_block(node->body.get());
        if (node->left) walk(node->left.get());
        pop_scope();
        break;
    }

    case NodeType::Match: {
        walk(node->left.get());
        const ASTNode* case_ptr = node->args.get();
        while (case_ptr) {
            push_scope();
            walk_block(case_ptr->body.get());
            pop_scope();
            case_ptr = case_ptr->next.get();
        }
        break;
    }

    case NodeType::For: {
        walk(node->left.get());
        push_scope();
        declare(node->value, OwnershipState::Owned, node->src_line);
        walk_block(node->body.get());
        pop_scope();
        break;
    }

    case NodeType::Function:
    case NodeType::Lambda: {
        push_scope();
        const ASTNode* param = node->args.get();
        while (param) {
            declare(param->value, OwnershipState::Owned, node->src_line);
            param = param->next.get();
        }
        walk_block(node->body.get());
        pop_scope();
        break;
    }

    case NodeType::PerformanceFn: {
        push_scope();
        const ASTNode* param = node->args.get();
        while (param) {
            declare(param->value, OwnershipState::Owned, node->src_line);
            param = param->next.get();
        }
        walk_block(node->body.get());
        pop_scope();
        break;
    }

    case NodeType::Class: {
        push_scope();
        walk_block(node->body.get());
        pop_scope();
        break;
    }

    case NodeType::Try: {
        push_scope();
        walk_block(node->body.get());
        pop_scope();
        if (node->orelse) {
            push_scope();
            walk_block(node->orelse.get());
            pop_scope();
        }
        break;
    }

    case NodeType::Throw: {
        walk(node->left.get());
        break;
    }

    case NodeType::Array: {
        const ASTNode* elem = node->args.get();
        while (elem) { walk(elem); elem = elem->next.get(); }
        break;
    }

    case NodeType::Index: {
        assert_readable(node->value, node->src_line);
        walk(node->left.get());
        break;
    }

    case NodeType::IndexAssign: {
        assert_writable(node->value, node->src_line);
        walk(node->left.get());
        walk(node->right.get());
        break;
    }

    case NodeType::Attribute: {
        walk(node->left.get());
        break;
    }

    case NodeType::New: {
        walk(node->args.get());
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
        break;

    default:
        walk(node->left.get());
        walk(node->right.get());
        walk_block(node->body.get());
        break;
    }
}