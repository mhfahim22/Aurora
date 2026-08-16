#include "compiler/codegen.hpp"
#include "compiler/ast.hpp"
#include "compiler/type_registry.hpp"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/DerivedTypes.h>

#include <unordered_map>
#include <string>
#include <vector>
#include <stdexcept>
#include <sstream>
#include <cstdlib>

/* ── Forward declaration for extern_type_to_llvm helper ── */
/* (defined in codegen_stmt.cpp, used here for struct field types) */
static llvm::Type* extern_type_to_llvm(llvm::LLVMContext& ctx, const std::string& type_name);

/* ── Helper: resolve LLVM type for a struct field, preferring AstTypeKind ── */
static llvm::Type* struct_field_llvm_type(llvm::LLVMContext& ctx, const StructFieldInfo& f) {
    if (f.type_kind != AstTypeKind::Unknown) {
        switch (f.type_kind) {
            case AstTypeKind::String:  return llvm::PointerType::getUnqual(ctx);
            case AstTypeKind::Float:   return llvm::Type::getDoubleTy(ctx);
            case AstTypeKind::F32:     return llvm::Type::getFloatTy(ctx);
            case AstTypeKind::Bool:
            case AstTypeKind::I8:
            case AstTypeKind::U8:
            case AstTypeKind::Byte:
            case AstTypeKind::Char:    return llvm::Type::getInt8Ty(ctx);
            case AstTypeKind::I16:
            case AstTypeKind::U16:     return llvm::Type::getInt16Ty(ctx);
            case AstTypeKind::I32:
            case AstTypeKind::U32:     return llvm::Type::getInt32Ty(ctx);
            case AstTypeKind::I64:
            case AstTypeKind::U64:     return llvm::Type::getInt64Ty(ctx);
            case AstTypeKind::FixedArray:
                if (!f.type_name.empty())
                    return extern_type_to_llvm(ctx, f.type_name);
                return llvm::Type::getInt64Ty(ctx);
            default:                   return llvm::Type::getInt64Ty(ctx);
        }
    }
    if (!f.type_name.empty())
        return extern_type_to_llvm(ctx, f.type_name);
    return llvm::Type::getInt64Ty(ctx);
}

/* ── struct type cache: struct_name → LLVM StructType ── */
static std::unordered_map<std::string, StructLayout> struct_layout_cache_;

/* Get or create an LLVM struct type for a registered struct */
static llvm::StructType* get_or_create_struct_type(
        llvm::LLVMContext& ctx,
        const std::string& struct_name) {

    /* Return from cache if already created */
    auto it = struct_layout_cache_.find(struct_name);
    if (it != struct_layout_cache_.end())
        return it->second.llvm_struct;

    const StructInfo* info = global_type_registry().get_struct(struct_name);
    if (!info)
        throw std::runtime_error("unknown struct '" + struct_name + "'");

    StructLayout layout;
    layout.field_names.reserve(info->fields.size());

    if (info->is_opaque) {
        /* Opaque types: forward-declared extern struct with no visible fields.
           Use [1 x i8] instead of empty struct — ensures unique address and
           non-zero size per C ABI requirement. */
        llvm::Type* opaque_body = llvm::ArrayType::get(llvm::Type::getInt8Ty(ctx), 1);
        layout.field_names.emplace_back("__opaque");
        layout.field_types.push_back(opaque_body);
    } else if (info->is_union) {
        /* For unions: store all fields, create LLVM type using largest field */
        /* Proper union: all fields start at offset 0, size = max field size */
        llvm::DataLayout dl(llvm::DataLayout(""));
        int64_t max_size = 0;
        int max_idx = 0;
        std::vector<llvm::Type*> all_field_tys;
        for (auto& f : info->fields) {
            llvm::Type* ft = extern_type_to_llvm(ctx, f.type_name);
            all_field_tys.push_back(ft);
            int64_t sz = dl.getTypeStoreSize(ft);
            if (sz > max_size) { max_size = sz; max_idx = static_cast<int>(all_field_tys.size()) - 1; }
        }
        /* Store ALL field names for field access */
        for (auto& f : info->fields)
            layout.field_names.push_back(f.name);
        /* Create union with the largest field as body (gives correct size/alignment) */
        layout.field_types.push_back(all_field_tys[max_idx]);
    } else {
        for (auto& f : info->fields) {
            layout.field_names.push_back(f.name);
            layout.field_types.push_back(struct_field_llvm_type(ctx, f));
        }
    }

    llvm::StructType* st = llvm::StructType::create(ctx, layout.field_types, struct_name);
    layout.llvm_struct = st;
    struct_layout_cache_[struct_name] = std::move(layout);
    return st;
}

