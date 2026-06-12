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

    /* Propagate is_array / is_string from source for type-preserving ops
       (move, copy) — the RHS node type is not NodeType::Array in these cases. */
    auto resolve_flags = [&](bool& out_array, bool& out_string) {
        const ASTNode* rhs = node->right.get();
        if (rhs->type == NodeType::Array) {
            out_array = true; out_string = false; return;
        }
        if (rhs->type == NodeType::Str) {
            out_array = false; out_string = true; return;
        }
        if (rhs->type == NodeType::Copy || rhs->type == NodeType::Move) {
            /* look up source variable's flags */
            const std::string& src_name = rhs->value;
            VarRecord* src = lookup_var(src_name);
            if (src) { out_array = src->is_array; out_string = src->is_string; return; }
        }
        out_array  = false;
        out_string = val->getType()->isPointerTy();
    };

    bool flag_array = false, flag_string = false;
    resolve_flags(flag_array, flag_string);

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
        /* Use strategy-based allocation */
        slot = gen_allocation_for_var(name, val->getType(), init_state, strat);
        builder_->CreateStore(val, slot);
        declare_var(name, slot, init_state);
        auto* r = lookup_var(name);
        r->is_array  = flag_array;
        r->is_string = flag_string;
        rec = r;
    } else {
        auto* alloca_inst = llvm::dyn_cast<llvm::AllocaInst>(rec->alloca_ptr);
        llvm::Type* slot_ty = alloca_inst ? alloca_inst->getAllocatedType() : i64_ty();
        if (slot_ty != val->getType()) {
            auto* new_slot = create_entry_alloca(name + "_r", val->getType());
            builder_->CreateStore(val, new_slot);
            rec->alloca_ptr = new_slot;
        } else {
            builder_->CreateStore(val, rec->alloca_ptr);
        }
        rec->is_array  = flag_array;
        rec->is_string = flag_string;
    }

    /* Track struct type for struct literal variables */
    if (node->right && node->right->type == NodeType::StructLiteral)
        rec->struct_type = node->right->value;

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

    switch (rec->state) {
        case OwnershipState::Owned:
            if (rec->is_array) {
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
    if (rec->is_array) {
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
    (void)node;
}

/* ── output expr ── */
void Codegen::gen_output(const ASTNode* node) {
    if (!node->left) return;

    /* Float literal */
    if (node->left->type == NodeType::Float) {
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
            llvm::Type* alloc_ty = alloca_inst ? alloca_inst->getAllocatedType() : i64_ty();
            llvm::Value* val = builder_->CreateLoad(alloc_ty, rec->alloca_ptr, node->left->value);
            if (rec->is_array)
                builder_->CreateCall(fn_array_print_, { val });
            else if (rec->is_string)
                builder_->CreateCall(fn_print_str_, { val });
            else if (alloc_ty->isDoubleTy())
                builder_->CreateCall(fn_print_float_, { val });
            else
                builder_->CreateCall(fn_printf_, { val });
            return;
        }
    }

    /* Array literal */
    if (node->left->type == NodeType::Array) {
        llvm::Value* arr = gen_array(node->left.get());
        builder_->CreateCall(fn_array_print_, { arr });
        builder_->CreateCall(fn_array_free_,  { arr });
        return;
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
    if (val->getType()->isDoubleTy())
        builder_->CreateCall(fn_print_float_, { val });
    else if (val->getType()->isPointerTy())
        builder_->CreateCall(fn_print_str_, { val });
    else
        builder_->CreateCall(fn_printf_, { val });
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
    /* Function returns i8*, so bitcast i64/int results to i8* */
    if (!val) {
        val = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(i8ptr_ty()));
    } else if (val->getType() != i8ptr_ty()) {
        if (val->getType()->isIntegerTy())
            val = builder_->CreateIntToPtr(val, i8ptr_ty(), "ret_cast");
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
    push_scope();
    gen_block(node->body.get());
    pop_scope_and_drop();
    safe_br(merge_bb);

    /* else / elseif */
    if (node->orelse) {
        builder_->SetInsertPoint(else_bb);
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

    /* Check if the iterable is a variable holding an array */
    if (node->left->type == NodeType::Var) {
        VarRecord* iter_rec = lookup_var(node->left->value);
        if (iter_rec && iter_rec->is_array) {
            /* Array iteration: use aurora_array_len to get the length */
            arr_ptr_val = builder_->CreateLoad(i64_ty(), iter_rec->alloca_ptr,
                                                         node->left->value + "_arr_len");
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

    if (rec->is_array) {
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

    if (rec->is_array) {
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

    /* Build param types — env ptr first if capturing */
    std::vector<std::string> param_names;
    std::vector<llvm::Type*> param_types;
    if (has_captures) {
        param_types.push_back(i8ptr_ty());
        param_names.push_back("__closure_env");
    }
    const ASTNode* p = node->args.get();
    while (p) {
        param_names.push_back(p->value);
        param_types.push_back(i8ptr_ty());
        p = p->next.get();
    }

    auto* fn_type = llvm::FunctionType::get(i8ptr_ty(), param_types, false);
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
            auto* gep = builder_->CreateGEP(i64_ty(), env_i64, i64((int64_t)i), node->captures[i] + "_gep");
            llvm::Value* val = builder_->CreateLoad(i64_ty(), gep, node->captures[i] + "_cap");
            auto* slot = create_entry_alloca(node->captures[i], i64_ty());
            builder_->CreateStore(val, slot);
            declare_var(node->captures[i], slot, OwnershipState::Owned);
        }
    }

    /* Allocate params (skip env if capturing) */
    int param_start = has_captures ? 1 : 0;
    for (int i = param_start; i < (int)param_names.size(); i++) {
        auto* slot = create_entry_alloca(param_names[i], i64_ty());
        llvm::Value* val = fn->getArg(i);
        if (val->getType() != i64_ty())
            val = builder_->CreatePtrToInt(val, i64_ty(), param_names[i] + "_unbox");
        builder_->CreateStore(val, slot);
        declare_var(param_names[i], slot, OwnershipState::Owned);
    }

    gen_block(node->body.get());
    pop_scope_and_drop();
    safe_ret(llvm::ConstantPointerNull::get(i8ptr_ty()));

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

        /* Allocate env array and store captured values */
        size_t ncaptures = node->captures.size();
        auto* env_array_ty = llvm::ArrayType::get(i64_ty(), ncaptures);
        auto* env_alloca = create_entry_alloca(var_name + "_env", env_array_ty);
        auto* env_as_i64 = builder_->CreateBitCast(env_alloca, llvm::PointerType::get(i64_ty(), 0), var_name + "_env_i64");
        for (size_t i = 0; i < ncaptures; i++) {
            VarRecord* cap_rec = lookup_var(node->captures[i]);
            if (cap_rec && cap_rec->alloca_ptr) {
                llvm::Value* cap_val = builder_->CreateLoad(i64_ty(), cap_rec->alloca_ptr, node->captures[i]);
                auto* cap_gep = builder_->CreateGEP(i64_ty(), env_as_i64, i64((int64_t)i), node->captures[i] + "_cap_slot");
                builder_->CreateStore(cap_val, cap_gep);
            }
        }

        /* Store env pointer in closure */
        llvm::Value* env_ptr = builder_->CreateBitCast(env_alloca, i8ptr_ty(), var_name + "_env_ptr");
        auto* env_gep = builder_->CreateStructGEP(closure_ty, closure_alloca, 1, "env_slot");
        builder_->CreateStore(env_ptr, env_gep);

        /* Declare closure variable */
        declare_var(var_name, closure_alloca, OwnershipState::Owned);
        VarRecord* rec = lookup_var(var_name);
        if (rec) rec->is_closure = true;
    } else {
        /* Non-capturing: store function pointer (same as before) */
        auto* fn_ptr = builder_->CreateBitCast(fn, i8ptr_ty(), lambda_name + "_ptr");
        auto* slot = create_entry_alloca(var_name, i8ptr_ty());
        builder_->CreateStore(fn_ptr, slot);
        declare_var(var_name, slot, OwnershipState::Borrowed);
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

    /* The left child is the expression to spawn (expected: function call).
       Create a wrapper function that evaluates the arguments and calls
       the target function, then call aurora_task_create + aurora_spawn. */

    static uint64_t spawn_id = 0;
    std::string wrapper_name = "_spawn_wrapper_" + std::to_string(spawn_id++);

    auto* wrapper_ty = llvm::FunctionType::get(i8ptr_ty(), { i8ptr_ty() }, false);
    auto* wrapper = llvm::Function::Create(
        wrapper_ty, llvm::Function::InternalLinkage, wrapper_name, module_.get());
    auto* entry_bb = llvm::BasicBlock::Create(ctx_, "entry", wrapper);
    llvm::IRBuilder<> wb(entry_bb);

    llvm::Value* wrapper_result = llvm::ConstantPointerNull::get(i8ptr_ty());

    if (node->left->type == NodeType::Call) {
        llvm::Function* callee = module_->getFunction(node->left->value);
        if (callee) {
            std::vector<llvm::Value*> call_args;
            const ASTNode* arg_node = node->left->args.get();
            while (arg_node) {
                /* Evaluate the argument expression inside the wrapper */
                llvm::Value* arg_val = nullptr;
                if (arg_node->type == NodeType::Num) {
                    arg_val = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_),
                        std::stoll(arg_node->value), true);
                } else if (arg_node->type == NodeType::Float) {
                    arg_val = llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx_),
                        std::stod(arg_node->value));
                } else if (arg_node->type == NodeType::Str) {
                    arg_val = wb.CreateGlobalStringPtr(arg_node->value, "spawn_str");
                } else {
                    arg_val = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0, true);
                }
                call_args.push_back(arg_val);
                arg_node = arg_node->next.get();
            }

            /* Match parameter types */
            for (size_t i = 0; i < call_args.size() && i < callee->arg_size(); ++i) {
                llvm::Type* param_ty = callee->getFunctionType()->getParamType(i);
                if (param_ty->isPointerTy() && call_args[i]->getType()->isIntegerTy())
                    call_args[i] = wb.CreateIntToPtr(call_args[i], param_ty);
                else if (param_ty->isIntegerTy() && call_args[i]->getType()->isPointerTy())
                    call_args[i] = wb.CreatePtrToInt(call_args[i], param_ty);
                else if (param_ty != call_args[i]->getType())
                    call_args[i] = wb.CreateBitCast(call_args[i], param_ty);
            }

            call_args.resize(callee->arg_size(),
                             llvm::ConstantPointerNull::get(i8ptr_ty()));

            llvm::Value* ret = wb.CreateCall(callee, call_args, "spawn_call");

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
        { wrapper, llvm::ConstantPointerNull::get(i8ptr_ty()) }, "task");
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
        ASTNode* params = node->body->args.get();

        /* Generate wrapper body: pass arguments through arg struct pointer */
        /* For simplicity, the wrapper receives void* and casts to first arg type */
        if (!entry_bb->getTerminator())
            builder_->CreateRet(llvm::ConstantPointerNull::get(i8ptr_ty()));

        /* Create the callable async function: i8* name(i8* arg) -> task handle */
        auto* fn_ty = llvm::FunctionType::get(i8ptr_ty(), { i8ptr_ty() }, false);
        async_callee = llvm::Function::Create(
            fn_ty, llvm::Function::ExternalLinkage, func_name, module_.get());
        auto* callee_bb = llvm::BasicBlock::Create(ctx_, "entry", async_callee);
        llvm::IRBuilder<> cb(callee_bb);

        /* In the callable: create task from wrapper, spawn, return task handle */
        llvm::Value* arg_ptr = &(*async_callee->arg_begin());
        auto* task = cb.CreateCall(fn_task_create_,
            { wrapper, arg_ptr }, "task");
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
        std::cerr << "Warning: new " << class_name << "() failed - class not found\n";
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
    if (fn_panic) {
        llvm::Value* msg = i64(0);
        if (node->left) {
            msg = gen_expr(node->left.get());
            if (msg && !msg->getType()->isPointerTy())
                msg = builder_->CreateIntToPtr(msg, i8ptr_ty(), "panic_msg");
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
    gen_block(node->body.get());
    builder_->CreateCall(server_start_, { srv });
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
            llvm::Function* fn_route = module_->getFunction("aurora_route_register");
            if (fn_route && !child->value.empty()) {
                llvm::Value* path_str = builder_->CreateGlobalStringPtr(
                    "/" + child->value, "api_route");
                llvm::Function* handler_fn = module_->getFunction(child->value);
                if (handler_fn) {
                    llvm::Value* handler_ptr = builder_->CreateBitCast(handler_fn, i8ptr_ty());
                    builder_->CreateCall(fn_route, { path_str, handler_ptr });
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
    llvm::Value* path = llvm::ConstantPointerNull::get(i8ptr_ty());
    llvm::Value* handler = llvm::ConstantPointerNull::get(i8ptr_ty());
    if (node->left) {
        path = gen_expr(node->left.get());
        if (node->left->next)
            handler = gen_expr(node->left->next.get());
    }
    builder_->CreateCall(route_register_, { path, handler });
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
                { i64(0), i64((int)i) });
            builder_->CreateStore(dims[i], gep);
        }
        auto* ndim = i64((int64_t)dims.size());
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

llvm::Type* Codegen::extern_type_to_llvm(const std::string& type_name) {
    if (type_name == "int" || type_name == "i64" || type_name == "Int" || type_name == "u64")
        return i64_ty();
    if (type_name == "i32" || type_name == "u32")
        return llvm::Type::getInt32Ty(ctx_);
    if (type_name == "i16")
        return llvm::Type::getInt16Ty(ctx_);
    if (type_name == "i8" || type_name == "char")
        return llvm::Type::getInt8Ty(ctx_);
    if (type_name == "float" || type_name == "f64" || type_name == "Float" || type_name == "double")
        return llvm::Type::getDoubleTy(ctx_);
    if (type_name == "f32")
        return llvm::Type::getFloatTy(ctx_);
    if (type_name == "string" || type_name == "String" || type_name == "str" || type_name == "cstring"
        || type_name == "char*" || type_name == "void*"
        || type_name == "pointer" || type_name == "Pointer" || type_name == "ptr")
        return i8ptr_ty();
    if (type_name == "bool" || type_name == "Bool")
        return llvm::Type::getInt8Ty(ctx_); /* C99 _Bool is 1 byte on all platforms */
    if (type_name == "void" || type_name == "Void")
        return llvm::Type::getVoidTy(ctx_);
    /* Check for registered struct types */
    if (global_type_registry().has_struct(type_name))
        return codegen_get_struct_type(ctx_, type_name);
    /* Check for registered enum types */
    if (global_type_registry().has_enum(type_name))
        return i64_ty();
    /* default: i64 (Aurora's generic int type) */
    return i64_ty();
}

/* ── Map calling convention string to LLVM CallingConv::ID ── */
static llvm::CallingConv::ID parse_calling_conv(const std::string& cc) {
    if (cc == "stdcall")  return llvm::CallingConv::X86_StdCall;
    if (cc == "fastcall") return llvm::CallingConv::X86_FastCall;
    if (cc == "thiscall") return llvm::CallingConv::X86_ThisCall;
    if (cc == "vectorcall") return llvm::CallingConv::X86_VectorCall;
    if (cc == "win64")    return llvm::CallingConv::Win64;
    if (cc == "sysv64")   return llvm::CallingConv::X86_64_SysV;
    /* Default: C calling convention */
    return llvm::CallingConv::C;
}

void Codegen::gen_extern_fn(const ASTNode* node) {
    if (!node) return;
    const std::string& fname = node->value;
    if (fname.empty()) return;

    /* Parse calling convention */
    llvm::CallingConv::ID call_conv = parse_calling_conv(node->calling_conv);

    /* Build parameter types + track callback signatures + cstring info */
    std::vector<llvm::Type*> param_types;
    std::vector<CallbackSig> cb_sigs;
    ExternStringInfo str_info;
    const ASTNode* param = node->args.get();
    int pidx = 0;
    while (param) {
        if (param->right && param->right->type == NodeType::FunctionType) {
            const ASTNode* ct  = param->right.get();
            std::vector<llvm::Type*> cb_param_tys;
            const ASTNode* cp = ct->args.get();
            while (cp) {
                cb_param_tys.push_back(extern_type_to_llvm(cp->value));
                cp = cp->next.get();
            }
            llvm::Type* cb_ret = llvm::Type::getVoidTy(ctx_);
            if (ct->left)
                cb_ret = extern_type_to_llvm(ct->left->value);
            auto* cb_fn_type = llvm::FunctionType::get(cb_ret, cb_param_tys, false);
            param_types.push_back(llvm::PointerType::get(cb_fn_type, 0));
            cb_sigs.push_back({pidx, cb_fn_type});
        } else {
            std::string ptype_name = "int";
            if (param->right)
                ptype_name = param->right->value;
            param_types.push_back(extern_type_to_llvm(ptype_name));
            /* Track cstring/pointer params for auto conversion & cost checking */
            if (ptype_name == "cstring" || ptype_name == "char*" ||
                ptype_name == "string" || ptype_name == "String" || ptype_name == "str")
                str_info.param_indices.push_back(pidx);
            if (ptype_name == "pointer" || ptype_name == "Pointer" || ptype_name == "ptr" ||
                ptype_name == "void*" || ptype_name == "cstring" || ptype_name == "char*")
                str_info.has_pointer_param = true;
        }
        pidx++;
        param = param->next.get();
    }
    if (!cb_sigs.empty())
        extern_callback_sigs_[fname] = std::move(cb_sigs);

    /* Return type */
    std::string ret_type_name = "void";
    if (node->left)
        ret_type_name = node->left->value;
    llvm::Type* ret_type = extern_type_to_llvm(ret_type_name);
    /* Track if return type is cstring or pointer */
    if (ret_type_name == "cstring" || ret_type_name == "char*" ||
        ret_type_name == "string" || ret_type_name == "String" || ret_type_name == "str")
        str_info.return_is_cstring = true;
    if (ret_type_name == "pointer" || ret_type_name == "Pointer" || ret_type_name == "ptr" ||
        ret_type_name == "void*")
        str_info.has_pointer_param = true;
    if (!str_info.param_indices.empty() || str_info.return_is_cstring)
        extern_string_info_[fname] = std::move(str_info);

    /* ════════════════════════════════════════════════════════════════════
       Cost checking: verify declared @cost matches actual types
     ════════════════════════════════════════════════════════════════════ */
    if (!node->cost_level.empty()) {
        /* Raw pointer (void*) is zero-cost at the FFI boundary — just a register.
           cstring/char* requires string marshaling = alloc.
           Callbacks require trampoline = indirection. */
        bool needs_marshal = str_info.return_is_cstring || !str_info.param_indices.empty();
        bool has_callback = !cb_sigs.empty();

        std::string actual_cost;
        if (has_callback)          actual_cost = "indirection";
        else if (needs_marshal)    actual_cost = "alloc";
        else                       actual_cost = "zero";

        if (node->cost_level == "zero" && needs_marshal) {
            std::cerr << "\033[1;33mWarning\033[0m: extern function '" << fname
                      << "' declared @cost(zero) but uses string marshaling"
                      << " (actual cost: " << actual_cost << ")\n";
        } else if (node->cost_level == "zero" && has_callback) {
            std::cerr << "\033[1;33mWarning\033[0m: extern function '" << fname
                      << "' declared @cost(zero) but uses callback parameters"
                      << " (actual cost: " << actual_cost << ")\n";
        } else if (node->cost_level == "alloc" && has_callback) {
            std::cerr << "\033[1;33mWarning\033[0m: extern function '" << fname
                      << "' declared @cost(alloc) but uses callback parameters"
                      << " (actual cost: " << actual_cost << ")\n";
        }
    }

    /* ── Case 1: Plain extern (no library) — create LLVM external declaration ── */
    if (!node->right) {
        auto* fn_type = llvm::FunctionType::get(ret_type, param_types, node->is_vararg);
        llvm::Function* fn = llvm::Function::Create(
            fn_type, llvm::Function::ExternalLinkage, fname, module_.get());
        fn->setCallingConv(call_conv);
        /* Add to @llvm.compiler.used so StripDeadPrototypes doesn't remove it */
        {
            auto* i8ptr = llvm::PointerType::getUnqual(ctx_);
            auto* bitcast = llvm::ConstantExpr::getBitCast(fn, i8ptr);
            llvm::GlobalVariable* used_gv = module_->getGlobalVariable("llvm.compiler.used");
            if (used_gv) {
                /* Append to existing array */
                auto* old_arr = llvm::cast<llvm::ConstantArray>(used_gv->getInitializer());
                std::vector<llvm::Constant*> elems;
                for (auto& op : old_arr->operands())
                    elems.push_back(llvm::cast<llvm::Constant>(&op));
                elems.push_back(bitcast);
                auto* arr_ty = llvm::ArrayType::get(i8ptr, elems.size());
                used_gv->setInitializer(llvm::ConstantArray::get(arr_ty, elems));
            } else {
                auto* arr_ty = llvm::ArrayType::get(i8ptr, 1);
                auto* arr = llvm::ConstantArray::get(arr_ty, { bitcast });
                new llvm::GlobalVariable(
                    *module_, arr_ty, false,
                    llvm::GlobalValue::AppendingLinkage,
                    arr, "llvm.compiler.used");
            }
        }
        std::cerr << "[ffi] extern function '" << fname << "' declared\n";
        return;
    }

    /* ── Case 2: extern "libname" — generate lazy dynamic loading wrapper ── */
    const std::string& libname = node->right->value;
    auto* ptr_type = llvm::PointerType::get(ctx_, 0);

    /* Create global function pointer (i8*) initialized to null */
    auto* fn_ptr_global = new llvm::GlobalVariable(
        *module_, ptr_type, false,
        llvm::GlobalValue::InternalLinkage,
        llvm::ConstantPointerNull::get(ptr_type),
        fname + ".dl_ptr");

    /* Create wrapper function with proper signature */
    auto* fn_type = llvm::FunctionType::get(ret_type, param_types, node->is_vararg);
    auto* wrapper = llvm::Function::Create(
        fn_type, llvm::Function::InternalLinkage, fname, module_.get());
    wrapper->setCallingConv(call_conv);

    /* Save current builder state */
    llvm::BasicBlock* saved_block = builder_->GetInsertBlock();

    /* Create basic blocks for the wrapper */
    auto* entry    = llvm::BasicBlock::Create(ctx_, "dl.entry", wrapper);
    auto* resolve  = llvm::BasicBlock::Create(ctx_, "dl.resolve", wrapper);
    auto* call     = llvm::BasicBlock::Create(ctx_, "dl.call", wrapper);

    /* ── entry block: check if ptr is null ── */
    builder_->SetInsertPoint(entry);
    llvm::Value* loaded = builder_->CreateLoad(ptr_type, fn_ptr_global, "dl.ptr");
    llvm::Value* is_null = builder_->CreateIsNull(loaded, "dl.isnull");
    builder_->CreateCondBr(is_null, resolve, call);

    /* ── resolve block: call aurora_dl_resolve or aurora_ecosystem_resolve ── */
    builder_->SetInsertPoint(resolve);

    /* Determine if this is an ecosystem extern (pypi_, npm_, cargo_, native_) */
    bool is_ecosystem = (libname.size() > 5 && libname.substr(0, 5) == "pypi_")
                     || (libname.size() > 4 && libname.substr(0, 4) == "npm_")
                     || (libname.size() > 6 && libname.substr(0, 6) == "cargo_")
                     || (libname.size() > 7 && libname.substr(0, 7) == "native_");

    const char* resolve_fn_name = is_ecosystem
        ? "aurora_ecosystem_resolve" : "aurora_dl_resolve";

    llvm::Function* resolve_fn = module_->getFunction(resolve_fn_name);
    if (!resolve_fn) {
        resolve_fn = llvm::Function::Create(
            llvm::FunctionType::get(ptr_type, { ptr_type, ptr_type }, false),
            llvm::Function::ExternalLinkage, resolve_fn_name, module_.get());
    }

    llvm::Value* lib_cstr = builder_->CreateGlobalStringPtr(libname, "dl.lib");
    llvm::Value* fn_cstr  = builder_->CreateGlobalStringPtr(fname, "dl.fn");
    llvm::Value* resolved = builder_->CreateCall(resolve_fn, { lib_cstr, fn_cstr }, "dl.resolved");

    /* Error recovery: if resolved is null, call aurora_panic and abort */
    auto* err_check = llvm::BasicBlock::Create(ctx_, "dl.errcheck", wrapper);
    auto* err_blk   = llvm::BasicBlock::Create(ctx_, "dl.error", wrapper);
    auto* resolve_cont = llvm::BasicBlock::Create(ctx_, "dl.resolve_cont", wrapper);

    builder_->CreateBr(err_check);

    builder_->SetInsertPoint(err_check);
    llvm::Value* is_err = builder_->CreateIsNull(resolved, "dl.errnull");
    builder_->CreateCondBr(is_err, err_blk, resolve_cont);

    builder_->SetInsertPoint(err_blk);
    std::string err_msg = "[ffi] ERROR: failed to resolve '" + fname + "' from "
        + (is_ecosystem ? "ecosystem " : "library '") + libname
        + (is_ecosystem ? "" : "'") + "\n";
    llvm::Value* err_msg_str = builder_->CreateGlobalStringPtr(err_msg, "dl.errmsg");
    llvm::Function* panic_fn = module_->getFunction("aurora_panic");
    if (!panic_fn) {
        auto* void_ty = llvm::Type::getVoidTy(ctx_);
        auto* pt = llvm::PointerType::getUnqual(ctx_);
        auto* ft = llvm::FunctionType::get(void_ty, { pt }, false);
        panic_fn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "aurora_panic", module_.get());
    }
    builder_->CreateCall(panic_fn, { err_msg_str });
    builder_->CreateUnreachable();

    builder_->SetInsertPoint(resolve_cont);
    builder_->CreateStore(resolved, fn_ptr_global);
    builder_->CreateBr(call);

    /* ── call block: call through resolved function pointer ── */
    builder_->SetInsertPoint(call);

    /* PHI node: entry → loaded (pre-resolved), resolve_cont → resolved (just resolved) */
    llvm::PHINode* fn_ptr_phi = builder_->CreatePHI(ptr_type, 2, "dl.fnphi");
    fn_ptr_phi->addIncoming(loaded, entry);
    fn_ptr_phi->addIncoming(resolved, resolve_cont);

    /* Bitcast void* to proper function pointer type */
    auto* fn_ptr_typed = builder_->CreateBitCast(fn_ptr_phi, llvm::PointerType::get(fn_type, 0), "dl.fncast");

    /* Collect the wrapper function's own parameters (passed from caller) */
    std::vector<llvm::Value*> args;
    for (auto& arg : wrapper->args()) {
        args.push_back(&arg);
    }

    /* Create call through function pointer */
    std::string call_name = ret_type->isVoidTy() ? "" : "dl.call";
    llvm::CallInst* call_result = builder_->CreateCall(fn_type, fn_ptr_typed, args, call_name);
    builder_->CreateRet(ret_type->isVoidTy() ? nullptr : call_result);

    /* Restore builder state */
    if (saved_block)
        builder_->SetInsertPoint(saved_block);

    std::cerr << "[ffi] extern function '" << fname
              << "' from library '" << libname
              << "' declared (dynamic loading)\n";
}

/* ════════════════════════════════════════════════════════════
   FFI — External Struct Declaration
   ════════════════════════════════════════════════════════════ */
void Codegen::gen_extern_struct(const ASTNode* node) {
    if (!node) return;
    const std::string& sname = node->value;
    /* Creating the LLVM struct type registers it in the module */
    codegen_get_struct_type(ctx_, sname);
    std::cerr << "[ffi] extern struct '" << sname << "' declared\n";
}

/* ════════════════════════════════════════════════════════════
   FFI — External Union Declaration
   ════════════════════════════════════════════════════════════ */
void Codegen::gen_extern_union(const ASTNode* node) {
    if (!node) return;
    const std::string& uname = node->value;
    /* Creating the LLVM union type registers it (only largest field is stored) */
    codegen_get_struct_type(ctx_, uname);
    std::cerr << "[ffi] extern union '" << uname << "' declared\n";
}

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
