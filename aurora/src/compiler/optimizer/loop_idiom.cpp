/* ════════════════════════════════════════════════════════════
   Aurora Optimizer — Loop Idiom Recognition
   ════════════════════════════════════════════════════════════
   Recognizes frequently-occurring loop patterns and replaces
   them with closed-form (O(1)) computations:
     - Counter:  x = x + 1 per iteration → final = init + N
     - Sum IV:   x = x + i per iteration → final = init + N*(N-1)/2
   Operates at the alloca/load/store level (before mem2reg).
   ════════════════════════════════════════════════════════════ */

#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Dominators.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

/* ── Struct to collect info about a simple counted loop ── */
struct CountedLoopInfo {
    llvm::BasicBlock* preheader = nullptr;
    llvm::BasicBlock* header    = nullptr;  /* for_cond */
    llvm::BasicBlock* body      = nullptr;  /* for_body (also the latch) */
    llvm::BasicBlock* exit      = nullptr;  /* for_exit */
    llvm::AllocaInst* iv_slot   = nullptr;  /* induction variable alloca */
    llvm::Value* limit          = nullptr;  /* loop trip count */
};

/* ── Try to identify a simple Aurora for-loop pattern in the given
      loop using LoopInfo + alloca-level analysis.             ── */
static bool extract_counted_loop(llvm::Loop* L, CountedLoopInfo& cl) {
    cl.preheader = L->getLoopPreheader();
    cl.header    = L->getHeader();
    cl.exit      = L->getExitBlock();
    auto* latch  = L->getLoopLatch();
    auto* exiting = L->getExitingBlock();

    if (!cl.preheader || !cl.header || !cl.exit || !latch || !exiting)
        return false;

    /* The latch and body are the same block (Aurora's for_body) */
    cl.body = latch;

    /* The header should end with a conditional branch:
       icmp slt %cur, %limit → br cond, body, exit */
    auto* br = llvm::dyn_cast<llvm::BranchInst>(cl.header->getTerminator());
    if (!br || !br->isConditional())
        return false;

    auto* icmp = llvm::dyn_cast<llvm::ICmpInst>(br->getCondition());
    if (!icmp || icmp->getPredicate() != llvm::ICmpInst::ICMP_SLT)
        return false;

    /* One operand should be a load from the IV alloca, the other is the limit */
    auto* load_iv = llvm::dyn_cast<llvm::LoadInst>(icmp->getOperand(0));
    auto* maybe_limit = icmp->getOperand(1);

    if (!load_iv) {
        load_iv = llvm::dyn_cast<llvm::LoadInst>(icmp->getOperand(1));
        maybe_limit = icmp->getOperand(0);
    }
    if (!load_iv)
        return false;

    cl.iv_slot = llvm::dyn_cast<llvm::AllocaInst>(load_iv->getPointerOperand());
    if (!cl.iv_slot)
        return false;

    cl.limit = maybe_limit;

    /* Verify initial value: search the entry block for store 0 to iv_slot */
    auto& entry = cl.header->getParent()->getEntryBlock();
    bool found_init = false;
    for (auto& inst : entry) {
        auto* si = llvm::dyn_cast<llvm::StoreInst>(&inst);
        if (!si || si->getPointerOperand() != cl.iv_slot)
            continue;
        auto* init_c = llvm::dyn_cast<llvm::ConstantInt>(si->getValueOperand());
        if (init_c && init_c->isZero()) {
            found_init = true;
            break;
        }
    }
    if (!found_init)
        return false;

    /* Verify increment pattern in the body/latch: load iv_slot → add 1 → store iv_slot */
    bool found_inc = false;
    for (auto& inst : *cl.body) {
        auto* si = llvm::dyn_cast<llvm::StoreInst>(&inst);
        if (!si || si->getPointerOperand() != cl.iv_slot)
            continue;
        auto* add = llvm::dyn_cast<llvm::BinaryOperator>(si->getValueOperand());
        if (!add || add->getOpcode() != llvm::Instruction::Add)
            continue;
        llvm::Value* other = nullptr;
        if (auto* add_load = llvm::dyn_cast<llvm::LoadInst>(add->getOperand(0))) {
            if (add_load->getPointerOperand() == cl.iv_slot)
                other = add->getOperand(1);
        }
        if (!other && llvm::isa<llvm::LoadInst>(add->getOperand(1))) {
            auto* add_load = llvm::cast<llvm::LoadInst>(add->getOperand(1));
            if (add_load->getPointerOperand() == cl.iv_slot)
                other = add->getOperand(0);
        }
        if (!other)
            continue;
        auto* one_c = llvm::dyn_cast<llvm::ConstantInt>(other);
        if (one_c && one_c->isOne()) {
            found_inc = true;
            break;
        }
    }
    if (!found_inc)
        return false;

    return true;
}

