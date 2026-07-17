#include "common/errors.hpp"
#include "compiler/codegen.hpp"
#include "compiler/class_oop.hpp"
#include "compiler/type_registry.hpp"

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>

/* ════════════════════════════════════════════════════════════
   Expression generators  (codegen_expr.cpp)
   ════════════════════════════════════════════════════════════ */

/* ════════════════════════════════════════════════════════════
   Cache helper — hoist aurora_str_from_cstr for string literals
   to the entry block so they aren't re-allocated inside loops.
   ════════════════════════════════════════════════════════════ */
llvm::Value* Codegen::get_literal_aurora(const std::string& str) {
    auto it = literal_aurora_cache_.find(str);
    if (it != literal_aurora_cache_.end())
        return it->second;

    /* Emit at the end of the entry block, before the terminator if one
       already exists (i.e. when called from inside a loop body). */
    auto& entry = cur_fn_->getEntryBlock();
    auto* term = entry.getTerminator();
    llvm::IRBuilder<> entry_builder(
        &entry, term ? term->getIterator() : entry.end());
    auto* raw = entry_builder.CreateGlobalStringPtr(str, "strlit_");
    auto* aurora = entry_builder.CreateCall(fn_str_from_cstr_, { raw }, "aurora_lit");

    /* Also cache the pointer to the AuroraStr so the codegen can GEP into
       it directly without re-loading the AuroraStr* every time. */
    literal_aurora_cache_[str] = aurora;
    return aurora;
}

llvm::Value* Codegen::gen_expr(const ASTNode* node) {
    if (!node) return i64(0);
    if (dibuilder_ && node->src_line > 0 && debug_cur_fn_)
        builder_->SetCurrentDebugLocation(
            llvm::DILocation::get(ctx_, node->src_line, node->src_col, debug_cur_fn_));
    switch (node->type) {
        case NodeType::Num:       return i64(std::stoll(node->value));
        case NodeType::Float:     return llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx_), std::stod(node->value));
        case NodeType::Str:       return get_literal_aurora(node->value);
        case NodeType::Var:       return gen_var(node);
        case NodeType::BinOp:     return gen_binop(node);
        case NodeType::UnaryOp:   return gen_unary(node);
        case NodeType::Call:      return gen_call(node);
        case NodeType::Index:     return gen_index(node);
        case NodeType::Array:     return gen_array(node);
        case NodeType::StructLiteral: return gen_struct_literal(node);
        case NodeType::Move:      return gen_move_expr(node);
        case NodeType::Copy:      return gen_copy_expr(node);
        case NodeType::SharedRef: return gen_shared_expr(node);
        case NodeType::WeakRef:   return gen_weak_expr(node);
        case NodeType::Borrow:    return gen_borrow_expr(node);
        case NodeType::Attribute: {
            /* request.xxx — HTTP request field access */
            if (node->left && node->left->type == NodeType::Var && node->left->value == "request") {
                VarRecord* http_req_rec = lookup_var("__http_req");
                if (!http_req_rec || !http_req_rec->alloca_ptr) return i64(0);
                llvm::Value* req_val = builder_->CreateLoad(i8ptr_ty(), http_req_rec->alloca_ptr, "req");
                llvm::Value* field_str = builder_->CreateGlobalStringPtr(node->value, "rfield");
                auto* fn_get_field = module_->getFunction("aurora_http_get_field");
                if (!fn_get_field) return i64(0);
                llvm::Value* result = builder_->CreateCall(fn_get_field, { req_val, field_str }, "field_val");
                auto* fn_from_cstr = module_->getFunction("aurora_str_from_cstr");
                if (fn_from_cstr && result)
                    result = builder_->CreateCall(fn_from_cstr, { result }, "field_aurora");
                return result ? result : i64(0);
            }
            /* request.params.xxx — HTTP route parameter access */
            if (node->left && node->left->type == NodeType::Attribute &&
                node->left->value == "params" && node->left->left &&
                node->left->left->type == NodeType::Var && node->left->left->value == "request") {
                VarRecord* http_req_rec = lookup_var("__http_req");
                if (!http_req_rec || !http_req_rec->alloca_ptr) return i64(0);
                llvm::Value* req_val = builder_->CreateLoad(i8ptr_ty(), http_req_rec->alloca_ptr, "req");
                llvm::Value* param_str = builder_->CreateGlobalStringPtr(node->value, "rparam");
                auto* fn_get_param = module_->getFunction("aurora_http_get_param");
                if (!fn_get_param) return i64(0);
                llvm::Value* result = builder_->CreateCall(fn_get_param, { req_val, param_str }, "param_val");
                auto* fn_from_cstr = module_->getFunction("aurora_str_from_cstr");
                if (fn_from_cstr && result)
                    result = builder_->CreateCall(fn_from_cstr, { result }, "param_aurora");
                return result ? result : i64(0);
            }
            /* request.query.xxx — HTTP query parameter access */
            if (node->left && node->left->type == NodeType::Attribute &&
                node->left->value == "query" && node->left->left &&
                node->left->left->type == NodeType::Var && node->left->left->value == "request") {
                VarRecord* http_req_rec = lookup_var("__http_req");
                if (!http_req_rec || !http_req_rec->alloca_ptr) return i64(0);
                llvm::Value* req_val = builder_->CreateLoad(i8ptr_ty(), http_req_rec->alloca_ptr, "req");
                llvm::Value* param_str = builder_->CreateGlobalStringPtr(node->value, "rqparam");
                auto* fn = module_->getFunction("aurora_http_get_query_param");
                if (!fn) return i64(0);
                llvm::Value* result = builder_->CreateCall(fn, { req_val, param_str }, "qparam_val");
                auto* fn_from_cstr = module_->getFunction("aurora_str_from_cstr");
                if (fn_from_cstr && result)
                    result = builder_->CreateCall(fn_from_cstr, { result }, "qparam_aurora");
                return result ? result : i64(0);
            }
            /* request.form.xxx — HTTP form field access */
            if (node->left && node->left->type == NodeType::Attribute &&
                node->left->value == "form" && node->left->left &&
                node->left->left->type == NodeType::Var && node->left->left->value == "request") {
                VarRecord* http_req_rec = lookup_var("__http_req");
                if (!http_req_rec || !http_req_rec->alloca_ptr) return i64(0);
                llvm::Value* req_val = builder_->CreateLoad(i8ptr_ty(), http_req_rec->alloca_ptr, "req");
                llvm::Value* param_str = builder_->CreateGlobalStringPtr(node->value, "rfparam");
                auto* fn = module_->getFunction("aurora_http_get_form_param");
                if (!fn) return i64(0);
                llvm::Value* result = builder_->CreateCall(fn, { req_val, param_str }, "fparam_val");
                auto* fn_from_cstr = module_->getFunction("aurora_str_from_cstr");
                if (fn_from_cstr && result)
                    result = builder_->CreateCall(fn_from_cstr, { result }, "fparam_aurora");
                return result ? result : i64(0);
            }
            /* request.cookie.xxx — HTTP cookie access */
            if (node->left && node->left->type == NodeType::Attribute &&
                node->left->value == "cookie" && node->left->left &&
                node->left->left->type == NodeType::Var && node->left->left->value == "request") {
                VarRecord* http_req_rec = lookup_var("__http_req");
                if (!http_req_rec || !http_req_rec->alloca_ptr) return i64(0);
                llvm::Value* req_val = builder_->CreateLoad(i8ptr_ty(), http_req_rec->alloca_ptr, "req");
                llvm::Value* param_str = builder_->CreateGlobalStringPtr(node->value, "rcparam");
                auto* fn = module_->getFunction("aurora_http_get_cookie");
                if (!fn) return i64(0);
                llvm::Value* result = builder_->CreateCall(fn, { req_val, param_str }, "cparam_val");
                auto* fn_from_cstr = module_->getFunction("aurora_str_from_cstr");
                if (fn_from_cstr && result)
                    result = builder_->CreateCall(fn_from_cstr, { result }, "cparam_aurora");
                return result ? result : i64(0);
            }
            /* obj.field read */
            const std::string& obj_name   = node->left ? node->left->value : "";
            const std::string& field_name = node->value;
            if (oop_is_object(obj_name)) {
                return oop_gen_field_get(ctx_, *builder_, obj_name, field_name, node->src_line);
            }
            /* Extern struct field access */
            VarRecord* rec = lookup_var(obj_name);
            if (rec && !rec->struct_type.empty()) {
                llvm::StructType* st = codegen_get_struct_type(ctx_, rec->struct_type);
                if (st) {
                    int idx = codegen_struct_field_index(rec->struct_type, field_name);
                    if (idx >= 0) {
                        llvm::Value* struct_ptr = builder_->CreateLoad(llvm::PointerType::get(ctx_, 0), rec->alloca_ptr, obj_name);
                        llvm::Value* field_ptr = builder_->CreateStructGEP(st, struct_ptr, idx, field_name);
                        llvm::Type* field_type = st->getElementType(idx);
                        return builder_->CreateLoad(field_type, field_ptr, obj_name + "." + field_name);
                    }
                }
            }
            /* Fallback: p.field where p is a function parameter (i64 boxed pointer).
               Search all registered classes for one with this field name. */
            if (rec) {
                std::string cls_name = global_class_registry().find_class_by_field(field_name);
                if (!cls_name.empty()) {
                    llvm::Value* boxed = builder_->CreateLoad(i64_ty(), rec->alloca_ptr, obj_name);
                    llvm::Value* ptr = builder_->CreateIntToPtr(boxed, i8ptr_ty(), obj_name + "_ptr");
                    return oop_gen_self_field_get(ctx_, *builder_, ptr, cls_name, field_name, node->src_line);
                }
            }
            return i64(0);
        }
        case NodeType::Lambda: {
            /* Inline lambda expression — create LLVM function, return i8* fn ptr */
            if (!node->captures.empty()) {
                global_diag().warn(node->src_line, "inline lambda with captures not yet supported, returning 0");
                return i64(0);
            }

            /* Build param types — prefer annotation, fall back to i8* */
            std::vector<llvm::Type*> param_types;
            const ASTNode* p = node->args.get();
            while (p) {
                param_types.push_back(ast_kind_to_abi_type(ctx_, p->type_annotation.kind, i8ptr_ty()));
                p = p->next.get();
            }

            auto lambda_ret_kind = get_annotation_kind(node);
            auto* lambda_ret_ty  = ast_kind_to_abi_type(ctx_, lambda_ret_kind, i8ptr_ty());
            auto* fn_type = llvm::FunctionType::get(lambda_ret_ty, param_types, false);
            auto* fn = llvm::Function::Create(
                fn_type, llvm::Function::InternalLinkage,
                node->value.empty() ? "_lambda_expr" : node->value, module_.get());

            /* Name params */
            int ai = 0;
            for (auto& arg : fn->args()) {
                std::string pname = "_p" + std::to_string(ai++);
                const ASTNode* pp = node->args.get();
                for (int i = 0; i < ai - 1 && pp; i++) pp = pp->next.get();
                if (pp) pname = pp->value;
                arg.setName(pname);
            }

            /* Emit debug info for the lambda function */
            llvm::DISubprogram* lambda_sp = nullptr;
            if (dibuilder_) {
                llvm::SmallVector<llvm::Metadata*, 4> param_dbg_types;
                param_dbg_types.push_back(get_debug_type(lambda_ret_kind));
                const ASTNode* pp = node->args.get();
                while (pp) {
                    param_dbg_types.push_back(get_debug_type(pp->type_annotation.kind));
                    pp = pp->next.get();
                }
                auto* subr_ty = dibuilder_->createSubroutineType(
                    dibuilder_->getOrCreateTypeArray(param_dbg_types));
                lambda_sp = dibuilder_->createFunction(
                    debug_file_, fn->getName(), fn->getName(), debug_file_,
                    node->src_line, subr_ty, node->src_line,
                    llvm::DINode::FlagPrototyped,
                    llvm::DISubprogram::SPFlagDefinition);
                fn->setSubprogram(lambda_sp);
            }

            auto* entry_bb = llvm::BasicBlock::Create(ctx_, "entry", fn);
            auto* saved_fn = cur_fn_;
            auto* saved_bb = builder_->GetInsertBlock();
            auto saved_scopes = std::move(scopes_);
            auto saved_cache = std::move(literal_aurora_cache_);
            auto* saved_debug_fn = debug_cur_fn_;
            scopes_.clear();

            cur_fn_ = fn;
            debug_cur_fn_ = lambda_sp;
            builder_->SetInsertPoint(entry_bb);
            push_scope();

            /* Set DILocation for lambda body if debug enabled */
            if (dibuilder_ && lambda_sp) {
                auto* loc = llvm::DILocation::get(ctx_, node->src_line, node->src_col, lambda_sp);
                builder_->SetCurrentDebugLocation(loc);
            }

            /* Allocate and store params */
            ai = 0;
            p = node->args.get();
            while (p) {
                auto* slot = create_entry_alloca(p->value, i64_ty());
                llvm::Value* val = fn->getArg(ai++);
                if (val->getType() != i64_ty())
                    val = builder_->CreatePtrToInt(val, i64_ty(), p->value + "_unbox");
                builder_->CreateStore(val, slot);
                declare_var(p->value, slot, OwnershipState::Owned, p->type_annotation.kind);
                p = p->next.get();
            }

            gen_block(node->body.get());
            pop_scope_and_drop();
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

            literal_aurora_cache_ = std::move(saved_cache);
            cur_fn_ = saved_fn;
            debug_cur_fn_ = saved_debug_fn;
            scopes_ = std::move(saved_scopes);
            if (saved_bb) builder_->SetInsertPoint(saved_bb);

            return builder_->CreateBitCast(fn, i8ptr_ty(), "lambda_ptr");
        }
        default: {
            global_diag().warn(node->src_line, "unhandled expression node type " + std::to_string(static_cast<int>(node->type)) + ", returning 0");
            return i64(0);
        }
    }
}

