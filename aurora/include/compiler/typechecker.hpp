#pragma once
#include "compiler/ast.hpp"
#include "compiler/ast/ast_type.hpp"

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

enum class AuroraType {
    Unknown,
    Void,
    Int,
    Float,
    String,
    Bool,
    Array,
    Struct,
    Function,
    Class,
    Enum,
    Interface,
    Tuple,
    Pointer,

    /* ── Phase 2: Collection types ── */
    List,
    Map,
    Set,
    Vector,
    Stack,
    Queue,
    Json,

    /* Generic type parameter (unknown at analysis time, resolved at instantiation) */
    Generic,
};

struct TypeInfo {
    AuroraType base_type      { AuroraType::Unknown };
    std::string user_type_name;     /* for struct/class names */
    int         array_dim      { 1 };
    bool        is_mutable     { true };
    bool        is_optional    { false };

    /* For struct types — ordered field types */
    std::vector<std::pair<std::string, AuroraType>> fields;

    /* For class types */
    std::string class_name;
};

inline const char* aurora_type_name(AuroraType t) {
    switch (t) {
        case AuroraType::Unknown: return "unknown";
        case AuroraType::Void:    return "void";
        case AuroraType::Int:     return "int";
        case AuroraType::Float:   return "float";
        case AuroraType::String:  return "string";
        case AuroraType::Bool:    return "bool";
        case AuroraType::Array:   return "array";
        case AuroraType::Struct:  return "struct";
        case AuroraType::Function: return "function";
        case AuroraType::Class:   return "class";
        case AuroraType::Enum:    return "enum";
        case AuroraType::Interface: return "interface";
        case AuroraType::Tuple:   return "tuple";
        case AuroraType::List:    return "list";
        case AuroraType::Map:     return "map";
        case AuroraType::Set:     return "set";
        case AuroraType::Vector:  return "vector";
        case AuroraType::Stack:   return "stack";
        case AuroraType::Queue:   return "queue";
        case AuroraType::Json:    return "json";
        case AuroraType::Pointer: return "pointer";
        case AuroraType::Generic: return "generic";
    }
    return "unknown";
}

/* ── AuroraType → AstTypeKind conversion ── */
inline AstTypeKind to_ast_type_kind(AuroraType t) {
    switch (t) {
        case AuroraType::Unknown:  return AstTypeKind::Unknown;
        case AuroraType::Void:     return AstTypeKind::Void;
        case AuroraType::Int:      return AstTypeKind::Int;
        case AuroraType::Float:    return AstTypeKind::Float;
        case AuroraType::String:   return AstTypeKind::String;
        case AuroraType::Bool:     return AstTypeKind::Bool;
        case AuroraType::Array:    return AstTypeKind::Array;
        case AuroraType::Struct:   return AstTypeKind::Struct;
        case AuroraType::Function: return AstTypeKind::Function;
        case AuroraType::Class:    return AstTypeKind::Class;
        case AuroraType::Enum:      return AstTypeKind::Enum;
        case AuroraType::Interface: return AstTypeKind::Interface;
        case AuroraType::Tuple:    return AstTypeKind::Tuple;
        case AuroraType::Pointer:  return AstTypeKind::Pointer;
        case AuroraType::List:     return AstTypeKind::List;
        case AuroraType::Map:       return AstTypeKind::Map;
        case AuroraType::Set:       return AstTypeKind::Set;
        case AuroraType::Vector:   return AstTypeKind::Vector;
        case AuroraType::Stack:    return AstTypeKind::Stack;
        case AuroraType::Queue:    return AstTypeKind::Queue;
        case AuroraType::Json:     return AstTypeKind::Json;
        case AuroraType::Generic:  return AstTypeKind::Unknown;
    }
    return AstTypeKind::Unknown;
}

/* ── ABI validation ── */
/* Check if a type name is valid for C FFI (int, float, string, struct, etc.) */
bool     is_valid_abi_type(const std::string& name);
void     validate_abi_type(const std::string& name, int line, const std::string& context);

struct TypeError : std::runtime_error {
    int line;

    TypeError(const std::string& msg, int ln)
        : std::runtime_error(msg), line(ln) {}
};

struct FunctionTypeInfo {
    std::vector<AuroraType> params;
    AuroraType              result { AuroraType::Unknown };
    std::string             class_name;  /* if method, the owning class */
    bool                    is_vararg { false };

    /* Generic/template support */
    bool is_generic{ false };
    std::vector<std::string> generic_params;  /* e.g. ["T", "U"] for function foo[T, U](...) */
    const ASTNode* generic_ast_node{ nullptr };  /* original AST node for monomorphization */

