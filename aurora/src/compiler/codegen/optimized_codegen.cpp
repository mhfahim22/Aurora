// optimized_codegen.cpp — Optimized Code Generation Implementation
// Part of the Aurora compiler pipeline — Phase 6

#include "compiler/codegen.hpp"
#include "compiler/optimized_codegen.hpp"
#include <llvm/TargetParser/Host.h>
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
    auto triple = llvm::sys::getProcessTriple();
    module_->setTargetTriple(llvm::Triple(triple));
    module_->setDataLayout(llvm_target_data_layout(triple));

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

    /* ── Expression-as-statement: generate and discard ── */
    case NodeType::Num:
    case NodeType::Float:
    case NodeType::Str:
    case NodeType::Var: {
        gen_expr(node);
        break;
    }
    case NodeType::BinOp: {
        gen_expr(node);
        break;
    }
    case NodeType::UnaryOp: {
        gen_expr(node);
        break;
    }
    case NodeType::Call: {
        gen_expr(node);
        break;
    }

    /* ── Output ── */
    case NodeType::Output: {
        gen_output(node);
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

    /* Compute RHS value first */
    llvm::Value* rhs = gen_expr(node->right.get());
    if (!rhs) rhs = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);

    /* Determine the target alloca pointer */
    llvm::Value* ptr = nullptr;

    /* Check for forced allocation strategy from attribute */
    AllocStrategy forced = node->memory_meta.forced_strategy;
    if (forced == AllocStrategy::ForcedGC) {
        ptr = gen_gc_alloc(name, llvm::Type::getInt64Ty(ctx_), 64);
    } else if (forced == AllocStrategy::ForcedStack) {
        ptr = gen_stack_alloc(name, llvm::Type::getInt64Ty(ctx_), 64);
    } else if (forced == AllocStrategy::ForcedArena) {
        ptr = gen_arena_alloc(name, llvm::Type::getInt64Ty(ctx_), 64);
    } else if (forced == AllocStrategy::ForcedRAII) {
        ptr = gen_stack_alloc(name, llvm::Type::getInt64Ty(ctx_), 64);
        gen_destructor_insert(name, ptr);
    } else if (forced == AllocStrategy::ForcedARC) {
        ptr = gen_stack_alloc(name, llvm::Type::getInt64Ty(ctx_), 64);
        gen_refcount_alloc(name, ptr);
    } else {
        /* Check if we have allocation info from memory analyzer */
        auto it = allocations_.find(name);
        if (it != allocations_.end()) {
            AllocationInfo& alloc = it->second;
            switch (alloc.strategy) {
                case AllocStrategy::Stack:
                    ptr = alloc.alloca_ptr;
                    break;
                case AllocStrategy::Arena:
                    ptr = gen_arena_alloc(name, llvm::Type::getInt64Ty(ctx_), alloc.size);
                    break;
                case AllocStrategy::Heap:
                    ptr = gen_heap_alloc(name, llvm::Type::getInt64Ty(ctx_), alloc.size);
                    break;
                case AllocStrategy::RAII:
                    ptr = gen_stack_alloc(name, llvm::Type::getInt64Ty(ctx_), alloc.size);
                    gen_destructor_insert(name, ptr);
                    break;
                case AllocStrategy::ARC:
                    ptr = gen_stack_alloc(name, llvm::Type::getInt64Ty(ctx_), alloc.size);
                    gen_refcount_alloc(name, ptr);
                    break;
                case AllocStrategy::GC:
                    ptr = gen_gc_alloc(name, llvm::Type::getInt64Ty(ctx_), alloc.size);
                    break;
                default:
                    break;
            }
        }
        /* If no allocation info exists, create a default stack alloc */
        if (!ptr)
            ptr = create_entry_alloca(name, llvm::Type::getInt64Ty(ctx_));
    }

    /* Store the RHS into the allocated slot (convert to i64 for uniform representation) */
    if (ptr) {
        llvm::Value* store_val = rhs;
        if (store_val->getType() != llvm::Type::getInt64Ty(ctx_)) {
            if (store_val->getType()->isPointerTy())
                store_val = builder_->CreatePtrToInt(store_val, llvm::Type::getInt64Ty(ctx_), name + "_box");
            else if (store_val->getType()->isDoubleTy())
                store_val = builder_->CreateBitCast(store_val, llvm::Type::getInt64Ty(ctx_), name + "_box");
            else
                store_val = builder_->CreatePtrToInt(store_val, llvm::Type::getInt64Ty(ctx_), name + "_conv");
        }
        builder_->CreateStore(store_val, ptr);
    }
}