llvm::Value* Codegen::gen_var(const ASTNode* node) {
    VarRecord* rec = lookup_var(node->value);
    if (!rec || !rec->alloca_ptr) {
        llvm::Function* fn = module_->getFunction(node->value);
        if (fn) return builder_->CreatePtrToInt(fn, i64_ty(), node->value + "_fnptr");
        return i64(0);
    }

    /* Weak var — must lock before use */
    if (rec->state == OwnershipState::Weak) {
        /* LLVM 18 opaque pointers: lock returns ptr, load i64 from it */
        llvm::Value* locked = emit_weak_lock(rec->alloca_ptr);
        /* null check — if locked is null, return 0 (simplified panic) */
        llvm::Value* is_null = builder_->CreateICmpEQ(
            builder_->CreatePtrToInt(locked, i64_ty(), "lock_as_int"),
            i64(0), "is_null");
        /* Load value only if non-null */
        auto* live_bb = llvm::BasicBlock::Create(
            locked->getContext(), "weak_live", cur_fn_);
        auto* dead_bb = llvm::BasicBlock::Create(
            locked->getContext(), "weak_dead", cur_fn_);
        auto* merge_bb = llvm::BasicBlock::Create(
            locked->getContext(), "weak_merge", cur_fn_);
        builder_->CreateCondBr(is_null, dead_bb, live_bb);
        builder_->SetInsertPoint(live_bb);
        llvm::Value* loaded = builder_->CreateLoad(i64_ty(), rec->alloca_ptr, "weak_val");
        safe_br(merge_bb);
        builder_->SetInsertPoint(dead_bb);
        safe_br(merge_bb);
        builder_->SetInsertPoint(merge_bb);
        auto* phi = builder_->CreatePHI(i64_ty(), 2, "weak_result");
        phi->addIncoming(loaded, live_bb);
        phi->addIncoming(i64(0), dead_bb);
        return phi;
    }

    /* Load with the actual allocated type */
    auto* alloca_inst = llvm::dyn_cast<llvm::AllocaInst>(rec->alloca_ptr);
    auto* global_var = llvm::dyn_cast<llvm::GlobalVariable>(rec->alloca_ptr);
    llvm::Type* alloc_ty;
    bool is_arena_slot = false;
    if (alloca_inst) {
        alloc_ty = alloca_inst->getAllocatedType();
    } else if (global_var) {
        alloc_ty = global_var->getValueType();
        /* GlobalVariable may store i64 even for pointer-typed values; rebox if needed */
    } else {
        /* Arena/GC heap slot — always i64 (see gen_allocation_for_var) */
        alloc_ty = i64_ty();
        is_arena_slot = true;
    }
    llvm::Value* loaded = builder_->CreateLoad(alloc_ty, rec->alloca_ptr, node->value);
    /* Rebox i64 → ptr for pointer-typed variables stored in i64 slots */
    if (is_arena_slot && (rec->type_kind == AstTypeKind::String ||
                          rec->type_kind == AstTypeKind::Array ||
                          rec->type_kind == AstTypeKind::Struct ||
                          rec->type_kind == AstTypeKind::Pointer ||
                          rec->type_kind == AstTypeKind::Class))
        return builder_->CreateIntToPtr(loaded, i8ptr_ty(), node->value + "_ptr");
    return loaded;
}

bool Codegen::expr_is_string_type(const ASTNode* node) {
    /* H2 Phase C: prefer type_annotation over structural checks */
    auto k = get_annotation_kind(node);
    if (k != AstTypeKind::Unknown)
        return k == AstTypeKind::String;
    /* Fallback: structural checks for nodes not yet annotated */
    if (!node) return false;
    switch (node->type) {
        case NodeType::Str:  return true;
        case NodeType::Num:
        case NodeType::Float: return false;
        case NodeType::Var: {
            VarRecord* rec = lookup_var(node->value);
            return rec && rec->type_kind == AstTypeKind::String;
        }
        case NodeType::Call: {
            auto dot = node->value.find('.');
            if (dot == std::string::npos)
                return global_string_fns().count(node->value) > 0;
            std::string obj_name    = node->value.substr(0, dot);
            std::string method_name = node->value.substr(dot + 1);
            std::string cls_name = oop_class_of(obj_name);
            if (cls_name.empty()) {
                VarRecord* rec = lookup_var(obj_name);
                if (rec)
                    cls_name = global_class_registry().find_class_by_method(method_name);
            }
            if (cls_name.empty()) return true;
            const ClassMethodInfo* mi = global_class_registry().find_method(cls_name, method_name);
            return mi ? mi->return_kind == AstTypeKind::String : true;
        }
        case NodeType::Attribute: {
            if (node->left && node->left->value == "self") {
                std::string cls_name = oop_class_of("__self__");
                if (!cls_name.empty()) {
                    const ClassFieldInfo* fi = global_class_registry().find_field(cls_name, node->value);
                    if (fi) {
                        return fi->type_kind == AstTypeKind::String;
                    }
                    return true;
                }
            }
            return true;
        }
        case NodeType::Index:
            return false;
        default:
            return true;
    }
}

