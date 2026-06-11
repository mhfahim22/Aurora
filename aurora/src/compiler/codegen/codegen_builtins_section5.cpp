#include "compiler/codegen_builtins.hpp"
#include "compiler/ast.hpp"

llvm::Value* codegen_builtin_section5(
    const std::string& name,
    const ASTNode* node,
    llvm::IRBuilder<>& builder,
    llvm::LLVMContext& ctx,
    const BuiltinFunctions& builtins,
    std::function<llvm::Value*(const ASTNode*)> gen_expr,
    llvm::Module* module)
{
    (void)node; (void)builder; (void)ctx; (void)builtins; (void)gen_expr; (void)module; (void)name;
    return nullptr;
}
