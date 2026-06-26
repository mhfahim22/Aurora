// test_optimizer.cpp — Optimizer Pass Tests
// Tests const_fold, DCE, and strength_reduce passes

#include "common/test_suite.hpp"
#include "compiler/aurora_optimizer.hpp"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <memory>

static llvm::LLVMContext ctx;

/* ── Helper: create an empty function with entry block ── */
static llvm::Function* make_test_fn(llvm::Module* mod, const char* name) {
    auto* i64 = llvm::Type::getInt64Ty(ctx);
    auto* fty = llvm::FunctionType::get(i64, { i64, i64 }, false);
    auto* fn = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, name, mod);
    auto* bb = llvm::BasicBlock::Create(ctx, "entry", fn);
    llvm::IRBuilder<>(bb).CreateRet(
        llvm::ConstantInt::get(i64, 0));
    return fn;
}

/* ════════════════════════════════════════════════════════════
   Constant Folding Tests
   ════════════════════════════════════════════════════════════ */

bool test_fold_add_constants() {
    auto mod = std::make_unique<llvm::Module>("test", ctx);
    auto* fn = make_test_fn(mod.get(), "test_add");
    fn->deleteBody();

    llvm::IRBuilder<> builder(ctx);
    auto* bb = llvm::BasicBlock::Create(ctx, "entry", fn);
    builder.SetInsertPoint(bb);

    auto* lhs = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 10);
    auto* rhs = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 20);
    auto* add = llvm::BinaryOperator::CreateAdd(lhs, rhs, "add", bb);
    builder.CreateRet(add);

    TEST_ASSERT(fold_binary_constants(*fn), "should fold add of constants");
    return true;
}

bool test_fold_mul_constants() {
    auto mod = std::make_unique<llvm::Module>("test", ctx);
    auto* fn = make_test_fn(mod.get(), "test_mul");
    fn->deleteBody();

    llvm::IRBuilder<> builder(ctx);
    auto* bb = llvm::BasicBlock::Create(ctx, "entry", fn);
    builder.SetInsertPoint(bb);

    auto* lhs = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 7);
    auto* rhs = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 6);
    auto* mul = llvm::BinaryOperator::CreateMul(lhs, rhs, "mul", bb);
    builder.CreateRet(mul);

    TEST_ASSERT(fold_binary_constants(*fn), "should fold mul of constants");
    return true;
}

bool test_fold_icmp_eq() {
    auto mod = std::make_unique<llvm::Module>("test", ctx);
    auto* fn = make_test_fn(mod.get(), "test_icmp");
    fn->deleteBody();

    llvm::IRBuilder<> builder(ctx);
    auto* bb = llvm::BasicBlock::Create(ctx, "entry", fn);
    builder.SetInsertPoint(bb);

    auto* lhs = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 42);
    auto* rhs = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 42);
    auto* icmp = new llvm::ICmpInst(*bb, llvm::ICmpInst::ICMP_EQ, lhs, rhs, "cmp");
    builder.CreateRet(icmp);

    TEST_ASSERT(fold_comparison_constants(*fn), "should fold icmp eq of constants");
    return true;
}

bool test_fold_icmp_ne() {
    auto mod = std::make_unique<llvm::Module>("test", ctx);
    auto* fn = make_test_fn(mod.get(), "test_icmp_ne");
    fn->deleteBody();

    llvm::IRBuilder<> builder(ctx);
    auto* bb = llvm::BasicBlock::Create(ctx, "entry", fn);
    builder.SetInsertPoint(bb);

    auto* lhs = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 1);
    auto* rhs = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 2);
    auto* icmp = new llvm::ICmpInst(*bb, llvm::ICmpInst::ICMP_NE, lhs, rhs, "cmp");
    builder.CreateRet(icmp);

    TEST_ASSERT(fold_comparison_constants(*fn), "should fold icmp ne of constants");
    return true;
}

