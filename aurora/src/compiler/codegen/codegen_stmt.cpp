#include "common/errors.hpp"

#include "compiler/codegen.hpp"
#include "compiler/class_oop.hpp"
#include "compiler/type_registry.hpp"

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <iostream>

/* ════════════════════════════════════════════════════════════
   Statement generators  (codegen_stmt.cpp)
   ════════════════════════════════════════════════════════════ */

/* ── x = expr ── */
void Codegen::gen_assign(const ASTNode* node) {
    if (!node->left || !node->right) return;

    /* obj.field = expr — OOP field write */
    if (node->left->type == NodeType::Attribute && node->left->left) {
        const std::string& obj_name   = node->left->left->value;
        const std::string& field_name = node->left->value;
        if (oop_is_object(obj_name)) {
            llvm::Value* val = gen_expr(node->right.get());
            oop_gen_field_set(ctx_, *builder_, obj_name, field_name, val, node->src_line);
            return;
        }
        /* Fallback: p.field where p is a function param (i64 boxed pointer) */
        VarRecord* rec = lookup_var(obj_name);
        if (rec) {
            std::string cls_name = global_class_registry().find_class_by_field(field_name);
            if (!cls_name.empty()) {
                llvm::Value* boxed = builder_->CreateLoad(i64_ty(), rec->alloca_ptr, obj_name);
                llvm::Value* ptr = builder_->CreateIntToPtr(boxed, i8ptr_ty(), obj_name + "_ptr");
                llvm::Value* val = gen_expr(node->right.get());
                oop_gen_self_field_set(ctx_, *builder_, ptr, cls_name, field_name, val, node->src_line);
                return;
            }
        }
    }

    const std::string& name = node->left->value;

    /* ClassName(...) — object creation */
    if (node->right->type == NodeType::Call) {
        const std::string& call_name = node->right->value;
        if (global_class_registry().has(call_name)) {
            auto gen_expr_fn = [this](const ASTNode* n) -> llvm::Value* {
                return gen_expr(n);
            };
            llvm::Value* obj_ptr = oop_gen_new_object(
                ctx_, *builder_, *module_, call_name,
                node->right->args.get(), name, gen_expr_fn);
            /* store pointer in a local alloca so VarRecord works */
            auto* slot = create_entry_alloca(name, llvm::PointerType::getUnqual(ctx_));
            builder_->CreateStore(obj_ptr, slot);
            declare_var(name, slot, OwnershipState::Owned);
            return;
        }
    }

    /* new ClassName(args) — object creation with new keyword */
    if (node->right->type == NodeType::New) {
        const std::string& class_name = node->right->value;
        if (global_class_registry().has(class_name)) {
            auto gen_expr_fn = [this](const ASTNode* n) -> llvm::Value* {
                return gen_expr(n);
            };
            llvm::Value* obj_ptr = oop_gen_new_object(
                ctx_, *builder_, *module_, class_name,
                node->right->args.get(), name, gen_expr_fn);
            auto* slot = create_entry_alloca(name, llvm::PointerType::getUnqual(ctx_));
            builder_->CreateStore(obj_ptr, slot);
            declare_var(name, slot, OwnershipState::Owned);
            return;
        }
    }

    llvm::Value* val = gen_expr(node->right.get());
    if (!val) val = i64(0);

    /* H3 Phase D: resolve type kind up front (annotation-first) */
    auto resolve_kind = [&]() -> AstTypeKind {
        const ASTNode* rhs = node->right.get();
        auto rk = get_annotation_kind(rhs);
        if (rk == AstTypeKind::Array || rk == AstTypeKind::String) return rk;
        if (rk == AstTypeKind::Unknown && (rhs->type == NodeType::Copy || rhs->type == NodeType::Move)) {
            const std::string& src_name = rhs->value;
            VarRecord* src = lookup_var(src_name);
            if (src) return src->type_kind;
        }
        return AstTypeKind::Unknown;
    };

    AstTypeKind assign_kind = resolve_kind();

    VarRecord* rec = lookup_var(name);

    /* Determine ownership state from memory analyzer results */
    OwnershipState init_state = OwnershipState::Owned;
    if (node->memory_meta.ownership_type == OwnershipType::Shared ||
        node->memory_meta.alloc_strategy == AllocStrategy::ARC) {
        init_state = OwnershipState::Shared;
    }
    /* typeof returns a global string constant — never free it */
    if (node->right->type == NodeType::Call &&
        node->right->value == "typeof") {
        init_state = OwnershipState::Borrowed;
    }

    /* Determine allocation strategy from forced/auto-detected */
    AllocStrategy strat = node->memory_meta.forced_strategy;
    if (strat == AllocStrategy::Unknown)
        strat = node->memory_meta.alloc_strategy;

    if (!rec) {
        llvm::Value* slot;
        slot = gen_allocation_for_var(name, val->getType(), init_state, strat);
        auto* store_val = val;
        if (auto* gv_slot = llvm::dyn_cast<llvm::GlobalVariable>(slot)) {
            auto* gv_ty = gv_slot->getValueType();
            if (store_val->getType() != gv_ty) {
                if (store_val->getType()->isPointerTy() && gv_ty->isIntegerTy())
                    store_val = builder_->CreatePtrToInt(store_val, gv_ty, name + "_gv_int");
                else if (store_val->getType()->isIntegerTy() && gv_ty->isPointerTy())
                    store_val = builder_->CreateIntToPtr(store_val, gv_ty, name + "_gv_ptr");
            }
        } else if (!llvm::isa<llvm::AllocaInst>(slot) && store_val->getType()->isPointerTy()) {
            store_val = builder_->CreatePtrToInt(store_val, i64_ty(), name + "_slot_int");
        }
        builder_->CreateStore(store_val, slot);
        declare_var(name, slot, init_state, assign_kind);
        rec = lookup_var(name);
    } else {
        auto* slot_ptr = rec->alloca_ptr;
        if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(slot_ptr)) {
            auto* store_val = val;
            auto* gv_ty = gv->getValueType();
            if (store_val->getType() != gv_ty) {
                if (store_val->getType()->isPointerTy() && gv_ty->isIntegerTy())
                    store_val = builder_->CreatePtrToInt(store_val, gv_ty, name + "_gv_int");
                else if (store_val->getType()->isIntegerTy() && gv_ty->isPointerTy())
                    store_val = builder_->CreateIntToPtr(store_val, gv_ty, name + "_gv_ptr");
            }
            builder_->CreateStore(store_val, gv);
        } else {
            auto* alloca_inst = llvm::dyn_cast<llvm::AllocaInst>(slot_ptr);
            llvm::Type* slot_ty = alloca_inst ? alloca_inst->getAllocatedType() : i64_ty();
            if (slot_ty != val->getType()) {
                auto* new_slot = create_entry_alloca(name + "_r", val->getType());
                builder_->CreateStore(val, new_slot);
                rec->alloca_ptr = new_slot;
            } else {
                builder_->CreateStore(val, rec->alloca_ptr);
            }
        }
        rec->type_kind = assign_kind;
    }

    /* Closure-returning function calls: propagate is_closure to receiver */
    if (node->right && node->right->type == NodeType::Call) {
        if (closure_returning_fns_.count(node->right->value))
            rec->is_closure = true;
    }

    /* Track struct type for struct literal variables */
    if (node->right && node->right->type == NodeType::StructLiteral)
        rec->struct_type = node->right->value;

    /* Track struct type for generic struct construction: Box[Int](42) */
    if (node->right && node->right->type == NodeType::Call && node->right->template_args) {
        const ASTNode* ra = node->right.get();
        std::string sname = ra->value;
        const ASTNode* ta = ra->template_args.get();
        while (ta) { sname += "__" + ta->value; ta = ta->next.get(); }
        if (global_type_registry().has_struct(sname))
            rec->struct_type = sname;
    }

    /* Register with GC if forced or auto-detected */
    if (strat == AllocStrategy::ForcedGC || strat == AllocStrategy::GC) {
        rec->is_gc_root = true;
        if (val->getType()->isPointerTy()) {
            auto* cast_val = builder_->CreateBitCast(val, i8ptr_ty(), name + "_gc");
            emit_gc_register_root(cast_val);
        }
    }
}

/* ── move x (standalone statement) ── */
void Codegen::gen_move(const ASTNode* node) {
    VarRecord* rec = lookup_var(node->value);
    if (!rec) return;
    /* Poison the source storage */
    emit_poison(rec->alloca_ptr);
    rec->state = OwnershipState::Moved;
}

/* ── drop x ── */
void Codegen::gen_drop(const ASTNode* node) {
    VarRecord* rec = lookup_var(node->value);
    if (!rec || !rec->alloca_ptr) return;

    auto* alloca_inst = llvm::dyn_cast<llvm::AllocaInst>(rec->alloca_ptr);
    llvm::Type* slot_ty = alloca_inst ? alloca_inst->getAllocatedType() : i64_ty();

    auto drop_k = get_annotation_kind(node);
    switch (rec->state) {
        case OwnershipState::Owned:
            if (drop_k == AstTypeKind::Array) {
                llvm::Value* arr = builder_->CreateLoad(slot_ty, rec->alloca_ptr, node->value + "_drop");
                builder_->CreateCall(fn_array_free_, { arr });
            } else if (slot_ty->isPointerTy()) {
                /* Load the value first, then pass to drop_glue */
                llvm::Value* val = builder_->CreateLoad(slot_ty, rec->alloca_ptr, node->value + "_drop_val");
                emit_drop(val);
            }
            break;
        case OwnershipState::Shared:
            if (slot_ty->isPointerTy())
                emit_refcount_dec(builder_->CreateLoad(slot_ty, rec->alloca_ptr, node->value + "_shared_drop"));
            break;
        case OwnershipState::Weak:
            if (slot_ty->isPointerTy())
                emit_weak_release(builder_->CreateLoad(slot_ty, rec->alloca_ptr, node->value + "_weak_drop"));
            break;
        default:
            break;
    }
    /* Poison so any use-after-drop is visible */
    emit_poison(rec->alloca_ptr);
    rec->state = OwnershipState::Moved;
}

/* ── delete x ── */
void Codegen::gen_delete(const ASTNode* node) {
    if (!node->left) return;
    VarRecord* rec = lookup_var(node->left->value);
    if (!rec || !rec->alloca_ptr) return;
    auto* alloca_inst = llvm::dyn_cast<llvm::AllocaInst>(rec->alloca_ptr);
    llvm::Type* slot_ty = alloca_inst ? alloca_inst->getAllocatedType() : i64_ty();
    auto del_k = get_annotation_kind(node);
    if (del_k == AstTypeKind::Array) {
        llvm::Value* arr = builder_->CreateLoad(slot_ty, rec->alloca_ptr, node->left->value + "_delete");
        builder_->CreateCall(fn_array_free_, { arr });
    } else if (slot_ty->isPointerTy()) {
        llvm::Value* val = builder_->CreateLoad(slot_ty, rec->alloca_ptr, node->left->value + "_delete_val");
        emit_drop(val);
    }
    emit_poison(rec->alloca_ptr);
    rec->state = OwnershipState::Moved;
}

/* ── shared x (standalone) ── */
void Codegen::gen_shared_ref(const ASTNode* node) {
    VarRecord* rec = lookup_var(node->value);
    if (!rec || !rec->alloca_ptr) return;
    emit_refcount_inc(rec->alloca_ptr);
    rec->state = OwnershipState::Shared;
}

/* ── weak x (standalone) ── */
void Codegen::gen_weak_ref(const ASTNode* node) {
    VarRecord* rec = lookup_var(node->value);
    if (!rec) return;
    rec->state = OwnershipState::Weak;
}

/* ── borrow x (standalone) ── */
void Codegen::gen_borrow(const ASTNode* node) {
    /* Borrow is a no-op in IR; the ownership tracker already validated it. */
    static_cast<void>(node);
}

