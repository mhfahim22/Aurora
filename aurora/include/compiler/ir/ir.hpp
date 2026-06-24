#pragma once
#include <string>
#include <vector>
#include <deque>
#include <memory>
#include <unordered_map>
#include <variant>
#include <cstdint>

/* ════════════════════════════════════════════════════════════
   Aurora IR — Standalone Intermediate Representation
   ════════════════════════════════════════════════════════════
   Lightweight SSA-based IR for Aurora-specific optimizations.
   Lowered to LLVM IR for final code generation.
   ════════════════════════════════════════════════════════════ */

/* ── Type kinds ── */
enum class IrTypeKind : uint8_t {
    Void, Int1, Int8, Int32, Int64,
    Float32, Float64, Ptr, Struct, Array, Func
};

/* ── Type — flat, non-recursive (uses indices for children) ── */
struct IrType {
    IrTypeKind kind{IrTypeKind::Void};
    std::string name;                /* struct/alias name */
    int64_t array_size{0};
    bool is_const{false};
    int32_t child_idx{-1};           /* ptr_element / array_element */
    int32_t ret_idx{-1};             /* func return type */
    std::vector<int32_t> param_idxs; /* func param types */
    std::vector<int32_t> field_idxs; /* struct field types */
};

/* ── Value ── */
struct IrValue {
    std::string name;
    int32_t type_idx{-1};
    bool is_const{false};
    std::variant<int64_t, double> data{int64_t{0}};

    int64_t i64() const { return std::get<int64_t>(data); }
    double  f64() const { return std::get<double>(data); }
    void set_i64(int64_t v) { data = v; }
    void set_f64(double v)  { data = v; }
};

/* ── Binary operators ── */
enum class IrBinOp : uint8_t {
    Add, Sub, Mul, SDiv, UDiv, SRem, URem,
    And, Or, Xor, Shl, AShr, LShr,
    FAdd, FSub, FMul, FDiv, FRem
};

/* ── Comparison predicates ── */
enum class IrCmpPred : uint8_t {
    EQ, NE, SLT, SGT, SLE, SGE, ULT, UGT, ULE, UGE
};

/* ── Instruction kinds ── */
struct IrAlloca     { int32_t type_idx{-1}; std::string result_name; };
struct IrLoad       { IrValue ptr; std::string result_name; int32_t loaded_type{-1}; };
struct IrStore      { IrValue ptr; IrValue value; };
struct IrBinOpInst  { IrBinOp op; IrValue lhs, rhs; std::string result_name; };
struct IrICmp       { IrCmpPred pred; IrValue lhs, rhs; std::string result_name; };
struct IrCall       { std::string callee; std::vector<IrValue> args; std::string result_name; int32_t ret_type_idx{-1}; };
struct IrRet        { IrValue value; };
struct IrBr         { std::string target; };
struct IrCondBr     { IrValue cond; std::string true_bb, false_bb; };
struct IrPhi        { std::vector<std::pair<std::string,std::string>> incoming; int32_t type_idx{-1}; std::string result_name; };
struct IrGEP        { IrValue ptr; std::vector<IrValue> indices; std::string result_name; };
struct IrBitCast    { IrValue value; int32_t target_type{-1}; std::string result_name; };
struct IrStrLiteral { int32_t string_idx{-1}; std::string result_name; };

using IrInstruction = std::variant<
    IrAlloca, IrLoad, IrStore, IrBinOpInst, IrICmp,
    IrCall, IrRet, IrBr, IrCondBr, IrPhi, IrGEP, IrBitCast, IrStrLiteral
>;

/* ── Basic block ── */
struct IrBasicBlock {
    std::string name;
    std::vector<IrInstruction> instructions;
};

/* ── Function parameter ── */
struct IrParam {
    std::string name;
    int32_t type_idx{-1};
};

/* ── Function ── */
struct IrFunction {
    std::string name;
    std::vector<IrParam> params;
    int32_t ret_type_idx{-1};
    mutable std::deque<IrBasicBlock> blocks;
    std::vector<std::string> block_order;
};

/* ── Global variable ── */
struct IrGlobal {
    std::string name;
    int32_t type_idx{-1};
    bool is_constant{false};
};

/* ── Type pool helper functions ── */
int32_t ir_type_pool_add(std::vector<IrType>& pool, IrType t);
int32_t ir_make_ptr(std::vector<IrType>& pool, int32_t elem_idx);
int32_t ir_make_array(std::vector<IrType>& pool, int32_t elem_idx, int64_t size);
int32_t ir_make_fn(std::vector<IrType>& pool, int32_t ret_idx, std::vector<int32_t> param_idxs);
int32_t ir_make_struct(std::vector<IrType>& pool, const std::string& name, std::vector<int32_t> field_idxs);
int32_t ir_make_primitive(std::vector<IrType>& pool, IrTypeKind kind);

IrValue ir_const_i64(int64_t v);
IrValue ir_const_i32(int32_t v);
IrValue ir_const_f64(double v);
IrValue ir_ssa(const std::string& name, int32_t type_idx);

/* ── Module ── */
struct IrModule {
    std::string name;
    std::vector<IrType> type_pool;
    std::deque<IrFunction> functions;
    std::vector<IrFunction> declarations; /* external function declarations (no body) */
    std::vector<IrGlobal> globals;
    std::vector<std::string> string_pool;

    IrFunction* get_function(const std::string& name);
    const IrFunction* get_function(const std::string& name) const;
    IrBasicBlock* get_block(const IrFunction& fn, const std::string& name) const;
    std::string to_string() const;
    int32_t add_string(const std::string& s);
};