bool test_fold_branch_constant_true() {
    auto mod = std::make_unique<llvm::Module>("test", ctx);
    auto* fn = make_test_fn(mod.get(), "test_br_true");
    fn->deleteBody();

    llvm::IRBuilder<> builder(ctx);
    auto* entry = llvm::BasicBlock::Create(ctx, "entry", fn);
    auto* then_bb = llvm::BasicBlock::Create(ctx, "then", fn);
    auto* else_bb = llvm::BasicBlock::Create(ctx, "else", fn);

    builder.SetInsertPoint(entry);
    auto* cond = llvm::ConstantInt::getTrue(ctx);
    builder.CreateCondBr(cond, then_bb, else_bb);

    builder.SetInsertPoint(then_bb);
    builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 1));

    builder.SetInsertPoint(else_bb);
    builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0));

    TEST_ASSERT(fold_branch_constants(*fn), "should fold conditional branch with constant true");
    return true;
}

bool test_fold_branch_constant_false() {
    auto mod = std::make_unique<llvm::Module>("test", ctx);
    auto* fn = make_test_fn(mod.get(), "test_br_false");
    fn->deleteBody();

    llvm::IRBuilder<> builder(ctx);
    auto* entry = llvm::BasicBlock::Create(ctx, "entry", fn);
    auto* then_bb = llvm::BasicBlock::Create(ctx, "then", fn);
    auto* else_bb = llvm::BasicBlock::Create(ctx, "else", fn);

    builder.SetInsertPoint(entry);
    auto* cond = llvm::ConstantInt::getFalse(ctx);
    builder.CreateCondBr(cond, then_bb, else_bb);

    builder.SetInsertPoint(then_bb);
    builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 1));

    builder.SetInsertPoint(else_bb);
    builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0));

    TEST_ASSERT(fold_branch_constants(*fn), "should fold conditional branch with constant false");
    return true;
}

/* ════════════════════════════════════════════════════════════
   Dead Code Elimination Tests
   ════════════════════════════════════════════════════════════ */

bool test_dce_dead_instruction() {
    auto mod = std::make_unique<llvm::Module>("test", ctx);
    auto* fn = make_test_fn(mod.get(), "test_dce");
    fn->deleteBody();

    llvm::IRBuilder<> builder(ctx);
    auto* bb = llvm::BasicBlock::Create(ctx, "entry", fn);
    builder.SetInsertPoint(bb);

    auto* c1 = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 10);
    auto* c2 = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 20);
    auto* dead = llvm::BinaryOperator::CreateAdd(c1, c2, "dead", bb);
    (void)dead;
    builder.CreateRet(c1);

    int before = (int)bb->size();
    TEST_ASSERT(eliminate_dead_instructions(*fn), "should eliminate dead instruction");
    TEST_ASSERT((int)bb->size() < before, "instruction count should decrease");
    return true;
}

bool test_dce_keep_live_instruction() {
    auto mod = std::make_unique<llvm::Module>("test", ctx);
    auto* fn = make_test_fn(mod.get(), "test_live");
    fn->deleteBody();

    llvm::IRBuilder<> builder(ctx);
    auto* bb = llvm::BasicBlock::Create(ctx, "entry", fn);
    builder.SetInsertPoint(bb);

    auto* c1 = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 10);
    auto* c2 = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 20);
    auto* add = llvm::BinaryOperator::CreateAdd(c1, c2, "live", bb);
    builder.CreateRet(add);

    int before = (int)bb->size();
    eliminate_dead_instructions(*fn);
    TEST_ASSERT((int)bb->size() == before, "live instruction should not be removed");
    return true;
}

/* ════════════════════════════════════════════════════════════
   Strength Reduction Tests
   ════════════════════════════════════════════════════════════ */

