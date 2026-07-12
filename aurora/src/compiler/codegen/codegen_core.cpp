#include "compiler/codegen.hpp"
#include "compiler/class_oop.hpp"

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Host.h>

#include <stdexcept>
#include <sstream>

/* ════════════════════════════════════════════════════════════
   Constructor
   ════════════════════════════════════════════════════════════ */
Codegen::Codegen(llvm::LLVMContext& ctx,
                 std::unique_ptr<llvm::Module>& module,
                 std::unique_ptr<llvm::IRBuilder<>>& builder)
    : ctx_(ctx), module_(module), builder_(builder)
{}

void Codegen::set_source_file(const std::string& path) {
    source_file_path_ = path;
}

void Codegen::init_debug_info() {
    if (!dibuilder_ || !debug_enabled_) return;
    std::string filename = source_file_path_;
    std::string dir;
    auto pos = filename.find_last_of("/\\");
    if (pos != std::string::npos) {
        dir = filename.substr(0, pos);
        filename = filename.substr(pos + 1);
    }
    debug_file_ = dibuilder_->createFile(filename, dir);
    debug_cu_ = dibuilder_->createCompileUnit(
        llvm::dwarf::DW_LANG_C,
        debug_file_,
        "Aurora Compiler",
        false, "", 0);
}

llvm::DIType* Codegen::get_debug_type(AstTypeKind kind) {
    if (!dibuilder_) return nullptr;
    switch (kind) {
        case AstTypeKind::Int:
        case AstTypeKind::Bool:
            return dibuilder_->createBasicType("int", 64, llvm::dwarf::DW_ATE_signed);
        case AstTypeKind::Float:
            return dibuilder_->createBasicType("float", 64, llvm::dwarf::DW_ATE_float);
        case AstTypeKind::String:
            return dibuilder_->createPointerType(
                dibuilder_->createBasicType("char", 8, llvm::dwarf::DW_ATE_signed_char),
                64);
        case AstTypeKind::Void:
            return nullptr;
        default:
            return dibuilder_->createBasicType("ptr", 64, llvm::dwarf::DW_ATE_address);
    }
}

void Codegen::emit_coverage_trace(int line) {
    if (!coverage_enabled_ || !cur_fn_ || !fn_coverage_trace_) return;
    if (!source_file_ptr_) return;
    builder_->CreateCall(fn_coverage_trace_, {source_file_ptr_, i64(line)});
}

/* ════════════════════════════════════════════════════════════
   Main entry point
   ════════════════════════════════════════════════════════════ */
