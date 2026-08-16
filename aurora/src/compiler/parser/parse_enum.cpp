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

    /* Parse variant names from indented body */
    /* Supports: VariantName, VariantName(Type1, Type2), VariantName */
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

        /* Check for Variant(Type1, Type2, ...) with associated data */
        int vi = 1;
        if (vi < (int)vtoks.size() && vtoks[vi].is_operator('(')) {
            vi++;
            ASTNode* ftail = nullptr;
            while (vi < (int)vtoks.size() && !vtoks[vi].is_operator(')')) {
                if (vtoks[vi].is_operator(',')) { vi++; continue; }
                if (vi < (int)vtoks.size() && (vtoks[vi].is_identifier() || vtoks[vi].is(TokenKind::Keyword))) {
                    auto ftype = make_node(NodeType::Var, vtoks[vi].value, vline.line_no);
                    ASTNode* raw_ft = ftype.get();
                    if (!variant->right) { variant->right = std::move(ftype); ftail = raw_ft; }
                    else                  { ftail->next = std::move(ftype);   ftail = raw_ft; }
                }
                vi++;
            }
            /* skip ')' */
        }

        if (!stmt->args) { stmt->args = std::move(variant); tail = raw; }
        else             { tail->next = std::move(variant);  tail = raw; }

        advance();
    }

    return stmt;
}