llvm::Value* Codegen::gen_binop(const ASTNode* node) {
    const std::string& op = node->value;

    /* String concatenation — handle Str literals and Var holding ptr */
    if (op == "+") {
        /* Generate both sides first to check types */
        llvm::Value* L = nullptr;
        llvm::Value* R = nullptr;

        if (node->left->type == NodeType::Str) {
            L = get_literal_aurora(node->left->value);
        } else {
            L = gen_expr(node->left.get());
        }

        if (node->right->type == NodeType::Str) {
            R = get_literal_aurora(node->right->value);
        } else {
            R = gen_expr(node->right.get());
        }

        if (!L) L = i64(0);
        if (!R) R = i64(0);

        /* If annotation says string, or either side is a pointer → string concat */
        if (get_annotation_kind(node) == AstTypeKind::String ||
            L->getType()->isPointerTy() || R->getType()->isPointerTy()) {
            /* Non-pointer operands: if the expression is a string type → re-box pointer;
               if it's an integer type → convert to string representation */
            if (!L->getType()->isPointerTy()) {
                if (node->left && expr_is_string_type(node->left.get()))
                    L = builder_->CreateIntToPtr(L, i8ptr_ty(), "rebox_l");
                else
                    L = builder_->CreateCall(fn_int_to_str_, { L }, "l_int_str");
            }
            if (!R->getType()->isPointerTy()) {
                if (node->right && expr_is_string_type(node->right.get()))
                    R = builder_->CreateIntToPtr(R, i8ptr_ty(), "rebox_r");
                else
                    R = builder_->CreateCall(fn_int_to_str_, { R }, "r_int_str");
            }

            /* ── String concat — append R to L with exponential buffer growth ── */
            auto* result = builder_->CreateCall(fn_str_append_, { L, R }, "strcat_result");
            return result;
        }

        /* Both numeric — fall through to normal Add below */
        llvm::Value* Lv = L, *Rv = R;
        /* H2 Phase C: prefer annotation for float detection, fall back to LLVM type */
        bool is_float = (get_annotation_kind(node) == AstTypeKind::Float) ||
                         Lv->getType()->isDoubleTy() || Rv->getType()->isDoubleTy();
        if (is_float) {
            auto* dbl = llvm::Type::getDoubleTy(ctx_);
            if (!Lv->getType()->isDoubleTy()) {
                if (get_annotation_kind(node->left.get()) == AstTypeKind::Float)
                    Lv = builder_->CreateBitCast(Lv, dbl, "fp_unbox");
                else
                    Lv = builder_->CreateSIToFP(Lv, dbl, "itof");
            }
            if (!Rv->getType()->isDoubleTy()) {
                if (get_annotation_kind(node->right.get()) == AstTypeKind::Float)
                    Rv = builder_->CreateBitCast(Rv, dbl, "fp_unbox");
                else
                    Rv = builder_->CreateSIToFP(Rv, dbl, "itof");
            }
            return builder_->CreateFAdd(Lv, Rv, "fadd");
        }
        return builder_->CreateAdd(Lv, Rv, "add", false, true);
    }

    llvm::Value* L = gen_expr(node->left.get());
    llvm::Value* R = gen_expr(node->right.get());

    /* H2 Phase C: prefer annotation for float detection, fall back to LLVM type */
    bool is_float = (get_annotation_kind(node) == AstTypeKind::Float) ||
                     L->getType()->isDoubleTy() || R->getType()->isDoubleTy();
    if (is_float) {
        auto* dbl = llvm::Type::getDoubleTy(ctx_);
        if (!L->getType()->isDoubleTy()) {
            if (get_annotation_kind(node->left.get()) == AstTypeKind::Float)
                L = builder_->CreateBitCast(L, dbl, "fp_unbox");
            else
                L = builder_->CreateSIToFP(L, dbl, "itof");
        }
        if (!R->getType()->isDoubleTy()) {
            if (get_annotation_kind(node->right.get()) == AstTypeKind::Float)
                R = builder_->CreateBitCast(R, dbl, "fp_unbox");
            else
                R = builder_->CreateSIToFP(R, dbl, "itof");
        }

        if (op == "+")  return builder_->CreateFAdd(L, R, "fadd");
        if (op == "-")  return builder_->CreateFSub(L, R, "fsub");
        if (op == "*")  return builder_->CreateFMul(L, R, "fmul");
        if (op == "/")  return builder_->CreateFDiv(L, R, "fdiv");
        if (op == "==") return builder_->CreateZExt(builder_->CreateFCmpOEQ(L,R,"feq"),  i64_ty());
        if (op == "!=") return builder_->CreateZExt(builder_->CreateFCmpONE(L,R,"fne"),  i64_ty());
        if (op == "<")  return builder_->CreateZExt(builder_->CreateFCmpOLT(L,R,"flt"),  i64_ty());
        if (op == ">")  return builder_->CreateZExt(builder_->CreateFCmpOGT(L,R,"fgt"),  i64_ty());
        if (op == "<=") return builder_->CreateZExt(builder_->CreateFCmpOLE(L,R,"flte"), i64_ty());
        if (op == ">=") return builder_->CreateZExt(builder_->CreateFCmpOGE(L,R,"fgte"), i64_ty());
        return i64(0);
    }

    /* Type unification: if operands differ, cast both to i64 uniformly */
    if (L->getType() != R->getType()) {
        if (L->getType()->isPointerTy())
            L = builder_->CreatePtrToInt(L, i64_ty(), "l_ptoi");
        if (R->getType()->isPointerTy())
            R = builder_->CreatePtrToInt(R, i64_ty(), "r_ptoi");
    }

    /* Integer arithmetic (nsw = signed overflow UB, safe for Aurora) */
    if (op == "+")  return builder_->CreateAdd (L, R, "add", false, true);
    if (op == "-")  return builder_->CreateSub (L, R, "sub", false, true);
    if (op == "*")  return builder_->CreateMul (L, R, "mul", false, true);
    if (op == "/")  return builder_->CreateSDiv(L, R, "div");
    if (op == "%")  return builder_->CreateSRem(L, R, "rem");
    if (op == "**") {
        auto* result_slot = create_entry_alloca("__pow_result", i64_ty());
        builder_->CreateStore(i64(1), result_slot);
        auto* exp_slot = create_entry_alloca("__pow_exp", i64_ty());
        builder_->CreateStore(R, exp_slot);
        auto* cond_bb = llvm::BasicBlock::Create(ctx_, "pow_cond", cur_fn_);
        auto* body_bb = llvm::BasicBlock::Create(ctx_, "pow_body", cur_fn_);
        auto* exit_bb = llvm::BasicBlock::Create(ctx_, "pow_exit", cur_fn_);
        safe_br(cond_bb);
        builder_->SetInsertPoint(cond_bb);
        auto* exp_val = builder_->CreateLoad(i64_ty(), exp_slot, "exp");
        builder_->CreateCondBr(builder_->CreateICmpSGT(exp_val, i64(0)), body_bb, exit_bb);
        builder_->SetInsertPoint(body_bb);
        auto* cur = builder_->CreateLoad(i64_ty(), result_slot, "cur");
        builder_->CreateStore(builder_->CreateMul(cur, L, "pow_mul", false, true), result_slot);
        builder_->CreateStore(builder_->CreateSub(exp_val, i64(1), "pow_dec", false, true), exp_slot);
        safe_br(cond_bb);
        builder_->SetInsertPoint(exit_bb);
        return builder_->CreateLoad(i64_ty(), result_slot, "pow_result");
    }
    if (op == "//") return builder_->CreateSDiv(L, R, "floordiv");
    if (op == "==") return builder_->CreateZExt(builder_->CreateICmpEQ (L,R,"eq"),  i64_ty());
    if (op == "!=") return builder_->CreateZExt(builder_->CreateICmpNE (L,R,"ne"),  i64_ty());
    if (op == "<")  return builder_->CreateZExt(builder_->CreateICmpSLT(L,R,"lt"),  i64_ty());
    if (op == ">")  return builder_->CreateZExt(builder_->CreateICmpSGT(L,R,"gt"),  i64_ty());
    if (op == "<=") return builder_->CreateZExt(builder_->CreateICmpSLE(L,R,"lte"), i64_ty());
    if (op == ">=") return builder_->CreateZExt(builder_->CreateICmpSGE(L,R,"gte"), i64_ty());
    if (op == "and") return builder_->CreateAnd(L, R, "and");
    if (op == "or")  return builder_->CreateOr (L, R, "or");
    if (op == "^" || op == "xor") return builder_->CreateXor(L, R, "xor");
    if (op == "&") return builder_->CreateAnd(L, R, "band");
    if (op == "|") return builder_->CreateOr (L, R, "bor");
    if (op == "in") {
        /* Check if L is a member of collection R (array for now) */
        return builder_->CreateCall(fn_array_contains_int_, { R, L }, "contains");
    }
    return i64(0);
}

llvm::Value* Codegen::gen_unary(const ASTNode* node) {
    llvm::Value* val = gen_expr(node->left.get());
    if (node->value == "-") {
        /* H2 Phase C: prefer annotation for float negation, fall back to LLVM type */
        if (get_annotation_kind(node->left.get()) == AstTypeKind::Float ||
            val->getType()->isDoubleTy())
            return builder_->CreateFNeg(val, "neg");
#if LLVM_VERSION_MAJOR >= 18
        return builder_->CreateNSWNeg(val, "neg");
#else
        return builder_->CreateNeg(val, "neg", false, true);
#endif
    }
    if (node->value == "not") {
        if (val->getType()->isIntegerTy(1))
            return builder_->CreateNot(val, "not");
        auto* is_zero = builder_->CreateICmpEQ(val, i64(0), "not");
        return builder_->CreateZExt(is_zero, i64_ty(), "not_ext");
    }
    return val;
}

