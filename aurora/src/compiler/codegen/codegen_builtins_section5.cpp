#include "compiler/codegen_builtins.hpp"
#include "compiler/ast.hpp"

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>

static llvm::Type* i64_ty(llvm::LLVMContext& ctx) { return llvm::Type::getInt64Ty(ctx); }
static llvm::PointerType* ptr_ty(llvm::LLVMContext& ctx) { return llvm::PointerType::getUnqual(ctx); }

static llvm::Value* to_ptr(llvm::IRBuilder<>& builder, llvm::LLVMContext& ctx,
                            llvm::Value* val) {
    if (val->getType()->isPointerTy()) return val;
    return builder.CreateIntToPtr(val, ptr_ty(ctx), "i64toptr");
}

llvm::Value* codegen_builtin_section5(
    const std::string& name,
    const ASTNode* node,
    llvm::IRBuilder<>& builder,
    llvm::LLVMContext& ctx,
    const BuiltinFunctions& builtins,
    std::function<llvm::Value*(const ASTNode*)> gen_expr,
    llvm::Module* module)
{
    static_cast<void>(module);

    /* ── Data loading ── */
    if (name == "csv" && node->args) {
        llvm::Value* path = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.csv_fn, { path }, "csv_ret");
    }
    if (name == "data" && node->args) {
        llvm::Value* path = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.data_fn, { path }, "data_ret");
    }

    /* ── Data processing ── */
    if (name == "clean" && node->args) {
        llvm::Value* d = gen_expr(node->args.get());
        return builder.CreateCall(builtins.clean_fn, { d }, "clean_ret");
    }
    if (name == "normalize" && node->args) {
        llvm::Value* d = gen_expr(node->args.get());
        return builder.CreateCall(builtins.normalize_fn, { d }, "norm_ret");
    }
    if (name == "standard" && node->args) {
        llvm::Value* d = gen_expr(node->args.get());
        return builder.CreateCall(builtins.standard_fn, { d }, "std_ret");
    }
    if (name == "shuffle" && node->args) {
        llvm::Value* d = gen_expr(node->args.get());
        return builder.CreateCall(builtins.shuffle_fn, { d }, "shuffle_ret");
    }
    if (name == "split_data" && node->args) {
        llvm::Value* d = gen_expr(node->args.get());
        llvm::Value* ratio = llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx), 0.8);
        if (node->args->next)
            ratio = gen_expr(node->args->next.get());
        return builder.CreateCall(builtins.split_data_fn, { d, ratio }, "split_ret");
    }

    /* ── Model lifecycle ── */
    if (name == "model_create" && node->args) {
        llvm::Value* type_a = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.model_create_fn, { type_a }, "model");
    }
    if (name == "model_save" && node->args) {
        llvm::Value* m = gen_expr(node->args.get());
        llvm::Value* path = to_ptr(builder, ctx, node->args->next ? gen_expr(node->args->next.get()) : llvm::ConstantPointerNull::get(ptr_ty(ctx)));
        return builder.CreateCall(builtins.model_save_fn, { m, path }, "save_ret");
    }
    if (name == "model_load" && node->args) {
        llvm::Value* path = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.model_load_fn, { path }, "model");
    }

    /* ── Model config ── */
    if (name == "set_loss" && node->args) {
        llvm::Value* m = gen_expr(node->args.get());
        llvm::Value* loss = to_ptr(builder, ctx, node->args->next ? gen_expr(node->args->next.get()) : llvm::ConstantPointerNull::get(ptr_ty(ctx)));
        return builder.CreateCall(builtins.set_loss_fn, { m, loss }, "loss_ret");
    }
    if (name == "set_optimizer" && node->args) {
        llvm::Value* m = gen_expr(node->args.get());
        llvm::Value* opt = to_ptr(builder, ctx, node->args->next ? gen_expr(node->args->next.get()) : llvm::ConstantPointerNull::get(ptr_ty(ctx)));
        return builder.CreateCall(builtins.set_optimizer_fn, { m, opt }, "opt_ret");
    }
    if (name == "set_lr" && node->args) {
        llvm::Value* m = gen_expr(node->args.get());
        llvm::Value* lr_val = llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx), 0.001);
        if (node->args->next) lr_val = gen_expr(node->args->next.get());
        return builder.CreateCall(builtins.set_lr_fn, { m, lr_val }, "lr_ret");
    }
    if (name == "set_batch_size" && node->args) {
        llvm::Value* m = gen_expr(node->args.get());
        llvm::Value* bs = llvm::ConstantInt::get(i64_ty(ctx), 32);
        if (node->args->next) bs = gen_expr(node->args->next.get());
        return builder.CreateCall(builtins.set_batch_size_fn, { m, bs }, "bs_ret");
    }
    if (name == "set_epochs" && node->args) {
        llvm::Value* m = gen_expr(node->args.get());
        llvm::Value* ep = llvm::ConstantInt::get(i64_ty(ctx), 10);
        if (node->args->next) ep = gen_expr(node->args->next.get());
        return builder.CreateCall(builtins.set_epochs_fn, { m, ep }, "ep_ret");
    }
    if (name == "set_validation_split" && node->args) {
        llvm::Value* m = gen_expr(node->args.get());
        llvm::Value* split = llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx), 0.2);
        if (node->args->next) split = gen_expr(node->args->next.get());
        return builder.CreateCall(builtins.set_validation_split_fn, { m, split }, "vs_ret");
    }
    if (name == "set_verbose" && node->args) {
        llvm::Value* m = gen_expr(node->args.get());
        llvm::Value* v = llvm::ConstantInt::get(i64_ty(ctx), 1);
        if (node->args->next) v = gen_expr(node->args->next.get());
        return builder.CreateCall(builtins.set_verbose_fn, { m, v }, "verb_ret");
    }
    if (name == "set_early_stop" && node->args) {
        llvm::Value* m = gen_expr(node->args.get());
        llvm::Value* p = llvm::ConstantInt::get(i64_ty(ctx), 5);
        if (node->args->next) p = gen_expr(node->args->next.get());
        return builder.CreateCall(builtins.set_early_stop_fn, { m, p }, "es_ret");
    }

    /* ── Layer creation ── */
    if (name == "dense" && node->args) {
        llvm::Value* units = gen_expr(node->args.get());
        llvm::Value* activation = llvm::ConstantPointerNull::get(ptr_ty(ctx));
        if (node->args->next)
            activation = to_ptr(builder, ctx, gen_expr(node->args->next.get()));
        return builder.CreateCall(builtins.dense_fn, { units, activation }, "layer");
    }
    if (name == "conv" && node->args) {
        llvm::Value* filters = gen_expr(node->args.get());
        llvm::Value* kernel_size = llvm::ConstantInt::get(i64_ty(ctx), 3);
        llvm::Value* stride = llvm::ConstantInt::get(i64_ty(ctx), 1);
        if (node->args->next) kernel_size = gen_expr(node->args->next.get());
        if (node->args->next && node->args->next->next) stride = gen_expr(node->args->next->next.get());
        return builder.CreateCall(builtins.conv_fn, { filters, kernel_size, stride }, "conv_layer");
    }
    if (name == "lstm" && node->args) {
        llvm::Value* units = gen_expr(node->args.get());
        return builder.CreateCall(builtins.lstm_fn, { units }, "lstm_layer");
    }
    if (name == "gru" && node->args) {
        llvm::Value* units = gen_expr(node->args.get());
        return builder.CreateCall(builtins.gru_fn, { units }, "gru_layer");
    }
    if (name == "dropout" && node->args) {
        llvm::Value* rate = gen_expr(node->args.get());
        return builder.CreateCall(builtins.dropout_fn, { rate }, "dropout_ret");
    }
    if (name == "batchnorm") {
        return builder.CreateCall(builtins.batchnorm_fn, {}, "bn_layer");
    }
    if (name == "attention") {
        return builder.CreateCall(builtins.attention_fn, {}, "attn_layer");
    }
    if (name == "transformer") {
        return builder.CreateCall(builtins.transformer_fn, {}, "tf_layer");
    }
    if (name == "embedding" && node->args) {
        llvm::Value* vocab = gen_expr(node->args.get());
        llvm::Value* dim = llvm::ConstantInt::get(i64_ty(ctx), 128);
        if (node->args->next) dim = gen_expr(node->args->next.get());
        return builder.CreateCall(builtins.embedding_fn, { vocab, dim }, "embed_layer");
    }
    if (name == "layernorm") {
        return builder.CreateCall(builtins.layernorm_fn, {}, "ln_layer");
    }

    /* ── Model operations ── */
    if (name == "ml_add" && node->args) {
        llvm::Value* m = gen_expr(node->args.get());
        llvm::Value* layer = gen_expr(node->args->next.get());
        return builder.CreateCall(builtins.add_fn, { m, layer }, "add_ret");
    }
    if (name == "ml_train" && node->args) {
        llvm::Value* m = gen_expr(node->args.get());
        llvm::Value* d = gen_expr(node->args->next.get());
        return builder.CreateCall(builtins.train_fn, { m, d }, "train_ret");
    }
    if (name == "ml_fit" && node->args) {
        llvm::Value* m = gen_expr(node->args.get());
        llvm::Value* x = gen_expr(node->args->next.get());
        llvm::Value* y = gen_expr(node->args->next->next.get());
        return builder.CreateCall(builtins.fit_fn, { m, x, y }, "fit_ret");
    }
    if (name == "ml_test" && node->args) {
        llvm::Value* m = gen_expr(node->args.get());
        llvm::Value* d = gen_expr(node->args->next.get());
        return builder.CreateCall(builtins.test_fn, { m, d }, "test_ret");
    }
    if (name == "ml_predict" && node->args) {
        llvm::Value* m = gen_expr(node->args.get());
        llvm::Value* input = gen_expr(node->args->next.get());
        return builder.CreateCall(builtins.predict_fn, { m, input }, "pred_ret");
    }
    if (name == "ml_retrain" && node->args) {
        llvm::Value* m = gen_expr(node->args.get());
        return builder.CreateCall(builtins.retrain_fn, { m }, "retrain_ret");
    }

    return nullptr;
}