#pragma once
#include <string>

/* ════════════════════════════════════════════════
   Aurora Language — Token
   ════════════════════════════════════════════════ */

enum class TokenType {
    Keyword,
    Identifier,
    Number,
    Float,
    Operator,
    String,
    Attribute,    /* @performance, @inline, etc. */
    Newline,
    Indent,
    Dedent,
    Unknown
};

struct Token {
    TokenType   type  { TokenType::Unknown };
    std::string value {};
    int         line  { 0 };  /* source line number (for error messages) */
    int         col   { 0 };  /* source column */

    bool is(TokenType t)              const { return type == t; }
    bool is_keyword (const char* kw)  const { return type == TokenType::Keyword    && value == kw; }
    bool is_operator(char op)         const { return type == TokenType::Operator   && !value.empty() && value[0] == op; }
    bool is_operator(const char* op)  const { return type == TokenType::Operator   && value == op; }
    bool is_identifier()              const { return type == TokenType::Identifier; }
    bool is_number()                  const { return type == TokenType::Number || type == TokenType::Float; }
    bool is_string()                  const { return type == TokenType::String;     }
    bool is_unknown()                 const { return type == TokenType::Unknown;    }
    bool is_attribute()               const { return type == TokenType::Attribute;  }
    bool is_attribute(const char* a)  const { return type == TokenType::Attribute && value == a; }
};

/* ── Token utility functions (token.cpp) ── */
const char* token_type_name(TokenType type);
std::string token_debug_string(const Token& t);
bool token_is_compound_assign(const std::string& op);
bool token_is_two_char_op(const std::string& op);