/* ── output expr ── */
void Codegen::gen_output(const ASTNode* node) {
    if (!node->left) return;

    /* H2 Phase C: prefer annotation for type dispatch */
    {
        auto lk = get_annotation_kind(node->left.get());
        /* Float literal */
        if (lk == AstTypeKind::Float && node->left->type == NodeType::Float) {
            double fval = std::stod(node->left->value);
            llvm::Value* fv = llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx_), fval);
            builder_->CreateCall(fn_print_float_, { fv });
            return;
        }

        /* Variable */
        if (node->left->type == NodeType::Var) {
            VarRecord* rec = lookup_var(node->left->value);
            if (rec && rec->alloca_ptr) {
                auto* alloca_inst = llvm::dyn_cast<llvm::AllocaInst>(rec->alloca_ptr);
                auto* global_var = llvm::dyn_cast<llvm::GlobalVariable>(rec->alloca_ptr);
                llvm::Type* alloc_ty = alloca_inst ? alloca_inst->getAllocatedType()
                                     : global_var ? global_var->getValueType()
                                     : i64_ty();
                llvm::Value* val = builder_->CreateLoad(alloc_ty, rec->alloca_ptr, node->left->value);
                if (lk == AstTypeKind::Array)
                    builder_->CreateCall(fn_array_print_, { val });
                else if (lk == AstTypeKind::String)
                    builder_->CreateCall(fn_print_str_, { val });
                else if (lk == AstTypeKind::Float || alloc_ty->isDoubleTy()) {
                    if (val->getType()->isIntegerTy())
                        val = builder_->CreateBitCast(val, llvm::Type::getDoubleTy(ctx_), "fp_val");
                    builder_->CreateCall(fn_print_float_, { val });
                } else
                    builder_->CreateCall(fn_printf_, { val });
                return;
            }
        }

        /* Array literal */
        if (lk == AstTypeKind::Array && node->left->type == NodeType::Array) {
            llvm::Value* arr = gen_array(node->left.get());
            builder_->CreateCall(fn_array_print_, { arr });
            builder_->CreateCall(fn_array_free_,  { arr });
            return;
        }
    }

    /* Index access output(arr[i]) */
    if (node->left->type == NodeType::Index) {
        VarRecord* rec = lookup_var(node->left->value);
        if (rec && rec->alloca_ptr) {
            llvm::Value* arr_ptr = builder_->CreateLoad(i64_ty(), rec->alloca_ptr,
                                                        node->left->value + "_arr");
            llvm::Value* idx = gen_expr(node->left->left.get());
            llvm::Value* tag = builder_->CreateCall(fn_array_get_tag_, { arr_ptr, idx }, "tag");

            auto* str_bb      = llvm::BasicBlock::Create(ctx_, "os_str",      cur_fn_);
            auto* maybe_flt_bb= llvm::BasicBlock::Create(ctx_, "os_maybe_flt",cur_fn_);
            auto* flt_bb      = llvm::BasicBlock::Create(ctx_, "os_flt",      cur_fn_);
            auto* int_bb      = llvm::BasicBlock::Create(ctx_, "os_int",      cur_fn_);
            auto* done_bb     = llvm::BasicBlock::Create(ctx_, "os_done",     cur_fn_);

            /* Tag 2 (heap string) or tag 4 (SSO string) → string */
            auto* is_str2 = builder_->CreateICmpEQ(tag, i64(2), "is_str2");
            auto* is_str4 = builder_->CreateICmpEQ(tag, i64(4), "is_str4");
            auto* is_str  = builder_->CreateOr(is_str2, is_str4, "is_str");

            builder_->CreateCondBr(is_str, str_bb, maybe_flt_bb);

            builder_->SetInsertPoint(str_bb);
            builder_->CreateCall(fn_print_str_,
                { builder_->CreateCall(fn_array_get_str_, { arr_ptr, idx }) });
            safe_br(done_bb);

            builder_->SetInsertPoint(maybe_flt_bb);
            builder_->CreateCondBr(
                builder_->CreateICmpEQ(tag, i64(1), "is_flt"), flt_bb, int_bb);

            builder_->SetInsertPoint(flt_bb);
            builder_->CreateCall(fn_print_float_,
                { builder_->CreateCall(fn_array_get_flt_, { arr_ptr, idx }) });
            safe_br(done_bb);

            builder_->SetInsertPoint(int_bb);
            builder_->CreateCall(fn_printf_,
                { builder_->CreateCall(fn_array_get_int_, { arr_ptr, idx }) });
            safe_br(done_bb);

            builder_->SetInsertPoint(done_bb);
            return;
        }
    }

    /* Everything else */
    llvm::Value* val = gen_expr(node->left.get());
    if (!val) val = i64(0);
    /* H2 Phase C: prefer annotation, fall back to LLVM type */
    {
        auto lk = get_annotation_kind(node->left.get());
        if (lk == AstTypeKind::Float || val->getType()->isDoubleTy()) {
            if (val->getType()->isIntegerTy())
                val = builder_->CreateBitCast(val, llvm::Type::getDoubleTy(ctx_), "fp_val");
            builder_->CreateCall(fn_print_float_, { val });
        } else if (lk == AstTypeKind::String || val->getType()->isPointerTy())
            builder_->CreateCall(fn_print_str_, { val });
        else
            builder_->CreateCall(fn_printf_, { val });
    }
}

/* ── return expr ── */
void Codegen::gen_return(const ASTNode* node) {
    llvm::Value* val = node->left ? gen_expr(node->left.get()) : nullptr;
    /* Mark the return variable as moved so scope cleanup doesn't free it */
    if (node->left && node->left->type == NodeType::Var) {
        VarRecord* ret_rec = lookup_var(node->left->value);
        if (ret_rec) ret_rec->state = OwnershipState::Moved;
    }
    emit_all_scope_cleanup();
    if (current_block_terminated()) return;
    auto* ret_ty = cur_fn_->getReturnType();
    if (ret_ty->isVoidTy()) {
        builder_->CreateRetVoid();
        return;
    }
    /* Default return value matching the function's LLVM return type */
    if (!val) {
        if (ret_ty->isPointerTy())
            val = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ret_ty));
        else if (ret_ty->isFloatingPointTy())
            val = llvm::ConstantFP::get(ret_ty, 0.0);
        else
            val = llvm::ConstantInt::get(ret_ty, 0);
    } else if (val->getType()->isStructTy()) {
        /* Closure returns: heap-allocate storage so the struct survives
           the function return (stack allocas would be freed). */
        auto* struct_ty = llvm::cast<llvm::StructType>(val->getType());
        llvm::Value* heap_slot = builder_->CreateCall(
            fn_arena_alloc_, { i64(static_cast<int64_t>(module_->getDataLayout().getTypeAllocSize(struct_ty))) },
            "ret_closure_heap");
        builder_->CreateStore(val, heap_slot);
        val = heap_slot;
    }
    /* Convert val to match ret_ty if needed */
    if (val->getType() != ret_ty) {
        if (val->getType()->isIntegerTy() && ret_ty->isPointerTy())
            val = builder_->CreateIntToPtr(val, ret_ty, "ret_cast");
        else if (val->getType()->isPointerTy() && ret_ty->isIntegerTy())
            val = builder_->CreatePtrToInt(val, ret_ty, "ret_unbox");
        else if (val->getType()->isIntegerTy() && ret_ty->isIntegerTy() &&
                 val->getType()->getIntegerBitWidth() != ret_ty->getIntegerBitWidth()) {
            if (val->getType()->getIntegerBitWidth() > ret_ty->getIntegerBitWidth())
                val = builder_->CreateTrunc(val, ret_ty, "ret_trunc");
            else
                val = builder_->CreateZExt(val, ret_ty, "ret_zext");
        }
        else if (val->getType()->isIntegerTy() && ret_ty->isFloatingPointTy())
            val = builder_->CreateSIToFP(val, ret_ty, "ret_itof");
        else if (val->getType()->isFloatingPointTy() && ret_ty->isIntegerTy())
            val = builder_->CreateFPToSI(val, ret_ty, "ret_ftoi");
    }
    safe_ret(val);
}

/* ── Pattern matching helpers ── */

/* Extend small integer to i64 for uniform handling */
llvm::Value* Codegen::ensure_i64(llvm::Value* v) {
    if (v->getType()->isIntegerTy() && v->getType()->getIntegerBitWidth() < 64)
        return builder_->CreateZExt(v, i64_ty(), "ext_i64");
    return v;
}

/* Generate condition for a pattern against match_val. Returns i1. */
llvm::Value* Codegen::gen_pattern_cond(const ASTNode* pattern, llvm::Value* match_val) {
    switch (pattern->type) {
        case NodeType::Num: {
            llvm::Value* lit = i64(std::stoll(pattern->value));
            llvm::Value* val = ensure_i64(match_val);
            return builder_->CreateICmpEQ(val, lit, "pat_cmp");
        }
        case NodeType::Var:
            return builder_->getInt1(true);
        case NodeType::Call: {
            const std::string& struct_name = pattern->value;
            llvm::StructType* st = codegen_get_struct_type(ctx_, struct_name);
            if (!st) return builder_->getInt1(true);
            llvm::Value* sp = builder_->CreateBitCast(
                match_val, llvm::PointerType::get(st, 0), "pat_st");
            int fi = 0;
            llvm::Value* cond = builder_->getInt1(true);
            const ASTNode* fp = pattern->args.get();
            while (fp) {
                llvm::Value* fptr = builder_->CreateStructGEP(st, sp, fi, "pat_f");
                llvm::Type* fty = st->getElementType(fi);
                llvm::Value* fval = builder_->CreateLoad(fty, fptr, "pat_fv");
                fval = ensure_i64(fval);
                cond = builder_->CreateAnd(cond, gen_pattern_cond(fp, fval), "pat_and");
                fi++;
                fp = fp->next.get();
            }
            return cond;
        }
        case NodeType::Array: {
            int ecnt = 0;
            { const ASTNode* e = pattern->args.get(); while (e) { ecnt++; e = e->next.get(); } }
            llvm::Value* alen = builder_->CreateCall(fn_array_len_, { match_val }, "pat_len");
            llvm::Value* cond = builder_->CreateICmpEQ(alen, i64(ecnt), "pat_lencmp");
            int ei = 0;
            const ASTNode* ep = pattern->args.get();
            while (ep) {
                llvm::Value* ev = builder_->CreateCall(
                    fn_array_get_int_, { match_val, i64(ei) }, "pat_elem");
                cond = builder_->CreateAnd(cond, gen_pattern_cond(ep, ev), "pat_and");
                ei++;
                ep = ep->next.get();
            }
            return cond;
        }
        default:
            return builder_->getInt1(true);
    }
}

/* Bind variables from a matched pattern. Creates variables in the current scope. */
void Codegen::gen_pattern_bind(const ASTNode* pattern, llvm::Value* match_val) {
    switch (pattern->type) {
        case NodeType::Var:
            if (pattern->value != "_") {
                llvm::Value* val = ensure_i64(match_val);
                auto* slot = create_entry_alloca(pattern->value, val->getType());
                builder_->CreateStore(val, slot);
                declare_var(pattern->value, slot, OwnershipState::Owned);
            }
            break;
        case NodeType::Call: {
            const std::string& struct_name = pattern->value;
            llvm::StructType* st = codegen_get_struct_type(ctx_, struct_name);
            if (!st) break;
            llvm::Value* sp = builder_->CreateBitCast(
                match_val, llvm::PointerType::get(st, 0), "pat_st_bind");
            int fi = 0;
            const ASTNode* fp = pattern->args.get();
            while (fp) {
                llvm::Value* fptr = builder_->CreateStructGEP(st, sp, fi, "pat_f_bind");
                llvm::Type* fty = st->getElementType(fi);
                llvm::Value* fval = builder_->CreateLoad(fty, fptr, "pat_fv_bind");
                fval = ensure_i64(fval);
                gen_pattern_bind(fp, fval);
                fi++;
                fp = fp->next.get();
            }
            break;
        }
        case NodeType::Array: {
            int ei = 0;
            const ASTNode* ep = pattern->args.get();
            while (ep) {
                llvm::Value* ev = builder_->CreateCall(
                    fn_array_get_int_, { match_val, i64(ei) }, "pat_elem_bind");
                gen_pattern_bind(ep, ev);
                ei++;
                ep = ep->next.get();
            }
            break;
        }
        default:
            break;
    }
}

