#include "compiler/ir/ir_lowering.hpp"
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <unordered_map>

/* ════════════════════════════════════════════════════════════
   Aurora IR → LLVM IR Lowering (LLVM 18 opaque pointer compatible)
   ════════════════════════════════════════════════════════════ */

static llvm::Type* lower_type(int32_t idx, const std::vector<IrType>& pool,
                               llvm::LLVMContext& ctx) {
    if (idx < 0 || (size_t)idx >= pool.size())
        return llvm::Type::getVoidTy(ctx);

    const IrType& t = pool[idx];
    switch (t.kind) {
        case IrTypeKind::Void:   return llvm::Type::getVoidTy(ctx);
        case IrTypeKind::Int1:   return llvm::Type::getInt1Ty(ctx);
        case IrTypeKind::Int8:   return llvm::Type::getInt8Ty(ctx);
        case IrTypeKind::Int32:  return llvm::Type::getInt32Ty(ctx);
        case IrTypeKind::Int64:  return llvm::Type::getInt64Ty(ctx);
        case IrTypeKind::Float32: return llvm::Type::getFloatTy(ctx);
        case IrTypeKind::Float64: return llvm::Type::getDoubleTy(ctx);
        case IrTypeKind::Ptr:
            return llvm::PointerType::getUnqual(ctx);
        case IrTypeKind::Struct: {
            if (t.field_idxs.empty())
                return llvm::StructType::create(ctx, t.name);
            std::vector<llvm::Type*> elems;
            for (int32_t fi : t.field_idxs)
                elems.push_back(lower_type(fi, pool, ctx));
            return llvm::StructType::create(ctx, elems, t.name);
        }
        case IrTypeKind::Array: {
            auto* elem = lower_type(t.child_idx, pool, ctx);
            return llvm::ArrayType::get(elem, t.array_size);
        }
        case IrTypeKind::Func: {
            auto* ret = lower_type(t.ret_idx, pool, ctx);
            std::vector<llvm::Type*> params;
            for (int32_t pi : t.param_idxs)
                params.push_back(lower_type(pi, pool, ctx));
            return llvm::FunctionType::get(ret, params, false);
        }
    }
    return llvm::Type::getVoidTy(ctx);
}

static llvm::Value* lower_value(const IrValue& v, llvm::IRBuilder<>& builder,
                                 const std::vector<IrType>& pool,
                                 std::unordered_map<std::string, llvm::Value*>& ssa_map) {
    if (v.is_const) {
        if (v.type_idx >= 0 && (size_t)v.type_idx < pool.size()) {
            if (pool[v.type_idx].kind == IrTypeKind::Float64)
                return llvm::ConstantFP::get(builder.getContext(), llvm::APFloat(v.f64));
            if (pool[v.type_idx].kind == IrTypeKind::Float32)
                return llvm::ConstantFP::get(builder.getContext(), llvm::APFloat((float)v.f64));
        }
        return llvm::ConstantInt::get(llvm::Type::getInt64Ty(builder.getContext()), v.i64);
    }
    auto it = ssa_map.find(v.name);
    return (it != ssa_map.end()) ? it->second : nullptr;
}

