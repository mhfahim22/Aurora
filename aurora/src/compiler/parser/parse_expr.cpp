#include "compiler/parser.hpp"
#include <stdexcept>
#include <sstream>

/* ── Helper: consistent error for missing closing delimiter ── */
[[noreturn]] static void throw_missing_close(int line, int col, char expected, const char* context) {
    const char* hints[] = {
        "every opening '(' needs a matching ')'",
        "every opening '[' needs a matching ']'",
        "every opening '{' needs a matching '}'",
    };
    const char* hint = (expected == ')') ? hints[0] :
                        (expected == ']') ? hints[1] : hints[2];
    std::string msg = "missing '" + std::string(1, expected) + "' to close " + context;
    throw std::runtime_error(format_error_with_hint(line, col, msg, hint));
}

/* ════════════════════════════════════════════════════════════
   Helper: parse trailing chains (.field, .method(), [idx], ())
   ════════════════════════════════════════════════════════════
   After parsing an atom (identifier, call result, etc.), continue
   to consume any chained member access, method calls, or indexing.
   This enables: a.b.c, a.b(), a().b, a().b(), a[0].field, etc. */
ASTNode::Ptr Parser::parse_trailing_chains(ASTNode::Ptr base,
                                           const std::vector<Token>& toks,
                                           int& idx, int cnt) {
    while (idx < cnt) {
        /* ── .field or .method(args) ── */
        if (toks[idx].is_operator('.')) {
            idx++;
            if (idx >= cnt || !(toks[idx].is_identifier() || toks[idx].is(TokenType::Keyword))) {
                std::ostringstream msg;
                msg << "Line " << toks[idx-1].line << ": attribute access needs a field name";
                throw std::runtime_error(msg.str());
            }
            std::string field = toks[idx++].value;
            int src_ln = toks[idx-2].line;

            /* method call: obj.method(args) */
            if (idx < cnt && toks[idx].is_operator('(')) {
                idx++;
                /* Build qualified name from base chain: collect all segments */
                std::string call_name;
                {
                    /* Walk the base chain to reconstruct a dotted qualified name */
                    std::vector<std::string> segs;
                    const ASTNode* cur = base.get();
                    while (cur) {
                        if (cur->type == NodeType::Var || cur->type == NodeType::Call ||
                            cur->type == NodeType::Attribute) {
                            segs.push_back(cur->value);
                        }
                        cur = cur->left.get();
                    }
                    /* Reverse: innermost first */
                    for (auto it = segs.rbegin(); it != segs.rend(); ++it)
                        call_name += (it == segs.rbegin() ? "" : ".") + *it;
                    if (!call_name.empty()) call_name += ".";
                    call_name += field;
                }

                auto call  = make_node(NodeType::Call, call_name, src_ln);
                call->left = std::move(base);
                ASTNode* tail = nullptr;
                while (idx < cnt && !toks[idx].is_operator(')')) {
                    if (toks[idx].is_operator(',')) { idx++; continue; }
                    auto arg = parse_expr(toks, idx);
                    ASTNode* raw = arg.get();
                    if (!call->args) { call->args = std::move(arg); tail = raw; }
                    else             { tail->next = std::move(arg); tail = raw; }
                }
                if (idx < cnt) idx++;
                else throw_missing_close(src_ln, 0, ')', "method call");
                base = std::move(call);
                continue;
            }

            /* plain field access: obj.field */
            auto node  = make_node(NodeType::Attribute, field, src_ln);
            node->left = std::move(base);
            base = std::move(node);
            continue;
        }

        /* ── [idx] indexing ── */
        if (toks[idx].is_operator('[')) {
            idx++;
            int src_ln = toks[idx-1].line;
            auto index_expr = parse_expr(toks, idx);
            if (idx < cnt && toks[idx].is_operator(']')) idx++;
            else throw_missing_close(src_ln, 0, ']', "index expression");
            /* Create Index node with base as right child */
            auto idx_node = make_node(NodeType::Index, "", src_ln);
            idx_node->left = std::move(index_expr);
            idx_node->right = std::move(base);
            base = std::move(idx_node);
            continue;
        }

        break;  /* no more chains */
    }
    return base;
}

