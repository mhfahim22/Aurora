#pragma once
/* ════════════════════════════════════════════════════════════
   Aurora Optimizer — Declarations
   ════════════════════════════════════════════════════════════
   Custom optimization passes for Aurora IR.
   These run before LLVM's built-in O3 pipeline.
   ════════════════════════════════════════════════════════════ */

namespace llvm {
    class Function;
    class Module;
}

/* ── const_fold.cpp ── */
bool fold_binary_constants(llvm::Function& fn);
bool fold_comparison_constants(llvm::Function& fn);
bool fold_unary_constants(llvm::Function& fn);
bool fold_branch_constants(llvm::Function& fn);

/* ── dead_code.cpp ── */
bool eliminate_dead_instructions(llvm::Function& fn);
bool eliminate_unreachable_blocks(llvm::Function& fn);

/* ── strength_reduce.cpp ── */
bool strength_reduce(llvm::Function& fn);

/* ── Main entry point (aurora_optimizer.cpp) ── */
void run_aurora_optimizer(llvm::Module* module);
