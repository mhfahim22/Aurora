#include "compiler/codegen.hpp"
#include "compiler/class_oop.hpp"

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/Support/raw_ostream.h>

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

/* ════════════════════════════════════════════════════════════
   Main entry point
   ════════════════════════════════════════════════════════════ */
void Codegen::generate(const ASTNode* root) {
    /* Step 0 — clear OOP caches for fresh compilation */
    oop_clear();
    oop_clear_object_types();
    oop_clear_vtable_cache();

    /* Step 0 — set target info for LLVM optimization */
    module_->setTargetTriple("x86_64-pc-windows-msvc");
    module_->setDataLayout("e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128");

    /* Step 1 — run type checking before LLVM IR is emitted. */
    TypeChecker type_checker;
    type_checker.analyse(root);

    /* Step 2 — run ownership analysis (throws OwnershipError on violations) */
    ownership_.analyse(root);

    /* Step 3 — declare runtime helpers */
    declare_runtime_helpers();

    /* Step 4 — check if the user defines "function main():" in the AST.
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

    /* Step 5 — create a function for top-level code.  If the user also
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

    /* Step 6 — generate IR for the AST */
    push_scope();
    gen_block(root);
    pop_scope_and_drop();

    /* Return 0 from entry function */
    if (!current_block_terminated())
        safe_ret(llvm::ConstantInt::get(i32_ty, 0));

    /* Step 7 — if the user defined "function main():", rename the user's
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
}

/* ════════════════════════════════════════════════════════════
   Strategy-based specialized codegen dispatch
   ════════════════════════════════════════════════════════════ */

llvm::Value* Codegen::gen_allocation_for_var(const std::string& name,
                                              llvm::Type* ty,
                                              OwnershipState state) {
    llvm::Value* ptr = create_entry_alloca(name, ty);
    llvm::PointerType* ptr_ty = llvm::PointerType::getUnqual(ctx_);

    /* Emit ownership-specific setup based on state */
    switch (state) {
        case OwnershipState::Shared: {
            /* Shared: allocate SharedBox wrapper */
            llvm::Value* shared_box = builder_->CreateCall(
                module_->getOrInsertFunction("aurora_shared_new",
                    llvm::FunctionType::get(i8ptr_ty(),
                        {i8ptr_ty(), i8ptr_ty()}, false)),
                {ptr, llvm::ConstantPointerNull::get(ptr_ty)});
            builder_->CreateStore(shared_box, ptr);
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
   Runtime helper declarations
   ════════════════════════════════════════════════════════════

   These functions are implemented in aurora_runtime.c and linked
   at the final link step.  We just declare their signatures here
   so the IR can call them.
   ════════════════════════════════════════════════════════════ */
void Codegen::declare_runtime_helpers() {
    /* void drop_glue(i8* ptr) */
    fn_drop_glue_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i8ptr_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_drop_glue", module_.get());

    /* void refcount_inc(i8* ptr) */
    fn_refcount_inc_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i8ptr_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_refcount_inc", module_.get());

    /* void refcount_dec(i8* ptr) */
    fn_refcount_dec_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i8ptr_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_refcount_dec", module_.get());

    /* i8* weak_lock(i8* ptr) — returns null if object is dead */
    fn_weak_lock_ = llvm::Function::Create(
        llvm::FunctionType::get(i8ptr_ty(), { i8ptr_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_weak_lock", module_.get());

    /* void weak_release(i8* ptr) */
    fn_weak_release_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i8ptr_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_weak_release", module_.get());

    /* void aurora_gc_register_root(i8* ptr) */
    fn_gc_register_root_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i8ptr_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_gc_register_root", module_.get());

    /* void aurora_gc_unregister_root(i8* ptr) */
    fn_gc_unregister_root_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i8ptr_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_gc_unregister_root", module_.get());

    /* void aurora_print_int(i64 val) */
    fn_printf_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_print_int", module_.get());

    /* void aurora_print_str(i8* ptr) — prints AuroraStr* */
    fn_print_str_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i8ptr_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_print_str", module_.get());

    /* i8* aurora_str_from_cstr(i8*) — wraps C string in AuroraStr */
    fn_str_from_cstr_ = llvm::Function::Create(
        llvm::FunctionType::get(i8ptr_ty(), { i8ptr_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_str_from_cstr", module_.get());

    /* i8* aurora_str_new(i64) — allocate new AuroraStr with given capacity */
    fn_str_new_ = llvm::Function::Create(
        llvm::FunctionType::get(i8ptr_ty(), { i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_str_new", module_.get());

    /* void aurora_str_free(i8*) — free an AuroraStr */
    fn_str_free_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i8ptr_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_str_free", module_.get());

    /* void aurora_print_float(double val) */
    fn_print_float_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { llvm::Type::getDoubleTy(ctx_) }, false),
        llvm::Function::ExternalLinkage, "aurora_print_float", module_.get());

    /* i8* aurora_str_append(i8*, i8*) — append b to a, reuse a's buffer */
    fn_str_append_ = llvm::Function::Create(
        llvm::FunctionType::get(i8ptr_ty(), { i8ptr_ty(), i8ptr_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_str_append", module_.get());

    /* void aurora_str_reserve(AuroraStr*, i64) — ensure capacity */
    fn_str_reserve_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i8ptr_ty(), i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_str_reserve", module_.get());

    /* AuroraStr* aurora_str_concat(AuroraStr* a, AuroraStr* b) */
    fn_str_concat_ = llvm::Function::Create(
        llvm::FunctionType::get(i8ptr_ty(), { i8ptr_ty(), i8ptr_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_str_concat", module_.get());

    /* AuroraStr* aurora_int_to_str(i64) — convert integer to AuroraStr */
    fn_int_to_str_ = llvm::Function::Create(
        llvm::FunctionType::get(i8ptr_ty(), { i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_int_to_str", module_.get());

    /* AuroraStr* aurora_float_to_str(double) — convert float to AuroraStr */
    llvm::Function::Create(
        llvm::FunctionType::get(i8ptr_ty(), { llvm::Type::getDoubleTy(ctx_) }, false),
        llvm::Function::ExternalLinkage, "aurora_float_to_str", module_.get());

    /* ── Array runtime ── */
    auto* dbl = llvm::Type::getDoubleTy(ctx_);
    auto* ptr = i8ptr_ty();

    /* i64 aurora_array_new(i64 cap) */
    fn_array_new_ = llvm::Function::Create(
        llvm::FunctionType::get(i64_ty(), { i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_array_new", module_.get());

    /* void aurora_array_reserve(i64 arr, i64 cap) */
    fn_array_reserve_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i64_ty(), i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_array_reserve", module_.get());

    /* void aurora_array_push_int(i64 arr, i64 val) */
    fn_array_push_int_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i64_ty(), i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_array_push_int", module_.get());

    /* void aurora_array_push_float(i64 arr, double val) */
    fn_array_push_flt_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i64_ty(), dbl }, false),
        llvm::Function::ExternalLinkage, "aurora_array_push_float", module_.get());

    /* void aurora_array_push_str(i64 arr, i8* str) */
    fn_array_push_str_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i64_ty(), ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_array_push_str", module_.get());

    /* void aurora_array_push_array(i64 arr, i64 nested) */
    fn_array_push_arr_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i64_ty(), i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_array_push_array", module_.get());

    /* i64 aurora_array_get_int(i64 arr, i64 idx) */
    fn_array_get_int_ = llvm::Function::Create(
        llvm::FunctionType::get(i64_ty(), { i64_ty(), i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_array_get_int", module_.get());

    /* double aurora_array_get_float(i64 arr, i64 idx) */
    fn_array_get_flt_ = llvm::Function::Create(
        llvm::FunctionType::get(dbl, { i64_ty(), i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_array_get_float", module_.get());

    /* i8* aurora_array_get_str(i64 arr, i64 idx) */
    fn_array_get_str_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { i64_ty(), i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_array_get_str", module_.get());

    /* i64 aurora_array_get_tag(i64 arr, i64 idx) */
    fn_array_get_tag_ = llvm::Function::Create(
        llvm::FunctionType::get(i64_ty(), { i64_ty(), i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_array_get_tag", module_.get());

    /* void aurora_array_set_int(i64 arr, i64 idx, i64 val) */
    fn_array_set_int_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i64_ty(), i64_ty(), i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_array_set_int", module_.get());

    /* void aurora_array_set_float(i64 arr, i64 idx, double val) */
    fn_array_set_flt_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i64_ty(), i64_ty(), dbl }, false),
        llvm::Function::ExternalLinkage, "aurora_array_set_float", module_.get());

    /* void aurora_array_set_str(i64 arr, i64 idx, i8* val) */
    fn_array_set_str_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i64_ty(), i64_ty(), ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_array_set_str", module_.get());

    /* i64 aurora_array_len(i64 arr) */
    fn_array_len_ = llvm::Function::Create(
        llvm::FunctionType::get(i64_ty(), { i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_array_len", module_.get());

    /* void aurora_array_print(i64 arr) */
    fn_array_print_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_array_print", module_.get());

    /* void aurora_array_free(i64 arr) */
    fn_array_free_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_array_free", module_.get());

    /* i64 aurora_array_contains_int(i64 arr, i64 val) */
    fn_array_contains_int_ = llvm::Function::Create(
        llvm::FunctionType::get(i64_ty(), { i64_ty(), i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_array_contains_int", module_.get());

    /* ── Async runtime ── */
    auto* fnptr_ty = llvm::PointerType::getUnqual(ctx_);

    /* AuroraTask* aurora_task_create(i8* (*func)(i8*), i8* arg) */
    fn_task_create_ = llvm::Function::Create(
        llvm::FunctionType::get(fnptr_ty, { fnptr_ty, fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_task_create", module_.get());

    /* void aurora_task_destroy(AuroraTask*) */
    fn_task_destroy_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_task_destroy", module_.get());

    /* i32 aurora_task_is_done(AuroraTask*) */
    fn_task_is_done_ = llvm::Function::Create(
        llvm::FunctionType::get(i64_ty(), { fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_task_is_done", module_.get());

    /* i8* aurora_task_get_result(AuroraTask*) */
    fn_task_get_result_ = llvm::Function::Create(
        llvm::FunctionType::get(fnptr_ty, { fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_task_get_result", module_.get());

    /* void aurora_task_set_result(AuroraTask*, i8* result) */
    fn_task_set_result_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { fnptr_ty, fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_task_set_result", module_.get());

    /* void aurora_spawn(AuroraTask*) */
    fn_spawn_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_spawn", module_.get());

    /* void aurora_wait(AuroraTask*) */
    fn_wait_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_wait", module_.get());

    /* ── Phase 2: Collection type constructors ── */
    auto* i32_ty = llvm::Type::getInt32Ty(ctx_);
    /* i8* list_new() */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "list_new", module_.get());
    /* void list_push(i8*, i64) */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { ptr, i64_ty() }, false),
        llvm::Function::ExternalLinkage, "list_push", module_.get());
    /* i64 list_get(i8*, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(i64_ty(), { ptr, i32_ty }, false),
        llvm::Function::ExternalLinkage, "list_get", module_.get());
    /* i32 list_len(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32_ty, { ptr }, false),
        llvm::Function::ExternalLinkage, "list_len", module_.get());
    /* void list_free(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { ptr }, false),
        llvm::Function::ExternalLinkage, "list_free", module_.get());
    /* i8* map_new() */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "map_new", module_.get());
    /* void map_set(i8*, i8*, i64) */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { ptr, ptr, i64_ty() }, false),
        llvm::Function::ExternalLinkage, "map_set", module_.get());
    /* i64 map_get(i8*, i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(i64_ty(), { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "map_get", module_.get());
    /* i32 map_has(i8*, i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32_ty, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "map_has", module_.get());
    /* void map_free(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { ptr }, false),
        llvm::Function::ExternalLinkage, "map_free", module_.get());
    /* i8* set_new() */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "set_new", module_.get());
    /* void set_add(i8*, i64) */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { ptr, i64_ty() }, false),
        llvm::Function::ExternalLinkage, "set_add", module_.get());
    /* i32 set_has(i8*, i64) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32_ty, { ptr, i64_ty() }, false),
        llvm::Function::ExternalLinkage, "set_has", module_.get());
    /* void set_free(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { ptr }, false),
        llvm::Function::ExternalLinkage, "set_free", module_.get());
    /* i8* vector_new(double, double, double) */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { dbl, dbl, dbl }, false),
        llvm::Function::ExternalLinkage, "vector_new", module_.get());
    /* double vector_x(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(dbl, { ptr }, false),
        llvm::Function::ExternalLinkage, "vector_x", module_.get());
    /* double vector_y(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(dbl, { ptr }, false),
        llvm::Function::ExternalLinkage, "vector_y", module_.get());
    /* double vector_z(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(dbl, { ptr }, false),
        llvm::Function::ExternalLinkage, "vector_z", module_.get());
    /* i8* stack_new() */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "stack_new", module_.get());
    /* void stack_push(i8*, i64) */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { ptr, i64_ty() }, false),
        llvm::Function::ExternalLinkage, "stack_push", module_.get());
    /* i64 stack_pop(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(i64_ty(), { ptr }, false),
        llvm::Function::ExternalLinkage, "stack_pop", module_.get());
    /* i32 stack_empty(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32_ty, { ptr }, false),
        llvm::Function::ExternalLinkage, "stack_empty", module_.get());
    /* void stack_free(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { ptr }, false),
        llvm::Function::ExternalLinkage, "stack_free", module_.get());
    /* i8* queue_new() */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "queue_new", module_.get());
    /* void queue_enqueue(i8*, i64) */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { ptr, i64_ty() }, false),
        llvm::Function::ExternalLinkage, "queue_enqueue", module_.get());
    /* i64 queue_dequeue(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(i64_ty(), { ptr }, false),
        llvm::Function::ExternalLinkage, "queue_dequeue", module_.get());
    /* i32 queue_empty(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32_ty, { ptr }, false),
        llvm::Function::ExternalLinkage, "queue_empty", module_.get());
    /* void queue_free(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { ptr }, false),
        llvm::Function::ExternalLinkage, "queue_free", module_.get());
    /* i8* json_new() */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "json_new", module_.get());
    /* void json_set(i8*, i8*, i64) */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { ptr, ptr, i64_ty() }, false),
        llvm::Function::ExternalLinkage, "json_set", module_.get());
    /* void json_free(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { ptr }, false),
        llvm::Function::ExternalLinkage, "json_free", module_.get());
    /* void aurora_panic(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_panic", module_.get());

    /* ── AI/Tensor runtime helpers ── */
    declare_ai_runtime_helpers();

    /* ── Domain runtime helper declarations ── */
    declare_domain_runtime_helpers();

    /* ── Built-in functions (registered via codegen_builtins.cpp) ── */
    register_builtins(module_.get(), ctx_, builtins_);
}

/* ════════════════════════════════════════════════════════════
   Domain-specific runtime helper declarations
   ════════════════════════════════════════════════════════════ */
void Codegen::declare_domain_runtime_helpers() {
    auto* ctx = &ctx_;
    auto* mod = module_.get();
    auto* v   = llvm::Type::getVoidTy(*ctx);
    auto* i64 = llvm::Type::getInt64Ty(*ctx);
    auto* ptr = llvm::PointerType::getUnqual(*ctx);

    /* ── Game Engine ── */
    /* scene_init() / scene_shutdown() */
    scene_init_ = llvm::Function::Create(
        llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_scene_init", mod);
    scene_shutdown_ = llvm::Function::Create(
        llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_scene_shutdown", mod);

    /* entity_create(i64 type) → i8* */
    entity_create_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { i64 }, false),
        llvm::Function::ExternalLinkage, "aurora_entity_create", mod);
    /* entity_destroy(i8*) */
    entity_destroy_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_entity_destroy", mod);
    /* entity_set_pos(i8*, double, double, double) */
    entity_set_pos_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, llvm::Type::getDoubleTy(*ctx), llvm::Type::getDoubleTy(*ctx), llvm::Type::getDoubleTy(*ctx) }, false),
        llvm::Function::ExternalLinkage, "aurora_entity_set_pos", mod);

    /* sprite_create(i8*, double, double) → i8* */
    sprite_create_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, llvm::Type::getDoubleTy(*ctx), llvm::Type::getDoubleTy(*ctx) }, false),
        llvm::Function::ExternalLinkage, "aurora_sprite_create", mod);

    /* camera_create(double, double, double) → i8* */
    camera_create_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { llvm::Type::getDoubleTy(*ctx), llvm::Type::getDoubleTy(*ctx), llvm::Type::getDoubleTy(*ctx) }, false),
        llvm::Function::ExternalLinkage, "aurora_camera_create", mod);

    /* physics_init() / physics_step(double) */
    physics_init_ = llvm::Function::Create(
        llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_physics_init", mod);
    physics_step_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { llvm::Type::getDoubleTy(*ctx) }, false),
        llvm::Function::ExternalLinkage, "aurora_physics_step", mod);

    /* collision_check(i8*, i8*) → i32 */
    collision_check_ = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_collision_check", mod);

    /* audio_play(i8*) */
    audio_play_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_play", mod);

    /* animation_play(i8*, i8*, i64 duration) */
    animation_play_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr, i64 }, false),
        llvm::Function::ExternalLinkage, "aurora_animation_play", mod);

    /* engine_frame_start() / engine_frame_end() */
    engine_frame_start_ = llvm::Function::Create(
        llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_engine_frame_start", mod);
    engine_frame_end_ = llvm::Function::Create(
        llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_engine_frame_end", mod);
    /* engine_render() */
    engine_render_ = llvm::Function::Create(
        llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_engine_render", mod);
    /* engine_is_key_down(i32) → i32 */
    engine_is_key_down_ = llvm::Function::Create(
        llvm::FunctionType::get(i64, { i64 }, false),
        llvm::Function::ExternalLinkage, "aurora_engine_is_key_down", mod);
    /* engine_delta_time() → double */
    engine_delta_time_ = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getDoubleTy(*ctx), {}, false),
        llvm::Function::ExternalLinkage, "aurora_engine_delta_time", mod);

    /* ── Backend Framework ── */
    /* server_init(i64 port) → i8* */
    server_init_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { i64 }, false),
        llvm::Function::ExternalLinkage, "aurora_server_init", mod);
    server_start_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_server_start", mod);
    server_stop_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_server_stop", mod);
    /* server_accept_and_handle(i8*, i8*) */
    server_accept_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_server_accept_and_handle", mod);

    /* database_connect(i8*) → i8* */
    db_connect_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_connect", mod);
    db_query_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_query", mod);
    db_close_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_close", mod);

    /* cache_init() → i8* */
    cache_init_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_cache_init", mod);
    cache_set_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_cache_set", mod);
    cache_get_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_cache_get", mod);

    /* session_create() → i8* */
    session_create_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_session_create", mod);
    session_destroy_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_session_destroy", mod);

    /* auth_login(i8*, i8*) → i32 */
    auth_login_ = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_auth_login", mod);

    /* http_parse_request(i8*) → i8* */
    http_parse_req_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_http_parse_request", mod);
    /* http_response_new() → i8* */
    http_resp_new_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_http_response_new", mod);
    /* http_response_set_status(i8*, i32, i8*) */
    http_resp_status_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, i64, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_http_response_set_status", mod);
    /* http_response_set_body(i8*, i8*) */
    http_resp_body_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_http_response_set_body", mod);
    /* http_response_send(i8*, i64) → i32 */
    http_resp_send_ = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, i64 }, false),
        llvm::Function::ExternalLinkage, "aurora_http_response_send", mod);
    /* router_new() → i8* */
    router_new_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_router_new", mod);
    /* route_add(i8*, i8*, i8*, i8*) */
    route_add_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr, ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_route_add", mod);
    /* route_dispatch(i8*, i8*, i8*) → i32 */
    route_dispatch_ = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_route_dispatch", mod);

    /* ── UI Framework ── */
    /* component_init() / component_render() */
    ui_init_ = llvm::Function::Create(
        llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_ui_init", mod);
    ui_shutdown_ = llvm::Function::Create(
        llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_ui_shutdown", mod);
    ui_render_ = llvm::Function::Create(
        llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_ui_render", mod);

    /* route_register(i8*, i8*) */
    route_register_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_route_register", mod);

    /* style_apply(i8*, i8*) */
    style_apply_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_style_apply", mod);

    /* ── Utility ── */
    /* aurora_sleep(i64 ms) */
    fn_sleep_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { i64 }, false),
        llvm::Function::ExternalLinkage, "aurora_sleep", mod);

    /* aurora_time() → i64 */
    fn_time_ = llvm::Function::Create(
        llvm::FunctionType::get(i64, {}, false),
        llvm::Function::ExternalLinkage, "aurora_time", mod);

    /* aurora_random() → i64 */
    fn_random_ = llvm::Function::Create(
        llvm::FunctionType::get(i64, {}, false),
        llvm::Function::ExternalLinkage, "aurora_random", mod);
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

        auto* alloca_inst = llvm::dyn_cast<llvm::AllocaInst>(rec.alloca_ptr);
        llvm::Type* slot_ty = alloca_inst ? alloca_inst->getAllocatedType() : nullptr;
        llvm::Value* value = nullptr;

        if (slot_ty)
            value = builder_->CreateLoad(slot_ty, rec.alloca_ptr, kv.first + "_cleanup");

        if (rec.is_array && value && value->getType()->isIntegerTy(64)) {
            builder_->CreateCall(fn_array_free_, { value });
            continue;
        }
        if (rec.is_string) continue;

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
    for (int i = (int)scopes_.size() - 1; i >= 0; --i) {
        emit_scope_cleanup(scopes_[i]);
        if (current_block_terminated()) break;
    }
}

void Codegen::declare_var(const std::string& name,
                          llvm::Value*       alloca_ptr,
                          OwnershipState     state) {
    if (scopes_.empty()) push_scope();
    scopes_.back().vars[name] = VarRecord{ alloca_ptr, state };
}

VarRecord* Codegen::lookup_var(const std::string& name) {
    for (int i = (int)scopes_.size() - 1; i >= 0; i--) {
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
    return tmp_builder.CreateAlloca(ty, nullptr, name);
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
        case NodeType::Inline:        /* no-op */             break;
        case NodeType::NoInline:      /* no-op */             break;
        case NodeType::ConstExpr:     gen_expr(node->left.get()); break;
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
            /* Render: emit UI render tree with child components */
            auto* fn_render = module_->getFunction("aurora_ui_render");
            if (fn_render) {
                if (node->left) gen_expr(node->left.get());
                builder_->CreateCall(fn_render, {});
            }
            if (node->body) gen_block(node->body.get());
            break;
        }
        case NodeType::Style:         gen_style(node);         break;
        case NodeType::Theme:         gen_theme(node);         break;
        case NodeType::Route:         gen_route(node);         break;
        case NodeType::Page: {
            /* Page: register route for this page component */
            std::string page_path = node->value.empty() ? "/" : "/" + node->value;
            llvm::Value* path_str = builder_->CreateGlobalStringPtr(page_path, "page_path");
            auto* fn_route = module_->getFunction("aurora_route_register");
            if (fn_route) {
                auto* handler = llvm::ConstantPointerNull::get(i8ptr_ty());
                builder_->CreateCall(fn_route, { path_str, handler });
            }
            if (node->body) gen_block(node->body.get());
            break;
        }
        case NodeType::Layout: {
            /* Layout: set up layout container with children */
            if (node->body) gen_block(node->body.get());
            auto* fn_layout = module_->getFunction("aurora_gui_layout_vertical");
            if (fn_layout && node->left) {
                llvm::Value* children = gen_expr(node->left.get());
                builder_->CreateCall(fn_layout, { children });
            }
            break;
        }
        case NodeType::Animate:       gen_animate(node);       break;
        case NodeType::Transition:    gen_transition(node);    break;
        case NodeType::Server:        gen_server(node);        break;
        case NodeType::Request: {
            /* Request: parse HTTP request data (side-effect only) */
            if (node->left) gen_expr(node->left.get());
            break;
        }
        case NodeType::Response: {
            /* Response: build HTTP response (side-effect only) */
            if (node->left) gen_expr(node->left.get());
            break;
        }
        case NodeType::Api:           gen_api(node);           break;
        case NodeType::Middleware: {
            /* Middleware: register a middleware function in the pipeline */
            if (node->body) gen_block(node->body.get());
            auto* fn_auth = module_->getFunction("aurora_auth_login");
            if (fn_auth && node->left) {
                llvm::Value* req = gen_expr(node->left.get());
                builder_->CreateCall(fn_auth, { req, llvm::ConstantPointerNull::get(i8ptr_ty()) });
            }
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
            /* Model: define data model with schema */
            if (node->body) gen_block(node->body.get());
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
            /* Update: frame start, body, frame end, render */
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
