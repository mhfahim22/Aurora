#include "../../include/tools/lsp.hpp"
#include "compiler/keywords.hpp"
#include <set>

/* ── Semantic token type indices (must match lsp.hpp legend order) ── */
enum STType {
    ST_Variable = 0, ST_Function, ST_Method, ST_Class, ST_Interface,
    ST_Enum, ST_Struct, ST_Parameter, ST_Property, ST_Keyword,
    ST_Comment, ST_String, ST_Number, ST_Operator, ST_Type,
    ST_Namespace, ST_Macro, ST_Modifier, ST_Event, ST_Decorator
};

enum STMod {
    ST_None = 0,
    ST_Declaration = 1,
    ST_Definition = 2,
    ST_Readonly = 4,
    ST_Static = 8,
    ST_Abstract = 16,
    ST_Deprecated = 32,
    ST_Async = 64,
    ST_Modification = 128,
    ST_Documentation = 256
};

/* ════════════════════════════════════════════════════════════
   Semantic token provider — maps lexed tokens + AST to semantic tokens
   ════════════════════════════════════════════════════════════ */

std::vector<SemanticToken> LspServer::get_semantic_tokens(DocumentState& doc) {
    std::vector<SemanticToken> tokens;
    const auto& lines = doc.lines;

    /* Build set of function/class names from AST */
    std::set<std::string> func_names;
    std::set<std::string> class_names;
    std::set<std::string> struct_names;
    std::set<std::string> enum_names;
    std::set<std::string> interface_names;
    std::set<std::string> var_names;

    if (doc.ast) {
        walk_ast(doc.ast.get(), [&](ASTNode* n) {
            switch (n->type) {
                case NodeType::Function: func_names.insert(n->value); break;
                case NodeType::Class: class_names.insert(n->value); break;
                case NodeType::StructDecl: struct_names.insert(n->value); break;
                case NodeType::EnumDecl: enum_names.insert(n->value); break;
                case NodeType::InterfaceDecl: interface_names.insert(n->value); break;
                case NodeType::Var: if (!n->value.empty()) var_names.insert(n->value); break;
                default: break;
            }
        });
    }

    /* Process each line's tokens into semantic tokens */
    int prev_line = 0;
    int prev_col = 0;

    for (auto& ll : lines) {
        for (auto& t : ll.tokens) {
            SemanticToken st;
            st.line = ll.line_no - 1;
            st.startChar = t.col;
            st.length = (int)t.value.size();
            st.tokenType = ST_Variable;
            st.tokenModifiers = 0;

            /* Determine token type based on token kind and AST info */
            switch (t.type) {
                case TokenType::Keyword: {
                    st.tokenType = ST_Keyword;
                    std::string kw = t.value;

                    if (kw == "true" || kw == "false" || kw == "null")
                        st.tokenType = ST_Variable; /* constants */

                    if (kw == "function" || kw == "lambda")
                        st.tokenModifiers |= ST_Declaration;

                    if (kw == "class") st.tokenType = ST_Class;
                    if (kw == "interface") st.tokenType = ST_Interface;
                    if (kw == "enum") st.tokenType = ST_Enum;
                    if (kw == "struct") st.tokenType = ST_Struct;
                    if (kw == "public" || kw == "private" || kw == "protected" ||
                        kw == "static" || kw == "final" || kw == "abstract" ||
                        kw == "const" || kw == "mutable")
                        st.tokenType = ST_Modifier;

                    if (kw == "import" || kw == "from" || kw == "namespace" || kw == "module")
                        st.tokenType = ST_Namespace;

                    if (kw == "async" || kw == "await" || kw == "spawn")
                        st.tokenModifiers |= ST_Async;

                    break;
                }
                case TokenType::Identifier: {
                    st.tokenType = ST_Variable;

                    /* Check if known function/class from AST */
                    if (func_names.count(t.value))
                        st.tokenType = ST_Function;
                    else if (class_names.count(t.value))
                        st.tokenType = ST_Class;
                    else if (struct_names.count(t.value))
                        st.tokenType = ST_Struct;
                    else if (enum_names.count(t.value))
                        st.tokenType = ST_Enum;
                    else if (interface_names.count(t.value))
                        st.tokenType = ST_Interface;

                    /* Check if it's a type name (uppercase convention or builtin type) */
                    static const std::set<std::string> types = {
                        "int", "float", "string", "bool", "void",
                        "list", "map", "set", "array", "json",
                        "tuple", "vector", "stack", "queue"
                    };
                    if (types.count(t.value))
                        st.tokenType = ST_Type;

                    /* Check if followed by '(' → function call */
                    /* Don't reset modifiers — merge them; currently no identifier-specific modifiers set */
                    break;
                }
                case TokenType::Number:
                case TokenType::Float:
                    st.tokenType = ST_Number;
                    break;
                case TokenType::String:
                    st.tokenType = ST_String;
                    break;
                case TokenType::Operator:
                    st.tokenType = ST_Operator;
                    break;
                case TokenType::Attribute:
                    st.tokenType = ST_Decorator;
                    break;
                default:
                    st.tokenType = ST_Variable;
                    break;
            }

            /* Delta-encoding: store relative to previous token */
            if (tokens.empty()) {
                /* First token: absolute position */
            } else {
                if (st.line == prev_line) {
                    st.startChar -= prev_col + tokens.back().length;
                    st.line = 0;
                } else {
                    st.line -= prev_line;
                }
            }

            prev_line = ll.line_no - 1;
            prev_col = t.col;

            tokens.push_back(st);
        }
    }

    return tokens;
}
