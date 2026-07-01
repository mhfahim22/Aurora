#include "compiler/parser.hpp"
#include <stdexcept>
#include <sstream>

ASTNode::Ptr Parser::parse_enum() {
    const LexedLine& ll   = cur_line();
    const auto&      toks = ll.tokens;
    int              cnt  = static_cast<int>(toks.size());
    int              ci   = ll.indent;
    int              ln   = ll.line_no;

    if (cnt < 2 || !toks[1].is_identifier())
        throw std::runtime_error(
            "Line " + std::to_string(ln) + ": enum needs a name");

    std::string ename = toks[1].value;
    auto stmt = make_node(NodeType::EnumDecl, ename, ln);

    int idx = 2;
    if (idx < cnt && toks[idx].is_operator(':')) idx++;
    require_token_end(toks, idx, "enum definition");
    advance();

    /* Parse variant names from indented body (each line is just a name) */
    ASTNode* tail = nullptr;
    while (!at_end()) {
        if (!skip_blanks()) break;
        if (cur_indent() <= ci) break;

        const LexedLine& vline = cur_line();
        const auto& vtoks = vline.tokens;
        if (vtoks.empty()) { advance(); continue; }

        std::string vname = vtoks[0].value;
        auto variant = make_node(NodeType::Var, vname, vline.line_no);
        ASTNode* raw = variant.get();

        if (!stmt->args) { stmt->args = std::move(variant); tail = raw; }
        else             { tail->next = std::move(variant);  tail = raw; }

        advance();
    }

    return stmt;
}