#include "compiler/codegen.hpp"
#include "compiler/class_oop.hpp"

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/ADT/SmallVector.h>

/* ════════════════════════════════════════════════════════════
   Function / Class / Method generation  (codegen_function.cpp)
   ════════════════════════════════════════════════════════════ */

/* ── function fname(params): body ── */
void Codegen::gen_function(const ASTNode* node) {
    /* Build param type list — prefer annotation, fall back to i64 */
    std::vector<std::string>    param_names;
    std::vector<llvm::Type*>    param_types;
    std::vector<AstTypeKind>    param_type_kinds;
    const ASTNode* p = node->args.get();
    while (p) {
        param_names.push_back(p->value);
        param_types.push_back(ast_kind_to_abi_type(ctx_, p->type_annotation.kind, i64_ty()));
        param_type_kinds.push_back(p->type_annotation.kind);
        p = p->next.get();
    }

    auto ret_kind = get_annotation_kind(node);
    auto* ret_ty  = ast_kind_to_abi_type(ctx_, ret_kind, i8ptr_ty());
    auto* fn_type = llvm::FunctionType::get(ret_ty, param_types, false);
    auto* fn      = llvm::Function::Create(
        fn_type, llvm::Function::ExternalLinkage,
        node->value, module_.get());

    /* Name the LLVM arguments and add optimization attributes */
    int ai = 0;
    for (auto& arg : fn->args()) {
        arg.setName(param_names[ai]);
        if (arg.getType()->isPointerTy()) {
            arg.addAttr(llvm::Attribute::NoCapture);
            arg.addAttr(llvm::Attribute::NoAlias);
        }
        ai++;
    }

    /* Prevent inlining of closure-returning functions: when the
       optimizer inlines them, the indirect call through the closure
       {ptr,ptr} struct is incorrectly simplified into calling the
       struct pointer directly instead of extracting field 0. */
    if (closure_returning_fns_.count(node->value))
        fn->addFnAttr(llvm::Attribute::NoInline);

    /* Create DISubprogram for debug info */
    llvm::DISubprogram* sp = nullptr;
    if (dibuilder_ && debug_file_) {
        llvm::SmallVector<llvm::Metadata*, 8> debug_types;
        debug_types.push_back(get_debug_type(ret_kind));
        for (auto k : param_type_kinds)
            debug_types.push_back(get_debug_type(k));
        sp = dibuilder_->createFunction(
            debug_file_,
            node->value,
            fn->getName(),
            debug_file_,
            node->src_line,
            dibuilder_->createSubroutineType(
                dibuilder_->getOrCreateTypeArray(debug_types)),
            node->src_line,
            llvm::DINode::FlagZero,
            llvm::DISubprogram::SPFlagDefinition);
        if (sp) {
            fn->setSubprogram(sp);
            debug_cur_fn_ = sp;
        }
    }

    auto* saved_fn_sp = debug_cur_fn_;
    auto* entry_bb  = llvm::BasicBlock::Create(ctx_, "entry", fn);
    auto* saved_fn  = cur_fn_;
    auto* saved_bb  = builder_->GetInsertBlock();
    auto  saved_cache  = std::move(literal_aurora_cache_);
    auto  saved_scopes = std::move(scopes_);

    cur_fn_ = fn;
    builder_->SetInsertPoint(entry_bb);

    /* Create fresh scope stack so module-level vars aren't 
       accidentally cleaned up by gen_return's emit_all_scope_cleanup. */
    push_scope();

    /* Coverage: trace function entry */
    if (coverage_enabled_ && node->value != "main")
        emit_coverage_trace(node->src_line);

    /* Allocate params as i64 (for arithmetic) */
    ai = 0;
    for (auto& arg : fn->args()) {
        auto* slot = create_entry_alloca(param_names[ai], i64_ty());
        builder_->CreateStore(&arg, slot);
        declare_var(param_names[ai], slot, OwnershipState::Owned, param_type_kinds[ai]);
        ai++;
    }

    gen_block(node->body.get());
    pop_scope_and_drop();

    /* Default return: appropriate zero for the function's return type */
    {
        auto* rt = fn->getReturnType();
        if (rt->isVoidTy()) {
            if (!current_block_terminated()) builder_->CreateRetVoid();
        } else if (rt->isPointerTy()) {
            safe_ret(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(rt)));
        } else {
            safe_ret(llvm::ConstantInt::get(rt, 0));
        }
    }

    /* Restore caller context */
    literal_aurora_cache_ = std::move(saved_cache);
    scopes_ = std::move(saved_scopes);
    debug_cur_fn_ = saved_fn_sp;
    cur_fn_ = saved_fn;
    if (saved_bb) builder_->SetInsertPoint(saved_bb);
}