ASTNode::Ptr Parser::parse_factor(const std::vector<Token>& toks, int& idx) {
    int cnt = (int)toks.size();
    if (idx >= cnt) return make_node(NodeType::Num, "0");

    const Token& t = toks[idx];

    /* grouped or tuple */
    if (t.is_operator('(')) {
        int start_idx = idx;
        idx++;
        auto inner = parse_expr(toks, idx);

        /* If followed by comma, it's a tuple literal */
        if (idx < cnt && toks[idx].is_operator(',')) {
            auto tup = make_node(NodeType::Array, "__tuple__", t.line);
            ASTNode* raw = inner.get();
            tup->args = std::move(inner);
            ASTNode* tail = raw;

            while (idx < cnt && toks[idx].is_operator(',')) {
                idx++;
                if (idx < cnt && !toks[idx].is_operator(')')) {
                    auto elem = parse_expr(toks, idx);
                    ASTNode* eraw = elem.get();
                    tail->next = std::move(elem);
                    tail = eraw;
                }
            }

            if (idx < cnt && toks[idx].is_operator(')')) idx++;
            else throw_missing_close(t.line, t.col, ')', "tuple literal");
            return tup;
        }

        if (idx < cnt && toks[idx].is_operator(')')) idx++;
        else throw_missing_close(t.line, t.col, ')', "grouped expression");
        return inner;
    }

    /* array literal */
    if (t.is_operator('[')) {
        idx++;
        auto arr  = make_node(NodeType::Array, "");
        ASTNode* tail = nullptr;
        while (idx < cnt && !toks[idx].is_operator(']')) {
            if (toks[idx].is_operator(',')) { idx++; continue; }
            auto elem = parse_expr(toks, idx);
            ASTNode* raw = elem.get();
            if (!arr->args) { arr->args = std::move(elem); tail = raw; }
            else            { tail->next = std::move(elem); tail = raw; }
        }
        if (idx < cnt) idx++;
        else throw_missing_close(t.line, t.col, ']', "array literal");
        return arr;
    }

    /* string */
    if (t.is_string()) { idx++; return make_node(NodeType::Str, t.value, t.line); }

    /* float */
    if (t.is(TokenType::Float)) { idx++; return make_node(NodeType::Float, t.value, t.line); }

    /* number */
    if (t.is_number()) { idx++; return make_node(NodeType::Num, t.value, t.line); }

    /* boolean & null keywords */
    if (t.is_keyword("true"))  { idx++; return make_node(NodeType::Num, "1", t.line); }
    if (t.is_keyword("false")) { idx++; return make_node(NodeType::Num, "0", t.line); }
    if (t.is_keyword("null"))  { idx++; return make_node(NodeType::Num, "0", t.line); }

    /* ── new Constructor(args) ── */
    if (t.is_keyword("new")) {
        int src_ln = t.line; idx++;
        if (idx >= cnt || !(toks[idx].is_identifier() || toks[idx].is(TokenType::Keyword))) {
            std::ostringstream msg;
            msg << "Line " << src_ln << ": expected class name after 'new'";
            throw std::runtime_error(msg.str());
        }
        std::string class_name = toks[idx++].value;

        auto call = make_node(NodeType::New, class_name, src_ln);
        ASTNode* tail = nullptr;

        if (idx < cnt && toks[idx].is_operator('(')) {
            idx++;
            while (idx < cnt && !toks[idx].is_operator(')')) {
                if (toks[idx].is_operator(',')) { idx++; continue; }
                auto arg = parse_expr(toks, idx);
                ASTNode* raw = arg.get();
                if (!call->args) { call->args = std::move(arg); tail = raw; }
                else             { tail->next = std::move(arg); tail = raw; }
            }
            if (idx < cnt) idx++;
            else {
                std::ostringstream msg;
                msg << "Line " << src_ln << ": missing ')' in constructor call";
                throw std::runtime_error(msg.str());
            }
        }

        /* Handle trailing chains on the constructor result */
        return parse_trailing_chains(std::move(call), toks, idx, cnt);
    }

    /* ── Phase 2: memory keywords usable inside expressions ── */
    /* Helper: check if the next token is '(' (function-call-like) */
    auto next_is_open_paren = [&]() -> bool {
        return idx + 1 < cnt && toks[idx + 1].is_operator('(');
    };

    if (t.is_keyword("move")) {
        if (next_is_open_paren())
            ; /* treat as function call, fall through to phase 3 */
        else {
            int src_ln = t.line; idx++;
            if (idx >= cnt || !(toks[idx].is_identifier() || toks[idx].is(TokenType::Keyword)))
                throw std::runtime_error("Line " + std::to_string(src_ln) + ": move needs a variable name");
            std::string var = toks[idx++].value;
            return make_node(NodeType::Move, var, src_ln);
        }
    }
    if (t.is_keyword("shared")) {
        int src_ln = t.line; idx++;
        if (idx >= cnt || !(toks[idx].is_identifier() || toks[idx].is(TokenType::Keyword)))
            throw std::runtime_error("Line " + std::to_string(src_ln) + ": shared needs a variable name");
        std::string var = toks[idx++].value;
        return make_node(NodeType::SharedRef, var, src_ln);
    }
    if (t.is_keyword("weak")) {
        int src_ln = t.line; idx++;
        if (idx >= cnt || !(toks[idx].is_identifier() || toks[idx].is(TokenType::Keyword)))
            throw std::runtime_error("Line " + std::to_string(src_ln) + ": weak needs a variable name");
        std::string var = toks[idx++].value;
        return make_node(NodeType::WeakRef, var, src_ln);
    }
    if (t.is_keyword("borrow")) {
        int src_ln = t.line; idx++;
        if (idx >= cnt || !(toks[idx].is_identifier() || toks[idx].is(TokenType::Keyword)))
            throw std::runtime_error("Line " + std::to_string(src_ln) + ": borrow needs a variable name");
        std::string var = toks[idx++].value;
        return make_node(NodeType::Borrow, var, src_ln);
    }
    if (t.is_keyword("copy")) {
        if (next_is_open_paren())
            ; /* treat as function call, fall through to phase 3 */
        else {
            int src_ln = t.line; idx++;
            if (idx >= cnt || !(toks[idx].is_identifier() || toks[idx].is(TokenType::Keyword)))
                throw std::runtime_error("Line " + std::to_string(src_ln) + ": copy needs a variable name");
            std::string var = toks[idx++].value;
            return make_node(NodeType::Copy, var, src_ln);
        }
    }
    if (t.is_keyword("reference")) {
        int src_ln = t.line; idx++;
        if (idx >= cnt || !(toks[idx].is_identifier() || toks[idx].is(TokenType::Keyword)))
            throw std::runtime_error("Line " + std::to_string(src_ln) + ": reference needs a variable name");
        std::string var = toks[idx++].value;
        return make_node(NodeType::Reference, var, src_ln);
    }
    if (t.is_keyword("pointer")) {
        int src_ln = t.line; idx++;
        if (idx >= cnt || !(toks[idx].is_identifier() || toks[idx].is(TokenType::Keyword)))
            throw std::runtime_error("Line " + std::to_string(src_ln) + ": pointer needs a variable name");
        std::string var = toks[idx++].value;
        return make_node(NodeType::Pointer, var, src_ln);
    }

    /* identifier: call / index / attribute / plain var */
    if (t.is(TokenType::Identifier) || t.is(TokenType::Keyword)) {
        std::string name = t.value;
        int src_ln = t.line;
        idx++;

        /* function call */
        if (idx < cnt && toks[idx].is_operator('(')) {
            idx++;
            auto call  = make_node(NodeType::Call, name, src_ln);
            ASTNode* tail = nullptr;
            while (idx < cnt && !toks[idx].is_operator(')')) {
                if (toks[idx].is_operator(',')) { idx++; continue; }
                auto arg = parse_expr(toks, idx);
                ASTNode* raw = arg.get();
                if (!call->args) { call->args = std::move(arg); tail = raw; }
                else             { tail->next = std::move(arg); tail = raw; }
            }
            if (idx < cnt) idx++;
            else {
                std::ostringstream msg;
                msg << "Line " << src_ln << ": missing ')' in call to '" << name << "'";
                throw std::runtime_error(msg.str());
            }
            /* Handle trailing chains on call result (e.g., foo().bar) */
            return parse_trailing_chains(std::move(call), toks, idx, cnt);
        }

        /* array index */
        if (idx < cnt && toks[idx].is_operator('[')) {
            idx++;
            auto node  = make_node(NodeType::Index, name, src_ln);
            node->left = parse_expr(toks, idx);
            if (idx < cnt && toks[idx].is_operator(']')) idx++;
            else {
                std::ostringstream msg;
                msg << "Line " << src_ln << ": missing ']' in index expression";
                throw std::runtime_error(msg.str());
            }
            /* Handle trailing chains on index result (e.g., arr[0].field) */
            return parse_trailing_chains(std::move(node), toks, idx, cnt);
        }

        /* struct literal: StructName { field1: expr, field2: expr } */
        if (idx < cnt && toks[idx].is_operator('{')) {
            idx++;
            auto sl = make_node(NodeType::StructLiteral, name, src_ln);
            ASTNode* ftail = nullptr;
            while (idx < cnt && !toks[idx].is_operator('}')) {
                if (toks[idx].is_operator(',') || toks[idx].is_operator(';')) { idx++; continue; }
                if (idx >= cnt || !(toks[idx].is_identifier() || toks[idx].is(TokenType::Keyword)))
                    throw std::runtime_error("Line " + std::to_string(src_ln) + ": expected field name in struct literal");
                auto field = make_node(NodeType::Var, toks[idx].value, src_ln);
                idx++;
                if (idx < cnt && toks[idx].is_operator(':')) {
                    idx++;
                    field->left = parse_expr(toks, idx);
                }
                ASTNode* raw = field.get();
                if (!sl->args) { sl->args = std::move(field); ftail = raw; }
                else           { ftail->next = std::move(field); ftail = raw; }
            }
            if (idx < cnt) idx++; /* skip '}' */
            return parse_trailing_chains(std::move(sl), toks, idx, cnt);
        }

        /* attribute access or method call: obj.field  /  obj.method(args) */
        if (idx < cnt && toks[idx].is_operator('.')) {
            idx++;
            if (idx >= cnt || !(toks[idx].is_identifier() || toks[idx].is(TokenType::Keyword))) {
                std::ostringstream msg;
                msg << "Line " << src_ln << ": attribute access needs a field name";
                throw std::runtime_error(msg.str());
            }
            std::string field = toks[idx++].value;

            /* method call: obj.method(args) */
            if (idx < cnt && toks[idx].is_operator('(')) {
                idx++;
                auto call  = make_node(NodeType::Call, name + "." + field, src_ln);
                call->left = make_node(NodeType::Var, name, src_ln);
                ASTNode* tail = nullptr;
                while (idx < cnt && !toks[idx].is_operator(')')) {
                    if (toks[idx].is_operator(',')) { idx++; continue; }
                    auto arg = parse_expr(toks, idx);
                    ASTNode* raw = arg.get();
                    if (!call->args) { call->args = std::move(arg); tail = raw; }
                    else             { tail->next = std::move(arg); tail = raw; }
                }
                if (idx < cnt) idx++;
                else {
                    std::ostringstream msg;
                    msg << "Line " << src_ln << ": missing ')' in method call '" << name << "." << field << "'";
                    throw std::runtime_error(msg.str());
                }
                /* Handle trailing chains (e.g., obj.method().field) */
                return parse_trailing_chains(std::move(call), toks, idx, cnt);
            }

            /* plain field access: obj.field */
            auto node  = make_node(NodeType::Attribute, field, src_ln);
            node->left = make_node(NodeType::Var, name, src_ln);
            /* Handle trailing chains (e.g., obj.field.method()) */
            return parse_trailing_chains(std::move(node), toks, idx, cnt);
        }

        return make_node(NodeType::Var, name, src_ln);
    }

    std::ostringstream msg;
    msg << "Line " << t.line << ", column " << t.col
        << ": unexpected token '" << t.value << "' in expression";
    throw std::runtime_error(msg.str());
}

