#include "compiler/ir/ir.hpp"
#include <sstream>

/* ════════════════════════════════════════════════════════════
   Type pool helpers
   ════════════════════════════════════════════════════════════ */

int32_t ir_type_pool_add(std::vector<IrType>& pool, IrType t) {
    int32_t idx = static_cast<int32_t>(pool.size());
    pool.push_back(std::move(t));
    return idx;
}

int32_t ir_make_ptr(std::vector<IrType>& pool, int32_t elem_idx) {
    IrType t;
    t.kind = IrTypeKind::Ptr;
    t.child_idx = elem_idx;
    return ir_type_pool_add(pool, std::move(t));
}

int32_t ir_make_array(std::vector<IrType>& pool, int32_t elem_idx, int64_t size) {
    IrType t;
    t.kind = IrTypeKind::Array;
    t.child_idx = elem_idx;
    t.array_size = size;
    return ir_type_pool_add(pool, std::move(t));
}

int32_t ir_make_fn(std::vector<IrType>& pool, int32_t ret_idx, std::vector<int32_t> param_idxs) {
    IrType t;
    t.kind = IrTypeKind::Func;
    t.ret_idx = ret_idx;
    t.param_idxs = std::move(param_idxs);
    return ir_type_pool_add(pool, std::move(t));
}

int32_t ir_make_struct(std::vector<IrType>& pool, const std::string& name, std::vector<int32_t> field_idxs) {
    IrType t;
    t.kind = IrTypeKind::Struct;
    t.name = name;
    t.field_idxs = std::move(field_idxs);
    return ir_type_pool_add(pool, std::move(t));
}

/* Simple linear-scan deduplication for primitive types.
   Callers (e.g. AstToIr) maintain a type_cache_ to avoid repeated O(n) scans. */
int32_t ir_make_primitive(std::vector<IrType>& pool, IrTypeKind kind) {
    for (int32_t i = 0; i < static_cast<int32_t>(pool.size()); i++)
        if (pool[i].kind == kind) return i;
    IrType t;
    t.kind = kind;
    return ir_type_pool_add(pool, std::move(t));
}

IrValue ir_const_i64(int64_t v) {
    IrValue val;
    val.is_const = true;
    val.set_i64(v);
    return val;
}

IrValue ir_const_i32(int32_t v) {
    IrValue val;
    val.is_const = true;
    val.set_i64(v);
    return val;
}

IrValue ir_const_f64(double v) {
    IrValue val;
    val.is_const = true;
    val.set_f64(v);
    return val;
}

IrValue ir_ssa(const std::string& name, int32_t type_idx) {
    IrValue val;
    val.name = name;
    val.type_idx = type_idx;
    val.is_const = false;
    val.set_i64(0);
    return val;
}

/* ════════════════════════════════════════════════════════════
   IrModule member functions
   ════════════════════════════════════════════════════════════ */

IrFunction* IrModule::get_function(const std::string& name) {
    for (auto& fn : functions)
        if (fn.name == name) return &fn;
    for (auto& fn : declarations)
        if (fn.name == name) return &fn;
    return nullptr;
}

const IrFunction* IrModule::get_function(const std::string& name) const {
    for (const auto& fn : functions)
        if (fn.name == name) return &fn;
    for (const auto& fn : declarations)
        if (fn.name == name) return &fn;
    return nullptr;
}

IrBasicBlock* IrModule::get_block(const IrFunction& fn, const std::string& name) const {
    for (const auto& bb : fn.blocks)
        if (bb.name == name) return const_cast<IrBasicBlock*>(&bb);
    return nullptr;
}

int32_t IrModule::add_string(const std::string& s) {
    int32_t idx = static_cast<int32_t>(string_pool.size());
    for (int32_t i = 0; i < static_cast<int32_t>(string_pool.size()); i++)
        if (string_pool[i] == s) return i;
    string_pool.push_back(s);
    return idx;
}

/* ════════════════════════════════════════════════════════════
   Printing
   ════════════════════════════════════════════════════════════ */

static const char* kind_name(IrTypeKind k) {
    switch (k) {
        case IrTypeKind::Void: return "void";
        case IrTypeKind::Int1: return "i1";
        case IrTypeKind::Int8: return "i8";
        case IrTypeKind::Int32: return "i32";
        case IrTypeKind::Int64: return "i64";
        case IrTypeKind::Float32: return "f32";
        case IrTypeKind::Float64: return "f64";
        case IrTypeKind::Ptr: return "ptr";
        case IrTypeKind::Struct: return "struct";
        case IrTypeKind::Array: return "array";
        case IrTypeKind::Func: return "func";
    }
    return "?";
}

