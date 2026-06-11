/* ════════════════════════════════════════════════════════════
   Aurora Optimizer — Strength Reduction
   ════════════════════════════════════════════════════════════
   Replace expensive operations with cheaper ones:
     - mul by power-of-2  → shl
     - sdiv by power-of-2 → ashr (with signed correction)
     - udiv by power-of-2 → lshr
     - urem by power-of-2 → and
     - mul by small constant → shift+add sequence
   ════════════════════════════════════════════════════════════ */

#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Operator.h>
#include <llvm/ADT/SmallVector.h>

/* ── Helper: is power of two? ── */
static bool is_power_of_two(uint64_t v) {
    return v != 0 && (v & (v - 1)) == 0;
}

/* ── Helper: log2 of power-of-two ── */
static unsigned log2_64(uint64_t v) {
    unsigned r = 0;
    while (v > 1) { v >>= 1; r++; }
    return r;
}

/* ── Reduce mul by constant ── */
static bool reduce_mul(llvm::BinaryOperator* binop) {
    auto* rhs = llvm::dyn_cast<llvm::ConstantInt>(binop->getOperand(1));
    if (!rhs) return false;

    uint64_t c = rhs->getZExtValue();

    /* mul x, 0 → 0 */
    if (c == 0) {
        auto* zero = llvm::ConstantInt::get(binop->getType(), 0);
        binop->replaceAllUsesWith(zero);
        binop->eraseFromParent();
        return true;
    }

    /* mul x, 1 → x */
    if (c == 1) {
        binop->replaceAllUsesWith(binop->getOperand(0));
        binop->eraseFromParent();
        return true;
    }

    /* mul x, 2^K → shl x, K */
    if (is_power_of_two(c)) {
        unsigned shift = log2_64(c);
        auto* shl = llvm::BinaryOperator::Create(
            llvm::Instruction::Shl,
            binop->getOperand(0),
            llvm::ConstantInt::get(binop->getType(), shift),
            "str_shl", binop);
        shl->setHasNoUnsignedWrap(binop->hasNoUnsignedWrap());
        shl->setHasNoSignedWrap(binop->hasNoSignedWrap());
        binop->replaceAllUsesWith(shl);
        binop->eraseFromParent();
        return true;
    }

    /* mul x, 3 → (x << 1) + x */
    /* mul x, 5 → (x << 2) + x */
    /* mul x, 9 → (x << 3) + x */
    if (c == 3 || c == 5 || c == 9 || c == 17 || c == 33) {
        unsigned shift = c == 3 ? 1 : (c == 5 ? 2 : (c == 9 ? 3 : (c == 17 ? 4 : 5)));
        auto* shl = llvm::BinaryOperator::Create(
            llvm::Instruction::Shl,
            binop->getOperand(0),
            llvm::ConstantInt::get(binop->getType(), shift),
            "str_mul_shl", binop);
        auto* add = llvm::BinaryOperator::Create(
            llvm::Instruction::Add,
            shl, binop->getOperand(0), "str_mul_add", binop);
        add->setHasNoUnsignedWrap(binop->hasNoUnsignedWrap());
        add->setHasNoSignedWrap(binop->hasNoSignedWrap());
        binop->replaceAllUsesWith(add);
        binop->eraseFromParent();
        return true;
    }

    return false;
}

/* ── Reduce sdiv by power-of-2 ── */
static bool reduce_sdiv(llvm::BinaryOperator* binop) {
    auto* rhs = llvm::dyn_cast<llvm::ConstantInt>(binop->getOperand(1));
    if (!rhs) return false;

    int64_t c = rhs->getSExtValue();

    /* sdiv x, 1 → x */
    if (c == 1) {
        binop->replaceAllUsesWith(binop->getOperand(0));
        binop->eraseFromParent();
        return true;
    }

    /* sdiv x, -1 → -x */
    if (c == -1) {
        auto* neg = llvm::BinaryOperator::CreateNeg(
            binop->getOperand(0), "str_neg", binop);
        binop->replaceAllUsesWith(neg);
        binop->eraseFromParent();
        return true;
    }

    /* sdiv x, 2^K → ashr x, K (with signed correction) */
    uint64_t abs_c = c < 0 ? static_cast<uint64_t>(-c) : c;
    if (is_power_of_two(abs_c)) {
        unsigned shift = c < 0 ? log2_64(-c) : log2_64(c);
        llvm::Type* ty = binop->getType();

        /* Signed division by power-of-2:
           result = (x + ((x >> 63) >> (64-shift))) >> shift
           = (x + (sign_bits >> (64-shift))) >> ashift */
        auto* sign = llvm::BinaryOperator::Create(
            llvm::Instruction::AShr,
            binop->getOperand(0),
            llvm::ConstantInt::get(ty, 63),
            "str_sdiv_sign", binop);
        auto* corr = llvm::BinaryOperator::Create(
            llvm::Instruction::LShr,
            sign,
            llvm::ConstantInt::get(ty, 64 - shift),
            "str_sdiv_corr", binop);
        auto* add = llvm::BinaryOperator::Create(
            llvm::Instruction::Add,
            binop->getOperand(0), corr,
            "str_sdiv_add", binop);
        auto* ashr = llvm::BinaryOperator::Create(
            llvm::Instruction::AShr,
            add,
            llvm::ConstantInt::get(ty, shift),
            "str_sdiv_ashr", binop);

        /* Handle negative divisor: negate result */
        if (c < 0) {
            auto* neg = llvm::BinaryOperator::CreateNeg(
                ashr, "str_sdiv_neg", binop);
            binop->replaceAllUsesWith(neg);
        } else {
            binop->replaceAllUsesWith(ashr);
        }
        binop->eraseFromParent();
        return true;
    }

    return false;
}

