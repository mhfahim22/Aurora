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

    /* Phase 2: user-defined / extended types — mirrors AuroraType */
    Enum,
    Interface,
    Tuple,
    Pointer,
    List,
    Map,
    Set,
    Vector,
    Stack,
    Queue,
    Json,

    /* Integer/float subtypes (Phase 7 type-system fix) */
    I8, I16, I32, I64,
    U8, U16, U32, U64,
    F32,
    Byte,
    Char,

    /* Fixed-size arrays (Phase 11 type-system fix) */
    FixedArray,
};

inline const char* ast_type_kind_name(AstTypeKind k) {
    switch (k) {
        case AstTypeKind::Unknown:   return "unknown";
        case AstTypeKind::Int:       return "int";
        case AstTypeKind::Float:     return "float";
        case AstTypeKind::String:    return "string";
        case AstTypeKind::Bool:      return "bool";
        case AstTypeKind::Array:     return "array";
        case AstTypeKind::Struct:    return "struct";
        case AstTypeKind::Function:  return "function";
        case AstTypeKind::Class:     return "class";
        case AstTypeKind::Void:      return "void";
        case AstTypeKind::Enum:      return "enum";
        case AstTypeKind::Interface: return "interface";
        case AstTypeKind::Tuple:     return "tuple";
        case AstTypeKind::Pointer:   return "pointer";
        case AstTypeKind::List:      return "list";
        case AstTypeKind::Map:       return "map";
        case AstTypeKind::Set:       return "set";
        case AstTypeKind::Vector:    return "vector";
        case AstTypeKind::Stack:     return "stack";
        case AstTypeKind::Queue:     return "queue";
        case AstTypeKind::Json:      return "json";
        case AstTypeKind::I8:        return "i8";
        case AstTypeKind::I16:       return "i16";
        case AstTypeKind::I32:       return "i32";
        case AstTypeKind::I64:       return "i64";
        case AstTypeKind::U8:        return "u8";
        case AstTypeKind::U16:       return "u16";
        case AstTypeKind::U32:       return "u32";
        case AstTypeKind::U64:       return "u64";
        case AstTypeKind::F32:       return "f32";
        case AstTypeKind::Byte:      return "byte";
        case AstTypeKind::Char:      return "char";
        case AstTypeKind::FixedArray: return "fixed_array";
    }
    return "unknown";
}

/* ── Type annotation info carried on AST nodes ──
   Populated by TypeChecker during semantic analysis; consumed
   by codegen for type-aware IR generation.
   Added as part of H2 staged type-system migration (Phase A). */
struct AstTypeAnnotation {
    AstTypeKind kind { AstTypeKind::Unknown };
    bool        is_mutable { true };
    bool        is_nullable { false };

    /* For arrays: element type */
    AstTypeKind element_kind { AstTypeKind::Unknown };

    /* For fixed-size arrays: element type and size */
    int         fixed_array_size { 0 };
    AstTypeKind fixed_array_element_kind { AstTypeKind::Unknown };

    /* For user-defined types (struct, enum, class, interface): the type name */
    std::string type_name {};
};