llvm::Value* Codegen::gen_call(const ASTNode* node) {
    /* Helper: convert i64 value to double for float output if annotation says float */
    auto to_float_val = [&](llvm::Value* v, const ASTNode* n) -> llvm::Value* {
        if (v && !v->getType()->isDoubleTy() && get_annotation_kind(n) == AstTypeKind::Float)
            return builder_->CreateBitCast(v, llvm::Type::getDoubleTy(ctx_), "fp_val");
        return v;
    };

    /* Built-in: output(expr) — redirect to gen_output logic */
    if (node->value == "output" && node->args) {
        const ASTNode* arg = node->args.get();
        /* H2 Phase C: prefer annotation over node type check */
        if (get_annotation_kind(arg) == AstTypeKind::Float &&
            arg->type == NodeType::Float) {
            double fval = std::stod(arg->value);
            llvm::Value* fv = llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx_), fval);
            builder_->CreateCall(fn_print_float_, { fv });
        } else if (arg->type == NodeType::Var) {
            VarRecord* rec = lookup_var(arg->value);
            bool is_str = get_annotation_kind(arg) == AstTypeKind::String;
            if (rec && is_str) {
                llvm::Type* alloc_ty = [&]() -> llvm::Type* {
                    if (auto* ai = llvm::dyn_cast<llvm::AllocaInst>(rec->alloca_ptr))
                        return ai->getAllocatedType();
                    if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(rec->alloca_ptr))
                        return gv->getValueType();
                    return i8ptr_ty();
                }();
                llvm::Value* val = builder_->CreateLoad(alloc_ty, rec->alloca_ptr, arg->value);
                if (alloc_ty->isIntegerTy())
                    val = builder_->CreateIntToPtr(val, i8ptr_ty(), arg->value + "_str");
                builder_->CreateCall(fn_print_str_, { val });
            } else {
                llvm::Value* val = to_float_val(gen_expr(arg), arg);
                if (!val) val = i64(0);
                /* H2 Phase C: prefer annotation, fall back to LLVM type */
                auto ak = get_annotation_kind(arg);
                if (ak == AstTypeKind::Float || val->getType()->isDoubleTy())
                    builder_->CreateCall(fn_print_float_, { val });
                else if (ak == AstTypeKind::String || val->getType()->isPointerTy())
                    builder_->CreateCall(fn_print_str_, { val });
                else
                    builder_->CreateCall(fn_printf_, { val });
            }
        } else {
            llvm::Value* val = gen_expr(arg);
            if (!val) val = i64(0);
            auto ak = get_annotation_kind(arg);
            if (ak == AstTypeKind::Float || val->getType()->isDoubleTy()) {
                if (val->getType()->isIntegerTy())
                    val = builder_->CreateBitCast(val, llvm::Type::getDoubleTy(ctx_), "fp_val");
                builder_->CreateCall(fn_print_float_, { val });
            } else if (get_annotation_kind(arg) == AstTypeKind::String ||
                       val->getType()->isPointerTy())
                builder_->CreateCall(fn_print_str_, { val });
            else
                builder_->CreateCall(fn_printf_, { val });
        }
        return i64(0);
    }

    /* Built-in: reserve(arr, cap) */
    if (node->value == "reserve" && node->args) {
        const ASTNode* arg1 = node->args.get();
        if (arg1 && arg1->next) {
            const ASTNode* arg2 = arg1->next.get();
            llvm::Value* arr_val = gen_expr(arg1);
            llvm::Value* cap_val = gen_expr(arg2);
            if (arr_val && cap_val) {
                if (cap_val->getType() != i64_ty() && cap_val->getType()->isIntegerTy())
                    cap_val = builder_->CreateZExt(cap_val, i64_ty());
                builder_->CreateCall(fn_array_reserve_, { arr_val, cap_val });
            }
        }
        return i64(0);
    }

    /* ── request("field") — get HTTP request field ── */
    if (node->value == "request") {
        VarRecord* http_req_rec = lookup_var("__http_req");
        if (!http_req_rec || !http_req_rec->alloca_ptr) return i64(0);
        llvm::Value* req_val = builder_->CreateLoad(i8ptr_ty(), http_req_rec->alloca_ptr, "req");
        llvm::Value* field_val = llvm::ConstantPointerNull::get(i8ptr_ty());
        if (node->args) {
            field_val = gen_expr(node->args.get());
            if (field_val && field_val->getType() != i8ptr_ty())
                field_val = builder_->CreateIntToPtr(field_val, i8ptr_ty(), "field_ptr");
            if (field_val) {
                auto* as_cstr = module_->getFunction("aurora_str_as_cstr");
                if (as_cstr)
                    field_val = builder_->CreateCall(as_cstr, { field_val }, "field_cstr");
            }
        }
        auto* fn_get_field = module_->getFunction("aurora_http_get_field");
        if (!fn_get_field) return i64(0);
        llvm::Value* result = builder_->CreateCall(fn_get_field, { req_val, field_val ? field_val : llvm::ConstantPointerNull::get(i8ptr_ty()) }, "field_val");
        /* Wrap the const char* result back into an AuroraStr* */
        auto* fn_from_cstr = module_->getFunction("aurora_str_from_cstr");
        if (fn_from_cstr && result)
            result = builder_->CreateCall(fn_from_cstr, { result }, "field_aurora");
        return result ? result : i64(0);
    }

    /* ── json(expr) — set response body with JSON content type ── */
    if (node->value == "json") {
        VarRecord* http_res_rec = lookup_var("__http_res");
        if (!http_res_rec || !http_res_rec->alloca_ptr) return i64(0);
        llvm::Value* res_val = builder_->CreateLoad(i8ptr_ty(), http_res_rec->alloca_ptr, "res");
        llvm::Value* body = node->args ? gen_expr(node->args.get()) : i64(0);
        if (body) {
            if (body->getType() != i8ptr_ty())
                body = builder_->CreateIntToPtr(body, i8ptr_ty(), "body_ptr");
            auto* as_cstr = module_->getFunction("aurora_str_as_cstr");
            if (as_cstr)
                body = builder_->CreateCall(as_cstr, { body }, "json_cstr");
        }
        auto* fn_set_json = module_->getFunction("aurora_http_response_set_json");
        if (fn_set_json && body)
            builder_->CreateCall(fn_set_json, { res_val, body });
        return i64(0);
    }

    /* ── status(code) — set HTTP response status code ── */
    if (node->value == "status") {
        VarRecord* http_res_rec = lookup_var("__http_res");
        if (!http_res_rec || !http_res_rec->alloca_ptr) return i64(0);
        llvm::Value* res_val = builder_->CreateLoad(i8ptr_ty(), http_res_rec->alloca_ptr, "res");
        llvm::Value* code = node->args ? gen_expr(node->args.get()) : i64(200);
        auto* fn_set_status = module_->getFunction("aurora_http_response_set_status_code");
        if (fn_set_status)
            builder_->CreateCall(fn_set_status, { res_val, code });
        return i64(0);
    }

    /* ── html(expr) — set response body with text/html content type ── */
    if (node->value == "html") {
        VarRecord* http_res_rec = lookup_var("__http_res");
        if (!http_res_rec || !http_res_rec->alloca_ptr) return i64(0);
        llvm::Value* res_val = builder_->CreateLoad(i8ptr_ty(), http_res_rec->alloca_ptr, "res");
        llvm::Value* body = node->args ? gen_expr(node->args.get()) : i64(0);
        llvm::Value* body_len = i64(0);
        if (body) {
            if (body->getType() != i8ptr_ty())
                body = builder_->CreateIntToPtr(body, i8ptr_ty(), "body_ptr");
            auto* strlen_fn = module_->getFunction("aurora_strlen");
            if (strlen_fn)
                body_len = builder_->CreateCall(strlen_fn, { body }, "body_len");
            auto* as_cstr = module_->getFunction("aurora_str_as_cstr");
            if (as_cstr)
                body = builder_->CreateCall(as_cstr, { body }, "body_cstr");
        }
        auto* fn_set_body_n = module_->getFunction("aurora_http_response_set_body_n");
        auto* fn_set_ct = module_->getFunction("aurora_http_response_set_content_type");
        if (fn_set_body_n && body) {
            builder_->CreateCall(fn_set_body_n, { res_val, body, body_len });
            if (fn_set_ct)
                builder_->CreateCall(fn_set_ct, { res_val, builder_->CreateGlobalStringPtr("text/html") });
        }
        return i64(0);
    }

    /* ── redirect(url, code) — set HTTP redirect response ── */
    if (node->value == "redirect") {
        VarRecord* http_res_rec = lookup_var("__http_res");
        if (!http_res_rec || !http_res_rec->alloca_ptr) return i64(0);
        llvm::Value* res_val = builder_->CreateLoad(i8ptr_ty(), http_res_rec->alloca_ptr, "res");
        llvm::Value* url = node->args ? gen_expr(node->args.get()) : i64(0);
        llvm::Value* code = (node->args && node->args->next) ? gen_expr(node->args->next.get()) : i64(302);
        if (url) {
            if (url->getType() != i8ptr_ty())
                url = builder_->CreateIntToPtr(url, i8ptr_ty(), "url_ptr");
            auto* as_cstr = module_->getFunction("aurora_str_as_cstr");
            if (as_cstr)
                url = builder_->CreateCall(as_cstr, { url }, "url_cstr");
        }
        auto* fn_redirect = module_->getFunction("aurora_http_response_redirect");
        if (fn_redirect)
            builder_->CreateCall(fn_redirect, { res_val, url ? url : llvm::ConstantPointerNull::get(i8ptr_ty()), code });
        return i64(0);
    }

    /* ── content_type(type) — set HTTP response Content-Type header ── */
    if (node->value == "content_type") {
        VarRecord* http_res_rec = lookup_var("__http_res");
        if (!http_res_rec || !http_res_rec->alloca_ptr) return i64(0);
        llvm::Value* res_val = builder_->CreateLoad(i8ptr_ty(), http_res_rec->alloca_ptr, "res");
        llvm::Value* ct = node->args ? gen_expr(node->args.get()) : i64(0);
        if (ct) {
            if (ct->getType() != i8ptr_ty())
                ct = builder_->CreateIntToPtr(ct, i8ptr_ty(), "ct_ptr");
            auto* as_cstr = module_->getFunction("aurora_str_as_cstr");
            if (as_cstr)
                ct = builder_->CreateCall(as_cstr, { ct }, "ct_cstr");
        }
        auto* fn_set_ct = module_->getFunction("aurora_http_response_set_content_type");
        if (fn_set_ct && ct)
            builder_->CreateCall(fn_set_ct, { res_val, ct });
        return i64(0);
    }

    /* ── Phase 2: Collection type constructors ── */
    {
        /* Helper: i64 → i8* for collection API calls */
        auto i64_to_ptr = [&](llvm::Value* v) -> llvm::Value* {
            if (v->getType()->isPointerTy()) return v;
            return builder_->CreateIntToPtr(v, i8ptr_ty(), "i64toptr");
        };
        /* Helper: i64 → i32 for list_get index */
        auto i64_to_i32 = [&](llvm::Value* v) -> llvm::Value* {
            if (v->getType()->isIntegerTy(32)) return v;
            return builder_->CreateTrunc(v, llvm::Type::getInt32Ty(ctx_), "i64toi32");
        };
        /* Helper: convert arg to double */
        auto to_double = [&](llvm::Value* v) -> llvm::Value* {
            auto* dbl = llvm::Type::getDoubleTy(ctx_);
            if (v->getType()->isDoubleTy()) return v;
            return builder_->CreateSIToFP(v, dbl, "itod");
        };

        if (node->value == "list") {
            auto* ctor = module_->getFunction("list_new");
            auto* push = module_->getFunction("list_push");
            if (ctor) {
                llvm::Value* ret = builder_->CreateCall(ctor, {}, "list");
                llvm::Value* ptr = i64_to_ptr(ret);
                const ASTNode* arg = node->args.get();
                while (arg && push) {
                    llvm::Value* val = gen_expr(arg);
                    if (!val) val = i64(0);
                    builder_->CreateCall(push, { ptr, val });
                    arg = arg->next.get();
                }
                return builder_->CreatePtrToInt(ptr, i64_ty(), "list_boxed");
            }
            return i64(0);
        }

        if (node->value == "map") {
            auto* ctor = module_->getFunction("map_new");
            auto* set = module_->getFunction("map_set");
            if (ctor) {
                llvm::Value* ret = builder_->CreateCall(ctor, {}, "map");
                llvm::Value* ptr = i64_to_ptr(ret);
                const ASTNode* arg = node->args.get();
                while (arg && arg->next && set) {
                    llvm::Value* key = gen_expr(arg);
                    arg = arg->next.get();
                    llvm::Value* val = gen_expr(arg);
                    if (!key) key = i64(0);
                    if (!val) val = i64(0);
                    if (!key->getType()->isPointerTy())
                        key = builder_->CreateIntToPtr(key, i8ptr_ty(), "key_as_ptr");
                    builder_->CreateCall(set, { ptr, key, val });
                    arg = arg->next.get();
                }
                return builder_->CreatePtrToInt(ptr, i64_ty(), "map_boxed");
            }
            return i64(0);
        }

        if (node->value == "set") {
            auto* ctor = module_->getFunction("set_new");
            auto* add = module_->getFunction("set_add");
            if (ctor) {
                llvm::Value* ret = builder_->CreateCall(ctor, {}, "set");
                llvm::Value* ptr = i64_to_ptr(ret);
                const ASTNode* arg = node->args.get();
                while (arg && add) {
                    llvm::Value* val = gen_expr(arg);
                    if (!val) val = i64(0);
                    builder_->CreateCall(add, { ptr, val });
                    arg = arg->next.get();
                }
                return builder_->CreatePtrToInt(ptr, i64_ty(), "set_boxed");
            }
            return i64(0);
        }

        if (node->value == "stack") {
            auto* ctor = module_->getFunction("stack_new");
            auto* spush = module_->getFunction("stack_push");
            if (ctor) {
                llvm::Value* ret = builder_->CreateCall(ctor, {}, "stack");
                llvm::Value* ptr = i64_to_ptr(ret);
                const ASTNode* arg = node->args.get();
                while (arg && spush) {
                    llvm::Value* val = gen_expr(arg);
                    if (!val) val = i64(0);
                    builder_->CreateCall(spush, { ptr, val });
                    arg = arg->next.get();
                }
                return builder_->CreatePtrToInt(ptr, i64_ty(), "stack_boxed");
            }
            return i64(0);
        }

        if (node->value == "queue") {
            auto* ctor = module_->getFunction("queue_new");
            auto* enq = module_->getFunction("queue_enqueue");
            if (ctor) {
                llvm::Value* ret = builder_->CreateCall(ctor, {}, "queue");
                llvm::Value* ptr = i64_to_ptr(ret);
                const ASTNode* arg = node->args.get();
                while (arg && enq) {
                    llvm::Value* val = gen_expr(arg);
                    if (!val) val = i64(0);
                    builder_->CreateCall(enq, { ptr, val });
                    arg = arg->next.get();
                }
                return builder_->CreatePtrToInt(ptr, i64_ty(), "queue_boxed");
            }
            return i64(0);
        }

        if (node->value == "json") {
            auto* ctor = module_->getFunction("json_new");
            auto* jset = module_->getFunction("json_set");
            if (ctor) {
                llvm::Value* ret = builder_->CreateCall(ctor, {}, "json");
                llvm::Value* ptr = i64_to_ptr(ret);
                const ASTNode* arg = node->args.get();
                while (arg && arg->next && jset) {
                    llvm::Value* key = gen_expr(arg);
                    arg = arg->next.get();
                    llvm::Value* val = gen_expr(arg);
                    if (!key) key = i64(0);
                    if (!val) val = i64(0);
                    if (!key->getType()->isPointerTy())
                        key = builder_->CreateIntToPtr(key, i8ptr_ty(), "jkey_as_ptr");
                    builder_->CreateCall(jset, { ptr, key, val });
                    arg = arg->next.get();
                }
                return builder_->CreatePtrToInt(ptr, i64_ty(), "json_boxed");
            }
            return i64(0);
        }

        if (node->value == "vector") {
            auto* ctor = module_->getFunction("vector_new");
            if (ctor) {
                auto* dbl_ty = llvm::Type::getDoubleTy(ctx_);
                llvm::Value* xv = llvm::ConstantFP::get(dbl_ty, 0.0);
                llvm::Value* yv = llvm::ConstantFP::get(dbl_ty, 0.0);
                llvm::Value* zv = llvm::ConstantFP::get(dbl_ty, 0.0);
                const ASTNode* arg = node->args.get();
                if (arg) { xv = to_double(gen_expr(arg)); arg = arg->next.get(); }
                if (arg) { yv = to_double(gen_expr(arg)); arg = arg->next.get(); }
                if (arg) { zv = to_double(gen_expr(arg)); arg = arg->next.get(); }
                llvm::Value* ret = builder_->CreateCall(ctor, { xv, yv, zv }, "vector");
                return builder_->CreatePtrToInt(ret, i64_ty(), "vector_boxed");
            }
            return i64(0);
        }

        if (node->value == "array") {
            llvm::Value* arr = gen_array(node);
            return arr;
        }

        if (node->value == "tuple") {
            /* tuple is same as array for now */
            llvm::Value* arr = gen_array(node);
            return arr;
        }
    }

    /* ── panic(msg) — halt with message ── */
    if (node->value == "panic") {
        auto* fn_panic = module_->getFunction("aurora_panic");
        auto* fn_as_cstr = module_->getFunction("aurora_str_as_cstr");
        if (fn_panic) {
            llvm::Value* msg = i64(0);
            if (node->args) {
                msg = gen_expr(node->args.get());
                if (msg) {
                    if (!msg->getType()->isPointerTy())
                        msg = builder_->CreateIntToPtr(msg, i8ptr_ty(), "panic_msg");
                    else if (fn_as_cstr)
                        msg = builder_->CreateCall(fn_as_cstr, { msg }, "panic_cstr");
                }
            }
            builder_->CreateCall(fn_panic, { msg ? msg : i64(0) });
        }
        builder_->CreateUnreachable();
        auto* dead_bb = llvm::BasicBlock::Create(ctx_, "panic_dead", cur_fn_);
        builder_->SetInsertPoint(dead_bb);
        return i64(0);
    }

    /* ── debug(msg) / log(msg) — output with prefix ── */
    if (node->value == "debug" || node->value == "log") {
        const char* prefix = (node->value == "debug") ? "[DEBUG] " : "[LOG] ";
        if (node->args) {
            llvm::Value* msg_val = gen_expr(node->args.get());
            if (msg_val) {
                if (msg_val->getType()->isPointerTy()) {
                    builder_->CreateCall(fn_print_str_, {
                        builder_->CreateCall(fn_str_from_cstr_,
                            builder_->CreateGlobalStringPtr(prefix, node->value + "_pfx"),
                            node->value + "_pfx_obj") });
                    builder_->CreateCall(fn_print_str_, { msg_val });
                } else {
                    builder_->CreateCall(fn_print_str_, {
                        builder_->CreateCall(fn_str_from_cstr_,
                            builder_->CreateGlobalStringPtr(prefix, node->value + "_pfx"),
                            node->value + "_pfx_obj") });
                    if (msg_val->getType()->isDoubleTy())
                        builder_->CreateCall(fn_print_float_, { msg_val });
                    else
                        builder_->CreateCall(fn_printf_, { msg_val });
                }
            }
        }
        return i64(0);
    }

    /* ── Built-in functions (handled via codegen_builtins.cpp) ── */
    {
        auto gen_expr_fn = [this](const ASTNode* n) -> llvm::Value* {
            return gen_expr(n);
        };
        llvm::Value* builtin_result = codegen_builtin_call(
            node->value, node, *builder_, ctx_, builtins_, gen_expr_fn, module_.get());
        if (builtin_result) return builtin_result;
    }

    /* obj.method(args) — method call */
    {
        auto dot = node->value.find('.');
        if (dot != std::string::npos) {
            std::string obj_name    = node->value.substr(0, dot);
            std::string method_name = node->value.substr(dot + 1);
            if (oop_is_object(obj_name)) {
                auto gen_expr_fn = [this](const ASTNode* n) -> llvm::Value* {
                    return gen_expr(n);
                };
                /* Check if this is a self.field access inside a method */
                return oop_gen_method_call(ctx_, *builder_, *module_,
                    obj_name, method_name,
                    node->args.get(), node->src_line, gen_expr_fn);
            }
            /* Fallback: p.method() where p is a function param (i64 boxed ptr) */
            {
                VarRecord* rec = lookup_var(obj_name);
                if (rec) {
                    std::string cls_name = global_class_registry().find_class_by_method(method_name);
                    if (!cls_name.empty()) {
                        auto gen_expr_fn = [this](const ASTNode* n) -> llvm::Value* {
                            return gen_expr(n);
                        };
                        llvm::Value* boxed = builder_->CreateLoad(i64_ty(), rec->alloca_ptr, obj_name);
                        llvm::Value* ptr = builder_->CreateIntToPtr(boxed, i8ptr_ty(), obj_name + "_ptr");
                        return oop_gen_method_call_ptr(ctx_, *builder_, *module_,
                            cls_name, ptr, method_name,
                            node->args.get(), node->src_line, gen_expr_fn);
                    }
                }
            }
        }
    }

    /* For generic calls, use the mangled function name */
    std::string callee_name = node->value;
    if (node->template_args) {
        const ASTNode* ta = node->template_args.get();
        while (ta) {
            callee_name += "__" + ta->value;
            ta = ta->next.get();
        }
    }

    /* ── Struct construction: Box[Int](42) ── */
    if (node->template_args && global_type_registry().has_struct(callee_name)) {
        llvm::StructType* st = codegen_get_struct_type(ctx_, callee_name);
        if (!st) return i64(0);
        llvm::Value* ptr = codegen_struct_alloca(ctx_, *builder_, cur_fn_, callee_name, callee_name + ".inst");
        const ASTNode* f = node->args.get();
        unsigned idx = 0;
        while (f && idx < st->getNumElements()) {
            llvm::Value* fval = gen_expr(f);
            if (fval) {
                llvm::Value* fptr = builder_->CreateStructGEP(st, ptr, idx);
                llvm::Type* field_ty = st->getElementType(idx);
                if (fval->getType() != field_ty) {
                    if (fval->getType()->isIntegerTy() && field_ty->isFloatingPointTy())
                        fval = builder_->CreateSIToFP(fval, field_ty, "field_itof");
                    else if (fval->getType()->isFloatingPointTy() && field_ty->isIntegerTy())
                        fval = builder_->CreateFPToSI(fval, field_ty, "field_ftoi");
                    else if (fval->getType()->isIntegerTy() && field_ty->isIntegerTy()) {
                        if (fval->getType()->getIntegerBitWidth() > field_ty->getIntegerBitWidth())
                            fval = builder_->CreateTrunc(fval, field_ty, "field_trunc");
                        else
                            fval = builder_->CreateZExt(fval, field_ty, "field_zext");
                    }
                }
                builder_->CreateStore(fval, fptr);
            }
            idx++;
            f = f->next.get();
        }
        return ptr;
    }

    llvm::Function* callee = module_->getFunction(callee_name);
    if (!callee) {
        /* Not a direct function — check if it's a lambda/closure variable */
        VarRecord* rec = lookup_var(node->value);
        if (rec) {
            if (rec->is_closure) return gen_closure_call(node, rec);
            else                 return gen_fnptr_call(node, rec);
        }
        return i64(0);
    }

    /* Collect arguments — handle struct by value + callbacks */
    std::vector<llvm::Value*> args;
    const ASTNode* arg = node->args.get();
    int pidx = 0;

    /* Check if callee has callbacks that need trampolines */
    auto cb_it = extern_callback_sigs_.find(node->value);
    const std::vector<CallbackSig>* cb_sigs = (cb_it != extern_callback_sigs_.end()) ? &cb_it->second : nullptr;
    size_t cb_next = 0;

    /* Check if callee has cstring params that need Aurora string → char* conversion */
    auto str_it = extern_string_info_.find(node->value);
    const ExternStringInfo* str_info = (str_it != extern_string_info_.end()) ? &str_it->second : nullptr;
    size_t str_next = 0;

    llvm::FunctionType* ft = callee->getFunctionType();
    unsigned num_fixed_params = ft->getNumParams();
    while (arg) {
        bool is_vararg_param = (args.size() >= num_fixed_params && ft->isVarArg());
        bool param_in_range = (args.size() < num_fixed_params);
        llvm::Type* param_ty = (is_vararg_param || !param_in_range) ? nullptr : ft->getParamType(args.size());

        /* Check if this param is a callback that needs a trampoline */
        bool is_callback = false;
        if (cb_sigs) {
            while (cb_next < cb_sigs->size() && (*cb_sigs)[cb_next].index < pidx)
                cb_next++;
            if (cb_next < cb_sigs->size() && (*cb_sigs)[cb_next].index == pidx) {
                is_callback = true;
            }
        }

        /* Check if this param is a cstring that needs Aurora string → char* conversion */
        bool is_cstring = false;
        if (str_info) {
            while (str_next < str_info->param_indices.size() && str_info->param_indices[str_next] < pidx)
                str_next++;
            if (str_next < str_info->param_indices.size() && str_info->param_indices[str_next] == pidx) {
                is_cstring = true;
            }
        }

        /* Generate arg value: string literal for cstring bypasses heap alloc */
        llvm::Value* v = nullptr;
        if (is_cstring && arg->type == NodeType::Str) {
            v = builder_->CreateGlobalStringPtr(arg->value, "strlit_");
        } else {
            v = gen_expr(arg);
        }
        if (!v) v = i64(0);

        if (is_callback) {
            /* Generate trampoline: a real C-callable wrapper that calls the Aurora function ref */
            llvm::FunctionType* cb_fn_type = (*cb_sigs)[cb_next].fn_type;
            std::string tramp_name = node->value + "_tramp_" + std::to_string(cb_next);
            std::string global_name = node->value + "_cb_" + std::to_string(cb_next);

            /* Create global to hold the Aurora callback ref */
            llvm::GlobalVariable* cb_global = module_->getGlobalVariable(global_name);
            if (!cb_global) {
                auto* cb_init = llvm::ConstantInt::get(i64_ty(), 0, true);
                cb_global = new llvm::GlobalVariable(
                    *module_, i64_ty(), false,
                    llvm::GlobalValue::InternalLinkage,
                    cb_init, global_name);
            }

            /* Create trampoline function if not already created */
            llvm::Function* tramp = module_->getFunction(tramp_name);
            if (!tramp) {
                tramp = llvm::Function::Create(
                    cb_fn_type, llvm::Function::InternalLinkage, tramp_name, module_.get());
                tramp->setCallingConv(llvm::CallingConv::C);
                auto* saved = builder_->GetInsertBlock();
                auto* entry = llvm::BasicBlock::Create(ctx_, "entry", tramp);
                builder_->SetInsertPoint(entry);
                llvm::Value* cb_ref = builder_->CreateLoad(i64_ty(), cb_global, "cb_ref");
                auto* cb_ptr_ty = llvm::PointerType::get(cb_fn_type, 0);
                llvm::Value* cb_fn = builder_->CreateIntToPtr(cb_ref, cb_ptr_ty, "cb_fn");
                std::vector<llvm::Value*> cb_args;
                for (auto& ca : tramp->args())
                    cb_args.push_back(&ca);
                llvm::CallInst* cb_result = builder_->CreateCall(cb_fn_type, cb_fn, cb_args);
                if (cb_fn_type->getReturnType()->isVoidTy())
                    builder_->CreateRetVoid();
                else
                    builder_->CreateRet(cb_result);
                if (saved) builder_->SetInsertPoint(saved);
            }

            /* Store Aurora callback ref to global */
            builder_->CreateStore(v, cb_global);

            /* Pass trampoline function pointer */
            v = tramp;
            cb_next++;
        }
        /* C string: auto-convert to char* */
        /* For string literals: create GlobalStringPtr directly (already done above) */
        /* For non-literal Aurora strings: use aurora_str_as_cstr (zero-copy, no allocation) */
        else if (is_cstring && !(arg->type == NodeType::Str) && (v->getType()->isPointerTy() || v->getType()->isIntegerTy())) {
            auto* as_cstr = module_->getFunction("aurora_str_as_cstr");
            if (!as_cstr)
                as_cstr = llvm::Function::Create(
                    llvm::FunctionType::get(i8ptr_ty(), { i8ptr_ty() }, false),
                    llvm::Function::ExternalLinkage, "aurora_str_as_cstr", module_.get());
            llvm::Value* ptr_v = v;
            if (ptr_v->getType()->isIntegerTy())
                ptr_v = builder_->CreateIntToPtr(v, i8ptr_ty(), "aurora_str_as_i8ptr");
            v = builder_->CreateCall(as_cstr, { ptr_v }, "cstr_view");
            str_next++;
        } else if (is_cstring) {
            str_next++;
        }
        /* Struct by value: load struct from pointer if param expects struct */
        else if (!is_vararg_param && param_ty->isStructTy() && v->getType()->isPointerTy()) {
            v = builder_->CreateLoad(param_ty, v, "struct_val");
        }
        /* Integer size conversion (i64 ↔ i32, etc.) */
        else if (!is_vararg_param && param_ty->isIntegerTy() && v->getType()->isIntegerTy()) {
            if (param_ty->getIntegerBitWidth() < v->getType()->getIntegerBitWidth())
                v = builder_->CreateTrunc(v, param_ty, "arg_trunc");
            else if (param_ty->getIntegerBitWidth() > v->getType()->getIntegerBitWidth())
                v = builder_->CreateZExt(v, param_ty, "arg_zext");
        }
        /* Regular pointer cast */
        else if (!is_vararg_param && param_ty->isPointerTy() && v->getType()->isIntegerTy())
            v = builder_->CreateIntToPtr(v, param_ty, "arg_cast");
        else if (!is_vararg_param && param_ty->isIntegerTy() && v->getType()->isPointerTy())
            v = builder_->CreatePtrToInt(v, param_ty, "arg_cast");
        /* Float ↔ integer conversion */
        else if (!is_vararg_param && param_ty->isFloatingPointTy() && v->getType()->isIntegerTy())
            v = builder_->CreateSIToFP(v, param_ty, "arg_itof");
        else if (!is_vararg_param && param_ty->isIntegerTy() && v->getType()->isFloatingPointTy())
            v = builder_->CreateFPToSI(v, param_ty, "arg_ftoi");

        args.push_back(v);
        pidx++;
        arg = arg->next.get();
    }

    /* Check arg count: for vararg, allow extra args; for regular, must match exactly */
    if (!ft->isVarArg() && args.size() != callee->arg_size()) return i64(0);
    if (ft->isVarArg() && args.size() < callee->arg_size()) return i64(0);

    llvm::Type* ret_ty = ft->getReturnType();
    std::string call_nm = ret_ty->isVoidTy() ? "" : ("call_" + node->value);
    llvm::CallInst* ret_inst = builder_->CreateCall(callee, args, call_nm);
    ret_inst->setCallingConv(callee->getCallingConv());
    llvm::Value* ret = ret_inst;

    if (ret_ty->isStructTy()) {
        /* struct return — keep as LLVM struct value, no conversion */
    }
    /* C string return: wrap char* in Aurora string via aurora_str_from_cstr */
    else if (str_info && str_info->return_is_cstring && ret->getType()->isPointerTy()) {
        if (!fn_str_from_cstr_) {
            fn_str_from_cstr_ = module_->getFunction("aurora_str_from_cstr");
            if (!fn_str_from_cstr_)
                fn_str_from_cstr_ = llvm::Function::Create(
                    llvm::FunctionType::get(i8ptr_ty(), { i8ptr_ty() }, false),
                    llvm::Function::ExternalLinkage, "aurora_str_from_cstr", module_.get());
        }
        ret = builder_->CreateCall(fn_str_from_cstr_, { ret }, "cstr_wrap");
    }
    /* Non-string pointer return: unbox boxed object to i64 (Aurora internal convention) */
    else if (ret->getType()->isPointerTy()) {
        bool is_str_fn = (global_string_fns().count(node->value) > 0);
        if (!is_str_fn)
            ret = builder_->CreatePtrToInt(ret, i64_ty(), "ret_unbox");
    }

    /* Floating-point return: bitcast to i64 for Aurora's internal representation */
    else if (ret->getType()->isFloatingPointTy())
        ret = builder_->CreateBitCast(ret, i64_ty(), "ret_fp_unbox");

    /* Extend smaller integer types to i64 for Aurora's internal representation */
    else if (ret->getType()->isIntegerTy() && ret->getType()->getIntegerBitWidth() < 64)
        ret = builder_->CreateZExt(ret, i64_ty(), "ret_zext");

    return ret;
}

