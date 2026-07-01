#include "compiler/codegen.hpp"
#include "compiler/class_oop.hpp"
#include "compiler/ast.hpp"
#include "compiler/type_registry.hpp"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/DerivedTypes.h>

#include <unordered_map>
#include <string>
#include <vector>
#include <set>
#include <stdexcept>
#include <sstream>

/* ════════════════════════════════════════════════════════════
   Aurora OOP — LLVM IR Code Generator Extension (with vtable support)
   ════════════════════════════════════════════════════════════

   Object memory layout (heap allocated struct) — with optional vtable ptr:
   ┌─────────────────────────────────────────────┐
   │  vtable_ptr  (i8* — pointer to vtable)      │  ← if has_vtable
   │  field_0  (i64 / double / i8*)              │
   │  field_1  ...                               │
   │  ...                                        │
   └─────────────────────────────────────────────┘

   Vtable layout (global constant):
   ┌─────────────────────────────────────────────┐
   │  method_0_ptr  (i8* — function pointer)     │
   │  method_1_ptr  (i8* — function pointer)     │
   │  ...                                        │
   └─────────────────────────────────────────────┘

   প্রতিটা object একটা malloc করা struct pointer।
   vtable থাকলে first field হয় vtable pointer।
   field access → GEP (GetElementPtr) দিয়ে।
   method call  → vtable মাধ্যমে dispatch (virtual) বা direct (non-virtual)।
   ════════════════════════════════════════════════════════════ */

/* ── Vtable cache: class_name → LLVM global vtable variable ── */
static std::unordered_map<std::string, llvm::GlobalVariable*> vtable_cache_;

/* ── Check if a class has a vtable pointer field ── */
static bool class_has_vtable_ptr(const std::string& class_name) {
    const ClassInfo* cls = global_class_registry().get(class_name);
    return cls && cls->has_vtable;
}

/* ── Get or create the vtable global for a class ── */
static llvm::GlobalVariable* get_or_create_vtable(
        llvm::LLVMContext& ctx,
        llvm::Module&      module,
        const std::string& class_name) {
    auto it = vtable_cache_.find(class_name);
    if (it != vtable_cache_.end()) return it->second;

    auto vtable_methods = global_class_registry().compute_vtable(class_name);
    if (vtable_methods.empty()) {
        vtable_cache_[class_name] = nullptr;
        return nullptr;
    }

    std::vector<llvm::Constant*> vtable_entries;
    for (auto* m : vtable_methods) {
        llvm::Function* fn = module.getFunction(m->llvm_name);
        if (!fn) {
            /* H3-F: annotation-aware return type; params default to i8* (matching gen_class_oop) */
            auto* vt_ret_ty = ast_kind_to_abi_type(ctx, m->return_kind,
                llvm::Type::getInt64Ty(ctx));
            std::vector<llvm::Type*> param_types;
            param_types.push_back(llvm::PointerType::getUnqual(ctx)); /* self */
            for (size_t i = 0; i < m->params.size(); i++)
                param_types.push_back(llvm::PointerType::getUnqual(ctx));
            auto* fn_ty = llvm::FunctionType::get(vt_ret_ty, param_types, false);
            fn = llvm::Function::Create(
                fn_ty, llvm::Function::ExternalLinkage, m->llvm_name, module);
        }
        vtable_entries.push_back(llvm::ConstantExpr::getBitCast(
            fn, llvm::PointerType::getUnqual(ctx)));
    }

    llvm::ArrayType* vtable_type = llvm::ArrayType::get(
        llvm::PointerType::getUnqual(ctx), vtable_entries.size());

    llvm::Constant* vtable_init = llvm::ConstantArray::get(vtable_type, vtable_entries);

    auto* gv = new llvm::GlobalVariable(
        module, vtable_type, true, llvm::GlobalValue::InternalLinkage,
        vtable_init, "__vtable_" + class_name);

    vtable_cache_[class_name] = gv;
    return gv;
}

