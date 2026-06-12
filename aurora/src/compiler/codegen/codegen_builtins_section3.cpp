#include "compiler/codegen_builtins.hpp"
#include "compiler/ast.hpp"

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>

static llvm::Type* i64_ty(llvm::LLVMContext& ctx) { return llvm::Type::getInt64Ty(ctx); }
static llvm::Type* i8_ty(llvm::LLVMContext& ctx) { return llvm::Type::getInt8Ty(ctx); }
static llvm::Type* void_ty(llvm::LLVMContext& ctx) { return llvm::Type::getVoidTy(ctx); }
static llvm::Type* ptr_ty(llvm::LLVMContext& ctx) { return llvm::PointerType::getUnqual(ctx); }

static llvm::Value* to_ptr(llvm::IRBuilder<>& builder, llvm::LLVMContext& ctx,
                            llvm::Value* val) {
    if (val->getType()->isPointerTy()) return val;
    return builder.CreateIntToPtr(val, ptr_ty(ctx), "i64toptr");
}

llvm::Value* codegen_builtin_section3(
    const std::string& name,
    const ASTNode* node,
    llvm::IRBuilder<>& builder,
    llvm::LLVMContext& ctx,
    const BuiltinFunctions& builtins,
    std::function<llvm::Value*(const ASTNode*)> gen_expr,
    llvm::Module* module)
{
    auto* i64 = i64_ty(ctx);
    auto* v   = void_ty(ctx);
    auto* ptr = ptr_ty(ctx);
    auto* dbl = llvm::Type::getDoubleTy(ctx);
    /* ── Collection builtins ── */
    if (name == "push" && node->args && node->args->next) {
        llvm::Value* arr = gen_expr(node->args.get());
        llvm::Value* val = gen_expr(node->args->next.get());
        if (val->getType()->isPointerTy())
            builder.CreateCall(builtins.push_str_fn, { arr, val });
        else
            builder.CreateCall(builtins.push_fn, { arr, val });
        return llvm::ConstantInt::get(i64, 0);
    }
    if (name == "pop" && node->args) {
        llvm::Value* arr = gen_expr(node->args.get());
        return builder.CreateCall(builtins.pop_fn, { arr }, "pop_ret");
    }
    if (name == "insert" && node->args && node->args->next && node->args->next->next) {
        llvm::Value* arr = gen_expr(node->args.get());
        llvm::Value* idx = gen_expr(node->args->next.get());
        llvm::Value* val = gen_expr(node->args->next->next.get());
        builder.CreateCall(builtins.insert_fn, { arr, idx, val });
        return llvm::ConstantInt::get(i64, 0);
    }
    if (name == "remove" && node->args && node->args->next) {
        llvm::Value* arr = gen_expr(node->args.get());
        llvm::Value* idx = gen_expr(node->args->next.get());
        builder.CreateCall(builtins.remove_fn, { arr, idx });
        return llvm::ConstantInt::get(i64, 0);
    }
    if (name == "clear" && node->args) {
        llvm::Value* arr = gen_expr(node->args.get());
        builder.CreateCall(builtins.clear_fn, { arr });
        return llvm::ConstantInt::get(i64, 0);
    }
    if (name == "sort" && node->args) {
        llvm::Value* arr = gen_expr(node->args.get());
        builder.CreateCall(builtins.sort_fn, { arr });
        return llvm::ConstantInt::get(i64, 0);
    }
    if (name == "unique" && node->args) {
        llvm::Value* arr = gen_expr(node->args.get());
        return builder.CreateCall(builtins.unique_fn, { arr }, "unique_ret");
    }

    /* ── Error / Confirm builtins ── */
    if (name == "error" && node->args) {
        llvm::Value* msg = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.error_fn, { msg }, "error_ret");
    }
    if (name == "ask" && node->args) {
        llvm::Value* prompt = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.ask_fn, { prompt }, "ask_ret");
    }

    /* ── char(str) — get first character ── */
    if (name == "char" && node->args) {
        llvm::Value* s = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.char_fn, { s }, "char_ret");
    }

    /* ── dir(path) — alias for path ── */
    if (name == "dir" && node->args) {
        llvm::Value* p = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.dirname_fn, { p }, "dir_ret");
    }

    /* ── spawn(task) ── */
    if (name == "spawn" && node->args) {
        llvm::Value* task = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.spawn_fn, { task }, "spawn_ret");
    }

    /* ── await(task) ── */
    if (name == "await" && node->args) {
        llvm::Value* task = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.await_fn, { task }, "await_ret");
    }

    /* ── chan(cap) ── */
    if (name == "chan" && node->args) {
        llvm::Value* cap = gen_expr(node->args.get());
        if (cap->getType()->isDoubleTy())
            cap = builder.CreateFPToSI(cap, i64, "fptosi");
        return builder.CreateCall(builtins.chan_fn, { cap }, "chan_ret");
    }

    /* ── send(chan, val) ── */
    if (name == "send" && node->args && node->args->next) {
        llvm::Value* ch = gen_expr(node->args.get());
        llvm::Value* val = gen_expr(node->args->next.get());
        builder.CreateCall(builtins.send_fn, { ch, val });
        return llvm::ConstantInt::get(i64, 0);
    }

    /* ── recv(chan) ── */
    if (name == "recv" && node->args) {
        llvm::Value* ch = gen_expr(node->args.get());
        return builder.CreateCall(builtins.recv_fn, { ch }, "recv_ret");
    }

    /* ── fiber_create(fn, arg) ── */
    if (name == "fiber_create" && node->args && node->args->next) {
        llvm::Value* fn = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* arg = gen_expr(node->args->next.get());
        if (arg->getType()->isDoubleTy())
            arg = builder.CreateFPToSI(arg, i64, "fptosi");
        return builder.CreateCall(builtins.fiber_create_fn, { fn, arg }, "fiber_ret");
    }
    /* ── fiber_resume(fiber) ── */
    if (name == "fiber_resume" && node->args) {
        llvm::Value* f = gen_expr(node->args.get());
        if (f->getType()->isDoubleTy())
            f = builder.CreateFPToSI(f, i64, "fptosi");
        builder.CreateCall(builtins.fiber_resume_fn, { f });
        return llvm::ConstantInt::get(i64, 0);
    }
    /* ── fiber_yield() ── */
    if (name == "fiber_yield" && !node->args) {
        builder.CreateCall(builtins.fiber_yield_fn, {});
        return llvm::ConstantInt::get(i64, 0);
    }
    /* ── fiber_is_done(fiber) ── */
    if (name == "fiber_is_done" && node->args) {
        llvm::Value* f = gen_expr(node->args.get());
        if (f->getType()->isDoubleTy())
            f = builder.CreateFPToSI(f, i64, "fptosi");
        return builder.CreateCall(builtins.fiber_is_done_fn, { f }, "fiber_done");
    }
    /* ── fiber_get_result(fiber) ── */
    if (name == "fiber_get_result" && node->args) {
        llvm::Value* f = gen_expr(node->args.get());
        if (f->getType()->isDoubleTy())
            f = builder.CreateFPToSI(f, i64, "fptosi");
        return builder.CreateCall(builtins.fiber_get_result_fn, { f }, "fiber_result");
    }
    /* ── fiber_destroy(fiber) ── */
    if (name == "fiber_destroy" && node->args) {
        llvm::Value* f = gen_expr(node->args.get());
        if (f->getType()->isDoubleTy())
            f = builder.CreateFPToSI(f, i64, "fptosi");
        builder.CreateCall(builtins.fiber_destroy_fn, { f });
        return llvm::ConstantInt::get(i64, 0);
    }

    /* ── event_on(name, handler) ── */
    if (name == "event_on" && node->args && node->args->next) {
        llvm::Value* ev_name = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* handler = to_ptr(builder, ctx, gen_expr(node->args->next.get()));
        builder.CreateCall(builtins.event_on_fn, { ev_name, handler });
        return llvm::ConstantInt::get(i64, 0);
    }

    /* ── event_off(name, handler) ── */
    if (name == "event_off" && node->args && node->args->next) {
        llvm::Value* ev_name = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* handler = to_ptr(builder, ctx, gen_expr(node->args->next.get()));
        builder.CreateCall(builtins.event_off_fn, { ev_name, handler });
        return llvm::ConstantInt::get(i64, 0);
    }

    /* ── event_emit(name, arg) ── */
    if (name == "event_emit" && node->args && node->args->next) {
        llvm::Value* ev_name = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* arg = to_ptr(builder, ctx, gen_expr(node->args->next.get()));
        builder.CreateCall(builtins.event_emit_fn, { ev_name, arg });
        return llvm::ConstantInt::get(i64, 0);
    }

    /* ── measure() ── */
    if (name == "measure" && !node->args) {
        return builder.CreateCall(builtins.measure_fn, {}, "measure_ret");
    }

    /* ── bench(fn, iterations) ── */
    if (name == "bench" && node->args && node->args->next) {
        llvm::Value* fn = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* iters = gen_expr(node->args->next.get());
        if (iters->getType()->isDoubleTy())
            iters = builder.CreateFPToSI(iters, i64, "fptosi");
        return builder.CreateCall(builtins.bench_fn, { fn, iters }, "bench_ret");
    }

    /* ── profile(fn) ── */
    if (name == "profile" && node->args) {
        llvm::Value* fn = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.profile_fn, { fn }, "profile_ret");
    }

    /* ── trace(fn) ── */
    if (name == "trace" && node->args) {
        llvm::Value* fn = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.trace_fn, { fn }, "trace_ret");
    }

    /* ── fields(obj) ── */
    if (name == "fields" && node->args) {
        llvm::Value* obj = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.fields_fn, { obj }, "fields_ret");
    }

    /* ── methods(obj) ── */
    if (name == "methods" && node->args) {
        llvm::Value* obj = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.methods_fn, { obj }, "methods_ret");
    }

    /* ── install(pkg) ── */
    if (name == "install" && node->args) {
        llvm::Value* pkg = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.install_fn, { pkg }, "install_ret");
    }

    /* ── update(pkg) ── */
    if (name == "update" && node->args) {
        llvm::Value* pkg = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.update_fn, { pkg }, "update_ret");
    }

    /* ── search(query) ── */
    if (name == "search" && node->args) {
        llvm::Value* query = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.search_fn, { query }, "search_ret");
    }

    return nullptr;
}