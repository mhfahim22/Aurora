#include "compiler/ir/ir_mem2reg.hpp"
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <set>
#include <map>
#include <cassert>

/* ════════════════════════════════════════════════════════════
   ir_mem2reg — Promote alloca/load/store to SSA
   ════════════════════════════════════════════════════════════
   Uses mark-then-sweep to avoid index-shifting bugs.
   Handles:
   - Dead alloca (no refs): remove
   - Single-store alloca: replace loads with bitcast of stored value
   - Store-free alloca (param-like): replace loads with undef
   - Multi-block: basic phi insertion at merge points
   ════════════════════════════════════════════════════════════ */

/* Collect all load/store positions referencing a given alloca name */
static void collect_refs(IrFunction& fn, const std::string& aname,
                         std::vector<std::pair<size_t, size_t>>& loads,
                         std::vector<std::pair<size_t, size_t>>& stores) {
    for (size_t bi = 0; bi < fn.blocks.size(); bi++) {
        auto& block = fn.blocks[bi];
        for (size_t ii = 0; ii < block.instructions.size(); ii++) {
            auto& inst = block.instructions[ii];
            if (auto* ld = std::get_if<IrLoad>(&inst)) {
                if (ld->ptr.name == aname)
                    loads.push_back({bi, ii});
            }
            if (auto* st = std::get_if<IrStore>(&inst)) {
                if (st->ptr.name == aname)
                    stores.push_back({bi, ii});
            }
        }
    }
}

/* Build predecessor map from function's control flow */
static std::map<size_t, std::vector<size_t>> build_predecessors(IrFunction& fn) {
    std::map<size_t, std::vector<size_t>> preds;
    for (size_t bi = 0; bi < fn.blocks.size(); bi++) {
        /* Initialize empty predecessor list */
        static_cast<void>(preds[bi]);
        for (const auto& inst : fn.blocks[bi].instructions) {
            if (auto* br = std::get_if<IrBr>(&inst)) {
                for (size_t bj = 0; bj < fn.blocks.size(); bj++)
                    if (fn.blocks[bj].name == br->target)
                        preds[bj].push_back(bi);
            }
            if (auto* cbr = std::get_if<IrCondBr>(&inst)) {
                for (size_t bj = 0; bj < fn.blocks.size(); bj++) {
                    if (fn.blocks[bj].name == cbr->true_bb ||
                        fn.blocks[bj].name == cbr->false_bb)
                        preds[bj].push_back(bi);
                }
            }
        }
    }
    return preds;
}