/* ── Concrete generic function instantiation ──
   Emits a monomorphized version of a generic function with
   resolved param types and the mangled instantiation name.
   The LLVM function declaration is expected to already exist
   (created as a forward decl in generate()).               */
void Codegen::gen_generic_instance(const std::string& mangled_name,
                                    const ASTNode* generic_node,
                                    const std::vector<AstTypeKind>& param_kinds,
                                    AstTypeKind result_kind) {
    /* Build param names from the generic AST node */
    std::vector<std::string> param_names;
    {
        const ASTNode* p = generic_node->args.get();
        while (p) {
            param_names.push_back(p->value);
            p = p->next.get();
        }
    }

    llvm::Function* fn = module_->getFunction(mangled_name);
    if (!fn) {
        /* Build param type list from concrete kinds (forward decl missing) */
        std::vector<llvm::Type*> param_types;
        size_t pk_idx = 0;
        const ASTNode* p2 = generic_node->args.get();
        while (p2) {
            AstTypeKind kind = (pk_idx < param_kinds.size())
                ? param_kinds[pk_idx]
                : AstTypeKind::Unknown;
            param_types.push_back(ast_kind_to_abi_type(ctx_, kind, i64_ty()));
            pk_idx++;
            p2 = p2->next.get();
        }
        auto* ret_ty = ast_kind_to_abi_type(ctx_, result_kind, i8ptr_ty());
        auto* fn_type = llvm::FunctionType::get(ret_ty, param_types, false);
        fn = llvm::Function::Create(
            fn_type, llvm::Function::ExternalLinkage,
            mangled_name, module_.get());
    }

    /* Name the LLVM arguments */
    int ai = 0;
    for (auto& arg : fn->args()) {
        arg.setName(param_names[ai]);
        if (arg.getType()->isPointerTy()) {
            arg.addAttr(llvm::Attribute::NoCapture);
            arg.addAttr(llvm::Attribute::NoAlias);
        }
        ai++;
    }

    /* Prevent inlining of closure-returning functions */
    if (closure_returning_fns_.count(generic_node->value))
        fn->addFnAttr(llvm::Attribute::NoInline);

    /* Create DISubprogram for debug info */
    llvm::DISubprogram* sp = nullptr;
    if (dibuilder_ && debug_file_) {
        llvm::SmallVector<llvm::Metadata*, 8> debug_types;
        debug_types.push_back(get_debug_type(result_kind));
        for (auto k : param_kinds)
            debug_types.push_back(get_debug_type(k));
        sp = dibuilder_->createFunction(
            debug_file_,
            mangled_name,
            fn->getName(),
            debug_file_,
            generic_node->src_line,
            dibuilder_->createSubroutineType(
                dibuilder_->getOrCreateTypeArray(debug_types)),
            generic_node->src_line,
            llvm::DINode::FlagZero,
            llvm::DISubprogram::SPFlagDefinition);
        if (sp) {
            fn->setSubprogram(sp);
            debug_cur_fn_ = sp;
        }
    }

    auto* saved_fn_sp = debug_cur_fn_;
    auto* entry_bb  = llvm::BasicBlock::Create(ctx_, "entry", fn);
    auto* saved_fn  = cur_fn_;
    auto* saved_bb  = builder_->GetInsertBlock();
    auto  saved_cache  = std::move(literal_aurora_cache_);
    auto  saved_scopes = std::move(scopes_);

    cur_fn_ = fn;
    builder_->SetInsertPoint(entry_bb);
    push_scope();

    if (coverage_enabled_)
        emit_coverage_trace(generic_node->src_line);

    /* Allocate params with concrete LLVM type matching the function signature */
    ai = 0;
    for (auto& arg : fn->args()) {
        llvm::Type* arg_ty = arg.getType();
        auto* slot = create_entry_alloca(param_names[ai], arg_ty);
        builder_->CreateStore(&arg, slot);
        declare_var(param_names[ai], slot, OwnershipState::Owned,
                    ai < static_cast<int>(param_kinds.size()) ? param_kinds[ai] : AstTypeKind::Unknown);
        ai++;
    }

    gen_block(generic_node->body.get());
    pop_scope_and_drop();

    /* Default return */
    {
        auto* rt = fn->getReturnType();
        if (rt->isVoidTy()) {
            if (!current_block_terminated()) builder_->CreateRetVoid();
        } else if (rt->isPointerTy()) {
            safe_ret(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(rt)));
        } else {
            safe_ret(llvm::ConstantInt::get(rt, 0));
        }
    }

    /* Restore caller context */
    literal_aurora_cache_ = std::move(saved_cache);
    scopes_ = std::move(saved_scopes);
    debug_cur_fn_ = saved_fn_sp;
    cur_fn_ = saved_fn;
    if (saved_bb) builder_->SetInsertPoint(saved_bb);
}