static std::string type_str(const IrType& t, const std::vector<IrType>& pool) {
    switch (t.kind) {
        case IrTypeKind::Void: case IrTypeKind::Int1: case IrTypeKind::Int8:
        case IrTypeKind::Int32: case IrTypeKind::Int64:
        case IrTypeKind::Float32: case IrTypeKind::Float64:
            return kind_name(t.kind);
        case IrTypeKind::Ptr: {
            if (t.child_idx >= 0 && static_cast<size_t>(t.child_idx) < pool.size())
                return type_str(pool[t.child_idx], pool) + "*";
            return "void*";
        }
        case IrTypeKind::Struct: {
            if (!t.name.empty()) return "%" + t.name;
            std::string s = "{ ";
            for (size_t i = 0; i < t.field_idxs.size(); i++) {
                if (i > 0) s += ", ";
                int32_t fi = t.field_idxs[i];
                s += (fi >= 0 && static_cast<size_t>(fi) < pool.size()) ? type_str(pool[fi], pool) : "?";
            }
            return s + " }";
        }
        case IrTypeKind::Array: {
            std::string s = "[" + std::to_string(t.array_size) + " x ";
            int32_t ci = t.child_idx;
            s += (ci >= 0 && static_cast<size_t>(ci) < pool.size()) ? type_str(pool[ci], pool) : "?";
            return s + "]";
        }
        case IrTypeKind::Func: {
            std::string s = "(";
            for (size_t i = 0; i < t.param_idxs.size(); i++) {
                if (i > 0) s += ", ";
                int32_t pi = t.param_idxs[i];
                s += (pi >= 0 && static_cast<size_t>(pi) < pool.size()) ? type_str(pool[pi], pool) : "?";
            }
            int32_t ri = t.ret_idx;
            s += ") -> ";
            s += (ri >= 0 && static_cast<size_t>(ri) < pool.size()) ? type_str(pool[ri], pool) : "?";
            return s;
        }
    }
    return "?";
}

static std::string val_str(const IrValue& v, const std::vector<IrType>& pool) {
    std::string t;
    if (v.type_idx >= 0 && static_cast<size_t>(v.type_idx) < pool.size())
        t = type_str(pool[v.type_idx], pool) + " ";
    if (v.is_const) {
        return t + std::to_string(v.i64());
    }
    return t + "%" + v.name;
}

static const char* binop_str(IrBinOp op) {
    switch (op) {
        case IrBinOp::Add: return "add"; case IrBinOp::Sub: return "sub";
        case IrBinOp::Mul: return "mul"; case IrBinOp::SDiv: return "sdiv";
        case IrBinOp::UDiv: return "udiv"; case IrBinOp::SRem: return "srem";
        case IrBinOp::URem: return "urem"; case IrBinOp::And: return "and";
        case IrBinOp::Or: return "or"; case IrBinOp::Xor: return "xor";
        case IrBinOp::Shl: return "shl"; case IrBinOp::AShr: return "ashr";
        case IrBinOp::LShr: return "lshr"; case IrBinOp::FAdd: return "fadd";
        case IrBinOp::FSub: return "fsub"; case IrBinOp::FMul: return "fmul";
        case IrBinOp::FDiv: return "fdiv"; case IrBinOp::FRem: return "frem";
    }
    return "?";
}

static const char* pred_str(IrCmpPred p) {
    switch (p) {
        case IrCmpPred::EQ: return "eq"; case IrCmpPred::NE: return "ne";
        case IrCmpPred::SLT: return "slt"; case IrCmpPred::SGT: return "sgt";
        case IrCmpPred::SLE: return "sle"; case IrCmpPred::SGE: return "sge";
        case IrCmpPred::ULT: return "ult"; case IrCmpPred::UGT: return "ugt";
        case IrCmpPred::ULE: return "ule"; case IrCmpPred::UGE: return "uge";
    }
    return "?";
}