void Codegen::generate(const ASTNode* root) {
    /* Step 0 — clear OOP caches for fresh compilation */
    oop_clear();
    oop_clear_object_types();
    oop_clear_vtable_cache();

    /* Step 0 — ensure LLVM native target is initialized */
    ensure_llvm_init();

    /* Step 0 — set target info for LLVM optimization */
    auto triple_str = llvm::sys::getProcessTriple();
    module_->setTargetTriple(triple_str);
    module_->setDataLayout(llvm_target_data_layout(triple_str));

    /* Step 0a — initialize DWARF debug info */
    if (debug_enabled_ && !source_file_path_.empty())
        init_debug_info();

    /* Step 1 — run type checking before LLVM IR is emitted. */
    TypeChecker type_checker;
    type_checker.analyse(root);

    /* Step 1b — create forward declarations for concrete generic functions.
       These must exist before the AST walk so gen_call can resolve them
       by mangled name. Bodies are emitted in Step 9. */
    {
        auto& concrete_generics = type_checker.get_concrete_generics();
        for (auto& [mangled, info] : concrete_generics) {
            if (info.generic_node && !module_->getFunction(mangled)) {
                std::vector<llvm::Type*> param_types;
                for (auto k : info.param_kinds)
                    param_types.push_back(ast_kind_to_abi_type(ctx_, k, i64_ty()));
                auto* ret_ty = ast_kind_to_abi_type(ctx_, info.result_kind, i8ptr_ty());
                auto* fn_type = llvm::FunctionType::get(ret_ty, param_types, false);
                llvm::Function::Create(fn_type, llvm::Function::ExternalLinkage,
                                       mangled, module_.get());
            }
        }
    }

    /* Step 2 — run ownership analysis (throws OwnershipError on violations) */
    ownership_.analyse(root);

    /* Step 3 — identify closure-returning functions so callers can
       propagate the `is_closure` flag on receiving variables. */
    scan_closure_returning_fns(root);

    /* Step 4 — declare runtime helpers */
    declare_runtime_helpers();

    /* Step 5 — check if the user defines "function main():" in the AST.
       If so, we need to avoid a name collision with the synthetic entry
       point we create for top-level code. */
    bool user_has_main = false;
    {
        const ASTNode* n = root;
        while (n) {
            if (n->type == NodeType::Function || n->type == NodeType::PerformanceFn) {
                if (n->value == "main") { user_has_main = true; break; }
            }
            n = n->next.get();
        }
    }

    /* Step 6 — create a function for top-level code.  If the user also
       has "function main():", use a temp name to avoid a collision so
       the user gets "main" and the top-level block gets "__entry".
       Use CRT-compatible signature: int main(int argc, char** argv). */
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(ctx_);
    auto* ptr_ty = llvm::PointerType::get(ctx_, 0);
    auto* entry_fn_type = llvm::FunctionType::get(
        i32_ty, { i32_ty, ptr_ty }, false);
    const char* entry_name = user_has_main ? "__entry" : "main";

    cur_fn_ = llvm::Function::Create(
        entry_fn_type, llvm::Function::ExternalLinkage, entry_name, module_.get()
    );
    /* Name the parameters for clarity even if unused */
    if (!user_has_main) {
        cur_fn_->arg_begin()->setName("argc");
        (cur_fn_->arg_begin() + 1)->setName("argv");
    }
    auto* entry_bb = llvm::BasicBlock::Create(ctx_, "entry", cur_fn_);
    builder_->SetInsertPoint(entry_bb);

    /* Create DISubprogram for the entry function (debug info) */
    if (dibuilder_ && debug_file_) {
        debug_cur_fn_ = dibuilder_->createFunction(
            debug_file_,
            entry_name,
            cur_fn_->getName(),
            debug_file_,
            0,
            dibuilder_->createSubroutineType(
                dibuilder_->getOrCreateTypeArray(std::nullopt)),
            0,
            llvm::DINode::FlagZero,
            llvm::DISubprogram::SPFlagDefinition);
        if (debug_cur_fn_)
            cur_fn_->setSubprogram(debug_cur_fn_);
    }

    /* Initialize coverage source file ptr now that builder has insertion point */
    if (!source_file_path_.empty())
        source_file_ptr_ = builder_->CreateGlobalStringPtr(source_file_path_, "cov_file");

    /* Install crash handler at program start */
    {
        llvm::Function* init_crash = module_->getFunction("aurora_install_crash_handler");
        if (!init_crash) {
            auto* void_ty = llvm::Type::getVoidTy(ctx_);
            init_crash = llvm::Function::Create(
                llvm::FunctionType::get(void_ty, {}, false),
                llvm::Function::ExternalLinkage,
                "aurora_install_crash_handler", module_.get());
        }
        builder_->CreateCall(init_crash, {});
    }

    /* Step 7 — generate IR for the AST */
    push_scope();
    gen_block(root);
    pop_scope_and_drop();

    /* Return 0 from entry function */
    if (!current_block_terminated())
        safe_ret(llvm::ConstantInt::get(i32_ty, 0));

    /* Step 8 — forward-declare concrete generic function instances so they
       are visible during main() codegen. */
    {
        auto& concrete_generics = type_checker.get_concrete_generics();
        for (auto& [mangled, info] : concrete_generics) {
            if (!info.generic_node) continue;
            if (module_->getFunction(mangled)) continue;
            std::vector<llvm::Type*> param_types;
            for (auto k : info.param_kinds)
                param_types.push_back(ast_kind_to_abi_type(ctx_, k, i64_ty()));
            auto* ret_ty = ast_kind_to_abi_type(ctx_, info.result_kind, i8ptr_ty());
            auto* fn_type = llvm::FunctionType::get(ret_ty, param_types, false);
            llvm::Function::Create(
                fn_type, llvm::Function::ExternalLinkage,
                mangled, module_.get());
        }
    }

    /* Step 9 — if the user defined "function main():", rename the user's
       function to "main_user" and create a real @main(i32, ptr) entry point
       matching the CRT's expected main(argc, argv) signature. */
    if (user_has_main) {
        llvm::Function* user_main = module_->getFunction("main");
        if (user_main && user_main != cur_fn_ &&
            user_main->arg_size() == 0) {
            user_main->setName("main_user");

            llvm::Function* entry_fn = module_->getFunction("__entry");

            /* CRT signature: int main(int argc, char** argv) */
            auto* main_fn_type = llvm::FunctionType::get(
                i32_ty, { i32_ty, ptr_ty }, false);

            cur_fn_ = llvm::Function::Create(
                main_fn_type, llvm::Function::ExternalLinkage, "main", module_.get()
            );
            auto* argc_arg = cur_fn_->arg_begin();
            auto* argv_arg = cur_fn_->arg_begin() + 1;
            argc_arg->setName("argc");
            argv_arg->setName("argv");

            auto* mb = llvm::BasicBlock::Create(ctx_, "entry", cur_fn_);
            builder_->SetInsertPoint(mb);

            /* Create DISubprogram for wrapping main (debug info) */
            if (dibuilder_ && debug_file_) {
                auto* main_sp = dibuilder_->createFunction(
                    debug_file_,
                    "main",
                    cur_fn_->getName(),
                    debug_file_,
                    0,
                    dibuilder_->createSubroutineType(
                        dibuilder_->getOrCreateTypeArray(std::nullopt)),
                    0,
                    llvm::DINode::FlagZero,
                    llvm::DISubprogram::SPFlagDefinition);
                if (main_sp)
                    cur_fn_->setSubprogram(main_sp);
            }

            /* Install crash handler */
            {
                llvm::Function* init_crash = module_->getFunction("aurora_install_crash_handler");
                if (init_crash)
                    builder_->CreateCall(init_crash, {});
            }

            if (entry_fn)
                builder_->CreateCall(entry_fn, { argc_arg, argv_arg });
            builder_->CreateCall(user_main, {});
            builder_->CreateRet(llvm::ConstantInt::get(i32_ty, 0));
        }
    }

    /* Step 9 — emit concrete generic function instantiations (monomorphization).
       These use the original generic function body but with
       resolved param/result types and mangled names. */
    {
        auto& concrete_generics = type_checker.get_concrete_generics();
        for (auto& [mangled, info] : concrete_generics) {
            if (info.generic_node && info.generic_node->body) {
                gen_generic_instance(mangled, info.generic_node,
                                     info.param_kinds, info.result_kind);
            }
        }
    }

    /* Step 10 — finalize DWARF debug info (must be after all codegen) */
    if (dibuilder_)
        dibuilder_->finalize();
}

/* ════════════════════════════════════════════════════════════
   Strategy-based specialized codegen dispatch
   ════════════════════════════════════════════════════════════ */

