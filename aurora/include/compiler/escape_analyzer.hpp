#pragma once
#include "compiler/ast.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

/* ════════════════════════════════════════════════════════════
   Aurora Phase 2 — Escape Analysis Engine
   ════════════════════════════════════════════════════════════

   Determines whether an object "escapes" its declaring scope.

   Escape Types:
     NoEscape       — Object stays within its declaring scope
     ArgEscape      — Object escapes through function arguments
     ReturnEscape   — Object escapes through return value
     GlobalEscape   — Object assigned to global/static storage
     ClosureEscape  — Object captured by a closure

   Why it matters:
     - NoEscape objects can be stack-allocated
     - ArgEscape objects need heap allocation
     - ReturnEscape objects need heap allocation
     - GlobalEscape objects need heap allocation
     - ClosureEscape objects need heap allocation

   ════════════════════════════════════════════════════════════ */

/* ── Escape information for a single variable ── */
struct EscapeInfo {
    std::string var_name;
    int         decl_line        { 0 };
    EscapeStatus status          { EscapeStatus::Unknown };
    bool        is_global        { false };
    bool        is_closure_param { false };
    size_t      ref_count        { 0 };  /* number of references */

    /* Where the variable escapes */
    struct EscapePoint {
        EscapeStatus type;
        int         line;
        std::string context;  /* function name, etc. */
    };
    std::vector<EscapePoint> escape_points;
};

/* ── Closure capture info ── */
struct ClosureCaptureInfo {
    std::string closure_name;
    int         closure_line    { 0 };
    std::vector<std::string> captured_vars;
};

/* ── Global variable info ── */
struct GlobalVarInfo {
    std::string var_name;
    int         decl_line { 0 };
    bool        is_mutable { true };
};

/* ════════════════════════════════════════════════════════════
   EscapeAnalyzer — main escape analysis class
   ════════════════════════════════════════════════════════════ */
class EscapeAnalyzer {
public:
    EscapeAnalyzer() = default;

    /* ── Main entry point ── */
    void analyse(const ASTNode* root);

    /* ── Query results ── */
    EscapeStatus get_escape_status(const std::string& var_name) const;
    bool does_escape(const std::string& var_name) const;
    const std::unordered_map<std::string, EscapeInfo>& get_all_escape_info() const {
        return escape_info_;
    }

    /* ── Get analysis reports ── */
    const std::vector<ClosureCaptureInfo>& get_closure_captures() const {
        return closure_captures_;
    }
    const std::vector<GlobalVarInfo>& get_global_vars() const {
        return global_vars_;
    }

    /* ── Statistics ── */
    int count_no_escape() const;
    int count_arg_escape() const;
    int count_return_escape() const;
    int count_global_escape() const;
    int count_closure_escape() const;

private:
    /* ── Escape info per variable ── */
    std::unordered_map<std::string, EscapeInfo> escape_info_;

    /* ── Closure captures ── */
    std::vector<ClosureCaptureInfo> closure_captures_;

    /* ── Global variables ── */
    std::vector<GlobalVarInfo> global_vars_;

    /* ── Current function context ── */
    std::string current_func_name_;
    bool        in_performance_mode_ { false };

    /* ── Scope tracking ── */
    struct Scope {
        std::unordered_set<std::string> local_vars;
        std::string func_name;
    };
    std::vector<Scope> scope_stack_;

    /* ── AST Walker ── */
    void walk(const ASTNode* node);
    void walk_block(const ASTNode* node);

    /* ── Escape detection methods ── */
    void detect_return_escape(const ASTNode* node);
    void detect_closure_capture(const ASTNode* node);
    void detect_global_assignment(const ASTNode* node);
    void detect_reference_escape(const ASTNode* node);
    void detect_argument_escape(const ASTNode* node);

    /* ── Recursive helper for closure capture (HIGH 6 fix) ── */
    void find_all_var_refs(const ASTNode* node, std::unordered_set<std::string>& out_vars) const;

    /* ── Helper methods ── */
    void record_escape(const std::string& var_name, EscapeStatus status,
                       int line, const std::string& context);
    void add_local_var(const std::string& name);
    void push_scope(const std::string& func_name = "");
    void pop_scope();

    /* ── Check if variable is in current scope ── */
    bool is_local_var(const std::string& name) const;
    bool is_global_var(const std::string& name) const;
};