/* ── object pointer tracking: var_name → {class_name, llvm::Value*} ── */
struct ObjRecord {
    std::string  class_name;
    llvm::Value* ptr { nullptr };   /* struct pointer */
};

static std::unordered_map<std::string, ObjRecord> obj_map_;

/* ════════════════════════════════════════════════════════════
    H2 Phase D2: annotation-aware field type helpers
    ════════════════════════════════════════════════════════════ */
/* These helpers prefer the resolved AstTypeKind (type_kind) over legacy
   boolean flags (is_string, is_float). When type_kind is Unknown (not yet
   populated or from the legacy path), they fall back to the booleans. */
static llvm::Type* field_llvm_type(llvm::LLVMContext& ctx, const ClassFieldInfo& f) {
    switch (f.type_kind) {
        case AstTypeKind::String:  return llvm::PointerType::getUnqual(ctx);
        case AstTypeKind::Float:   return llvm::Type::getDoubleTy(ctx);
        default:                   return llvm::Type::getInt64Ty(ctx);
    }
}

static bool field_annotation_is_string(const ClassFieldInfo& f) {
    return f.type_kind == AstTypeKind::String;
}

static bool field_annotation_is_float(const ClassFieldInfo& f) {
    return f.type_kind == AstTypeKind::Float;
}

static bool method_annotation_returns_string(const ClassMethodInfo& m) {
    return m.return_kind == AstTypeKind::String;
}

/* ════════════════════════════════════════════════════════════
    LLVM struct type তৈরি করা
    ════════════════════════════════════════════════════════════ */

/* Helper: returns the field index adjusted for vtable ptr offset.
   If class has vtable, field indices start at 1 (index 0 = vtable ptr). */
static int field_index_with_vtable(const std::string& class_name, int field_pos) {
    const ClassInfo* cls = global_class_registry().get(class_name);
    if (cls && cls->has_vtable)
        return field_pos + 1;  /* skip vtable pointer at index 0 */
    return field_pos;
}

/* Helper: get the total number of user fields (excluding vtable ptr) */
static int count_user_fields(const std::string& class_name) {
    return static_cast<int>(global_class_registry().all_fields(class_name).size());
}

/* class এর জন্য LLVM StructType তৈরি করা */
static llvm::StructType* get_or_create_struct(
        llvm::LLVMContext&   ctx,
        const std::string&   class_name)
{
    /* already exist করলে return */
    if (auto* existing = llvm::StructType::getTypeByName(ctx, class_name))
        return existing;

    const ClassInfo* cls = global_class_registry().get(class_name);
    if (!cls) {
        throw std::runtime_error("Unknown class: " + class_name);
    }

    auto all_fields = global_class_registry().all_fields(class_name);

    std::vector<llvm::Type*> field_types;
    /* Vtable pointer goes first if class has virtual methods */
    if (cls->has_vtable) {
        field_types.push_back(llvm::PointerType::getUnqual(ctx)); /* vtable ptr */
    }
    for (auto& f : all_fields) {
        field_types.push_back(field_llvm_type(ctx, f));
    }

    return llvm::StructType::create(ctx, field_types, class_name);
}

/* ════════════════════════════════════════════════════════════
   Object তৈরি করা: p = Person("Alice", 25)
   ════════════════════════════════════════════════════════════ */
