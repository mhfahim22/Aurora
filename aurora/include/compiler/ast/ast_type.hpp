#pragma once

/* ════════════════════════════════════════════════════════════
   ast_type.hpp — Type-related AST Node Types
   ════════════════════════════════════════════════════════════
   These may be merged into ast.hpp or split into a type-
   specific AST later.  Currently serves as a placeholder for
   the kind-of-type each AST node represents.
   ════════════════════════════════════════════════════════════ */

enum class AstTypeKind {
    Unknown,
    Int,
    Float,
    String,
    Bool,
    Array,
    Struct,
    Function,
    Class,
    Void,
};

inline const char* ast_type_kind_name(AstTypeKind k) {
    switch (k) {
        case AstTypeKind::Unknown:  return "unknown";
        case AstTypeKind::Int:      return "int";
        case AstTypeKind::Float:    return "float";
        case AstTypeKind::String:   return "string";
        case AstTypeKind::Bool:     return "bool";
        case AstTypeKind::Array:    return "array";
        case AstTypeKind::Struct:   return "struct";
        case AstTypeKind::Function: return "function";
        case AstTypeKind::Class:    return "class";
        case AstTypeKind::Void:     return "void";
    }
    return "unknown";
}

/* ── Future: type annotation info carried on AST nodes ── */
struct AstTypeAnnotation {
    AstTypeKind kind { AstTypeKind::Unknown };
    bool        is_mutable { true };
    bool        is_nullable { false };

    /* For arrays: element type */
    AstTypeKind element_kind { AstTypeKind::Unknown };
};
