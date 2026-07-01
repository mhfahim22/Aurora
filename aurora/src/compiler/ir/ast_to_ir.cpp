#include "compiler/ir/ast_to_ir.hpp"
#include <cassert>

/* ════════════════════════════════════════════════════════════
   AstToIr — Constructor
   ════════════════════════════════════════════════════════════ */

AstToIr::AstToIr() {
}

int32_t AstToIr::i64_type() {
    auto it = type_cache_.find(IrTypeKind::Int64);
    if (it != type_cache_.end()) return it->second;
    return type_cache_[IrTypeKind::Int64] = ir_make_primitive(mod_.type_pool, IrTypeKind::Int64);
}
int32_t AstToIr::f64_type() {
    auto it = type_cache_.find(IrTypeKind::Float64);
    if (it != type_cache_.end()) return it->second;
    return type_cache_[IrTypeKind::Float64] = ir_make_primitive(mod_.type_pool, IrTypeKind::Float64);
}
int32_t AstToIr::i8_type() {
    auto it = type_cache_.find(IrTypeKind::Int8);
    if (it != type_cache_.end()) return it->second;
    return type_cache_[IrTypeKind::Int8] = ir_make_primitive(mod_.type_pool, IrTypeKind::Int8);
}
int32_t AstToIr::i1_type() {
    auto it = type_cache_.find(IrTypeKind::Int1);
    if (it != type_cache_.end()) return it->second;
    return type_cache_[IrTypeKind::Int1] = ir_make_primitive(mod_.type_pool, IrTypeKind::Int1);
}
int32_t AstToIr::ptr_type() {
    auto it = type_cache_.find(IrTypeKind::Ptr);
    if (it != type_cache_.end()) return it->second;
    return type_cache_[IrTypeKind::Ptr] = ir_make_primitive(mod_.type_pool, IrTypeKind::Ptr);
}
int32_t AstToIr::void_type() {
    auto it = type_cache_.find(IrTypeKind::Void);
    if (it != type_cache_.end()) return it->second;
    return type_cache_[IrTypeKind::Void] = ir_make_primitive(mod_.type_pool, IrTypeKind::Void);
}

/* ════════════════════════════════════════════════════════════
   SSA Value Helpers
   ════════════════════════════════════════════════════════════ */

std::string AstToIr::fresh_ssa(const std::string& hint) {
    return "%" + hint + std::to_string(ssa_counter_++);
}

IrValue AstToIr::ssa(const std::string& name, int32_t ty) {
    IrValue v;
    v.name = name;
    v.type_idx = ty;
    v.is_const = false;
    return v;
}

/* ════════════════════════════════════════════════════════════
   Basic Block Helpers
   ════════════════════════════════════════════════════════════ */

IrBasicBlock* AstToIr::add_block(const std::string& name) {
    if (!cur_fn_) return nullptr;
    cur_fn_->blocks.push_back({name, {}});
    cur_fn_->block_order.push_back(name);
    return &cur_fn_->blocks.back();
}

void AstToIr::set_insert(IrBasicBlock* bb) {
    cur_bb_ = bb;
}

void AstToIr::emit_inst(IrInstruction inst) {
    if (cur_bb_) cur_bb_->instructions.push_back(std::move(inst));
}

bool AstToIr::block_terminated() {
    if (!cur_bb_ || cur_bb_->instructions.empty()) return false;
    const auto& last = cur_bb_->instructions.back();
    return std::holds_alternative<IrRet>(last) ||
           std::holds_alternative<IrBr>(last) ||
           std::holds_alternative<IrCondBr>(last);
}

void AstToIr::safe_emit_br(const std::string& target) {
    if (!block_terminated())
        emit_inst(IrBr{target});
}

/* ════════════════════════════════════════════════════════════
   Top-Level Translation
   ════════════════════════════════════════════════════════════ */

IrModule AstToIr::translate(const ASTNode* root) {
    mod_ = IrModule{};
    mod_.name = "__main__";
    ssa_counter_ = 0;
    symbols_.clear();
    type_cache_.clear();
    cur_fn_ = nullptr;
    cur_bb_ = nullptr;

    walk_block(root);

    return std::move(mod_);
}

