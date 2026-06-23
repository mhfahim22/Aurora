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

llvm::Value* codegen_builtin_section2(
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
    /* ── Math builtins ── */
    auto to_dbl = [&](llvm::Value* v) -> llvm::Value* {
        if (v->getType()->isDoubleTy()) return v;
        return builder.CreateSIToFP(v, dbl, "itof");
    };
    auto from_dbl = [&](llvm::Value* v) -> llvm::Value* {
        if (v->getType()->isDoubleTy()) return v;
        return v;
    };

    if (name == "abs" && node->args) {
        llvm::Value* val = gen_expr(node->args.get());
        if (val->getType()->isDoubleTy())
            return builder.CreateCall(builtins.abs_fn, { val }, "abs_ret");
        llvm::Value* neg;
#if LLVM_VERSION_MAJOR >= 18
        neg = builder.CreateNSWNeg(val, "neg");
#else
        neg = builder.CreateNeg(val, "neg", false, true);
#endif
        llvm::Value* cmp = builder.CreateICmpSGT(val, llvm::ConstantInt::get(i64, 0), "abs_cmp");
        return builder.CreateSelect(cmp, val, neg, "abs_sel");
    }
    if (name == "sqrt" && node->args) {
        llvm::Value* val = to_dbl(gen_expr(node->args.get()));
        return builder.CreateCall(builtins.sqrt_fn, { val }, "sqrt_ret");
    }
    if (name == "floor" && node->args) {
        llvm::Value* val = to_dbl(gen_expr(node->args.get()));
        return builder.CreateCall(builtins.floor_fn, { val }, "floor_ret");
    }
    if (name == "ceil" && node->args) {
        llvm::Value* val = to_dbl(gen_expr(node->args.get()));
        return builder.CreateCall(builtins.ceil_fn, { val }, "ceil_ret");
    }
    if (name == "round" && node->args) {
        llvm::Value* val = to_dbl(gen_expr(node->args.get()));
        return builder.CreateCall(builtins.round_fn, { val }, "round_ret");
    }
    if (name == "pow" && node->args && node->args->next) {
        llvm::Value* a = to_dbl(gen_expr(node->args.get()));
        llvm::Value* b = to_dbl(gen_expr(node->args->next.get()));
        return builder.CreateCall(builtins.pow_fn, { a, b }, "pow_ret");
    }
    if (name == "clamp" && node->args && node->args->next && node->args->next->next) {
        llvm::Value* v = to_dbl(gen_expr(node->args.get()));
        llvm::Value* l = to_dbl(gen_expr(node->args->next.get()));
        llvm::Value* h = to_dbl(gen_expr(node->args->next->next.get()));
        return builder.CreateCall(builtins.clamp_fn, { v, l, h }, "clamp_ret");
    }
    if (name == "rand" && !node->args) {
        return builder.CreateCall(builtins.rand_fn, {}, "rand_ret");
    }

    /* ── File builtins ── */
    if (name == "read" && node->args) {
        llvm::Value* path = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.read_file, { path }, "read_ret");
    }
    if (name == "write" && node->args && node->args->next) {
        llvm::Value* path = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* cont = to_ptr(builder, ctx, gen_expr(node->args->next.get()));
        return builder.CreateCall(builtins.write_file, { path, cont }, "write_ret");
    }
    if (name == "append" && node->args && node->args->next) {
        llvm::Value* path = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* cont = to_ptr(builder, ctx, gen_expr(node->args->next.get()));
        return builder.CreateCall(builtins.append_file, { path, cont }, "append_ret");
    }
    if (name == "exists" && node->args) {
        llvm::Value* path = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.file_exists, { path }, "exists_ret");
    }
    if (name == "delete" && node->args) {
        llvm::Value* path = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.delete_file, { path }, "delete_ret");
    }
    if (name == "copy" && node->args && node->args->next) {
        llvm::Value* src = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* dst = to_ptr(builder, ctx, gen_expr(node->args->next.get()));
        return builder.CreateCall(builtins.copy_file, { src, dst }, "copy_ret");
    }
    if (name == "move" && node->args && node->args->next) {
        llvm::Value* old = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* nw  = to_ptr(builder, ctx, gen_expr(node->args->next.get()));
        return builder.CreateCall(builtins.move_file, { old, nw }, "move_ret");
    }

    /* ── Path builtins ── */
    if (name == "cwd" && !node->args) {
        return builder.CreateCall(builtins.cwd_fn, {}, "cwd_ret");
    }
    if (name == "cd" && node->args) {
        llvm::Value* p = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.cd_fn, { p }, "cd_ret");
    }
    if (name == "path" && node->args) {
        llvm::Value* p = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.dirname_fn, { p }, "path_ret");
    }
    if (name == "name" && node->args) {
        llvm::Value* p = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.basename_fn, { p }, "name_ret");
    }
    if (name == "ext" && node->args) {
        llvm::Value* p = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.ext_fn, { p }, "ext_ret");
    }

    /* ── Time builtins ── */
    if (name == "now" && !node->args) {
        return builder.CreateCall(builtins.now_fn, {}, "now_ret");
    }
    if (name == "stamp" && !node->args) {
        return builder.CreateCall(builtins.stamp_fn, {}, "stamp_ret");
    }
    if (name == "sleep" && node->args) {
        llvm::Value* ms = gen_expr(node->args.get());
        if (ms->getType()->isDoubleTy())
            ms = builder.CreateFPToSI(ms, i64, "fptosi");
        builder.CreateCall(builtins.sleep_fn, { ms });
        return llvm::ConstantInt::get(i64, 0);
    }

    /* ── OS builtins ── */
    if (name == "os" && !node->args) {
        llvm::Value* buf = builder.CreateAlloca(i8_ty(ctx), llvm::ConstantInt::get(i64, 64), "os_buf");
        llvm::Value* buf_i64 = builder.CreatePtrToInt(buf, i64, "os_buf_i64");
        llvm::Value* ret = builder.CreateCall(builtins.os_fn,
            { buf, llvm::ConstantInt::get(i64, 64) }, "os_ret");
        llvm::Function* str_from_cstr = module->getFunction("aurora_str_from_cstr");
        if (str_from_cstr)
            return builder.CreateCall(str_from_cstr, { buf }, "os_str");
        return ret;
    }
    if (name == "cpu" && !node->args) {
        return builder.CreateCall(builtins.cpu_fn, {}, "cpu_ret");
    }
    if (name == "mem" && !node->args) {
        return builder.CreateCall(builtins.mem_fn, {}, "mem_ret");
    }
    if (name == "env" && node->args) {
        llvm::Value* n = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.env_fn, { n }, "env_ret");
    }
    if (name == "run" && node->args) {
        llvm::Value* cmd = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.run_fn, { cmd }, "run_ret");
    }
    if (name == "exit" && node->args) {
        llvm::Value* code = gen_expr(node->args.get());
        if (code->getType()->isDoubleTy())
            code = builder.CreateFPToSI(code, i64, "fptosi");
        builder.CreateCall(builtins.exit_fn, { code });
        builder.CreateUnreachable();
        auto* dead_bb = llvm::BasicBlock::Create(ctx, "exit_dead", builder.GetInsertBlock()->getParent());
        builder.SetInsertPoint(dead_bb);
        return llvm::ConstantInt::get(i64, 0);
    }

    /* ── JSON builtins ── */
    if (name == "encode" && node->args) {
        llvm::Value* val = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.encode_json, { val }, "encode_ret");
    }
    if (name == "decode" && node->args) {
        llvm::Value* str = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.decode_json, { str }, "decode_ret");
    }

    /* ── HTTP builtins ── */
    if (name == "get" && node->args) {
        llvm::Value* url = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* buf = builder.CreateAlloca(i8_ty(ctx), llvm::ConstantInt::get(i64, 4096), "get_buf");
        llvm::Value* buf_i64 = builder.CreatePtrToInt(buf, i64, "get_buf_i64");
        llvm::Value* ret = builder.CreateCall(builtins.http_get,
            { url, buf, llvm::ConstantInt::get(i64, 4096) }, "get_ret");
        llvm::Function* str_from_cstr = module->getFunction("aurora_str_from_cstr");
        if (str_from_cstr)
            return builder.CreateCall(str_from_cstr, { buf }, "get_str");
        return ret;
    }
    if (name == "post" && node->args && node->args->next) {
        llvm::Value* url  = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* body = to_ptr(builder, ctx, gen_expr(node->args->next.get()));
        llvm::Value* ct   = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty(ctx)));
        if (node->args->next->next)
            ct = to_ptr(builder, ctx, gen_expr(node->args->next->next.get()));
        llvm::Value* buf = builder.CreateAlloca(i8_ty(ctx), llvm::ConstantInt::get(i64, 4096), "post_buf");
        llvm::Value* ret = builder.CreateCall(builtins.http_post,
            { url, body, ct, llvm::ConstantInt::get(i64, 4096) }, "post_ret");
        llvm::Function* str_from_cstr = module->getFunction("aurora_str_from_cstr");
        if (str_from_cstr)
            return builder.CreateCall(str_from_cstr, { buf }, "post_str");
        return ret;
    }

    return nullptr;
}