ASTNode::Ptr Parser::parse_unary(const std::vector<Token>& toks, int& idx) {
    int cnt = (int)toks.size();
    if (idx < cnt) {
        if (toks[idx].is_operator('-')) {
            int op_line = toks[idx].line;
            idx++;
            auto op  = make_node(NodeType::UnaryOp, "-", op_line);
            op->left = parse_factor(toks, idx);
            return op;
        }
        if (toks[idx].is_keyword("not")) {
            int op_line = toks[idx].line;
            idx++;
            auto op  = make_node(NodeType::UnaryOp, "not", op_line);
            op->left = parse_factor(toks, idx);
            return op;
        }
    }
    return parse_factor(toks, idx);
}

ASTNode::Ptr Parser::parse_term(const std::vector<Token>& toks, int& idx) {
    auto left = parse_unary(toks, idx);
    int  cnt  = (int)toks.size();
    while (idx < cnt && toks[idx].is(TokenType::Operator)) {
        const std::string& v = toks[idx].value;
        if (v != "*" && v != "/" && v != "//" && v != "%" && v != "**") break;
        auto op  = make_node(NodeType::BinOp, v, toks[idx].line);
        idx++;
        op->left = std::move(left);
        op->right= parse_unary(toks, idx);
        left     = std::move(op);
    }
    return left;
}

