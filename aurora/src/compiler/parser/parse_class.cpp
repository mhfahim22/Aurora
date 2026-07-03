#include "compiler/parser.hpp"
#include <stdexcept>
#include <sstream>

/* ════════════════════════════════════════════════════════════
   parse_class.cpp — Class definition parser
   ════════════════════════════════════════════════════════════
   Syntax:
       class Name:
           field = default_value
           function method(params):
               body

       class Child extends Parent:
           ...

       abstract class Name:
           ...

       final class Name:
           ...
   ════════════════════════════════════════════════════════════ */

ASTNode::Ptr Parser::parse_class() {
    const LexedLine& ll   = cur_line();
    const auto&      toks = ll.tokens;
    int              cnt  = static_cast<int>(toks.size());
    int              ci   = ll.indent;
    int              ln   = ll.line_no;

    int di = 0;  /* index into tokens */
    /* Check for abstract/final prefix before 'class' keyword */
    bool is_abstract = false;
    bool is_final = false;
    if (cnt > 1 && toks[0].is_keyword("abstract") && toks[1].is_keyword("class")) {
        is_abstract = true;
        di = 1;
    } else if (cnt > 1 && toks[0].is_keyword("final") && toks[1].is_keyword("class")) {
        is_final = true;
        di = 1;
    }

    if (cnt < di + 2 || !toks[di + 1].is_identifier())
        throw std::runtime_error(
            "Line " + std::to_string(ln) + ": class needs a name");

    std::string cname = toks[di + 1].value;
    auto stmt = make_node(NodeType::Class, cname, ln);
    stmt->is_abstract = is_abstract;
    stmt->is_final = is_final;

    int idx = di + 2;

    /* ── Generic type parameters: class Foo[T, U] ── */
    if (idx < cnt && toks[idx].is_operator('[')) {
        idx++;
        ASTNode* tp_tail = nullptr;
        while (idx < cnt && !toks[idx].is_operator(']')) {
            if (toks[idx].is_operator(',')) { idx++; continue; }
            if (idx < cnt && (toks[idx].is_identifier() || toks[idx].is(TokenKind::Keyword))) {
                auto tp = make_node(NodeType::TypeParam, toks[idx].value, ln);
                ASTNode* raw = tp.get();
                if (!stmt->template_params) { stmt->template_params = std::move(tp); tp_tail = raw; }
                else                         { tp_tail->next = std::move(tp); tp_tail = raw; }
            }
            idx++;
        }
        if (idx < cnt && toks[idx].is_operator(']')) idx++;
    }

    /* extends ParentClass */
    if (idx < cnt && toks[idx].is_keyword("extends")) {
        idx++;
        if (idx >= cnt || !toks[idx].is_identifier())
            throw std::runtime_error(
                "Line " + std::to_string(ln) + ": expected parent class name after 'extends'");
        stmt->left = make_node(NodeType::Var, toks[idx].value, ln);
        idx++;
    }

    /* implements Interface1, Interface2, ... */
    if (idx < cnt && toks[idx].is_keyword("implements")) {
        idx++;
        ASTNode* itail = nullptr;
        while (idx < cnt && (toks[idx].is_identifier() || toks[idx].is_operator(','))) {
            if (toks[idx].is_operator(',')) { idx++; continue; }
            if (!toks[idx].is_identifier())
                throw std::runtime_error(
                    "Line " + std::to_string(ln) + ": expected interface name after 'implements'");
            auto iface = make_node(NodeType::Var, toks[idx].value, ln);
            idx++;
            ASTNode* raw = iface.get();
            if (!stmt->args) { stmt->args = std::move(iface); itail = raw; }
            else             { itail->next = std::move(iface); itail = raw; }
        }
    }

    if (idx < cnt && toks[idx].is_operator(':')) idx++;
    require_token_end(toks, idx, "class definition");
    advance();

    stmt->body = parse_block(ci);
    return stmt;
}