#include "compiler/codegen.hpp"
#include "compiler/class_oop.hpp"

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>

/* ════════════════════════════════════════════════════════════
   Function / Class / Method generation  (codegen_function.cpp)
   ════════════════════════════════════════════════════════════ */

/* ── function fname(params): body ── */
void Codegen::gen_function(const ASTNode* node) {
    /* Build param type list — use i64 for all params (values stored as i64 internally) */
    std::vector<std::string>    param_names;
    std::vector<llvm::Type*>    param_types;
    const ASTNode* p = node->args.get();
    while (p) {
        param_names.push_back(p->value);
        param_types.push_back(i64_ty());
        p = p->next.get();
    }

    auto* fn_type = llvm::FunctionType::get(i8ptr_ty(), param_types, false);
    auto* fn      = llvm::Function::Create(
        fn_type, llvm::Function::ExternalLinkage,
        node->value, module_.get());

    /* Name the LLVM arguments and add optimization attributes */
    int ai = 0;
    for (auto& arg : fn->args()) {
        arg.setName(param_names[ai]);
        arg.addAttr(llvm::Attribute::NoCapture);
        if (arg.getType()->isPointerTy())
            arg.addAttr(llvm::Attribute::NoAlias);
        ai++;
    }

    auto* entry_bb  = llvm::BasicBlock::Create(ctx_, "entry", fn);
    auto* saved_fn  = cur_fn_;
    auto* saved_bb  = builder_->GetInsertBlock();
    auto  saved_scopes = std::move(scopes_);
    auto  saved_cache  = std::move(literal_aurora_cache_);
    scopes_.clear();

    cur_fn_ = fn;
    builder_->SetInsertPoint(entry_bb);

    push_scope();

    /* Allocate params as i64 (for arithmetic) */
    ai = 0;
    for (auto& arg : fn->args()) {
        auto* slot = create_entry_alloca(param_names[ai], i64_ty());
        builder_->CreateStore(&arg, slot);
        declare_var(param_names[ai], slot, OwnershipState::Owned);
        ai++;
    }

    gen_block(node->body.get());
    pop_scope_and_drop();

    /* Default return: null pointer */
    safe_ret(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(i8ptr_ty())));

    /* Restore caller context */
    scopes_ = std::move(saved_scopes);
    literal_aurora_cache_ = std::move(saved_cache);
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

            /* self pointer + declared params */
            std::vector<llvm::Type*> param_types;
            param_types.push_back(llvm::PointerType::getUnqual(ctx_)); /* self */
            const ASTNode* param = stmt->args.get();
            while (param) {
                if (param->value != "self")
                    param_types.push_back(i8ptr_ty());
                param = param->next.get();
            }

            auto* fn_ty = llvm::FunctionType::get(i8ptr_ty(), param_types, false);
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
                    declare_var(param->value, slot, OwnershipState::Owned);
                    ++ai;
                }
                param = param->next.get();
            }

            /* Determine method return type from the method body */
            if (ClassInfo* mutable_cls = global_class_registry().get_mut(class_name)) {
                for (auto& m : mutable_cls->methods) {
                    if (m.name == stmt->value) {
                        /* Walk the method body for Return nodes */
                        const ASTNode* b = stmt->body.get();
                        while (b) {
                            if (b->type == NodeType::Return && b->left) {
                                /* Check return expression type */
                                if (b->left->type == NodeType::Attribute && b->left->left &&
                                    b->left->left->value == "self") {
                                    /* return self.<field> */
                                    const ClassFieldInfo* fi = global_class_registry().find_field(class_name, b->left->value);
                                    m.returns_string = fi ? fi->is_string : true;
                                } else if (b->left->type == NodeType::Str) {
                                    m.returns_string = true;
                                } else if (b->left->type == NodeType::Num) {
                                    m.returns_string = false;
                                }
                                /* Only check the first return statement */
                                break;
                            }
                            b = b->next.get();
                        }
                        break;
                    }
                }
            }

            /* Walk method body */
            const ASTNode* body = stmt->body.get();
            while (body) {
                gen_stmt_in_method(body, self_ptr, class_name);
                body = body->next.get();
            }

            if (!current_block_terminated())
                builder_->CreateRet(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(i8ptr_ty())));

            pop_scope_and_drop();
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
            if (a->getType()->isDoubleTy() || b->getType()->isDoubleTy()) {
                auto* dbl = llvm::Type::getDoubleTy(ctx_);
                if (!a->getType()->isDoubleTy()) a = builder_->CreateSIToFP(a, dbl);
                if (!b->getType()->isDoubleTy()) b = builder_->CreateSIToFP(b, dbl);
            }
        };

        /* Arithmetic operators */
        if (op == "+") {
            if (L->getType()->isPointerTy() || R->getType()->isPointerTy()) {
                /* string concat — convert non-string to string, allocate new */
                if (!L->getType()->isPointerTy())
                    L = builder_->CreateCall(fn_int_to_str_, { L }, "l_int_str");
                if (!R->getType()->isPointerTy())
                    R = builder_->CreateCall(fn_int_to_str_, { R }, "r_int_str");
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
        if (val->getType()->isDoubleTy())
            builder_->CreateCall(fn_print_float_, { val });
        else if (val->getType()->isPointerTy())
            builder_->CreateCall(fn_print_str_, { val });
        else
            builder_->CreateCall(fn_printf_, { val });
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
