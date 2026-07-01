#include "compiler/ir/ir_optimizer.hpp"
#include "compiler/ir/ir_mem2reg.hpp"
#include <algorithm>
#include <unordered_set>

/* ════════════════════════════════════════════════════════════
   Helpers
   ════════════════════════════════════════════════════════════ */

static bool is_pow2(int64_t v) { return v > 0 && (v & (v - 1)) == 0; }
static int log2_64(int64_t v) { int r = 0; while (v > 1) { v >>= 1; r++; } return r; }

/* Collect all SSA value names referenced in a block */
static std::unordered_set<std::string> uses_in_block(const IrBasicBlock& bb) {
    std::unordered_set<std::string> uses;
    for (const auto& inst : bb.instructions) {
        std::visit([&](const auto& i) {
            using T = std::decay_t<decltype(i)>;
            if constexpr (std::is_same_v<T, IrLoad>) {
                if (!i.ptr.is_const) uses.insert(i.ptr.name);
            } else if constexpr (std::is_same_v<T, IrStore>) {
                if (!i.ptr.is_const) uses.insert(i.ptr.name);
                if (!i.value.is_const) uses.insert(i.value.name);
            } else if constexpr (std::is_same_v<T, IrBinOpInst>) {
                if (!i.lhs.is_const) uses.insert(i.lhs.name);
                if (!i.rhs.is_const) uses.insert(i.rhs.name);
            } else if constexpr (std::is_same_v<T, IrICmp>) {
                if (!i.lhs.is_const) uses.insert(i.lhs.name);
                if (!i.rhs.is_const) uses.insert(i.rhs.name);
            } else if constexpr (std::is_same_v<T, IrCall>) {
                for (const auto& a : i.args)
                    if (!a.is_const) uses.insert(a.name);
            } else if constexpr (std::is_same_v<T, IrCondBr>) {
                if (!i.cond.is_const) uses.insert(i.cond.name);
            } else if constexpr (std::is_same_v<T, IrRet>) {
                if (!i.value.is_const) uses.insert(i.value.name);
            } else if constexpr (std::is_same_v<T, IrPhi>) {
                for (const auto& [val, blk] : i.incoming)
                    uses.insert(val);
            } else if constexpr (std::is_same_v<T, IrGEP>) {
                if (!i.ptr.is_const) uses.insert(i.ptr.name);
                for (const auto& idx : i.indices)
                    if (!idx.is_const) uses.insert(idx.name);
            } else if constexpr (std::is_same_v<T, IrBitCast>) {
                if (!i.value.is_const) uses.insert(i.value.name);
            } else if constexpr (std::is_same_v<T, IrStrLiteral>) {
                /* strlit has no SSA uses */
            }
        }, inst);
    }
    return uses;
}

/* Collect all defined SSA names in a block */
static std::unordered_set<std::string> defs_in_block(const IrBasicBlock& bb) {
    std::unordered_set<std::string> defs;
    for (const auto& inst : bb.instructions) {
        std::visit([&](const auto& i) {
            using T = std::decay_t<decltype(i)>;
            if constexpr (std::is_same_v<T, IrAlloca>)    { if (!i.result_name.empty()) defs.insert(i.result_name); }
            else if constexpr (std::is_same_v<T, IrLoad>) { if (!i.result_name.empty()) defs.insert(i.result_name); }
            else if constexpr (std::is_same_v<T, IrBinOpInst>) { if (!i.result_name.empty()) defs.insert(i.result_name); }
            else if constexpr (std::is_same_v<T, IrICmp>) { if (!i.result_name.empty()) defs.insert(i.result_name); }
            else if constexpr (std::is_same_v<T, IrCall>) { if (!i.result_name.empty()) defs.insert(i.result_name); }
            else if constexpr (std::is_same_v<T, IrPhi>)  { if (!i.result_name.empty()) defs.insert(i.result_name); }
            else if constexpr (std::is_same_v<T, IrGEP>)  { if (!i.result_name.empty()) defs.insert(i.result_name); }
            else if constexpr (std::is_same_v<T, IrBitCast>) { if (!i.result_name.empty()) defs.insert(i.result_name); }
            else if constexpr (std::is_same_v<T, IrStrLiteral>) { if (!i.result_name.empty()) defs.insert(i.result_name); }
        }, inst);
    }
    return defs;
}