/* ════════════════════════════════════════════════════════════
   OOP: Class definition — emit all method functions
   ════════════════════════════════════════════════════════════ */
void Codegen::gen_class_oop(const ASTNode* node) {
    const std::string& class_name = node->value;
    const ClassInfo* cls = global_class_registry().get(class_name);
    if (!cls) return;

    /* Save current context */
    auto saved_scopes = std::move(scopes_);
    auto saved_cache  = std::move(literal_aurora_cache_);
    llvm::Function* saved_fn = cur_fn_;
    llvm::BasicBlock* saved_bb = builder_->GetInsertBlock();
    scopes_.clear();

    /* Walk class body — emit each method */
    const ASTNode* stmt = node->body.get();
    while (stmt) {
        if (stmt->type == NodeType::Function) {
            std::string llvm_name = class_name + "__" + stmt->value;

            /* self pointer + declared params — prefer annotation, fall back to i8* */
            std::vector<llvm::Type*> param_types;
            param_types.push_back(llvm::PointerType::getUnqual(ctx_)); /* self */
            const ASTNode* param = stmt->args.get();
            while (param) {
                if (param->value != "self")
                    param_types.push_back(ast_kind_to_abi_type(ctx_, param->type_annotation.kind, i8ptr_ty()));
                param = param->next.get();
            }

            auto method_ret_kind = get_annotation_kind(stmt);
            auto* method_ret_ty  = ast_kind_to_abi_type(ctx_, method_ret_kind, i8ptr_ty());
            auto* fn_ty = llvm::FunctionType::get(method_ret_ty, param_types, false);
            llvm::Function* fn = module_->getFunction(llvm_name);
            if (!fn)
                fn = llvm::Function::Create(
                    fn_ty, llvm::Function::ExternalLinkage, llvm_name, *module_);

            /* Set param names */
            auto ai = fn->arg_begin();
            ai->setName("self");
            llvm::Value* self_ptr = &*ai++;
            param = stmt->args.get();
            while (param) {
                if (param->value != "self" && ai != fn->arg_end()) {
                    ai->setName(param->value);
                    ++ai;
                }
                param = param->next.get();
            }

            llvm::BasicBlock* entry = llvm::BasicBlock::Create(ctx_, "entry", fn);
            builder_->SetInsertPoint(entry);
            cur_fn_ = fn;

            /* Create DISubprogram for OOP method */
            llvm::DISubprogram* saved_method_sp = debug_cur_fn_;
            if (dibuilder_ && debug_file_) {
                llvm::SmallVector<llvm::Metadata*, 8> debug_types;
                debug_types.push_back(get_debug_type(method_ret_kind));
                debug_types.push_back(dibuilder_->createBasicType("self", 64, llvm::dwarf::DW_ATE_address));
                const ASTNode* mp = stmt->args.get();
                while (mp) {
                    if (mp->value != "self")
                        debug_types.push_back(get_debug_type(mp->type_annotation.kind));
                    mp = mp->next.get();
                }
                auto* method_sp = dibuilder_->createFunction(
                    debug_file_,
                    stmt->value,
                    fn->getName(),
                    debug_file_,
                    stmt->src_line,
                    dibuilder_->createSubroutineType(
                        dibuilder_->getOrCreateTypeArray(debug_types)),
                    stmt->src_line,
                    llvm::DINode::FlagZero,
                    llvm::DISubprogram::SPFlagDefinition);
                if (method_sp) {
                    fn->setSubprogram(method_sp);
                    debug_cur_fn_ = method_sp;
                }
            }

            literal_aurora_cache_.clear();
            push_scope();

            /* Register self_ptr so self.field works */
            /* We track it via a special key "__self__" */
            oop_register_object_ptr("__self__", class_name, self_ptr);

            /* Register method parameters as local vars (bitcast i8* to i64) */
            ai = fn->arg_begin();
            ++ai; /* skip self */
            param = stmt->args.get();
            while (param) {
                if (param->value != "self" && ai != fn->arg_end()) {
                    auto* slot = create_entry_alloca(param->value, i64_ty());
                    llvm::Value* val = &*ai;
                    if (val->getType() != i64_ty())
                        val = builder_->CreatePtrToInt(val, i64_ty(), param->value + "_unbox");
                    builder_->CreateStore(val, slot);
                    declare_var(param->value, slot, OwnershipState::Owned, param->type_annotation.kind);
                    ++ai;
                }
                param = param->next.get();
            }

            /* Walk method body */
            const ASTNode* body = stmt->body.get();
            while (body) {
                if (dibuilder_ && body->src_line > 0 && debug_cur_fn_)
                    builder_->SetCurrentDebugLocation(
                        llvm::DILocation::get(ctx_, body->src_line, body->src_col, debug_cur_fn_));
                gen_stmt_in_method(body, self_ptr, class_name);
                body = body->next.get();
            }

            if (!current_block_terminated()) {
                auto* rt = fn->getReturnType();
                if (rt->isVoidTy()) {
                    builder_->CreateRetVoid();
                } else if (rt->isPointerTy()) {
                    builder_->CreateRet(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(rt)));
                } else {
                    builder_->CreateRet(llvm::ConstantInt::get(rt, 0));
                }
            }

            pop_scope_and_drop();
            debug_cur_fn_ = saved_method_sp;
        }
        stmt = stmt->next.get();
    }

    /* Restore context */
    scopes_ = std::move(saved_scopes);
    literal_aurora_cache_ = std::move(saved_cache);
    cur_fn_ = saved_fn;
    if (saved_bb) builder_->SetInsertPoint(saved_bb);
}

