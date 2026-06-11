// optimized_codegen.cpp — Optimized Code Generation Implementation
// Part of the Aurora compiler pipeline — Phase 6

#include "compiler/optimized_codegen.hpp"
#include <sstream>
#include <iostream>

/* ════════════════════════════════════════════════════════════
   OptimizedCodegen — Constructor
   ════════════════════════════════════════════════════════════ */

OptimizedCodegen::OptimizedCodegen(llvm::LLVMContext& ctx,
                                   std::unique_ptr<llvm::Module>& module,
                                   std::unique_ptr<llvm::IRBuilder<>>& builder)
    : ctx_(ctx), module_(module), builder_(builder) {
}

/* ════════════════════════════════════════════════════════════
   Main Entry Point
   ════════════════════════════════════════════════════════════ */

void OptimizedCodegen::generate(const ASTNode* root,
                                const MemoryAnalyzer& memory_analyzer) {
    memory_analyzer_ = &memory_analyzer;

    /* Step 0: Set target info for LLVM optimization */
    module_->setTargetTriple("x86_64-pc-windows-msvc");
    module_->setDataLayout("e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128");

    /* Step 1: Declare runtime helpers */
    declare_runtime_helpers();
    declare_arena_allocator();

    /* Step 2: Pre-analyze allocations based on memory analyzer results */
    const auto& all_results = memory_analyzer.get_all_results();
    for (const auto& [func_name, result] : all_results) {
        for (const auto& [var_name, meta] : result.variables) {
            AllocationInfo alloc;
            alloc.strategy = meta.alloc_strategy;
            alloc.size = meta.size_estimate > 0 ? meta.size_estimate : 64;
            alloc.needs_destructor = (meta.alloc_strategy == AllocStrategy::RAII);
            alloc.needs_refcount = (meta.alloc_strategy == AllocStrategy::ARC);
            allocations_[var_name] = alloc;
        }
    }

    /* Step 3: Walk AST and generate optimized code with scope management */
    push_scope();
    walk_block(root);
    pop_scope();
}

/* ════════════════════════════════════════════════════════════
   Runtime Helper Declarations
   ════════════════════════════════════════════════════════════ */