llvm::Value* oop_gen_new_object(
        llvm::LLVMContext&             ctx,
        llvm::IRBuilder<>&             builder,
        llvm::Module&                  module,
        const std::string&             class_name,
        const ASTNode*                 args_node,
        const std::string&             var_name,
        std::function<llvm::Value*(const ASTNode*)> gen_expr)
{
    const ClassInfo* cls = global_class_registry().get(class_name);
    if (!cls) {
        throw std::runtime_error("Line " + std::to_string(args_node ? args_node->src_line : 0)
                                 + ": undefined class '" + class_name + "'");
    }

    auto all_fields = global_class_registry().all_fields(class_name);
    llvm::StructType* struct_ty = get_or_create_struct(ctx, class_name);

    /* malloc দিয়ে heap allocate করা */
    llvm::Function* malloc_fn = module.getFunction("malloc");
    if (!malloc_fn) {
        auto* malloc_ty = llvm::FunctionType::get(
            llvm::PointerType::getUnqual(ctx),
            { llvm::Type::getInt64Ty(ctx) },
            false);
        malloc_fn = llvm::Function::Create(
            malloc_ty, llvm::Function::ExternalLinkage, "malloc", module);
    }

    /* sizeof struct */
    auto* size_val = llvm::ConstantExpr::getSizeOf(struct_ty);
    llvm::Value* obj_ptr = builder.CreateCall(malloc_fn, { size_val }, var_name + "_ptr");

    /* argument list collect করা */
    std::vector<llvm::Value*> arg_vals;
    /* named args detect করার জন্য — এখন positional handle করছি */
    const ASTNode* arg = args_node;
    while (arg) {
        arg_vals.push_back(gen_expr(arg));
        arg = arg->next.get();
    }

    /* Initialize vtable pointer if class has one */
    if (cls->has_vtable) {
        llvm::GlobalVariable* vtable_gv = get_or_create_vtable(ctx, module, class_name);
        if (vtable_gv) {
            llvm::Value* vtable_ptr = builder.CreateStructGEP(
                struct_ty, obj_ptr, 0, var_name + ".vtable_ptr");
            builder.CreateStore(vtable_gv, vtable_ptr);
        }
    }

    int vtable_offset = cls->has_vtable ? 1 : 0;

    /* fields initialize করা */
    for (int i = 0; i < static_cast<int>(all_fields.size()); i++) {
        auto& field = all_fields[i];

        int storage_idx = i + vtable_offset;

        /* GEP দিয়ে field pointer নাও */
        llvm::Value* field_ptr = builder.CreateStructGEP(
            struct_ty, obj_ptr, storage_idx, var_name + "." + field.name + "_ptr");

        llvm::Value* val = nullptr;

        if (i < static_cast<int>(arg_vals.size())) {
            /* argument থেকে value নাও */
            val = arg_vals[i];
        } else {
            /* default value use করো — H2 Phase D2: prefer annotation */
            bool is_str = field_annotation_is_string(field);
            bool is_flt = field_annotation_is_float(field);
            if (is_str) {
                /* default string constant */
                std::string def = field.default_value;
                if (def.size() >= 2 && def.front() == '"') def = def.substr(1, def.size()-2);
                val = builder.CreateGlobalStringPtr(def, ".str." + field.name);
            } else if (is_flt) {
                double dval = std::stod(field.default_value.empty() ? "0.0" : field.default_value);
                val = llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx), dval);
            } else {
                int64_t ival = 0;
                try { ival = std::stoll(field.default_value); } catch(...) {}
                val = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), ival, true);
            }
        }

        /* type mismatch হলে convert — H2 Phase D2: prefer annotation */
        {
            bool is_str = field_annotation_is_string(field);
            bool is_flt = field_annotation_is_float(field);
            if (is_flt && val->getType()->isIntegerTy())
                val = builder.CreateSIToFP(val, llvm::Type::getDoubleTy(ctx));
            else if (!is_str && !is_flt && val->getType()->isDoubleTy())
                val = builder.CreateFPToSI(val, llvm::Type::getInt64Ty(ctx));
        }

        builder.CreateStore(val, field_ptr);
    }

    /* obj_map_ এ register করা */
    obj_map_[var_name] = { class_name, obj_ptr };

    return obj_ptr;
}

/* ════════════════════════════════════════════════════════════
   Field read: output(p.name)
   ════════════════════════════════════════════════════════════ */