void OptimizedCodegen::gen_return(const ASTNode* node) {
    static_cast<void>(node);
    /* If returning a variable with refcount, decrement */
    if (node->left && node->left->type == NodeType::Var) {
        const std::string& name = node->left->value;
        auto it = allocations_.find(name);
        if (it != allocations_.end() && it->second.needs_refcount) {
            /* Don't decrement on return - ownership is transferred */
        }
    }
    if (!builder_->GetInsertBlock() || builder_->GetInsertBlock()->getTerminator()) return;
    llvm::Function* cur_fn = builder_->GetInsertBlock()->getParent();
    auto* rt = cur_fn ? cur_fn->getReturnType() : nullptr;
    if (!rt) {
        builder_->CreateRet(llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0));
        return;
    }
    if (rt->isVoidTy()) {
        builder_->CreateRetVoid();
    } else if (rt->isPointerTy()) {
        builder_->CreateRet(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(rt)));
    } else if (rt->isDoubleTy()) {
        builder_->CreateRet(llvm::ConstantFP::get(rt, 0.0));
    } else {
        builder_->CreateRet(llvm::ConstantInt::get(rt, 0));
    }
}

void OptimizedCodegen::gen_function(const ASTNode* node) {
    std::vector<std::string> param_names;
    std::vector<llvm::Type*> param_types;
    const ASTNode* p = node->args.get();
    while (p) {
        param_names.push_back(p->value);
        param_types.push_back(ast_kind_to_abi_type(ctx_, p->type_annotation.kind, llvm::Type::getInt64Ty(ctx_)));
        p = p->next.get();
    }

    auto ret_kind = get_annotation_kind(node);
    auto* ret_ty = ast_kind_to_abi_type(ctx_, ret_kind, i8ptr_ty());
    auto* fn_type = llvm::FunctionType::get(ret_ty, param_types, false);
    auto* fn = llvm::Function::Create(
        fn_type, llvm::Function::ExternalLinkage,
        node->value, module_.get());

    int ai = 0;
    for (auto& arg : fn->args()) {
        arg.setName(param_names[ai]);
        if (arg.getType()->isPointerTy()) {
            arg.addAttr(llvm::Attribute::getWithCaptureInfo(ctx_, llvm::CaptureInfo::none()));
            arg.addAttr(llvm::Attribute::NoAlias);
        }
        ai++;
    }

    auto* entry_bb = llvm::BasicBlock::Create(ctx_, "entry", fn);
    auto* saved_bb = builder_->GetInsertBlock();

    builder_->SetInsertPoint(entry_bb);

    push_scope();

    ai = 0;
    for (auto& arg : fn->args()) {
        auto* slot = create_entry_alloca(param_names[ai], llvm::Type::getInt64Ty(ctx_));
        llvm::Value* arg_val = &arg;
        if (arg_val->getType() != llvm::Type::getInt64Ty(ctx_)) {
            if (arg_val->getType()->isPointerTy())
                arg_val = builder_->CreatePtrToInt(arg_val, llvm::Type::getInt64Ty(ctx_), param_names[ai] + "_box");
            else if (arg_val->getType()->isDoubleTy())
                arg_val = builder_->CreateBitCast(arg_val, llvm::Type::getInt64Ty(ctx_), param_names[ai] + "_box");
        }
        builder_->CreateStore(arg_val, slot);
        ai++;
    }

    walk_block(node->body.get());
    pop_scope();

    if (!builder_->GetInsertBlock() || !builder_->GetInsertBlock()->getTerminator()) {
        if (ret_ty->isVoidTy()) {
            builder_->CreateRetVoid();
        } else if (ret_ty->isPointerTy()) {
            builder_->CreateRet(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ret_ty)));
        } else if (ret_ty->isDoubleTy()) {
            builder_->CreateRet(llvm::ConstantFP::get(ret_ty, 0.0));
        } else {
            builder_->CreateRet(llvm::ConstantInt::get(ret_ty, 0));
        }
    }

    if (saved_bb) builder_->SetInsertPoint(saved_bb);
}

/* ════════════════════════════════════════════════════════════
   Expression Generators
   ════════════════════════════════════════════════════════════ */

llvm::Value* OptimizedCodegen::gen_expr(const ASTNode* node) {
    if (!node) return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);
    switch (node->type) {
        case NodeType::Num:   return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), std::stoll(node->value));
        case NodeType::Float: return llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx_), std::stod(node->value));
        case NodeType::Var:   return gen_var(node);
        case NodeType::BinOp: return gen_binop(node);
        case NodeType::UnaryOp: return gen_unary(node);
        case NodeType::Call:  return gen_call(node);
        default:
            return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);
    }
}