/* ════════════════════════════════════════════════════════════
   Walk Helpers
   ════════════════════════════════════════════════════════════ */

void AstToIr::walk_block(const ASTNode* node) {
    while (node) {
        walk(node);
        node = node->next.get();
    }
}

void AstToIr::walk(const ASTNode* node) {
    if (!node) return;

    switch (node->type) {

    case NodeType::Function:
    case NodeType::PerformanceFn:
    case NodeType::Lambda:
        gen_function(node);
        break;

    case NodeType::Assign:
        gen_assign(node);
        break;

    case NodeType::Return:
        gen_return(node);
        break;

    case NodeType::Output:
        gen_output(node);
        break;

    case NodeType::If:
        gen_if(node);
        break;

    case NodeType::Else:
        walk_block(node->body.get());
        break;

    case NodeType::While:
        gen_while(node);
        break;

    case NodeType::For:
        gen_for(node);
        break;

    case NodeType::Loop:
        gen_loop(node);
        break;

    case NodeType::Repeat:
        gen_repeat(node);
        break;

    case NodeType::Break:
        gen_break();
        break;

    case NodeType::Continue:
        gen_continue();
        break;

    case NodeType::Skip:
        break;

    case NodeType::Pass:
        break;

    case NodeType::Match: {
        walk(node->left.get());
        const ASTNode* cp = node->args.get();
        while (cp) {
            walk_block(cp->body.get());
            cp = cp->next.get();
        }
        break;
    }

    case NodeType::Num:
    case NodeType::Float:
    case NodeType::Str:
    case NodeType::Var:
    case NodeType::BinOp:
    case NodeType::UnaryOp:
    case NodeType::Call:
        gen_expr(node);
        break;

    case NodeType::Index:
        walk(node->left.get());
        break;

    case NodeType::IndexAssign:
        if (node->left) walk(node->left.get());
        if (node->right) walk(node->right.get());
        break;

    /* Memory management — walk children */
    case NodeType::Move:
    case NodeType::Drop:
    case NodeType::SharedRef:
    case NodeType::WeakRef:
    case NodeType::Borrow:
    case NodeType::Copy:
    case NodeType::Free:
    case NodeType::Delete:
        break;

    /* Classes, structs, enums, etc — skip for IR gen */
    case NodeType::Class:
    case NodeType::StructDecl:
    case NodeType::EnumDecl:
    case NodeType::InterfaceDecl:
    case NodeType::TypeAlias:
    case NodeType::Import:
    case NodeType::NamespaceDecl:
    case NodeType::ModuleDecl:
    case NodeType::PackageDecl:
    case NodeType::AliasDecl:
    case NodeType::ExternFn:
    case NodeType::ExternStruct:
    case NodeType::ExternUnion:
    case NodeType::FunctionType:
    case NodeType::Block:
    case NodeType::Yield:
        break;

    default:
        walk(node->left.get());
        walk(node->right.get());
        walk_block(node->body.get());
        break;
    }
}

/* ════════════════════════════════════════════════════════════
   Function Translation
   ════════════════════════════════════════════════════════════ */

void AstToIr::gen_function(const ASTNode* node) {
    IrFunction fn;
    fn.name = node->value;

    /* Parameters */
    int32_t param_ty = i64_type();
    const ASTNode* p = node->args.get();
    while (p) {
        fn.params.push_back({p->value, param_ty});
        p = p->next.get();
    }
    fn.ret_type_idx = i64_type();

    /* Create entry block */
    fn.blocks.push_back({"entry", {}});
    fn.block_order.push_back("entry");

    mod_.functions.push_back(std::move(fn));
    cur_fn_ = &mod_.functions.back();
    cur_bb_ = &cur_fn_->blocks.back();
    symbols_.clear();

    /* Register parameters as SSA values available in the entry block */
    for (auto& param : cur_fn_->params) {
        symbols_[param.name] = {"%" + param.name, true};
    }

    /* Walk function body */
    walk_block(node->body.get());

    /* Default return if no terminator */
    if (!block_terminated())
        emit_inst(IrRet{ir_const_i64(0)});

    cur_fn_ = nullptr;
    cur_bb_ = nullptr;
}

