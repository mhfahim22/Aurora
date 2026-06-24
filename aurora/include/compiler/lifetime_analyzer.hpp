#pragma once
#include "compiler/ast.hpp"
#include <string>
#include <vector>
#include <unordered_map>

/* ════════════════════════════════════════════════════════════
   Aurora Phase 3 — Lifetime Analysis Engine
   ════════════════════════════════════════════════════════════

   Determines how long an object lives (its lifetime scope).

   Lifetime Scopes:
     Function    — lives within the function scope
     Block       — lives within a specific block (if/while/for)
     Loop        — lives within a loop iteration
     Temporary   — expression temporary (dies after expression)
     Global      — lives for entire program execution

   Why it matters:
     - Function lifetime → Stack or Arena allocation
     - Block lifetime   → Stack or Arena allocation
     - Loop lifetime    → Stack (per iteration) or Arena
     - Temporary        → Stack only
     - Global           → Heap allocation

   Region Grouping:
     Variables with the same lifetime scope can be grouped
     into regions for arena allocation.

   ════════════════════════════════════════════════════════════ */

/* ── Lifetime information for a single variable ── */
struct LifetimeInfo {
    std::string var_name;
    int         decl_line        { 0 };
    LifetimeScope scope          { LifetimeScope::Unknown };
    int         scope_depth      { 0 };   /* nesting depth */
    int         start_line       { 0 };   /* first use line */
    int         end_line         { 0 };   /* last use line */
    bool        is_loop_var      { false }; /* is this a loop variable? */
    bool        is_param         { false }; /* is this a function parameter? */

    /* Region grouping */
    int         region_id        { -1 };  /* which region this belongs to */
    bool        can_share_arena  { false }; /* can share arena with others? */
};

/* ── Region information for arena allocation ── */
struct RegionInfo {
    int         region_id;
    LifetimeScope scope_type;
    int         start_line      { 0 };
    int         end_line        { 0 };
    std::vector<std::string> var_names;
    int         total_size_est  { 0 };   /* estimated total size */
};

/* ── Scope information ── */
struct ScopeInfo3 {
    int         scope_id;
    int         parent_id       { -1 };
    LifetimeScope scope_type;
    int         depth           { 0 };
    int         start_line      { 0 };
    int         end_line        { 0 };
    std::vector<std::string> local_vars;
};

/* ════════════════════════════════════════════════════════════
   LifetimeAnalyzer — main lifetime analysis class
   ════════════════════════════════════════════════════════════ */
class LifetimeAnalyzer {
public:
    LifetimeAnalyzer() = default;

    /* ── Main entry point ── */
    void analyse(const ASTNode* root);

    /* ── Query results ── */
    LifetimeScope get_lifetime_scope(const std::string& var_name) const;
    bool is_function_lifetime(const std::string& var_name) const;
    bool is_block_lifetime(const std::string& var_name) const;
    bool is_loop_lifetime(const std::string& var_name) const;
    bool is_temporary(const std::string& var_name) const;

    /* ── Get analysis reports ── */
    const std::unordered_map<std::string, LifetimeInfo>& get_all_lifetime_info() const {
        return lifetime_info_;
    }
    const std::vector<RegionInfo>& get_regions() const {
        return regions_;
    }
    const std::vector<ScopeInfo3>& get_scopes() const {
        return scopes_;
    }

    /* ── Statistics ── */
    int count_function_lifetime() const;
    int count_block_lifetime() const;
    int count_loop_lifetime() const;
    int count_temporary() const;
    int count_global_lifetime() const;
    int count_regions() const;

    /* ── Print report ── */
    void print_lifetime_report() const;

private:
    /* ── Lifetime info per variable ── */
    std::unordered_map<std::string, LifetimeInfo> lifetime_info_;

    /* ── Regions for arena allocation ── */
    std::vector<RegionInfo> regions_;

    /* ── Scope stack ── */
    std::vector<ScopeInfo3> scopes_;
    int next_scope_id_ { 0 };
    int current_depth_ { 0 };

    /* ── Current function context ── */
    std::string current_func_name_;
    int         current_func_line_ { 0 };
    bool        in_loop_ { false };
    int         loop_depth_ { 0 };

    /* ── AST Walker ── */
    void walk(const ASTNode* node);
    void walk_block(const ASTNode* node);

    /* ── Lifetime detection methods ── */
    void detect_scope_lifetime(const std::string& name, int line);
    void detect_function_lifetime(const std::string& name, int line);
    void detect_loop_lifetime(const std::string& name, int line);
    void detect_temporary_lifetime(const std::string& name, int line);
    void detect_global_lifetime(const std::string& name, int line);

    /* ── Region grouping ── */
    void create_region(LifetimeScope scope_type, int start_line);
    void add_to_region(const std::string& var_name);
    void finalize_region();
    void unify_regions();
    int  find_or_create_region(LifetimeScope scope_type, int line);

    /* ── Scope management ── */
    void push_scope(LifetimeScope scope_type, int line);
    void pop_scope(int line);
    void add_local_var(const std::string& name);

    /* ── Helper methods ── */
    void record_variable(const std::string& name, LifetimeScope scope,
                         int line, bool is_loop = false, bool is_param = false);
    LifetimeScope determine_lifetime(const std::string& name) const;
    void update_last_use(const std::string& name, int line);
};
