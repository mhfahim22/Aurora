#pragma once
#include "compiler/ast.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

/* ════════════════════════════════════════════════════════════
   Aurora Phase 4 — Ownership & Alias Analysis Engine
   ════════════════════════════════════════════════════════════

   Determines ownership semantics and alias relationships.

   Ownership Types:
     Single    — exactly one owner (can use RAII/Stack)
     Shared    — multiple owners (need ARC)
     Weak      — non-owning reference (need weak pointer)
     Borrowed  — temporary immutable reference

   Borrow Types:
     Mutable     — can modify through borrow (exclusive)
     Immutable   — cannot modify through borrow (shared)

   Alias Graph:
     Tracks which variables point to the same data.
     Helps determine if sharing is needed.

   Why it matters:
     - Single ownership → Stack/RAII (fastest)
     - Shared ownership → ARC (reference counting overhead)
     - Weak references  → need lock before access
     - Borrows          → no ownership transfer

   ════════════════════════════════════════════════════════════ */

/* ── Ownership information for a single variable ── */
struct OwnershipInfo {
    std::string var_name;
    int         decl_line        { 0 };
    OwnershipType type           { OwnershipType::Single };

    /* Ownership tracking */
    int         owner_count      { 1 };   /* number of owners */
    bool        is_mutable       { true }; /* can be mutated? */
    bool        has_mutable_borrow { false }; /* currently borrowed mutably? */
    int         immutable_borrow_count { 0 }; /* number of immutable borrows */

    /* Reference counting (for ARC) */
    int         strong_ref_count { 0 };   /* strong references */
    int         weak_ref_count   { 0 };   /* weak references */

    /* Alias tracking */
    std::vector<std::string> aliases;     /* variables pointing to same data */
    std::string borrowed_from;            /* if this is a borrow, from which var */
};

/* ── Borrow information ── */
struct BorrowInfo {
    std::string borrower;      /* who is borrowing */
    std::string lender;        /* who is being borrowed from */
    bool        is_mutable;    /* mutable or immutable borrow? */
    int         start_line;    /* where borrow starts */
    int         end_line;      /* where borrow ends */
};

/* ── Alias edge in the alias graph ── */
struct AliasEdge {
    std::string from;
    std::string to;
    bool        is_strong;     /* strong or weak reference? */
    int         line;
};

/* ════════════════════════════════════════════════════════════
   OwnershipAnalyzer — main ownership analysis class
   ════════════════════════════════════════════════════════════ */
class OwnershipAnalyzer {
public:
    OwnershipAnalyzer() = default;

    /* ── Main entry point ── */
    void analyse(const ASTNode* root);

    /* ── Query results ── */
    OwnershipType get_ownership_type(const std::string& var_name) const;
    bool is_single_owner(const std::string& var_name) const;
    bool is_shared(const std::string& var_name) const;
    bool is_borrowed(const std::string& var_name) const;
    bool has_aliases(const std::string& var_name) const;

    /* ── Reference cycle detection ── */
    void detect_reference_cycles();
    bool is_in_reference_cycle(const std::string& var_name) const;
    const std::unordered_set<std::string>& get_cycle_variables() const {
        return cycle_vars_;
    }

    /* ── Get analysis reports ── */
    const std::unordered_map<std::string, OwnershipInfo>& get_all_ownership_info() const {
        return ownership_info_;
    }
    const std::vector<BorrowInfo>& get_borrows() const {
        return borrows_;
    }
    const std::vector<AliasEdge>& get_alias_edges() const {
        return alias_edges_;
    }

    /* ── Statistics ── */
    int count_single() const;
    int count_shared() const;
    int count_weak() const;
    int count_borrowed() const;
    int count_aliases() const;

    /* ── Print report ── */
    void print_ownership_report() const;
    void print_alias_graph() const;

private:
    /* ── Ownership info per variable (key = func::name) ── */
    std::unordered_map<std::string, OwnershipInfo> ownership_info_;

    /* ── Unscoped fallback map (backward compat for single-function programs) ── */
    /* TODO: remove ownership_info_unscoped_ fallback once all callers are
       updated to use scoped keys (func::name). */
    std::unordered_map<std::string, OwnershipInfo> ownership_info_unscoped_;

    /* ── Borrow tracking ── */
    std::vector<BorrowInfo> borrows_;

    /* ── Known consuming functions (CRITICAL 1 fix) ── */
    /* Maps function name → vector of argument indices that are consumed (moved) */
    std::unordered_map<std::string, std::vector<int>> consumed_args_map_;

    /* ── Alias graph edges ── */
    std::vector<AliasEdge> alias_edges_;

    /* ── Reference cycle detection results ── */
    std::unordered_set<std::string> cycle_vars_;

    /* ── Current function context ── */
    std::string current_func_name_;

    /* ── Scope a bare variable name with current function ── */
    std::string key(const std::string& raw_name) const {
        return current_func_name_.empty()
            ? raw_name
            : current_func_name_ + "::" + raw_name;
    }

    /* ── Scope tracking ── */
    struct Scope {
        std::unordered_set<std::string> local_vars;
        std::string func_name;
    };
    std::vector<Scope> scope_stack_;

    /* ── AST Walker ── */
    void walk(const ASTNode* node);
    void walk_block(const ASTNode* node);

    /* ── Ownership detection methods ── */
    void detect_single_owner(const std::string& name, int line);
    void detect_shared_owner(const std::string& name, int line);
    void detect_mutable_borrow(const std::string& borrower,
                               const std::string& lender, int line);
    void detect_immutable_borrow(const std::string& borrower,
                                 const std::string& lender, int line);
    void detect_move(const std::string& name, int line);
    void detect_copy(const std::string& name, int line);

    /* ── Alias graph methods ── */
    void add_alias_edge(const std::string& from, const std::string& to,
                        bool is_strong, int line);
    void merge_aliases(const std::string& a, const std::string& b);
    void propagate_ownership(const std::string& name);

    /* ── Helper methods ── */
    void record_variable(const std::string& name, OwnershipType type,
                         int line, bool is_mutable = true);
    void add_local_var(const std::string& name);
    void push_scope(const std::string& func_name = "");
    void pop_scope();
    bool is_local_var(const std::string& name) const;
};
