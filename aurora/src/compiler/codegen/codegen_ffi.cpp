#include "compiler/codegen.hpp"
#include "compiler/type_registry.hpp"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <iostream>

llvm::Type* Codegen::extern_type_to_llvm(const std::string& type_name) {
    if (type_name == "int" || type_name == "i64" || type_name == "Int" || type_name == "u64")
        return i64_ty();
    if (type_name == "i32" || type_name == "u32")
        return llvm::Type::getInt32Ty(ctx_);
    if (type_name == "i16")
        return llvm::Type::getInt16Ty(ctx_);
    if (type_name == "i8" || type_name == "char")
        return llvm::Type::getInt8Ty(ctx_);
    if (type_name == "float" || type_name == "f64" || type_name == "Float" || type_name == "double")
        return llvm::Type::getDoubleTy(ctx_);
    if (type_name == "f32")
        return llvm::Type::getFloatTy(ctx_);
    if (type_name == "string" || type_name == "String" || type_name == "str" || type_name == "cstring"
        || type_name == "char*" || type_name == "void*"
        || type_name == "pointer" || type_name == "Pointer" || type_name == "ptr")
        return i8ptr_ty();
    if (type_name == "bool" || type_name == "Bool")
        return llvm::Type::getInt8Ty(ctx_); /* C99 _Bool is 1 byte on all platforms */
    if (type_name == "void" || type_name == "Void")
        return llvm::Type::getVoidTy(ctx_);
    /* Check for registered struct types */
    if (global_type_registry().has_struct(type_name))
        return codegen_get_struct_type(ctx_, type_name);
    /* Check for registered enum types */
    if (global_type_registry().has_enum(type_name))
        return i64_ty();
    /* default: i64 (Aurora's generic int type) */
    return i64_ty();
}

/* ── Map calling convention string to LLVM CallingConv::ID ── */
static llvm::CallingConv::ID parse_calling_conv(const std::string& cc) {
    if (cc == "stdcall")  return llvm::CallingConv::X86_StdCall;
    if (cc == "fastcall") return llvm::CallingConv::X86_FastCall;
    if (cc == "thiscall") return llvm::CallingConv::X86_ThisCall;
    if (cc == "vectorcall") return llvm::CallingConv::X86_VectorCall;
    if (cc == "win64")    return llvm::CallingConv::Win64;
    if (cc == "sysv64")   return llvm::CallingConv::X86_64_SysV;
    /* Default: C calling convention */
    return llvm::CallingConv::C;
}

