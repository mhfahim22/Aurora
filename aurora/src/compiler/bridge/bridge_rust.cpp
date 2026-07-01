#include "compiler/codegen.hpp"
#include "bridge_codegen.hpp"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <iostream>

void Codegen::gen_bridge_rust_fn(const ASTNode* node) {
    if (!node) return;
    const std::string& fname = node->value;
    if (fname.empty()) return;

    /* Build parameter types (same as gen_extern_fn) */
    std::vector<llvm::Type*> param_types;
    const ASTNode* param = node->args.get();
    while (param) {
        std::string ptype_name = "int";
        auto pk = get_annotation_kind(param);
        if (pk != AstTypeKind::Unknown) {
            if (pk == AstTypeKind::Struct && !param->type_annotation.type_name.empty())
                ptype_name = param->type_annotation.type_name;
            else
                ptype_name = ast_type_kind_name(pk);
        } else if (param->right) {
            ptype_name = param->right->value;
        }
        param_types.push_back(extern_type_to_llvm(ptype_name));
        param = param->next.get();
    }

    std::string ret_type_name = "void";
    {
        auto rk = get_annotation_kind(node);
        if (rk != AstTypeKind::Unknown) {
            if (rk == AstTypeKind::Struct && !node->type_annotation.type_name.empty())
                ret_type_name = node->type_annotation.type_name;
            else
                ret_type_name = ast_type_kind_name(rk);
        } else if (node->left) {
            ret_type_name = node->left->value;
        }
    }
    llvm::Type* ret_type = extern_type_to_llvm(ret_type_name);

    /* Rust cdylibs expose C ABI — use aurora_dl_resolve + function pointer call.
       This is similar to gen_extern_fn Case 2 but uses ecosystem resolution. */

    auto* ptr_type = llvm::PointerType::get(ctx_, 0);

    /* Library name: if not specified, use the function name */
    std::string lib_name = node->right ? node->right->value : fname;
    /* Rust cdylib naming convention: lib<name>.so / <name>.dll */
    /* The runtime resolver handles platform-specific naming */

    /* Create global function pointer (i8*) initialized to null */
    auto* fn_ptr_global = new llvm::GlobalVariable(
        *module_, ptr_type, false,
        llvm::GlobalValue::InternalLinkage,
        llvm::ConstantPointerNull::get(ptr_type),
        fname + ".rs_ptr");

    /* Create wrapper function with proper signature */
    auto* fn_type = llvm::FunctionType::get(ret_type, param_types, node->is_vararg);
    auto* wrapper = llvm::Function::Create(
        fn_type, llvm::Function::InternalLinkage, fname, module_.get());

    llvm::BasicBlock* saved_block = builder_->GetInsertBlock();

    auto* entry   = llvm::BasicBlock::Create(ctx_, "rs.entry", wrapper);
    auto* resolve = llvm::BasicBlock::Create(ctx_, "rs.resolve", wrapper);
    auto* call_bb = llvm::BasicBlock::Create(ctx_, "rs.call", wrapper);

    /* ── entry block: check if ptr is null ── */
    builder_->SetInsertPoint(entry);
    llvm::Value* loaded = builder_->CreateLoad(ptr_type, fn_ptr_global, "rs.ptr");
    llvm::Value* is_null = builder_->CreateIsNull(loaded, "rs.isnull");
    builder_->CreateCondBr(is_null, resolve, call_bb);

    /* ── resolve block: call aurora_dl_resolve ── */
    builder_->SetInsertPoint(resolve);

    /* Use ecosystem resolve for Rust crates */
    llvm::Function* resolve_fn = module_->getFunction("aurora_ecosystem_resolve");
    if (!resolve_fn) {
        resolve_fn = llvm::Function::Create(
            llvm::FunctionType::get(ptr_type, { ptr_type, ptr_type }, false),
            llvm::Function::ExternalLinkage, "aurora_ecosystem_resolve", module_.get());
    }

    llvm::Value* lib_cstr = builder_->CreateGlobalStringPtr(
        "cargo_" + lib_name, "rs.lib");
    llvm::Value* fn_cstr  = builder_->CreateGlobalStringPtr(fname, "rs.fn");
    llvm::Value* resolved = builder_->CreateCall(resolve_fn, { lib_cstr, fn_cstr },
                                                  "rs.resolved");

    /* Error check */
    auto* err_check = llvm::BasicBlock::Create(ctx_, "rs.errcheck", wrapper);
    auto* err_blk   = llvm::BasicBlock::Create(ctx_, "rs.error", wrapper);
    auto* resolve_cont = llvm::BasicBlock::Create(ctx_, "rs.resolve_cont", wrapper);

    builder_->CreateBr(err_check);

    builder_->SetInsertPoint(err_check);
    llvm::Value* is_err = builder_->CreateIsNull(resolved, "rs.errnull");
    builder_->CreateCondBr(is_err, err_blk, resolve_cont);

    builder_->SetInsertPoint(err_blk);
    std::string err_msg = "[bridge:rust] ERROR: failed to resolve '" + fname
        + "' from Rust crate '" + lib_name + "'\n";
    llvm::Value* err_msg_str = builder_->CreateGlobalStringPtr(err_msg, "rs.errmsg");
    llvm::Function* panic_fn = module_->getFunction("aurora_panic");
    if (!panic_fn) {
        auto* void_ty = llvm::Type::getVoidTy(ctx_);
        auto* pt = llvm::PointerType::getUnqual(ctx_);
        auto* ft = llvm::FunctionType::get(void_ty, { pt }, false);
        panic_fn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                           "aurora_panic", module_.get());
    }
    builder_->CreateCall(panic_fn, { err_msg_str });
    builder_->CreateUnreachable();

    builder_->SetInsertPoint(resolve_cont);
    builder_->CreateStore(resolved, fn_ptr_global);
    builder_->CreateBr(call_bb);

    /* ── call block: call through resolved function pointer ── */
    builder_->SetInsertPoint(call_bb);

    llvm::PHINode* fn_ptr_phi = builder_->CreatePHI(ptr_type, 2, "rs.fnphi");
    fn_ptr_phi->addIncoming(loaded, entry);
    fn_ptr_phi->addIncoming(resolved, resolve_cont);

    /* Bitcast void* to proper function pointer type */
    auto* fn_ptr_typed = builder_->CreateBitCast(
        fn_ptr_phi, llvm::PointerType::get(fn_type, 0), "rs.fncast");

    std::vector<llvm::Value*> args;
    for (auto& arg : wrapper->args())
        args.push_back(&arg);

    std::string call_name = ret_type->isVoidTy() ? "" : "rs.call";
    llvm::CallInst* call_result = builder_->CreateCall(fn_type, fn_ptr_typed, args, call_name);
    builder_->CreateRet(ret_type->isVoidTy() ? nullptr : call_result);

    if (saved_block)
        builder_->SetInsertPoint(saved_block);

    std::cerr << "[bridge:rust] extern function '" << fname
              << "' from crate '" << lib_name << "'\n";
}
