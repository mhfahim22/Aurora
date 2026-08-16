#include "compiler/typechecker.hpp"
#include "compiler/type_registry.hpp"
#include "compiler/ast.hpp"
#include "compiler/ast/ast_type.hpp"

#include <sstream>

/* H2 Phase D2: inline annotation helper (annotate_node is static in typechecker.cpp) */
static void annotate_node_decl(const ASTNode* node, AstTypeKind kind, const std::string& type_name = "") {
    if (!node) return;
    auto& ann = const_cast<ASTNode*>(node)->type_annotation;
    ann.kind = kind;
    ann.type_name = type_name;
}

/* H2 Phase E-2: extern type name to AstTypeKind conversion (mirrors resolve_type_name) */
static AstTypeKind extern_type_name_to_kind(const std::string& name) {
    if (name == "int" || name == "Int")    return AstTypeKind::Int;
    if (name == "i8")    return AstTypeKind::I8;
    if (name == "i16")   return AstTypeKind::I16;
    if (name == "i32")   return AstTypeKind::I32;
    if (name == "i64")   return AstTypeKind::I64;
    if (name == "u8" || name == "byte") return AstTypeKind::U8;
    if (name == "u16")   return AstTypeKind::U16;
    if (name == "u32")   return AstTypeKind::U32;
    if (name == "u64")   return AstTypeKind::U64;
    if (name == "f32" || name == "float") return AstTypeKind::F32;
    if (name == "f64" || name == "Float" || name == "double") return AstTypeKind::Float;
    if (name == "char")  return AstTypeKind::Char;
    if (name == "string" || name == "String" || name == "str" || name == "cstring") return AstTypeKind::String;
    if (name == "bool" || name == "Bool")                                     return AstTypeKind::Bool;
    if (name == "void" || name == "Void")                                     return AstTypeKind::Void;
    if (!name.empty() && name[0] == '[')                                       return AstTypeKind::FixedArray;
    return AstTypeKind::Unknown;
}

/* Helper: parse [elem_type;size] string → kind + size */
static bool parse_fixed_array_str(const std::string& name, AstTypeKind& elem_kind, int& size) {
    if (name.size() < 5 || name[0] != '[' || name.back() != ']')
        return false;
    auto semi = name.find(';');
    if (semi == std::string::npos) return false;
    std::string elem_name = name.substr(1, semi - 1);
    std::string size_str = name.substr(semi + 1, name.size() - semi - 2);
    elem_kind = extern_type_name_to_kind(elem_name);
    size = std::atoi(size_str.c_str());
    return size > 0 && elem_kind != AstTypeKind::Unknown;
}

/* ── Register type alias: type T = BaseType ── */
void TypeChecker::register_type_alias(const ASTNode* node) {
    if (!node || node->type != NodeType::TypeAlias) return;

    std::string alias = node->value;
    std::string base  = node->left ? node->left->value : "";

    if (base.empty()) return;

    global_type_registry().register_alias(alias, base);
    /* H2 Phase D2: annotate with resolved base type */
    {
        AuroraType at = resolve_type_name(base);
        static const auto aurora_to_ast = [](AuroraType t) -> AstTypeKind {
            switch (t) {
                case AuroraType::Unknown:  return AstTypeKind::Unknown;
                case AuroraType::Int:      return AstTypeKind::Int;
                case AuroraType::Float:    return AstTypeKind::Float;
                case AuroraType::String:   return AstTypeKind::String;
                case AuroraType::Bool:     return AstTypeKind::Bool;
                case AuroraType::Struct:   return AstTypeKind::Struct;
                case AuroraType::Enum:      return AstTypeKind::Enum;
                case AuroraType::Interface: return AstTypeKind::Interface;
                case AuroraType::Array:    return AstTypeKind::Array;
                case AuroraType::Pointer:  return AstTypeKind::Pointer;
                case AuroraType::Void:     return AstTypeKind::Void;
                case AuroraType::Generic:  return AstTypeKind::Unknown;
                case AuroraType::I8:    return AstTypeKind::I8;
                case AuroraType::I16:   return AstTypeKind::I16;
                case AuroraType::I32:   return AstTypeKind::I32;
                case AuroraType::I64:   return AstTypeKind::I64;
                case AuroraType::U8:    return AstTypeKind::U8;
                case AuroraType::U16:   return AstTypeKind::U16;
                case AuroraType::U32:   return AstTypeKind::U32;
                case AuroraType::U64:   return AstTypeKind::U64;
                case AuroraType::F32:   return AstTypeKind::F32;
                case AuroraType::Byte:  return AstTypeKind::Byte;
                case AuroraType::Char:  return AstTypeKind::Char;
                default:                return AstTypeKind::Unknown;
            }
        };
        annotate_node_decl(node, aurora_to_ast(at), base);
    }
}