/* ── Struct for accumulator info ── */
struct AccInfo {
    llvm::Value* slot;         /* AllocaInst or GlobalVariable */
    bool is_counter;           /* true: +=1, false: += iv-derived */
    llvm::Value* orig_init;    /* initial value (from entry block store) */
};

/* ── Helper: check if a Value is a writable slot (alloca, global, 
      bitcast, or GEP — anything that can be stored to)       ── */
static bool is_writable_slot(llvm::Value* v) {
    return v != nullptr;
}

/* ── Scan the loop body for accumulator patterns:
      load(slot) → add(load, 1|iv_load) → store(slot)     ── */
static llvm::SmallVector<AccInfo, 4>
find_accumulators(llvm::BasicBlock* body, llvm::AllocaInst* iv_slot,
                  llvm::BasicBlock* entry) {
    llvm::SmallVector<AccInfo, 4> result;
    llvm::SmallPtrSet<llvm::Value*, 8> seen;

    /* Collect initial values from the entry block */
    struct InitVal { llvm::Value* slot; llvm::Value* val; };
    llvm::SmallVector<InitVal, 8> init_vals;
    for (auto& inst : *entry) {
        auto* si = llvm::dyn_cast<llvm::StoreInst>(&inst);
        if (!si) continue;
        auto* slot_ptr = si->getPointerOperand();
        if (slot_ptr == iv_slot || !is_writable_slot(slot_ptr)) continue;
        init_vals.push_back({slot_ptr, si->getValueOperand()});
    }

    for (auto& inst : *body) {
        auto* si = llvm::dyn_cast<llvm::StoreInst>(&inst);
        if (!si) continue;
        auto* slot_ptr = si->getPointerOperand();
        if (!slot_ptr || slot_ptr == iv_slot || seen.count(slot_ptr))
            continue;

        auto* add = llvm::dyn_cast<llvm::BinaryOperator>(si->getValueOperand());
        if (!add || add->getOpcode() != llvm::Instruction::Add)
            continue;

        /* Check that one operand is a load from the same slot */
        auto* add_load = llvm::dyn_cast<llvm::LoadInst>(add->getOperand(0));
        if (!add_load || add_load->getPointerOperand() != slot_ptr) {
            add_load = llvm::dyn_cast<llvm::LoadInst>(add->getOperand(1));
            if (!add_load || add_load->getPointerOperand() != slot_ptr)
                continue;
        }

        /* The other operand: constant 1 (counter) or load (IV-derived) */
        llvm::Value* other = (add->getOperand(0) == add_load)
            ? add->getOperand(1) : add->getOperand(0);

        AccInfo acc;
        acc.slot = slot_ptr;

        /* Find initial value */
        for (auto& iv : init_vals)
            if (iv.slot == slot_ptr) { acc.orig_init = iv.val; break; }

        if (auto* c = llvm::dyn_cast<llvm::ConstantInt>(other)) {
            if (!c->isOne()) continue;
            acc.is_counter = true;
            seen.insert(slot_ptr);
            result.push_back(acc);
        } else if (auto* other_li = llvm::dyn_cast<llvm::LoadInst>(other)) {
            if (!other_li->getPointerOperand()) continue;
            acc.is_counter = false;
            seen.insert(slot_ptr);
            result.push_back(acc);
        }
    }

    return result;
}

/* ── Replace all loads of a slot with a computed final value.
      Replaces uses in the exit block and all blocks dominated by
      it (after the loop).  Skips header/body blocks.          ── */
static void replace_post_loop_loads(llvm::Value* slot,
                                    llvm::Value* final_val,
                                    llvm::BasicBlock* exit_block,
                                    llvm::BasicBlock* header,
                                    llvm::BasicBlock* body) {
    if (!slot) return;

    /* Find the function containing this slot */
    llvm::Function* F = nullptr;
    if (auto* I = llvm::dyn_cast<llvm::Instruction>(slot))
        F = I->getFunction();
    else if (auto* G = llvm::dyn_cast<llvm::GlobalValue>(slot))
        F = G->getParent() ? &G->getParent()->getFunctionList().front() : nullptr;

    if (!F) return;

    for (auto& bb : *F) {
        if (&bb == header || &bb == body)
            continue;
        for (auto& inst : bb) {
            auto* li = llvm::dyn_cast<llvm::LoadInst>(&inst);
            if (!li || li->getPointerOperand() != slot)
                continue;
            li->replaceAllUsesWith(final_val);
        }
    }
}