void OptimizedCodegen::declare_runtime_helpers() {
    /* void drop_glue(i8* ptr) */
    fn_drop_glue_ = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(ctx_),
                               { llvm::PointerType::getUnqual(ctx_) }, false),
        llvm::Function::ExternalLinkage, "aurora_drop_glue", module_.get());

    /* void refcount_inc(i8* ptr) */
    fn_refcount_inc_ = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(ctx_),
                               { llvm::PointerType::getUnqual(ctx_) }, false),
        llvm::Function::ExternalLinkage, "aurora_refcount_inc", module_.get());

    /* void refcount_dec(i8* ptr) */
    fn_refcount_dec_ = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(ctx_),
                               { llvm::PointerType::getUnqual(ctx_) }, false),
        llvm::Function::ExternalLinkage, "aurora_refcount_dec", module_.get());

    /* ── Async runtime ── */
    auto* fnptr_ty = llvm::PointerType::getUnqual(ctx_);

    /* AuroraTask* aurora_task_create(i8* (*func)(i8*), i8* arg) */
    fn_task_create_ = llvm::Function::Create(
        llvm::FunctionType::get(fnptr_ty, { fnptr_ty, fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_task_create", module_.get());

    /* void aurora_task_destroy(AuroraTask*) */
    fn_task_destroy_ = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(ctx_), { fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_task_destroy", module_.get());

    /* i64 aurora_task_is_done(AuroraTask*) */
    fn_task_is_done_ = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getInt64Ty(ctx_), { fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_task_is_done", module_.get());

    /* i8* aurora_task_get_result(AuroraTask*) */
    fn_task_get_result_ = llvm::Function::Create(
        llvm::FunctionType::get(fnptr_ty, { fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_task_get_result", module_.get());

    /* void aurora_task_set_result(AuroraTask*, i8* result) */
    fn_task_set_result_ = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(ctx_), { fnptr_ty, fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_task_set_result", module_.get());

    /* void aurora_spawn(AuroraTask*) */
    fn_spawn_ = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(ctx_), { fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_spawn", module_.get());

    /*     void aurora_wait(AuroraTask*) */
    fn_wait_ = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(ctx_), { fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_wait", module_.get());

    /* i64 aurora_array_contains_int(i64 arr, i64 val) */
    fn_array_contains_int_ = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getInt64Ty(ctx_),
                               { llvm::Type::getInt64Ty(ctx_), llvm::Type::getInt64Ty(ctx_) }, false),
        llvm::Function::ExternalLinkage, "aurora_array_contains_int", module_.get());

    /* void aurora_free(i8* ptr) */
    fn_free_ = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(ctx_),
                               { llvm::PointerType::getUnqual(ctx_) }, false),
        llvm::Function::ExternalLinkage, "aurora_free", module_.get());

    /* void aurora_gc_register_root(i8* ptr) */
    fn_gc_register_root_ = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(ctx_),
                               { llvm::PointerType::getUnqual(ctx_) }, false),
        llvm::Function::ExternalLinkage, "aurora_gc_register_root", module_.get());

    /* void aurora_gc_unregister_root(i8* ptr) */
    fn_gc_unregister_root_ = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(ctx_),
                               { llvm::PointerType::getUnqual(ctx_) }, false),
        llvm::Function::ExternalLinkage, "aurora_gc_unregister_root", module_.get());
}

void OptimizedCodegen::declare_arena_allocator() {
    /* i8* arena_alloc(i64 size) */
    fn_arena_alloc_ = llvm::Function::Create(
        llvm::FunctionType::get(llvm::PointerType::getUnqual(ctx_),
                               { llvm::Type::getInt64Ty(ctx_) }, false),
        llvm::Function::ExternalLinkage, "aurora_arena_alloc", module_.get());
}

/* ════════════════════════════════════════════════════════════
   Allocation Generation Methods
   ════════════════════════════════════════════════════════════ */

llvm::Value* OptimizedCodegen::gen_stack_alloc(const std::string& name,
                                                llvm::Type* ty, int size) {
    /* Create alloca in entry block (standard LLVM pattern) */
    llvm::AllocaInst* alloca = create_entry_alloca(name, ty);

    AllocationInfo& alloc = allocations_[name];
    alloc.alloca_ptr = alloca;
    alloc.strategy = AllocStrategy::Stack;

    return alloca;
}

llvm::Value* OptimizedCodegen::gen_arena_alloc(const std::string& name,
                                                llvm::Type* ty, int size) {
    /* Call arena allocator */
    llvm::Value* size_val = llvm::ConstantInt::get(
        llvm::Type::getInt64Ty(ctx_), size);

    llvm::Value* ptr = builder_->CreateCall(fn_arena_alloc_, { size_val },
                                            name + "_arena");

    /* Store arena pointer in alloca */
    llvm::AllocaInst* alloca = create_entry_alloca(name,
        llvm::PointerType::getUnqual(ctx_));
    builder_->CreateStore(ptr, alloca);

    AllocationInfo& alloc = allocations_[name];
    alloc.alloca_ptr = alloca;
    alloc.arena_ptr = ptr;
    alloc.strategy = AllocStrategy::Arena;

    return alloca;
}

llvm::Value* OptimizedCodegen::gen_heap_alloc(const std::string& name,
                                               llvm::Type* ty, int size) {
    /* For now, use malloc-like allocation */
    /* In future, integrate with GC */
    llvm::Value* size_val = llvm::ConstantInt::get(
        llvm::Type::getInt64Ty(ctx_), size);

    /* Call aurora_malloc (to be implemented in runtime) */
    llvm::Function* fn_malloc = module_->getFunction("aurora_malloc");
    if (!fn_malloc) {
        auto* fty = llvm::FunctionType::get(
            llvm::PointerType::getUnqual(ctx_),
            { llvm::Type::getInt64Ty(ctx_) }, false);
        fn_malloc = llvm::Function::Create(fty, llvm::Function::ExternalLinkage,
                                           "aurora_malloc", module_.get());
    }

    llvm::Value* ptr = builder_->CreateCall(fn_malloc, { size_val },
                                            name + "_heap");

    /* Store heap pointer in alloca */
    llvm::AllocaInst* alloca = create_entry_alloca(name,
        llvm::PointerType::getUnqual(ctx_));
    builder_->CreateStore(ptr, alloca);

    AllocationInfo& alloc = allocations_[name];
    alloc.alloca_ptr = alloca;
    alloc.strategy = AllocStrategy::Heap;

    /* Record heap free at scope exit */
    add_scope_exit_action(alloca, ScopeExitActionType::HeapFree);

    return alloca;
}

void OptimizedCodegen::gen_refcount_alloc(const std::string& name,
                                           llvm::Value* ptr) {
    /* Increment reference count on allocation */
    llvm::Value* ptr_i8 = builder_->CreateBitCast(ptr, i8ptr_ty(), name + "_rc_inc");
    builder_->CreateCall(fn_refcount_inc_, { ptr_i8 });

    AllocationInfo& alloc = allocations_[name];
    alloc.needs_refcount = true;
    alloc.strategy = AllocStrategy::ARC;
    alloc.alloca_ptr = ptr;

    /* Record refcount decrement at scope exit */
    add_scope_exit_action(ptr, ScopeExitActionType::RefcountDec);
}

void OptimizedCodegen::gen_destructor_insert(const std::string& name,
                                              llvm::Value* ptr) {
    /* Record the destructor to be called at scope exit */
    AllocationInfo& alloc = allocations_[name];
    alloc.needs_destructor = true;
    alloc.strategy = AllocStrategy::RAII;
    alloc.alloca_ptr = ptr;
    add_scope_exit_action(ptr, ScopeExitActionType::DropGlue);
}

llvm::Value* OptimizedCodegen::gen_gc_alloc(const std::string& name,
                                              llvm::Type* ty, int size) {
    /* Allocate from heap via aurora_gc_alloc (tracks allocation for GC) */
    llvm::Value* size_val = llvm::ConstantInt::get(
        llvm::Type::getInt64Ty(ctx_), size);

    llvm::Function* fn_gc_alloc = module_->getFunction("aurora_gc_alloc");
    if (!fn_gc_alloc) {
        auto* fty = llvm::FunctionType::get(
            llvm::PointerType::getUnqual(ctx_),
            { llvm::Type::getInt64Ty(ctx_) }, false);
        fn_gc_alloc = llvm::Function::Create(fty, llvm::Function::ExternalLinkage,
                                             "aurora_gc_alloc", module_.get());
    }

    llvm::Value* ptr = builder_->CreateCall(fn_gc_alloc, { size_val },
                                            name + "_gc");

    /* Register pointer as GC root */
    llvm::Function* fn_reg = module_->getFunction("aurora_gc_register_root");
    if (!fn_reg) {
        auto* fty = llvm::FunctionType::get(
            llvm::Type::getVoidTy(ctx_),
            { llvm::PointerType::getUnqual(ctx_) }, false);
        fn_reg = llvm::Function::Create(fty, llvm::Function::ExternalLinkage,
                                        "aurora_gc_register_root", module_.get());
    }
    builder_->CreateCall(fn_reg, { ptr });

    /* Store GC-managed pointer in alloca */
    llvm::AllocaInst* alloca = create_entry_alloca(name,
        llvm::PointerType::getUnqual(ctx_));
    builder_->CreateStore(ptr, alloca);

    AllocationInfo& alloc = allocations_[name];
    alloc.alloca_ptr = alloca;
    alloc.strategy = AllocStrategy::GC;

    /* Record GC unregister at scope exit */
    add_scope_exit_action(alloca, ScopeExitActionType::GcUnregister);

    return alloca;
}

/* ════════════════════════════════════════════════════════════
   AST Walker
   ════════════════════════════════════════════════════════════ */

void OptimizedCodegen::walk_block(const ASTNode* node) {
    while (node) {
        walk(node);
        node = node->next.get();
    }
}

void OptimizedCodegen::walk(const ASTNode* node) {
    if (!node) return;

    switch (node->type) {

    /* ── Function definition ── */
    case NodeType::Function:
    case NodeType::PerformanceFn:
    case NodeType::Lambda: {
        gen_function(node);
        break;
    }

    /* ── Assignment ── */
    case NodeType::Assign: {
        gen_assign(node);
        break;
    }

    /* ── Return ── */
    case NodeType::Return: {
        gen_return(node);
        break;
    }

    /* ── If / Else ── */
    case NodeType::If: {
        walk(node->left.get());   /* condition */
        push_scope();
        walk_block(node->body.get());
        pop_scope();
        if (node->orelse) {
            push_scope();
            if (node->orelse->type == NodeType::If) {
                walk(node->orelse.get());
            } else {
                walk_block(node->orelse->body.get());
            }
            pop_scope();
        }
        break;
    }
    case NodeType::Else: {
        push_scope();
        walk_block(node->body.get());
        pop_scope();
        break;
    }

    /* ── While / Loop / For / Match ── */
    case NodeType::While: {
        walk(node->left.get());   /* condition */
        push_scope();
        walk_block(node->body.get());
        pop_scope();
        break;
    }
    case NodeType::Loop:
    case NodeType::Repeat: {
        push_scope();
        walk_block(node->body.get());
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
        walk(node->left.get());   /* iterable */
        push_scope();
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
        push_scope();
        walk_block(node->body.get());
        pop_scope();
        break;
    }

    /* ── Try / Catch ── */
    case NodeType::Try: {
        push_scope();
        walk_block(node->body.get());
        pop_scope();
        if (node->orelse) {
            push_scope();
            walk_block(node->orelse.get());
            pop_scope();
        }
        if (node->right) {
            push_scope();
            walk_block(node->right->body.get());
            pop_scope();
        }
        break;
    }

    /* ── Memory management nodes ── */
    case NodeType::Move:
    case NodeType::Drop:
    case NodeType::SharedRef:
    case NodeType::WeakRef:
    case NodeType::Borrow:
    case NodeType::Copy:
    case NodeType::Free: {
        /* Handled by existing codegen */
        break;
    }

    /* ── Leaf nodes ── */
    case NodeType::Num:
    case NodeType::Float:
    case NodeType::Str:
    case NodeType::Var:
    case NodeType::StructLiteral:
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
    case NodeType::Call:
    case NodeType::Spawn:
    case NodeType::Wait:
    case NodeType::Async:
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
   Statement Generators
   ════════════════════════════════════════════════════════════ */

void OptimizedCodegen::gen_assign(const ASTNode* node) {
    if (!node->left || !node->right) return;

    const std::string& name = node->left->value;

    /* Check for forced allocation strategy from attribute */
    AllocStrategy forced = node->memory_meta.forced_strategy;
    if (forced == AllocStrategy::ForcedGC) {
        gen_gc_alloc(name, llvm::Type::getInt64Ty(ctx_), 64);
        return;
    }
    if (forced == AllocStrategy::ForcedStack) {
        gen_stack_alloc(name, llvm::Type::getInt64Ty(ctx_), 64);
        return;
    }
    if (forced == AllocStrategy::ForcedArena) {
        gen_arena_alloc(name, llvm::Type::getInt64Ty(ctx_), 64);
        return;
    }
    if (forced == AllocStrategy::ForcedRAII) {
        auto* alloca = gen_stack_alloc(name, llvm::Type::getInt64Ty(ctx_), 64);
        gen_destructor_insert(name, alloca);
        return;
    }
    if (forced == AllocStrategy::ForcedARC) {
        auto* alloca = gen_stack_alloc(name, llvm::Type::getInt64Ty(ctx_), 64);
        gen_refcount_alloc(name, alloca);
        return;
    }

    /* Check if we have allocation info from memory analyzer */
    auto it = allocations_.find(name);
    if (it != allocations_.end()) {
        AllocationInfo& alloc = it->second;

        /* Generate allocation based on strategy */
        switch (alloc.strategy) {
            case AllocStrategy::Stack:
                /* Stack allocation - already handled by alloca */
                break;

            case AllocStrategy::Arena:
                /* Arena allocation */
                gen_arena_alloc(name, llvm::Type::getInt64Ty(ctx_), alloc.size);
                break;

            case AllocStrategy::Heap:
                /* Heap allocation */
                gen_heap_alloc(name, llvm::Type::getInt64Ty(ctx_), alloc.size);
                break;

            case AllocStrategy::RAII: {
                /* RAII - stack with destructor */
                auto* alloca = gen_stack_alloc(name, llvm::Type::getInt64Ty(ctx_), alloc.size);
                gen_destructor_insert(name, alloca);
                break;
            }

            case AllocStrategy::ARC: {
                /* ARC - reference counted */
                auto* alloca = gen_stack_alloc(name, llvm::Type::getInt64Ty(ctx_), alloc.size);
                gen_refcount_alloc(name, alloca);
                break;
            }

            case AllocStrategy::GC:
                /* GC - heap allocation with GC root tracking */
                gen_gc_alloc(name, llvm::Type::getInt64Ty(ctx_), alloc.size);
                break;

            default:
                break;
        }
    }
}

void OptimizedCodegen::gen_return(const ASTNode* node) {
    if (!node->left) return;

    /* If returning a variable with refcount, decrement */
    if (node->left->type == NodeType::Var) {
        const std::string& name = node->left->value;
        auto it = allocations_.find(name);
        if (it != allocations_.end() && it->second.needs_refcount) {
            /* Don't decrement on return - ownership is transferred */
        }
    }
}

void OptimizedCodegen::gen_function(const ASTNode* node) {
    /* This is a placeholder - actual function generation
       is handled by the existing codegen */
}

/* ════════════════════════════════════════════════════════════
   Helper Methods
   ════════════════════════════════════════════════════════════ */

llvm::AllocaInst* OptimizedCodegen::create_entry_alloca(const std::string& name,
                                                         llvm::Type* ty) {
    /* Create alloca in entry block of current function — prefer current BB */
    llvm::Function* func = nullptr;
    if (builder_->GetInsertBlock())
        func = builder_->GetInsertBlock()->getParent();
    if (!func)
        func = module_->getFunction("main");
    if (!func)
        return nullptr;

    llvm::IRBuilder<> temp_builder(&func->getEntryBlock());
    return temp_builder.CreateAlloca(ty, nullptr, name);
}

llvm::Type* OptimizedCodegen::get_type_for_var(const std::string& name) const {
    /* Default to i64 for now */
    return llvm::Type::getInt64Ty(ctx_);
}

int OptimizedCodegen::estimate_size(const std::string& name) const {
    auto it = allocations_.find(name);
    return (it != allocations_.end()) ? it->second.size : 64;
}

llvm::PointerType* OptimizedCodegen::i8ptr_ty() {
    return llvm::PointerType::getUnqual(ctx_);
}

/* ════════════════════════════════════════════════════════════
    Scope Management
    ════════════════════════════════════════════════════════════ */

void OptimizedCodegen::push_scope() {
    scope_exit_actions_.emplace_back();
}

void OptimizedCodegen::pop_scope() {
    if (scope_exit_actions_.empty()) return;
    emit_scope_exit_cleanup();
    scope_exit_actions_.pop_back();
}

void OptimizedCodegen::emit_scope_exit_cleanup() {
    if (scope_exit_actions_.empty()) return;
    auto* bb = builder_->GetInsertBlock();
    if (!bb || bb->getTerminator()) return;

    auto& actions = scope_exit_actions_.back();
    if (actions.empty()) return;

    /* Emit in REVERSE order (LIFO) */
    for (auto it = actions.rbegin(); it != actions.rend(); ++it) {
        bb = builder_->GetInsertBlock();
        if (!bb || bb->getTerminator()) break;

        llvm::Value* alloca_ptr = it->first;
        if (!alloca_ptr) continue;

        /* Load the current value from the alloca for cleanup */
        llvm::Value* val = builder_->CreateLoad(i8ptr_ty(), alloca_ptr, "scope_cleanup");

        switch (it->second) {
            case ScopeExitActionType::DropGlue:
                emit_drop(val);
                break;
            case ScopeExitActionType::HeapFree:
                emit_heap_free(val);
                break;
            case ScopeExitActionType::GcUnregister:
                emit_gc_unregister(val);
                break;
            case ScopeExitActionType::RefcountDec:
                emit_refcount_dec(val);
                break;
        }
    }
}

void OptimizedCodegen::add_scope_exit_action(llvm::Value* ptr, ScopeExitActionType type) {
    if (scope_exit_actions_.empty()) {
        scope_exit_actions_.emplace_back();
    }
    scope_exit_actions_.back().emplace_back(ptr, type);
}

void OptimizedCodegen::emit_drop(llvm::Value* ptr) {
    if (!ptr || !fn_drop_glue_) return;
    llvm::Value* ptr_i8 = builder_->CreateBitCast(ptr, i8ptr_ty(), "drop_ptr");
    builder_->CreateCall(fn_drop_glue_, { ptr_i8 });
}

void OptimizedCodegen::emit_heap_free(llvm::Value* ptr) {
    if (!ptr) return;
    /* Lazily get or declare aurora_free */
    llvm::Function* fn_free = module_->getFunction("aurora_free");
    if (!fn_free) {
        auto* fty = llvm::FunctionType::get(
            llvm::Type::getVoidTy(ctx_), { i8ptr_ty() }, false);
        fn_free = llvm::Function::Create(fty, llvm::Function::ExternalLinkage,
                                         "aurora_free", module_.get());
    }
    llvm::Value* ptr_i8 = builder_->CreateBitCast(ptr, i8ptr_ty(), "free_ptr");
    builder_->CreateCall(fn_free, { ptr_i8 });
}

void OptimizedCodegen::emit_gc_unregister(llvm::Value* ptr) {
    if (!ptr) return;
    llvm::Function* fn_unreg = module_->getFunction("aurora_gc_unregister_root");
    if (!fn_unreg) {
        auto* fty = llvm::FunctionType::get(
            llvm::Type::getVoidTy(ctx_), { i8ptr_ty() }, false);
        fn_unreg = llvm::Function::Create(fty, llvm::Function::ExternalLinkage,
                                          "aurora_gc_unregister_root", module_.get());
    }
    llvm::Value* ptr_i8 = builder_->CreateBitCast(ptr, i8ptr_ty(), "gc_unreg_ptr");
    builder_->CreateCall(fn_unreg, { ptr_i8 });
}

void OptimizedCodegen::emit_refcount_dec(llvm::Value* ptr) {
    if (!ptr || !fn_refcount_dec_) return;
    llvm::Value* ptr_i8 = builder_->CreateBitCast(ptr, i8ptr_ty(), "rcdec_ptr");
    builder_->CreateCall(fn_refcount_dec_, { ptr_i8 });
}

/* ════════════════════════════════════════════════════════════
    Query Methods
    ════════════════════════════════════════════════════════════ */

const AllocationInfo& OptimizedCodegen::get_allocation(const std::string& name) const {
    static AllocationInfo empty;
    auto it = allocations_.find(name);
    return (it != allocations_.end()) ? it->second : empty;
}

bool OptimizedCodegen::needs_arena_alloc(const std::string& name) const {
    auto it = allocations_.find(name);
    return (it != allocations_.end() &&
            it->second.strategy == AllocStrategy::Arena);
}

bool OptimizedCodegen::needs_refcount(const std::string& name) const {
    auto it = allocations_.find(name);
    return (it != allocations_.end() && it->second.needs_refcount);
}

bool OptimizedCodegen::needs_destructor(const std::string& name) const {
    auto it = allocations_.find(name);
    return (it != allocations_.end() && it->second.needs_destructor);
}
