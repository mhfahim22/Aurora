#pragma once
#include "compiler/ir/ir.hpp"

namespace llvm {
    class Module;
    class LLVMContext;
}

/* ── Lower Aurora IR module to LLVM IR module ── */
llvm::Module* lower_ir_to_llvm(const IrModule& ir_mod, llvm::LLVMContext& ctx);