/* ── match expr: case val: ... default: ... ── */
void Codegen::gen_match(const ASTNode* node) {
    llvm::Value* match_val = gen_expr(node->left.get());

    auto* end_bb = llvm::BasicBlock::Create(ctx_, "match_end", cur_fn_);

    const ASTNode* case_node = node->args.get();
    while (case_node) {
        if (case_node->value == "default") {
            push_scope();
            gen_block(case_node->body.get());
            pop_scope_and_drop();
            safe_br(end_bb);
            break;
        }

        if (case_node->args) {
            const ASTNode* pattern = case_node->args.get();
            llvm::Value* cond = gen_pattern_cond(pattern, match_val);

            auto* case_bb = llvm::BasicBlock::Create(ctx_, "match_case", cur_fn_);
            auto* next_bb = llvm::BasicBlock::Create(ctx_, "match_next", cur_fn_);

            builder_->CreateCondBr(cond, case_bb, next_bb);

            builder_->SetInsertPoint(case_bb);
            push_scope();
            gen_pattern_bind(pattern, match_val);
            gen_block(case_node->body.get());
            pop_scope_and_drop();
            safe_br(end_bb);

            builder_->SetInsertPoint(next_bb);
        } else {
            llvm::Value* case_val = i64(std::stoll(case_node->value));
            llvm::Value* cmp = builder_->CreateICmpEQ(match_val, case_val, "match_cmp");

            auto* case_bb  = llvm::BasicBlock::Create(ctx_, "match_case",  cur_fn_);
            auto* next_bb  = llvm::BasicBlock::Create(ctx_, "match_next",  cur_fn_);

            builder_->CreateCondBr(cmp, case_bb, next_bb);

            builder_->SetInsertPoint(case_bb);
            push_scope();
            gen_block(case_node->body.get());
            pop_scope_and_drop();
            safe_br(end_bb);

            builder_->SetInsertPoint(next_bb);
        }
        case_node = case_node->next.get();
    }

    if (!builder_->GetInsertBlock()->getTerminator())
        safe_br(end_bb);
    if (end_bb->hasNPredecessorsOrMore(1))
        builder_->SetInsertPoint(end_bb);
}

void Codegen::gen_if(const ASTNode* node) {
    llvm::Value* cond = gen_expr(node->left.get());
    /* Treat non-zero as true */
    llvm::Value* cond_bool = builder_->CreateICmpNE(cond, i64(0), "if_cond");

    auto* then_bb  = llvm::BasicBlock::Create(ctx_, "then",  cur_fn_);
    auto* merge_bb = llvm::BasicBlock::Create(ctx_, "merge", cur_fn_);
    auto* else_bb  = node->orelse ? llvm::BasicBlock::Create(ctx_, "else", cur_fn_)
                                  : merge_bb;

    builder_->CreateCondBr(cond_bool, then_bb, else_bb);

    /* then */
    builder_->SetInsertPoint(then_bb);
    emit_coverage_trace(node->src_line);
    push_scope();
    gen_block(node->body.get());
    pop_scope_and_drop();
    safe_br(merge_bb);

    /* else / elseif */
    if (node->orelse) {
        builder_->SetInsertPoint(else_bb);
        emit_coverage_trace(node->orelse->src_line);
        push_scope();
        if (node->orelse->type == NodeType::If)
            gen_if(node->orelse.get());
        else
            gen_block(node->orelse->body.get());
        pop_scope_and_drop();
        safe_br(merge_bb);
    }

    /* Only set merge point if it's reachable */
    if (!merge_bb->getSinglePredecessor() || merge_bb->hasNPredecessorsOrMore(1))
        builder_->SetInsertPoint(merge_bb);
}

/* ── while cond: body ── */
void Codegen::gen_while(const ASTNode* node) {
    auto* cond_bb  = llvm::BasicBlock::Create(ctx_, "while_cond", cur_fn_);
    auto* body_bb  = llvm::BasicBlock::Create(ctx_, "while_body", cur_fn_);
    auto* exit_bb  = llvm::BasicBlock::Create(ctx_, "while_exit", cur_fn_);

    loop_stack_.push_back({ cond_bb, exit_bb });
    safe_br(cond_bb);

    /* condition */
    builder_->SetInsertPoint(cond_bb);
    llvm::Value* cond      = gen_expr(node->left.get());
    llvm::Value* cond_bool = builder_->CreateICmpNE(cond, i64(0), "while_cond_b");
    builder_->CreateCondBr(cond_bool, body_bb, exit_bb);

    /* body — no new scope, variables visible outside loop */
    builder_->SetInsertPoint(body_bb);
    emit_coverage_trace(node->src_line);
    gen_block(node->body.get());
    safe_br(cond_bb);

    builder_->SetInsertPoint(exit_bb);
    loop_stack_.pop_back();
}

/* ── loop ── */
void Codegen::gen_loop(const ASTNode* node) {
    auto* body_bb  = llvm::BasicBlock::Create(ctx_, "loop_body", cur_fn_);
    auto* exit_bb  = llvm::BasicBlock::Create(ctx_, "loop_exit", cur_fn_);

    loop_stack_.push_back({ body_bb, exit_bb });
    safe_br(body_bb);

    builder_->SetInsertPoint(body_bb);
    emit_coverage_trace(node->src_line);
    gen_block(node->body.get());
    safe_br(body_bb);

    builder_->SetInsertPoint(exit_bb);
    loop_stack_.pop_back();
}

/* ── repeat body until cond ── */
void Codegen::gen_repeat(const ASTNode* node) {
    auto* body_bb  = llvm::BasicBlock::Create(ctx_, "repeat_body", cur_fn_);
    auto* exit_bb  = llvm::BasicBlock::Create(ctx_, "repeat_exit", cur_fn_);

    loop_stack_.push_back({ body_bb, exit_bb });
    safe_br(body_bb);

    builder_->SetInsertPoint(body_bb);
    gen_block(node->body.get());

    llvm::Value* cond      = gen_expr(node->left.get());
    llvm::Value* cond_bool = builder_->CreateICmpNE(cond, i64(0), "repeat_cond");
    builder_->CreateCondBr(cond_bool, exit_bb, body_bb);

    builder_->SetInsertPoint(exit_bb);
    loop_stack_.pop_back();
}

/* ── for var in expr ── */
void Codegen::gen_for(const ASTNode* node) {
    /* Support both:
       - for i in 10          → count 0..9 (integer limit)
       - for x in arr         → iterate array elements (x = arr[i])
       - for i in len(arr)    → iterate array indices 0..len-1 */
    llvm::Value* limit = nullptr;
    bool is_array_iter = false;
    llvm::Value* arr_ptr_val = nullptr;

    /* H3 Phase B: annotation-first array detection for for-in */
    auto for_lk = get_annotation_kind(node->left.get());
    if (for_lk == AstTypeKind::Array ||
        (for_lk == AstTypeKind::Unknown && node->left->type == NodeType::Var)) {
        VarRecord* iter_rec = node->left->type == NodeType::Var ? lookup_var(node->left->value) : nullptr;
        if (for_lk == AstTypeKind::Array ||
            (for_lk == AstTypeKind::Unknown && iter_rec && iter_rec->type_kind == AstTypeKind::Array)) {
            /* Array iteration: use aurora_array_len to get the length */
            llvm::Value* arr_raw = gen_expr(node->left.get());
            arr_ptr_val = arr_raw;
            limit = builder_->CreateCall(fn_array_len_, { arr_ptr_val }, "arr_len");
            is_array_iter = true;
        }
    }

    /* Fallback: treat as integer count */
    if (!limit) {
        limit = gen_expr(node->left.get());
    }

    /* Index variable (always i64) */
    auto* idx_slot = create_entry_alloca(node->value + "_idx", i64_ty());
    builder_->CreateStore(i64(0), idx_slot);

    /* Loop variable (the user-visible name) */
    auto* loop_var_slot = create_entry_alloca(node->value, i64_ty());

    auto* cond_bb = llvm::BasicBlock::Create(ctx_, "for_cond", cur_fn_);
    auto* body_bb = llvm::BasicBlock::Create(ctx_, "for_body", cur_fn_);
    auto* exit_bb = llvm::BasicBlock::Create(ctx_, "for_exit", cur_fn_);

    loop_stack_.push_back({ cond_bb, exit_bb });
    safe_br(cond_bb);

    /* condition: i < limit */
    builder_->SetInsertPoint(cond_bb);
    llvm::Value* cur_idx    = builder_->CreateLoad(i64_ty(), idx_slot, node->value + "_idx");
    llvm::Value* cond_bool  = builder_->CreateICmpSLT(cur_idx, limit, "for_cond_b");
    builder_->CreateCondBr(cond_bool, body_bb, exit_bb);

    /* body */
    builder_->SetInsertPoint(body_bb);
    push_scope();

    if (is_array_iter && arr_ptr_val) {
        /* Array iteration: x = arr[i] — load element by index */
        llvm::Value* elem = builder_->CreateCall(fn_array_get_int_, { arr_ptr_val, cur_idx },
                                                  node->value + "_elem");
        builder_->CreateStore(elem, loop_var_slot);
    } else {
        /* Count iteration: x = i */
        builder_->CreateStore(cur_idx, loop_var_slot);
    }

    declare_var(node->value, loop_var_slot, OwnershipState::Owned);
    gen_block(node->body.get());
    pop_scope_and_drop();

    /* increment index */
    llvm::Value* next = builder_->CreateAdd(
        builder_->CreateLoad(i64_ty(), idx_slot, "for_i"),
        i64(1), "for_inc", false, true);
    builder_->CreateStore(next, idx_slot);
    safe_br(cond_bb);

    builder_->SetInsertPoint(exit_bb);
    loop_stack_.pop_back();
}

/* ── break — exit current loop ── */
void Codegen::gen_break() {
    if (loop_stack_.empty()) return;
    safe_br(loop_stack_.back().exit_bb);
}

/* ── continue — jump to loop condition ── */
void Codegen::gen_continue() {
    if (loop_stack_.empty()) return;
    safe_br(loop_stack_.back().cond_bb);
}

/* ── skip — skip rest of current iteration (same as continue) ── */
void Codegen::gen_skip() {
    gen_continue();
}

/* ── copy x  (statement form: deep copy in place, source unchanged) ──
   copy x     — no useful statement form for scalars; for arrays it
                re-allocates x with a fresh deep copy of itself.
   y = copy x — expression form (gen_copy_expr, in codegen_expr.cpp).  */
void Codegen::gen_copy(const ASTNode* node) {
    if (!node->left) return;
    const std::string& name = node->left->value;
    VarRecord* rec = lookup_var(name);
    if (!rec || !rec->alloca_ptr) return;

    if (get_annotation_kind(node) == AstTypeKind::Array) {
        /* Re-assign the variable to a deep copy of itself */
        llvm::Value* arr = builder_->CreateLoad(i64_ty(), rec->alloca_ptr, name + "_copy_src");

        llvm::Function* fn_copy = module_->getFunction("aurora_array_copy");
        if (!fn_copy) {
            auto* fty = llvm::FunctionType::get(i64_ty(), {i64_ty()}, false);
            fn_copy = llvm::Function::Create(fty, llvm::Function::ExternalLinkage,
                                             "aurora_array_copy", module_.get());
        }
        llvm::Value* new_arr = builder_->CreateCall(fn_copy, {arr}, name + "_copy_dst");
        builder_->CreateStore(new_arr, rec->alloca_ptr);
    }
    /* Scalars: copy is a no-op as a statement — value semantics already apply */
}

/* ── free x  —  raw memory release, bypasses RAII destructor ──
   Use when you know the object's lifetime is over and want to release
   memory immediately without going through drop glue.
   After free, the variable is poisoned (use-after-free = compile error). */