bool test_strength_reduce_mul_pow2() {
    auto mod = std::make_unique<llvm::Module>("test", ctx);
    auto* fn = make_test_fn(mod.get(), "test_mul_pow2");
    fn->deleteBody();

    llvm::IRBuilder<> builder(ctx);
    auto* bb = llvm::BasicBlock::Create(ctx, "entry", fn);
    builder.SetInsertPoint(bb);

    auto* lhs = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 5);
    auto* rhs = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 8);
    auto* mul = llvm::BinaryOperator::CreateMul(lhs, rhs, "mul", bb);
    builder.CreateRet(mul);

    TEST_ASSERT(strength_reduce(*fn), "should reduce mul by power-of-2 to shl");
    return true;
}

bool test_strength_reduce_mul_zero() {
    auto mod = std::make_unique<llvm::Module>("test", ctx);
    auto* fn = make_test_fn(mod.get(), "test_mul_zero");
    fn->deleteBody();

    llvm::IRBuilder<> builder(ctx);
    auto* bb = llvm::BasicBlock::Create(ctx, "entry", fn);
    builder.SetInsertPoint(bb);

    auto* lhs = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 42);
    auto* rhs = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0);
    auto* mul = llvm::BinaryOperator::CreateMul(lhs, rhs, "mul", bb);
    builder.CreateRet(mul);

    TEST_ASSERT(strength_reduce(*fn), "should reduce mul by 0 to constant 0");
    return true;
}

bool test_strength_reduce_mul_one() {
    auto mod = std::make_unique<llvm::Module>("test", ctx);
    auto* fn = make_test_fn(mod.get(), "test_mul_one");
    fn->deleteBody();

    llvm::IRBuilder<> builder(ctx);
    auto* bb = llvm::BasicBlock::Create(ctx, "entry", fn);
    builder.SetInsertPoint(bb);

    auto* lhs = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 99);
    auto* rhs = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 1);
    auto* mul = llvm::BinaryOperator::CreateMul(lhs, rhs, "mul", bb);
    builder.CreateRet(mul);

    TEST_ASSERT(strength_reduce(*fn), "should reduce mul by 1 to identity");
    return true;
}

bool test_strength_reduce_sdiv_pow2() {
    auto mod = std::make_unique<llvm::Module>("test", ctx);
    auto* fn = make_test_fn(mod.get(), "test_sdiv_pow2");
    fn->deleteBody();

    llvm::IRBuilder<> builder(ctx);
    auto* bb = llvm::BasicBlock::Create(ctx, "entry", fn);
    builder.SetInsertPoint(bb);

    auto* lhs = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 100);
    auto* rhs = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 4);
    auto* sdiv = llvm::BinaryOperator::CreateSDiv(lhs, rhs, "sdiv", bb);
    builder.CreateRet(sdiv);

    TEST_ASSERT(strength_reduce(*fn), "should reduce sdiv by power-of-2 to ashr");
    return true;
}

bool test_strength_reduce_udiv_pow2() {
    auto mod = std::make_unique<llvm::Module>("test", ctx);
    auto* fn = make_test_fn(mod.get(), "test_udiv_pow2");
    fn->deleteBody();

    llvm::IRBuilder<> builder(ctx);
    auto* bb = llvm::BasicBlock::Create(ctx, "entry", fn);
    builder.SetInsertPoint(bb);

    auto* lhs = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 100);
    auto* rhs = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 8);
    auto* udiv = llvm::BinaryOperator::CreateUDiv(lhs, rhs, "udiv", bb);
    builder.CreateRet(udiv);

    TEST_ASSERT(strength_reduce(*fn), "should reduce udiv by power-of-2 to lshr");
    return true;
}

bool test_strength_reduce_urem_pow2() {
    auto mod = std::make_unique<llvm::Module>("test", ctx);
    auto* fn = make_test_fn(mod.get(), "test_urem_pow2");
    fn->deleteBody();

    llvm::IRBuilder<> builder(ctx);
    auto* bb = llvm::BasicBlock::Create(ctx, "entry", fn);
    builder.SetInsertPoint(bb);

    auto* lhs = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 42);
    auto* rhs = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 16);
    auto* urem = llvm::BinaryOperator::CreateURem(lhs, rhs, "urem", bb);
    builder.CreateRet(urem);

    TEST_ASSERT(strength_reduce(*fn), "should reduce urem by power-of-2 to and");
    return true;
}