void Codegen::gen_extern_fn(const ASTNode* node) {
    if (!node) return;
    const std::string& fname = node->value;
    if (fname.empty()) return;

    /* Parse calling convention */
    llvm::CallingConv::ID call_conv = parse_calling_conv(node->calling_conv);

    /* Build parameter types + track callback signatures + cstring info */
    std::vector<llvm::Type*> param_types;
    std::vector<CallbackSig> cb_sigs;
    ExternStringInfo str_info;
    const ASTNode* param = node->args.get();
    int pidx = 0;
    while (param) {
        if (param->right && param->right->type == NodeType::FunctionType) {
            const ASTNode* ct  = param->right.get();
            std::vector<llvm::Type*> cb_param_tys;
            const ASTNode* cp = ct->args.get();
            while (cp) {
                cb_param_tys.push_back(extern_type_to_llvm(cp->value));
                cp = cp->next.get();
            }
            llvm::Type* cb_ret = llvm::Type::getVoidTy(ctx_);
            if (ct->left)
                cb_ret = extern_type_to_llvm(ct->left->value);
            auto* cb_fn_type = llvm::FunctionType::get(cb_ret, cb_param_tys, false);
            param_types.push_back(llvm::PointerType::get(cb_fn_type, 0));
            cb_sigs.push_back({pidx, cb_fn_type});
        } else {
            std::string ptype_name = "int";
            if (param->right)
                ptype_name = param->right->value;
            param_types.push_back(extern_type_to_llvm(ptype_name));
            /* Track cstring/pointer params for auto conversion & cost checking */
            if (ptype_name == "cstring" || ptype_name == "char*" ||
                ptype_name == "string" || ptype_name == "String" || ptype_name == "str")
                str_info.param_indices.push_back(pidx);
            if (ptype_name == "pointer" || ptype_name == "Pointer" || ptype_name == "ptr" ||
                ptype_name == "void*" || ptype_name == "cstring" || ptype_name == "char*")
                str_info.has_pointer_param = true;
        }
        pidx++;
        param = param->next.get();
    }
    if (!cb_sigs.empty())
        extern_callback_sigs_[fname] = std::move(cb_sigs);

    /* Return type */
    std::string ret_type_name = "void";
    if (node->left)
        ret_type_name = node->left->value;
    llvm::Type* ret_type = extern_type_to_llvm(ret_type_name);
    /* Track if return type is cstring or pointer */
    if (ret_type_name == "cstring" || ret_type_name == "char*" ||
        ret_type_name == "string" || ret_type_name == "String" || ret_type_name == "str")
        str_info.return_is_cstring = true;
    if (ret_type_name == "pointer" || ret_type_name == "Pointer" || ret_type_name == "ptr" ||
        ret_type_name == "void*")
        str_info.has_pointer_param = true;
    if (!str_info.param_indices.empty() || str_info.return_is_cstring)
        extern_string_info_[fname] = std::move(str_info);

    /* ════════════════════════════════════════════════════════════════════
       Cost checking: verify declared @cost matches actual types
     ════════════════════════════════════════════════════════════════════ */
    if (!node->cost_level.empty()) {
        /* Raw pointer (void*) is zero-cost at the FFI boundary — just a register.
           cstring/char* requires string marshaling = alloc.
           Callbacks require trampoline = indirection. */
        bool needs_marshal = str_info.return_is_cstring || !str_info.param_indices.empty();
        bool has_callback = !cb_sigs.empty();

        std::string actual_cost;
        if (has_callback)          actual_cost = "indirection";
        else if (needs_marshal)    actual_cost = "alloc";
        else                       actual_cost = "zero";

        if (node->cost_level == "zero" && needs_marshal) {
            std::cerr << "\033[1;33mWarning\033[0m: extern function '" << fname
                      << "' declared @cost(zero) but uses string marshaling"
                      << " (actual cost: " << actual_cost << ")\n";
        } else if (node->cost_level == "zero" && has_callback) {
            std::cerr << "\033[1;33mWarning\033[0m: extern function '" << fname
                      << "' declared @cost(zero) but uses callback parameters"
                      << " (actual cost: " << actual_cost << ")\n";
        } else if (node->cost_level == "alloc" && has_callback) {
            std::cerr << "\033[1;33mWarning\033[0m: extern function '" << fname
                      << "' declared @cost(alloc) but uses callback parameters"
                      << " (actual cost: " << actual_cost << ")\n";
        }
    }

    /* ── Case 1: Plain extern (no library) — create LLVM external declaration ── */
    if (!node->right) {
        auto* fn_type = llvm::FunctionType::get(ret_type, param_types, node->is_vararg);
        llvm::Function* fn = llvm::Function::Create(
            fn_type, llvm::Function::ExternalLinkage, fname, module_.get());
        fn->setCallingConv(call_conv);
        /* Add to @llvm.compiler.used so StripDeadPrototypes doesn't remove it */
        {
            auto* i8ptr = llvm::PointerType::getUnqual(ctx_);
            auto* bitcast = llvm::ConstantExpr::getBitCast(fn, i8ptr);
            llvm::GlobalVariable* used_gv = module_->getGlobalVariable("llvm.compiler.used");
            if (used_gv) {
                /* Append to existing array */
                auto* old_arr = llvm::cast<llvm::ConstantArray>(used_gv->getInitializer());
                std::vector<llvm::Constant*> elems;
                for (auto& op : old_arr->operands())
                    elems.push_back(llvm::cast<llvm::Constant>(&op));
                elems.push_back(bitcast);
                auto* arr_ty = llvm::ArrayType::get(i8ptr, elems.size());
                used_gv->setInitializer(llvm::ConstantArray::get(arr_ty, elems));
            } else {
                auto* arr_ty = llvm::ArrayType::get(i8ptr, 1);
                auto* arr = llvm::ConstantArray::get(arr_ty, { bitcast });
                new llvm::GlobalVariable(
                    *module_, arr_ty, false,
                    llvm::GlobalValue::AppendingLinkage,
                    arr, "llvm.compiler.used");
            }
        }
        std::cerr << "[ffi] extern function '" << fname << "' declared\n";
        return;
    }

    /* ── Case 2: extern "libname" — generate lazy dynamic loading wrapper ── */
    const std::string& libname = node->right->value;
    auto* ptr_type = llvm::PointerType::get(ctx_, 0);

    /* Create global function pointer (i8*) initialized to null */
    auto* fn_ptr_global = new llvm::GlobalVariable(
        *module_, ptr_type, false,
        llvm::GlobalValue::InternalLinkage,
        llvm::ConstantPointerNull::get(ptr_type),
        fname + ".dl_ptr");

    /* Create wrapper function with proper signature */
    auto* fn_type = llvm::FunctionType::get(ret_type, param_types, node->is_vararg);
    auto* wrapper = llvm::Function::Create(
        fn_type, llvm::Function::InternalLinkage, fname, module_.get());
    wrapper->setCallingConv(call_conv);

    /* Save current builder state */
    llvm::BasicBlock* saved_block = builder_->GetInsertBlock();

    /* Create basic blocks for the wrapper */
    auto* entry    = llvm::BasicBlock::Create(ctx_, "dl.entry", wrapper);
    auto* resolve  = llvm::BasicBlock::Create(ctx_, "dl.resolve", wrapper);
    auto* call     = llvm::BasicBlock::Create(ctx_, "dl.call", wrapper);

    /* ── entry block: check if ptr is null ── */
    builder_->SetInsertPoint(entry);
    llvm::Value* loaded = builder_->CreateLoad(ptr_type, fn_ptr_global, "dl.ptr");
    llvm::Value* is_null = builder_->CreateIsNull(loaded, "dl.isnull");
    builder_->CreateCondBr(is_null, resolve, call);

    /* ── resolve block: call aurora_dl_resolve or aurora_ecosystem_resolve ── */
    builder_->SetInsertPoint(resolve);

    /* Determine if this is an ecosystem extern (pypi_, npm_, cargo_, native_) */
    bool is_ecosystem = (libname.size() > 5 && libname.substr(0, 5) == "pypi_")
                     || (libname.size() > 4 && libname.substr(0, 4) == "npm_")
                     || (libname.size() > 6 && libname.substr(0, 6) == "cargo_")
                     || (libname.size() > 7 && libname.substr(0, 7) == "native_");

    const char* resolve_fn_name = is_ecosystem
        ? "aurora_ecosystem_resolve" : "aurora_dl_resolve";

    llvm::Function* resolve_fn = module_->getFunction(resolve_fn_name);
    if (!resolve_fn) {
        resolve_fn = llvm::Function::Create(
            llvm::FunctionType::get(ptr_type, { ptr_type, ptr_type }, false),
            llvm::Function::ExternalLinkage, resolve_fn_name, module_.get());
    }

    llvm::Value* lib_cstr = builder_->CreateGlobalStringPtr(libname, "dl.lib");
    llvm::Value* fn_cstr  = builder_->CreateGlobalStringPtr(fname, "dl.fn");
    llvm::Value* resolved = builder_->CreateCall(resolve_fn, { lib_cstr, fn_cstr }, "dl.resolved");

    /* Error recovery: if resolved is null, call aurora_panic and abort */
    auto* err_check = llvm::BasicBlock::Create(ctx_, "dl.errcheck", wrapper);
    auto* err_blk   = llvm::BasicBlock::Create(ctx_, "dl.error", wrapper);
    auto* resolve_cont = llvm::BasicBlock::Create(ctx_, "dl.resolve_cont", wrapper);

    builder_->CreateBr(err_check);

    builder_->SetInsertPoint(err_check);
    llvm::Value* is_err = builder_->CreateIsNull(resolved, "dl.errnull");
    builder_->CreateCondBr(is_err, err_blk, resolve_cont);

    builder_->SetInsertPoint(err_blk);
    std::string err_msg = "[ffi] ERROR: failed to resolve '" + fname + "' from "
        + (is_ecosystem ? "ecosystem " : "library '") + libname
        + (is_ecosystem ? "" : "'") + "\n";
    llvm::Value* err_msg_str = builder_->CreateGlobalStringPtr(err_msg, "dl.errmsg");
    llvm::Function* panic_fn = module_->getFunction("aurora_panic");
    if (!panic_fn) {
        auto* void_ty = llvm::Type::getVoidTy(ctx_);
        auto* pt = llvm::PointerType::getUnqual(ctx_);
        auto* ft = llvm::FunctionType::get(void_ty, { pt }, false);
        panic_fn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "aurora_panic", module_.get());
    }
    builder_->CreateCall(panic_fn, { err_msg_str });
    builder_->CreateUnreachable();

    builder_->SetInsertPoint(resolve_cont);
    builder_->CreateStore(resolved, fn_ptr_global);
    builder_->CreateBr(call);

    /* ── call block: call through resolved function pointer ── */
    builder_->SetInsertPoint(call);

    /* PHI node: entry → loaded (pre-resolved), resolve_cont → resolved (just resolved) */
    llvm::PHINode* fn_ptr_phi = builder_->CreatePHI(ptr_type, 2, "dl.fnphi");
    fn_ptr_phi->addIncoming(loaded, entry);
    fn_ptr_phi->addIncoming(resolved, resolve_cont);

    /* Bitcast void* to proper function pointer type */
    auto* fn_ptr_typed = builder_->CreateBitCast(fn_ptr_phi, llvm::PointerType::get(fn_type, 0), "dl.fncast");

    /* Collect the wrapper function's own parameters (passed from caller) */
    std::vector<llvm::Value*> args;
    for (auto& arg : wrapper->args()) {
        args.push_back(&arg);
    }

    /* Create call through function pointer */
    std::string call_name = ret_type->isVoidTy() ? "" : "dl.call";
    llvm::CallInst* call_result = builder_->CreateCall(fn_type, fn_ptr_typed, args, call_name);
    builder_->CreateRet(ret_type->isVoidTy() ? nullptr : call_result);

    /* Restore builder state */
    if (saved_block)
        builder_->SetInsertPoint(saved_block);

    std::cerr << "[ffi] extern function '" << fname
              << "' from library '" << libname
              << "' declared (dynamic loading)\n";
}

/* ════════════════════════════════════════════════════════════
   FFI — External Struct Declaration
   ════════════════════════════════════════════════════════════ */
void Codegen::gen_extern_struct(const ASTNode* node) {
    if (!node) return;
    const std::string& sname = node->value;
    /* Creating the LLVM struct type registers it in the module */
    codegen_get_struct_type(ctx_, sname);
    std::cerr << "[ffi] extern struct '" << sname << "' declared\n";
}

/* ════════════════════════════════════════════════════════════
   FFI — External Union Declaration
   ════════════════════════════════════════════════════════════ */
void Codegen::gen_extern_union(const ASTNode* node) {
    if (!node) return;
    const std::string& uname = node->value;
    /* Creating the LLVM union type registers it (only largest field is stored) */
    codegen_get_struct_type(ctx_, uname);
    std::cerr << "[ffi] extern union '" << uname << "' declared\n";
}