/* ════════════════════════════════════════════════════════════
   Statement Generators
   ════════════════════════════════════════════════════════════ */

void AstToIr::gen_assign(const ASTNode* node) {
    if (!node->left || !node->right) return;

    const std::string& name = node->left->value;
    IrValue rhs = gen_expr(node->right.get());

    /* If this is a new variable, emit alloca and create symbol entry */
    if (symbols_.find(name) == symbols_.end()) {
        std::string alloca_name = fresh_ssa(name + ".addr");
        emit_inst(IrAlloca{i64_type(), alloca_name});
        symbols_[name] = {alloca_name, false};
    }

    /* Store RHS into variable's alloca */
    emit_inst(IrStore{ssa(symbols_[name].ssa_name, ptr_type()), rhs});
}

void AstToIr::gen_return(const ASTNode* node) {
    IrValue val = ir_const_i64(0);
    if (node->left) val = gen_expr(node->left.get());
    emit_inst(IrRet{val});
}

void AstToIr::gen_output(const ASTNode* node) {
    if (!node->left) return;
    IrValue val = gen_expr(node->left.get());

    /* Declare aurora_print_int */
    bool found = false;
    for (auto& fn : mod_.functions)
        if (fn.name == "aurora_print_int") { found = true; break; }
    if (!found) {
        for (auto& d : mod_.declarations)
            if (d.name == "aurora_print_int") { found = true; break; }
    }
    if (!found) {
        IrFunction print_fn;
        print_fn.name = "aurora_print_int";
        print_fn.ret_type_idx = void_type();
        print_fn.params.push_back({"n", i64_type()});
        mod_.declarations.push_back(std::move(print_fn));
    }

    emit_inst(IrCall{"aurora_print_int", {val}, "", void_type()});
}

/* ════════════════════════════════════════════════════════════
   Control Flow
   ════════════════════════════════════════════════════════════ */

void AstToIr::gen_if(const ASTNode* node) {
    if (!cur_fn_) return;

    IrValue cond = gen_expr(node->left.get());

    std::string then_name = fresh_ssa("if.then");
    std::string else_name = fresh_ssa("if.else");
    std::string merge_name = fresh_ssa("if.merge");
    std::string next_bb_name = (node->next ? fresh_ssa("if.next") : merge_name);

    /* Branch to then/else */
    emit_inst(IrCondBr{cond, then_name, else_name});

    /* Then block */
    add_block(then_name);
    set_insert(&cur_fn_->blocks.back());
    walk_block(node->body.get());
    if (node->orelse || node->next)
        safe_emit_br(merge_name);

    /* Else block */
    add_block(else_name);
    set_insert(&cur_fn_->blocks.back());
    if (node->orelse) {
        if (node->orelse->type == NodeType::If) {
            gen_if(node->orelse.get());
        } else {
            walk_block(node->orelse->body.get());
        }
    }
    if (node->orelse || node->next)
        safe_emit_br(merge_name);

    /* Merge block */
    if (node->orelse || node->next) {
        add_block(merge_name);
        set_insert(&cur_fn_->blocks.back());
    }

    /* Handle chained if-else via next */
    if (node->next) {
        IrBasicBlock* saved_bb = cur_bb_;
        if (node->next->type == NodeType::If) {
            gen_if(node->next.get());
        } else {
            walk(node->next.get());
        }
    }
}

