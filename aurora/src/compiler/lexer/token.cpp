#include "compiler/token.hpp"
#include <unordered_map>

static const std::unordered_map<TokenType, const char*>& token_type_names() {
    static const std::unordered_map<TokenType, const char*> names = {
        {TokenType::Keyword,    "keyword"},
        {TokenType::Identifier, "identifier"},
        {TokenType::Number,     "number"},
        {TokenType::Float,      "float"},
        {TokenType::Operator,   "operator"},
        {TokenType::String,     "string"},
        {TokenType::Attribute,  "attribute"},
        {TokenType::Newline,    "newline"},
        {TokenType::Indent,     "indent"},
        {TokenType::Dedent,     "dedent"},
        {TokenType::Unknown,    "unknown"},
    };
    return names;
}

const char* token_type_name(TokenType type) {
    auto it = token_type_names().find(type);
    return (it != token_type_names().end()) ? it->second : "???";
}

std::string token_debug_string(const Token& t) {
    std::string result = token_type_name(t.type);
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
    return op == "+=" || op == "-=" || op == "*=" || op == "/=";
}

bool token_is_two_char_op(const std::string& op) {
    return op == "==" || op == "!=" || op == "<=" || op == ">=" ||
           op == "->" || op == "**" || op == "//" || op == "..";
}
