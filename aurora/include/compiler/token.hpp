#pragma once
#include <string>

/* ════════════════════════════════════════════════
   Aurora Language — Token
   ════════════════════════════════════════════════ */

enum class TokenKind {
    Keyword,
    Identifier,
    Number,
    Float,
    Operator,
    String,
    Attribute,
    Newline,
    Indent,
    Dedent,
    Unknown
};

struct Token {
    TokenKind   kind  { TokenKind::Unknown };
    std::string value {};
    int         line  { 0 };
    int         col   { 0 };

    bool is(TokenKind t)                    const { return kind == t; }
    bool is_keyword (const char* kw)        const { return kind == TokenKind::Keyword   && value == kw; }
    bool is_operator(char op)               const { return kind == TokenKind::Operator  && !value.empty() && value[0] == op; }
    bool is_operator(const char* op)        const { return kind == TokenKind::Operator  && value == op; }
    bool is_identifier()                    const { return kind == TokenKind::Identifier; }
    bool is_number()                        const { return kind == TokenKind::Number || kind == TokenKind::Float; }
    bool is_string()                        const { return kind == TokenKind::String;    }
    bool is_unknown()                       const { return kind == TokenKind::Unknown;   }
    bool is_attribute()                     const { return kind == TokenKind::Attribute; }
    bool is_attribute(const char* a)        const { return kind == TokenKind::Attribute && value == a; }
};

const char* token_type_name(TokenKind kind);
std::string token_debug_string(const Token& t);
bool token_is_compound_assign(const std::string& op);
bool token_is_two_char_op(const std::string& op);