bool test_strength_reduce_add_zero() {
    auto mod = std::make_unique<llvm::Module>("test", ctx);
    auto* fn = make_test_fn(mod.get(), "test_add_zero");
    fn->deleteBody();

    llvm::IRBuilder<> builder(ctx);
    auto* bb = llvm::BasicBlock::Create(ctx, "entry", fn);
    builder.SetInsertPoint(bb);

    auto* lhs = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 42);
    auto* rhs = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0);
    auto* add = llvm::BinaryOperator::CreateAdd(lhs, rhs, "add", bb);
    builder.CreateRet(add);

    TEST_ASSERT(strength_reduce(*fn), "should reduce add 0 to identity");
    return true;
}

/* ════════════════════════════════════════════════════════════
   All Optimizer Passes via run_aurora_optimizer (integration)
   ════════════════════════════════════════════════════════════ */

bool test_full_optimizer_pipeline() {
    auto mod = std::make_unique<llvm::Module>("test", ctx);

    /* Create a function with foldable constants and dead code */
    auto* i64 = llvm::Type::getInt64Ty(ctx);
    auto* fty = llvm::FunctionType::get(i64, { i64, i64 }, false);
    auto* fn = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, "test_pipeline", mod.get());

    auto* bb = llvm::BasicBlock::Create(ctx, "entry", fn);
    llvm::IRBuilder<> builder(bb);

    auto* c1 = llvm::ConstantInt::get(i64, 5);
    auto* c2 = llvm::ConstantInt::get(i64, 10);
    auto* add = llvm::BinaryOperator::CreateAdd(c1, c2, "foldable", bb);
    auto* dead = llvm::BinaryOperator::CreateMul(c1, add, "dead_mul", bb);
    (void)dead;
    builder.CreateRet(add);

    llvm::BasicBlock* dead_bb = llvm::BasicBlock::Create(ctx, "dead", fn);
    llvm::IRBuilder<>(dead_bb).CreateRet(c1);

    run_aurora_optimizer(mod.get());

    TEST_ASSERT(!llvm::verifyFunction(*fn, &llvm::outs()), "function should verify after optimizer");
    return true;
}

/* ════════════════════════════════════════════════════════════
   Aurora IR Tests
   ════════════════════════════════════════════════════════════ */

#include "compiler/ir/ir.hpp"
#include "compiler/ir/ir_lowering.hpp"
#include "compiler/ir/ir_mem2reg.hpp"
#include "compiler/ir/ir_optimizer.hpp"
#include <cstdlib>

static bool test_ir_type_pool_dedup() {
    std::vector<IrType> pool;
    /* ir_make_primitive now deduplicates */
    int32_t i64_1 = ir_make_primitive(pool, IrTypeKind::Int64);
    int32_t i64_2 = ir_make_primitive(pool, IrTypeKind::Int64);
    int32_t ptr_1 = ir_make_primitive(pool, IrTypeKind::Ptr);
    /* Same kind → same index */
    TEST_ASSERT(i64_1 == i64_2, "Int64 should deduplicate");
    /* Different kind → different index */
    TEST_ASSERT(i64_1 != ptr_1, "Int64 != Ptr");
    TEST_ASSERT(pool.size() == 2, "pool should have 2 types (Int64, Ptr)");
    return true;
}

static bool test_ir_basic_block() {
    IrFunction fn;
    fn.name = "test_fn";
    fn.blocks.push_back({"entry", {}});
    fn.block_order.push_back("entry");
    fn.blocks.push_back({"exit", {}});
    fn.block_order.push_back("exit");

    IrModule mod;
    mod.functions.push_back(std::move(fn));
    TEST_ASSERT(!mod.get_function("test_fn")->blocks.empty(), "blocks should not be empty");
    return true;
}

