#pragma once
#include <string>
#include <vector>
#include "compiler/token.hpp"

/* ════════════════════════════════════════════════
   LexedLine — একটা source line এর tokens
   ════════════════════════════════════════════════ */
struct LexedLine {
    std::vector<Token> tokens {};
    int                indent  { 0 };
    int                line_no { 0 };

    bool empty()    const { return tokens.empty(); }
    bool is_blank() const {
        for (auto& t : tokens)
            if (!t.is_unknown()) return false;
        return true;
    }

    int  size()     const { return (int)tokens.size(); }
    const Token& operator[](int i) const { return tokens[i]; }
};

/* ════════════════════════════════════════════════
   Lexer
   .aura source → vector<LexedLine>
   ════════════════════════════════════════════════ */
class Lexer {
public:
    std::vector<LexedLine> lex(const std::string& source);
    LexedLine lex_line(const std::string& line, int line_no = 0);

private:
    bool block_comment_continue_ = false;
    static int  get_indent(const std::string& line);
    static void strip_cr  (std::string& line);
};
