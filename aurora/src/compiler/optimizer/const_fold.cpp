/* ════════════════════════════════════════════════════════════
   Aurora Optimizer — Constant Folding
   ════════════════════════════════════════════════════════════
   Evaluate constant expressions at compile time:
     - Arithmetic: add, sub, mul, div, rem, and, or, xor
     - Comparison: eq, ne, slt, sgt, sle, sge
     - Unary: neg, not
     - Branch: simplify constant条件跳转
   ════════════════════════════════════════════════════════════ */

#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Operator.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/ADT/SmallVector.h>

bool fold_binary_constants(llvm::Function& fn) {
    bool changed = false;

    for (auto& bb : fn) {
        llvm::SmallVector<llvm::Instruction*, 16> to_remove;

        for (auto& inst : bb) {
            /* Fold binary ops on constants */
            if (auto* binop = llvm::dyn_cast<llvm::BinaryOperator>(&inst)) {
                auto* lhs = llvm::dyn_cast<llvm::ConstantInt>(binop->getOperand(0));
                auto* rhs = llvm::dyn_cast<llvm::ConstantInt>(binop->getOperand(1));
                if (!lhs || !rhs) continue;

                llvm::Constant* result = nullptr;
                switch (binop->getOpcode()) {
                    case llvm::Instruction::Add:
                        result = llvm::ConstantInt::get(binop->getType(),
                            lhs->getValue() + rhs->getValue());
                        break;
                    case llvm::Instruction::Sub:
                        result = llvm::ConstantInt::get(binop->getType(),
                            lhs->getValue() - rhs->getValue());
                        break;
                    case llvm::Instruction::Mul:
                        result = llvm::ConstantInt::get(binop->getType(),
                            lhs->getValue() * rhs->getValue());
                        break;
                    case llvm::Instruction::SDiv:
                        if (rhs->getValue() == 0) continue;
                        result = llvm::ConstantInt::get(binop->getType(),
                            lhs->getValue().sdiv(rhs->getValue()));
                        break;
                    case llvm::Instruction::SRem:
                        if (rhs->getValue() == 0) continue;
                        result = llvm::ConstantInt::get(binop->getType(),
                            lhs->getValue().srem(rhs->getValue()));
                        break;
                    case llvm::Instruction::And:
                        result = llvm::ConstantInt::get(binop->getType(),
                            lhs->getValue() & rhs->getValue());
                        break;
                    case llvm::Instruction::Or:
                        result = llvm::ConstantInt::get(binop->getType(),
                            lhs->getValue() | rhs->getValue());
                        break;
                    case llvm::Instruction::Xor:
                        result = llvm::ConstantInt::get(binop->getType(),
                            lhs->getValue() ^ rhs->getValue());
                        break;
                    default: continue;
                }
                if (result) {
                    binop->replaceAllUsesWith(result);
                    to_remove.push_back(binop);
                    changed = true;
                }
            }
        }

        for (auto* inst : to_remove)
            inst->eraseFromParent();
    }

    return changed;
}