void Codegen::gen_free(const ASTNode* node) {
    if (!node->left) return;
    const std::string& name = node->left->value;
    VarRecord* rec = lookup_var(name);
    if (!rec || !rec->alloca_ptr) return;

    auto* alloca_inst = llvm::dyn_cast<llvm::AllocaInst>(rec->alloca_ptr);
    llvm::Type* slot_ty = alloca_inst ? alloca_inst->getAllocatedType() : i64_ty();

    if (get_annotation_kind(node) == AstTypeKind::Array) {
        /* For arrays: call aurora_array_free directly */
        llvm::Value* arr = builder_->CreateLoad(slot_ty, rec->alloca_ptr, name + "_free_arr");
        builder_->CreateCall(fn_array_free_, {arr});
    } else if (rec->state == OwnershipState::Shared && slot_ty->isPointerTy()) {
        /* Shared ref: decrement refcount (may trigger destruction) */
        emit_refcount_dec(builder_->CreateLoad(slot_ty, rec->alloca_ptr, name + "_free_shared"));
    } else if (rec->state == OwnershipState::Weak && slot_ty->isPointerTy()) {
        /* Weak ref: release weak reference */
        emit_weak_release(builder_->CreateLoad(slot_ty, rec->alloca_ptr, name + "_free_weak"));
    } else if (slot_ty->isPointerTy()) {
        /* Plain owned pointer: load value first, then emit drop glue */
        llvm::Value* val = builder_->CreateLoad(slot_ty, rec->alloca_ptr, name + "_free_val");
        emit_drop(val);
    }

    /* Poison so any subsequent use is caught at compile time */
    emit_poison(rec->alloca_ptr);
    rec->state = OwnershipState::Moved;
}

/* ── lambda(params): body — create anonymous function ── */
void Codegen::gen_lambda(const ASTNode* node) {
    static uint64_t lambda_id = 0;
    std::string lambda_name = "_lambda_" + std::to_string(lambda_id++);
    bool has_captures = !node->captures.empty();

    /* Build param types — env ptr first if capturing;
       user params prefer annotation, fall back to i8* */
    std::vector<std::string> param_names;
    std::vector<llvm::Type*> param_types;
    if (has_captures) {
        param_types.push_back(i8ptr_ty());
        param_names.push_back("__closure_env");
    }
    const ASTNode* p = node->args.get();
    while (p) {
        param_names.push_back(p->value);
        param_types.push_back(ast_kind_to_abi_type(ctx_, p->type_annotation.kind, i8ptr_ty()));
        p = p->next.get();
    }

    auto lambda_ret_kind = get_annotation_kind(node);
    auto* lambda_ret_ty  = ast_kind_to_abi_type(ctx_, lambda_ret_kind, i8ptr_ty());
    auto* fn_type = llvm::FunctionType::get(lambda_ret_ty, param_types, false);
    auto* fn = llvm::Function::Create(
        fn_type, llvm::Function::InternalLinkage, lambda_name, module_.get());

    int ai = 0;
    for (auto& arg : fn->args()) arg.setName(param_names[ai++]);

    auto* entry_bb = llvm::BasicBlock::Create(ctx_, "entry", fn);
    auto* saved_fn = cur_fn_;
    auto* saved_bb = builder_->GetInsertBlock();
    auto saved_scopes = std::move(scopes_);
    auto saved_cache  = std::move(literal_aurora_cache_);
    scopes_.clear();

    cur_fn_ = fn;
    builder_->SetInsertPoint(entry_bb);
    push_scope();

    /* Load captured values from env array into local variables */
    if (has_captures) {
        llvm::Value* env = fn->getArg(0);
        llvm::Value* env_i64 = builder_->CreateBitCast(env, llvm::PointerType::get(i64_ty(), 0), "env_arr");
        for (size_t i = 0; i < node->captures.size(); i++) {
            auto* gep = builder_->CreateGEP(i64_ty(), env_i64, i64(static_cast<int64_t>(i)), node->captures[i] + "_gep");
            llvm::Value* val = builder_->CreateLoad(i64_ty(), gep, node->captures[i] + "_cap");
            auto* slot = create_entry_alloca(node->captures[i], i64_ty());
            builder_->CreateStore(val, slot);
            declare_var(node->captures[i], slot, OwnershipState::Owned);
        }
    }

    /* Allocate params (skip env if capturing) */
    int param_start = has_captures ? 1 : 0;
    for (int i = param_start; i < static_cast<int>(param_names.size()); i++) {
        auto* slot = create_entry_alloca(param_names[i], i64_ty());
        llvm::Value* val = fn->getArg(i);
        if (val->getType() != i64_ty())
            val = builder_->CreatePtrToInt(val, i64_ty(), param_names[i] + "_unbox");
        builder_->CreateStore(val, slot);
        declare_var(param_names[i], slot, OwnershipState::Owned);
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
    scopes_ = std::move(saved_scopes);
    if (saved_bb) builder_->SetInsertPoint(saved_bb);

    /* Store lambda as variable */
    std::string var_name = node->value.empty() ? lambda_name : node->value;
    if (has_captures) {
        /* Create closure struct { i8* fn_ptr, i8* env } */
        auto* closure_ty = llvm::StructType::get(ctx_, { i8ptr_ty(), i8ptr_ty() }, false);
        auto* closure_alloca = create_entry_alloca(var_name + "_closure", closure_ty);

        /* Store fn ptr */
        llvm::Value* fn_ptr = builder_->CreateBitCast(fn, i8ptr_ty(), lambda_name + "_ptr");
        auto* fn_gep = builder_->CreateStructGEP(closure_ty, closure_alloca, 0, "fn_slot");
        builder_->CreateStore(fn_ptr, fn_gep);

        /* Allocate env array and store captured values.
           Heap-allocate via arena so the env survives function returns;
           stack allocas become dangling pointers when a closure outlives
           its creating function. */
        size_t ncaptures = node->captures.size();
        size_t env_bytes = ncaptures * 8;
        auto* env_heap = builder_->CreateCall(
            fn_arena_alloc_, { i64(static_cast<int64_t>(env_bytes)) }, var_name + "_env_heap");
        auto* env_as_i64 = builder_->CreateBitCast(env_heap, llvm::PointerType::get(i64_ty(), 0), var_name + "_env_i64");
        for (size_t i = 0; i < ncaptures; i++) {
            VarRecord* cap_rec = lookup_var(node->captures[i]);
            if (cap_rec && cap_rec->alloca_ptr) {
                llvm::Value* cap_val = builder_->CreateLoad(i64_ty(), cap_rec->alloca_ptr, node->captures[i]);
                auto* cap_gep = builder_->CreateGEP(i64_ty(), env_as_i64, i64(static_cast<int64_t>(i)), node->captures[i] + "_cap_slot");
                builder_->CreateStore(cap_val, cap_gep);
            }
        }

        /* Store env pointer in closure */
        llvm::Value* env_ptr = builder_->CreateBitCast(env_heap, i8ptr_ty(), var_name + "_env_ptr");
        auto* env_gep = builder_->CreateStructGEP(closure_ty, closure_alloca, 1, "env_slot");
        builder_->CreateStore(env_ptr, env_gep);

        /* Declare closure variable */
        declare_var(var_name, closure_alloca, OwnershipState::Owned);
        VarRecord* rec = lookup_var(var_name);
        if (rec) rec->is_closure = true;
    } else {
        /* Non-capturing: store function pointer as GlobalVariable so
           any function (including outlined try-bodies) can reference it
           without cross-function alloca issues. */
        auto* fn_ptr = llvm::ConstantExpr::getBitCast(fn, i8ptr_ty());
        auto* gv = new llvm::GlobalVariable(
            *module_, i8ptr_ty(), false,
            llvm::GlobalVariable::InternalLinkage,
            fn_ptr, var_name);
        declare_var(var_name, gv, OwnershipState::Borrowed);
    }
}

/* ── gen_index_assign — arr[idx] = expr ── */
void Codegen::gen_index_assign(const ASTNode* node) {
    VarRecord* rec = lookup_var(node->value);
    if (!rec || !rec->alloca_ptr) return;

    llvm::Value* arr_ptr = builder_->CreateLoad(i64_ty(), rec->alloca_ptr,
                                                node->value + "_arr");
    llvm::Value* idx = gen_expr(node->left.get());

    const ASTNode* rhs = node->right.get();
    if (!rhs) return;

    if (rhs->type == NodeType::Str) {
        auto* sp = builder_->CreateGlobalStringPtr(rhs->value, "ia_str");
        builder_->CreateCall(fn_array_set_str_, { arr_ptr, idx, sp });
    } else if (rhs->type == NodeType::Float) {
        double fv = std::stod(rhs->value);
        llvm::Value* fval = llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx_), fv);
        builder_->CreateCall(fn_array_set_flt_, { arr_ptr, idx, fval });
    } else {
        llvm::Value* val = gen_expr(rhs);
        if (!val) val = i64(0);
        if (val->getType()->isDoubleTy())
            builder_->CreateCall(fn_array_set_flt_, { arr_ptr, idx, val });
        else
            builder_->CreateCall(fn_array_set_int_, { arr_ptr, idx, val });
    }
}

/* ── spawn expr — execute expression on thread pool ── */
void Codegen::gen_spawn(const ASTNode* node) {
    if (!node->left) return;

    static uint64_t spawn_id = 0;
    std::string wrapper_name = "_spawn_wrapper_" + std::to_string(spawn_id++);

    auto* wrapper_ty = llvm::FunctionType::get(i8ptr_ty(), { i8ptr_ty() }, false);
    auto* wrapper = llvm::Function::Create(
        wrapper_ty, llvm::Function::InternalLinkage, wrapper_name, module_.get());
    auto* entry_bb = llvm::BasicBlock::Create(ctx_, "entry", wrapper);
    llvm::IRBuilder<> wb(entry_bb);

    llvm::Value* wrapper_result = llvm::ConstantPointerNull::get(i8ptr_ty());
    llvm::Value* arg_buffer = nullptr;

    if (node->left->type == NodeType::Call) {
        llvm::Function* callee = module_->getFunction(node->left->value);
        if (callee) {
            /* Step 1: Evaluate arguments in the CALLER context */
            std::vector<llvm::Value*> arg_values;
            const ASTNode* arg_node = node->left->args.get();
            while (arg_node) {
                llvm::Value* arg_val = gen_expr(arg_node);
                if (!arg_val)
                    arg_val = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0, true);
                arg_values.push_back(arg_val);
                arg_node = arg_node->next.get();
            }

            /* Step 2: Allocate heap buffer and store evaluated args in caller */
            if (!arg_values.empty()) {
                size_t n = arg_values.size();
                auto* i64_ty = llvm::Type::getInt64Ty(ctx_);
                llvm::Function* alloc_fn = module_->getFunction("aurora_alloc");
                if (!alloc_fn) {
                    auto* alloc_ty = llvm::FunctionType::get(
                        i8ptr_ty(), { i64_ty }, false);
                    alloc_fn = llvm::Function::Create(
                        alloc_ty, llvm::Function::ExternalLinkage, "aurora_alloc", module_.get());
                }
                arg_buffer = builder_->CreateCall(alloc_fn, { i64(n * 8) }, "spawn_args");
                auto* arg_array = builder_->CreateBitCast(
                    arg_buffer, llvm::PointerType::get(ctx_, 0), "arg_array");
                for (size_t i = 0; i < n; i++) {
                    llvm::Value* store_val = arg_values[i];
                    if (store_val->getType()->isPointerTy())
                        store_val = builder_->CreatePtrToInt(store_val, i64_ty);
                    else if (store_val->getType()->isDoubleTy())
                        store_val = builder_->CreateBitCast(store_val, i64_ty);
                    llvm::Value* gep = builder_->CreateConstGEP1_64(i64_ty, arg_array, i);
                    builder_->CreateStore(store_val, gep);
                }
            }

            /* Step 3: In the wrapper, unpack args and call target */
            std::vector<llvm::Value*> call_args;
            auto* wrapper_arg = wrapper->getArg(0);
            if (!arg_values.empty()) {
                auto* i64_ty = llvm::Type::getInt64Ty(ctx_);
                auto* arg_array = wb.CreateBitCast(
                    wrapper_arg, llvm::PointerType::get(ctx_, 0), "arg_array");
                for (size_t i = 0; i < arg_values.size(); i++) {
                    llvm::Value* loaded = wb.CreateLoad(i64_ty,
                        wb.CreateConstGEP1_64(i64_ty, arg_array, i),
                        "arg_" + std::to_string(i));
                    llvm::Type* param_ty = callee->getFunctionType()->getParamType(i);
                    if (param_ty->isPointerTy())
                        call_args.push_back(wb.CreateIntToPtr(loaded, param_ty));
                    else if (param_ty->isDoubleTy())
                        call_args.push_back(wb.CreateBitCast(loaded, param_ty));
                    else
                        call_args.push_back(wb.CreateIntCast(loaded, param_ty, true));
                }
            }

            call_args.resize(callee->arg_size(),
                             llvm::ConstantPointerNull::get(i8ptr_ty()));

            llvm::Value* ret = wb.CreateCall(callee, call_args, "spawn_call");

            /* Free argument buffer in the wrapper */
            if (arg_buffer) {
                llvm::Function* free_fn = module_->getFunction("aurora_free");
                if (!free_fn) {
                    auto* free_ty = llvm::FunctionType::get(
                        llvm::Type::getVoidTy(ctx_), { i8ptr_ty() }, false);
                    free_fn = llvm::Function::Create(
                        free_ty, llvm::Function::ExternalLinkage, "aurora_free", module_.get());
                }
                wb.CreateCall(free_fn, { wrapper_arg });
            }

            if (ret->getType()->isIntegerTy())
                wrapper_result = wb.CreateIntToPtr(ret, i8ptr_ty(), "spawn_ret");
            else if (ret->getType()->isPointerTy())
                wrapper_result = ret;
            else
                wrapper_result = wb.CreateBitCast(ret, i8ptr_ty(), "spawn_ret");
        }
    }

    wb.CreateRet(wrapper_result);

    auto* task = builder_->CreateCall(fn_task_create_,
        { wrapper, arg_buffer ? arg_buffer : llvm::ConstantPointerNull::get(i8ptr_ty()) }, "task");
    builder_->CreateCall(fn_spawn_, { task });
}