ASTNode::Ptr Parser::parse_add(const std::vector<Token>& toks, int& idx) {
    auto left = parse_term(toks, idx);
    int  cnt  = (int)toks.size();
    while (idx < cnt && toks[idx].is(TokenType::Operator) &&
           (toks[idx].value == "+" || toks[idx].value == "-")) {
        auto op  = make_node(NodeType::BinOp, toks[idx].value, toks[idx].line);
        idx++;
        op->left = std::move(left);
        op->right= parse_term(toks, idx);
        left     = std::move(op);
    }
    return left;
}

ASTNode::Ptr Parser::parse_range(const std::vector<Token>& toks, int& idx) {
    auto left = parse_add(toks, idx);
    int  cnt  = (int)toks.size();
    /* range operator: a..b (exclusive), a..=b (inclusive) */
    if (idx < cnt && toks[idx].is(TokenType::Operator) &&
        (toks[idx].value == ".." || toks[idx].value == "..=")) {
        bool inclusive = (toks[idx].value == "..=");
        auto op  = make_node(NodeType::BinOp, inclusive ? "..=" : "..", toks[idx].line);
        idx++;
        op->left  = std::move(left);
        op->right = parse_add(toks, idx);
        left      = std::move(op);
    }
    return left;
}

ASTNode::Ptr Parser::parse_bitwise(const std::vector<Token>& toks, int& idx) {
    auto left = parse_range(toks, idx);
    int  cnt  = (int)toks.size();
    while (idx < cnt) {
        bool is_xor_op = toks[idx].is(TokenType::Operator) && toks[idx].value == "^";
        bool is_xor_kw = toks[idx].is_keyword("xor");
        bool is_and_op = toks[idx].is(TokenType::Operator) && toks[idx].value == "&";
        bool is_or_op  = toks[idx].is(TokenType::Operator) && toks[idx].value == "|";
        if (!is_xor_op && !is_xor_kw && !is_and_op && !is_or_op) break;

        std::string op_name;
        if (is_xor_kw || is_xor_op) op_name = "^";
        else if (is_and_op) op_name = "&";
        else if (is_or_op) op_name = "|";

        auto op = make_node(NodeType::BinOp, op_name, toks[idx].line);
        idx++;
        op->left  = std::move(left);
        op->right = parse_range(toks, idx);
        left      = std::move(op);
    }
    return left;
}