/* ── Codegen: struct declaration ──
   struct declarations just register the LLVM type.
   No runtime code is emitted at declaration site. */
void Codegen::gen_struct_decl(const ASTNode* node) {
    if (!node) return;
    const std::string& sname = node->value;
    get_or_create_struct_type(ctx_, sname);
}

/* ── Enum type cache ── */
static std::unordered_map<std::string, llvm::StructType*> enum_type_cache_;

/* Get or create the tagged union LLVM type for an enum with data */
static llvm::StructType* get_or_create_enum_type(
        llvm::LLVMContext& ctx,
        const std::string& enum_name) {
    auto it = enum_type_cache_.find(enum_name);
    if (it != enum_type_cache_.end())
        return it->second;

    const EnumInfo* einfo = global_type_registry().get_enum(enum_name);
    if (!einfo || !einfo->has_data) {
        /* Fallback: plain i64 */
        auto* ty = llvm::StructType::create(ctx, llvm::Type::getInt64Ty(ctx), enum_name);
        enum_type_cache_[enum_name] = ty;
        return ty;
    }

    size_t max_fields = 0;
    for (auto& v : einfo->variants)
        if (v.fields.size() > max_fields)
            max_fields = v.fields.size();

    std::vector<llvm::Type*> members;
    members.push_back(llvm::Type::getInt64Ty(ctx)); /* discriminant */
    members.push_back(llvm::ArrayType::get(llvm::Type::getInt64Ty(ctx), max_fields));
    auto* st = llvm::StructType::create(ctx, members, enum_name);
    enum_type_cache_[enum_name] = st;
    return st;
}

/* ── Codegen: enum declaration ──
   Enums without data become i64 (the discriminant).
   Enums with data become a tagged union:
     %EnumName = type { i64, [MAX_DATA_WORDS x i64] } */
void Codegen::gen_enum_decl(const ASTNode* node) {
    if (!node) return;
    get_or_create_enum_type(ctx_, node->value);
}

/* ── Codegen: type alias ──
   Type aliases are compile-time only — no codegen needed. */
void Codegen::gen_type_alias(const ASTNode* node) {
    static_cast<void>(node);
}

/* ── Codegen: interface declaration ──
   Interfaces are compile-time only — no codegen needed
   (vtables will come in a later phase). */
void Codegen::gen_interface_decl(const ASTNode* node) {
    static_cast<void>(node);
}

/* ════════════════════════════════════════════════════════════
   Struct utility functions for use from other codegen files
   ════════════════════════════════════════════════════════════ */

/* Check if a struct type is already registered in the LLVM module */
bool codegen_struct_is_registered(const std::string& name) {
    return struct_layout_cache_.count(name) > 0;
}

/* Get the LLVM struct type (for use in alloca, GEP, etc.) */
llvm::StructType* codegen_get_struct_type(
        llvm::LLVMContext& ctx,
        const std::string& name) {
    return get_or_create_struct_type(ctx, name);
}

/* Get the field index within a struct */
int codegen_struct_field_index(const std::string& struct_name,
                                const std::string& field_name) {
    auto it = struct_layout_cache_.find(struct_name);
    if (it == struct_layout_cache_.end()) return -1;
    auto& names = it->second.field_names;
    for (int i = 0; i < static_cast<int>(names.size()); i++)
        if (names[i] == field_name) return i;
    return -1;
}

/* Allocate a struct on the stack and return a pointer */
llvm::Value* codegen_struct_alloca(
        llvm::LLVMContext& ctx,
        llvm::IRBuilder<>& builder,
        llvm::Function* cur_fn,
        const std::string& struct_name,
        const std::string& var_name) {
    llvm::StructType* st = get_or_create_struct_type(ctx, struct_name);
    llvm::AllocaInst* alloca = builder.CreateAlloca(st, nullptr, var_name);
    return alloca;
}