llvm::Value* Codegen::gen_allocation_for_var(const std::string& name,
                                              llvm::Type* ty,
                                              OwnershipState state,
                                              AllocStrategy strategy) {
    llvm::PointerType* ptr_ty = llvm::PointerType::getUnqual(ctx_);

    /* Resolve forced strategies to base */
    if (is_forced_strategy(strategy))
        strategy = forced_to_base(strategy);

    /* Strategy-specific allocation */
    switch (strategy) {
        case AllocStrategy::Arena: {
            /* Arena allocation: call aurora_arena_alloc */
            llvm::Value* raw = builder_->CreateCall(fn_arena_alloc_, { i64(8) },
                                                     name + "_arena");
            /* Use i64 slot type to match gen_var's fallback load type */
            return builder_->CreateBitCast(raw, i64_ty()->getPointerTo(), name);
        }
        case AllocStrategy::GC: {
            /* GC allocation: call aurora_gc_alloc */
            llvm::Value* raw = builder_->CreateCall(fn_gc_alloc_, { i64(8) },
                                                     name + "_gc");
            /* Use i64 slot type to match gen_var's fallback load type */
            return builder_->CreateBitCast(raw, i64_ty()->getPointerTo(), name);
        }
        default:
            break;
    }

    /* Module-level variables: use GlobalVariable (original type preserved)
       so they are accessible from any function (not just the entry fn). */
    if (scopes_.size() == 1) {
        llvm::Constant* gv_init = nullptr;
        if (ty->isPointerTy()) {
            gv_init = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(ctx_));
        } else if (ty->isDoubleTy()) {
            gv_init = llvm::ConstantFP::get(ty, 0.0);
        } else {
            gv_init = llvm::ConstantInt::get(ty, 0);
        }
        auto* gv = new llvm::GlobalVariable(
            *module_, ty, false,
            llvm::GlobalVariable::InternalLinkage,
            gv_init, name);
        return gv;
    }

    llvm::Value* ptr = create_entry_alloca(name, ty);

    /* Emit ownership-specific setup based on state */
    switch (state) {
        case OwnershipState::Shared: {
            /* Shared: allocate SharedBox wrapper (only for pointer types) */
            if (ty->isPointerTy()) {
                llvm::Value* shared_box = builder_->CreateCall(
                    module_->getOrInsertFunction("aurora_shared_new",
                        llvm::FunctionType::get(i8ptr_ty(),
                            {i8ptr_ty(), i8ptr_ty()}, false)),
                    {ptr, llvm::ConstantPointerNull::get(ptr_ty)});
                builder_->CreateStore(shared_box, ptr);
            }
            break;
        }
        case OwnershipState::Weak: {
            /* Weak: allocate weak reference */
            llvm::Value* weak_box = builder_->CreateCall(
                module_->getOrInsertFunction("aurora_weak_new",
                    llvm::FunctionType::get(i8ptr_ty(),
                        {i8ptr_ty()}, false)),
                {llvm::ConstantPointerNull::get(ptr_ty)});
            builder_->CreateStore(weak_box, ptr);
            break;
        }
        case OwnershipState::Borrowed: {
            /* Borrowed: store null initially */
            builder_->CreateStore(llvm::ConstantPointerNull::get(ptr_ty), ptr);
            break;
        }
        default:
            /* Owned: plain alloca, no wrapper needed */
            break;
    }

    return ptr;
}

void Codegen::dump() const {
    module_->print(llvm::outs(), nullptr);
}
/* ════════════════════════════════════════════════════════════
   Scope management
   ════════════════════════════════════════════════════════════ */
void Codegen::push_scope() {
    scopes_.emplace_back();
}

void Codegen::pop_scope_and_drop() {
    if (scopes_.empty()) return;
    CodegenScope& top = scopes_.back();
    emit_scope_cleanup(top);
    scopes_.pop_back();
}

void Codegen::emit_scope_cleanup(CodegenScope& scope) {
    if (current_block_terminated()) return;

    for (auto& kv : scope.vars) {
        const VarRecord& rec = kv.second;
        if (!rec.alloca_ptr || current_block_terminated()) continue;
        if (rec.state == OwnershipState::Moved) continue;

        auto* slot_ptr = rec.alloca_ptr;

        /* Skip GlobalVariable-backed variables in per-function cleanup.
           Module-level globals are shared state and must only be cleaned
           up at program exit (when the module scope is popped by the top-
           level caller). Premature cleanup causes use-after-free when
           multiple functions reference the same global. */
        if (llvm::dyn_cast<llvm::GlobalVariable>(slot_ptr))
            continue;

        auto* alloca_inst = llvm::dyn_cast<llvm::AllocaInst>(slot_ptr);
        llvm::Type* slot_ty = alloca_inst ? alloca_inst->getAllocatedType()
                                       : nullptr;
        llvm::Value* value = nullptr;

        if (slot_ty)
            value = builder_->CreateLoad(slot_ty, slot_ptr, kv.first + "_cleanup");

        if (rec.type_kind == AstTypeKind::Array && value && value->getType()->isIntegerTy(64)) {
            builder_->CreateCall(fn_array_free_, { value });
            continue;
        }
        if (rec.type_kind == AstTypeKind::String) continue;

        switch (rec.state) {
            case OwnershipState::Shared:
                if (value && value->getType()->isPointerTy())
                    emit_refcount_dec(value);
                break;
            case OwnershipState::Weak:
                if (value && value->getType()->isPointerTy())
                    emit_weak_release(value);
                break;
            case OwnershipState::Owned:
                /* Call drop_glue for owned heap-allocated pointers */
                if (value && value->getType()->isPointerTy())
                    emit_drop(value);
                break;
            default:
                break;
        }

        /* Unregister from GC if this variable was registered as a root */
        if (rec.is_gc_root && value && value->getType()->isPointerTy()) {
            auto* cast_val = builder_->CreateBitCast(value, i8ptr_ty(), kv.first + "_gc_cleanup");
            emit_gc_unregister_root(cast_val);
        }
    }
}

void Codegen::emit_all_scope_cleanup() {
    for (int i = static_cast<int>(scopes_.size()) - 1; i >= 0; --i) {
        emit_scope_cleanup(scopes_[i]);
        if (current_block_terminated()) break;
    }
}