llvm::Value* OptimizedCodegen::gen_var(const ASTNode* node) {
    const std::string& name = node->value;
    auto it = allocations_.find(name);
    if (it == allocations_.end()) {
        /* Variable not analyzed — load from a fresh alloca */
        auto* alloca = create_entry_alloca(name, llvm::Type::getInt64Ty(ctx_));
        return builder_->CreateLoad(llvm::Type::getInt64Ty(ctx_), alloca, name);
    }
    llvm::Value* ptr = it->second.alloca_ptr;
    if (!ptr) return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);
    return builder_->CreateLoad(llvm::Type::getInt64Ty(ctx_), ptr, name + ".val");
}

llvm::Value* OptimizedCodegen::gen_binop(const ASTNode* node) {
    if (!node->left || !node->right) return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);
    auto* lhs = gen_expr(node->left.get());
    auto* rhs = gen_expr(node->right.get());
    if (!lhs || !rhs) return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);

    const std::string& op = node->value;
    /* Type unification: if one side is ptr and the other i64, cast ptr to i64 */
    if (lhs->getType() != rhs->getType()) {
        if (lhs->getType()->isPointerTy() && rhs->getType()->isIntegerTy())
            lhs = builder_->CreatePtrToInt(lhs, llvm::Type::getInt64Ty(ctx_), "l_ptoi");
        else if (rhs->getType()->isPointerTy() && lhs->getType()->isIntegerTy())
            rhs = builder_->CreatePtrToInt(rhs, llvm::Type::getInt64Ty(ctx_), "r_ptoi");
    }

    /* Float detection: if either operand is double, promote and use float ops */
    if (lhs->getType()->isDoubleTy() || rhs->getType()->isDoubleTy()) {
        auto* dbl = llvm::Type::getDoubleTy(ctx_);
        if (!lhs->getType()->isDoubleTy())
            lhs = builder_->CreateSIToFP(lhs, dbl, "itof");
        if (!rhs->getType()->isDoubleTy())
            rhs = builder_->CreateSIToFP(rhs, dbl, "itof");
        if (op == "+")  return builder_->CreateFAdd(lhs, rhs, "fadd");
        if (op == "-")  return builder_->CreateFSub(lhs, rhs, "fsub");
        if (op == "*")  return builder_->CreateFMul(lhs, rhs, "fmul");
        if (op == "/")  return builder_->CreateFDiv(lhs, rhs, "fdiv");
        if (op == "==") return builder_->CreateFCmpOEQ(lhs, rhs, "feq");
        if (op == "!=") return builder_->CreateFCmpONE(lhs, rhs, "fne");
        if (op == "<")  return builder_->CreateFCmpOLT(lhs, rhs, "flt");
        if (op == ">")  return builder_->CreateFCmpOGT(lhs, rhs, "fgt");
        if (op == "<=") return builder_->CreateFCmpOLE(lhs, rhs, "fle");
        if (op == ">=") return builder_->CreateFCmpOGE(lhs, rhs, "fge");
        return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);
    }

    if (op == "+")  return builder_->CreateAdd(lhs, rhs, "add");
    if (op == "-")  return builder_->CreateSub(lhs, rhs, "sub");
    if (op == "*")  return builder_->CreateMul(lhs, rhs, "mul");
    if (op == "/")  return builder_->CreateSDiv(lhs, rhs, "div");
    if (op == "%")  return builder_->CreateSRem(lhs, rhs, "rem");
    if (op == "&")  return builder_->CreateAnd(lhs, rhs, "and");
    if (op == "|")  return builder_->CreateOr(lhs, rhs, "or");
    if (op == "^")  return builder_->CreateXor(lhs, rhs, "xor");
    if (op == "<<") return builder_->CreateShl(lhs, rhs, "shl");
    if (op == ">>") return builder_->CreateAShr(lhs, rhs, "ashr");
    if (op == "==") return builder_->CreateZExt(builder_->CreateICmpEQ(lhs, rhs, "eq"), llvm::Type::getInt64Ty(ctx_));
    if (op == "!=") return builder_->CreateZExt(builder_->CreateICmpNE(lhs, rhs, "ne"), llvm::Type::getInt64Ty(ctx_));
    if (op == "<")  return builder_->CreateZExt(builder_->CreateICmpSLT(lhs, rhs, "lt"), llvm::Type::getInt64Ty(ctx_));
    if (op == ">")  return builder_->CreateZExt(builder_->CreateICmpSGT(lhs, rhs, "gt"), llvm::Type::getInt64Ty(ctx_));
    if (op == "<=") return builder_->CreateZExt(builder_->CreateICmpSLE(lhs, rhs, "le"), llvm::Type::getInt64Ty(ctx_));
    if (op == ">=") return builder_->CreateZExt(builder_->CreateICmpSGE(lhs, rhs, "ge"), llvm::Type::getInt64Ty(ctx_));

    return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);
}

