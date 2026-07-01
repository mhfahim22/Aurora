#include "compiler/parser.hpp"
#include <stdexcept>
#include <sstream>

ASTNode::Ptr Parser::parse_type_alias() {
    const LexedLine& ll   = cur_line();
    const auto&      toks = ll.tokens;
    int              cnt  = static_cast<int>(toks.size());
    int              ln   = ll.line_no;

    if (cnt < 4 || !toks[1].is_identifier() || !toks[2].is_operator('='))
        throw std::runtime_error(
            "Line " + std::to_string(ln) + ": syntax: type Name = BaseType");

    std::string alias_name = toks[1].value;
    std::string base_name  = toks[3].value;

    auto stmt = make_node(NodeType::TypeAlias, alias_name, ln);
    stmt->left = make_node(NodeType::Var, base_name, ln);

    int idx = 4;
    require_token_end(toks, idx, "type alias");
    advance();
    return stmt;
}