/* Standalone type mapper (accessible from codegen_types). */
/* Struct names are now recognized: they resolve to the named LLVM struct type. */
static llvm::Type* extern_type_to_llvm(llvm::LLVMContext& ctx, const std::string& type_name) {
    if (type_name == "int" || type_name == "Int")
        return llvm::Type::getInt64Ty(ctx);
    if (type_name == "i8")
        return llvm::Type::getInt8Ty(ctx);
    if (type_name == "i16")
        return llvm::Type::getInt16Ty(ctx);
    if (type_name == "i32")
        return llvm::Type::getInt32Ty(ctx);
    if (type_name == "i64" || type_name == "long")
        return llvm::Type::getInt64Ty(ctx);
    if (type_name == "u8" || type_name == "byte")
        return llvm::Type::getInt8Ty(ctx);
    if (type_name == "u16")
        return llvm::Type::getInt16Ty(ctx);
    if (type_name == "u32" || type_name == "uint")
        return llvm::Type::getInt32Ty(ctx);
    if (type_name == "u64" || type_name == "ulong")
        return llvm::Type::getInt64Ty(ctx);
    if (type_name == "f32" || type_name == "float" || type_name == "Float")
        return llvm::Type::getFloatTy(ctx);
    if (type_name == "f64" || type_name == "double")
        return llvm::Type::getDoubleTy(ctx);
    if (type_name == "char")
        return llvm::Type::getInt8Ty(ctx);
    if (type_name == "string" || type_name == "String" || type_name == "str" || type_name == "cstring"
        || type_name == "char*" || type_name == "void*"
        || type_name == "pointer" || type_name == "Pointer" || type_name == "ptr")
        return llvm::PointerType::get(ctx, 0);
    if (type_name == "bool" || type_name == "Bool")
        return llvm::Type::getInt8Ty(ctx); /* C99 _Bool */
    if (type_name == "void" || type_name == "Void")
        return llvm::Type::getVoidTy(ctx);
    /* Fixed-size array: [elem_type; size] */
    if (!type_name.empty() && type_name[0] == '[' && type_name.back() == ']') {
        auto semi = type_name.find(';');
        if (semi != std::string::npos) {
            std::string elem = type_name.substr(1, semi - 1);
            std::string ssize = type_name.substr(semi + 1, type_name.size() - semi - 2);
            int arr_size = std::atoi(ssize.c_str());
            if (arr_size > 0) {
                llvm::Type* elem_ty = extern_type_to_llvm(ctx, elem);
                return llvm::ArrayType::get(elem_ty, static_cast<unsigned>(arr_size));
            }
        }
    }
    /* Check if it's a registered struct type */
    if (global_type_registry().has_struct(type_name))
        return get_or_create_struct_type(ctx, type_name);
    /* Check if it's a registered enum type */
    if (global_type_registry().has_enum(type_name)) {
        const EnumInfo* einfo = global_type_registry().get_enum(type_name);
        if (einfo && einfo->has_data)
            return get_or_create_enum_type(ctx, type_name);
        return llvm::Type::getInt64Ty(ctx);
    }
    /* default: i64 */
    return llvm::Type::getInt64Ty(ctx);
}

/* Get pointer to a struct field (GEP).
   For union types, all fields start at offset 0 — we GEP to index 0
   (the union body of the largest type) then bitcast to the requested field type. */
llvm::Value* codegen_struct_gep(
        llvm::IRBuilder<>& builder,
        llvm::Value* struct_ptr,
        const std::string& struct_name,
        const std::string& field_name) {
    int idx = codegen_struct_field_index(struct_name, field_name);
    if (idx < 0)
        throw std::runtime_error("struct '" + struct_name +
            "' has no field '" + field_name + "'");

    /* Check if this is a union type */
    const StructInfo* sinfo = global_type_registry().get_struct(struct_name);
    bool is_union = sinfo && sinfo->is_union;

    llvm::LLVMContext& ctx = builder.getContext();
    llvm::StructType* st = get_or_create_struct_type(ctx, struct_name);

    if (is_union) {
        /* For unions: GEP to index 0 (the body), then bitcast to field type */
        llvm::Value* indices0[] = {
            llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0, true),
            llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0, true)
        };
        llvm::Value* body_ptr = builder.CreateGEP(st, struct_ptr, indices0, field_name + ".body");
        /* Look up field type from type registry */
        llvm::Type* field_ty = llvm::Type::getInt64Ty(ctx);
        if (sinfo) {
            for (auto& f : sinfo->fields) {
                if (f.name == field_name) {
                    field_ty = struct_field_llvm_type(ctx, f);
                    break;
                }
            }
        }
        llvm::PointerType* field_ptr_ty = llvm::PointerType::get(field_ty, 0);
        return builder.CreateBitCast(body_ptr, field_ptr_ty, field_name + ".unionptr");
    }

    llvm::Value* indices[] = {
        llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0, true),
        llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), idx, true)
    };
    return builder.CreateGEP(st, struct_ptr, indices, field_name + ".ptr");
}

/* ════════════════════════════════════════════════════════════
   Enum type helpers
   ════════════════════════════════════════════════════════════ */

/* Get or create the LLVM struct type for an enum (tagged union if has data) */
llvm::StructType* codegen_get_enum_type(llvm::LLVMContext& ctx, const std::string& enum_name) {
    return get_or_create_enum_type(ctx, enum_name);
}

/* Check if an enum has data-carrying variants */
bool codegen_enum_has_data(const std::string& enum_name) {
    const EnumInfo* einfo = global_type_registry().get_enum(enum_name);
    return einfo && einfo->has_data;
}