llvm::Value* OptimizedCodegen::gen_unary(const ASTNode* node) {
    if (!node->left) return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);
    auto* val = gen_expr(node->left.get());
    const std::string& op = node->value;
    if (op == "-") return builder_->CreateNeg(val, "neg");
    if (op == "~") return builder_->CreateNot(val, "not");
    if (op == "!") {
        auto* zero = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);
        return builder_->CreateICmpEQ(val, zero, "not");
    }
    return val;
}

llvm::Value* OptimizedCodegen::gen_call(const ASTNode* node) {
    if (!node->left) return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);
    const std::string& callee = node->left->value;

    /* Gather arguments */
    std::vector<llvm::Value*> args;
    const ASTNode* arg = node->args.get();
    while (arg) {
        args.push_back(gen_expr(arg));
        arg = arg->next.get();
    }

    /* Look up or declare the function */
    llvm::Function* fn = module_->getFunction(callee);
    if (!fn) {
        auto rk = get_annotation_kind(node);
        auto* ret_type = ast_kind_to_abi_type(ctx_, rk, i8ptr_ty());
        auto* i64 = llvm::Type::getInt64Ty(ctx_);
        std::vector<llvm::Type*> param_tys(args.size(), i64);
        auto* fty = llvm::FunctionType::get(ret_type, param_tys, false);
        fn = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, callee, module_.get());
    }

    /* Convert arguments from uniform i64 to function's expected param types */
    {
        auto* fty = fn->getFunctionType();
        for (size_t i = 0; i < args.size() && i < fty->getNumParams(); i++) {
            llvm::Type* param_ty = fty->getParamType(i);
            if (args[i]->getType() != param_ty) {
                if (args[i]->getType()->isIntegerTy() && param_ty->isPointerTy())
                    args[i] = builder_->CreateIntToPtr(args[i], param_ty, "arg_unbox");
                else if (args[i]->getType()->isPointerTy() && param_ty->isIntegerTy())
                    args[i] = builder_->CreatePtrToInt(args[i], param_ty, "arg_box");
                else if (args[i]->getType()->isIntegerTy() && param_ty->isDoubleTy())
                    args[i] = builder_->CreateSIToFP(args[i], param_ty, "arg_itof");
                else if (args[i]->getType()->isDoubleTy() && param_ty->isIntegerTy())
                    args[i] = builder_->CreateFPToSI(args[i], param_ty, "arg_ftoi");
            }
        }
    }

    return builder_->CreateCall(fn, args, callee + "_call");
}

void OptimizedCodegen::gen_output(const ASTNode* node) {
    if (!node->left) return;
    auto* val = gen_expr(node->left.get());
    if (!val) return;

    llvm::Type* ty = val->getType();
    if (ty->isDoubleTy()) {
        llvm::Function* fn = module_->getFunction("aurora_print_float");
        if (!fn) {
            auto* fty = llvm::FunctionType::get(
                llvm::Type::getVoidTy(ctx_),
                { llvm::Type::getDoubleTy(ctx_) }, false);
            fn = llvm::Function::Create(fty, llvm::Function::ExternalLinkage,
                                        "aurora_print_float", module_.get());
        }
        builder_->CreateCall(fn, { val });
    } else if (ty->isPointerTy()) {
        llvm::Function* fn = module_->getFunction("aurora_print_str");
        if (!fn) {
            auto* fty = llvm::FunctionType::get(
                llvm::Type::getVoidTy(ctx_),
                { i8ptr_ty() }, false);
            fn = llvm::Function::Create(fty, llvm::Function::ExternalLinkage,
                                        "aurora_print_str", module_.get());
        }
        builder_->CreateCall(fn, { val });
    } else {
        llvm::Function* fn = module_->getFunction("aurora_print_int");
        if (!fn) {
            auto* fty = llvm::FunctionType::get(
                llvm::Type::getVoidTy(ctx_),
                { llvm::Type::getInt64Ty(ctx_) }, false);
            fn = llvm::Function::Create(fty, llvm::Function::ExternalLinkage,
                                        "aurora_print_int", module_.get());
        }
        builder_->CreateCall(fn, { val });
    }
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