static void print_inst(std::ostream& os, const IrInstruction& inst,
                       const std::vector<IrType>& pool, const std::string& indent) {
    std::visit([&](const auto& i) {
        using T = std::decay_t<decltype(i)>;
        if constexpr (std::is_same_v<T, IrAlloca>) {
            os << indent << "%" << i.result_name << " = alloca "
               << ((i.type_idx >= 0 && static_cast<size_t>(i.type_idx) < pool.size())
                   ? type_str(pool[i.type_idx], pool) : "?") << "\n";
        } else if constexpr (std::is_same_v<T, IrLoad>) {
            std::string ty;
            if (i.loaded_type >= 0 && static_cast<size_t>(i.loaded_type) < pool.size())
                ty = type_str(pool[i.loaded_type], pool);
            os << indent << "%" << i.result_name << " = load " << ty << ", " << val_str(i.ptr, pool) << "\n";
        } else if constexpr (std::is_same_v<T, IrStore>) {
            os << indent << "store " << val_str(i.value, pool) << ", " << val_str(i.ptr, pool) << "\n";
        } else if constexpr (std::is_same_v<T, IrBinOpInst>) {
            os << indent << "%" << i.result_name << " = " << binop_str(i.op)
               << " " << val_str(i.lhs, pool) << ", " << val_str(i.rhs, pool) << "\n";
        } else if constexpr (std::is_same_v<T, IrICmp>) {
            os << indent << "%" << i.result_name << " = icmp " << pred_str(i.pred)
               << " " << val_str(i.lhs, pool) << ", " << val_str(i.rhs, pool) << "\n";
        } else if constexpr (std::is_same_v<T, IrCall>) {
            os << indent;
            if (!i.result_name.empty()) {
                std::string rt;
                if (i.ret_type_idx >= 0 && static_cast<size_t>(i.ret_type_idx) < pool.size())
                    rt = type_str(pool[i.ret_type_idx], pool) + " ";
                os << "%" << i.result_name << " = call " << rt;
            } else {
                os << "call ";
            }
            os << "@" << i.callee << "(";
            for (size_t j = 0; j < i.args.size(); j++) {
                if (j > 0) os << ", ";
                os << val_str(i.args[j], pool);
            }
            os << ")\n";
        } else if constexpr (std::is_same_v<T, IrRet>) {
            os << indent << "ret " << val_str(i.value, pool) << "\n";
        } else if constexpr (std::is_same_v<T, IrBr>) {
            os << indent << "br label %" << i.target << "\n";
        } else if constexpr (std::is_same_v<T, IrCondBr>) {
            os << indent << "br " << val_str(i.cond, pool) << ", label %" << i.true_bb << ", label %" << i.false_bb << "\n";
        } else if constexpr (std::is_same_v<T, IrPhi>) {
            std::string pt;
            if (i.type_idx >= 0 && static_cast<size_t>(i.type_idx) < pool.size())
                pt = type_str(pool[i.type_idx], pool) + " ";
            os << indent << "%" << i.result_name << " = phi " << pt << "[ ";
            for (size_t j = 0; j < i.incoming.size(); j++) {
                if (j > 0) os << ", ";
                os << "%" << i.incoming[j].first << ", %" << i.incoming[j].second;
            }
            os << " ]\n";
        } else if constexpr (std::is_same_v<T, IrGEP>) {
            os << indent << "%" << i.result_name << " = getelementptr " << val_str(i.ptr, pool);
            for (const auto& idx : i.indices)
                os << ", " << val_str(idx, pool);
            os << "\n";
        } else if constexpr (std::is_same_v<T, IrBitCast>) {
            os << indent << "%" << i.result_name << " = bitcast " << val_str(i.value, pool)
               << " to " << ((i.target_type >= 0 && static_cast<size_t>(i.target_type) < pool.size())
                   ? type_str(pool[i.target_type], pool) : "?") << "\n";
        } else if constexpr (std::is_same_v<T, IrStrLiteral>) {
            os << indent << "%" << i.result_name << " = strlit " << i.string_idx << "\n";
        }
    }, inst);
}

std::string IrModule::to_string() const {
    std::ostringstream os;
    os << "; Aurora IR Module: " << name << "\n\n";

    if (!string_pool.empty()) {
        os << "; String pool:\n";
        for (size_t i = 0; i < string_pool.size(); i++)
            os << ";   " << i << ": \"" << string_pool[i] << "\"\n";
        os << "\n";
    }

    for (const auto& g : globals) {
        os << "global @" << g.name
           << " : " << ((g.type_idx >= 0 && static_cast<size_t>(g.type_idx) < type_pool.size())
               ? type_str(type_pool[g.type_idx], type_pool) : "?");
        if (g.is_constant) os << " (const)";
        os << "\n";
    }
    if (!globals.empty()) os << "\n";

    for (const auto& fn : functions) {
        os << "define @" << fn.name << "(";
        for (size_t i = 0; i < fn.params.size(); i++) {
            if (i > 0) os << ", ";
            int32_t ti = fn.params[i].type_idx;
            os << ((ti >= 0 && static_cast<size_t>(ti) < type_pool.size())
                   ? type_str(type_pool[ti], type_pool) : "?")
               << " %" << fn.params[i].name;
        }
        os << ") {\n";

        for (const auto& bb_name : fn.block_order) {
            auto* bb = get_block(fn, bb_name);
            if (!bb) continue;
            os << bb_name << ":\n";
            for (const auto& inst : bb->instructions)
                print_inst(os, inst, type_pool, "  ");
        }
        os << "}\n\n";
    }

    return os.str();
}