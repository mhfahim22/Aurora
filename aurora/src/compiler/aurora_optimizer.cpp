#include "compiler/aurora_optimizer.hpp"

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>

/* ════════════════════════════════════════════════════════════
   Aurora Optimizer — Main Entry Point
   ════════════════════════════════════════════════════════════
   Runs all optimization passes in a fixed-point loop.
   Passes are defined in:
     - const_fold.cpp (constant folding)
     - dead_code.cpp   (dead code elimination)
   ════════════════════════════════════════════════════════════ */

static int count_instructions(llvm::Function& fn) {
    int n = 0;
    for (auto& bb : fn) n += static_cast<int>(bb.size());
    return n;
}

void run_aurora_optimizer(llvm::Module* module) {
    bool changed = true;
    int  iterations = 0;

    /* Run passes until no more changes (max 5 iterations) */
    while (changed && iterations < 5) {
        changed = false;
        iterations++;

        for (auto& fn : *module) {
            if (fn.isDeclaration()) continue;

            /* Skip very large functions (≥100K instructions) — these are
               stress-test loops that won't benefit from our simple passes */
            if (count_instructions(fn) >= 100000) continue;

            /* Constant folding */
            changed |= fold_binary_constants(fn);
            changed |= fold_comparison_constants(fn);
            changed |= fold_unary_constants(fn);
            changed |= fold_branch_constants(fn);

            /* Strength reduction */
            changed |= strength_reduce(fn);

            /* Dead code elimination */
            changed |= eliminate_dead_instructions(fn);
            changed |= eliminate_unreachable_blocks(fn);
        }
    }
}