/* ── arr[idx] — element access ── */
llvm::Value* Codegen::gen_index(const ASTNode* node) {
    VarRecord* rec = lookup_var(node->value);
    if (!rec || !rec->alloca_ptr) return i64(0);

    /* Load the array handle (stored as i64 — runtime uses i64 handle, not ptr) */
    llvm::Value* arr_ptr = builder_->CreateLoad(i64_ty(), rec->alloca_ptr,
                                                node->value + "_arr");
    /* Evaluate index */
    llvm::Value* idx = gen_expr(node->left.get());

    /* Get element tag to decide which getter to call */
    llvm::Value* tag = builder_->CreateCall(fn_array_get_tag_, { arr_ptr, idx }, "tag");

    /* Build a tag-based dispatch:
       tag==0 → get_int, tag==1 → get_float (cast to i64), tag==2 → str (ptr as i64)
       Simple approach: always return i64; floats lose precision here,
       full dispatch would use PHI node (done in Phase 3 with proper value type) */
    llvm::Value* int_val = builder_->CreateCall(fn_array_get_int_, { arr_ptr, idx }, "elem_int");

    /* tag == 2 or 4 → get string pointer as i64 */
    auto* is_str2 = builder_->CreateICmpEQ(tag, i64(2), "is_str2");
    auto* is_str4 = builder_->CreateICmpEQ(tag, i64(4), "is_str4");
    auto* is_str  = builder_->CreateOr(is_str2, is_str4, "is_str");
    auto* str_ptr_raw = builder_->CreateCall(fn_array_get_str_, { arr_ptr, idx }, "elem_str");
    auto* str_as_i64  = builder_->CreatePtrToInt(str_ptr_raw, i64_ty(), "str_as_i64");

    /* tag == 1 → float: get_float then bitcast to i64 for uniform repr */
    auto* is_flt  = builder_->CreateICmpEQ(tag, i64(1), "is_flt");
    auto* flt_val = builder_->CreateCall(fn_array_get_flt_, { arr_ptr, idx }, "elem_flt");
    auto* flt_bits= builder_->CreateBitCast(flt_val, i64_ty(), "flt_bits");

    /* Select: if str → str_as_i64, if float → flt_bits, else int_val */
    llvm::Value* result = builder_->CreateSelect(is_flt, flt_bits,  int_val, "sel_flt");
    result              = builder_->CreateSelect(is_str, str_as_i64, result,  "sel_str");

    return result;
}

