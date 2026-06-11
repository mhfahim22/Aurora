#include "compiler/codegen.hpp"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <iostream>

/* ════════════════════════════════════════════════════════════
   AI/ML Code Generation
   ════════════════════════════════════════════════════════════
   Generates LLVM IR for AI/ML constructs:
     - Tensor allocation & operations
     - Neural network layer calls
     - Predict/inference calls
   ════════════════════════════════════════════════════════════ */

/* ── Declare AI runtime helpers in the module ── */
void Codegen::declare_ai_runtime_helpers() {
    auto* ctx = &ctx_;
    auto* mod = module_.get();
    auto* dbl = llvm::Type::getDoubleTy(*ctx);
    auto* i64 = llvm::Type::getInt64Ty(*ctx);
    auto* ptr = llvm::PointerType::getUnqual(*ctx);

    /* Tensor runtime */
    /* i8* tensor_new(i64 dims, i64* shape) */
    tensor_new_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { i64, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_tensor_new", mod);

    /* void tensor_free(i8* tensor) */
    tensor_free_ = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(*ctx), { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_tensor_free", mod);

    /* i64 tensor_ndim(i8* tensor) */
    tensor_ndim_ = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_tensor_ndim", mod);

    /* i64 tensor_shape(i8* tensor, i64 dim) */
    tensor_shape_ = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, i64 }, false),
        llvm::Function::ExternalLinkage, "aurora_tensor_shape", mod);

    /* double tensor_get(i8* tensor, i64* indices) */
    tensor_get_ = llvm::Function::Create(
        llvm::FunctionType::get(dbl, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_tensor_get", mod);

    /* void tensor_set(i8* tensor, i64* indices, double val) */
    tensor_set_ = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(*ctx), { ptr, ptr, dbl }, false),
        llvm::Function::ExternalLinkage, "aurora_tensor_set", mod);

    /* i8* tensor_add(i8* a, i8* b) */
    tensor_add_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_tensor_add", mod);

    /* i8* tensor_sub(i8* a, i8* b) */
    tensor_sub_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_tensor_sub", mod);

    /* i8* tensor_mul(i8* a, i8* b) */
    tensor_mul_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_tensor_mul", mod);

    /* i8* tensor_matmul(i8* a, i8* b) */
    tensor_matmul_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_tensor_matmul", mod);

    /* i8* tensor_reshape(i8* tensor, i64 ndim, i64* shape) */
    tensor_reshape_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, i64, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_tensor_reshape", mod);

    /* Neural network runtime */
    /* i8* neural_forward(i8* input, i8* weights, i8* bias) */
    neural_forward_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_neural_forward", mod);

    /* i8* predict(i8* model, i8* input) */
    predict_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_predict", mod);
}

/* ── Generate AI tensor expression ── */
llvm::Value* Codegen::gen_tensor_expr(const ASTNode* node) {
    if (!node) return nullptr;

    /* tensor(shape...) — create new tensor */
    if (node->type == NodeType::Tensor) {
        /* Count dimensions from args */
        std::vector<llvm::Value*> dims;
        const ASTNode* arg = node->args.get();
        while (arg) {
            dims.push_back(gen_expr(arg));
            arg = arg->next.get();
        }

        auto* i64_ty = llvm::Type::getInt64Ty(ctx_);
        auto* ptr_ty = llvm::PointerType::getUnqual(ctx_);

        /* Allocate shape array on stack */
        auto* shape_alloca = create_entry_alloca(node->value + "_shape",
            llvm::ArrayType::get(i64_ty, dims.size()));
        for (size_t i = 0; i < dims.size(); i++) {
            auto* gep = builder_->CreateInBoundsGEP(
                llvm::ArrayType::get(i64_ty, dims.size()),
                shape_alloca,
                { i64(0), i64((int)i) });
            builder_->CreateStore(dims[i], gep);
        }

        auto* ndim = i64((int64_t)dims.size());
        auto* shape_ptr = builder_->CreateBitCast(shape_alloca, ptr_ty);
        return builder_->CreateCall(tensor_new_, { ndim, shape_ptr }, "tensor");
    }

    return nullptr;
}

/* ── Generate AI predict call ── */
llvm::Value* Codegen::gen_ai_predict(const ASTNode* node) {
    if (!node || !node->left || !node->right) {
        std::cerr << "predict requires model and input arguments\n";
        return nullptr;
    }
    auto* model = gen_expr(node->left.get());
    auto* input = gen_expr(node->right.get());
    return builder_->CreateCall(predict_, { model, input }, "prediction");
}

/* ── Generate neural network forward call ── */
llvm::Value* Codegen::gen_neural_forward(const ASTNode* node) {
    if (!node || !node->left || !node->right) {
        std::cerr << "neural forward requires input and weights\n";
        return nullptr;
    }
    auto* input = gen_expr(node->left.get());
    auto* weights = gen_expr(node->right.get());
    auto* bias = node->orelse ? gen_expr(node->orelse.get())
                               : llvm::ConstantPointerNull::get(
                                   llvm::PointerType::getUnqual(ctx_));
    return builder_->CreateCall(neural_forward_, { input, weights, bias }, "neural_out");
}
