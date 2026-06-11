#pragma once
#include "compiler/ast.hpp"
#include "compiler/lexer.hpp"
#include <sstream>

/* ════════════════════════════════════════════════════════════
   Error formatting helpers
   ════════════════════════════════════════════════════════════ */
inline std::string format_error(int line, int col, const std::string& msg) {
    std::ostringstream ss;
    ss << "\033[1;31mError\033[0m at \033[1;33mline " << line;
    if (col > 0) ss << ", column " << col;
    ss << "\033[0m: " << msg;
    return ss.str();
}

inline std::string format_error_with_hint(int line, int col,
                                           const std::string& msg,
                                           const std::string& hint) {
    std::ostringstream ss;
    ss << "\033[1;31mError\033[0m at \033[1;33mline " << line;
    if (col > 0) ss << ", column " << col;
    ss << "\033[0m: " << msg << "\n";
    ss << "  \033[1;36mhint:\033[0m " << hint;
    return ss.str();
}

class Parser {
public:
    explicit Parser(const std::vector<LexedLine>& lines)
        : lines_(lines), cur_(0) {}

    ASTNode::Ptr parse();

private:
    const std::vector<LexedLine>& lines_;
    int cur_;
    std::string pending_cost_ {}; /* @cost(zero|alloc|indirection) set by previous line */

    ASTNode::Ptr parse_block(int parent_indent);
    ASTNode::Ptr parse_brace_block(int brace_depth = 1);
    ASTNode::Ptr parse_stmt();

    ASTNode::Ptr parse_class();
    ASTNode::Ptr parse_struct();
    ASTNode::Ptr parse_enum();
    ASTNode::Ptr parse_interface();
    ASTNode::Ptr parse_type_alias();
    ASTNode::Ptr parse_expr  (const std::vector<Token>& toks, int& idx);
    ASTNode::Ptr parse_cmp   (const std::vector<Token>& toks, int& idx);
    ASTNode::Ptr parse_bitwise(const std::vector<Token>& toks, int& idx);
    ASTNode::Ptr parse_range (const std::vector<Token>& toks, int& idx);
    ASTNode::Ptr parse_add   (const std::vector<Token>& toks, int& idx);
    ASTNode::Ptr parse_term  (const std::vector<Token>& toks, int& idx);
    ASTNode::Ptr parse_unary (const std::vector<Token>& toks, int& idx);
    ASTNode::Ptr parse_factor(const std::vector<Token>& toks, int& idx);

    ASTNode::Ptr parse_trailing_chains(ASTNode::Ptr base,
                                       const std::vector<Token>& toks,
                                       int& idx, int cnt);

    void require_token_end(const std::vector<Token>& toks, int idx, const char* context) const;

    bool skip_blanks();
    bool at_end()               const { return cur_ >= (int)lines_.size(); }
    const LexedLine& cur_line() const { return lines_[cur_]; }
    int  cur_indent()           const { return at_end() ? -1 : cur_line().indent; }
    void advance()                    { cur_++; }
};