static llvm::Value* lower_instruction(const IrInstruction& inst, llvm::IRBuilder<>& builder,
                                       const std::vector<IrType>& pool,
                                       const std::vector<std::string>& string_pool,
                                       std::unordered_map<int32_t, llvm::Value*>& str_cache,
                                       std::unordered_map<std::string, llvm::Value*>& ssa_map,
                                       llvm::Function* cur_fn) {
    return std::visit([&](const auto& i) -> llvm::Value* {
        using T = std::decay_t<decltype(i)>;
        (void)string_pool; (void)str_cache;

        if constexpr (std::is_same_v<T, IrAlloca>) {
            auto* ty = lower_type(i.type_idx, pool, builder.getContext());
            auto* alloca = builder.CreateAlloca(ty, nullptr, i.result_name);
            ssa_map[i.result_name] = alloca;
            return alloca;
        }

        else if constexpr (std::is_same_v<T, IrLoad>) {
            auto* ptr = lower_value(i.ptr, builder, pool, ssa_map);
            if (!ptr) return nullptr;
            auto* loaded_ty = lower_type(i.loaded_type, pool, builder.getContext());
            auto* loaded = builder.CreateLoad(loaded_ty, ptr, i.result_name);
            ssa_map[i.result_name] = loaded;
            return loaded;
        }

        else if constexpr (std::is_same_v<T, IrStore>) {
            auto* ptr = lower_value(i.ptr, builder, pool, ssa_map);
            auto* val = lower_value(i.value, builder, pool, ssa_map);
            if (!ptr || !val) return nullptr;
            return builder.CreateStore(val, ptr);
        }

        else if constexpr (std::is_same_v<T, IrBinOpInst>) {
            auto* lhs = lower_value(i.lhs, builder, pool, ssa_map);
            auto* rhs = lower_value(i.rhs, builder, pool, ssa_map);
            if (!lhs || !rhs) return nullptr;
            llvm::Value* result = nullptr;
            switch (i.op) {
                case IrBinOp::Add: result = builder.CreateAdd(lhs, rhs, i.result_name); break;
                case IrBinOp::Sub: result = builder.CreateSub(lhs, rhs, i.result_name); break;
                case IrBinOp::Mul: result = builder.CreateMul(lhs, rhs, i.result_name); break;
                case IrBinOp::SDiv: result = builder.CreateSDiv(lhs, rhs, i.result_name); break;
                case IrBinOp::UDiv: result = builder.CreateUDiv(lhs, rhs, i.result_name); break;
                case IrBinOp::SRem: result = builder.CreateSRem(lhs, rhs, i.result_name); break;
                case IrBinOp::URem: result = builder.CreateURem(lhs, rhs, i.result_name); break;
                case IrBinOp::And: result = builder.CreateAnd(lhs, rhs, i.result_name); break;
                case IrBinOp::Or: result = builder.CreateOr(lhs, rhs, i.result_name); break;
                case IrBinOp::Xor: result = builder.CreateXor(lhs, rhs, i.result_name); break;
                case IrBinOp::Shl: result = builder.CreateShl(lhs, rhs, i.result_name); break;
                case IrBinOp::AShr: result = builder.CreateAShr(lhs, rhs, i.result_name); break;
                case IrBinOp::LShr: result = builder.CreateLShr(lhs, rhs, i.result_name); break;
                case IrBinOp::FAdd: result = builder.CreateFAdd(lhs, rhs, i.result_name); break;
                case IrBinOp::FSub: result = builder.CreateFSub(lhs, rhs, i.result_name); break;
                case IrBinOp::FMul: result = builder.CreateFMul(lhs, rhs, i.result_name); break;
                case IrBinOp::FDiv: result = builder.CreateFDiv(lhs, rhs, i.result_name); break;
                case IrBinOp::FRem: result = builder.CreateFRem(lhs, rhs, i.result_name); break;
            }
            if (result) ssa_map[i.result_name] = result;
            return result;
        }

        else if constexpr (std::is_same_v<T, IrICmp>) {
            auto* lhs = lower_value(i.lhs, builder, pool, ssa_map);
            auto* rhs = lower_value(i.rhs, builder, pool, ssa_map);
            if (!lhs || !rhs) return nullptr;
            llvm::CmpInst::Predicate pred;
            switch (i.pred) {
                case IrCmpPred::EQ: pred = llvm::CmpInst::ICMP_EQ; break;
                case IrCmpPred::NE: pred = llvm::CmpInst::ICMP_NE; break;
                case IrCmpPred::SLT: pred = llvm::CmpInst::ICMP_SLT; break;
                case IrCmpPred::SGT: pred = llvm::CmpInst::ICMP_SGT; break;
                case IrCmpPred::SLE: pred = llvm::CmpInst::ICMP_SLE; break;
                case IrCmpPred::SGE: pred = llvm::CmpInst::ICMP_SGE; break;
                case IrCmpPred::ULT: pred = llvm::CmpInst::ICMP_ULT; break;
                case IrCmpPred::UGT: pred = llvm::CmpInst::ICMP_UGT; break;
                case IrCmpPred::ULE: pred = llvm::CmpInst::ICMP_ULE; break;
                case IrCmpPred::UGE: pred = llvm::CmpInst::ICMP_UGE; break;
            }
            auto* result = builder.CreateICmp(pred, lhs, rhs, i.result_name);
            ssa_map[i.result_name] = result;
            return result;
        }

        else if constexpr (std::is_same_v<T, IrCall>) {
            llvm::Function* callee = cur_fn->getParent()->getFunction(i.callee);
            if (!callee) return nullptr;
            std::vector<llvm::Value*> llvm_args;
            for (const auto& a : i.args) {
                auto* v = lower_value(a, builder, pool, ssa_map);
                if (v) llvm_args.push_back(v);
            }
            auto* result = builder.CreateCall(callee, llvm_args, i.result_name);
            if (!i.result_name.empty()) ssa_map[i.result_name] = result;
            return result;
        }

        else if constexpr (std::is_same_v<T, IrRet>) {
            auto* val = lower_value(i.value, builder, pool, ssa_map);
            if (val) return builder.CreateRet(val);
            return builder.CreateRetVoid();
        }

        else if constexpr (std::is_same_v<T, IrBr>) {
            llvm::BasicBlock* target = nullptr;
            for (auto& b : *cur_fn) {
                if (b.getName() == i.target) { target = &b; break; }
            }
            if (target) return builder.CreateBr(target);
            return nullptr;
        }

        else if constexpr (std::is_same_v<T, IrCondBr>) {
            auto* cond = lower_value(i.cond, builder, pool, ssa_map);
            if (!cond) return nullptr;
            llvm::BasicBlock *true_bb = nullptr, *false_bb = nullptr;
            for (auto& b : *cur_fn) {
                if (b.getName() == i.true_bb) true_bb = &b;
                if (b.getName() == i.false_bb) false_bb = &b;
            }
            if (!true_bb || !false_bb) return nullptr;
            return builder.CreateCondBr(cond, true_bb, false_bb);
        }

        else if constexpr (std::is_same_v<T, IrPhi>) {
            auto* phi = builder.CreatePHI(
                lower_type(i.type_idx, pool, builder.getContext()),
                (unsigned)i.incoming.size(), i.result_name);
            for (const auto& [val_name, block_name] : i.incoming) {
                auto* val = lower_value(ir_ssa(val_name, i.type_idx), builder, pool, ssa_map);
                llvm::BasicBlock* bb = nullptr;
                for (auto& b : *cur_fn) {
                    if (b.getName() == block_name) { bb = &b; break; }
                }
                if (val && bb) phi->addIncoming(val, bb);
            }
            ssa_map[i.result_name] = phi;
            return phi;
        }

        else if constexpr (std::is_same_v<T, IrGEP>) {
            auto* ptr = lower_value(i.ptr, builder, pool, ssa_map);
            if (!ptr) return nullptr;
            std::vector<llvm::Value*> llvm_indices;
            for (const auto& idx : i.indices)
                llvm_indices.push_back(lower_value(idx, builder, pool, ssa_map));
            /* Determine pointee type from the pointer's element type */
            llvm::Type* elem_ty = llvm::Type::getInt64Ty(builder.getContext());
            if (i.ptr.type_idx >= 0 && (size_t)i.ptr.type_idx < pool.size()) {
                int32_t child = pool[i.ptr.type_idx].child_idx;
                if (child >= 0)
                    elem_ty = lower_type(child, pool, builder.getContext());
            }
            auto* result = builder.CreateGEP(elem_ty, ptr, llvm_indices, i.result_name);
            ssa_map[i.result_name] = result;
            return result;
        }

        else if constexpr (std::is_same_v<T, IrBitCast>) {
            auto* val = lower_value(i.value, builder, pool, ssa_map);
            if (!val) return nullptr;
            auto* result = builder.CreateBitCast(
                val, lower_type(i.target_type, pool, builder.getContext()), i.result_name);
            ssa_map[i.result_name] = result;
            return result;
        }

        else if constexpr (std::is_same_v<T, IrStrLiteral>) {
            /* Check cache first */
            auto it = str_cache.find(i.string_idx);
            if (it != str_cache.end()) {
                ssa_map[i.result_name] = it->second;
                return it->second;
            }
            /* Need a C string pointer: create GlobalStringPtr */
            auto& ctx = builder.getContext();
            auto* mod = builder.GetInsertBlock()->getModule();
            const std::string& str = (size_t)i.string_idx < string_pool.size()
                ? string_pool[i.string_idx] : "";
            auto* gstr = builder.CreateGlobalStringPtr(str, ".str." + std::to_string(i.string_idx));
            /* Call aurora_str_from_cstr to create AuroraStr* */
            auto* callee = mod->getFunction("aurora_str_from_cstr");
            if (!callee) {
                auto* ptr_ty = llvm::PointerType::get(ctx, 0);
                auto* fn_ty = llvm::FunctionType::get(ptr_ty, {ptr_ty}, false);
                callee = llvm::Function::Create(fn_ty, llvm::Function::ExternalLinkage,
                                                 "aurora_str_from_cstr", mod);
            }
            auto* result = builder.CreateCall(callee, {gstr}, i.result_name);
            ssa_map[i.result_name] = result;
            str_cache[i.string_idx] = result;
            return result;
        }

        return nullptr;
    }, inst);
}

