#include "compiler/token.hpp"
#include <unordered_map>

static const std::unordered_map<TokenKind, const char*>& token_type_names() {
    static const std::unordered_map<TokenKind, const char*> names = {
        {TokenKind::Keyword,    "keyword"},
        {TokenKind::Identifier, "identifier"},
        {TokenKind::Number,     "number"},
        {TokenKind::Float,      "float"},
        {TokenKind::Operator,   "operator"},
        {TokenKind::String,     "string"},
        {TokenKind::Attribute,  "attribute"},
        {TokenKind::Newline,    "newline"},
        {TokenKind::Indent,     "indent"},
        {TokenKind::Dedent,     "dedent"},
        {TokenKind::Unknown,    "unknown"},
    };
    return names;
}

const char* token_type_name(TokenKind type) {
    auto it = token_type_names().find(type);
    return (it != token_type_names().end()) ? it->second : "???";
}

std::string token_debug_string(const Token& t) {
    std::string result = token_type_name(t.kind);
    result += "(\"";
    result += t.value;
    result += "\"";
    if (t.line > 0 || t.col > 0) {
        result += " @ ";
        if (t.line > 0) {
            result += "L";
            result += std::to_string(t.line);
        }
        if (t.col > 0) {
            if (t.line > 0) result += ":";
            result += "C";
            result += std::to_string(t.col);
        }
    }
    result += ")";
    return result;
}

bool token_is_compound_assign(const std::string& op) {
    return op == "+=" || op == "-=" || op == "*=" || op == "/=" ||
           op == "%=" || op == "&=" || op == "|=" || op == "^=" ||
           op == "<<=" || op == ">>=";
}

bool token_is_two_char_op(const std::string& op) {
    return op == "==" || op == "!=" || op == "<=" || op == ">=" ||
           op == "->" || op == "**" || op == "//" || op == "..";
}
