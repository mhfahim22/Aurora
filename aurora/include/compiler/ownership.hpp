#pragma once
#include "compiler/ast.hpp"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <sstream>

/* ════════════════════════════════════════════════════════════
   Aurora Phase 2 — Ownership Tracker (Rust-level)
   ════════════════════════════════════════════════════════════

   Rust-style ownership with NLL (Non-Lexical Lifetimes):
   - Each variable has exactly one owner at any time
   - Borrows follow XOR rule: one mutable XOR many immutable
   - Borrows end at last use (not at scope exit)
   - References cannot outlive their referent
   ════════════════════════════════════════════════════════════ */

enum class OwnershipState {
    Owned,
    Moved,
    Shared,
    Weak,
    Borrowed,
    MutBorrowed,
};

struct VarInfo {
    OwnershipState state    { OwnershipState::Owned };
    int            src_line { 0 };
    bool           is_mut   { true };
    int            lifetime_id { -1 };
};

inline const char* state_name(OwnershipState s) {
    switch (s) {
        case OwnershipState::Owned:       return "Owned";
        case OwnershipState::Moved:       return "Moved";
        case OwnershipState::Shared:      return "Shared";
        case OwnershipState::Weak:        return "Weak";
        case OwnershipState::Borrowed:    return "&";
        case OwnershipState::MutBorrowed: return "&mut";
    }
    return "Unknown";
}

struct BorrowRecord {
    std::string borrower;
    std::string lender;
    bool        is_mutable;
    int         start_line;
    int         last_use_line { -1 };
    bool        ended        { false };
};

struct OwnershipError : std::runtime_error {
    int         line;
    std::string var;

    OwnershipError(const std::string& msg, const std::string& var_name, int ln)
        : std::runtime_error(msg), line(ln), var(var_name) {}
};

struct Scope {
    std::unordered_map<std::string, VarInfo> vars;
    int scope_id { 0 };
    int parent_id { -1 };

    VarInfo* find(const std::string& name) {
        auto it = vars.find(name);
        return (it != vars.end()) ? &it->second : nullptr;
    }

    std::vector<std::string> owned_vars() const {
        std::vector<std::string> result;
        for (auto& [name, info] : vars)
            if (info.state == OwnershipState::Owned)
                result.push_back(name);
        return result;
    }
};

class OwnershipTracker {
public:
    void analyse(const ASTNode* root);

    /* ── Query helpers (used by codegen) ── */
    OwnershipState state_of(const std::string& name) const;
    bool is_readable(const std::string& name) const;
    bool is_writable(const std::string& name) const;

    /* Get the set of variables that are currently borrowed */
    const std::unordered_map<std::string, std::vector<BorrowRecord>>&
        active_borrows() const { return active_borrows_; }

    /* Debug: print all active borrows */
    void dump_borrows() const;

private:
    std::vector<Scope> scopes_;
    int next_scope_id_ { 1 };
    int next_lifetime_id_ { 1 };

    std::unordered_map<std::string, std::vector<BorrowRecord>> active_borrows_;
    std::unordered_map<std::string, int> lifetime_map_;

    /* ── Alias tracking for CRITICAL 2 fix ── */
    std::unordered_map<std::string, std::unordered_set<std::string>> alias_map_;

    /* ── NLL borrow end tracking for MEDIUM 5 fix ── */
    void try_end_borrows(const std::string& lender_name, int current_line);

    /* ── Scope management ── */
    void push_scope();
    std::vector<std::string> pop_scope();
    void declare(const std::string& name, OwnershipState state, int ln);
    VarInfo* lookup(const std::string& name);
    int current_scope_id() const;

    /* ── Borrow tracking ── */
    void end_borrows_for(const std::string& borrower_name);
    bool has_active_borrows(const std::string& lender_name) const;
    bool has_mut_borrow(const std::string& lender_name) const;
    void end_all_borrows(const std::string& lender_name, int end_line);

    /* ── State transitions ── */
    void do_move(const std::string& name, int ln);
    void do_shared(const std::string& name, int ln);
    void do_weak(const std::string& name, int ln);
    void do_borrow(const std::string& name, int ln, bool is_mutable = false);
    void do_reborrow(const std::string& borrower, const std::string& new_borrower,
                     int ln, bool is_mutable);
    void do_drop(const std::string& name, int ln);
    void assign(const std::string& name, int ln, OwnershipState new_state);
    void record_alias(const std::string& lhs, const std::string& rhs);

    void check_mutable_borrow_allowed(const std::string& name, int ln);
    void assert_readable(const std::string& name, int ln);
    void assert_writable(const std::string& name, int ln);
    void assert_not_moved(const std::string& name, int ln);

    /* ── NLL: record last use of a borrow ── */
    void record_use(const std::string& name, int ln);

    /* ── Return reference validation ── */
    void validate_return(const ASTNode* node);

    /* ── AST walker ── */
    void walk(const ASTNode* node);
    void walk_block(const ASTNode* node);
};