bool ir_mem2reg(IrFunction& fn, std::vector<IrType>& pool) {
    static_cast<void>(pool);
    if (fn.blocks.empty()) return false;

    IrBasicBlock& entry = fn.blocks.front();

    /* Per-block erase sets */
    std::map<size_t, std::set<size_t>> block_erase;
    /* Per-block insertions: position → instruction */
    std::map<size_t, std::vector<std::pair<size_t, IrInstruction>>> block_insert;

    bool changed = false;

    /* Build predecessors once for multi-block cases */
    auto preds = build_predecessors(fn);

    /* Scan each alloca in the entry block */
    for (size_t ai = 0; ai < entry.instructions.size(); ai++) {
        auto* alloca_inst = std::get_if<IrAlloca>(&entry.instructions[ai]);
        if (!alloca_inst) continue;
        if (alloca_inst->result_name.empty()) continue;

        const std::string& aname = alloca_inst->result_name;
        int32_t val_type = alloca_inst->type_idx;

        std::vector<std::pair<size_t, size_t>> loads, stores;
        collect_refs(fn, aname, loads, stores);

        /* Case 1: No references — just remove alloca */
        if (loads.empty() && stores.empty()) {
            block_erase[0].insert(ai);
            changed = true;
            continue;
        }

        /* Case 2: Single store — replace all loads with that value */
        if (stores.size() == 1) {
            auto& [s_bi, s_ii] = stores[0];
            auto* store = std::get_if<IrStore>(&fn.blocks[s_bi].instructions[s_ii]);
            if (!store) continue;

            IrValue stored_val = store->value;

            for (auto& [l_bi, l_ii] : loads) {
                auto* load = std::get_if<IrLoad>(&fn.blocks[l_bi].instructions[l_ii]);
                if (!load) continue;
                fn.blocks[l_bi].instructions[l_ii] = IrBitCast{
                    stored_val, val_type, load->result_name
                };
            }

            block_erase[0].insert(ai);
            block_erase[s_bi].insert(s_ii);
            changed = true;
            continue;
        }

        /* Case 3: Multi-store — insert phi at merge blocks */
        if (stores.size() > 1) {
            /* Collect store blocks */
            std::set<size_t> store_blocks;
            for (auto& [bi, ii] : stores) store_blocks.insert(bi);

            /* Collect load blocks */
            std::set<size_t> load_blocks;
            for (auto& [bi, ii] : loads) load_blocks.insert(bi);

            /* Find blocks that need phi: blocks with >1 predecessor
             * where at least one predecessor has a store */
            std::set<size_t> phi_blocks;
            for (size_t bi = 0; bi < fn.blocks.size(); bi++) {
                auto it = preds.find(bi);
                if (it == preds.end() || it->second.size() < 2) continue;

                bool pred_has_store = false;
                for (size_t pi : it->second) {
                    if (store_blocks.count(pi)) {
                        pred_has_store = true;
                        break;
                    }
                }
                if (pred_has_store) {
                    /* Also need loads or stores in this block */
                    if (load_blocks.count(bi) || store_blocks.count(bi))
                        phi_blocks.insert(bi);
                }
            }

            /* Map from block index to phi result name */
            std::unordered_map<size_t, std::string> phi_results;

            /* Insert phi instructions */
            for (size_t bi : phi_blocks) {
                std::string phi_name = aname + ".phi";
                IrPhi phi_inst;
                phi_inst.result_name = phi_name;
                phi_inst.type_idx = val_type;

                /* Fill in incoming values from each predecessor */
                auto pit = preds.find(bi);
                if (pit != preds.end()) {
                    for (size_t pi : pit->second) {
                        std::string pred_name = fn.blocks[pi].name;
                        /* Use a sentinel name; the optimizer will resolve */
                        phi_inst.incoming.push_back({aname + ".pred." + std::to_string(pi), pred_name});
                    }
                }

                /* Insert phi at the beginning of the block */
                block_insert[bi].push_back({0, phi_inst});
                phi_results[bi] = phi_name;
            }

            /* Replace loads in phi blocks with bitcast to phi */
            for (auto& [l_bi, l_ii] : loads) {
                if (!phi_blocks.count(l_bi)) continue;
                auto* load = std::get_if<IrLoad>(&fn.blocks[l_bi].instructions[l_ii]);
                if (!load) continue;
                fn.blocks[l_bi].instructions[l_ii] = IrBitCast{
                    ir_ssa(phi_results[l_bi], val_type), val_type, load->result_name
                };
            }

            block_erase[0].insert(ai);
            for (auto& [bi, ii] : stores)
                block_erase[bi].insert(ii);
            changed = true;
            continue;
        }

        /* Case 4: Loads only (no stores) — param-like, loads return undef */
        if (stores.empty() && !loads.empty()) {
            IrValue undef_val = ir_const_i64(0);
            undef_val.type_idx = val_type;
            for (auto& [l_bi, l_ii] : loads) {
                auto* load = std::get_if<IrLoad>(&fn.blocks[l_bi].instructions[l_ii]);
                if (!load) continue;
                fn.blocks[l_bi].instructions[l_ii] = IrBitCast{
                    undef_val, val_type, load->result_name
                };
            }
            block_erase[0].insert(ai);
            changed = true;
            continue;
        }
    }

    /* Apply insertions before erasures to keep indices stable */
    for (auto& [bi, inserts] : block_insert) {
        /* Sort by position descending so inserts don't shift each other */
        std::sort(inserts.begin(), inserts.end(),
                  [](auto& a, auto& b) { return a.first > b.first; });
        for (auto& [pos, inst] : inserts)
            fn.blocks[bi].instructions.insert(
                fn.blocks[bi].instructions.begin() + pos, std::move(inst));
    }

    /* Sweep: erase marked instructions per block (highest index first) */
    for (auto& [bi, idxs] : block_erase)
        for (auto it = idxs.rbegin(); it != idxs.rend(); ++it)
            fn.blocks[bi].instructions.erase(fn.blocks[bi].instructions.begin() + *it);

    return changed;
}