/* ── async: body — execute body on thread pool ── */
void Codegen::gen_async(const ASTNode* node) {
    if (!node->body) return;

    static uint64_t async_id = 0;
    std::string wrapper_name = "_async_wrapper_" + std::to_string(async_id++);

    auto* wrapper_ty = llvm::FunctionType::get(i8ptr_ty(), { i8ptr_ty() }, false);
    auto* wrapper = llvm::Function::Create(
        wrapper_ty, llvm::Function::InternalLinkage, wrapper_name, module_.get());
    auto* entry_bb = llvm::BasicBlock::Create(ctx_, "entry", wrapper);

    /* Save current function/builder state */
    auto* saved_fn = cur_fn_;
    auto saved_insert = builder_->saveIP();
    auto saved_cache  = std::move(literal_aurora_cache_);
    builder_->SetInsertPoint(entry_bb);
    cur_fn_ = wrapper;

    /* ── Async function: register a callable function ── */
    bool is_async_function = (node->body->type == NodeType::Function);
    std::string func_name;
    llvm::Function* async_callee = nullptr;

    if (is_async_function) {
        func_name = node->body->value;

        /* Generate wrapper body: generate the actual function body,
           then return null (the task machinery ignores the return value) */
        if (node->body->body)
            gen_block(node->body->body.get());
        if (!entry_bb->getTerminator())
            builder_->CreateRet(llvm::ConstantPointerNull::get(i8ptr_ty()));

        /* Create the callable async function: i8* name() -> task handle.
           Takes zero args so user can call simple_async() directly. */
        auto* fn_ty = llvm::FunctionType::get(i8ptr_ty(), {}, false);
        async_callee = llvm::Function::Create(
            fn_ty, llvm::Function::ExternalLinkage, func_name, module_.get());
        auto* callee_bb = llvm::BasicBlock::Create(ctx_, "entry", async_callee);
        llvm::IRBuilder<> cb(callee_bb);

        /* In the callable: create task from wrapper (with null arg), spawn, return task */
        auto* task = cb.CreateCall(fn_task_create_,
            { wrapper, llvm::ConstantPointerNull::get(i8ptr_ty()) }, "task");
        cb.CreateCall(fn_spawn_, { task });
        cb.CreateRet(task);
    }

    /* ── Plain async block: generate body inside wrapper ── */
    if (!is_async_function) {
        gen_block(node->body.get());

        if (!entry_bb->getTerminator())
            builder_->CreateRet(llvm::ConstantPointerNull::get(i8ptr_ty()));
    }

    /* Restore builder to main function */
    literal_aurora_cache_ = std::move(saved_cache);
    cur_fn_ = saved_fn;
    builder_->restoreIP(saved_insert);

    /* Plain async block: spawn immediately (statement context) */
    if (!is_async_function) {
        auto* task = builder_->CreateCall(fn_task_create_,
            { wrapper, llvm::ConstantPointerNull::get(i8ptr_ty()) }, "async_task");
        builder_->CreateCall(fn_spawn_, { task });
    }
}

/* ── wait expr — wait for task completion and get result ── */
void Codegen::gen_wait(const ASTNode* node) {
    if (!node->left) return;

    llvm::Value* task = gen_expr(node->left.get());
    if (!task) return;

    /* If task is i64, cast to pointer for the API calls */
    if (task->getType()->isIntegerTy())
        task = builder_->CreateIntToPtr(task, i8ptr_ty(), "task_ptr");

    builder_->CreateCall(fn_wait_, { task });
    /* result is available but unused in statement form */
    builder_->CreateCall(fn_task_get_result_, { task }, "wait_result");
}

/* ── new ClassName(args) — object creation ── */
void Codegen::gen_new(const ASTNode* node) {
    const std::string& class_name = node->value;

    /* Use existing OOP infrastructure to create the object */
    auto gen_expr_fn = [this](const ASTNode* n) -> llvm::Value* {
        return gen_expr(n);
    };

    llvm::Value* obj_ptr = oop_gen_new_object(
        ctx_, *builder_, *module_, class_name,
        node->args.get(), "", gen_expr_fn);

    if (!obj_ptr) {
        global_diag().warn(node->src_line, "new " + class_name + "() failed - class not found");
    }
}

/* ── reference x — take the address of x ── */
void Codegen::gen_reference(const ASTNode* node) {
    const std::string& name = node->value;
    VarRecord* rec = lookup_var(name);
    if (rec && rec->alloca_ptr) {
        /* Take the address of the variable's slot and store as pointer */
        llvm::Value* ptr = rec->alloca_ptr;
        auto* tmp = create_entry_alloca(name + "_ref", i8ptr_ty());
        builder_->CreateStore(ptr, tmp);
        declare_var(name + "_ref", tmp, OwnershipState::Borrowed);
    }
}

/* ── pointer x — take raw pointer of x ── */
void Codegen::gen_pointer(const ASTNode* node) {
    const std::string& name = node->value;
    VarRecord* rec = lookup_var(name);
    if (rec && rec->alloca_ptr) {
        /* Take the address of the variable's storage slot */
        llvm::Value* ptr = rec->alloca_ptr;
        auto* tmp = create_entry_alloca(name + "_ptr_tmp", i8ptr_ty());
        builder_->CreateStore(ptr, tmp);
        declare_var(name + "_ptr", tmp, OwnershipState::Borrowed);
    }
}

/* ── unsafe: block — same as normal block for now ── */
void Codegen::gen_unsafe_block(const ASTNode* node) {
    if (node->body) gen_block(node->body.get());
}

/* ── safe: block — same as normal block ── */
void Codegen::gen_safe_block(const ASTNode* node) {
    if (node->body) gen_block(node->body.get());
}

/* ── panic [expr] — halt execution ── */
void Codegen::gen_panic(const ASTNode* node) {
    auto* fn_panic = module_->getFunction("aurora_panic");
    auto* fn_as_cstr = module_->getFunction("aurora_str_as_cstr");
    if (fn_panic) {
        llvm::Value* msg = i64(0);
        if (node->left) {
            msg = gen_expr(node->left.get());
            if (msg) {
                if (!msg->getType()->isPointerTy())
                    msg = builder_->CreateIntToPtr(msg, i8ptr_ty(), "panic_msg");
                else if (fn_as_cstr)
                    msg = builder_->CreateCall(fn_as_cstr, { msg }, "panic_cstr");
            }
        }
        builder_->CreateCall(fn_panic, { msg ? msg : llvm::ConstantPointerNull::get(i8ptr_ty()) });
    }
    builder_->CreateUnreachable();
    auto* dead_bb = llvm::BasicBlock::Create(ctx_, "panic_dead", cur_fn_);
    builder_->SetInsertPoint(dead_bb);
}

/* ── debug expr — debug output ── */
void Codegen::gen_debug(const ASTNode* node) {
    auto* prefix = get_literal_aurora("[DEBUG] ");
    builder_->CreateCall(fn_print_str_, { prefix });
    if (node->left) {
        llvm::Value* val = gen_expr(node->left.get());
        if (val) {
            if (val->getType()->isDoubleTy())
                builder_->CreateCall(fn_print_float_, { val });
            else if (val->getType()->isPointerTy())
                builder_->CreateCall(fn_print_str_, { val });
            else
                builder_->CreateCall(fn_printf_, { val });
        }
    }
}

/* ── log expr — log output ── */
void Codegen::gen_log(const ASTNode* node) {
    auto* prefix = get_literal_aurora("[LOG] ");
    builder_->CreateCall(fn_print_str_, { prefix });
    if (node->left) {
        llvm::Value* val = gen_expr(node->left.get());
        if (val) {
            if (val->getType()->isDoubleTy())
                builder_->CreateCall(fn_print_float_, { val });
            else if (val->getType()->isPointerTy())
                builder_->CreateCall(fn_print_str_, { val });
            else
                builder_->CreateCall(fn_printf_, { val });
        }
    }
}

/* ════════════════════════════════════════════════════════════
   Domain-specific Codegen — Game Engine
   ════════════════════════════════════════════════════════════ */

void Codegen::gen_scene(const ASTNode* node) {
    if (scene_init_)
        builder_->CreateCall(scene_init_, {});
    gen_block(node->body.get());
    if (scene_shutdown_)
        builder_->CreateCall(scene_shutdown_, {});
}

void Codegen::gen_entity(const ASTNode* node) {
    if (!entity_create_) return;
    llvm::Value* type_val = i64(0);
    if (node->left)
        type_val = gen_expr(node->left.get());
    llvm::Value* obj = builder_->CreateCall(entity_create_, { type_val }, "entity");
    /* store entity pointer — scope-managed */
    auto* slot = create_entry_alloca(node->value + "_entity", i8ptr_ty());
    builder_->CreateStore(obj, slot);
    declare_var(node->value + "_entity", slot, OwnershipState::Owned);
    gen_block(node->body.get());
    if (entity_destroy_) {
        builder_->CreateCall(entity_destroy_, { obj });
        builder_->CreateStore(llvm::ConstantPointerNull::get(i8ptr_ty()), slot);
    }
}

void Codegen::gen_sprite(const ASTNode* node) {
    if (!sprite_create_) return;
    llvm::Value* texture = llvm::ConstantPointerNull::get(i8ptr_ty());
    llvm::Value* x = llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx_), 0.0);
    llvm::Value* y = llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx_), 0.0);
    if (node->left) {
        auto* arg = node->left.get();
        if (arg) {
            texture = gen_expr(arg);
            if (arg->next) {
                x = gen_expr(arg->next.get());
                if (arg->next->next)
                    y = gen_expr(arg->next->next.get());
            }
        }
    }
    builder_->CreateCall(sprite_create_, { texture, x, y }, "sprite");
    if (node->body) gen_block(node->body.get());
}

void Codegen::gen_camera(const ASTNode* node) {
    if (!camera_create_) return;
    llvm::Value* x = llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx_), 0.0);
    llvm::Value* y = llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx_), 0.0);
    llvm::Value* z = llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx_), 0.0);
    if (node->left) {
        auto* arg = node->left.get();
        if (arg) {
            x = gen_expr(arg);
            if (arg->next) {
                y = gen_expr(arg->next.get());
                if (arg->next->next)
                    z = gen_expr(arg->next->next.get());
            }
        }
    }
    builder_->CreateCall(camera_create_, { x, y, z }, "camera");
    if (node->body) gen_block(node->body.get());
}

void Codegen::gen_physics(const ASTNode* node) {
    if (physics_init_)
        builder_->CreateCall(physics_init_, {});
    gen_block(node->body.get());
    if (physics_step_) {
        llvm::Value* dt = llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx_), 0.016);
        if (node->left)
            dt = gen_expr(node->left.get());
        builder_->CreateCall(physics_step_, { dt });
    }
}