void Codegen::declare_var(const std::string& name,
                          llvm::Value*       alloca_ptr,
                          OwnershipState     state,
                          AstTypeKind        type_kind) {
    if (scopes_.empty()) push_scope();
    scopes_.back().vars[name] = VarRecord{ alloca_ptr, state };
    scopes_.back().vars[name].type_kind = type_kind;

    /* Emit llvm.dbg.declare for user-visible variables */
    if (dibuilder_ && debug_cur_fn_ && !name.empty() && name[0] != '_' && type_kind != AstTypeKind::Unknown) {
        if (auto* AI = llvm::dyn_cast<llvm::AllocaInst>(alloca_ptr)) {
            auto* dbg_ty = get_debug_type(type_kind);
            if (!dbg_ty) dbg_ty = get_debug_type(AstTypeKind::Int);
            if (!dbg_ty) dbg_ty = dibuilder_->createBasicType("int", 64, llvm::dwarf::DW_ATE_signed);
            llvm::DILocation* cur_loc = builder_->getCurrentDebugLocation();
            unsigned line = cur_loc ? cur_loc->getLine() : 0;
            unsigned col = cur_loc ? cur_loc->getColumn() : 0;
            auto* var = dibuilder_->createAutoVariable(
                debug_cur_fn_, name, debug_file_, line, dbg_ty, true);
            dibuilder_->insertDeclare(
                AI, var, dibuilder_->createExpression(),
                llvm::DILocation::get(ctx_, line, col, debug_cur_fn_),
                &*cur_fn_->getEntryBlock().begin());
        }
    }
}

VarRecord* Codegen::lookup_var(const std::string& name) {
    for (int i = static_cast<int>(scopes_.size()) - 1; i >= 0; i--) {
        auto* r = scopes_[i].find(name);
        if (r) return r;
    }
    return nullptr;
}

llvm::AllocaInst* Codegen::create_entry_alloca(const std::string& name,
                                                llvm::Type*        ty) {
    /* Insert alloca at the very start of the entry block (mem2reg friendly). */
    llvm::IRBuilder<> tmp_builder(
        &cur_fn_->getEntryBlock(),
         cur_fn_->getEntryBlock().begin());
    auto* alloca = tmp_builder.CreateAlloca(ty, nullptr, name);

    return alloca;
}

/* ════════════════════════════════════════════════════════════
   Emission helpers
   ════════════════════════════════════════════════════════════ */
void Codegen::emit_drop(llvm::Value* ptr) {
    /* Call drop_glue for RAII cleanup of heap-allocated types */
    if (!ptr) return;
    builder_->CreateCall(fn_drop_glue_, { ptr });
}

void Codegen::emit_refcount_inc(llvm::Value* ptr) {
    builder_->CreateCall(fn_refcount_inc_, { ptr });
}

void Codegen::emit_refcount_dec(llvm::Value* ptr) {
    builder_->CreateCall(fn_refcount_dec_, { ptr });
}

llvm::Value* Codegen::emit_weak_lock(llvm::Value* ptr) {
    return builder_->CreateCall(fn_weak_lock_, { ptr }, "weak_locked");
}

void Codegen::emit_weak_release(llvm::Value* ptr) {
    builder_->CreateCall(fn_weak_release_, { ptr });
}

void Codegen::emit_gc_register_root(llvm::Value* ptr) {
    if (!ptr) return;
    builder_->CreateCall(fn_gc_register_root_, { ptr });
}

void Codegen::emit_gc_unregister_root(llvm::Value* ptr) {
    if (!ptr) return;
    builder_->CreateCall(fn_gc_unregister_root_, { ptr });
}

void Codegen::emit_poison(llvm::Value* ptr) {
    /* Write PoisonValue into the alloca so any load from it is UB-flagged. */
    auto* alloca_inst = llvm::dyn_cast<llvm::AllocaInst>(ptr);
    llvm::Type* ty = alloca_inst ? alloca_inst->getAllocatedType() : i64_ty();
    auto* poison = llvm::PoisonValue::get(ty);
    builder_->CreateStore(poison, ptr);
}

bool Codegen::current_block_terminated() const {
    auto* bb = builder_->GetInsertBlock();
    return !bb || bb->getTerminator() != nullptr;
}

bool Codegen::safe_br(llvm::BasicBlock* target) {
    if (current_block_terminated()) return false;
    builder_->CreateBr(target);
    return true;
}

bool Codegen::safe_ret(llvm::Value* value) {
    if (current_block_terminated()) return false;
    builder_->CreateRet(value);
    return true;
}

/* ════════════════════════════════════════════════════════════
   Block / Statement dispatcher
   ════════════════════════════════════════════════════════════ */
void Codegen::gen_block(const ASTNode* node) {
    while (node) {
        if (dibuilder_ && node->src_line > 0 && debug_cur_fn_)
            builder_->SetCurrentDebugLocation(
                llvm::DILocation::get(ctx_, node->src_line, node->src_col, debug_cur_fn_));
        gen_stmt(node);
        if (current_block_terminated()) break;
        node = node->next.get();
    }
}