/* ── Register struct from AST ── */
void TypeChecker::register_struct(const ASTNode* node) {
    if (!node) return;
    if (node->type != NodeType::StructDecl && node->type != NodeType::ExternStruct
        && node->type != NodeType::ExternUnion) return;

    StructInfo info;
    info.name = node->value;
    info.is_union = (node->type == NodeType::ExternUnion) || (node->type == NodeType::StructDecl && node->is_union);
    info.is_opaque = (node->type == NodeType::ExternStruct && !node->args);

    /* Detect generic structs */
    if (node->template_params) {
        info.is_generic = true;
        const ASTNode* tp = node->template_params.get();
        while (tp) {
            info.generic_params.push_back(tp->value);
            tp = tp->next.get();
        }
    }

    int pos = 0;

    if (node->type == NodeType::ExternStruct || node->type == NodeType::ExternUnion) {
        /* Fields from args list: field1: type, field2: type, ... */
        const ASTNode* f = node->args.get();
        while (f) {
            StructFieldInfo field;
            field.name     = f->value;
            field.position = pos++;
            if (f->right) {
                field.type_name = f->right->value;
                field.type_kind = extern_type_name_to_kind(field.type_name);
                /* element_kind stays Unknown — extern types are scalar/opaque */
                /* ABI validation: check field type is C-compatible */
                {
                    std::string kind = (node->type == NodeType::ExternUnion) ? "extern union" : "extern struct";
                    std::string ctx = kind + " '" + node->value + "' field '" + f->value + "'";
                    validate_abi_type(field.type_name, node->src_line, ctx);
                }
            }
            field.default_value = "0";
            info.fields.push_back(field);
            f = f->next.get();
        }
    } else {
        /* StructDecl: fields from body as Assign nodes */
        const ASTNode* stmt = node->body.get();
        while (stmt) {
            if (stmt->type == NodeType::Assign && stmt->left) {
                StructFieldInfo field;
                field.name     = stmt->left->value;
                field.position = pos++;

                if (stmt->right) {
                    if (stmt->right->type == NodeType::Str) {
                        field.default_value = "\"" + stmt->right->value + "\"";
                        field.type_kind = AstTypeKind::String;
                    } else if (stmt->right->type == NodeType::Float) {
                        field.default_value = stmt->right->value;
                        field.type_kind = AstTypeKind::Float;
                    } else if (stmt->right->type == NodeType::Num) {
                        field.default_value = stmt->right->value;
                        field.type_kind = AstTypeKind::Int;
                    } else if (stmt->right->type == NodeType::Var) {
                        field.type_name = stmt->right->value;
                        field.default_value = "0";
                    } else {
                        field.default_value = stmt->right->value;
                    }
                } else {
                    field.default_value = "0";
                }

                info.fields.push_back(field);
            }
            stmt = stmt->next.get();
        }
    }

    /* Register in user_types_ for typechecking */
    UserTypeEntry entry;
    entry.kind = AuroraType::Struct;
    for (auto& f : info.fields) {
        entry.fields.emplace_back(f.name, AuroraType::Unknown);
    }
    user_types_[info.name] = std::move(entry);

    global_type_registry().register_struct(std::move(info));
    annotate_node_decl(node, AstTypeKind::Struct, node->value);  /* H2 Phase D2 */
}

/* ── Register enum from AST ── */
void TypeChecker::register_enum(const ASTNode* node) {
    if (!node || node->type != NodeType::EnumDecl) return;

    EnumInfo info;
    info.name = node->value;

    int val = 0;
    const ASTNode* stmt = node->args.get();
    while (stmt) {
        EnumVariantInfo variant;
        variant.name  = stmt->value;
        variant.value = val++;

        /* Extract variant data fields from stmt->right linked list */
        const ASTNode* f = stmt->right.get();
        while (f) {
            EnumVariantField field;
            field.type_name = f->value;
            field.type_kind = extern_type_name_to_kind(field.type_name);
            /* Fallback: try resolve_type_name for user types */
            if (field.type_kind == AstTypeKind::Unknown) {
                AuroraType rt = resolve_type_name(field.type_name);
                if (rt != AuroraType::Unknown)
                    field.type_kind = to_ast_type_kind(rt);
            }
            variant.fields.push_back(field);
            f = f->next.get();
        }

        if (!variant.fields.empty())
            info.has_data = true;

        info.variants.push_back(variant);
        stmt = stmt->next.get();
    }

    /* Register in user_types_ */
    UserTypeEntry entry;
    entry.kind = AuroraType::Enum;
    for (auto& v : info.variants) {
        if (!v.fields.empty()) {
            entry.fields.emplace_back(v.name, AuroraType::Unknown);
        }
    }
    user_types_[info.name] = std::move(entry);

    global_type_registry().register_enum(std::move(info));
    annotate_node_decl(node, AstTypeKind::Enum, node->value);
}

/* ── Register interface from AST ── */
void TypeChecker::register_interface(const ASTNode* node) {
    if (!node || node->type != NodeType::InterfaceDecl) return;

    InterfaceInfo info;
    info.name = node->value;

    if (node->left) {
        info.parent_name = node->left->value;
    }

    const ASTNode* stmt = node->body.get();
    while (stmt) {
        if (stmt->type == NodeType::Function) {
            InterfaceMethodInfo method;
            method.name = stmt->value;

            const ASTNode* param = stmt->args.get();
            while (param) {
                method.params.push_back(param->value);
                param = param->next.get();
            }

            info.methods.push_back(method);
        }
        stmt = stmt->next.get();
    }

    /* Register in user_types_ */
    UserTypeEntry entry;
    entry.kind = AuroraType::Interface;
    if (node->left) entry.parent_name = node->left->value;
    user_types_[info.name] = std::move(entry);

    global_type_registry().register_interface(std::move(info));
    annotate_node_decl(node, AstTypeKind::Interface, node->value);  /* H2 Phase D2 */
}

