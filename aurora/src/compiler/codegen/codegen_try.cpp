#include "compiler/codegen.hpp"
#include "compiler/class_oop.hpp"

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>

/* ════════════════════════════════════════════════════════════
   try / catch / throw  (codegen_try.cpp)
   ─────────────────────────────────────────────────────────────
   Aurora syntax:
       try:
           <body>
       catch:
           <handler>

       throw expr
       throw          ← bare throw (val = 0)

   Runtime helper (memory.cpp):
       void aurora_try_exec(try_fn, catch_fn, ctx, finally_fn)
           — runs try_fn(ctx); if it throws AuroraThrow, runs
             catch_fn(ctx, val); finally_fn runs in all cases.
       void aurora_throw(val, has)
           — throws AuroraThrow{val, has}

   Outlined helper functions avoid LLVM's inability to model
   setjmp/longjmp across basic blocks.  Shared state is passed
   through an i64[] context array.
   ════════════════════════════════════════════════════════════ */

/* ── try / catch ── */
void Codegen::gen_try(const ASTNode* node) {
    /*
     * try/catch via outlined helper functions + aurora_try_exec().
     *
     * LLVM cannot model setjmp/longjmp across basic blocks inside the
     * same function — the optimizer rearranges or tail-calls through
     * the setjmp frame and longjmp lands in garbage.
     *
     * The fix: outline each try-body and catch-body into their own
     * private LLVM Functions.  aurora_try_exec() (in the runtime) runs
     * them with a real C setjmp on its own stack frame.  longjmp always
     * has a valid frame to return to.
     *
     * Shared mutable state (variables visible in the outer scope) is
     * passed through a small i64[] context array allocated in the outer
     * function's frame.  The outlined functions read vars from ctx at
     * entry and write them back before returning, so the outer scope
     * sees every modification.
     *
     * Runtime signature:
     *   void aurora_try_exec(
     *       void (*try_fn )(i64*),
     *       void (*catch_fn)(i64*, i64 ex_val),
     *       i64 *ctx);
     */

    static int try_counter = 0;
    const int  try_id      = try_counter++;

    llvm::LLVMContext& C    = ctx_;
    llvm::Type* i64T        = i64_ty();
    llvm::Type* voidT       = llvm::Type::getVoidTy(C);
    llvm::Type* i64PtrT     = llvm::PointerType::get(i64T, 0);

    /* Function types for the two outlined helpers */
    llvm::FunctionType* try_fty =
        llvm::FunctionType::get(voidT, {i64PtrT},         false);
    llvm::FunctionType* cat_fty =
        llvm::FunctionType::get(voidT, {i64PtrT, i64T},   false);

    /* Function type for the finally outlined helper */
    llvm::FunctionType* fin_fty =
        llvm::FunctionType::get(voidT, {i64PtrT}, false);

    /* aurora_try_exec declaration (idempotent) */
    llvm::Function* fn_exec = module_->getFunction("aurora_try_exec");
    if (!fn_exec) {
        auto* exec_fty = llvm::FunctionType::get(voidT,
            {llvm::PointerType::get(try_fty, 0),
             llvm::PointerType::get(cat_fty, 0),
             i64PtrT,
             llvm::PointerType::get(fin_fty, 0)}, false);
        fn_exec = llvm::Function::Create(
            exec_fty, llvm::Function::ExternalLinkage,
            "aurora_try_exec", module_.get());
    }

    /* ── Collect live scalar vars from every scope ──────────────────
       Only plain i64 slots; arrays/strings need separate handling
       (they carry runtime state in separate allocations, so the i64
       handle in the alloca is still the right thing to pass).        */
     struct LiveVar { std::string name; llvm::Value* slot; llvm::Type* ty; };
    std::vector<LiveVar> live;
    for (auto& scope : scopes_) {
        for (auto& [nm, rec] : scope.vars) {
            if (!rec.alloca_ptr) continue;

            llvm::Type* val_ty = nullptr;
            llvm::Value* slot = rec.alloca_ptr;

            if (auto* ai = llvm::dyn_cast<llvm::AllocaInst>(slot)) {
                val_ty = ai->getAllocatedType();
                if (ai->getParent()->getParent() != cur_fn_) continue;
            } else if (auto* inst = llvm::dyn_cast<llvm::Instruction>(slot)) {
                /* Non-AllocaInst Instruction slot (e.g. BitCastInst from
                   arena_alloc for string variables).  Only include slots
                   whose loaded value is a pointer.                     */
                if (inst->getParent()->getParent() != cur_fn_) continue;
                val_ty = i8ptr_ty();
            } else {
                /* Non-instruction slots (GlobalVariable, Constant, etc.)
                   should not participate in try/catch live-var save/restore. */
                continue;
            }

            if (!val_ty) continue;
            if (!val_ty->isIntegerTy(64) && !val_ty->isPointerTy()) continue;

            live.push_back({nm, slot, val_ty});
        }
    }
    const int nv = static_cast<int>(live.size());

    /* ── Allocate context array in the OUTER function's frame ──────── */
    /* slot nv is reserved for __exception__ (written by catch, unused by try) */
    llvm::AllocaInst* ctx_arr =
        llvm::IRBuilder<>(&cur_fn_->getEntryBlock(),
                           cur_fn_->getEntryBlock().begin())
        .CreateAlloca(i64T, i64(nv + 1), "try_ctx");

    /* Helpers to save/restore live vars to/from ctx */
    auto save_vars = [&]() {
        for (int i = 0; i < nv; i++) {
            llvm::Value* v = builder_->CreateLoad(live[i].ty, live[i].slot, "ctx_sv");
            if (live[i].ty->isPointerTy())
                v = builder_->CreatePtrToInt(v, i64T);
            llvm::Value* p = builder_->CreateGEP(i64T, ctx_arr, i64(i));
            builder_->CreateStore(v, p);
        }
    };
    auto restore_vars = [&]() {
        for (int i = 0; i < nv; i++) {
            llvm::Value* p = builder_->CreateGEP(i64T, ctx_arr, i64(i));
            llvm::Value* v = builder_->CreateLoad(i64T, p, "ctx_rv");
            if (live[i].ty->isPointerTy())
                v = builder_->CreateIntToPtr(v, live[i].ty);
            builder_->CreateStore(v, live[i].slot);
        }
    };

    /* ── Generic outlined-function builder ──────────────────────────── */
    auto build_outlined =
        [&](const std::string&    fname,
            llvm::FunctionType*   fty,
            const ASTNode*        body_node,
            bool                  is_catch,
            const std::string&    catch_var_name = "__exception__") -> llvm::Function*
    {
        auto* fn = llvm::Function::Create(
            fty, llvm::Function::PrivateLinkage, fname, module_.get());
        fn->addParamAttr(0, llvm::Attribute::NoCapture);  /* ctx ptr does not escape */

        llvm::Value* arg_ctx = fn->getArg(0);  arg_ctx->setName("ctx");
        llvm::Value* arg_ex  = is_catch ? fn->getArg(1) : nullptr;
        if (arg_ex) arg_ex->setName("ex");

        /* Save outer codegen state */
        llvm::Function*    saved_fn    = cur_fn_;
        llvm::BasicBlock*  saved_block = builder_->GetInsertBlock();
        auto               saved_cache = std::move(literal_aurora_cache_);

        cur_fn_ = fn;
        auto* entry_bb = llvm::BasicBlock::Create(C, "entry", fn);
        builder_->SetInsertPoint(entry_bb);

        /* Restore live vars from ctx into fresh alloca slots */
        push_scope();
        for (int i = 0; i < nv; i++) {
            auto* slot = create_entry_alloca(live[i].name, live[i].ty);
            llvm::Value* p = builder_->CreateGEP(i64T, arg_ctx, i64(i));
            llvm::Value* v = builder_->CreateLoad(i64T, p, "in");
            if (live[i].ty->isPointerTy())
                v = builder_->CreateIntToPtr(v, live[i].ty);
            builder_->CreateStore(v, slot);
            declare_var(live[i].name, slot, OwnershipState::Owned);
        }

        /* Expose catch variable (or __exception__) in catch bodies */
        if (is_catch && arg_ex) {
            auto* ex_slot = create_entry_alloca(catch_var_name, i64T);
            builder_->CreateStore(arg_ex, ex_slot);
            declare_var(catch_var_name, ex_slot, OwnershipState::Owned);
        }

        /* Generate the body */
        if (body_node) gen_block(body_node);

        /* Handle continue/break in catch bodies: the generated br instruction
           targets a basic block in the outer function, which is invalid in the
           outlined function.  Remove the terminator and let clean-up run.     */
        if (current_block_terminated()) {
            if (llvm::isa<llvm::BranchInst>(builder_->GetInsertBlock()->getTerminator()))
                builder_->GetInsertBlock()->getTerminator()->eraseFromParent();
            if (!current_block_terminated())
                builder_->SetInsertPoint(builder_->GetInsertBlock());
        }

        /* Write back all live vars to ctx before returning */
        if (!current_block_terminated()) {
            for (int i = 0; i < nv; i++) {
                VarRecord* rec = lookup_var(live[i].name);
                if (!rec || !rec->alloca_ptr) continue;
                llvm::Value* v = builder_->CreateLoad(live[i].ty, rec->alloca_ptr, "out");
                if (live[i].ty->isPointerTy())
                    v = builder_->CreatePtrToInt(v, i64T);
                llvm::Value* p = builder_->CreateGEP(i64T, arg_ctx, i64(i));
                builder_->CreateStore(v, p);
            }
        }

        pop_scope_and_drop();
        if (!current_block_terminated()) builder_->CreateRetVoid();

        /* Restore outer codegen state */
        literal_aurora_cache_ = std::move(saved_cache);
        cur_fn_ = saved_fn;
        builder_->SetInsertPoint(saved_block);
        return fn;
    };

    /* ── Emit the three helper functions ────────────────────────────── */
    auto* try_fn = build_outlined(
        "__try_"   + std::to_string(try_id), try_fty, node->body.get(),   false);
    std::string catch_var = node->value.empty() ? "__exception__" : node->value;
    auto* cat_fn = build_outlined(
        "__catch_" + std::to_string(try_id), cat_fty, node->orelse.get(), true, catch_var);

    /* Finally function (optional) */
    llvm::Function* fin_fn = nullptr;
    if (node->right && node->right->type == NodeType::Ensure) {
        fin_fn = build_outlined(
            "__finally_" + std::to_string(try_id), fin_fty,
            node->right->body.get(), false);
    }

    /* ── In the outer function: save → exec → restore ────────────────*/
    save_vars();
    llvm::Value* fin_arg = fin_fn
        ? static_cast<llvm::Value*>(fin_fn)
        : static_cast<llvm::Value*>(llvm::ConstantPointerNull::get(llvm::PointerType::get(fin_fty, 0)));
    builder_->CreateCall(fn_exec, {try_fn, cat_fn, ctx_arr, fin_arg});
    restore_vars();
}


