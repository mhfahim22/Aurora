#include "compiler/parser.hpp"
#include <stdexcept>
#include <sstream>

/* ── Helper: parse struct/union fields from a token list ── */
static void parse_extern_fields(const std::vector<Token>& ftoks, ASTNode* stmt, int ln) {
    ASTNode* ftail = nullptr;
    for (int fi = 0; fi < static_cast<int>(ftoks.size()); ) {
        if (ftoks[fi].is_operator(',') || ftoks[fi].is_operator(';')) { fi++; continue; }
        if (!ftoks[fi].is_identifier())
            throw std::runtime_error("Line " + std::to_string(ln) + ": expected field name in struct/union");
        auto field = make_node(NodeType::Var, ftoks[fi].value, ln);
        fi++;
        if (fi < static_cast<int>(ftoks.size()) && ftoks[fi].is_operator(':')) {
            fi++;
            if (fi < static_cast<int>(ftoks.size()) && (ftoks[fi].is_identifier() || ftoks[fi].is(TokenType::Keyword))) {
                field->right = make_node(NodeType::Var, ftoks[fi].value, ln);
                fi++;
            }
        }
        ASTNode* raw = field.get();
        if (!stmt->args) { stmt->args = std::move(field); ftail = raw; }
        else             { ftail->next = std::move(field); ftail = raw; }
    }
}

/* ── Helper: collect tokens inside { ... } potentially spanning lines ── */
static std::vector<Token> collect_braced_tokens(Parser& parser, const std::vector<Token>& toks, int& idx, int cnt, int ln, const char* kind) {
    std::vector<Token> ftoks;
    for (int i = idx; i < cnt; i++) {
        if (toks[i].is_operator('}')) { idx = i + 1; return ftoks; }
        ftoks.push_back(toks[i]);
    }
    /* } not on opening line — advance and read subsequent lines */
    while (!parser.at_end()) {
        parser.advance();
        if (parser.at_end()) break;
        const auto& nt = parser.cur_line().tokens;
        for (int i = 0; i < static_cast<int>(nt.size()); i++) {
            if (nt[i].is_operator('}')) { idx = cnt; return ftoks; }
            ftoks.push_back(nt[i]);
        }
    }
    throw std::runtime_error("Line " + std::to_string(ln) + ": unclosed '{' in " + kind);
}