llvm::Value* oop_gen_field_get(
        llvm::LLVMContext& ctx,
        llvm::IRBuilder<>& builder,
        const std::string& obj_name,
        const std::string& field_name,
        int                src_line)
{
    auto it = obj_map_.find(obj_name);
    if (it == obj_map_.end()) return nullptr;

    const std::string& class_name = it->second.class_name;
    llvm::Value*       obj_ptr    = it->second.ptr;

    auto all_fields = global_class_registry().all_fields(class_name);
    llvm::StructType* struct_ty = get_or_create_struct(ctx, class_name);
    const ClassInfo* cls = global_class_registry().get(class_name);
    int vtable_offset = (cls && cls->has_vtable) ? 1 : 0;

    for (int i = 0; i < static_cast<int>(all_fields.size()); i++) {
        if (all_fields[i].name == field_name) {
            int storage_idx = i + vtable_offset;
            llvm::Value* field_ptr = builder.CreateStructGEP(
                struct_ty, obj_ptr, storage_idx, obj_name + "." + field_name + "_gep");

            llvm::Type* elem_ty = field_llvm_type(ctx, all_fields[i]);
            return builder.CreateLoad(elem_ty, field_ptr, obj_name + "." + field_name);
        }
    }

    throw std::runtime_error("Line " + std::to_string(src_line) +
                             ": class '" + class_name +
                             "' has no field '" + field_name + "'");
}

/* ════════════════════════════════════════════════════════════
   Field write: p.name = "Charlie"
   ════════════════════════════════════════════════════════════ */
void oop_gen_field_set(
        llvm::LLVMContext& ctx,
        llvm::IRBuilder<>& builder,
        const std::string& obj_name,
        const std::string& field_name,
        llvm::Value*       value,
        int                src_line)
{
    auto it = obj_map_.find(obj_name);
    if (it == obj_map_.end()) {
        throw std::runtime_error("Line " + std::to_string(src_line) +
                                 ": '" + obj_name + "' is not an object");
    }

    const std::string& class_name = it->second.class_name;
    llvm::Value*       obj_ptr    = it->second.ptr;

    auto all_fields = global_class_registry().all_fields(class_name);
    llvm::StructType* struct_ty = get_or_create_struct(ctx, class_name);
    const ClassInfo* cls = global_class_registry().get(class_name);
    int vtable_offset = (cls && cls->has_vtable) ? 1 : 0;

    for (int i = 0; i < static_cast<int>(all_fields.size()); i++) {
        if (all_fields[i].name == field_name) {
            int storage_idx = i + vtable_offset;
            llvm::Value* field_ptr = builder.CreateStructGEP(
                struct_ty, obj_ptr, storage_idx, obj_name + "." + field_name + "_set");

            /* type convert যদি দরকার হয় — H2 Phase D2: prefer annotation */
            {
                bool is_str = field_annotation_is_string(all_fields[i]);
                bool is_flt = field_annotation_is_float(all_fields[i]);
                if (is_flt && value->getType()->isIntegerTy())
                    value = builder.CreateSIToFP(value, llvm::Type::getDoubleTy(ctx));
                else if (!is_str && !is_flt && value->getType()->isDoubleTy())
                    value = builder.CreateFPToSI(value, llvm::Type::getInt64Ty(ctx));
            }

            builder.CreateStore(value, field_ptr);
            return;
        }
    }

    throw std::runtime_error("Line " + std::to_string(src_line) +
                             ": class '" + class_name +
                             "' has no field '" + field_name + "'");
}

/* ════════════════════════════════════════════════════════════
   Method call: p.greet()
   ════════════════════════════════════════════════════════════ */
