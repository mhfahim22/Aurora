#include "compiler/parser.hpp"
#include <stdexcept>
#include <sstream>

ASTNode::Ptr Parser::parse_interface() {
    const LexedLine& ll   = cur_line();
    const auto&      toks = ll.tokens;
    int              cnt  = static_cast<int>(toks.size());
    int              ci   = ll.indent;
    int              ln   = ll.line_no;

    if (cnt < 2 || !toks[1].is_identifier())
        throw std::runtime_error(
            "Line " + std::to_string(ln) + ": interface needs a name");

    std::string iname = toks[1].value;
    auto stmt = make_node(NodeType::InterfaceDecl, iname, ln);

    int idx = 2;

    /* interface Name extends ParentInterface */
    if (idx < cnt && toks[idx].is_keyword("extends")) {
        idx++;
        if (idx >= cnt || !toks[idx].is_identifier())
            throw std::runtime_error(
                "Line " + std::to_string(ln) + ": expected parent interface name after 'extends'");
        stmt->left = make_node(NodeType::Var, toks[idx].value, ln);
        idx++;
    }

    if (idx < cnt && toks[idx].is_operator(':')) idx++;
    require_token_end(toks, idx, "interface definition");
    advance();

    stmt->body = parse_block(ci);
    return stmt;
}