ASTNode::Ptr Parser::parse_extern() {
    auto& toks = cur_line().tokens;
    int ln = cur_line().line_no;
    int cnt = static_cast<int>(toks.size());
    auto& t0 = toks[0];

    /* ── @cost(zero|alloc|indirection) — FFI cost annotation ── */
    if (t0.is_attribute("cost")) {
        if (cnt < 4 || !toks[1].is_operator('(')) {
            throw std::runtime_error("Line " + std::to_string(ln) +
                ": @cost needs (zero|alloc|indirection)");
        }
        std::string cost_val = toks[2].value;
        if (cost_val != "zero" && cost_val != "alloc" && cost_val != "indirection") {
            throw std::runtime_error("Line " + std::to_string(ln) +
                ": @cost must be zero, alloc, or indirection");
        }
        if (cnt < 4 || !toks[3].is_operator(')')) {
            throw std::runtime_error("Line " + std::to_string(ln) +
                ": @cost needs closing ')'");
        }
        pending_cost_ = cost_val;
        advance();
        return nullptr; /* pending_cost_ will be picked up by the next stmt */
    }

    /* ── extern struct / function definition (FFI) ── */
    /* extern struct Name { field1: type1, field2: type2 }                */
    /* extern "libname" function name(params) -> ret_type                */
    /* extern "stdcall" function name(params) -> ret_type                */
    /* extern "stdcall" "libname" function name(params) -> ret_type      */
    /* extern "c" function name(params) -> ret_type                      */
    /* extern function name(params) -> ret_type                          */
    /* ── Check for pending @cost from previous line ── */
    std::string stmt_cost;
    if (!pending_cost_.empty()) {
        stmt_cost = pending_cost_;
        pending_cost_.clear();
    }

    if (t0.is_keyword("extern")) {
        std::string lib_name;
        std::string call_conv = "c";
        int idx = 1;
        /* optional library name, calling convention, or ecosystem string */
        if (idx < cnt && toks[idx].is_string()) {
            std::string sval = toks[idx].value;
            /* Detect cross-ecosystem bridge identifiers */
            if (sval == "python" || sval == "quickjs" || sval == "rust") {
                /* Ecosystem bridge — store for codegen dispatch */
                /* Will be set on the stmt node after creation */
                pending_ecosystem_ = sval;
                idx++;
                /* Optional second string: module/package name within the ecosystem */
                if (idx < cnt && toks[idx].is_string()) {
                    lib_name = toks[idx].value;
                    idx++;
                }
            } else if (sval == "c" || sval == "stdcall" || sval == "fastcall" ||
                sval == "thiscall" || sval == "vectorcall" ||
                sval == "win64" || sval == "sysv64") {
                call_conv = sval;
                idx++;
                /* Optional second string: library name */
                if (idx < cnt && toks[idx].is_string()) {
                    lib_name = toks[idx].value;
                    idx++;
                }
            } else {
                /* Unknown string: treat as library name (backward compatible) */
                lib_name = sval;
                idx++;
            }
        }
        if (idx >= cnt) throw std::runtime_error("Line " + std::to_string(ln) + ": 'extern' needs 'struct' or 'function'");

        /* ── extern struct ── */
        if (toks[idx].is_keyword("struct")) {
            idx++;
            if (idx >= cnt || !toks[idx].is_identifier())
                throw std::runtime_error("Line " + std::to_string(ln) + ": extern struct needs a name");
            auto stmt = make_node(NodeType::ExternStruct, toks[idx].value, ln);
            idx++;

            /* Store library name if any */
            if (!lib_name.empty())
                stmt->right = make_node(NodeType::Str, lib_name, ln);
            if (!stmt_cost.empty()) stmt->cost_level = stmt_cost;
            if (!pending_ecosystem_.empty()) {
                stmt->ecosystem = pending_ecosystem_;
                pending_ecosystem_.clear();
            }

            /* Parse fields: { field1: type1, field2: type2 } or multi-line */
            if (idx < cnt && toks[idx].is_operator('{')) {
                idx++;
                auto ftoks = collect_braced_tokens(*this, toks, idx, cnt, ln, "struct");
                parse_extern_fields(ftoks, stmt.get(), ln);
            }

            require_token_end(toks, idx, "extern struct declaration");
            advance();
            return stmt;
        }

        /* ── extern union ── */
        if (toks[idx].is_keyword("union")) {
            idx++;
            if (idx >= cnt || !toks[idx].is_identifier())
                throw std::runtime_error("Line " + std::to_string(ln) + ": extern union needs a name");
            auto stmt = make_node(NodeType::ExternUnion, toks[idx].value, ln);
            idx++;

            /* Store library name if any */
            if (!lib_name.empty())
                stmt->right = make_node(NodeType::Str, lib_name, ln);
            if (!stmt_cost.empty()) stmt->cost_level = stmt_cost;
            if (!pending_ecosystem_.empty()) {
                stmt->ecosystem = pending_ecosystem_;
                pending_ecosystem_.clear();
            }

            /* Parse fields: { field1: type1, field2: type2 } — may span multiple lines */
            if (idx < cnt && toks[idx].is_operator('{')) {
                idx++;
                auto ftoks = collect_braced_tokens(*this, toks, idx, cnt, ln, "union");
                parse_extern_fields(ftoks, stmt.get(), ln);
            }

            require_token_end(toks, idx, "extern union declaration");
            advance();
            return stmt;
        }

        /* ── extern function ── */
        if (idx >= cnt || !toks[idx].is_keyword("function"))
            throw std::runtime_error("Line " + std::to_string(ln) + ": 'extern' must be followed by 'struct', 'function', or 'string'");
        idx++;
        if (idx >= cnt || !(toks[idx].is_identifier() || toks[idx].is(TokenType::Keyword)))
            throw std::runtime_error("Line " + std::to_string(ln) + ": extern function needs a name");
        std::string fname = toks[idx].value;
        auto stmt = make_node(NodeType::ExternFn, fname, ln);
        stmt->calling_conv = call_conv;
        if (!stmt_cost.empty()) stmt->cost_level = stmt_cost;
        if (!pending_ecosystem_.empty()) {
            stmt->ecosystem = pending_ecosystem_;
            pending_ecosystem_.clear();
        }
        idx++;

        /* Parse params: (param1: type1, param2: callback(p: type) -> ret, ...) */
        if (idx < cnt && toks[idx].is_operator('(')) {
            idx++;
            ASTNode* ptail = nullptr;
            while (idx < cnt && !toks[idx].is_operator(')')) {
                if (toks[idx].is_operator(',')) { idx++; continue; }
                /* varargs: extern function printf(fmt: cstring, ...) -> i64 */
                if (toks[idx].value == "...") {
                    stmt->is_vararg = true;
                    idx++;
                    break;
                }
                /* param name — can be identifier or keyword (e.g. `type`, `string`) */
                if (idx >= cnt || !(toks[idx].is_identifier() || toks[idx].is(TokenType::Keyword)))
                    throw std::runtime_error("Line " + std::to_string(ln) + ": expected parameter name");
                auto param = make_node(NodeType::Var, toks[idx].value, ln);
                idx++;
                /* optional : type  — can be simple type or callback(params) -> ret */
                if (idx < cnt && toks[idx].is_operator(':')) {
                    idx++;
                    if (idx < cnt && toks[idx].is_keyword("callback")) {
                        /* callback(param_types...) -> return_type */
                        auto fn_type = make_node(NodeType::FunctionType, "", ln);
                        idx++; /* skip 'callback' */
                        if (idx < cnt && toks[idx].is_operator('(')) {
                            idx++;
                            ASTNode* ct_tail = nullptr;
                            while (idx < cnt && !toks[idx].is_operator(')')) {
                                if (toks[idx].is_operator(',')) { idx++; continue; }
                                if (idx < cnt && (toks[idx].is_identifier() || toks[idx].is(TokenType::Keyword))) {
                                    auto ct = make_node(NodeType::Var, toks[idx].value, ln);
                                    idx++;
                                    ASTNode* raw_ct = ct.get();
                                    if (!fn_type->args) { fn_type->args = std::move(ct); ct_tail = raw_ct; }
                                    else                 { ct_tail->next = std::move(ct); ct_tail = raw_ct; }
                                } else break;
                            }
                            if (idx < cnt) idx++; /* ')' */
                        }
                        /* Parse optional -> return_type for callback */
                        if (idx < cnt && toks[idx].is_operator("->")) {
                            idx++;
                            if (idx < cnt && (toks[idx].is_identifier() || toks[idx].is(TokenType::Keyword))) {
                                fn_type->left = make_node(NodeType::Var, toks[idx].value, ln);
                                idx++;
                            }
                        } else {
                            fn_type->left = make_node(NodeType::Var, "void", ln);
                        }
                        param->right = std::move(fn_type);
                    } else if (idx < cnt && (toks[idx].is_identifier() || toks[idx].is(TokenType::Keyword))) {
                        param->right = make_node(NodeType::Var, toks[idx].value, ln);
                        idx++;
                    }
                }
                ASTNode* raw = param.get();
                if (!stmt->args) { stmt->args = std::move(param); ptail = raw; }
                else             { ptail->next = std::move(param); ptail = raw; }
            }
            if (idx < cnt) idx++; /* ')' */
        }

        /* Parse optional -> return_type */
        if (idx < cnt && toks[idx].is_operator("->")) {
            idx++;
            if (idx < cnt && (toks[idx].is_identifier() || toks[idx].is(TokenType::Keyword)))
                stmt->left = make_node(NodeType::Var, toks[idx].value, ln);
            idx++;
        } else {
            stmt->left = make_node(NodeType::Var, "void", ln);
        }

        /* Store library name */
        if (!lib_name.empty())
            stmt->right = make_node(NodeType::Str, lib_name, ln);

        require_token_end(toks, idx, "extern function declaration");
        advance();
        return stmt;
    }

    return nullptr;
}