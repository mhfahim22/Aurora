#include "compiler/codegen.hpp"
#include "bridge_codegen.hpp"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <iostream>

void Codegen::gen_bridge_quickjs_fn(const ASTNode* node) {
    if (!node) return;
    const std::string& fname = node->value;
    if (fname.empty()) return;

    /* Build parameter types */
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

    std::string mod_name = node->right ? node->right->value : fname;
    auto* ptr_type = llvm::PointerType::get(ctx_, 0);

    /* ── Declare aurora_bridge_quickjs_call runtime function ── */
    std::string bridge_fn_name = "aurora_bridge_quickjs_call";
    auto* bridge_fn_type = llvm::FunctionType::get(
        ptr_type,
        { ptr_type, ptr_type, ptr_type,
          llvm::Type::getInt32Ty(ctx_), ptr_type },
        false);

    llvm::Function* bridge_fn = module_->getFunction(bridge_fn_name);
    if (!bridge_fn) {
        bridge_fn = llvm::Function::Create(
            bridge_fn_type, llvm::Function::ExternalLinkage,
            bridge_fn_name, module_.get());
    }

    /* ── Create wrapper function ── */
    auto* fn_type = llvm::FunctionType::get(ret_type, param_types, node->is_vararg);
    auto* wrapper = llvm::Function::Create(
        fn_type, llvm::Function::InternalLinkage, fname, module_.get());

    llvm::BasicBlock* saved_block = builder_->GetInsertBlock();
    auto* entry = llvm::BasicBlock::Create(ctx_, "qjs.entry", wrapper);
    builder_->SetInsertPoint(entry);

    /* Build args array */
    unsigned arg_count = static_cast<unsigned>(param_types.size());
    llvm::Value* args_array = nullptr;

    if (arg_count > 0) {
        auto* arr_type = llvm::ArrayType::get(ptr_type, arg_count);
        args_array = builder_->CreateAlloca(arr_type, nullptr, "qjs.args_arr");

        unsigned idx = 0;
        for (auto& arg : wrapper->args()) {
            llvm::Value* arg_ptr = builder_->CreateGEP(
                arr_type, args_array,
                { llvm::ConstantInt::get(ctx_, llvm::APInt(32, 0)),
                  llvm::ConstantInt::get(ctx_, llvm::APInt(32, idx)) },
                "qjs.arg." + std::to_string(idx));
            llvm::Value* arg_val = &arg;
            if (arg.getType()->isIntegerTy() && arg.getType()->getIntegerBitWidth() == 64) {
                arg_val = builder_->CreateIntToPtr(arg_val, ptr_type,
                                                    "qjs.arg.ptr." + std::to_string(idx));
            } else {
                arg_val = builder_->CreateBitCast(
                    builder_->CreateAlloca(arg.getType(), nullptr, "qjs.arg.copy." + std::to_string(idx)),
                    ptr_type, "qjs.arg.ptr." + std::to_string(idx));
            }
            builder_->CreateStore(arg_val, arg_ptr);
            idx++;
        }
    }

    llvm::Value* mod_name_str = builder_->CreateGlobalStringPtr(mod_name, "qjs.mod");
    llvm::Value* fn_name_str = builder_->CreateGlobalStringPtr(fname, "qjs.fn");
    llvm::Value* arg_count_val = llvm::ConstantInt::get(ctx_, llvm::APInt(32, arg_count));
    llvm::Value* ret_type_str = builder_->CreateGlobalStringPtr(ret_type_name, "qjs.ret");

    std::vector<llvm::Value*> call_args;
    call_args.push_back(mod_name_str);
    call_args.push_back(fn_name_str);
    call_args.push_back(arg_count > 0 ? args_array
                         : llvm::ConstantPointerNull::get(ptr_type));
    call_args.push_back(arg_count_val);
    call_args.push_back(ret_type_str);

    llvm::Value* result = builder_->CreateCall(bridge_fn, call_args, "qjs.result");

    if (!ret_type->isVoidTy()) {
        if (ret_type->isIntegerTy() && ret_type->getIntegerBitWidth() == 64) {
            result = builder_->CreatePtrToInt(result, ret_type, "qjs.result.int");
        } else if (ret_type->isDoubleTy()) {
            result = builder_->CreatePtrToInt(result, i64_ty(), "qjs.result.i64");
            result = builder_->CreateBitCast(result, ret_type, "qjs.result.f64");
        } else {
            result = builder_->CreateBitCast(result, ret_type, "qjs.result.bitcast");
        }
    }

    builder_->CreateRet(ret_type->isVoidTy() ? nullptr : result);

    if (saved_block)
        builder_->SetInsertPoint(saved_block);

    std::cerr << "[bridge:quickjs] extern function '" << fname
              << "' from module '" << mod_name << "'\n";
}