void Codegen::gen_stmt(const ASTNode* node) {
    if (!node) return;
    switch (node->type) {
        case NodeType::Assign:        gen_assign(node);       break;
        case NodeType::IndexAssign:   gen_index_assign(node); break;
        case NodeType::Move:          gen_move(node);         break;
        case NodeType::Drop:          gen_drop(node);         break;
        case NodeType::Delete:        gen_delete(node);       break;
        case NodeType::Copy:          gen_copy(node);         break;
        case NodeType::Free:          gen_free(node);         break;
        case NodeType::SharedRef:     gen_shared_ref(node);   break;
        case NodeType::WeakRef:       gen_weak_ref(node);     break;
        case NodeType::Borrow:        gen_borrow(node);       break;
        case NodeType::Output:        gen_output(node);       break;
        case NodeType::Return:        gen_return(node);       break;
        case NodeType::If:            gen_if(node);           break;
        case NodeType::Match:         gen_match(node);        break;
        case NodeType::While:         gen_while(node);        break;
        case NodeType::Loop:          gen_loop(node);         break;
        case NodeType::Repeat:        gen_repeat(node);       break;
        case NodeType::For:           gen_for(node);          break;
        case NodeType::Function:      gen_function(node);     break;
        case NodeType::PerformanceFn: gen_function(node);     break;  /* same as Function */
        case NodeType::Lambda:        gen_lambda(node);       break;
        case NodeType::Try:           gen_try(node);          break;
        case NodeType::Throw:         gen_throw(node);        break;
        case NodeType::Call:          gen_call(node);         break;
        case NodeType::StructDecl:    gen_struct_decl(node);  break;
        case NodeType::EnumDecl:      gen_enum_decl(node);    break;
        case NodeType::TypeAlias:     gen_type_alias(node);   break;
        case NodeType::InterfaceDecl: gen_interface_decl(node); break;
        case NodeType::NamespaceDecl: gen_block(node->body.get()); break;
        case NodeType::UnsafeBlock:   gen_unsafe_block(node); break;
        case NodeType::SafeBlock:     gen_safe_block(node);   break;
        case NodeType::Parallel:      gen_parallel(node);      break;
        case NodeType::Thread:        gen_thread(node);        break;
        case NodeType::Callback:      gen_block(node->body.get()); break;
        case NodeType::Event:         gen_block(node->body.get()); break;
        case NodeType::Signal:        gen_signal(node);        break;
        case NodeType::Emit:          gen_emit(node);          break;
        case NodeType::Inline:
            if (node->left) {
                gen_stmt(node->left.get());
                if (cur_fn_) cur_fn_->addFnAttr(llvm::Attribute::AlwaysInline);
            }
            break;
        case NodeType::NoInline:
            if (node->left) {
                gen_stmt(node->left.get());
                if (cur_fn_) cur_fn_->addFnAttr(llvm::Attribute::NoInline);
            }
            break;
        case NodeType::ConstExpr:
            if (node->left) {
                gen_stmt(node->left.get());
                if (node->left->type == NodeType::Function && cur_fn_)
                    cur_fn_->addFnAttr(llvm::Attribute::ReadNone);
                /* Variables: gen_stmt stores the value; no extra attr needed */
            }
            break;
        case NodeType::Component:     gen_component(node);     break;
        case NodeType::State: {
            /* State: allocate state variable with UI state tracking */
            std::string state_name = node->value.empty() ? "_state" : node->value;
            auto* slot = create_entry_alloca(state_name, i64_ty());
            llvm::Value* init_val = i64(0);
            if (node->left) init_val = gen_expr(node->left.get());
            builder_->CreateStore(init_val, slot);
            declare_var(state_name, slot, OwnershipState::Owned);
            if (node->body) gen_block(node->body.get());
            break;
        }
        case NodeType::Properties: {
            /* Properties: allocate properties for the enclosing component */
            const ASTNode* child = node->body.get();
            while (child) {
                if (child->type == NodeType::Assign || child->type == NodeType::Var) {
                    gen_stmt(child);
                } else if (!child->value.empty()) {
                    /* Named property: allocate slot with default */
                    auto* slot = create_entry_alloca("prop_" + child->value, i64_ty());
                    llvm::Value* init = i64(0);
                    if (child->left) init = gen_expr(child->left.get());
                    builder_->CreateStore(init, slot);
                    declare_var("prop_" + child->value, slot, OwnershipState::Owned);
                }
                child = child->next.get();
            }
            break;
        }
        case NodeType::Render: {
            /* Render: render the component tree */
            if (node->left) gen_expr(node->left.get());
            if (comp_render_tree_ && ui_init_) {
                builder_->CreateCall(ui_init_, {});
                /* Load root component from the last created component */
                VarRecord* root_rec = lookup_var("__parent_comp");
                if (root_rec && root_rec->alloca_ptr) {
                    llvm::Value* root = builder_->CreateLoad(i8ptr_ty(), root_rec->alloca_ptr, "root_comp");
                    builder_->CreateCall(comp_render_tree_, { root });
                }
            } else {
                auto* fn_render = module_->getFunction("aurora_ui_render");
                if (fn_render) builder_->CreateCall(fn_render, {});
            }
            if (node->body) gen_block(node->body.get());
            break;
        }
        case NodeType::Style:         gen_style(node);         break;
        case NodeType::Theme:         gen_theme(node);         break;
        case NodeType::Route:         gen_route(node);         break;
        case NodeType::Page: {
            /* Page: create component and register route */
            std::string page_path = node->value.empty() ? "/" : "/" + node->value;
            llvm::Value* path_str = builder_->CreateGlobalStringPtr(page_path, "page_path");

            if (comp_create_ && node->body) {
                /* Create a component for the page body */
                llvm::Value* name_str = builder_->CreateGlobalStringPtr(
                    node->value.empty() ? "page" : node->value, "page_name");
                auto* z32 = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_), 0);
                llvm::Value* comp = builder_->CreateCall(comp_create_,
                    { name_str, z32, z32,
                      llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_), 800),
                      llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_), 600) },
                    "page_comp");

                /* Create render callback for the page */
                static uint64_t page_id = 0;
                std::string render_name = "_page_render_" + (node->value.empty() ? std::to_string(page_id++) : node->value);
                auto* render_ty = llvm::FunctionType::get(void_ty(),
                    { i8ptr_ty(), llvm::Type::getInt32Ty(ctx_),
                      llvm::Type::getInt32Ty(ctx_), llvm::Type::getInt32Ty(ctx_),
                      llvm::Type::getInt32Ty(ctx_) }, false);
                auto* render_fn = llvm::Function::Create(
                    render_ty, llvm::Function::InternalLinkage, render_name, module_.get());
                auto* entry_bb = llvm::BasicBlock::Create(ctx_, "entry", render_fn);
                auto* saved_fn = cur_fn_;
                auto saved_insert = builder_->saveIP();
                auto* saved_cache_ptr = &literal_aurora_cache_;
                builder_->SetInsertPoint(entry_bb);
                cur_fn_ = render_fn;

                gen_block(node->body.get());
                builder_->CreateRetVoid();

                literal_aurora_cache_ = std::move(*saved_cache_ptr);
                cur_fn_ = saved_fn;
                builder_->restoreIP(saved_insert);

                auto* render_cast = builder_->CreateBitCast(render_fn, i8ptr_ty());
                builder_->CreateCall(comp_set_render_, { comp, render_cast });
                builder_->CreateCall(comp_mount_, { comp });

                auto* handler_cast = builder_->CreateBitCast(render_fn, i8ptr_ty());
                if (route_register_) {
                    llvm::Value* ui_method = builder_->CreateGlobalStringPtr("GET", "ui_method");
                    builder_->CreateCall(route_register_, { ui_method, path_str, handler_cast });
                }
            } else {
                if (route_register_) {
                    auto* handler = llvm::ConstantPointerNull::get(i8ptr_ty());
                    llvm::Value* ui_method = builder_->CreateGlobalStringPtr("GET", "ui_method");
                    builder_->CreateCall(route_register_, { ui_method, path_str, handler });
                }
                if (node->body) gen_block(node->body.get());
            }
            break;
        }
        case NodeType::Layout: {
            /* Layout: create a container component for children */
            if (comp_create_) {
                llvm::Value* name_str = builder_->CreateGlobalStringPtr(
                    node->value.empty() ? "layout" : node->value, "layout_name");
                auto* z32 = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_), 0);
                llvm::Value* comp = builder_->CreateCall(comp_create_,
                    { name_str, z32, z32,
                      llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_), 800),
                      llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_), 600) },
                    "layout_comp");

                /* Store as parent for children */
                auto* comp_slot = create_entry_alloca("__parent_comp", i8ptr_ty());
                builder_->CreateStore(comp, comp_slot);
                declare_var("__parent_comp", comp_slot, OwnershipState::Owned);

                if (node->body) gen_block(node->body.get());

                /* Add layout to its parent if inside a component */
                VarRecord* parent_rec = lookup_var("__parent_comp");
                if (parent_rec && parent_rec->alloca_ptr && comp_add_child_) {
                    llvm::Value* parent_comp = builder_->CreateLoad(i8ptr_ty(), parent_rec->alloca_ptr, "layout_parent");
                    llvm::Value* is_parent = builder_->CreateICmpNE(parent_comp, comp);
                    auto* add_bb = llvm::BasicBlock::Create(ctx_, "layout_add", cur_fn_);
                    auto* skip_bb = llvm::BasicBlock::Create(ctx_, "layout_skip", cur_fn_);
                    builder_->CreateCondBr(is_parent, add_bb, skip_bb);
                    builder_->SetInsertPoint(add_bb);
                    builder_->CreateCall(comp_add_child_, { parent_comp, comp });
                    builder_->CreateBr(skip_bb);
                    builder_->SetInsertPoint(skip_bb);
                }
            } else {
                if (node->body) gen_block(node->body.get());
                auto* fn_layout = module_->getFunction("aurora_gui_layout_vertical");
                if (fn_layout && node->left) {
                    llvm::Value* children = gen_expr(node->left.get());
                    builder_->CreateCall(fn_layout, { children });
                }
            }
            break;
        }
        case NodeType::Animate:       gen_animate(node);       break;
        case NodeType::Transition:    gen_transition(node);    break;
        case NodeType::Server:        gen_server(node);        break;
        case NodeType::Request: {
            /* Request: retrieve stored request from route handler context */
            VarRecord* http_req_rec = lookup_var("__http_req");
            if (http_req_rec && http_req_rec->alloca_ptr) {
                builder_->CreateLoad(i8ptr_ty(), http_req_rec->alloca_ptr, "http_req");
            } else if (http_parse_req_) {
                llvm::Value* raw = llvm::ConstantPointerNull::get(i8ptr_ty());
                if (node->left) {
                    raw = gen_expr(node->left.get());
                    if (raw->getType() != i8ptr_ty())
                        raw = builder_->CreateBitCast(raw, i8ptr_ty());
                }
                builder_->CreateCall(http_parse_req_, { raw }, "req");
            }
            break;
        }
        case NodeType::Response: {
            /* Response: use stored response from route handler if available */
            VarRecord* http_res_rec = lookup_var("__http_res");
            llvm::Value* resp = nullptr;
            if (http_res_rec && http_res_rec->alloca_ptr) {
                resp = builder_->CreateLoad(i8ptr_ty(), http_res_rec->alloca_ptr, "http_res");
            } else if (http_resp_new_) {
                resp = builder_->CreateCall(http_resp_new_, {}, "resp");
            }
            if (!resp) break;

            const std::string& method = node->value;
            llvm::Function* str_to_c = module_->getFunction("aurora_str_as_cstr");

            if (method.empty()) {
                /* response(body) — set body with existing content-type */
                if (node->left && http_resp_body_) {
                    llvm::Value* body = gen_expr(node->left.get());
                    if (body->getType() != i8ptr_ty())
                        body = builder_->CreateBitCast(body, i8ptr_ty());
                    if (str_to_c) body = builder_->CreateCall(str_to_c, { body }, "cstr");
                    builder_->CreateCall(http_resp_body_, { resp, body });
                }
            } else if (method == "json") {
                /* response.json(body) */
                if (node->args && http_set_json_) {
                    llvm::Value* body = gen_expr(node->args.get());
                    if (body->getType() != i8ptr_ty())
                        body = builder_->CreateBitCast(body, i8ptr_ty());
                    if (str_to_c) body = builder_->CreateCall(str_to_c, { body }, "cstr");
                    builder_->CreateCall(http_set_json_, { resp, body });
                }
            } else if (method == "html") {
                /* response.html(body) */
                if (node->args && http_resp_ct_ && http_resp_body_) {
                    llvm::Value* ct = builder_->CreateGlobalStringPtr("text/html", "html_ct");
                    builder_->CreateCall(http_resp_ct_, { resp, ct });
                    llvm::Value* body = gen_expr(node->args.get());
                    if (body->getType() != i8ptr_ty())
                        body = builder_->CreateBitCast(body, i8ptr_ty());
                    if (str_to_c) body = builder_->CreateCall(str_to_c, { body }, "cstr");
                    builder_->CreateCall(http_resp_body_, { resp, body });
                }
            } else if (method == "status") {
                /* response.status(code) */
                if (node->args) {
                    llvm::Value* code = gen_expr(node->args.get());
                    auto* fn = module_->getFunction("aurora_http_response_set_status_code");
                    if (fn) builder_->CreateCall(fn, { resp, code });
                }
            } else if (method == "redirect") {
                /* response.redirect(url, code) */
                if (node->args) {
                    llvm::Value* url = gen_expr(node->args.get());
                    if (url->getType() != i8ptr_ty())
                        url = builder_->CreateBitCast(url, i8ptr_ty());
                    if (str_to_c) url = builder_->CreateCall(str_to_c, { url }, "cstr");
                    llvm::Value* code_val = llvm::ConstantInt::get(i64_ty(), 302);
                    if (node->args->next)
                        code_val = gen_expr(node->args->next.get());
                    auto* fn = module_->getFunction("aurora_http_response_redirect");
                    if (fn) builder_->CreateCall(fn, { resp, url, code_val });
                }
            } else if (method == "cookie") {
                /* response.cookie(name, value, ttl) */
                if (node->args && node->args->next) {
                    llvm::Value* name = gen_expr(node->args.get());
                    if (name->getType() != i8ptr_ty())
                        name = builder_->CreateBitCast(name, i8ptr_ty());
                    if (str_to_c) name = builder_->CreateCall(str_to_c, { name }, "cstr");
                    llvm::Value* value = gen_expr(node->args->next.get());
                    if (value->getType() != i8ptr_ty())
                        value = builder_->CreateBitCast(value, i8ptr_ty());
                    if (str_to_c) value = builder_->CreateCall(str_to_c, { value }, "cstr");
                    auto* fn = module_->getFunction("aurora_http_response_set_header");
                    if (fn) {
                        /* Build Set-Cookie header value: name=value */
                        std::vector<llvm::Value*> indices = {
                            llvm::ConstantInt::get(i64_ty(), 0),
                            llvm::ConstantInt::get(i64_ty(), 0)
                        };
                        llvm::Value* eq = builder_->CreateGlobalStringPtr("=", "eq_str");
                        llvm::Function* strcat_fn = module_->getFunction("aurora_str_concat");
                        if (strcat_fn) {
                            llvm::Value* cookie_val = builder_->CreateCall(strcat_fn, { name, eq }, "cookie_prefix");
                            cookie_val = builder_->CreateCall(strcat_fn, { cookie_val, value }, "cookie_full");
                            builder_->CreateCall(fn, { resp,
                                builder_->CreateGlobalStringPtr("Set-Cookie", "setcookie_name"),
                                cookie_val });
                        }
                    }
                }
            }
            break;
        }
        case NodeType::Api:           gen_api(node);           break;
        case NodeType::Middleware: {
            /* Middleware: generate body block (handler creation done in gen_server / standalone) */
            if (node->body) gen_block(node->body.get());
            break;
        }
        case NodeType::Database:      gen_database(node);      break;
        case NodeType::Query: {
            /* Query: execute database query */
            llvm::Function* fn_db_q = module_->getFunction("aurora_db_query");
            if (fn_db_q && node->left) {
                llvm::Value* query_str = gen_expr(node->left.get());
                llvm::Value* db = llvm::ConstantPointerNull::get(i8ptr_ty());
                if (node->right) db = gen_expr(node->right.get());
                builder_->CreateCall(fn_db_q, { db, query_str }, "query_result");
            }
            break;
        }
        case NodeType::Model: {
            gen_model(node);
            break;
        }
        case NodeType::Cache:         gen_cache(node);         break;
        case NodeType::Session:       gen_session(node);       break;
        case NodeType::Token: {
            /* Token: generate auth token */
            llvm::Function* fn_auth = module_->getFunction("aurora_auth_login");
            if (fn_auth && node->left) {
                llvm::Value* user = gen_expr(node->left.get());
                builder_->CreateCall(fn_auth, { user, llvm::ConstantPointerNull::get(i8ptr_ty()) }, "token");
            }
            break;
        }
        case NodeType::Auth:          gen_auth(node);          break;
        case NodeType::Cors:          gen_block(node->body.get()); break;
        case NodeType::WebSocket:     gen_block(node->body.get()); break;
        case NodeType::Sse:           gen_block(node->body.get()); break;
        case NodeType::Tpl: {
            /* template name "source" — compile + render */
            auto* fn_compile = module_->getFunction("aurora_template_compile");
            auto* fn_render = module_->getFunction("aurora_template_render");
            if (fn_compile && !node->value.empty()) {
                llvm::Value* name = builder_->CreateGlobalStringPtr(node->value, "tpl_name");
                llvm::Value* source = nullptr;
                if (node->left) source = gen_expr(node->left.get());
                else source = llvm::ConstantPointerNull::get(i8ptr_ty());
                llvm::Value* tpl = builder_->CreateCall(fn_compile, { name, source }, "tpl");
                if (fn_render && tpl) {
                    llvm::Value* ctx = llvm::ConstantPointerNull::get(i8ptr_ty());
                    if (node->body) {
                        /* body may set up context as JSON string */
                        ctx = gen_expr(node->body.get());
                    }
                    if (!ctx) ctx = llvm::ConstantPointerNull::get(i8ptr_ty());
                    builder_->CreateCall(fn_render, { name, ctx });
                }
            }
            break;
        }
        case NodeType::Validate:      gen_block(node->body.get()); break;
        case NodeType::Ai:            gen_ai(node);            break;
        case NodeType::Train: {
            /* Train: execute training loop */
            if (node->body) gen_block(node->body.get());
            if (predict_) {
                llvm::Value* model = llvm::ConstantPointerNull::get(i8ptr_ty());
                llvm::Value* input = llvm::ConstantPointerNull::get(i8ptr_ty());
                if (node->left) model = gen_expr(node->left.get());
                if (node->right) input = gen_expr(node->right.get());
                builder_->CreateCall(predict_, { model, input }, "train_predict");
            }
            break;
        }
        case NodeType::Predict:       gen_ai_predict_stmt(node); break;
        case NodeType::Tensor:        gen_tensor_stmt(node);   break;
        case NodeType::Neural: {
            /* Neural: execute neural network layer */
            if (node->body) gen_block(node->body.get());
            if (neural_forward_) {
                llvm::Value* input = llvm::ConstantPointerNull::get(i8ptr_ty());
                llvm::Value* weights = llvm::ConstantPointerNull::get(i8ptr_ty());
                llvm::Value* bias = llvm::ConstantPointerNull::get(i8ptr_ty());
                if (node->left) {
                    input = gen_expr(node->left.get());
                    if (node->left->next) weights = gen_expr(node->left->next.get());
                    if (node->left->next && node->left->next->next)
                        bias = gen_expr(node->left->next->next.get());
                }
                builder_->CreateCall(neural_forward_, { input, weights, bias }, "neural_out");
            }
            break;
        }
        case NodeType::Sleep:         gen_sleep(node);         break;
        case NodeType::Time:          gen_time(node);          break;
        case NodeType::Random:        gen_random(node);        break;
        case NodeType::Scene:         gen_scene(node);         break;
        case NodeType::Entity:        gen_entity(node);        break;
        case NodeType::Object: {
            /* Object: generic game object */
            if (node->body) gen_block(node->body.get());
            break;
        }
        case NodeType::Sprite:        gen_sprite(node);        break;
        case NodeType::Camera:        gen_camera(node);        break;
        case NodeType::Physics:       gen_physics(node);       break;
        case NodeType::Collision:     gen_collision(node);     break;
        case NodeType::Audio:         gen_audio(node);         break;
        case NodeType::Animation: {
            /* Animation: play animation on target */
            if (node->body) gen_block(node->body.get());
            if (animation_play_ && node->left) {
                llvm::Value* target = gen_expr(node->left.get());
                llvm::Value* anim_data = llvm::ConstantPointerNull::get(i8ptr_ty());
                if (node->left->next) anim_data = gen_expr(node->left->next.get());
                builder_->CreateCall(animation_play_, { target, anim_data });
            }
            break;
        }
        case NodeType::Input: {
            /* Input: check key state, run body if key pressed */
            if (engine_is_key_down_) {
                llvm::Value* key = i64(0);
                if (node->left) key = gen_expr(node->left.get());
                llvm::Value* pressed = builder_->CreateCall(engine_is_key_down_, { key }, "keydown");
                if (node->body) {
                    llvm::Function* parent = builder_->GetInsertBlock()->getParent();
                    llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(ctx_, "input_body", parent);
                    llvm::BasicBlock* done_bb = llvm::BasicBlock::Create(ctx_, "input_done", parent);
                    llvm::Value* cond = builder_->CreateICmpNE(pressed, i64(0), "key_cond");
                    builder_->CreateCondBr(cond, body_bb, done_bb);
                    builder_->SetInsertPoint(body_bb);
                    gen_block(node->body.get());
                    if (!current_block_terminated())
                        builder_->CreateBr(done_bb);
                    builder_->SetInsertPoint(done_bb);
                }
            } else {
                if (node->body) gen_block(node->body.get());
            }
            break;
        }
        case NodeType::Update: {
            /* Update: poll input, frame start, body, frame end, render */
            if (engine_poll_input_)
                builder_->CreateCall(engine_poll_input_, {});
            if (engine_frame_start_)
                builder_->CreateCall(engine_frame_start_, {});
            if (node->body) gen_block(node->body.get());
            if (engine_frame_end_)
                builder_->CreateCall(engine_frame_end_, {});
            if (engine_render_)
                builder_->CreateCall(engine_render_, {});
            break;
        }
        case NodeType::Tick: {
            /* Tick: call delta_time and run body */
            if (engine_delta_time_)
                builder_->CreateCall(engine_delta_time_, {}, "dt");
            if (node->body) gen_block(node->body.get());
            break;
        }
        case NodeType::Reference:     gen_reference(node);    break;
        case NodeType::Pointer:       gen_pointer(node);      break;
        case NodeType::ModuleDecl:    /* no-op */             break;
        case NodeType::PackageDecl:   /* no-op */             break;
        case NodeType::AliasDecl:     /* no-op */             break;
        case NodeType::Class:         gen_class_oop(node);    break;
        case NodeType::New:           gen_new(node);          break;
        case NodeType::Break:         gen_break();            break;
        case NodeType::Continue:      gen_continue();         break;
        case NodeType::Skip:          gen_skip();             break;
        case NodeType::Pass:          /* no-op */             break;
        case NodeType::Import:        /* no-op */             break;
        case NodeType::Spawn:         gen_spawn(node);        break;
        case NodeType::Wait:          gen_wait(node);         break;
        case NodeType::Async:         gen_async(node);        break;
        case NodeType::Panic:         gen_panic(node);        break;
        case NodeType::Debug:         gen_debug(node);        break;
        case NodeType::Log:           gen_log(node);          break;
        case NodeType::Yield:         gen_yield(node);        break;
        case NodeType::ExternFn:      gen_extern_fn(node);    break;
        case NodeType::ExternStruct:  gen_extern_struct(node); break;
        case NodeType::ExternUnion:   gen_extern_union(node);  break;
        default:                      gen_expr(node);         break;
    }
}