/* Internal: method call given explicit class_name + obj_ptr (no obj_map_ lookup) */
static llvm::Value* oop_gen_method_call_impl(
        llvm::LLVMContext& ctx,
        llvm::IRBuilder<>& builder,
        llvm::Module&      module,
        const std::string& class_name,
        llvm::Value*       obj_ptr,
        const std::string& method_name,
        const ASTNode*     args_node,
        int                src_line,
        std::function<llvm::Value*(const ASTNode*)> gen_expr)
{
    const ClassMethodInfo* method =
        global_class_registry().find_method(class_name, method_name);

    if (!method) {
        throw std::runtime_error("Line " + std::to_string(src_line) +
                                 ": class '" + class_name +
                                 "' has no method '" + method_name + "'");
    }

    /* args build করা (gen_expr returns uniform i64 by default; conversion happens per-branch) */
    std::vector<llvm::Value*> call_args;
    call_args.push_back(obj_ptr); /* self — i8* */

    const ASTNode* arg = args_node;
    while (arg) {
        llvm::Value* v = gen_expr(arg);
        if (!v) v = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0);
        call_args.push_back(v);
        arg = arg->next.get();
    }

    /* ── Helper: convert value to match a target LLVM type ── */
    auto convert_to_type = [&](llvm::Value* val, llvm::Type* target) -> llvm::Value* {
        if (!val || val->getType() == target) return val;
        if (val->getType()->isIntegerTy() && target->isPointerTy())
            return builder.CreateIntToPtr(val, target, "arg_cast");
        if (val->getType()->isPointerTy() && target->isIntegerTy())
            return builder.CreatePtrToInt(val, target, "arg_cast");
        if (val->getType()->isIntegerTy() && target->isDoubleTy())
            return builder.CreateSIToFP(val, target, "arg_itof");
        if (val->getType()->isDoubleTy() && target->isIntegerTy())
            return builder.CreateFPToSI(val, target, "arg_ftoi");
        if (val->getType()->isPointerTy() && target->isPointerTy())
            return builder.CreateBitCast(val, target, "arg_pcast");
        return val;
    };

    /* ── Virtual dispatch via vtable ── */
    const ClassInfo* cls = global_class_registry().get(class_name);
    if (cls && method->vtable_index >= 0) {
        /* Load vtable pointer from object (index 0) */
        llvm::StructType* st = get_or_create_struct(ctx, class_name);
        llvm::Value* vtable_ptr_ptr = builder.CreateStructGEP(
            st, obj_ptr, 0, "vtable_gep");
        llvm::Value* vtable_ptr = builder.CreateLoad(
            llvm::PointerType::getUnqual(ctx), vtable_ptr_ptr, "vtable");

        /* GEP into vtable array to get method pointer */
        llvm::Value* method_ptr_ptr = builder.CreateGEP(
            llvm::ArrayType::get(llvm::PointerType::getUnqual(ctx), 1),
            builder.CreateBitCast(vtable_ptr,
                llvm::PointerType::getUnqual(
                    llvm::ArrayType::get(llvm::PointerType::getUnqual(ctx), 1)),
                "vtable_arr"),
            { llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0, true),
              llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), method->vtable_index, true) },
            "vtable_method_gep");

        llvm::Value* method_ptr = builder.CreateLoad(
            llvm::PointerType::getUnqual(ctx), method_ptr_ptr, "vtable_method");

        /* H3-F: use annotation-aware return type; params default to i8* */
        std::vector<llvm::Type*> fn_param_types;
        fn_param_types.push_back(llvm::PointerType::getUnqual(ctx)); /* self */
        /* Try to get the actual function to match its signature */
        llvm::Function* existing_fn = module.getFunction(method->llvm_name);
        llvm::FunctionType* fn_ty = nullptr;
        if (existing_fn) {
            fn_ty = existing_fn->getFunctionType();
        } else {
            auto* virt_ret_ty = ast_kind_to_abi_type(ctx, method->return_kind,
                llvm::Type::getInt64Ty(ctx));
            for (size_t i = 0; i < method->params.size(); i++)
                fn_param_types.push_back(llvm::PointerType::getUnqual(ctx));
            fn_ty = llvm::FunctionType::get(virt_ret_ty, fn_param_types, false);
        }

        /* Convert args to match fn_ty param types */
        for (size_t i = 1; i < call_args.size() && i < fn_ty->getNumParams(); i++)
            call_args[i] = convert_to_type(call_args[i], fn_ty->getParamType(i));

        llvm::Value* ret = builder.CreateCall(fn_ty, method_ptr, call_args, method_name + "_virt_ret");
        if (ret->getType()->isPointerTy() && !method_annotation_returns_string(*method))
            ret = builder.CreatePtrToInt(ret, llvm::Type::getInt64Ty(ctx), "ret_unbox");
        else if (ret->getType()->isDoubleTy())
            ret = builder.CreateBitCast(ret, llvm::Type::getInt64Ty(ctx), "ret_fp_unbox");
        return ret;
    }

    /* ── Direct (non-virtual) call ── */
    llvm::Function* fn = module.getFunction(method->llvm_name);
    if (!fn) {
        std::vector<llvm::Type*> param_types;
        param_types.push_back(llvm::PointerType::getUnqual(ctx)); /* self */
        for (int i = 0; i < static_cast<int>(method->params.size()); i++)
            param_types.push_back(llvm::PointerType::getUnqual(ctx));

        auto* dir_ret_ty = ast_kind_to_abi_type(ctx, method->return_kind,
            llvm::PointerType::getUnqual(ctx));
        auto* fn_ty = llvm::FunctionType::get(dir_ret_ty, param_types, false);
        fn = llvm::Function::Create(
            fn_ty, llvm::Function::ExternalLinkage, method->llvm_name, module);
    }

    /* Convert args to match fn's param types */
    {
        auto* fty = fn->getFunctionType();
        for (size_t i = 1; i < call_args.size() && i < fty->getNumParams(); i++)
            call_args[i] = convert_to_type(call_args[i], fty->getParamType(i));
    }

    llvm::Value* ret = builder.CreateCall(fn, call_args, method_name + "_ret");
    if (ret->getType()->isPointerTy() && !method_annotation_returns_string(*method))
        ret = builder.CreatePtrToInt(ret, llvm::Type::getInt64Ty(ctx), "ret_unbox");
    else if (ret->getType()->isDoubleTy())
        ret = builder.CreateBitCast(ret, llvm::Type::getInt64Ty(ctx), "ret_fp_unbox");
    return ret;
}