void AstToIr::gen_while(const ASTNode* node) {
    if (!cur_fn_) return;

    std::string cond_name = fresh_ssa("while.cond");
    std::string body_name = fresh_ssa("while.body");
    std::string end_name = fresh_ssa("while.end");

    safe_emit_br(cond_name);

    /* Condition block */
    add_block(cond_name);
    set_insert(&cur_fn_->blocks.back());
    IrValue cond = gen_expr(node->left.get());
    emit_inst(IrCondBr{cond, body_name, end_name});

    /* Body block — push loop context */
    add_block(body_name);
    set_insert(&cur_fn_->blocks.back());
    loop_stack_.push_back({cond_name, end_name});
    walk_block(node->body.get());
    loop_stack_.pop_back();
    if (!block_terminated())
        emit_inst(IrBr{cond_name});

    /* End block */
    add_block(end_name);
    set_insert(&cur_fn_->blocks.back());
}

void AstToIr::gen_for(const ASTNode* node) {
    if (!cur_fn_) return;

    /* for <var> in <expr>: <body>
     * Generates: i from 0 to limit-1, loop-var = i
     */
    const std::string& var_name = node->value;
    std::string idx_name = var_name + ".idx";
    std::string limit_name = fresh_ssa("for.limit");

    IrValue limit = gen_expr(node->left.get());

    /* Allocate index and loop-variable slots */
    std::string idx_alloca = fresh_ssa(var_name + ".idx.addr");
    std::string var_alloca = fresh_ssa(var_name + ".addr");
    emit_inst(IrAlloca{i64_type(), idx_alloca});
    emit_inst(IrAlloca{i64_type(), var_alloca});
    emit_inst(IrStore{ssa(idx_alloca, ptr_type()), ir_const_i64(0)});

    std::string cond_name = fresh_ssa("for.cond");
    std::string body_name = fresh_ssa("for.body");
    std::string inc_name = fresh_ssa("for.inc");
    std::string end_name = fresh_ssa("for.end");

    safe_emit_br(cond_name);

    /* Condition: i < limit */
    add_block(cond_name);
    set_insert(&cur_fn_->blocks.back());
    IrValue cur_idx = ir_ssa(fresh_ssa(var_name + ".cur"), i64_type());
    emit_inst(IrLoad{ssa(idx_alloca, ptr_type()), cur_idx.name, i64_type()});
    std::string cond_result = fresh_ssa("for.cond.val");
    emit_inst(IrICmp{IrCmpPred::SLT, cur_idx, limit, cond_result});
    emit_inst(IrCondBr{ssa(cond_result, i1_type()), body_name, end_name});

    /* Body */
    add_block(body_name);
    set_insert(&cur_fn_->blocks.back());
    loop_stack_.push_back({inc_name, end_name});

    /* loop_var = i */
    {
        IrValue idx_val = ir_ssa(fresh_ssa(var_name + ".cur.body"), i64_type());
        emit_inst(IrLoad{ssa(idx_alloca, ptr_type()), idx_val.name, i64_type()});
        emit_inst(IrStore{ssa(var_alloca, ptr_type()), idx_val});
    }
    symbols_[var_name] = {var_alloca, false};

    walk_block(node->body.get());
    loop_stack_.pop_back();
    safe_emit_br(inc_name);

    /* Increment */
    add_block(inc_name);
    set_insert(&cur_fn_->blocks.back());
    {
        IrValue old = ir_ssa(fresh_ssa(var_name + ".old"), i64_type());
        emit_inst(IrLoad{ssa(idx_alloca, ptr_type()), old.name, i64_type()});
        std::string next_name = fresh_ssa(var_name + ".next");
        IrValue one = ir_const_i64(1);
        emit_inst(IrBinOpInst{IrBinOp::Add, old, one, next_name});
        emit_inst(IrStore{ssa(idx_alloca, ptr_type()), ir_ssa(next_name, i64_type())});
    }
    emit_inst(IrBr{cond_name});

    /* End block */
    add_block(end_name);
    set_insert(&cur_fn_->blocks.back());
}

void AstToIr::gen_loop(const ASTNode* node) {
    if (!cur_fn_) return;

    std::string body_name = fresh_ssa("loop.body");
    std::string end_name = fresh_ssa("loop.end");

    safe_emit_br(body_name);

    add_block(body_name);
    set_insert(&cur_fn_->blocks.back());
    loop_stack_.push_back({body_name, end_name});
    walk_block(node->body.get());
    loop_stack_.pop_back();
    if (!block_terminated())
        emit_inst(IrBr{body_name});

    add_block(end_name);
    set_insert(&cur_fn_->blocks.back());
}