/* ════════════════════════════════════════════════════════════
   Closure-returning function detection  (pre-scan)
   ════════════════════════════════════════════════════════════ */

void Codegen::scan_closure_returning_fns(const ASTNode* root) {
    closure_returning_fns_.clear();
    const ASTNode* n = root;
    while (n) {
        if (n->type == NodeType::Function || n->type == NodeType::PerformanceFn) {
            if (fn_body_returns_closure(n->body.get()))
                closure_returning_fns_.insert(n->value);
        }
        n = n->next.get();
    }
}

bool Codegen::fn_body_returns_closure(const ASTNode* body) {
    std::unordered_set<std::string> closure_vars;
    const ASTNode* stmt = body;
    while (stmt) {
        if (stmt->type == NodeType::Lambda && !stmt->captures.empty()) {
            if (!stmt->value.empty())
                closure_vars.insert(stmt->value);
        }
        if (stmt->type == NodeType::Assign && stmt->left) {
            std::string lhs = stmt->left->value;
            if (stmt->right && stmt->right->type == NodeType::Lambda && !stmt->right->captures.empty())
                closure_vars.insert(lhs);
        }
        if (stmt->type == NodeType::Return && stmt->left) {
            if (stmt->left->type == NodeType::Lambda && !stmt->left->captures.empty())
                return true;
            if (stmt->left->type == NodeType::Var && closure_vars.count(stmt->left->value))
                return true;
        }
        stmt = stmt->next.get();
    }
    return false;
}