/* ── gen_array — [e1, e2, ...] → AuroraArray* as i64 ── */
llvm::Value* Codegen::gen_array(const ASTNode* node) {
    /* Count elements */
    int64_t cap = 0;
    { const ASTNode* el = node->args.get(); while (el) { cap++; el = el->next.get(); } }

    llvm::Value* arr = builder_->CreateCall(fn_array_new_, { i64(cap) }, "arr");

    const ASTNode* el = node->args.get();
    while (el) {
        /* H2 Phase C: prefer annotation over node type / LLVM type checks */
        auto ek = get_annotation_kind(el);
        if (ek == AstTypeKind::String) {
            auto* sp = builder_->CreateGlobalStringPtr(el->value, "arr_s");
            builder_->CreateCall(fn_array_push_str_, { arr, sp });
        } else if (ek == AstTypeKind::Float) {
            double fv = std::stod(el->value);
            llvm::Value* fval = llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx_), fv);
            builder_->CreateCall(fn_array_push_flt_, { arr, fval });
        } else if (ek == AstTypeKind::Array) {
            llvm::Value* nested_arr = gen_array(el);
            builder_->CreateCall(fn_array_push_arr_, { arr, nested_arr });
        } else {
            llvm::Value* val = gen_expr(el);
            if (!val) val = i64(0);
            /* Fall back to LLVM type if annotation is unknown */
            if (ek == AstTypeKind::Float || val->getType()->isDoubleTy())
                builder_->CreateCall(fn_array_push_flt_, { arr, val });
            else
                builder_->CreateCall(fn_array_push_int_, { arr, val });
        }
        el = el->next.get();
    }
    return arr;
}

