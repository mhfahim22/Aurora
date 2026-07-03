#include "compiler/parser.hpp"
#include <stdexcept>
#include <sstream>

ASTNode::Ptr Parser::parse_struct() {
    const LexedLine& ll   = cur_line();
    const auto&      toks = ll.tokens;
    int              cnt  = static_cast<int>(toks.size());
    int              ci   = ll.indent;
    int              ln   = ll.line_no;

    if (cnt < 2 || !toks[1].is_identifier())
        throw std::runtime_error(
            "Line " + std::to_string(ln) + ": struct needs a name");

    std::string sname = toks[1].value;
    auto stmt = make_node(NodeType::StructDecl, sname, ln);

    int idx = 2;

    /* ── Generic type parameters: struct Foo[T, U] ── */
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

    if (idx < cnt && toks[idx].is_operator(':')) idx++;
    require_token_end(toks, idx, "struct definition");
    advance();

    stmt->body = parse_block(ci);
    return stmt;
}