/* Generate a statement inside a method body — handles self.field */
void Codegen::gen_stmt_in_method(const ASTNode* node,
                                   llvm::Value* self_ptr,
                                   const std::string& class_name) {
    if (!node) return;

    /* self.field = expr */
    if (node->type == NodeType::Assign && node->left &&
        node->left->type == NodeType::Attribute &&
        node->left->left &&
        node->left->left->value == "self") {

        const std::string& field_name = node->left->value;
        llvm::Value* val = gen_expr_in_method(node->right.get(), self_ptr, class_name);
        oop_gen_self_field_set(ctx_, *builder_, self_ptr, class_name, field_name, val, node->src_line);
        return;
    }

    /* Generic: substitute self-aware expr evaluation */
    gen_stmt_with_self(node, self_ptr, class_name);
}

/* Self-aware version of gen_expr */
llvm::Value* Codegen::gen_expr_in_method(const ASTNode* node,
                                           llvm::Value* self_ptr,
                                           const std::string& class_name) {
    if (!node) return i64(0);

    /* self.field read */
    if (node->type == NodeType::Attribute && node->left &&
        node->left->value == "self") {
        return oop_gen_self_field_get(ctx_, *builder_, self_ptr, class_name,
                                       node->value, node->src_line);
    }

    /* BinOp with possible self.field operands */
    if (node->type == NodeType::BinOp) {
        llvm::Value* L = gen_expr_in_method(node->left.get(), self_ptr, class_name);
        llvm::Value* R = gen_expr_in_method(node->right.get(), self_ptr, class_name);
        const std::string& op = node->value;

        /* Helper: promote to double if either side is float */
        auto promote_to_float = [&](llvm::Value*& a, llvm::Value*& b) {
            /* H2 Phase C: prefer annotation for float detection */
            if (get_annotation_kind(node) == AstTypeKind::Float ||
                a->getType()->isDoubleTy() || b->getType()->isDoubleTy()) {
                auto* dbl = llvm::Type::getDoubleTy(ctx_);
                if (!a->getType()->isDoubleTy()) a = builder_->CreateSIToFP(a, dbl);
                if (!b->getType()->isDoubleTy()) b = builder_->CreateSIToFP(b, dbl);
            }
        };

        /* Arithmetic operators */
        if (op == "+") {
            /* H2 Phase C: prefer annotation for string concat detection */
            if (get_annotation_kind(node) == AstTypeKind::String ||
                L->getType()->isPointerTy() || R->getType()->isPointerTy()) {
                /* string concat — convert non-string to string, allocate new */
                auto to_str = [&](llvm::Value*& V, const char* name) {
                    if (!V->getType()->isPointerTy()) {
                        if (V->getType()->isDoubleTy())
                            V = builder_->CreateCall(fn_float_to_str_, { V }, name);
                        else
                            V = builder_->CreateCall(fn_int_to_str_, { V }, name);
                    }
                };
                to_str(L, "l_str");
                to_str(R, "r_str");
                auto* str_ty = llvm::StructType::get(
                    ctx_, { i8ptr_ty(), i64_ty(), i64_ty() }, false);
                auto* rdata_gep = builder_->CreateStructGEP(str_ty, R, 0, "rdata");
                auto* rlen_gep  = builder_->CreateStructGEP(str_ty, R, 1, "rlen");
                auto* rdata = builder_->CreateLoad(i8ptr_ty(), rdata_gep, "rdata_v");
                auto* rlen  = builder_->CreateLoad(i64_ty(),   rlen_gep,  "rlen_v");
                auto* ldata_gep = builder_->CreateStructGEP(str_ty, L, 0, "ldata");
                auto* llen_gep  = builder_->CreateStructGEP(str_ty, L, 1, "llen");
                auto* ldata = builder_->CreateLoad(i8ptr_ty(), ldata_gep, "ldata_v");
                auto* llen  = builder_->CreateLoad(i64_ty(),   llen_gep,  "llen_v");
                auto* result = builder_->CreateCall(fn_str_append_, { L, R }, "strcat_result");
                return result;
            }
            promote_to_float(L, R);
            if (L->getType()->isDoubleTy())
                return builder_->CreateFAdd(L, R, "fadd");
            return builder_->CreateAdd(L, R, "add", false, true);
        }
        if (op == "-") {
            promote_to_float(L, R);
            if (L->getType()->isDoubleTy())
                return builder_->CreateFSub(L, R, "fsub");
            return builder_->CreateSub(L, R, "sub", false, true);
        }
        if (op == "*") {
            promote_to_float(L, R);
            if (L->getType()->isDoubleTy())
                return builder_->CreateFMul(L, R, "fmul");
            return builder_->CreateMul(L, R, "mul", false, true);
        }
        if (op == "/") {
            promote_to_float(L, R);
            if (L->getType()->isDoubleTy())
                return builder_->CreateFDiv(L, R, "fdiv");
            return builder_->CreateSDiv(L, R, "div");
        }
        if (op == "%") {
            promote_to_float(L, R);
            if (L->getType()->isDoubleTy())
                return builder_->CreateFRem(L, R, "frem");
            return builder_->CreateSRem(L, R, "srem");
        }
        if (op == "**") {
            promote_to_float(L, R);
            auto* pow_fn = module_->getFunction("aurora_pow");
            if (!pow_fn) {
                auto* fty = llvm::FunctionType::get(
                    llvm::Type::getDoubleTy(ctx_),
                    {llvm::Type::getDoubleTy(ctx_), llvm::Type::getDoubleTy(ctx_)}, false);
                pow_fn = llvm::Function::Create(fty, llvm::Function::ExternalLinkage,
                                                "aurora_pow", module_.get());
            }
            if (!L->getType()->isDoubleTy()) {
                auto* dbl = llvm::Type::getDoubleTy(ctx_);
                L = builder_->CreateSIToFP(L, dbl);
            }
            if (!R->getType()->isDoubleTy()) {
                auto* dbl = llvm::Type::getDoubleTy(ctx_);
                R = builder_->CreateSIToFP(R, dbl);
            }
            return builder_->CreateCall(pow_fn, { L, R }, "pow_result");
        }
        if (op == "//") {
            promote_to_float(L, R);
            if (L->getType()->isDoubleTy()) {
                auto* div = builder_->CreateFDiv(L, R, "fdiv");
                auto* trunc = builder_->CreateFPToSI(div, llvm::Type::getInt64Ty(ctx_), "trunc");
                return builder_->CreateSIToFP(trunc, llvm::Type::getDoubleTy(ctx_));
            }
            return builder_->CreateSDiv(L, R, "div");
        }

        /* Comparison operators */
        if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=") {
            if (L->getType()->isDoubleTy() || R->getType()->isDoubleTy()) {
                promote_to_float(L, R);
                llvm::CmpInst::Predicate pred;
                if (op == "==") pred = llvm::CmpInst::FCMP_OEQ;
                else if (op == "!=") pred = llvm::CmpInst::FCMP_UNE;
                else if (op == "<") pred = llvm::CmpInst::FCMP_OLT;
                else if (op == ">") pred = llvm::CmpInst::FCMP_OGT;
                else if (op == "<=") pred = llvm::CmpInst::FCMP_OLE;
                else pred = llvm::CmpInst::FCMP_OGE;
                return builder_->CreateFCmp(pred, L, R, "fcmp");
            }
            llvm::CmpInst::Predicate pred;
            if (op == "==") pred = llvm::CmpInst::ICMP_EQ;
            else if (op == "!=") pred = llvm::CmpInst::ICMP_NE;
            else if (op == "<") pred = llvm::CmpInst::ICMP_SLT;
            else if (op == ">") pred = llvm::CmpInst::ICMP_SGT;
            else if (op == "<=") pred = llvm::CmpInst::ICMP_SLE;
            else pred = llvm::CmpInst::ICMP_SGE;
            return builder_->CreateICmp(pred, L, R, "icmp");
        }

        /* Logical / bitwise operators */
        if (op == "and") {
            L = builder_->CreateICmpNE(L, llvm::ConstantInt::get(L->getType(), 0));
            R = builder_->CreateICmpNE(R, llvm::ConstantInt::get(R->getType(), 0));
            return builder_->CreateZExt(builder_->CreateAnd(L, R, "and"), i64_ty());
        }
        if (op == "or") {
            L = builder_->CreateICmpNE(L, llvm::ConstantInt::get(L->getType(), 0));
            R = builder_->CreateICmpNE(R, llvm::ConstantInt::get(R->getType(), 0));
            return builder_->CreateZExt(builder_->CreateOr(L, R, "or"), i64_ty());
        }
        if (op == "^" || op == "xor") {
            return builder_->CreateXor(L, R, "xor");
        }

        /* Fallback for unhandled operators */
        return gen_expr(node);
    }

    /* Method call on self: self.method(args) */
    if (node->type == NodeType::Call) {
        std::string method_name = node->value;
        auto dot = method_name.find('.');
        if (dot != std::string::npos) {
            method_name = method_name.substr(dot + 1);
        }
        if (global_class_registry().find_method(class_name, method_name)) {
            auto gen_fn = [this, self_ptr, &class_name](const ASTNode* n) -> llvm::Value* {
                return gen_expr_in_method(n, self_ptr, class_name);
            };
            return oop_gen_method_call_ptr(ctx_, *builder_, *module_,
                class_name, self_ptr, method_name, node->args.get(), node->src_line, gen_fn);
        }
    }

    return gen_expr(node);
}

