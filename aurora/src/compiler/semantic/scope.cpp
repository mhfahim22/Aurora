// scope.cpp — Scope tracking for memory analysis
// Part of the Aurora compiler pipeline — Phase 1

#include "compiler/ast.hpp"
#include <unordered_map>
#include <vector>
#include <string>

/* ════════════════════════════════════════════════════════════
   ScopeInfo — tracks variables in a single scope
   ════════════════════════════════════════════════════════════ */
struct ScopeInfo {
    std::unordered_map<std::string, MemoryMetadata> vars;
    int scope_id { 0 };
    int parent_scope_id { -1 };

    /* Check if a variable exists in this scope */
    bool has_var(const std::string& name) const {
        return vars.find(name) != vars.end();
    }

    /* Add a variable to this scope */
    void add_var(const std::string& name, const MemoryMetadata& meta) {
        vars[name] = meta;
    }

    /* Get variable metadata (returns nullptr if not found) */
    MemoryMetadata* get_var(const std::string& name) {
        auto it = vars.find(name);
        return (it != vars.end()) ? &it->second : nullptr;
    }
};

/* ════════════════════════════════════════════════════════════
   ScopeTracker — manages nested scopes
   ════════════════════════════════════════════════════════════ */
class ScopeTracker {
public:
    ScopeTracker() = default;

    /* Push a new scope */
    void push_scope() {
        int parent_id = scopes_.empty() ? -1 : scopes_.back().scope_id;
        ScopeInfo new_scope;
        new_scope.scope_id = next_scope_id_++;
        new_scope.parent_scope_id = parent_id;
        scopes_.push_back(std::move(new_scope));
    }

    /* Pop current scope and return its variables */
    ScopeInfo pop_scope() {
        if (scopes_.empty()) return ScopeInfo();
        ScopeInfo top = std::move(scopes_.back());
        scopes_.pop_back();
        return top;
    }

    /* Declare a variable in current scope */
    void declare(const std::string& name, const MemoryMetadata& meta) {
        if (scopes_.empty()) push_scope();
        scopes_.back().add_var(name, meta);
    }

    /* Look up a variable across all scopes (inner → outer) */
    MemoryMetadata* lookup(const std::string& name) {
        for (int i = (int)scopes_.size() - 1; i >= 0; i--) {
            auto* var = scopes_[i].get_var(name);
            if (var) return var;
        }
        return nullptr;
    }

    /* Get current scope depth */
    int depth() const { return (int)scopes_.size(); }

    /* Check if we're in a performance mode scope */
    bool in_performance_mode() const {
        return performance_mode_;
    }

    void set_performance_mode(bool val) { performance_mode_ = val; }

private:
    std::vector<ScopeInfo> scopes_;
    int next_scope_id_ { 0 };
    bool performance_mode_ { false };
};
