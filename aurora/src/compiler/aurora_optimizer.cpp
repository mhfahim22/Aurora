#include "compiler/aurora_optimizer.hpp"

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Operator.h>
#include <unordered_map>
#include <cstring>

/* ════════════════════════════════════════════════════════════
   Aurora Optimizer — Phase 28 Enhanced Version
   ════════════════════════════════════════════════════════════
   Runs all optimization passes in a fixed-point loop.
   Passes: constant folding, strength reduction, DCE, CSE,
           load/store forwarding, algebraic simplification,
           unreachable block elimination.
   ════════════════════════════════════════════════════════════ */

static int count_instructions(llvm::Function& fn) {
    int n = 0;
    for (auto& bb : fn) n += static_cast<int>(bb.size());
    return n;
}

/* ── Simple CSE (common subexpression elimination) ── */
static bool cse_pass(llvm::Function& fn) {
    bool changed = false;
    for (auto& bb : fn) {
        std::unordered_map<unsigned, llvm::Value*> seen;
        auto it = bb.begin();
        while (it != bb.end()) {
            llvm::Instruction& inst = *it;
            ++it; /* advance before potential erase */
            unsigned opcode = inst.getOpcode();
            if (opcode == llvm::Instruction::Add ||
                opcode == llvm::Instruction::Sub ||
                opcode == llvm::Instruction::Mul ||
                opcode == llvm::Instruction::SDiv ||
                opcode == llvm::Instruction::UDiv ||
                opcode == llvm::Instruction::And ||
                opcode == llvm::Instruction::Or  ||
                opcode == llvm::Instruction::Xor ||
                opcode == llvm::Instruction::ICmp) {
                unsigned hash = inst.getOpcode() ^
                    (inst.getOperand(0) ? (unsigned)(uintptr_t)inst.getOperand(0) : 0) ^
                    (inst.getOperand(1) ? (unsigned)(uintptr_t)inst.getOperand(1) : 0);
                auto it2 = seen.find(hash);
                if (it2 != seen.end()) {
                    inst.replaceAllUsesWith(it2->second);
                    inst.eraseFromParent();
                    changed = true;
                    continue;
                }
                seen[hash] = &inst;
            }
        }
    }
    return changed;
}

/* ── Load/store forwarding (simple).  If a store is immediately
      followed by a load from the same address, forward the value. ── */
static bool load_store_forwarding(llvm::Function& fn) {
    bool changed = false;
    for (auto& bb : fn) {
        for (auto it = bb.begin(); it != bb.end(); ++it) {
            auto* store = llvm::dyn_cast<llvm::StoreInst>(&*it);
            if (!store) continue;
            auto next = std::next(it);
            if (next == bb.end()) continue;
            auto* load = llvm::dyn_cast<llvm::LoadInst>(&*next);
            if (!load) continue;
            if (load->getPointerOperand() == store->getPointerOperand()) {
                load->replaceAllUsesWith(store->getValueOperand());
                load->eraseFromParent();
                changed = true;
            }
        }
    }
    return changed;
}

/* ── Algebraic simplification extensions ── */
static bool algebraic_simplify(llvm::Function& fn) {
    bool changed = false;
    for (auto& bb : fn) {
        auto it = bb.begin();
        while (it != bb.end()) {
            llvm::Instruction& inst = *it;
            ++it;

            /* (x + 0) -> x, (x - 0) -> x */
            if (auto* binop = llvm::dyn_cast<llvm::BinaryOperator>(&inst)) {
                auto* lhs = binop->getOperand(0);
                auto* rhs = binop->getOperand(1);
                auto* clhs = llvm::dyn_cast<llvm::ConstantInt>(lhs);
                auto* crhs = llvm::dyn_cast<llvm::ConstantInt>(rhs);

                switch (binop->getOpcode()) {
                case llvm::Instruction::Add:
                    if (crhs && crhs->isZero()) {
                        binop->replaceAllUsesWith(lhs); goto rem;
                    }
                    if (clhs && clhs->isZero()) {
                        if (binop->isCommutative()) { binop->replaceAllUsesWith(rhs); goto rem; }
                    }
                    break;
                case llvm::Instruction::Sub:
                    if (crhs && crhs->isZero()) { binop->replaceAllUsesWith(lhs); goto rem; }
                    break;
                case llvm::Instruction::Or:
                    if (crhs && crhs->isZero()) { binop->replaceAllUsesWith(lhs); goto rem; }
                    if (clhs && clhs->isZero() && binop->isCommutative()) { binop->replaceAllUsesWith(rhs); goto rem; }
                    break;
                case llvm::Instruction::And:
                    if (crhs && crhs->isAllOnesValue()) { binop->replaceAllUsesWith(lhs); goto rem; }
                    if (clhs && clhs->isAllOnesValue() && binop->isCommutative()) { binop->replaceAllUsesWith(rhs); goto rem; }
                    break;
                default: break;
                }
            }
            continue;
        rem:
            inst.eraseFromParent();
            changed = true;
        }
    }
    return changed;
}

void run_aurora_optimizer(llvm::Module* module) {
    bool changed = true;
    int  iterations = 0;
    const int MAX_ITER = 10; /* increased from 5 for Phase 28 */

    while (changed && iterations < MAX_ITER) {
        changed = false;
        iterations++;

        for (auto& fn : *module) {
            if (fn.isDeclaration()) continue;
            if (count_instructions(fn) >= 100000) continue;

            /* Phase 27 (+) passes */
            changed |= fold_binary_constants(fn);
            changed |= fold_comparison_constants(fn);
            changed |= fold_unary_constants(fn);
            changed |= fold_branch_constants(fn);
            changed |= strength_reduce(fn);

            /* Phase 28 — New passes */
            changed |= algebraic_simplify(fn);
            changed |= cse_pass(fn);
            changed |= load_store_forwarding(fn);

            /* Phase 27 (always last) */
            changed |= eliminate_dead_instructions(fn);
            changed |= eliminate_unreachable_blocks(fn);
        }
    }
}