/* ════════════════════════════════════════════════════════════
   Constant Folding
   ════════════════════════════════════════════════════════════ */

bool ir_fold_constants(IrFunction& fn, std::vector<IrType>& pool) {
    static_cast<void>(pool);
    bool changed = false;

    for (auto& bb : fn.blocks) {
        for (size_t i = 0; i < bb.instructions.size(); i++) {
            auto& inst = bb.instructions[i];

            /* Fold binary ops with both constants */
            if (auto* binop = std::get_if<IrBinOpInst>(&inst)) {
                if (binop->lhs.is_const && binop->rhs.is_const) {
                    uint64_t l = static_cast<uint64_t>(binop->lhs.i64()), r = static_cast<uint64_t>(binop->rhs.i64()), v = 0;
                    bool ok = true;
                    switch (binop->op) {
                        case IrBinOp::Add: v = l + r; break;
                        case IrBinOp::Sub: v = l - r; break;
                        case IrBinOp::Mul: v = l * r; break;
                        case IrBinOp::SDiv: if (r == 0) { ok = false; break; } v = static_cast<uint64_t>(static_cast<int64_t>(l) / static_cast<int64_t>(r)); break;
                        case IrBinOp::UDiv: if (r == 0) { ok = false; break; } v = l / r; break;
                        case IrBinOp::SRem: if (r == 0) { ok = false; break; } v = static_cast<uint64_t>(static_cast<int64_t>(l) % static_cast<int64_t>(r)); break;
                        case IrBinOp::URem: if (r == 0) { ok = false; break; } v = l % r; break;
                        case IrBinOp::And: v = l & r; break;
                        case IrBinOp::Or:  v = l | r; break;
                        case IrBinOp::Xor: v = l ^ r; break;
                        case IrBinOp::Shl: v = l << r; break;
                        case IrBinOp::AShr: v = static_cast<uint64_t>(static_cast<int64_t>(l) >> r); break;
                        case IrBinOp::LShr: v = l >> r; break;
                        default: ok = false; break;
                    }
                    if (ok) {
                        auto cv = ir_const_i64(static_cast<int64_t>(v));
                        cv.type_idx = binop->lhs.type_idx >= 0 ? binop->lhs.type_idx : binop->rhs.type_idx;
                        bb.instructions[i] = IrBitCast{cv, cv.type_idx, binop->result_name};
                        changed = true;
                    }
                }
            }

            /* Fold icmp with both constants */
            if (auto* icmp = std::get_if<IrICmp>(&inst)) {
                if (icmp->lhs.is_const && icmp->rhs.is_const) {
                    int64_t l = icmp->lhs.i64(), r = icmp->rhs.i64();
                    bool v = false;
                    switch (icmp->pred) {
                        case IrCmpPred::EQ: v = l == r; break;
                        case IrCmpPred::NE: v = l != r; break;
                        case IrCmpPred::SLT: v = l < r; break;
                        case IrCmpPred::SGT: v = l > r; break;
                        case IrCmpPred::SLE: v = l <= r; break;
                        case IrCmpPred::SGE: v = l >= r; break;
                        case IrCmpPred::ULT: v = static_cast<uint64_t>(l) < static_cast<uint64_t>(r); break;
                        case IrCmpPred::UGT: v = static_cast<uint64_t>(l) > static_cast<uint64_t>(r); break;
                        case IrCmpPred::ULE: v = static_cast<uint64_t>(l) <= static_cast<uint64_t>(r); break;
                        case IrCmpPred::UGE: v = static_cast<uint64_t>(l) >= static_cast<uint64_t>(r); break;
                    }
                    auto idx = ir_make_primitive(pool, IrTypeKind::Int1);
                    auto cv = ir_const_i64(v ? 1 : 0);
                    cv.type_idx = idx;
                    bb.instructions[i] = IrBitCast{cv, idx, icmp->result_name};
                    changed = true;
                }
            }

            /* Fold conditional branch with constant condition */
            if (auto* cbr = std::get_if<IrCondBr>(&inst)) {
                if (cbr->cond.is_const) {
                    const std::string& target = cbr->cond.i64() != 0 ? cbr->true_bb : cbr->false_bb;
                    bb.instructions[i] = IrBr{target};
                    changed = true;
                }
            }
        }
    }

    return changed;
}