ASTNode::Ptr Parser::parse_cmp(const std::vector<Token>& toks, int& idx) {
    auto left = parse_bitwise(toks, idx);
    int  cnt  = (int)toks.size();
    if (idx >= cnt) return left;

    /* Symbol comparisons: ==, !=, <, >, <=, >= */
    if (toks[idx].is(TokenType::Operator)) {
        const std::string& op = toks[idx].value;
        if (op=="==" || op=="!=" || op=="<" || op==">" || op=="<=" || op==">=") {
            auto node  = make_node(NodeType::BinOp, op, toks[idx].line);
            idx++;
            node->left = std::move(left);
            node->right= parse_bitwise(toks, idx);
            left = std::move(node);
        }
    }

    /* Keyword operators: equals, and, or (left-associative, allow chaining) */
    while (idx < cnt && toks[idx].is(TokenType::Keyword)) {
        const std::string& kw = toks[idx].value;
        if (kw == "equals") {
            auto node  = make_node(NodeType::BinOp, "==", toks[idx].line);
            idx++;
            node->left = std::move(left);
            node->right= parse_cmp(toks, idx);
            left = std::move(node);
        } else if (kw == "and" || kw == "or") {
            auto node  = make_node(NodeType::BinOp, kw, toks[idx].line);
            idx++;
            node->left = std::move(left);
            node->right= parse_cmp(toks, idx);
            left = std::move(node);
        } else if (kw == "in") {
            auto node  = make_node(NodeType::BinOp, "in", toks[idx].line);
            idx++;
            node->left = std::move(left);
            node->right= parse_cmp(toks, idx);
            left = std::move(node);
        } else {
            break;
        }
    }

    return left;
}

ASTNode::Ptr Parser::parse_expr(const std::vector<Token>& toks, int& idx) {
    return parse_cmp(toks, idx);
}