static bool test_ir_instruction_build() {
    IrInstruction inst1 = IrAlloca{0, "%tmp"};
    IrInstruction inst2 = IrBinOpInst{IrBinOp::Add, ir_const_i64(1), ir_const_i64(2), "%add"};
    IrInstruction inst3 = IrRet{ir_const_i64(0)};

    TEST_ASSERT(std::get_if<IrAlloca>(&inst1) != nullptr, "inst1 should be IrAlloca");
    TEST_ASSERT(std::get_if<IrBinOpInst>(&inst2) != nullptr, "inst2 should be IrBinOpInst");
    TEST_ASSERT(std::get_if<IrRet>(&inst3) != nullptr, "inst3 should be IrRet");
    return true;
}

static bool test_ir_printer() {
    IrModule mod;
    mod.name = "test_mod";
    std::string s = mod.to_string();
    TEST_ASSERT(!s.empty(), "string should not be empty");
    return true;
}

static bool test_ir_mem2reg() {
    std::vector<IrType> pool;
    ir_make_primitive(pool, IrTypeKind::Int64);
    ir_make_primitive(pool, IrTypeKind::Ptr);

    IrFunction fn;
    fn.name = "test";
    fn.params.push_back({"x", 0});
    fn.blocks.push_back({"entry", {}});
    fn.block_order.push_back("entry");

    /* entry:
     *   %a.addr = alloca i64
     *   store 42, %a.addr
     *   %v = load %a.addr
     *   ret %v
     */
    fn.blocks[0].instructions.push_back(IrAlloca{0, "%a.addr"});
    fn.blocks[0].instructions.push_back(IrStore{ir_ssa("%a.addr", 1), ir_const_i64(42)});
    fn.blocks[0].instructions.push_back(IrLoad{ir_ssa("%a.addr", 1), "%v", 0});

    bool changed = ir_mem2reg(fn, pool);
    TEST_ASSERT(changed, "mem2reg should have made changes");

    /* After mem2reg: alloca and store should be gone, load should be bitcast */
    bool has_alloca = false, has_store = false, has_load = false, has_bitcast = false;
    for (auto& inst : fn.blocks[0].instructions) {
        if (std::get_if<IrAlloca>(&inst)) has_alloca = true;
        if (std::get_if<IrStore>(&inst)) has_store = true;
        if (std::get_if<IrLoad>(&inst)) has_load = true;
        if (std::get_if<IrBitCast>(&inst)) has_bitcast = true;
    }
    TEST_ASSERT(!has_alloca, "alloca should be removed");
    TEST_ASSERT(!has_store, "store should be removed");
    TEST_ASSERT(!has_load, "load should be replaced");
    TEST_ASSERT(has_bitcast, "bitcast should be present");

    return true;
}

static bool test_ir_lowering() {
    std::vector<IrType> pool;
    ir_make_primitive(pool, IrTypeKind::Int64);
    ir_make_primitive(pool, IrTypeKind::Ptr);
    int32_t void_ty = ir_make_primitive(pool, IrTypeKind::Void);

    IrModule mod;
    mod.name = "lower_test";

    IrFunction fn;
    fn.name = "main";
    fn.ret_type_idx = 0; /* i64 */
    fn.blocks.push_back({"entry", {}});
    fn.block_order.push_back("entry");
    fn.blocks[0].instructions.push_back(IrRet{ir_const_i64(42)});
    mod.functions.push_back(std::move(fn));

    /* Lower to LLVM IR */
    auto* ctx = new llvm::LLVMContext();
    auto* llvm_mod = lower_ir_to_llvm(mod, *ctx);
    TEST_ASSERT(llvm_mod != nullptr, "lowering should produce a module");
    TEST_ASSERT(llvm_mod->getFunction("main") != nullptr, "main function should exist");

    delete llvm_mod;
    delete ctx;
    return true;
}