llvm::Module* lower_ir_to_llvm(const IrModule& ir_mod, llvm::LLVMContext& ctx) {
    auto* llvm_mod = new llvm::Module(ir_mod.name, ctx);

    for (const auto& g : ir_mod.globals) {
        auto* ty = lower_type(g.type_idx, ir_mod.type_pool, ctx);
        new llvm::GlobalVariable(
            *llvm_mod, ty, g.is_constant,
            llvm::GlobalValue::ExternalLinkage,
            nullptr, g.name);
    }

    std::unordered_map<std::string, llvm::Function*> fn_map;
    for (const auto& fn : ir_mod.functions) {
        auto* ret_ty = lower_type(fn.ret_type_idx, ir_mod.type_pool, ctx);
        std::vector<llvm::Type*> param_tys;
        for (const auto& p : fn.params)
            param_tys.push_back(lower_type(p.type_idx, ir_mod.type_pool, ctx));
        auto* fn_ty = llvm::FunctionType::get(ret_ty, param_tys, false);
        auto* llvm_fn = llvm::Function::Create(
            fn_ty, llvm::Function::ExternalLinkage, fn.name, llvm_mod);
        fn_map[fn.name] = llvm_fn;

        size_t ai = 0;
        for (auto& arg : llvm_fn->args()) {
            if (ai < fn.params.size())
                arg.setName(fn.params[ai].name);
            ai++;
        }
    }

    for (const auto& fn : ir_mod.functions) {
        auto* llvm_fn = fn_map[fn.name];
        if (!llvm_fn) continue;

        std::unordered_map<std::string, llvm::BasicBlock*> bb_map;

        for (const auto& bb_name : fn.block_order) {
            auto* bb = llvm::BasicBlock::Create(ctx, bb_name, llvm_fn);
            bb_map[bb_name] = bb;
        }

        std::unordered_map<std::string, llvm::Value*> ssa_map;

        /* Pre-populate ssa_map with function parameters */
        {
            size_t ai = 0;
            for (auto& arg : llvm_fn->args()) {
                if (ai < fn.params.size()) {
                    ssa_map["%" + fn.params[ai].name] = &arg;
                }
                ai++;
            }
        }

        for (const auto& bb_name : fn.block_order) {
            auto* bb = bb_map[bb_name];
            if (!bb) continue;

            const IrBasicBlock* ir_bb = nullptr;
            for (const auto& b : fn.blocks) {
                if (b.name == bb_name) { ir_bb = &b; break; }
            }
            if (!ir_bb) continue;

            llvm::IRBuilder<> builder(bb);

            std::unordered_map<int32_t, llvm::Value*> str_cache;
            for (const auto& inst : ir_bb->instructions) {
                lower_instruction(inst, builder, ir_mod.type_pool, ir_mod.string_pool,
                                  str_cache, ssa_map, llvm_fn);
            }
        }
    }

    return llvm_mod;
}