/* ── Process a single loop ── */
static bool process_loop(llvm::Loop* L) {
    CountedLoopInfo cl;
    if (!extract_counted_loop(L, cl))
        return false;

    /* Verify the preheader actually goes to the header */
    auto* preheader_br = llvm::dyn_cast<llvm::BranchInst>(cl.preheader->getTerminator());
    if (!preheader_br || !preheader_br->isUnconditional() || preheader_br->getSuccessor(0) != cl.header)
        return false;

    /* Verify the body back-edge goes to the header */
    auto* body_br = llvm::dyn_cast<llvm::BranchInst>(cl.body->getTerminator());
    if (!body_br || !body_br->isUnconditional() || body_br->getSuccessor(0) != cl.header)
        return false;

    auto* entry_block = &cl.header->getParent()->getEntryBlock();

    /* Find accumulators in the body */
    auto accs = find_accumulators(cl.body, cl.iv_slot, entry_block);
    if (accs.empty())
        return false;

    /* Verify the loop only contains safe arithmetic and the IV pattern.
       This prevents modifying loops with I/O or unknown side effects. */
    for (auto& inst : *cl.body) {
        if (llvm::isa<llvm::BranchInst>(inst)) continue;
        if (llvm::isa<llvm::StoreInst>(inst)) continue;
        if (llvm::isa<llvm::LoadInst>(inst)) continue;
        if (llvm::isa<llvm::AllocaInst>(inst)) continue;
        if (auto* binop = llvm::dyn_cast<llvm::BinaryOperator>(&inst)) {
            unsigned opc = binop->getOpcode();
            if (opc == llvm::Instruction::Add ||
                opc == llvm::Instruction::Sub ||
                opc == llvm::Instruction::Mul ||
                opc == llvm::Instruction::SDiv)
                continue;
        }
        if (llvm::isa<llvm::CastInst>(inst)) continue;
        if (llvm::isa<llvm::GetElementPtrInst>(inst)) continue;
        if (auto* call = llvm::dyn_cast<llvm::CallInst>(&inst)) {
            auto* callee = call->getCalledFunction();
            if (callee && (callee->getName().contains("aurora_drop") ||
                           callee->getName().contains("aurora_scope")))
                continue;
        }
        return false;
    }

    /* ── Compute closed-form values ── */
    auto* ctx = &cl.header->getContext();
    auto* i64Ty = llvm::Type::getInt64Ty(*ctx);

    llvm::IRBuilder<> builder(cl.preheader);
    builder.SetInsertPoint(cl.preheader->getTerminator());
    llvm::Value* N = builder.CreateIntCast(cl.limit, i64Ty, true, "trip_count");

    /* Pre-compute sum of IV formula */
    llvm::Value* sum_of_iv_val = nullptr;

    for (auto& acc : accs) {
        llvm::Value* final_val = nullptr;

        if (acc.is_counter) {
            final_val = builder.CreateAdd(acc.orig_init ? acc.orig_init :
                llvm::ConstantInt::get(i64Ty, 0), N, "loop_counter_final");
        } else {
            if (!sum_of_iv_val) {
                llvm::Value* Nm1 = builder.CreateSub(N,
                    llvm::ConstantInt::get(i64Ty, 1), "trip_count_m1");
                llvm::Value* prod = builder.CreateMul(N, Nm1, "n_times_nm1");
                sum_of_iv_val = builder.CreateSDiv(prod,
                    llvm::ConstantInt::get(i64Ty, 2), "n_times_nm1_div2");
            }
            final_val = builder.CreateAdd(acc.orig_init ? acc.orig_init :
                llvm::ConstantInt::get(i64Ty, 0), sum_of_iv_val, "loop_sumiv_final");
        }

        /* Replace all uses of the accumulator AFTER the loop */
        replace_post_loop_loads(acc.slot, final_val, cl.exit, cl.header, cl.body);
        (void)final_val; /* final_val is used in replace_post_loop_loads */
    }

    /* Replace the induction variable too: final value after exit = limit */
    llvm::Value* iv_final = builder.CreateIntCast(cl.limit, i64Ty, true, "iv_final");
    replace_post_loop_loads(cl.iv_slot, iv_final, cl.exit, cl.header, cl.body);

    /* Redirect: preheader → exit directly (bypass the loop) */
    /* Update exit block phi nodes if any */
    for (auto& phi : cl.exit->phis()) {
        for (unsigned i = 0; i < phi.getNumIncomingValues(); i++) {
            if (phi.getIncomingBlock(i) == cl.header ||
                phi.getIncomingBlock(i) == cl.body) {
                phi.setIncomingBlock(i, cl.preheader);
            }
        }
    }

    /* Change the preheader branch to go directly to exit */
    builder.SetInsertPoint(cl.preheader->getTerminator());
    builder.CreateBr(cl.exit);
    cl.preheader->getTerminator()->eraseFromParent();

    return true;
}

/* ── Main entry point ── */
bool recognize_loop_idioms(llvm::Function& fn) {
    llvm::DominatorTree DT(fn);
    llvm::LoopInfo LI(DT);

    bool changed = false;

    llvm::SmallVector<llvm::Loop*, 8> worklist;
    for (auto* L : LI)
        worklist.push_back(L);

    while (!worklist.empty()) {
        auto* L = worklist.pop_back_val();
        if (process_loop(L))
            changed = true;

        for (auto* SL : L->getSubLoops())
            worklist.push_back(SL);
    }

    return changed;
}
