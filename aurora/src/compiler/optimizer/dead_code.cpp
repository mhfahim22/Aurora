/* ════════════════════════════════════════════════════════════
   Aurora Optimizer — Dead Code Elimination
   ════════════════════════════════════════════════════════════
   Remove dead code:
     - Unused arithmetic/comparison instructions
     - Unreachable basic blocks
   ════════════════════════════════════════════════════════════ */

#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/CFG.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

/* ── Collect predecessors using simple scan ── */
static bool has_predecessors(llvm::BasicBlock* bb) {
    for (auto* pred : llvm::predecessors(bb))
        return true;
    return false;
}

bool eliminate_dead_instructions(llvm::Function& fn) {
    bool changed = false;

    for (auto& bb : fn) {
        llvm::SmallVector<llvm::Instruction*, 16> to_remove;

        for (auto& inst : bb) {
            /* Skip side-effecting instructions */
            if (llvm::isa<llvm::StoreInst>(inst)) continue;
            if (llvm::isa<llvm::CallInst>(inst)) continue;
            if (llvm::isa<llvm::BranchInst>(inst)) continue;
            if (llvm::isa<llvm::ReturnInst>(inst)) continue;
            if (llvm::isa<llvm::AllocaInst>(inst)) continue;
            if (inst.getType()->isVoidTy()) continue;

            /* If no uses, it's dead */
            if (inst.use_empty()) {
                to_remove.push_back(&inst);
                changed = true;
            }
        }

        for (auto* inst : to_remove)
            inst->eraseFromParent();
    }

    return changed;
}

bool eliminate_unreachable_blocks(llvm::Function& fn) {
    bool changed = false;
    bool local = true;

    /* Iterate until fixpoint — deleting a block may make more blocks unreachable */
    while (local) {
        local = false;
        llvm::SmallVector<llvm::BasicBlock*, 16> dead_blocks;

        for (auto& bb : fn) {
            /* Entry block is always reachable */
            if (&bb == &fn.getEntryBlock()) continue;

            /* Check if any predecessor still exists */
            if (!has_predecessors(&bb)) {
                dead_blocks.push_back(&bb);
            }
        }

        if (!dead_blocks.empty()) {
            changed = true;
            local = true;
            for (auto* bb : dead_blocks) {
                llvm::DeleteDeadBlock(bb);
            }
        }
    }

    return changed;
}