llvm::Value* oop_gen_method_call(
        llvm::LLVMContext& ctx,
        llvm::IRBuilder<>& builder,
        llvm::Module&      module,
        const std::string& obj_name,
        const std::string& method_name,
        const ASTNode*     args_node,
        int                src_line,
        std::function<llvm::Value*(const ASTNode*)> gen_expr)
{
    auto it = obj_map_.find(obj_name);
    if (it == obj_map_.end()) {
        throw std::runtime_error("Line " + std::to_string(src_line) +
                                 ": '" + obj_name + "' is not an object");
    }

    return oop_gen_method_call_impl(ctx, builder, module,
        it->second.class_name, it->second.ptr,
        method_name, args_node, src_line, gen_expr);
}

llvm::Value* oop_gen_method_call_ptr(
        llvm::LLVMContext& ctx,
        llvm::IRBuilder<>& builder,
        llvm::Module&      module,
        const std::string& class_name,
        llvm::Value*       obj_ptr,
        const std::string& method_name,
        const ASTNode*     args_node,
        int                src_line,
        std::function<llvm::Value*(const ASTNode*)> gen_expr)
{
    return oop_gen_method_call_impl(ctx, builder, module,
        class_name, obj_ptr,
        method_name, args_node, src_line, gen_expr);
}



/* ════════════════════════════════════════════════════════════
   self.field access inside method body
   ════════════════════════════════════════════════════════════ */
