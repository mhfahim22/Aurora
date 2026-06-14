#pragma once
#include "compiler/ir/ir.hpp"

/* ════════════════════════════════════════════════════════════
   Aurora IR Optimizer Passes
   ════════════════════════════════════════════════════════════
   Operate on IrFunction instead of llvm::Function.
   ════════════════════════════════════════════════════════════ */

/* mem2reg — promote alloca/load/store to SSA values */
bool ir_mem2reg(IrFunction& fn, std::vector<IrType>& pool);

/* Constant folding — evaluate constant expressions at compile time */
bool ir_fold_constants(IrFunction& fn, std::vector<IrType>& pool);

/* Dead code elimination — remove unused instructions */
bool ir_eliminate_dead_code(IrFunction& fn, std::vector<IrType>& pool);

/* Strength reduction — replace expensive ops with cheaper ones */
bool ir_strength_reduce(IrFunction& fn, std::vector<IrType>& pool);

/* Run all passes in a fixed-point loop (max 5 iterations) */
void ir_optimize(IrModule& mod);
