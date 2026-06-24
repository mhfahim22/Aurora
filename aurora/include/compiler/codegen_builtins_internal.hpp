#pragma once
#include "compiler/codegen_builtins.hpp"
#include "compiler/ast.hpp"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>

struct BuiCtx {
    const std::string& name;
    const ASTNode* node;
    llvm::IRBuilder<>& builder;
    llvm::LLVMContext& ctx;
    const BuiltinFunctions& builtins;
    std::function<llvm::Value*(const ASTNode*)> gen_expr;
    llvm::Type* i64;
    llvm::Type* ptr;
    llvm::Type* dbl;
    llvm::Module* module;
};

llvm::Value* to_ptr(llvm::IRBuilder<>& builder, llvm::LLVMContext& ctx, llvm::Value* val);

llvm::Value* codegen_builtin_math(const BuiCtx& c);
llvm::Value* codegen_builtin_io(const BuiCtx& c);
llvm::Value* codegen_builtin_coll(const BuiCtx& c);
llvm::Value* codegen_builtin_ai(const BuiCtx& c);
llvm::Value* codegen_builtin_backend(const BuiCtx& c);