llvm::Value* oop_gen_self_field_get(
        llvm::LLVMContext& ctx,
        llvm::IRBuilder<>& builder,
        llvm::Value*       self_ptr,
        const std::string& class_name,
        const std::string& field_name,
        int                src_line)
{
    auto all_fields = global_class_registry().all_fields(class_name);
    llvm::StructType* struct_ty = get_or_create_struct(ctx, class_name);
    const ClassInfo* cls = global_class_registry().get(class_name);
    int vtable_offset = (cls && cls->has_vtable) ? 1 : 0;

    for (int i = 0; i < static_cast<int>(all_fields.size()); i++) {
        if (all_fields[i].name == field_name) {
            int storage_idx = i + vtable_offset;
            llvm::Value* gep = builder.CreateStructGEP(
                struct_ty, self_ptr, storage_idx, "self." + field_name + "_ptr");

            llvm::Type* elem_ty = field_llvm_type(ctx, all_fields[i]);
            return builder.CreateLoad(elem_ty, gep, "self." + field_name);
        }
    }

    throw std::runtime_error("Line " + std::to_string(src_line) +
                              ": self has no field '" + field_name + "'");
}

/* ════════════════════════════════════════════════════════════
   Utility: variable কি object?
   ════════════════════════════════════════════════════════════ */
bool oop_is_object(const std::string& var_name) {
    return obj_map_.count(var_name) > 0;
}

std::string oop_class_of(const std::string& var_name) {
    auto it = obj_map_.find(var_name);
    return (it != obj_map_.end()) ? it->second.class_name : "";
}

llvm::Value* oop_get_ptr(const std::string& var_name) {
    auto it = obj_map_.find(var_name);
    return (it != obj_map_.end()) ? it->second.ptr : nullptr;
}

void oop_register_object_ptr(const std::string& var_name,
                              const std::string& class_name,
                              llvm::Value*       ptr) {
    obj_map_[var_name] = { class_name, ptr };
}

void oop_clear() {
    obj_map_.clear();
}

/* ════════════════════════════════════════════════════════════
   self.field write inside a method body
   ════════════════════════════════════════════════════════════ */
void oop_gen_self_field_set(
        llvm::LLVMContext& ctx,
        llvm::IRBuilder<>& builder,
        llvm::Value*       self_ptr,
        const std::string& class_name,
        const std::string& field_name,
        llvm::Value*       value,
        int                src_line)
{
    auto all_fields = global_class_registry().all_fields(class_name);
    llvm::StructType* struct_ty = get_or_create_struct(ctx, class_name);
    const ClassInfo* cls = global_class_registry().get(class_name);
    int vtable_offset = (cls && cls->has_vtable) ? 1 : 0;

    for (int i = 0; i < static_cast<int>(all_fields.size()); i++) {
        if (all_fields[i].name == field_name) {
            int storage_idx = i + vtable_offset;
            llvm::Value* gep = builder.CreateStructGEP(
                struct_ty, self_ptr, storage_idx, "self." + field_name + "_set");

            {
                bool is_str = field_annotation_is_string(all_fields[i]);
                bool is_flt = field_annotation_is_float(all_fields[i]);
                if (is_flt && value->getType()->isIntegerTy())
                    value = builder.CreateSIToFP(value, llvm::Type::getDoubleTy(ctx));
                else if (!is_str && !is_flt && value->getType()->isDoubleTy())
                    value = builder.CreateFPToSI(value, llvm::Type::getInt64Ty(ctx));
            }

            builder.CreateStore(value, gep);
            return;
        }
    }

    throw std::runtime_error("Line " + std::to_string(src_line) +
                              ": self has no field '" + field_name + "'");
}

/* ════════════════════════════════════════════════════════════
   Clear vtable cache (call when starting a new compilation)
   ════════════════════════════════════════════════════════════ */
void oop_clear_vtable_cache() {
    vtable_cache_.clear();
}