void Codegen::gen_collision(const ASTNode* node) {
    if (!collision_check_) return;
    llvm::Value* a = llvm::ConstantPointerNull::get(i8ptr_ty());
    llvm::Value* b = llvm::ConstantPointerNull::get(i8ptr_ty());
    if (node->left) {
        a = gen_expr(node->left.get());
        if (node->left->next)
            b = gen_expr(node->left->next.get());
    }
    builder_->CreateCall(collision_check_, { a, b }, "collision");
    if (node->body) gen_block(node->body.get());
}

void Codegen::gen_audio(const ASTNode* node) {
    if (!audio_play_) return;
    llvm::Value* clip = llvm::ConstantPointerNull::get(i8ptr_ty());
    if (node->left)
        clip = gen_expr(node->left.get());
    builder_->CreateCall(audio_play_, { clip });
    if (node->body) gen_block(node->body.get());
}

/* ════════════════════════════════════════════════════════════
   Domain-specific Codegen — Backend Framework
   ════════════════════════════════════════════════════════════ */

void Codegen::gen_server(const ASTNode* node) {
    if (!server_init_) return;
    llvm::Value* port = i64(8080);
    if (node->left)
        port = gen_expr(node->left.get());
    llvm::Value* srv = builder_->CreateCall(server_init_, { port }, "server");

    /* Route counter for generating unique handler function names */
    static uint64_t route_counter = 0;
    static uint64_t middleware_counter = 0;

    /* Process body: for each route/middleware node, generate handler functions and register them */
    const ASTNode* child = node->body.get();
    while (child) {
        if (child->type == NodeType::Middleware) {
            /* Create middleware handler function: int(i8* req, i8* res, i8* userdata) */
            std::string mw_name = "_middleware_hdl_" + std::to_string(middleware_counter++);
            auto* mw_fn_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(ctx_), { i8ptr_ty(), i8ptr_ty(), i8ptr_ty() }, false);
            auto* mw_fn = llvm::Function::Create(
                mw_fn_type, llvm::Function::InternalLinkage, mw_name, module_.get());
            auto* entry_bb = llvm::BasicBlock::Create(ctx_, "entry", mw_fn);

            auto* saved_fn = cur_fn_;
            auto saved_ip = builder_->saveIP();

            builder_->SetInsertPoint(entry_bb);
            cur_fn_ = mw_fn;

            literal_aurora_cache_.clear();

            auto arg_it = mw_fn->arg_begin();
            llvm::Value* req_param = arg_it++;
            llvm::Value* res_param = arg_it++;
            req_param->setName("req");
            res_param->setName("res");

            push_scope();
            declare_var("__http_req", create_entry_alloca("__http_req", i8ptr_ty()), OwnershipState::Borrowed);
            declare_var("__http_res", create_entry_alloca("__http_res", i8ptr_ty()), OwnershipState::Borrowed);
            builder_->CreateStore(req_param, lookup_var("__http_req")->alloca_ptr);
            builder_->CreateStore(res_param, lookup_var("__http_res")->alloca_ptr);

            /* Generate the middleware body */
            if (child->body) gen_block(child->body.get());

            pop_scope_and_drop();
            /* Return 0 to continue the middleware chain */
            builder_->CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_), 0));

            cur_fn_ = saved_fn;
            builder_->restoreIP(saved_ip);

            /* Register the middleware handler with the server */
            if (server_add_mw_) {
                llvm::Value* hdl_ptr = builder_->CreateBitCast(mw_fn, i8ptr_ty(), "mwhdl");
                builder_->CreateCall(server_add_mw_, { srv, hdl_ptr });
            }
        } else if (child->type == NodeType::Route) {
            /* Create handler function: void(i8* req, i8* res) */
            std::string handler_name = "_route_hdl_" + std::to_string(route_counter++);
            auto* hdl_fn_type = llvm::FunctionType::get(void_ty(), { i8ptr_ty(), i8ptr_ty() }, false);
            auto* hdl_fn = llvm::Function::Create(
                hdl_fn_type, llvm::Function::InternalLinkage, handler_name, module_.get());
            auto* entry_bb = llvm::BasicBlock::Create(ctx_, "entry", hdl_fn);

            /* Save current function state */
            auto* saved_fn = cur_fn_;
            auto saved_ip = builder_->saveIP();

            /* Set insert point to entry of new handler function */
            builder_->SetInsertPoint(entry_bb);
            cur_fn_ = hdl_fn;

            /* Clear literal cache — cached SSA values belong to the prior function */
            literal_aurora_cache_.clear();

            /* Get req/res parameters and give them names so they can be referenced */
            auto arg_it = hdl_fn->arg_begin();
            llvm::Value* req_param = arg_it++;
            llvm::Value* res_param = arg_it++;
            req_param->setName("req");
            res_param->setName("res");

            /* Push a scope for handler-local variables */
            push_scope();

            /* Store req/res in scope so response()/request keywords can find them */
            /* Borrowed: the C caller (aurora_server_accept_and_handle) owns and frees these */
            declare_var("__http_req", create_entry_alloca("__http_req", i8ptr_ty()), OwnershipState::Borrowed);
            declare_var("__http_res", create_entry_alloca("__http_res", i8ptr_ty()), OwnershipState::Borrowed);
            builder_->CreateStore(req_param, lookup_var("__http_req")->alloca_ptr);
            builder_->CreateStore(res_param, lookup_var("__http_res")->alloca_ptr);

            /* Set default Content-Type */
            if (http_resp_ct_) {
                llvm::Value* ct = builder_->CreateGlobalStringPtr("application/json", "ct");
                builder_->CreateCall(http_resp_ct_, { res_param, ct });
            }

            /* Generate the route handler body code */
            gen_block(child->body.get());

            /* Pop handler scope (drop __http_req/__http_res local allocas) */
            pop_scope_and_drop();

            builder_->CreateRetVoid();

            /* Restore codegen state */
            cur_fn_ = saved_fn;
            builder_->restoreIP(saved_ip);

            /* Register the handler function */
            if (route_register_) {
                /* Method from route's left child (default "GET") */
                llvm::Value* method_str = nullptr;
                if (child->left) {
                    method_str = gen_expr(child->left.get());
                    if (method_str->getType() != i8ptr_ty())
                        method_str = builder_->CreateBitCast(method_str, i8ptr_ty());
                    /* Convert AuroraStr* to const char* */
                    llvm::Function* str_to_c = module_->getFunction("aurora_str_as_cstr");
                    if (str_to_c)
                        method_str = builder_->CreateCall(str_to_c, { method_str }, "method_cstr");
                }
                if (!method_str)
                    method_str = builder_->CreateGlobalStringPtr("GET", "rmethod");

                /* Path from child's value */
                llvm::Value* path_str = builder_->CreateGlobalStringPtr(
                    child->value.empty() ? "/" : child->value.c_str(), "rpath");

                /* Cast handler function to i8* */
                llvm::Value* hdl_ptr = builder_->CreateBitCast(hdl_fn, i8ptr_ty(), "hdl");

                builder_->CreateCall(route_register_, { method_str, path_str, hdl_ptr });
            }
        } else if (child->type == NodeType::Cors) {
            /* CORS: call aurora_cors_apply_default on the response */
            auto* cors_fn = module_->getFunction("aurora_cors_apply_default");
            if (cors_fn && child->left) {
                /* Specific origin */
                llvm::Value* origin = gen_expr(child->left.get());
                auto* cors_with_origin = module_->getFunction("aurora_cors_apply_with_origin");
                auto* fn_from_cstr = module_->getFunction("aurora_str_as_cstr");
                if (cors_with_origin && origin && fn_from_cstr) {
                    if (origin->getType() != i8ptr_ty())
                        origin = builder_->CreateIntToPtr(origin, i8ptr_ty(), "origin_ptr");
                    origin = builder_->CreateCall(fn_from_cstr, { origin }, "origin_cstr");
                    builder_->CreateCall(cors_with_origin, { llvm::ConstantPointerNull::get(i8ptr_ty()), origin });
                }
            } else if (cors_fn) {
                /* Default CORS (*) — store flag on server to apply on each response */
                builder_->CreateCall(cors_fn, { llvm::ConstantPointerNull::get(i8ptr_ty()) });
            }
        } else if (child->type == NodeType::WebSocket) {
            /* WebSocket: generate a handler that upgrades to WS */
            static uint64_t ws_counter = 0;
            std::string ws_name = "_ws_hdl_" + std::to_string(ws_counter++);
            auto* ws_fn_type = llvm::FunctionType::get(void_ty(), { i8ptr_ty(), i8ptr_ty() }, false);
            auto* ws_fn = llvm::Function::Create(
                ws_fn_type, llvm::Function::InternalLinkage, ws_name, module_.get());
            auto* ws_entry = llvm::BasicBlock::Create(ctx_, "entry", ws_fn);
            auto* saved_fn = cur_fn_;
            auto saved_ip = builder_->saveIP();
            literal_aurora_cache_.clear();
            builder_->SetInsertPoint(ws_entry);
            cur_fn_ = ws_fn;
            auto arg_it = ws_fn->arg_begin();
            llvm::Value* ws_req = arg_it++;
            llvm::Value* ws_res = arg_it++;
            ws_req->setName("req");
            ws_res->setName("res");
            push_scope();
            declare_var("__http_req", create_entry_alloca("__http_req", i8ptr_ty()), OwnershipState::Borrowed);
            declare_var("__http_res", create_entry_alloca("__http_res", i8ptr_ty()), OwnershipState::Borrowed);
            builder_->CreateStore(ws_req, lookup_var("__http_req")->alloca_ptr);
            builder_->CreateStore(ws_res, lookup_var("__http_res")->alloca_ptr);
            if (child->body) gen_block(child->body.get());
            pop_scope_and_drop();
            builder_->CreateRetVoid();
            cur_fn_ = saved_fn;
            builder_->restoreIP(saved_ip);
            /* Register route */
            if (route_register_) {
                llvm::Value* method_str = builder_->CreateGlobalStringPtr("GET", "wsmethod");
                llvm::Value* path_str = builder_->CreateGlobalStringPtr(
                    child->value.empty() ? "/ws" : child->value.c_str(), "wspath");
                llvm::Value* hdl_ptr = builder_->CreateBitCast(ws_fn, i8ptr_ty(), "wshdl");
                builder_->CreateCall(route_register_, { method_str, path_str, hdl_ptr });
            }
        } else if (child->type == NodeType::Sse) {
            /* SSE: generate a handler that starts SSE stream */
            static uint64_t sse_counter = 0;
            std::string sse_name = "_sse_hdl_" + std::to_string(sse_counter++);
            auto* sse_fn_type = llvm::FunctionType::get(void_ty(), { i8ptr_ty(), i8ptr_ty() }, false);
            auto* sse_fn = llvm::Function::Create(
                sse_fn_type, llvm::Function::InternalLinkage, sse_name, module_.get());
            auto* sse_entry = llvm::BasicBlock::Create(ctx_, "entry", sse_fn);
            auto* saved_fn = cur_fn_;
            auto saved_ip = builder_->saveIP();
            literal_aurora_cache_.clear();
            builder_->SetInsertPoint(sse_entry);
            cur_fn_ = sse_fn;
            auto arg_it = sse_fn->arg_begin();
            llvm::Value* sse_req = arg_it++;
            llvm::Value* sse_res = arg_it++;
            sse_req->setName("req");
            sse_res->setName("res");
            push_scope();
            declare_var("__http_req", create_entry_alloca("__http_req", i8ptr_ty()), OwnershipState::Borrowed);
            declare_var("__http_res", create_entry_alloca("__http_res", i8ptr_ty()), OwnershipState::Borrowed);
            builder_->CreateStore(sse_req, lookup_var("__http_req")->alloca_ptr);
            builder_->CreateStore(sse_res, lookup_var("__http_res")->alloca_ptr);
            if (child->body) gen_block(child->body.get());
            pop_scope_and_drop();
            builder_->CreateRetVoid();
            cur_fn_ = saved_fn;
            builder_->restoreIP(saved_ip);
            if (route_register_) {
                llvm::Value* method_str = builder_->CreateGlobalStringPtr("GET", "ssemethod");
                llvm::Value* path_str = builder_->CreateGlobalStringPtr(
                    child->value.empty() ? "/events" : child->value.c_str(), "ssepath");
                llvm::Value* hdl_ptr = builder_->CreateBitCast(sse_fn, i8ptr_ty(), "ssehdl");
                builder_->CreateCall(route_register_, { method_str, path_str, hdl_ptr });
            }
        } else if (child->type == NodeType::Tpl) {
            /* Template: compile and register template */
            auto* tpl_fn = module_->getFunction("aurora_template_compile");
            if (tpl_fn) {
                llvm::Value* name_str = builder_->CreateGlobalStringPtr(
                    child->value.c_str(), "tpl_name");
                llvm::Value* source_str = llvm::ConstantPointerNull::get(i8ptr_ty());
                if (child->left) {
                    source_str = gen_expr(child->left.get());
                    if (source_str && source_str->getType() != i8ptr_ty())
                        source_str = builder_->CreateIntToPtr(source_str, i8ptr_ty(), "tpl_src");
                    auto* as_cstr = module_->getFunction("aurora_str_as_cstr");
                    if (as_cstr && source_str)
                        source_str = builder_->CreateCall(as_cstr, { source_str }, "tpl_src_cstr");
                }
                builder_->CreateCall(tpl_fn, { name_str, source_str }, "tpl");
            }
        } else if (child->type == NodeType::Validate) {
            /* Validate: generate validation checks (simplified — emit builtin) */
            auto* validate_fn = module_->getFunction("builtin_validate");
            if (validate_fn && child->body) {
                /* For now, just emit validate() call and generate body */
                builder_->CreateCall(validate_fn, {}, "validate_ret");
                gen_block(child->body.get());
            } else {
                gen_block(child->body.get());
            }
        } else {
            gen_stmt(child);
        }
        child = child->next.get();
    }

    /* Start the server (blocking — bind + listen + accept loop) */
    if (server_run_) {
        builder_->CreateCall(server_run_, { srv });
    }
}