void AstToIr::gen_repeat(const ASTNode* node) {
    if (!cur_fn_ || !node->left) return;

    std::string body_name = fresh_ssa("repeat.body");
    std::string end_name = fresh_ssa("repeat.end");

    safe_emit_br(body_name);

    add_block(body_name);
    set_insert(&cur_fn_->blocks.back());
    loop_stack_.push_back({body_name, end_name});
    walk_block(node->body.get());
    loop_stack_.pop_back();
    IrValue cond = gen_expr(node->left.get());
    emit_inst(IrCondBr{cond, end_name, body_name});

    add_block(end_name);
    set_insert(&cur_fn_->blocks.back());
}

void AstToIr::gen_break() {
    if (loop_stack_.empty()) return;
    safe_emit_br(loop_stack_.back().exit_bb);
}

void AstToIr::gen_continue() {
    if (loop_stack_.empty()) return;
    safe_emit_br(loop_stack_.back().cond_bb);
}

/* ════════════════════════════════════════════════════════════
   Expression Generators
   ════════════════════════════════════════════════════════════ */

IrValue AstToIr::gen_expr(const ASTNode* node) {
    if (!node) return ir_const_i64(0);
    switch (node->type) {
        case NodeType::Num:     return gen_num(node);
        case NodeType::Float:   return gen_float(node);
        case NodeType::Str:     return gen_str(node);
        case NodeType::Var:     return gen_var(node);
        case NodeType::BinOp:   return gen_binop(node);
        case NodeType::UnaryOp: return gen_unary(node);
        case NodeType::Call:    return gen_call(node);
        default:
            return ir_const_i64(0);
    }
}

IrValue AstToIr::gen_num(const ASTNode* node) {
    IrValue v;
    v.name = std::to_string(std::stoll(node->value));
    v.type_idx = i64_type();
    v.is_const = true;
    v.set_i64(std::stoll(node->value));
    return v;
}

IrValue AstToIr::gen_float(const ASTNode* node) {
    IrValue v;
    v.name = std::to_string(std::stod(node->value));
    v.type_idx = f64_type();
    v.is_const = true;
    v.set_f64(std::stod(node->value));
    return v;
}

IrValue AstToIr::gen_str(const ASTNode* node) {
    int32_t sidx = mod_.add_string(node->value);
    std::string result = fresh_ssa("str");
    emit_inst(IrStrLiteral{sidx, result});
    return ssa(result, ptr_type());
}

IrValue AstToIr::gen_var(const ASTNode* node) {
    const std::string& name = node->value;

    if (symbols_.find(name) == symbols_.end()) {
        std::string alloca_name = fresh_ssa(name + ".addr");
        emit_inst(IrAlloca{i64_type(), alloca_name});
        symbols_[name] = {alloca_name, false};
    }

    const SymInfo& info = symbols_[name];
    if (info.is_param) {
        return ssa(info.ssa_name, i64_type());
    }

    std::string result = fresh_ssa(name);
    emit_inst(IrLoad{ssa(info.ssa_name, ptr_type()), result, i64_type()});
    return ssa(result, i64_type());
}

