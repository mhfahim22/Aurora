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

llvm::Value* codegen_builtin_section1(
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
    /* ── len(arr/str) — single-argument length ── */
    if (name == "len" && node->args && !node->args->next) {
        llvm::Value* val = gen_expr(node->args.get());
        if (!val) return llvm::ConstantInt::get(i64, 0);
        if (val->getType()->isPointerTy())
            return builder.CreateCall(builtins.strlen_fn, { val }, "len_ret");
        return builder.CreateCall(builtins.len, { val }, "len_ret");
    }

    /* ── sum(arr) — single-argument array sum ── */
    if (name == "sum" && node->args && !node->args->next) {
        llvm::Value* val = gen_expr(node->args.get());
        if (val) return builder.CreateCall(builtins.sum, { val }, "sum_ret");
        return llvm::ConstantInt::get(i64, 0);
    }

    /* ── min(arr) — single-argument array min ── */
    if (name == "min" && node->args && !node->args->next) {
        llvm::Value* val = gen_expr(node->args.get());
        if (val) return builder.CreateCall(builtins.min, { val }, "min_ret");
        return llvm::ConstantInt::get(i64, 0);
    }

    /* ── min(a, b) — two-argument min ── */
    if (name == "min" && node->args && node->args->next && !node->args->next->next) {
        llvm::Value* a = gen_expr(node->args.get());
        llvm::Value* b = gen_expr(node->args->next.get());
        if (!a) a = llvm::ConstantInt::get(i64, 0);
        if (!b) b = llvm::ConstantInt::get(i64, 0);
        bool is_float = a->getType()->isDoubleTy() || b->getType()->isDoubleTy();
        if (is_float) {
            if (!a->getType()->isDoubleTy()) a = builder.CreateSIToFP(a, dbl, "itof");
            if (!b->getType()->isDoubleTy()) b = builder.CreateSIToFP(b, dbl, "itof");
            return builder.CreateCall(builtins.min2, { a, b }, "min_ret");
        }
        llvm::Value* cmp = builder.CreateICmpSLT(a, b, "min_cmp");
        return builder.CreateSelect(cmp, a, b, "min_sel");
    }

    /* ── max(arr) — single-argument array max ── */
    if (name == "max" && node->args && !node->args->next) {
        llvm::Value* val = gen_expr(node->args.get());
        if (val) return builder.CreateCall(builtins.max, { val }, "max_ret");
        return llvm::ConstantInt::get(i64, 0);
    }

    /* ── max(a, b) — two-argument max ── */
    if (name == "max" && node->args && node->args->next && !node->args->next->next) {
        llvm::Value* a = gen_expr(node->args.get());
        llvm::Value* b = gen_expr(node->args->next.get());
        if (!a) a = llvm::ConstantInt::get(i64, 0);
        if (!b) b = llvm::ConstantInt::get(i64, 0);
        bool is_float = a->getType()->isDoubleTy() || b->getType()->isDoubleTy();
        if (is_float) {
            if (!a->getType()->isDoubleTy()) a = builder.CreateSIToFP(a, dbl, "itof");
            if (!b->getType()->isDoubleTy()) b = builder.CreateSIToFP(b, dbl, "itof");
            return builder.CreateCall(builtins.max2, { a, b }, "max_ret");
        }
        llvm::Value* cmp = builder.CreateICmpSGT(a, b, "max_cmp");
        return builder.CreateSelect(cmp, a, b, "max_sel");
    }

    /* ── range(start, end) or range(end) ── */
    if (name == "range" && node->args) {
        llvm::Value* arg1 = gen_expr(node->args.get());
        llvm::Value* arg2 = nullptr;
        if (node->args->next)
            arg2 = gen_expr(node->args->next.get());
        if (!arg2) {
            arg2 = arg1;
            arg1 = llvm::ConstantInt::get(i64, 0);
        }
        return builder.CreateCall(builtins.range, { arg1, arg2 }, "range_ret");
    }

    /* ── outputln(value) ── */
    if (name == "outputln" && node->args) {
        llvm::Value* val = gen_expr(node->args.get());
        if (!val) return llvm::ConstantInt::get(i64, 0);
        if (val->getType()->isDoubleTy())
            builder.CreateCall(builtins.outputln_float, { val });
        else if (val->getType()->isPointerTy())
            builder.CreateCall(builtins.outputln_str, { val });
        else
            builder.CreateCall(builtins.outputln_int, { val });
        return llvm::ConstantInt::get(i64, 0);
    }

    /* ── outputN() ── */
    if (name == "outputN") {
        builder.CreateCall(builtins.outputN);
        return llvm::ConstantInt::get(i64, 0);
    }

    /* ── outputf(fmt, ...) ── */
    if (name == "outputf" && node->args) {
        llvm::Value* fmt_val = gen_expr(node->args.get());
        if (!fmt_val) return llvm::ConstantInt::get(i64, 0);
        std::vector<llvm::Value*> args_vec;
        const ASTNode* arg = node->args->next.get();
        int argc = 0;
        while (arg) {
            args_vec.push_back(gen_expr(arg));
            argc++;
            arg = arg->next.get();
        }
        llvm::Value* argv_ptr = nullptr;
        if (argc > 0) {
            llvm::Type* i64_arr_ty = llvm::ArrayType::get(i64, argc);
            llvm::Value* arr = new llvm::AllocaInst(i64_arr_ty, 0, "outf_argv", &*builder.GetInsertBlock()->getFirstInsertionPt());
            for (int i = 0; i < argc; i++) {
                llvm::Value* elem = builder.CreateGEP(i64_arr_ty, arr,
                    { llvm::ConstantInt::get(i64, 0),
                      llvm::ConstantInt::get(i64, i) });
                llvm::Value* val = args_vec[i];
                if (val->getType()->isPointerTy())
                    val = builder.CreatePtrToInt(val, i64);
                builder.CreateStore(val, elem);
            }
            argv_ptr = builder.CreateBitCast(arr, ptr_ty(ctx));
        } else {
            argv_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty(ctx)));
        }
        builder.CreateCall(builtins.outputf, { fmt_val,
            llvm::ConstantInt::get(i64, argc), argv_ptr });
        return llvm::ConstantInt::get(i64, 0);
    }

    /* ── input() ── */
    if (name == "input") {
        return builder.CreateCall(builtins.input, {}, "input_ret");
    }

    /* ── typeof(expr) ── */
    if (name == "typeof") {
        llvm::Value* val = node->args ? gen_expr(node->args.get()) : nullptr;
        llvm::Value* raw;
        if (val) {
            if (val->getType()->isDoubleTy())
                raw = builder.CreateGlobalStringPtr("float", "typeof");
            else if (val->getType()->isPointerTy())
                raw = builder.CreateGlobalStringPtr("string", "typeof");
            else if (val->getType()->isIntegerTy(64))
                raw = builder.CreateGlobalStringPtr("int", "typeof");
            else if (val->getType()->isIntegerTy(1))
                raw = builder.CreateGlobalStringPtr("bool", "typeof");
            else
                raw = builder.CreateGlobalStringPtr("unknown", "typeof");
        } else {
            raw = builder.CreateGlobalStringPtr("unknown", "typeof");
        }
        llvm::Function* str_from_cstr = module->getFunction("aurora_str_from_cstr");
        if (str_from_cstr)
            return builder.CreateCall(str_from_cstr, { raw }, "typeof_str");
        return raw;
    }

    /* ── sizeof(expr) ── */
    if (name == "sizeof") {
        llvm::Value* val = node->args ? gen_expr(node->args.get()) : nullptr;
        if (val) {
            llvm::Type* ty = val->getType();
            if (ty->isDoubleTy())
                return llvm::ConstantInt::get(i64, 8);
            if (ty->isPointerTy())
                return llvm::ConstantInt::get(i64, 8);
            if (ty->isIntegerTy(64))
                return llvm::ConstantInt::get(i64, 8);
            if (ty->isIntegerTy(32))
                return llvm::ConstantInt::get(i64, 4);
            if (ty->isIntegerTy(8))
                return llvm::ConstantInt::get(i64, 1);
            auto& layout = module->getDataLayout();
            return llvm::ConstantInt::get(i64, layout.getTypeAllocSize(ty));
        }
        return llvm::ConstantInt::get(i64, 8);
    }

    /* ── String builtins ── */
    if (name == "upper" && node->args) {
        llvm::Value* s = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.upper, { s }, "upper_ret");
    }
    if (name == "lower" && node->args) {
        llvm::Value* s = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.lower, { s }, "lower_ret");
    }
    if (name == "trim" && node->args) {
        llvm::Value* s = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.trim, { s }, "trim_ret");
    }
    if (name == "replace" && node->args && node->args->next && node->args->next->next) {
        llvm::Value* s   = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* old = to_ptr(builder, ctx, gen_expr(node->args->next.get()));
        llvm::Value* nw  = to_ptr(builder, ctx, gen_expr(node->args->next->next.get()));
        return builder.CreateCall(builtins.replace_fn, { s, old, nw }, "replace_ret");
    }
    if (name == "split" && node->args && node->args->next) {
        llvm::Value* s = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* d = to_ptr(builder, ctx, gen_expr(node->args->next.get()));
        return builder.CreateCall(builtins.split_fn, { s, d }, "split_ret");
    }
    if (name == "join" && node->args && node->args->next) {
        llvm::Value* arr = gen_expr(node->args.get());
        llvm::Value* sep = to_ptr(builder, ctx, gen_expr(node->args->next.get()));
        return builder.CreateCall(builtins.join_fn, { arr, sep }, "join_ret");
    }
    if (name == "has" && node->args && node->args->next) {
        llvm::Value* a = gen_expr(node->args.get());
        llvm::Value* b = gen_expr(node->args->next.get());
        if (a->getType()->isPointerTy() || b->getType()->isPointerTy()) {
            a = to_ptr(builder, ctx, a);
            b = to_ptr(builder, ctx, b);
            return builder.CreateCall(builtins.has_str, { a, b }, "has_ret");
        }
        return builder.CreateCall(builtins.has_arr, { a, b }, "has_ret");
    }
    if (name == "starts" && node->args && node->args->next) {
        llvm::Value* s = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* p = to_ptr(builder, ctx, gen_expr(node->args->next.get()));
        return builder.CreateCall(builtins.starts, { s, p }, "starts_ret");
    }
    if (name == "ends" && node->args && node->args->next) {
        llvm::Value* s = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* p = to_ptr(builder, ctx, gen_expr(node->args->next.get()));
        return builder.CreateCall(builtins.ends, { s, p }, "ends_ret");
    }
    if (name == "reverse" && node->args) {
        llvm::Value* val = gen_expr(node->args.get());
        if (val->getType()->isPointerTy())
            return builder.CreateCall(builtins.reverse_str, { val }, "reverse_ret");
        builder.CreateCall(builtins.reverse_arr, { val });
        return llvm::ConstantInt::get(i64, 0);
    }

    /* ── strlen(str) — alias for len(str) ── */
    if (name == "strlen" && node->args && !node->args->next) {
        llvm::Value* val = gen_expr(node->args.get());
        if (!val) return llvm::ConstantInt::get(i64, 0);
        if (val->getType()->isPointerTy())
            return builder.CreateCall(builtins.strlen_fn, { val }, "len_ret");
        return llvm::ConstantInt::get(i64, 0);
    }

    /* ── strcat(a, b) — string concatenation ── */
    if (name == "strcat" && node->args && node->args->next && !node->args->next->next) {
        llvm::Value* a = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* b = to_ptr(builder, ctx, gen_expr(node->args->next.get()));
        llvm::Function* append_fn = module->getFunction("aurora_str_append");
        if (append_fn) return builder.CreateCall(append_fn, { a, b }, "strcat_ret");
        return llvm::ConstantInt::get(i64, 0);
    }

    /* ── substr(str, start, len) — substring ── */
    if (name == "substr" && node->args && node->args->next && node->args->next->next && !node->args->next->next->next) {
        llvm::Value* s     = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* start = gen_expr(node->args->next.get());
        llvm::Value* len   = gen_expr(node->args->next->next.get());
        if (!start) start = llvm::ConstantInt::get(i64, 0);
        if (!len)   len   = llvm::ConstantInt::get(i64, 0);
        llvm::Function* substr_fn = module->getFunction("aurora_substr");
        if (substr_fn) return builder.CreateCall(substr_fn, { s, start, len }, "substr_ret");
        return llvm::ConstantInt::get(i64, 0);
    }

    /* ── index(str, sub) — find first index of substring ── */
    if (name == "index" && node->args && node->args->next && !node->args->next->next) {
        llvm::Value* s   = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* sub = to_ptr(builder, ctx, gen_expr(node->args->next.get()));
        llvm::Function* idx_fn = module->getFunction("aurora_str_index");
        if (idx_fn) return builder.CreateCall(idx_fn, { s, sub }, "index_ret");
        return llvm::ConstantInt::get(i64, -1);
    }

    /* ── Type conversion builtins ── */
    if (name == "str" && node->args) {
        llvm::Value* val = gen_expr(node->args.get());
        if (!val) return llvm::ConstantInt::get(i64, 0);
        if (val->getType()->isPointerTy()) return val;
        if (val->getType()->isDoubleTy()) {
            llvm::Function* f2s = module->getFunction("aurora_float_to_str");
            if (f2s) return builder.CreateCall(f2s, { val }, "str_ret");
        }
        llvm::Function* int_to_str = module->getFunction("aurora_int_to_str");
        if (int_to_str)
            return builder.CreateCall(int_to_str, { val }, "str_ret");
        return val;
    }
    if (name == "int" && node->args) {
        llvm::Value* val = gen_expr(node->args.get());
        if (!val) return llvm::ConstantInt::get(i64, 0);
        if (val->getType()->isDoubleTy())
            return builder.CreateFPToSI(val, i64, "fptosi");
        if (val->getType()->isPointerTy())
            return llvm::ConstantInt::get(i64, 0);
        return val;
    }
    if (name == "float" && node->args) {
        llvm::Value* val = gen_expr(node->args.get());
        if (!val) return llvm::ConstantFP::get(dbl, 0.0);
        if (val->getType()->isDoubleTy()) return val;
        if (val->getType()->isPointerTy())
            return llvm::ConstantFP::get(dbl, 0.0);
        return builder.CreateSIToFP(val, dbl, "itof");
    }
    if (name == "bool" && node->args) {
        llvm::Value* val = gen_expr(node->args.get());
        if (!val) return llvm::ConstantInt::get(i64, 0);
        llvm::Value* cmp;
        if (val->getType()->isPointerTy()) {
            llvm::Value* as_int = builder.CreatePtrToInt(val, i64, "ptrtoint");
            cmp = builder.CreateICmpNE(as_int, llvm::ConstantInt::get(i64, 0), "bool_cmp");
        } else {
            cmp = builder.CreateICmpNE(val, llvm::ConstantInt::get(i64, 0), "bool_cmp");
        }
        return builder.CreateZExt(cmp, i64, "bool_ret");
    }
    return nullptr;
}