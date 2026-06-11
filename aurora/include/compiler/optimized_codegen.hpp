#pragma once
#include "compiler/ast.hpp"
#include "compiler/memory_analyzer.hpp"
#include "compiler/ownership_analyzer.hpp"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/BasicBlock.h>

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <utility>

/* ── Scope exit action types ── */
enum class ScopeExitActionType { DropGlue, HeapFree, GcUnregister, RefcountDec };

/* ════════════════════════════════════════════════════════════
    Aurora Phase 6 — Optimized Code Generation
    ════════════════════════════════════════════════════════════

   Generates optimized LLVM IR based on allocation strategy
   decisions from Phase 5.

   Allocation Strategies:
     Stack    — alloca in entry block (fastest)
     Arena    — bump pointer allocation (fast)
     RAII     — alloca with destructor call at scope exit
     ARC      — alloca with refcount_inc/dec
     GC       — alloca with GC metadata

   ════════════════════════════════════════════════════════════ */

/* ── Allocation info for codegen ── */
struct AllocationInfo {
    AllocStrategy strategy;
    llvm::Value*   alloca_ptr { nullptr };
    llvm::Value*   arena_ptr  { nullptr };  /* for arena allocation */
    int            size       { 0 };
    bool           needs_destructor { false };
    bool           needs_refcount   { false };
};

/* ════════════════════════════════════════════════════════════
   OptimizedCodegen — main optimized code generation class
   ════════════════════════════════════════════════════════════ */
class OptimizedCodegen {
public:
    OptimizedCodegen(llvm::LLVMContext& ctx,
                     std::unique_ptr<llvm::Module>& module,
                     std::unique_ptr<llvm::IRBuilder<>>& builder);

    /* ── Main entry point ── */
    void generate(const ASTNode* root,
                  const MemoryAnalyzer& memory_analyzer);

    /* ── Get allocation info for a variable ── */
    const AllocationInfo& get_allocation(const std::string& name) const;

    /* ── Check if variable needs special handling ── */
    bool needs_arena_alloc(const std::string& name) const;
    bool needs_refcount(const std::string& name) const;
    bool needs_destructor(const std::string& name) const;

private:
    /* ── LLVM infrastructure ── */
    llvm::LLVMContext& ctx_;
    std::unique_ptr<llvm::Module>& module_;
    std::unique_ptr<llvm::IRBuilder<>>& builder_;

    /* ── Memory analyzer reference ── */
    const MemoryAnalyzer* memory_analyzer_ { nullptr };

    /* ── Allocation tracking ── */
    std::unordered_map<std::string, AllocationInfo> allocations_;

    /* ── Scope exit action tracking ── */
    std::vector<std::vector<std::pair<llvm::Value*, ScopeExitActionType>>> scope_exit_actions_;

    /* ── Runtime function declarations ── */
    llvm::Function* fn_arena_alloc_          { nullptr };
    llvm::Function* fn_refcount_inc_         { nullptr };
    llvm::Function* fn_refcount_dec_         { nullptr };
    llvm::Function* fn_drop_glue_            { nullptr };
    llvm::Function* fn_gc_register_root_     { nullptr };
    llvm::Function* fn_gc_unregister_root_   { nullptr };
    llvm::Function* fn_free_                 { nullptr };

    /* ── Array runtime ── */
    llvm::Function* fn_array_contains_int_ { nullptr };

    /* ── Async runtime ── */
    llvm::Function* fn_task_create_       { nullptr };
    llvm::Function* fn_task_destroy_      { nullptr };
    llvm::Function* fn_task_is_done_      { nullptr };
    llvm::Function* fn_task_get_result_   { nullptr };
    llvm::Function* fn_task_set_result_   { nullptr };
    llvm::Function* fn_spawn_             { nullptr };
    llvm::Function* fn_wait_              { nullptr };

    /* ── Setup methods ── */
    void declare_runtime_helpers();
    void declare_arena_allocator();

    /* ── Allocation generation methods ── */
    llvm::Value* gen_stack_alloc(const std::string& name,
                                 llvm::Type* ty, int size);
    llvm::Value* gen_arena_alloc(const std::string& name,
                                 llvm::Type* ty, int size);
    llvm::Value* gen_heap_alloc(const std::string& name,
                                llvm::Type* ty, int size);
    void gen_refcount_alloc(const std::string& name,
                           llvm::Value* ptr);
    void gen_destructor_insert(const std::string& name,
                               llvm::Value* ptr);
    llvm::Value* gen_gc_alloc(const std::string& name,
                              llvm::Type* ty, int size);

    /* ── AST Walker ── */
    void walk(const ASTNode* node);
    void walk_block(const ASTNode* node);

    /* ── Statement generators ── */
    void gen_assign(const ASTNode* node);
    void gen_return(const ASTNode* node);
    void gen_function(const ASTNode* node);

    /* ── Scope management ── */
    void push_scope();
    void pop_scope();
    void emit_scope_exit_cleanup();
    void add_scope_exit_action(llvm::Value* ptr, ScopeExitActionType type);

    /* ── Cleanup emission helpers ── */
    void emit_drop(llvm::Value* ptr);
    void emit_heap_free(llvm::Value* ptr);
    void emit_gc_unregister(llvm::Value* ptr);
    void emit_refcount_dec(llvm::Value* ptr);

    /* ── Helper methods ── */
    llvm::AllocaInst* create_entry_alloca(const std::string& name,
                                          llvm::Type* ty);
    llvm::Type* get_type_for_var(const std::string& name) const;
    int estimate_size(const std::string& name) const;
    llvm::PointerType* i8ptr_ty();
};