/* ── throw expr  /  throw ── */
void Codegen::gen_throw(const ASTNode* node) {
    auto get_or_decl = [&](const char* name,
                            llvm::Type* ret,
                            std::vector<llvm::Type*> params) -> llvm::Function* {
        if (auto* f = module_->getFunction(name)) return f;
        auto* fty = llvm::FunctionType::get(ret, params, false);
        return llvm::Function::Create(fty,
                                      llvm::Function::ExternalLinkage,
                                      name, module_.get());
    };

    llvm::Type* i64v  = i64_ty();
    llvm::Type* voidT = llvm::Type::getVoidTy(ctx_);
    auto* fn_throw = get_or_decl("aurora_throw", voidT, {i64v, i64v});

    llvm::Value* val = node->left ? gen_expr(node->left.get()) : i64(0);
    llvm::Value* has = node->left ? i64(1) : i64(0);
    builder_->CreateCall(fn_throw, {val, has});

    /* Ret instead of unreachable — prevents LLVM from inferring noreturn
       on the enclosing function (which would cause the optimizer to
       eliminate calls to functions that contain throws).  The ret is
       never reached at runtime because aurora_throw throws a C++
       exception via the runtime library. */
    auto* ret_ty = cur_fn_->getReturnType();
    if (ret_ty->isVoidTy())
        builder_->CreateRetVoid();
    else
        builder_->CreateRet(llvm::PoisonValue::get(ret_ty));

    /* Start a new dead block so subsequent code generation doesn't break */
    auto* dead_bb = llvm::BasicBlock::Create(ctx_, "throw_dead", cur_fn_);
    builder_->SetInsertPoint(dead_bb);
}