/* ── move x used as expression (e.g. y = move x) ── */
llvm::Value* Codegen::gen_move_expr(const ASTNode* node) {
    VarRecord* rec = lookup_var(node->value);
    if (!rec || !rec->alloca_ptr) return i64(0);

    llvm::Value* val = builder_->CreateLoad(i64_ty(), rec->alloca_ptr, "moved_val");

    /* Poison source */
    emit_poison(rec->alloca_ptr);
    rec->state = OwnershipState::Moved;

    return val;
}

/* ── copy x used as expression (y = copy x) ── */
llvm::Value* Codegen::gen_copy_expr(const ASTNode* node) {
    /* In expression context the var name is stored in node->value
       (set by parse_expr), not in node->left->value.               */
    const std::string& name = node->value;
    VarRecord* rec = lookup_var(name);
    if (!rec || !rec->alloca_ptr) return i64(0);

    if (get_annotation_kind(node) == AstTypeKind::Array) {
        llvm::Value* arr = builder_->CreateLoad(i64_ty(), rec->alloca_ptr, name + "_cexpr_src");

        llvm::Function* fn_copy = module_->getFunction("aurora_array_copy");
        if (!fn_copy) {
            auto* fty = llvm::FunctionType::get(i64_ty(), {i64_ty()}, false);
            fn_copy = llvm::Function::Create(fty, llvm::Function::ExternalLinkage,
                                             "aurora_array_copy", module_.get());
        }
        return builder_->CreateCall(fn_copy, {arr}, name + "_copy_result");
    }

    /* Scalar: load and return — source stays valid */
    return builder_->CreateLoad(i64_ty(), rec->alloca_ptr, name + "_copy_val");
}

