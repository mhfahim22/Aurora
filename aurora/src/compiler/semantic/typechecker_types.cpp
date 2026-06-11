#include "compiler/typechecker.hpp"
#include "compiler/type_registry.hpp"
#include "compiler/ast.hpp"

#include <sstream>

/* ── Register type alias: type T = BaseType ── */
void TypeChecker::register_type_alias(const ASTNode* node) {
    if (!node || node->type != NodeType::TypeAlias) return;

    std::string alias = node->value;
    std::string base  = node->left ? node->left->value : "";

    if (base.empty()) return;

    global_type_registry().register_alias(alias, base);
}

/* ── Register struct from AST ── */
void TypeChecker::register_struct(const ASTNode* node) {
    if (!node) return;
    if (node->type != NodeType::StructDecl && node->type != NodeType::ExternStruct
        && node->type != NodeType::ExternUnion) return;

    StructInfo info;
    info.name = node->value;
    info.is_union = (node->type == NodeType::ExternUnion);
    info.is_opaque = (node->type == NodeType::ExternStruct && !node->args);

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
                        field.is_string = true;
                    } else if (stmt->right->type == NodeType::Float) {
                        field.default_value = stmt->right->value;
                        field.is_float = true;
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
}

/* ── Register enum from AST ── */
void TypeChecker::register_enum(const ASTNode* node) {
    if (!node || node->type != NodeType::EnumDecl) return;

    EnumInfo info;
    info.name = node->value;

    int val = 0;
    const ASTNode* stmt = node->body.get();
    while (stmt) {
        EnumVariantInfo variant;
        variant.name  = stmt->value;
        variant.value = val++;

        info.variants.push_back(variant);
        stmt = stmt->next.get();
    }

    /* Register in user_types_ */
    UserTypeEntry entry;
    entry.kind = AuroraType::Enum;
    user_types_[info.name] = std::move(entry);

    global_type_registry().register_enum(std::move(info));
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
}

/* ── Type helpers ── */
bool TypeChecker::is_user_type(const std::string& name) const {
    return user_types_.count(name) > 0 || global_type_registry().is_user_type(name);
}

AuroraType TypeChecker::resolve_type_name(const std::string& name) const {
    if (name == "int")    return AuroraType::Int;
    if (name == "float")  return AuroraType::Float;
    if (name == "string") return AuroraType::String;
    if (name == "bool")   return AuroraType::Bool;
    if (name == "void")   return AuroraType::Void;
    if (name == "array")  return AuroraType::Array;

    /* Check type aliases */
    if (global_type_registry().is_alias(name)) {
        std::string resolved = global_type_registry().resolve_alias(name);
        return resolve_type_name(resolved);
    }

    /* Check user types */
    auto it = user_types_.find(name);
    if (it != user_types_.end()) return it->second.kind;

    return AuroraType::Unknown;
}