bool fold_comparison_constants(llvm::Function& fn) {
    bool changed = false;

    for (auto& bb : fn) {
        llvm::SmallVector<llvm::Instruction*, 16> to_remove;

        for (auto& inst : bb) {
            /* Fold icmp on constants */
            if (auto* icmp = llvm::dyn_cast<llvm::ICmpInst>(&inst)) {
                auto* lhs = llvm::dyn_cast<llvm::ConstantInt>(icmp->getOperand(0));
                auto* rhs = llvm::dyn_cast<llvm::ConstantInt>(icmp->getOperand(1));
                if (!lhs || !rhs) continue;

                bool result = false;
                switch (icmp->getPredicate()) {
                    case llvm::ICmpInst::ICMP_EQ:  result = lhs->getValue() == rhs->getValue(); break;
                    case llvm::ICmpInst::ICMP_NE:  result = lhs->getValue() != rhs->getValue(); break;
                    case llvm::ICmpInst::ICMP_SLT: result = lhs->getValue().slt(rhs->getValue()); break;
                    case llvm::ICmpInst::ICMP_SGT: result = lhs->getValue().sgt(rhs->getValue()); break;
                    case llvm::ICmpInst::ICMP_SLE: result = lhs->getValue().sle(rhs->getValue()); break;
                    case llvm::ICmpInst::ICMP_SGE: result = lhs->getValue().sge(rhs->getValue()); break;
                    case llvm::ICmpInst::ICMP_ULT: result = lhs->getValue().ult(rhs->getValue()); break;
                    case llvm::ICmpInst::ICMP_UGT: result = lhs->getValue().ugt(rhs->getValue()); break;
                    case llvm::ICmpInst::ICMP_ULE: result = lhs->getValue().ule(rhs->getValue()); break;
                    case llvm::ICmpInst::ICMP_UGE: result = lhs->getValue().uge(rhs->getValue()); break;
                    default: continue;
                }
                auto* replacement = llvm::ConstantInt::get(
                    llvm::Type::getInt1Ty(fn.getContext()), result);
                icmp->replaceAllUsesWith(replacement);
                to_remove.push_back(icmp);
                changed = true;
            }
        }

        for (auto* inst : to_remove)
            inst->eraseFromParent();
    }

    return changed;
}

bool fold_unary_constants(llvm::Function& fn) {
    bool changed = false;

    for (auto& bb : fn) {
        llvm::SmallVector<llvm::Instruction*, 16> to_remove;

        for (auto& inst : bb) {
            /* Fold sub 0, x → neg x */
            if (auto* sub = llvm::dyn_cast<llvm::BinaryOperator>(&inst)) {
                if (sub->getOpcode() == llvm::Instruction::Sub) {
                    auto* lhs = llvm::dyn_cast<llvm::ConstantInt>(sub->getOperand(0));
                    if (lhs && lhs->getValue() == 0) {
                        auto* rhs = sub->getOperand(1);
                        if (rhs->getType()->isIntegerTy(64)) {
                            auto* neg = llvm::BinaryOperator::CreateNeg(rhs, "neg_fold", &inst);
                            sub->replaceAllUsesWith(neg);
                            to_remove.push_back(sub);
                            changed = true;
                        }
                    }
                }
            }

            /* Fold not(or(x, -1)) → not(x) patterns */
            /* Fold not(not(x)) → x */
            if (auto* binop = llvm::dyn_cast<llvm::BinaryOperator>(&inst)) {
                if (binop->getOpcode() == llvm::Instruction::Xor) {
                    auto* rhs = llvm::dyn_cast<llvm::ConstantInt>(binop->getOperand(1));
                    if (rhs && rhs->getValue().isAllOnes()) {
                        /* xor x, -1 = not x */
                        auto* not_op = llvm::BinaryOperator::CreateNot(
                            binop->getOperand(0), "not_fold", &inst);
                        binop->replaceAllUsesWith(not_op);
                        to_remove.push_back(binop);
                        changed = true;
                    }
                }
            }
        }

        for (auto* inst : to_remove)
            inst->eraseFromParent();
    }

    return changed;
}

bool fold_branch_constants(llvm::Function& fn) {
    bool changed = false;

    for (auto& bb : fn) {
        auto* br = llvm::dyn_cast<llvm::BranchInst>(bb.getTerminator());
        if (!br || !br->isConditional()) continue;

        auto* cond = llvm::dyn_cast<llvm::ConstantInt>(br->getCondition());
        if (!cond) continue;

        /* Replace with unconditional branch */
        llvm::BasicBlock* target = cond->getValue() != 0
            ? br->getSuccessor(0)
            : br->getSuccessor(1);

        llvm::ReplaceInstWithInst(br, llvm::BranchInst::Create(target));
        changed = true;
    }

    return changed;
}