    /* Build a mangled name for a monomorphized instance.
       The function name is tracked externally in the TypeChecker's fns_ map. */
    std::string instantiated_name(const std::string& fn_name,
                                  const std::vector<std::string>& type_args) const {
        std::string result = class_name.empty() ? fn_name : class_name + "__" + fn_name;
        for (auto& ta : type_args)
            result += "__" + ta;
        return result;
    }
};

/* Concrete generic instantiation record — used by codegen to emit monomorphized functions */
struct ConcreteGenericInfo {
    const ASTNode* generic_node{ nullptr };     /* original generic function AST node */
    std::vector<AstTypeKind> param_kinds{};     /* resolved param type kinds */
    AstTypeKind result_kind{ AstTypeKind::Unknown };
};

class TypeChecker {
public:
    void analyse(const ASTNode* root);

    /* Type query */
    AuroraType get_type(const std::string& var_name) const;
    bool has_type(const std::string& var_name) const;
    std::string type_describe(const std::string& var_name) const;

    /* OOP: get current class context */
    const std::string& current_class_name() const { return current_class_name_; }
    bool is_inside_class() const { return !current_class_name_.empty(); }

    /* Access concrete generic instantiations (for codegen) */
    const std::unordered_map<std::string, ConcreteGenericInfo>& get_concrete_generics() const {
        return concrete_generics_;
    }

    /* Instantiate a generic struct: create concrete struct type with mangled name */
    std::string instantiate_generic_struct(const std::string& struct_name, const ASTNode* template_args);

private:
    std::vector<std::unordered_map<std::string, AuroraType>> scopes_; /* TODO: add variable shadowing diagnostic (future) */
    std::unordered_map<std::string, AstTypeKind> var_element_types_;  /* H2 Phase E-1: element kind per variable */
    std::unordered_map<std::string, FunctionTypeInfo> functions_;

    /* Per-variable struct type name tracking (e.g. "b" → "Box__Int") */
    std::unordered_map<std::string, std::string> var_struct_types_;

    /* Per-variable enum type name tracking (e.g. "col" → "Color") */
    std::unordered_map<std::string, std::string> var_enum_types_;

    /* User-defined type registry (struct/class/enum/interface name -> TypeInfo) */
    struct UserTypeEntry {
        AuroraType kind;       /* Struct, Class, Enum, or Interface */
        std::vector<std::pair<std::string, AuroraType>> fields;  /* members/variants */
        std::string parent_name;   /* for interface extends */
    };
    std::unordered_map<std::string, UserTypeEntry> user_types_;

    /* Type alias registry: alias_name -> resolved AuroraType */
    std::unordered_map<std::string, AuroraType> type_aliases_;

    AuroraType current_return_type_ { AuroraType::Unknown };
    bool       inside_function_     { false };

    /* OOP context tracking */
    std::string current_class_name_ {};
    int         current_class_depth_ { 0 };

    void push_scope();
    void pop_scope();

    void define_var(const std::string& name, AuroraType type);
    void define_var_elem(const std::string& name, AuroraType type, AstTypeKind elem_kind);
    AuroraType lookup_var(const std::string& name, int line) const;
    AstTypeKind lookup_var_elem(const std::string& name) const;
    std::string lookup_var_enum(const std::string& name) const;

    void walk_block(const ASTNode* node);
    void walk_stmt(const ASTNode* node);
    AuroraType infer_expr(const ASTNode* node);

    void define_match_pattern_vars(const ASTNode* pattern);
    void register_functions(const ASTNode* node);
    void register_struct(const ASTNode* node);
    AuroraType infer_binop(const ASTNode* node);
    AuroraType infer_unary(const ASTNode* node);
    AuroraType infer_call(const ASTNode* node);

    /* Phase 1 type system helpers */
    void register_type_alias(const ASTNode* node);
    void register_enum(const ASTNode* node);
    void register_interface(const ASTNode* node);
    bool is_user_type(const std::string& name) const;
    AuroraType resolve_type_name(const std::string& name);

    /* Generic type param scope helpers */
    void push_generic_params(const ASTNode* template_params);
    void pop_generic_params(const ASTNode* template_params);
    bool is_generic_param(const std::string& name) const;

    bool is_numeric(AuroraType type) const;
    bool is_boolish(AuroraType type) const;
    AuroraType common_numeric(AuroraType left, AuroraType right, int line) const;
    void expect_assignable(AuroraType target, AuroraType value, int line) const;

    /* Lambda capture tracking */
    std::unordered_set<std::string> var_refs_;
    bool tracking_var_refs_ = false;

    /* Concrete generic instantiations: mangled_name → info (for codegen) */
    std::unordered_map<std::string, ConcreteGenericInfo> concrete_generics_;

    /* Active generic type parameter names (for body analysis of generic functions/structs) */
    std::unordered_set<std::string> active_generic_params_;
};