/* ════════════════════════════════════════════════════════════
   Dead Code Elimination
   ════════════════════════════════════════════════════════════ */

static bool is_side_effecting(const IrInstruction& inst) {
    return std::visit([](const auto& i) -> bool {
        using T = std::decay_t<decltype(i)>;
        if constexpr (std::is_same_v<T, IrStore>) return true;
        if constexpr (std::is_same_v<T, IrCall>) return true;
        if constexpr (std::is_same_v<T, IrRet>) return true;
        if constexpr (std::is_same_v<T, IrBr>) return true;
        if constexpr (std::is_same_v<T, IrCondBr>) return true;
        if constexpr (std::is_same_v<T, IrStrLiteral>) return true; /* string literals have side effects (alloc) */
        static_cast<void>(i);
        return false;
    }, inst);
}

bool ir_eliminate_dead_code(IrFunction& fn, std::vector<IrType>& pool) {
    static_cast<void>(pool);
    bool changed = false;

    /* Collect all SSA uses across all blocks */
    std::unordered_set<std::string> all_uses;
    std::unordered_set<std::string> all_defs;
    for (const auto& bb : fn.blocks) {
        auto u = uses_in_block(bb);
        all_uses.insert(u.begin(), u.end());
        auto d = defs_in_block(bb);
        all_defs.insert(d.begin(), d.end());
    }

    for (auto& bb : fn.blocks) {
        auto& insts = bb.instructions;
        for (size_t i = 0; i < insts.size(); ) {
            auto& inst = insts[i];
            if (is_side_effecting(inst)) { i++; continue; }

            std::string def_name;
            std::visit([&](const auto& ii) {
                using T = std::decay_t<decltype(ii)>;
                if constexpr (std::is_same_v<T, IrAlloca>)    def_name = ii.result_name;
                else if constexpr (std::is_same_v<T, IrLoad>) def_name = ii.result_name;
                else if constexpr (std::is_same_v<T, IrBinOpInst>) def_name = ii.result_name;
                else if constexpr (std::is_same_v<T, IrICmp>) def_name = ii.result_name;
                else if constexpr (std::is_same_v<T, IrPhi>)  def_name = ii.result_name;
                else if constexpr (std::is_same_v<T, IrGEP>)  def_name = ii.result_name;
                else if constexpr (std::is_same_v<T, IrBitCast>) def_name = ii.result_name;
                else if constexpr (std::is_same_v<T, IrStrLiteral>) def_name = ii.result_name;
            }, inst);

            if (!def_name.empty() && all_uses.find(def_name) == all_uses.end()) {
                insts.erase(insts.begin() + i);
                changed = true;
            } else {
                i++;
            }
        }
    }

    /* Eliminate unreachable blocks (simple: blocks not referenced by any Br/CondBr) */
    std::unordered_set<std::string> reachable;
    reachable.insert(fn.block_order.empty() ? (fn.blocks.empty() ? "" : fn.blocks[0].name) : fn.block_order[0]);
    bool local = true;
    while (local) {
        local = false;
        for (const auto& bb : fn.blocks) {
            if (reachable.find(bb.name) == reachable.end()) continue;
            for (const auto& inst : bb.instructions) {
                if (auto* br = std::get_if<IrBr>(&inst)) {
                    if (reachable.insert(br->target).second) local = true;
                }
                if (auto* cbr = std::get_if<IrCondBr>(&inst)) {
                    if (reachable.insert(cbr->true_bb).second) local = true;
                    if (reachable.insert(cbr->false_bb).second) local = true;
                }
            }
        }
    }

    /* Remove unreachable blocks */
    for (auto it = fn.blocks.begin(); it != fn.blocks.end(); ) {
        if (reachable.find(it->name) == reachable.end()) {
            it = fn.blocks.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }

    /* Update block_order */
    std::vector<std::string> new_order;
    for (const auto& name : fn.block_order) {
        if (reachable.find(name) != reachable.end())
            new_order.push_back(name);
    }
    if (new_order.size() != fn.block_order.size()) changed = true;
    fn.block_order = std::move(new_order);

    return changed;
}

/* ════════════════════════════════════════════════════════════
   Strength Reduction
   ════════════════════════════════════════════════════════════ */

bool ir_strength_reduce(IrFunction& fn, std::vector<IrType>& pool) {
    bool changed = false;

    for (auto& bb : fn.blocks) {
        for (size_t i = 0; i < bb.instructions.size(); i++) {
            auto* binop = std::get_if<IrBinOpInst>(&bb.instructions[i]);
            if (!binop) continue;
            if (!binop->rhs.is_const) continue;

            int64_t c = binop->rhs.i64();
            IrValue lhs = binop->lhs;
            int32_t ti = binop->lhs.type_idx >= 0 ? binop->lhs.type_idx : binop->rhs.type_idx;

            auto replace_with = [&](IrBinOp op, int64_t rhs_const) {
                IrValue rc = ir_const_i64(rhs_const);
                rc.type_idx = ti;
                bb.instructions[i] = IrBinOpInst{op, lhs, rc, binop->result_name};
                changed = true;
            };

            auto replace_with_load = [&]() {
                /* Identity: forward LHS value via a bitcast to same type */
                bb.instructions[i] = IrBitCast{lhs, ti, binop->result_name};
                changed = true;
            };

            switch (binop->op) {
                case IrBinOp::Mul: {
                    if (c == 0) {
                        /* mul x, 0 → 0 */
                        auto cv = ir_const_i64(0); cv.type_idx = ti;
                        bb.instructions[i] = IrBitCast{cv, ti, binop->result_name};
                        changed = true;
                    } else if (c == 1) {
                        /* mul x, 1 → x */
                        replace_with_load();
                    } else if (is_pow2(c)) {
                        /* mul x, 2^K → shl x, K */
                        replace_with(IrBinOp::Shl, log2_64(c));
                    }
                    break;
                }
                case IrBinOp::SDiv: {
                    if (c == 1) {
                        replace_with_load();
                    }
                    break;
                }
                case IrBinOp::UDiv: {
                    if (c == 1) {
                        replace_with_load();
                    } else if (is_pow2(c)) {
                        replace_with(IrBinOp::LShr, log2_64(c));
                    }
                    break;
                }
                case IrBinOp::URem: {
                    if (is_pow2(c)) {
                        /* urem x, 2^K → x & (2^K - 1) */
                        replace_with(IrBinOp::And, c - 1);
                    }
                    break;
                }
                case IrBinOp::Add:
                case IrBinOp::Sub: {
                    if (c == 0) {
                        replace_with_load();
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    return changed;
}

/* ════════════════════════════════════════════════════════════
   Main optimizer entry point
   ════════════════════════════════════════════════════════════ */

void ir_optimize(IrModule& mod) {
    bool changed = true;
    int iterations = 0;

    /* First run mem2reg to promote allocas to SSA */
    for (auto& fn : mod.functions)
        ir_mem2reg(fn, mod.type_pool);

    /* Fixed-point loop */
    while (changed && iterations < 5) {
        changed = false;
        iterations++;
        for (auto& fn : mod.functions) {
            changed |= ir_fold_constants(fn, mod.type_pool);
            changed |= ir_strength_reduce(fn, mod.type_pool);
            changed |= ir_eliminate_dead_code(fn, mod.type_pool);
        }
    }
}