IrValue AstToIr::gen_binop(const ASTNode* node) {
    if (!node->left || !node->right) return ir_const_i64(0);

    IrValue lhs = gen_expr(node->left.get());
    IrValue rhs = gen_expr(node->right.get());
    const std::string& op = node->value;

    std::string result = fresh_ssa("binop");

    /* Map string ops to IrBinOp */
    if (op == "+") {
        emit_inst(IrBinOpInst{IrBinOp::Add, lhs, rhs, result});
    } else if (op == "-") {
        emit_inst(IrBinOpInst{IrBinOp::Sub, lhs, rhs, result});
    } else if (op == "*") {
        emit_inst(IrBinOpInst{IrBinOp::Mul, lhs, rhs, result});
    } else if (op == "/") {
        emit_inst(IrBinOpInst{IrBinOp::SDiv, lhs, rhs, result});
    } else if (op == "%") {
        emit_inst(IrBinOpInst{IrBinOp::SRem, lhs, rhs, result});
    } else if (op == "&") {
        emit_inst(IrBinOpInst{IrBinOp::And, lhs, rhs, result});
    } else if (op == "|") {
        emit_inst(IrBinOpInst{IrBinOp::Or, lhs, rhs, result});
    } else if (op == "^") {
        emit_inst(IrBinOpInst{IrBinOp::Xor, lhs, rhs, result});
    } else if (op == "<<") {
        emit_inst(IrBinOpInst{IrBinOp::Shl, lhs, rhs, result});
    } else if (op == ">>") {
        emit_inst(IrBinOpInst{IrBinOp::AShr, lhs, rhs, result});
    } else if (op == "==") {
        emit_inst(IrICmp{IrCmpPred::EQ, lhs, rhs, result});
    } else if (op == "!=") {
        emit_inst(IrICmp{IrCmpPred::NE, lhs, rhs, result});
    } else if (op == "<") {
        emit_inst(IrICmp{IrCmpPred::SLT, lhs, rhs, result});
    } else if (op == ">") {
        emit_inst(IrICmp{IrCmpPred::SGT, lhs, rhs, result});
    } else if (op == "<=") {
        emit_inst(IrICmp{IrCmpPred::SLE, lhs, rhs, result});
    } else if (op == ">=") {
        emit_inst(IrICmp{IrCmpPred::SGE, lhs, rhs, result});
    } else {
        return ir_const_i64(0);
    }

    return ssa(result, i64_type());
}

IrValue AstToIr::gen_unary(const ASTNode* node) {
    if (!node->left) return ir_const_i64(0);
    IrValue val = gen_expr(node->left.get());
    const std::string& op = node->value;

    if (op == "-") {
        std::string result = fresh_ssa("neg");
        IrValue zero = ir_const_i64(0);
        emit_inst(IrBinOpInst{IrBinOp::Sub, zero, val, result});
        return ssa(result, i64_type());
    }
    if (op == "~") {
        std::string result = fresh_ssa("not");
        IrValue minus_one = ir_const_i64(-1);
        emit_inst(IrBinOpInst{IrBinOp::Xor, val, minus_one, result});
        return ssa(result, i64_type());
    }
    if (op == "!") {
        std::string result = fresh_ssa("lnot");
        IrValue zero = ir_const_i64(0);
        emit_inst(IrICmp{IrCmpPred::EQ, val, zero, result});
        return ssa(result, i64_type());
    }

    return val;
}

IrValue AstToIr::gen_call(const ASTNode* node) {
    const std::string& callee = node->value;

    /* Map known builtins to runtime names */
    std::string target = callee;
    bool is_void = false;
    if (callee == "print") { target = "aurora_print_int"; is_void = true; }

    std::vector<IrValue> args;
    const ASTNode* arg = node->args.get();
    while (arg) {
        args.push_back(gen_expr(arg));
        arg = arg->next.get();
    }

    /* Ensure the callee function exists in the module (as declaration, not definition) */
    int32_t ret_ty = is_void ? void_type() : i64_type();
    auto* fn = mod_.get_function(target);
    if (!fn) {
        bool already_declared = false;
        for (const auto& d : mod_.declarations)
            if (d.name == target) { already_declared = true; break; }
        if (!already_declared) {
            IrFunction decl;
            decl.name = target;
            decl.ret_type_idx = ret_ty;
            for (size_t i = 0; i < args.size(); i++)
                decl.params.push_back({"p" + std::to_string(i), i64_type()});
            mod_.declarations.push_back(std::move(decl));
        }
    }

    if (is_void) {
        emit_inst(IrCall{target, args, "", void_type()});
        return ir_const_i64(0);
    }

    std::string result = fresh_ssa(target);
    emit_inst(IrCall{target, args, result, i64_type()});
    return ssa(result, i64_type());
}
