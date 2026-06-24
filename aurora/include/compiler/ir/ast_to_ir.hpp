#pragma once
#include "compiler/ir/ir.hpp"
#include "compiler/ast.hpp"
#include <string>
#include <unordered_map>

/* ════════════════════════════════════════════════════════════
   AstToIr — Convert AST to Aurora IR
   ════════════════════════════════════════════════════════════
   Walks the AST and emits Aurora IR instructions.
   The resulting IrModule can be optimized and lowered to LLVM IR.
   ════════════════════════════════════════════════════════════ */

class AstToIr {
public:
    AstToIr();

    IrModule translate(const ASTNode* root);

private:
    IrModule mod_;

    /* ── Current function state ── */
    IrFunction* cur_fn_{nullptr};
    IrBasicBlock* cur_bb_{nullptr};
    int ssa_counter_{0};

    /* ── Symbol table: name → {ssa_name, is_param} ── */
    struct SymInfo { std::string ssa_name; bool is_param{false}; };
    std::unordered_map<std::string, SymInfo> symbols_;

    /* ── Type pool helpers ── */
    int32_t i64_type();
    int32_t f64_type();
    int32_t i8_type();
    int32_t i1_type();
    int32_t ptr_type();
    int32_t void_type();
    std::unordered_map<IrTypeKind, int32_t> type_cache_;

    /* ── SSA value helpers ── */
    std::string fresh_ssa(const std::string& hint = "t");
    IrValue ssa(const std::string& name, int32_t ty = -1);

    /* ── Expression translation ── */
    IrValue gen_expr(const ASTNode* node);
    IrValue gen_var(const ASTNode* node);
    IrValue gen_binop(const ASTNode* node);
    IrValue gen_unary(const ASTNode* node);
    IrValue gen_call(const ASTNode* node);
    IrValue gen_num(const ASTNode* node);
    IrValue gen_float(const ASTNode* node);
    IrValue gen_str(const ASTNode* node);

    /* ── Statement translation ── */
    void gen_assign(const ASTNode* node);
    void gen_return(const ASTNode* node);
    void gen_output(const ASTNode* node);

    /* ── Control flow ── */
    void gen_if(const ASTNode* node);
    void gen_while(const ASTNode* node);
    void gen_for(const ASTNode* node);
    void gen_loop(const ASTNode* node);
    void gen_repeat(const ASTNode* node);
    void gen_break();
    void gen_continue();

    /* ── Loop context for break/continue ── */
    struct LoopCtx { std::string cond_bb; std::string exit_bb; };
    std::vector<LoopCtx> loop_stack_;

    /* ── Function translation ── */
    void gen_function(const ASTNode* node);

    /* ── Walk helpers ── */
    void walk(const ASTNode* node);
    void walk_block(const ASTNode* node);

    /* ── Basic block helpers ── */
    IrBasicBlock* add_block(const std::string& name);
    void set_insert(IrBasicBlock* bb);
    void emit_inst(IrInstruction inst);
    bool block_terminated();
    void safe_emit_br(const std::string& target);
};