static bool test_ir_full_pipeline() {
    std::vector<IrType> pool;
    ir_make_primitive(pool, IrTypeKind::Int64);
    ir_make_primitive(pool, IrTypeKind::Ptr);
    ir_make_primitive(pool, IrTypeKind::Void);

    IrModule mod;
    mod.name = "pipeline_test";

    IrFunction fn;
    fn.name = "calc";
    fn.ret_type_idx = 0;
    fn.blocks.push_back({"entry", {}});
    fn.block_order.push_back("entry");

    /* entry:
     *   %z = add 10, 20
     *   ret %z
     *
     * After const_fold: alloca/store proxy, then DCE should clean it up
     */
    auto& bb = fn.blocks[0];
    int32_t i64_idx = 0;
    bb.instructions.push_back(IrBinOpInst{IrBinOp::Add, ir_const_i64(10), ir_const_i64(20), "%z"});
    bb.instructions.push_back(IrRet{ir_ssa("%z", i64_idx)});

    mod.functions.push_back(std::move(fn));

    ir_optimize(mod);

    auto* lowered = mod.get_function("calc");
    TEST_ASSERT(lowered != nullptr, "calc should exist after optimization");

    /* After const_fold + DCE: only bitcast 30 + ret %z */
    int inst_count = 0;
    bool has_bitcast_30 = false;
    for (const auto& block : lowered->blocks) {
        for (const auto& inst : block.instructions) {
            inst_count++;
            if (auto* bc = std::get_if<IrBitCast>(&inst)) {
                if (bc->value.is_const && bc->value.i64() == 30)
                    has_bitcast_30 = true;
            }
        }
    }
    TEST_ASSERT(inst_count >= 2, "should have bitcast + ret");
    TEST_ASSERT(has_bitcast_30, "should have bitcast of 30");

    return true;
}

/* ════════════════════════════════════════════════════════════
   Main Test Runner
   ════════════════════════════════════════════════════════════ */

int main() {
    TestSuite suite;

    /* Constant Folding Tests */
    suite.add_test("Fold Add Constants", test_fold_add_constants);
    suite.add_test("Fold Mul Constants", test_fold_mul_constants);
    suite.add_test("Fold ICmp EQ", test_fold_icmp_eq);
    suite.add_test("Fold ICmp NE", test_fold_icmp_ne);
    suite.add_test("Fold Branch True", test_fold_branch_constant_true);
    suite.add_test("Fold Branch False", test_fold_branch_constant_false);

    /* Dead Code Elimination Tests */
    suite.add_test("DCE Dead Instruction", test_dce_dead_instruction);
    suite.add_test("DCE Keep Live Instruction", test_dce_keep_live_instruction);

    /* Strength Reduction Tests */
    suite.add_test("StrReduce Mul Pow2", test_strength_reduce_mul_pow2);
    suite.add_test("StrReduce Mul Zero", test_strength_reduce_mul_zero);
    suite.add_test("StrReduce Mul One", test_strength_reduce_mul_one);
    suite.add_test("StrReduce SDiv Pow2", test_strength_reduce_sdiv_pow2);
    suite.add_test("StrReduce UDiv Pow2", test_strength_reduce_udiv_pow2);
    suite.add_test("StrReduce URem Pow2", test_strength_reduce_urem_pow2);
    suite.add_test("StrReduce Add Zero", test_strength_reduce_add_zero);

    /* Integration Test */
    suite.add_test("Full Optimizer Pipeline", test_full_optimizer_pipeline);

    /* ── Aurora IR Tests ── */
    suite.add_test("IR Type Pool Dedup", test_ir_type_pool_dedup);
    suite.add_test("IR Basic Block", test_ir_basic_block);
    suite.add_test("IR Instruction Build", test_ir_instruction_build);
    suite.add_test("IR Printer", test_ir_printer);
    suite.add_test("IR Mem2Reg", test_ir_mem2reg);
    suite.add_test("IR Lowering", test_ir_lowering);
    suite.add_test("IR Full Pipeline", test_ir_full_pipeline);

    suite.run_all();

    return (suite.get_failed() > 0) ? 1 : 0;
}