/* ── shared x used as expression (y = shared x) ── */
llvm::Value* Codegen::gen_shared_expr(const ASTNode* node) {
    VarRecord* rec = lookup_var(node->value);
    if (!rec || !rec->alloca_ptr) return i64(0);

    /* Increment strong refcount */
    emit_refcount_inc(rec->alloca_ptr);
    rec->state = OwnershipState::Shared;

    /* Return the pointer value as i64 (the caller stores it in a new slot) */
    return builder_->CreateLoad(i64_ty(), rec->alloca_ptr, "shared_val");
}

/* ── weak x used as expression (y = weak x) ── */
llvm::Value* Codegen::gen_weak_expr(const ASTNode* node) {
    VarRecord* rec = lookup_var(node->value);
    if (!rec || !rec->alloca_ptr) return i64(0);
    rec->state = OwnershipState::Weak;
    return builder_->CreateLoad(i64_ty(), rec->alloca_ptr, "weak_val");
}

/* ── borrow x used as expression (f(borrow x)) ── */
llvm::Value* Codegen::gen_borrow_expr(const ASTNode* node) {
    VarRecord* rec = lookup_var(node->value);
    if (!rec || !rec->alloca_ptr) return i64(0);
    /* Return the raw value — no ownership change */
    return builder_->CreateLoad(i64_ty(), rec->alloca_ptr, "borrow_val");
}

/* ── struct literal: Point { x: 1, y: 2 } ── */
llvm::Value* Codegen::gen_struct_literal(const ASTNode* node) {
    if (!node) return i64(0);
    llvm::StructType* st = codegen_get_struct_type(ctx_, node->value);
    if (!st) return i64(0);
    llvm::Value* ptr = codegen_struct_alloca(ctx_, *builder_, cur_fn_, node->value, node->value + ".lit");
    const ASTNode* f = node->args.get();
    while (f) {
        int idx = codegen_struct_field_index(node->value, f->value);
        if (idx >= 0 && f->left) {
            llvm::Value* fptr = builder_->CreateStructGEP(st, ptr, idx, f->value);
            llvm::Value* fval = gen_expr(f->left.get());
            if (fval) {
                llvm::Type* field_ty = st->getElementType(idx);
                if (fval->getType() != field_ty) {
                    if (fval->getType()->isIntegerTy() && field_ty->isIntegerTy()) {
                        unsigned src_bits = fval->getType()->getIntegerBitWidth();
                        unsigned dst_bits = field_ty->getIntegerBitWidth();
                        if (src_bits > dst_bits)
                            fval = builder_->CreateTrunc(fval, field_ty, "trunc_" + f->value);
                        else
                            fval = builder_->CreateZExt(fval, field_ty, "ext_" + f->value);
                    }
                }
                builder_->CreateStore(fval, fptr);
            }
        }
        f = f->next.get();
    }
    return ptr;
}

/* ── Call a closure variable: load closure struct, extract fn ptr + env, call ── */
llvm::Value* Codegen::gen_closure_call(const ASTNode* node, VarRecord* rec) {
    auto* closure_ty = llvm::StructType::get(ctx_, { i8ptr_ty(), i8ptr_ty() }, false);

    /* Load the closure value and normalise to a {ptr, ptr}* pointer.
       When the closure is stored locally as a struct (AllocaInst of
       closure_ty), use the alloca pointer directly. When it comes from
       a function return (i64 boxed pointer), load + inttoptr. */
    llvm::Value* closure_ptr = rec->alloca_ptr;
    if (auto* ai = llvm::dyn_cast<llvm::AllocaInst>(closure_ptr)) {
        if (ai->getAllocatedType() != closure_ty) {
            /* Boxed pointer storage: load i64 then inttoptr */
            llvm::Value* boxed = builder_->CreateLoad(i64_ty(), closure_ptr, node->value + "_boxed");
            closure_ptr = builder_->CreateIntToPtr(boxed, llvm::PointerType::get(closure_ty, 0), node->value + "_closure_ptr");
        }
    } else {
        /* Not an alloca — assume boxed pointer (e.g. GlobalVariable or fn arg) */
        llvm::Value* boxed = builder_->CreateLoad(i64_ty(), closure_ptr, node->value + "_boxed");
        closure_ptr = builder_->CreateIntToPtr(boxed, llvm::PointerType::get(closure_ty, 0), node->value + "_closure_ptr");
    }

    /* Extract function pointer */
    auto* fn_ptr_gep = builder_->CreateStructGEP(closure_ty, closure_ptr, 0, "cfn_ptr");
    llvm::Value* fn_ptr = builder_->CreateLoad(i8ptr_ty(), fn_ptr_gep, "cfn");

    /* Extract env pointer */
    auto* env_gep = builder_->CreateStructGEP(closure_ty, closure_ptr, 1, "cenv");
    llvm::Value* env_ptr = builder_->CreateLoad(i8ptr_ty(), env_gep, "cenv");

    /* H3-F: derive return type from annotation, fall back to i8* */
    auto closure_ret_kind = get_annotation_kind(node);
    auto* ret_ty = ast_kind_to_abi_type(ctx_, closure_ret_kind, i8ptr_ty());

    /* Collect args: env first, then regular args */
    std::vector<llvm::Value*> args;
    std::vector<llvm::Type*> param_types;
    args.push_back(env_ptr);
    param_types.push_back(i8ptr_ty());  /* env is always i8* */
    const ASTNode* arg = node->args.get();
    while (arg) {
        llvm::Value* v = gen_expr(arg);
        if (!v) v = i64(0);
        /* Closure user params use i8* ABI (matching lambda generation defaults) */
        param_types.push_back(i8ptr_ty());
        if (v->getType() != i8ptr_ty()) {
            if (v->getType()->isPointerTy()) {
                /* already a pointer — keep */
            } else if (v->getType()->isIntegerTy()) {
                v = builder_->CreateIntToPtr(v, i8ptr_ty(), "arg_closure_cast");
            } else if (v->getType()->isDoubleTy()) {
                v = builder_->CreateBitCast(v, i64_ty(), "arg_closure_fp_box");
                v = builder_->CreateIntToPtr(v, i8ptr_ty(), "arg_closure_cast");
            }
        }
        args.push_back(v);
        arg = arg->next.get();
    }

    auto* fn_call_type = llvm::FunctionType::get(ret_ty, param_types, false);

    llvm::CallInst* call = builder_->CreateCall(fn_call_type, fn_ptr, args, "closure_call");
    call->setTailCallKind(llvm::CallInst::TCK_NoTail);
    llvm::Value* result = call;
    /* Normalize return to uniform representation */
    if (ret_ty->isPointerTy()) {
        if (closure_ret_kind != AstTypeKind::String)
            result = builder_->CreatePtrToInt(result, i64_ty(), "closure_ret");
    } else if (ret_ty->isDoubleTy()) {
        result = builder_->CreateBitCast(result, i64_ty(), "closure_ret_fp");
    } else if (ret_ty->isIntegerTy() && ret_ty->getIntegerBitWidth() < 64) {
        result = builder_->CreateZExt(result, i64_ty(), "closure_ret_zext");
    }
    return result;
}

/* ── Call a non-capturing lambda stored as a function pointer variable ── */
llvm::Value* Codegen::gen_fnptr_call(const ASTNode* node, VarRecord* rec) {
    /* Function parameters are stored as i64 in allocas; load i64 then cast to ptr */
    llvm::Value* fn_int = builder_->CreateLoad(i64_ty(), rec->alloca_ptr, node->value + "_fn_int");
    llvm::Value* fn_ptr = builder_->CreateIntToPtr(fn_int, i8ptr_ty(), node->value + "_fn");

    /* H3-F: derive return type from annotation, fall back to i8* */
    auto fnptr_ret_kind = get_annotation_kind(node);
    auto* ret_ty = ast_kind_to_abi_type(ctx_, fnptr_ret_kind, i8ptr_ty());

    std::vector<llvm::Value*> args;
    std::vector<llvm::Type*> param_types;
    const ASTNode* arg = node->args.get();
    while (arg) {
        llvm::Value* v = gen_expr(arg);
        if (!v) v = i64(0);
        /* Fnptr params use i8* ABI (matching function pointer generation defaults) */
        param_types.push_back(i8ptr_ty());
        if (v->getType() != i8ptr_ty()) {
            if (v->getType()->isPointerTy()) {
                /* already i8* or compatible pointer — keep */
            } else if (v->getType()->isIntegerTy()) {
                v = builder_->CreateIntToPtr(v, i8ptr_ty(), "arg_fnptr_cast");
            } else if (v->getType()->isDoubleTy()) {
                v = builder_->CreateBitCast(v, i64_ty(), "arg_fnptr_fp_box");
                v = builder_->CreateIntToPtr(v, i8ptr_ty(), "arg_fnptr_cast");
            }
        }
        args.push_back(v);
        arg = arg->next.get();
    }

    auto* fn_call_type = llvm::FunctionType::get(ret_ty, param_types, false);

    llvm::CallInst* call = builder_->CreateCall(fn_call_type, fn_ptr, args, node->value + "_call");
    call->setTailCallKind(llvm::CallInst::TCK_NoTail);
    llvm::Value* result = call;
    /* Normalize return to uniform representation */
    if (ret_ty->isPointerTy()) {
        if (fnptr_ret_kind != AstTypeKind::String)
            result = builder_->CreatePtrToInt(result, i64_ty(), node->value + "_ret");
    } else if (ret_ty->isDoubleTy()) {
        result = builder_->CreateBitCast(result, i64_ty(), node->value + "_ret_fp");
    } else if (ret_ty->isIntegerTy() && ret_ty->getIntegerBitWidth() < 64) {
        result = builder_->CreateZExt(result, i64_ty(), node->value + "_ret_zext");
    }
    return result;
}