/* ── Type helpers ── */
bool TypeChecker::is_user_type(const std::string& name) const {
    return user_types_.count(name) > 0 || global_type_registry().is_user_type(name);
}

AuroraType TypeChecker::resolve_type_name(const std::string& name) {
    if (name == "int" || name == "Int") return AuroraType::Int;
    if (name == "i8")    return AuroraType::I8;
    if (name == "i16")   return AuroraType::I16;
    if (name == "i32")   return AuroraType::I32;
    if (name == "i64" || name == "long") return AuroraType::I64;
    if (name == "u8" || name == "byte") return AuroraType::U8;
    if (name == "u16")   return AuroraType::U16;
    if (name == "u32")   return AuroraType::U32;
    if (name == "u64")   return AuroraType::U64;
    if (name == "f32")   return AuroraType::F32;
    if (name == "float" || name == "Float" || name == "f64" || name == "double") return AuroraType::Float;
    if (name == "char")  return AuroraType::Char;
    if (name == "string" || name == "String" || name == "str") return AuroraType::String;
    if (name == "bool" || name == "Bool") return AuroraType::Bool;
    if (name == "void" || name == "Void") return AuroraType::Void;
    if (name == "array")  return AuroraType::Array;

    /* Check generic type parameters (T, U, etc. in generic function bodies) */
    if (is_generic_param(name))
        return AuroraType::Generic;

    /* Check type aliases */
    if (global_type_registry().is_alias(name)) {
        std::string resolved = global_type_registry().resolve_alias(name);
        return resolve_type_name(resolved);
    }

    /* Check user types */
    auto it = user_types_.find(name);
    if (it != user_types_.end()) return it->second.kind;

    /* Check global type registry for enums/structs registered without AST nodes */
    if (global_type_registry().has_enum(name)) return AuroraType::Enum;
    if (global_type_registry().has_struct(name)) return AuroraType::Struct;

    return AuroraType::Unknown;
}

/* ── Instantiate a generic struct ── */
std::string TypeChecker::instantiate_generic_struct(const std::string& struct_name, const ASTNode* template_args) {
    const StructInfo* sinfo = global_type_registry().get_struct(struct_name);
    if (!sinfo || !sinfo->is_generic) return "";

    /* Build type param → concrete type map */
    std::unordered_map<std::string, AuroraType> type_map;
    {
        const ASTNode* tp = nullptr;
        {
            /* Find the AST node with template_params (look in user_types_ to get original struct def) */
            auto ut = user_types_.find(struct_name);
            if (ut == user_types_.end()) return "";
        }
        /* Match generic params to template args by position */
        size_t tp_idx = 0;
        const ASTNode* ta = template_args;
        while (ta && tp_idx < sinfo->generic_params.size()) {
            type_map[sinfo->generic_params[tp_idx]] = resolve_type_name(ta->value);
            tp_idx++;
            ta = ta->next.get();
        }
        if (tp_idx != sinfo->generic_params.size()) return "";
    }

    /* Build mangled name: Box__Int__Float */
    std::string mangled = struct_name;
    {
        const ASTNode* ta = template_args;
        while (ta) {
            mangled += "__" + ta->value;
            ta = ta->next.get();
        }
    }

    if (global_type_registry().has_struct(mangled))
        return mangled;

    /* Create concrete struct info */
    StructInfo concrete;
    concrete.name = mangled;
    concrete.is_generic = false;

    /* Resolve field types */
    for (auto& f : sinfo->fields) {
        StructFieldInfo field;
        field.name = f.name;
        field.position = static_cast<int>(concrete.fields.size());
        field.default_value = f.default_value;

        if (!f.type_name.empty()) {
            auto tm = type_map.find(f.type_name);
            if (tm != type_map.end()) {
                field.type_kind = to_ast_type_kind(tm->second);
                field.type_name = f.type_name;
            } else {
                field.type_name = f.type_name;
                field.type_kind = to_ast_type_kind(resolve_type_name(f.type_name));
            }
        }
        concrete.fields.push_back(field);
    }

    /* Register in global type registry */
    global_type_registry().register_struct(concrete);

    /* Register in user_types_ for type resolution */
    UserTypeEntry entry;
    entry.kind = AuroraType::Struct;
    for (auto& f : concrete.fields) {
        AuroraType ft = AuroraType::Unknown;
        if (!f.type_name.empty()) {
            auto tm = type_map.find(f.type_name);
            if (tm != type_map.end())
                ft = tm->second;
            else
                ft = resolve_type_name(f.type_name);
        }
        entry.fields.emplace_back(f.name, ft);
    }
    user_types_[mangled] = std::move(entry);

    return mangled;
}