/* ── Reduce udiv by power-of-2 → lshr ── */
static bool reduce_udiv(llvm::BinaryOperator* binop) {
    auto* rhs = llvm::dyn_cast<llvm::ConstantInt>(binop->getOperand(1));
    if (!rhs) return false;

    uint64_t c = rhs->getZExtValue();

    /* udiv x, 1 → x */
    if (c == 1) {
        binop->replaceAllUsesWith(binop->getOperand(0));
        binop->eraseFromParent();
        return true;
    }

    /* udiv x, 2^K → lshr x, K */
    if (is_power_of_two(c)) {
        unsigned shift = log2_64(c);
        auto* lshr = llvm::BinaryOperator::Create(
            llvm::Instruction::LShr,
            binop->getOperand(0),
            llvm::ConstantInt::get(binop->getType(), shift),
            "str_udiv", binop);
        binop->replaceAllUsesWith(lshr);
        binop->eraseFromParent();
        return true;
    }

    return false;
}

/* ── Reduce urem by power-of-2 → and ── */
static bool reduce_urem(llvm::BinaryOperator* binop) {
    auto* rhs = llvm::dyn_cast<llvm::ConstantInt>(binop->getOperand(1));
    if (!rhs) return false;

    uint64_t c = rhs->getZExtValue();
    if (!is_power_of_two(c)) return false;

    /* urem x, 2^K → x & (2^K - 1) */
    uint64_t mask = c - 1;
    auto* and_inst = llvm::BinaryOperator::Create(
        llvm::Instruction::And,
        binop->getOperand(0),
        llvm::ConstantInt::get(binop->getType(), mask),
        "str_urem", binop);
    binop->replaceAllUsesWith(and_inst);
    binop->eraseFromParent();
    return true;
}

/* ── Add/Sub identity: add/sub 0 → identity ── */
static bool reduce_add_sub(llvm::BinaryOperator* binop) {
    auto* rhs = llvm::dyn_cast<llvm::ConstantInt>(binop->getOperand(1));
    if (!rhs) return false;

    uint64_t c = rhs->getZExtValue();
    if (c == 0) {
        binop->replaceAllUsesWith(binop->getOperand(0));
        binop->eraseFromParent();
        return true;
    }
    return false;
}

/* ── Entry point ── */
bool strength_reduce(llvm::Function& fn) {
    bool changed = false;

    for (auto& bb : fn) {
        llvm::SmallVector<llvm::BinaryOperator*, 16> to_process;

        for (auto& inst : bb) {
            if (auto* binop = llvm::dyn_cast<llvm::BinaryOperator>(&inst)) {
                to_process.push_back(binop);
            }
        }

        for (auto* binop : to_process) {
            switch (binop->getOpcode()) {
                case llvm::Instruction::Mul:
                    changed |= reduce_mul(binop);
                    break;
                case llvm::Instruction::SDiv:
                    changed |= reduce_sdiv(binop);
                    break;
                case llvm::Instruction::UDiv:
                    changed |= reduce_udiv(binop);
                    break;
                case llvm::Instruction::URem:
                    changed |= reduce_urem(binop);
                    break;
                case llvm::Instruction::Add:
                case llvm::Instruction::Sub:
                    changed |= reduce_add_sub(binop);
                    break;
                default:
                    break;
            }
        }
    }

    return changed;
}