void Codegen::gen_api(const ASTNode* node) {
    /* API: register REST endpoints from the API body */
    static uint64_t api_id = 0;
    std::string api_name = node->value.empty() ? "_api_" + std::to_string(api_id++) : node->value;
    /* Iterate body children looking for Route nodes and register them */
    const ASTNode* child = node->body.get();
    while (child) {
        if (child->type == NodeType::Route) {
            /* Generate route registration inside gen_route */
            gen_route(child);
        } else if (child->type == NodeType::Function) {
            /* Generate the handler function */
            gen_function(child);
            /* Register it as a route if it has a path annotation */
            if (route_register_ && !child->value.empty()) {
                llvm::Value* method_str = builder_->CreateGlobalStringPtr("GET", "api_method");
                llvm::Value* path_str = builder_->CreateGlobalStringPtr(
                    "/" + child->value, "api_route");
                llvm::Function* handler_fn = module_->getFunction(child->value);
                if (handler_fn) {
                    llvm::Value* handler_ptr = builder_->CreateBitCast(handler_fn, i8ptr_ty());
                    builder_->CreateCall(route_register_, { method_str, path_str, handler_ptr });
                }
            }
        } else {
            gen_stmt(child);
        }
        child = child->next.get();
    }
}

void Codegen::gen_database(const ASTNode* node) {
    if (!db_connect_) return;
    llvm::Value* conn_str = llvm::ConstantPointerNull::get(i8ptr_ty());
    if (node->left)
        conn_str = gen_expr(node->left.get());
    llvm::Value* db = builder_->CreateCall(db_connect_, { conn_str }, "db");
    gen_block(node->body.get());
    if (db_close_)
        builder_->CreateCall(db_close_, { db });
}

void Codegen::gen_cache(const ASTNode* node) {
    if (!cache_init_) return;
    llvm::Value* cache = builder_->CreateCall(cache_init_, {}, "cache");
    gen_block(node->body.get());
}

void Codegen::gen_session(const ASTNode* node) {
    if (!session_create_) return;
    llvm::Value* sess = builder_->CreateCall(session_create_, {}, "session");
    gen_block(node->body.get());
    if (session_destroy_)
        builder_->CreateCall(session_destroy_, { sess });
}

void Codegen::gen_model(const ASTNode* node) {
    if (!fn_orm_schema_define_) return;
    if (!node->left) return;
    llvm::Value* name = gen_expr(node->left.get());
    llvm::Value* schema = builder_->CreateCall(fn_orm_schema_define_, { name }, "schema");
    if (node->body) {
        /* Body contains field definitions via builtin_schema()
           which we emit as a call to aurora_orm_schema_column.
           Children use the builtin model()/schema() functions. */
        gen_block(node->body.get());
    }
}

void Codegen::gen_auth(const ASTNode* node) {
    if (!auth_login_) return;
    llvm::Value* user = llvm::ConstantPointerNull::get(i8ptr_ty());
    llvm::Value* pass = llvm::ConstantPointerNull::get(i8ptr_ty());
    if (node->left) {
        user = gen_expr(node->left.get());
        if (node->left->next)
            pass = gen_expr(node->left->next.get());
    }
    builder_->CreateCall(auth_login_, { user, pass });
    if (node->body) gen_block(node->body.get());
}

/* ════════════════════════════════════════════════════════════
   Domain-specific Codegen — UI Framework
   ════════════════════════════════════════════════════════════ */

void Codegen::gen_component(const ASTNode* node) {
    if (!comp_create_) return;
    if (ui_init_)
        builder_->CreateCall(ui_init_, {});

    std::string comp_name = node->value.empty() ? "comp" : node->value;
    llvm::Value* name_str = builder_->CreateGlobalStringPtr(comp_name, "comp_name");
    auto* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_), 0);
    llvm::Value* comp = builder_->CreateCall(comp_create_,
        { name_str, zero, zero,
          llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_), 800),
          llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_), 600) },
        comp_name + "_ptr");

    /* Create render callback if there's a body */
    if (node->body) {
        static uint64_t comp_id = 0;
        std::string render_name = "_comp_render_" + comp_name + "_" + std::to_string(comp_id++);
        auto* render_ty = llvm::FunctionType::get(void_ty(),
            { i8ptr_ty(), llvm::Type::getInt32Ty(ctx_),
              llvm::Type::getInt32Ty(ctx_), llvm::Type::getInt32Ty(ctx_),
              llvm::Type::getInt32Ty(ctx_) }, false);
        auto* render_fn = llvm::Function::Create(
            render_ty, llvm::Function::InternalLinkage, render_name, module_.get());
        auto* entry_bb = llvm::BasicBlock::Create(ctx_, "entry", render_fn);
        auto* saved_fn = cur_fn_;
        auto saved_insert = builder_->saveIP();
        auto saved_cache  = std::move(literal_aurora_cache_);
        builder_->SetInsertPoint(entry_bb);
        cur_fn_ = render_fn;

        /* Store the component pointer in a scope variable so children can add_child */
        auto* comp_slot = create_entry_alloca("__parent_comp", i8ptr_ty());
        builder_->CreateStore(comp, comp_slot);
        declare_var("__parent_comp", comp_slot, OwnershipState::Owned);

        gen_block(node->body.get());
        builder_->CreateRetVoid();

        literal_aurora_cache_ = std::move(saved_cache);
        cur_fn_ = saved_fn;
        builder_->restoreIP(saved_insert);

        auto* render_cast = builder_->CreateBitCast(render_fn, i8ptr_ty());
        builder_->CreateCall(comp_set_render_, { comp, render_cast });
    }

    /* Store component in scope for parent add_child */
    auto* comp_var_slot = create_entry_alloca(comp_name + "_obj", i8ptr_ty());
    builder_->CreateStore(comp, comp_var_slot);

    /* Check if we're inside a parent component — if so, add as child */
    VarRecord* parent_rec = lookup_var("__parent_comp");
    if (parent_rec && parent_rec->alloca_ptr && comp_add_child_) {
        llvm::Value* parent_comp = builder_->CreateLoad(i8ptr_ty(), parent_rec->alloca_ptr, "parent_comp");
        llvm::Value* is_parent = builder_->CreateICmpNE(parent_comp,
            llvm::ConstantPointerNull::get(i8ptr_ty()));
        auto* add_child_bb = llvm::BasicBlock::Create(ctx_, "add_child", cur_fn_);
        auto* skip_child_bb = llvm::BasicBlock::Create(ctx_, "skip_child", cur_fn_);
        builder_->CreateCondBr(is_parent, add_child_bb, skip_child_bb);
        builder_->SetInsertPoint(add_child_bb);
        builder_->CreateCall(comp_add_child_, { parent_comp, comp });
        builder_->CreateBr(skip_child_bb);
        builder_->SetInsertPoint(skip_child_bb);
    }
}

void Codegen::gen_style(const ASTNode* node) {
    if (!style_apply_) return;
    llvm::Value* target = llvm::ConstantPointerNull::get(i8ptr_ty());
    llvm::Value* rules = llvm::ConstantPointerNull::get(i8ptr_ty());
    if (node->left) {
        target = gen_expr(node->left.get());
        if (node->left->next)
            rules = gen_expr(node->left->next.get());
    }
    builder_->CreateCall(style_apply_, { target, rules });
    if (node->body) gen_block(node->body.get());
}

void Codegen::gen_theme(const ASTNode* node) {
    /* Theme: register theme variables in a global theme store */
    static uint64_t theme_id = 0;
    std::string theme_name = node->value.empty() ? "_theme_" + std::to_string(theme_id++) : node->value;
    auto* theme_var = new llvm::GlobalVariable(
        *module_, i8ptr_ty(), false,
        llvm::GlobalValue::InternalLinkage,
        llvm::ConstantPointerNull::get(i8ptr_ty()),
        theme_name + "_data");
    gen_block(node->body.get());
    llvm::Function* fn_theme_apply = module_->getFunction("aurora_style_apply");
    if (fn_theme_apply) {
        llvm::Value* name_str = builder_->CreateGlobalStringPtr(theme_name, "theme_name");
        builder_->CreateCall(fn_theme_apply, { name_str,
            builder_->CreateLoad(i8ptr_ty(), theme_var) });
    }
}

void Codegen::gen_route(const ASTNode* node) {
    if (!route_register_) return;
    llvm::Value* method = builder_->CreateGlobalStringPtr("GET", "rmethod");
    llvm::Value* path = llvm::ConstantPointerNull::get(i8ptr_ty());
    llvm::Value* handler = llvm::ConstantPointerNull::get(i8ptr_ty());
    if (node->left) {
        path = gen_expr(node->left.get());
        if (node->left->next)
            handler = gen_expr(node->left->next.get());
    }
    builder_->CreateCall(route_register_, { method, path, handler });
    if (node->body) gen_block(node->body.get());
}

void Codegen::gen_animate(const ASTNode* node) {
    /* Animation: create animation timeline with frame callback */
    llvm::Function* fn_anim = module_->getFunction("aurora_animation_play");
    if (!fn_anim) return;
    static uint64_t anim_id = 0;
    std::string anim_name = node->value.empty() ? "_anim_" + std::to_string(anim_id++) : node->value;
    /* Wrap body as animation frame callback */
    std::string cb_name = anim_name + "_frame";
    auto* cb_ty = llvm::FunctionType::get(void_ty(), { i8ptr_ty() }, false);
    auto* cb = llvm::Function::Create(
        cb_ty, llvm::Function::InternalLinkage, cb_name, module_.get());
    auto* entry_bb = llvm::BasicBlock::Create(ctx_, "entry", cb);
    auto* saved_fn = cur_fn_;
    auto saved_insert = builder_->saveIP();
    auto saved_cache  = std::move(literal_aurora_cache_);
    builder_->SetInsertPoint(entry_bb);
    cur_fn_ = cb;
    if (node->body) gen_block(node->body.get());
    builder_->CreateRetVoid();
    literal_aurora_cache_ = std::move(saved_cache);
    cur_fn_ = saved_fn;
    builder_->restoreIP(saved_insert);

    llvm::Value* name_str = builder_->CreateGlobalStringPtr(anim_name, "anim_name");
    llvm::Value* cb_ptr = builder_->CreateBitCast(cb, i8ptr_ty(), "anim_cb");
    llvm::Value* duration = i64(1000);
    if (node->left) duration = gen_expr(node->left.get());
    builder_->CreateCall(fn_anim, { name_str, cb_ptr, duration });
}