/* Self-aware gen_stmt */
void Codegen::gen_stmt_with_self(const ASTNode* node,
                                  llvm::Value* self_ptr,
                                  const std::string& class_name) {
    if (!node) return;

    if (node->type == NodeType::Output && node->left) {
        llvm::Value* val = gen_expr_in_method(node->left.get(), self_ptr, class_name);
        if (!val) { gen_stmt(node); return; }
        /* H2 Phase C: prefer annotation, fall back to LLVM type */
        {
            auto lk = get_annotation_kind(node->left.get());
            if (lk == AstTypeKind::Float || val->getType()->isDoubleTy())
                builder_->CreateCall(fn_print_float_, { val });
            else if (lk == AstTypeKind::String || val->getType()->isPointerTy())
                builder_->CreateCall(fn_print_str_, { val });
            else
                builder_->CreateCall(fn_printf_, { val });
        }
        return;
    }

    if (node->type == NodeType::Call) {
        /* obj.method or self.method or output(...) */
        auto dot = node->value.find('.');
        if (dot != std::string::npos) {
            std::string obj_name    = node->value.substr(0, dot);
            std::string method_name = node->value.substr(dot + 1);
            if (obj_name == "self" && !class_name.empty()) {
                auto gen_fn = [this, self_ptr, &class_name](const ASTNode* n) -> llvm::Value* {
                    return gen_expr_in_method(n, self_ptr, class_name);
                };
                oop_gen_method_call_ptr(ctx_, *builder_, *module_,
                    class_name, self_ptr, method_name, node->args.get(), node->src_line, gen_fn);
                return;
            }
            if (oop_is_object(obj_name)) {
                auto gen_fn = [this](const ASTNode* n) -> llvm::Value* { return gen_expr(n); };
                oop_gen_method_call(ctx_, *builder_, *module_,
                    obj_name, method_name, node->args.get(), node->src_line, gen_fn);
                return;
            }
        }
        gen_stmt(node);
        return;
    }

    /* Return with self-aware expr */
    if (node->type == NodeType::Return) {
        llvm::Value* val = nullptr;
        if (node->left)
            val = gen_expr_in_method(node->left.get(), self_ptr, class_name);
        if (!val) val = i64(0);
        if (val->getType()->isIntegerTy() && !val->getType()->isPointerTy())
            val = builder_->CreateIntToPtr(val, i8ptr_ty(), "ret_cast");
        builder_->CreateRet(val);
        return;
    }

    /* throw with self-aware expr */
    if (node->type == NodeType::Throw) {
        gen_expr_in_method(node->left.get(), self_ptr, class_name);
        return;
    }

    /* Assign with self-aware rhs */
    if (node->type == NodeType::Assign && node->left && node->right) {
        llvm::Value* val = gen_expr_in_method(node->right.get(), self_ptr, class_name);
        if (!val) val = i64(0);
        const std::string& name = node->left->value;
        VarRecord* rec = lookup_var(name);
        if (!rec) {
            auto* slot = create_entry_alloca(name, val->getType());
            builder_->CreateStore(val, slot);
            declare_var(name, slot, OwnershipState::Owned);
        } else {
            builder_->CreateStore(val, rec->alloca_ptr);
        }
        return;
    }

    gen_stmt(node);
}