void Codegen::gen_transition(const ASTNode* node) {
    /* Transition: register a transition effect between states */
    llvm::Function* fn_style = module_->getFunction("aurora_style_apply");
    if (!fn_style) { gen_block(node->body.get()); return; }
    static uint64_t trans_id = 0;
    std::string tname = node->value.empty() ? "_trans_" + std::to_string(trans_id++) : node->value;
    llvm::Value* name_str = builder_->CreateGlobalStringPtr(tname, "trans_name");
    llvm::Value* from_val = llvm::ConstantPointerNull::get(i8ptr_ty());
    llvm::Value* to_val = llvm::ConstantPointerNull::get(i8ptr_ty());
    if (node->left) {
        from_val = gen_expr(node->left.get());
        if (node->left->next) to_val = gen_expr(node->left->next.get());
    }
    builder_->CreateCall(fn_style, { name_str, from_val });
    builder_->CreateCall(fn_style, { name_str, to_val });
    if (node->body) gen_block(node->body.get());
}

/* ════════════════════════════════════════════════════════════
   Domain-specific Codegen — AI/ML
   ════════════════════════════════════════════════════════════ */

void Codegen::gen_ai(const ASTNode* node) {
    /* AI block: initialize ML context, gen body, finalize */
    llvm::Function* fn_tensor_new = module_->getFunction("aurora_tensor_new");
    if (!fn_tensor_new) return;
    const ASTNode* child = node->body.get();
    while (child) {
        if (child->type == NodeType::Train) {
            /* Training block: setup model + training loop */
            gen_tensor_stmt(child);
            /* Generate the training body */
            if (child->body) gen_block(child->body.get());
            /* Call predict after training for verification */
            if (predict_) {
                llvm::Value* model = llvm::ConstantPointerNull::get(i8ptr_ty());
                llvm::Value* input = llvm::ConstantPointerNull::get(i8ptr_ty());
                if (child->left) model = gen_expr(child->left.get());
                if (child->right) input = gen_expr(child->right.get());
                builder_->CreateCall(predict_, { model, input }, "train_result");
            }
        } else if (child->type == NodeType::Predict) {
            gen_ai_predict_stmt(child);
            if (child->body) gen_block(child->body.get());
        } else if (child->type == NodeType::Tensor) {
            gen_tensor_stmt(child);
            if (child->body) gen_block(child->body.get());
        } else if (child->type == NodeType::Neural) {
            /* Neural network layer definition */
            if (child->body) gen_block(child->body.get());
            if (neural_forward_) {
                llvm::Value* input = llvm::ConstantPointerNull::get(i8ptr_ty());
                llvm::Value* weights = llvm::ConstantPointerNull::get(i8ptr_ty());
                llvm::Value* bias = llvm::ConstantPointerNull::get(i8ptr_ty());
                if (child->left) {
                    input = gen_expr(child->left.get());
                    if (child->left->next) weights = gen_expr(child->left->next.get());
                    if (child->left->next && child->left->next->next)
                        bias = gen_expr(child->left->next->next.get());
                }
                builder_->CreateCall(neural_forward_, { input, weights, bias }, "neural_out");
            }
        } else {
            gen_stmt(child);
        }
        child = child->next.get();
    }
}

void Codegen::gen_ai_predict_stmt(const ASTNode* node) {
    if (!predict_) return;
    llvm::Value* model = llvm::ConstantPointerNull::get(i8ptr_ty());
    llvm::Value* input = llvm::ConstantPointerNull::get(i8ptr_ty());
    if (node->left) model = gen_expr(node->left.get());
    if (node->right) input = gen_expr(node->right.get());
    builder_->CreateCall(predict_, { model, input }, "prediction");
}

void Codegen::gen_tensor_stmt(const ASTNode* node) {
    if (!tensor_new_) return;
    llvm::Value* result = gen_tensor_expr(node);
    if (!result) {
        std::vector<llvm::Value*> dims;
        const ASTNode* arg = node->args.get();
        while (arg) {
            dims.push_back(gen_expr(arg));
            arg = arg->next.get();
        }
        auto* i64_ty = llvm::Type::getInt64Ty(ctx_);
        auto* ptr_ty = llvm::PointerType::getUnqual(ctx_);
        auto* shape_alloca = create_entry_alloca(node->value + "_shape",
            llvm::ArrayType::get(i64_ty, dims.size()));
        for (size_t i = 0; i < dims.size(); i++) {
            auto* gep = builder_->CreateInBoundsGEP(
                llvm::ArrayType::get(i64_ty, dims.size()),
                shape_alloca,
                { i64(0), i64(static_cast<int>(i)) });
            builder_->CreateStore(dims[i], gep);
        }
        auto* ndim = i64(static_cast<int64_t>(dims.size()));
        auto* shape_ptr = builder_->CreateBitCast(shape_alloca, ptr_ty);
        builder_->CreateCall(tensor_new_, { ndim, shape_ptr }, "tensor");
    }
}

/* ════════════════════════════════════════════════════════════
   Domain-specific Codegen — Async Extensions
   ════════════════════════════════════════════════════════════ */

void Codegen::gen_parallel(const ASTNode* node) {
    if (!node->body) return;
    /* Wrap the parallel body in an async task and spawn it */
    static uint64_t parallel_id = 0;
    std::string wrapper_name = "_parallel_wrapper_" + std::to_string(parallel_id++);

    auto* wrapper_ty = llvm::FunctionType::get(i8ptr_ty(), { i8ptr_ty() }, false);
    auto* wrapper = llvm::Function::Create(
        wrapper_ty, llvm::Function::InternalLinkage, wrapper_name, module_.get());
    auto* entry_bb = llvm::BasicBlock::Create(ctx_, "entry", wrapper);

    auto* saved_fn = cur_fn_;
    auto saved_insert = builder_->saveIP();
    auto saved_cache  = std::move(literal_aurora_cache_);
    builder_->SetInsertPoint(entry_bb);
    cur_fn_ = wrapper;

    gen_block(node->body.get());
    if (!entry_bb->getTerminator())
        builder_->CreateRet(llvm::ConstantPointerNull::get(i8ptr_ty()));

    literal_aurora_cache_ = std::move(saved_cache);
    cur_fn_ = saved_fn;
    builder_->restoreIP(saved_insert);

    auto* task = builder_->CreateCall(fn_task_create_,
        { wrapper, llvm::ConstantPointerNull::get(i8ptr_ty()) }, "parallel_task");
    builder_->CreateCall(fn_spawn_, { task });
}

void Codegen::gen_thread(const ASTNode* node) {
    if (!node->body) return;
    /* Create an OS thread via task system */
    static uint64_t thread_id = 0;
    std::string wrapper_name = "_thread_wrapper_" + std::to_string(thread_id++);

    auto* wrapper_ty = llvm::FunctionType::get(i8ptr_ty(), { i8ptr_ty() }, false);
    auto* wrapper = llvm::Function::Create(
        wrapper_ty, llvm::Function::InternalLinkage, wrapper_name, module_.get());
    auto* entry_bb = llvm::BasicBlock::Create(ctx_, "entry", wrapper);

    auto* saved_fn = cur_fn_;
    auto saved_insert = builder_->saveIP();
    auto saved_cache  = std::move(literal_aurora_cache_);
    builder_->SetInsertPoint(entry_bb);
    cur_fn_ = wrapper;

    gen_block(node->body.get());
    if (!entry_bb->getTerminator())
        builder_->CreateRet(llvm::ConstantPointerNull::get(i8ptr_ty()));

    literal_aurora_cache_ = std::move(saved_cache);
    cur_fn_ = saved_fn;
    builder_->restoreIP(saved_insert);

    auto* task = builder_->CreateCall(fn_task_create_,
        { wrapper, llvm::ConstantPointerNull::get(i8ptr_ty()) }, "thread_task");
    builder_->CreateCall(fn_spawn_, { task });
}

void Codegen::gen_signal(const ASTNode* node) {
    /* Register a signal handler via the runtime event bus */
    std::string sig_name = node->value.empty() ? "_signal_" : node->value;
    llvm::Value* sig_cstr = builder_->CreateGlobalStringPtr(sig_name, "sig_name");

    /* If there's a body, wrap it as the signal handler function */
    if (node->body) {
        static uint64_t sig_id = 0;
        std::string handler_name = "_sig_handler_" + std::to_string(sig_id++);
        auto* handler_ty = llvm::FunctionType::get(i8ptr_ty(), { i8ptr_ty() }, false);
        auto* handler = llvm::Function::Create(
            handler_ty, llvm::Function::InternalLinkage, handler_name, module_.get());
        auto* entry_bb = llvm::BasicBlock::Create(ctx_, "entry", handler);
        auto* saved_fn = cur_fn_;
        auto saved_insert = builder_->saveIP();
        auto saved_cache  = std::move(literal_aurora_cache_);
        builder_->SetInsertPoint(entry_bb);
        cur_fn_ = handler;
        gen_block(node->body.get());
        if (!entry_bb->getTerminator())
            builder_->CreateRet(llvm::ConstantPointerNull::get(i8ptr_ty()));
        literal_aurora_cache_ = std::move(saved_cache);
        cur_fn_ = saved_fn;
        builder_->restoreIP(saved_insert);

        /* Call aurora_event_on(sig_name, handler, null) */
        auto* handler_cast = builder_->CreateBitCast(handler, i8ptr_ty());
        auto* null_user = llvm::ConstantPointerNull::get(i8ptr_ty());
        if (fn_event_on_)
            builder_->CreateCall(fn_event_on_, { sig_cstr, handler_cast, null_user });
    }
}

void Codegen::gen_emit(const ASTNode* node) {
    if (!node->left) return;
    /* Emit (dispatch) a signal via the runtime event bus */
    std::string sig_name = node->value.empty() ? "_signal_" : node->value;
    llvm::Value* sig_cstr = builder_->CreateGlobalStringPtr(sig_name, "sig_name");
    llvm::Value* arg = gen_expr(node->left.get());
    if (!arg) arg = llvm::ConstantPointerNull::get(i8ptr_ty());
    if (fn_event_emit_)
        builder_->CreateCall(fn_event_emit_, { sig_cstr, arg });
}

/* ════════════════════════════════════════════════════════════
   Domain-specific Codegen — Utility
   ════════════════════════════════════════════════════════════ */

void Codegen::gen_sleep(const ASTNode* node) {
    if (!fn_sleep_) return;
    llvm::Value* ms = i64(1000);
    if (node->left)
        ms = gen_expr(node->left.get());
    builder_->CreateCall(fn_sleep_, { ms });
}

void Codegen::gen_time(const ASTNode* node) {
    if (!fn_time_) return;
    llvm::Value* t = builder_->CreateCall(fn_time_, {}, "time_val");
    /* store result if assigned to a variable */
    if (node->left) {
        auto* slot = create_entry_alloca(node->left->value + "_time", i64_ty());
        builder_->CreateStore(t, slot);
        declare_var(node->left->value + "_time", slot, OwnershipState::Owned);
    }
}

void Codegen::gen_random(const ASTNode* node) {
    if (!fn_random_) return;
    llvm::Value* r = builder_->CreateCall(fn_random_, {}, "random_val");
    if (node->left) {
        auto* slot = create_entry_alloca(node->left->value + "_random", i64_ty());
        builder_->CreateStore(r, slot);
        declare_var(node->left->value + "_random", slot, OwnershipState::Owned);
    }
}

/* ════════════════════════════════════════════════════════════
   FFI — External Function Declaration
   ════════════════════════════════════════════════════════════ */


/* ── yield expr (generator yield) ── */
void Codegen::gen_yield(const ASTNode* node) {
    llvm::Value* val = i64(0);
    if (node->left) {
        val = gen_expr(node->left.get());
        if (val->getType()->isPointerTy())
            val = builder_->CreatePtrToInt(val, i64_ty(), "yield_val");
        else if (val->getType()->isDoubleTy())
            val = builder_->CreateFPToSI(val, i64_ty(), "yield_val");
    }
    if (!yield_fn_) {
        auto* v = llvm::Type::getVoidTy(ctx_);
        yield_fn_ = llvm::Function::Create(
            llvm::FunctionType::get(v, { i64_ty() }, false),
            llvm::Function::ExternalLinkage, "aurora_yield", module_.get());
    }
    if (yield_fn_)
        builder_->CreateCall(yield_fn_, { val });
}