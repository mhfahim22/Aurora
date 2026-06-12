#pragma once
#include "compiler/ast.hpp"
#include "compiler/ownership.hpp"
#include "compiler/typechecker.hpp"
#include "compiler/class_oop.hpp"
#include "compiler/codegen_builtins.hpp"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Verifier.h>

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>

/* ════════════════════════════════════════════════════════════
    Struct helper types
    ════════════════════════════════════════════════════════════ */
struct StructLayout {
    std::vector<std::string> field_names;
    std::vector<llvm::Type*> field_types;
    llvm::StructType*        llvm_struct{ nullptr };
};

/* ── Struct utility free functions (codegen_types.cpp) ── */
bool              codegen_struct_is_registered(const std::string& name);
llvm::StructType* codegen_get_struct_type(llvm::LLVMContext& ctx, const std::string& name);
int               codegen_struct_field_index(const std::string& struct_name,
                                             const std::string& field_name);
llvm::Value*      codegen_struct_alloca(llvm::LLVMContext& ctx, llvm::IRBuilder<>& builder,
                                        llvm::Function* cur_fn,
                                        const std::string& struct_name,
                                        const std::string& var_name);
llvm::Value*      codegen_struct_gep(llvm::IRBuilder<>& builder, llvm::Value* struct_ptr,
                                     const std::string& struct_name,
                                     const std::string& field_name);

/* ════════════════════════════════════════════════════════════
    OOP free functions (codegen_oop.cpp) — LLVM-dependent
    ════════════════════════════════════════════════════════════ */
llvm::Value* oop_gen_new_object(
        llvm::LLVMContext& ctx, llvm::IRBuilder<>& builder, llvm::Module& module,
        const std::string& class_name, const ASTNode* args_node,
        const std::string& var_name,
        std::function<llvm::Value*(const ASTNode*)> gen_expr);

llvm::Value* oop_gen_field_get(
        llvm::LLVMContext& ctx, llvm::IRBuilder<>& builder,
        const std::string& obj_name, const std::string& field_name, int src_line);

void oop_gen_field_set(
        llvm::LLVMContext& ctx, llvm::IRBuilder<>& builder,
        const std::string& obj_name, const std::string& field_name,
        llvm::Value* value, int src_line);

llvm::Value* oop_gen_method_call(
        llvm::LLVMContext& ctx, llvm::IRBuilder<>& builder, llvm::Module& module,
        const std::string& obj_name, const std::string& method_name,
        const ASTNode* args_node, int src_line,
        std::function<llvm::Value*(const ASTNode*)> gen_expr);

llvm::Value* oop_gen_method_call_ptr(
        llvm::LLVMContext& ctx, llvm::IRBuilder<>& builder, llvm::Module& module,
        const std::string& class_name, llvm::Value* obj_ptr,
        const std::string& method_name,
        const ASTNode* args_node, int src_line,
        std::function<llvm::Value*(const ASTNode*)> gen_expr);

llvm::Value* oop_gen_self_field_get(
        llvm::LLVMContext& ctx, llvm::IRBuilder<>& builder,
        llvm::Value* self_ptr, const std::string& class_name,
        const std::string& field_name, int src_line);

void oop_gen_self_field_set(
        llvm::LLVMContext& ctx, llvm::IRBuilder<>& builder,
        llvm::Value* self_ptr, const std::string& class_name,
        const std::string& field_name, llvm::Value* value, int src_line);

llvm::Value* oop_get_ptr           (const std::string& var_name);
void         oop_register_object_ptr(const std::string& var_name,
                                     const std::string& class_name,
                                     llvm::Value* ptr);

/* ════════════════════════════════════════════════════════════
   Aurora Phase 2 — LLVM IR Code Generator
   ════════════════════════════════════════════════════════════

   Architecture
   ─────────────
   One LLVMContext + one Module per compilation unit.
   IRBuilder<> tracks the current insertion point.

   Memory model
   ─────────────
   Every Aurora variable is an alloca in the entry block of its
   function (standard LLVM mem2reg-friendly pattern).

   Ownership is layered on top:
     • Owned   → plain alloca; drop_glue() called at scope exit
     • Moved   → source pointer poisoned (store undef); codegen
                 refuses to emit loads from it (ownership.hpp
                 already caught this at analyse() time)
     • Shared  → alloca holds a SharedBox* (see runtime layout
                 below); refcount_inc() on copy, refcount_dec()
                 at scope exit
     • Weak    → alloca holds a WeakBox*; weak_lock() before use
     • Borrowed→ alloca holds a raw i8* into the owner's storage;
                 no inc/dec

   Runtime helper layout (emitted once per module)
   ─────────────────────────────────────────────────
   SharedBox  = { i64 strong_count, i64 weak_count, i8* data }
   WeakBox    = SharedBox*   (same struct, just non-owning ref)

   Helper functions declared (not defined — linked from runtime):
     void  drop_glue      (i8* ptr)
     void  refcount_inc   (i8* ptr)          ; ++strong_count
     void  refcount_dec   (i8* ptr)          ; --strong; drop if 0
     i8*   weak_lock      (i8* weak_ptr)     ; returns null if dead
     void  weak_release   (i8* weak_ptr)

   ════════════════════════════════════════════════════════════ */


/* ── Variable record kept per scope ── */
struct VarRecord {
    llvm::Value*   alloca_ptr  { nullptr };   /* alloca instruction        */
    OwnershipState state       { OwnershipState::Owned };
    bool           is_array    { false };
    bool           is_string   { false };
    bool           is_gc_root  { false };     /* registered with GC        */
    bool           is_closure  { false };     /* lambda closure variable   */
    std::string    struct_type {};            /* extern struct name, if any */
};

/* ── One lexical scope on the codegen stack ── */
struct CodegenScope {
    std::unordered_map<std::string, VarRecord> vars;

    VarRecord* find(const std::string& name) {
        auto it = vars.find(name);
        return (it != vars.end()) ? &it->second : nullptr;
    }
};

/* ════════════════════════════════════════════════════════════
   Codegen
   ════════════════════════════════════════════════════════════ */
class Codegen {
public:
    Codegen(llvm::LLVMContext& ctx,
            std::unique_ptr<llvm::Module>& module,
            std::unique_ptr<llvm::IRBuilder<>>& builder);

    /* ── Main entry point ──
       Runs the ownership analyser, then walks the AST and
       emits LLVM IR into the module passed to the constructor. */
    void generate(const ASTNode* root);

    /* ── Dump IR to stdout (for debugging) ── */
    void dump() const;

    /* ── Get the raw module (after generate()) ── */
    llvm::Module* module() { return module_.get(); }

private:
    /* ── LLVM infrastructure (references, not owned) ── */
    llvm::LLVMContext&                  ctx_;
    std::unique_ptr<llvm::Module>&      module_;
    std::unique_ptr<llvm::IRBuilder<>>& builder_;

    /* ── Ownership tracker ── */
    OwnershipTracker ownership_;

    /* ── Loop context for break/continue ── */
    struct LoopContext {
        llvm::BasicBlock* cond_bb { nullptr };
        llvm::BasicBlock* exit_bb { nullptr };
    };
    std::vector<LoopContext> loop_stack_;

    /* ── Scope stack ── */
    std::vector<CodegenScope> scopes_;

    /* ── Runtime helper function declarations ── */
    llvm::Function* fn_drop_glue_    { nullptr };
    llvm::Function* fn_refcount_inc_ { nullptr };
    llvm::Function* fn_refcount_dec_ { nullptr };
    llvm::Function* fn_weak_lock_    { nullptr };
    llvm::Function* fn_weak_release_ { nullptr };
    llvm::Function* fn_gc_register_root_   { nullptr };
    llvm::Function* fn_gc_unregister_root_ { nullptr };
    llvm::Function* fn_arena_alloc_    { nullptr };
    llvm::Function* fn_gc_alloc_       { nullptr };
    llvm::Function* fn_gc_free_        { nullptr };
    llvm::Function* fn_printf_       { nullptr };
    llvm::Function* fn_print_str_    { nullptr };
    llvm::Function* fn_print_float_  { nullptr };
    llvm::Function* fn_str_concat_   { nullptr };
    llvm::Function* fn_str_from_cstr_{ nullptr };
    llvm::Function* fn_str_new_      { nullptr };
    llvm::Function* fn_str_free_     { nullptr };
    llvm::Function* fn_str_append_   { nullptr };
    llvm::Function* fn_str_reserve_  { nullptr };
    llvm::Function* fn_int_to_str_   { nullptr };

    /* ── Async runtime functions ── */
    llvm::Function* fn_task_create_ { nullptr };
    llvm::Function* fn_task_destroy_{ nullptr };
    llvm::Function* fn_task_is_done_ { nullptr };
    llvm::Function* fn_task_get_result_{ nullptr };
    llvm::Function* fn_task_set_result_{ nullptr };
    llvm::Function* fn_spawn_       { nullptr };
    llvm::Function* fn_wait_        { nullptr };

    /* ── Event bus runtime ── */
    llvm::Function* fn_event_bus_init_{ nullptr };
    llvm::Function* fn_event_on_      { nullptr };
    llvm::Function* fn_event_off_     { nullptr };
    llvm::Function* fn_event_emit_    { nullptr };
    llvm::Function* fn_event_bus_shutdown_{ nullptr };

    /* ── Fiber runtime ── */
    llvm::Function* fn_fiber_create_      { nullptr };
    llvm::Function* fn_fiber_destroy_     { nullptr };
    llvm::Function* fn_fiber_yield_       { nullptr };
    llvm::Function* fn_fiber_resume_      { nullptr };
    llvm::Function* fn_fiber_is_done_     { nullptr };
    llvm::Function* fn_fiber_get_result_  { nullptr };

    /* ── AI / Tensor runtime functions ── */
    llvm::Function* tensor_new_     { nullptr };
    llvm::Function* tensor_free_    { nullptr };
    llvm::Function* tensor_ndim_    { nullptr };
    llvm::Function* tensor_shape_   { nullptr };
    llvm::Function* tensor_get_     { nullptr };
    llvm::Function* tensor_set_     { nullptr };
    llvm::Function* tensor_add_     { nullptr };
    llvm::Function* tensor_sub_     { nullptr };
    llvm::Function* tensor_mul_     { nullptr };
    llvm::Function* tensor_matmul_  { nullptr };
    llvm::Function* tensor_reshape_ { nullptr };
    llvm::Function* neural_forward_ { nullptr };
    llvm::Function* predict_        { nullptr };

    /* ── Game Engine runtime functions ── */
    llvm::Function* scene_init_            { nullptr };
    llvm::Function* scene_shutdown_        { nullptr };
    llvm::Function* entity_create_         { nullptr };
    llvm::Function* entity_destroy_        { nullptr };
    llvm::Function* entity_set_pos_        { nullptr };
    llvm::Function* entity_get_pos_        { nullptr };
    llvm::Function* entity_set_velocity_   { nullptr };
    llvm::Function* entity_get_velocity_   { nullptr };
    llvm::Function* sprite_create_         { nullptr };
    llvm::Function* camera_create_         { nullptr };
    llvm::Function* physics_init_          { nullptr };
    llvm::Function* physics_step_          { nullptr };
    llvm::Function* physics_set_gravity_   { nullptr };
    llvm::Function* collision_check_       { nullptr };
    llvm::Function* audio_play_            { nullptr };
    llvm::Function* audio_play_tone_       { nullptr };
    llvm::Function* animation_play_        { nullptr };
    llvm::Function* engine_frame_start_    { nullptr };
    llvm::Function* engine_frame_end_      { nullptr };
    llvm::Function* engine_render_         { nullptr };
    llvm::Function* engine_is_key_down_    { nullptr };
    llvm::Function* engine_delta_time_     { nullptr };
    llvm::Function* engine_poll_input_     { nullptr };

    /* ── Backend Framework runtime functions ── */
    llvm::Function* server_init_     { nullptr };
    llvm::Function* server_start_    { nullptr };
    llvm::Function* server_stop_     { nullptr };
    llvm::Function* server_accept_   { nullptr };
    llvm::Function* db_connect_      { nullptr };
    llvm::Function* db_query_        { nullptr };
    llvm::Function* db_close_        { nullptr };
    llvm::Function* cache_init_      { nullptr };
    llvm::Function* cache_set_       { nullptr };
    llvm::Function* cache_get_       { nullptr };
    llvm::Function* session_create_  { nullptr };
    llvm::Function* session_destroy_ { nullptr };
    llvm::Function* auth_login_      { nullptr };
    llvm::Function* http_parse_req_  { nullptr };
    llvm::Function* http_resp_new_   { nullptr };
    llvm::Function* http_resp_status_{ nullptr };
    llvm::Function* http_resp_body_  { nullptr };
    llvm::Function* http_resp_send_  { nullptr };
    llvm::Function* router_new_      { nullptr };
    llvm::Function* route_add_       { nullptr };
    llvm::Function* route_dispatch_  { nullptr };

    /* ── UI Framework runtime functions ── */
    llvm::Function* ui_init_        { nullptr };
    llvm::Function* ui_shutdown_    { nullptr };
    llvm::Function* ui_render_      { nullptr };
    llvm::Function* route_register_ { nullptr };
    llvm::Function* style_apply_    { nullptr };
    llvm::Function* comp_create_    { nullptr };
    llvm::Function* comp_destroy_   { nullptr };
    llvm::Function* comp_add_child_ { nullptr };
    llvm::Function* comp_set_render_{ nullptr };
    llvm::Function* comp_set_state_ { nullptr };
    llvm::Function* comp_mount_     { nullptr };
    llvm::Function* comp_set_pos_   { nullptr };
    llvm::Function* comp_set_size_  { nullptr };
    llvm::Function* comp_show_      { nullptr };
    llvm::Function* comp_hide_      { nullptr };
    llvm::Function* comp_render_tree_{ nullptr };
    llvm::Function* comp_update_tree_{ nullptr };

    /* ── Utility runtime functions ── */
    llvm::Function* fn_sleep_       { nullptr };
    llvm::Function* fn_time_        { nullptr };
    llvm::Function* fn_random_      { nullptr };

    /* ── Array runtime functions ── */
    llvm::Function* fn_array_contains_int_ { nullptr };
    llvm::Function* fn_array_new_       { nullptr };
    llvm::Function* fn_array_reserve_   { nullptr };
    llvm::Function* fn_array_push_int_  { nullptr };
    llvm::Function* fn_array_push_flt_  { nullptr };
    llvm::Function* fn_array_push_str_  { nullptr };
    llvm::Function* fn_array_push_arr_  { nullptr };
    llvm::Function* fn_array_get_int_   { nullptr };
    llvm::Function* fn_array_get_flt_   { nullptr };
    llvm::Function* fn_array_get_str_   { nullptr };
    llvm::Function* fn_array_get_tag_   { nullptr };
    llvm::Function* fn_array_set_int_   { nullptr };
    llvm::Function* fn_array_set_flt_   { nullptr };
    llvm::Function* fn_array_set_str_   { nullptr };
    llvm::Function* fn_array_len_       { nullptr };
    llvm::Function* fn_array_print_     { nullptr };
    llvm::Function* fn_array_free_      { nullptr };

    /* ── Yield support ── */
    llvm::Function* yield_fn_ { nullptr };
    void gen_yield(const ASTNode* node);

    /* ── Domain-specific statement generators ── */
    /* Game Engine */
    void gen_scene    (const ASTNode* node);
    void gen_entity   (const ASTNode* node);
    void gen_sprite   (const ASTNode* node);
    void gen_camera   (const ASTNode* node);
    void gen_physics  (const ASTNode* node);
    void gen_collision(const ASTNode* node);
    void gen_audio    (const ASTNode* node);

    /* Backend Framework */
    void gen_server   (const ASTNode* node);
    void gen_api      (const ASTNode* node);
    void gen_database (const ASTNode* node);
    void gen_cache    (const ASTNode* node);
    void gen_session  (const ASTNode* node);
    void gen_auth     (const ASTNode* node);

    /* UI Framework */
    void gen_component  (const ASTNode* node);
    void gen_style      (const ASTNode* node);
    void gen_theme      (const ASTNode* node);
    void gen_route      (const ASTNode* node);
    void gen_animate    (const ASTNode* node);
    void gen_transition (const ASTNode* node);

    /* AI */
    void gen_ai           (const ASTNode* node);
    void gen_ai_predict_stmt(const ASTNode* node);
    void gen_tensor_stmt  (const ASTNode* node);

    /* Async */
    void gen_parallel (const ASTNode* node);
    void gen_thread   (const ASTNode* node);
    void gen_signal   (const ASTNode* node);
    void gen_emit     (const ASTNode* node);

    /* Utility */
    void gen_sleep  (const ASTNode* node);
    void gen_time   (const ASTNode* node);
    void gen_random (const ASTNode* node);

    /* ── FFI ── */
    void gen_extern_fn(const ASTNode* node);
    void gen_extern_struct(const ASTNode* node);
    void gen_extern_union(const ASTNode* node);
    llvm::Type* extern_type_to_llvm(const std::string& type_name);

    /* Callback param info: for extern function 'name', param at index 'i' has this signature */
    struct CallbackSig {
        int index;
        llvm::FunctionType* fn_type;
    };
    std::unordered_map<std::string, std::vector<CallbackSig>> extern_callback_sigs_;

    /* C-string param info: for extern function 'name', which param indices expect C strings */
    /* When an Aurora string is passed, the data pointer is auto-extracted */
    struct ExternStringInfo {
        std::vector<int> param_indices;  /* param indices that are cstring/char* */
        bool return_is_cstring{false};   /* true if return type is cstring/char* */
        bool has_pointer_param{false};   /* true if any param is pointer/void* */
    };
    std::unordered_map<std::string, ExternStringInfo> extern_string_info_;



    /* ── Built-in functions (separate struct for maintainability) ── */
    BuiltinFunctions builtins_;

    /* ── Current function being built ── */
    llvm::Function* cur_fn_ { nullptr };

    /* ══════════════════════════════════════════
       Setup helpers
       ══════════════════════════════════════════ */

    /* Declare all runtime helpers in the module. */
    void declare_runtime_helpers();

    /* Declare domain-specific runtime helpers (game, backend, UI, utility). */
    void declare_domain_runtime_helpers();

    /* ══════════════════════════════════════════
       Scope helpers
       ══════════════════════════════════════════ */
    void        push_scope();

    /* Emit Drop IR for every Owned / Shared variable going out of scope,
       then pop the scope. */
    void        pop_scope_and_drop();
    void        emit_scope_cleanup(CodegenScope& scope);
    void        emit_all_scope_cleanup();

    void        declare_var(const std::string& name,
                            llvm::Value*       alloca_ptr,
                            OwnershipState     state);

    VarRecord*  lookup_var(const std::string& name);

    /* Create an alloca in the entry block of cur_fn_ (mem2reg friendly). */
    llvm::AllocaInst* create_entry_alloca(const std::string& name,
                                          llvm::Type*        ty);

    /* Strategy-based allocation — emits ownership-specific wrappers */
    llvm::Value* gen_allocation_for_var(const std::string& name,
                                        llvm::Type* ty,
                                        OwnershipState state,
                                        AllocStrategy strategy = AllocStrategy::Stack);

    /* ══════════════════════════════════════════
       Emission helpers
       ══════════════════════════════════════════ */

    /* Emit a call to drop_glue(ptr). */
    void emit_drop(llvm::Value* ptr);

    /* Emit refcount_inc(ptr). */
    void emit_refcount_inc(llvm::Value* ptr);

    /* Emit refcount_dec(ptr) — decrements and drops if zero. */
    void emit_refcount_dec(llvm::Value* ptr);

    /* Emit weak_lock(ptr) → i8* (null if dead). */
    llvm::Value* emit_weak_lock(llvm::Value* ptr);

    /* Emit weak_release(ptr). */
    void emit_weak_release(llvm::Value* ptr);

    /* Emit GC root registration for a pointer. */
    void emit_gc_register_root(llvm::Value* ptr);

    /* Emit GC root unregistration for a pointer. */
    void emit_gc_unregister_root(llvm::Value* ptr);

    /* Store llvm::PoisonValue into ptr (mark moved-from storage). */
    void emit_poison(llvm::Value* ptr);

    bool current_block_terminated() const;
    bool safe_br(llvm::BasicBlock* target);
    bool safe_ret(llvm::Value* value);

    /* ══════════════════════════════════════════
       AST → IR walkers
       ══════════════════════════════════════════ */
    void         gen_block (const ASTNode* node);
    void         gen_stmt  (const ASTNode* node);
    llvm::Value* gen_expr  (const ASTNode* node);

    /* Statement generators */
    void gen_struct_decl (const ASTNode* node);
    void gen_enum_decl   (const ASTNode* node);
    void gen_type_alias  (const ASTNode* node);
    void gen_interface_decl(const ASTNode* node);
    void gen_assign      (const ASTNode* node);
    void gen_move        (const ASTNode* node);
    void gen_drop        (const ASTNode* node);
    void gen_delete      (const ASTNode* node);
    void gen_copy        (const ASTNode* node);
    void gen_free        (const ASTNode* node);
    void gen_shared_ref  (const ASTNode* node);
    void gen_reference   (const ASTNode* node);
    void gen_pointer     (const ASTNode* node);
    void gen_unsafe_block(const ASTNode* node);
    void gen_safe_block  (const ASTNode* node);
    void gen_panic       (const ASTNode* node);
    void gen_debug       (const ASTNode* node);
    void gen_log         (const ASTNode* node);
    void gen_weak_ref    (const ASTNode* node);
    void gen_borrow      (const ASTNode* node);
    void gen_output      (const ASTNode* node);
    void gen_return      (const ASTNode* node);
    void gen_if          (const ASTNode* node);
    void gen_match       (const ASTNode* node);
    llvm::Value* gen_pattern_cond (const ASTNode* pattern, llvm::Value* match_val);
    void          gen_pattern_bind (const ASTNode* pattern, llvm::Value* match_val);
    llvm::Value* ensure_i64       (llvm::Value* v);
    void gen_while       (const ASTNode* node);
    void gen_loop        (const ASTNode* node);
    void gen_repeat      (const ASTNode* node);
    void gen_for         (const ASTNode* node);
    void gen_break       ();
    void gen_continue    ();
    void gen_skip        ();
    void gen_function    (const ASTNode* node);
    void gen_lambda      (const ASTNode* node);
    void gen_try         (const ASTNode* node);
    void gen_throw       (const ASTNode* node);
    void gen_new         (const ASTNode* node);
    void gen_spawn       (const ASTNode* node);
    void gen_wait        (const ASTNode* node);
    void gen_async       (const ASTNode* node);
    void gen_class_oop              (const ASTNode* node);
    void gen_stmt_in_method         (const ASTNode* node, llvm::Value* self_ptr, const std::string& class_name);
    void gen_stmt_with_self         (const ASTNode* node, llvm::Value* self_ptr, const std::string& class_name);
    llvm::Value* gen_expr_in_method (const ASTNode* node, llvm::Value* self_ptr, const std::string& class_name);

    /* Expression generators */
    llvm::Value* gen_var       (const ASTNode* node);
    llvm::Value* gen_binop     (const ASTNode* node);
    llvm::Value* gen_unary     (const ASTNode* node);
    llvm::Value* gen_call      (const ASTNode* node);
    llvm::Value* gen_index     (const ASTNode* node);
    llvm::Value* gen_move_expr (const ASTNode* node);
    llvm::Value* gen_copy_expr (const ASTNode* node);
    llvm::Value* gen_shared_expr(const ASTNode* node);
    llvm::Value* gen_weak_expr  (const ASTNode* node);
    llvm::Value* gen_borrow_expr(const ASTNode* node);
    llvm::Value* gen_closure_call(const ASTNode* node, VarRecord* rec);
    llvm::Value* gen_fnptr_call(const ASTNode* node, VarRecord* rec);

    /* AI / Tensor support */
    void         declare_ai_runtime_helpers();
    llvm::Value* gen_tensor_expr   (const ASTNode* node);
    llvm::Value* gen_ai_predict    (const ASTNode* node);
    llvm::Value* gen_neural_forward(const ASTNode* node);

    /* Array support */
    llvm::Value* gen_array          (const ASTNode* node);
    llvm::Value* gen_struct_literal (const ASTNode* node);
    void         gen_index_assign   (const ASTNode* node);

    /* Utility */
    llvm::Type*  i64_ty()  { return llvm::Type::getInt64Ty(ctx_); }
    llvm::Type*  i8_ty()   { return llvm::Type::getInt8Ty(ctx_);  }
    llvm::PointerType* i8ptr_ty(){ return llvm::PointerType::getUnqual(ctx_); }
    llvm::Type*  void_ty() { return llvm::Type::getVoidTy(ctx_);  }
    llvm::Value* i64(int64_t v) {
        return llvm::ConstantInt::get(i64_ty(), v, true);
    }

    /* Hoist aurora_str_from_cstr(literal) to entry block — avoids re-allocation
       inside loops.  Caches result per unique literal string per function. */
    llvm::Value* get_literal_aurora(const std::string& str);
    std::unordered_map<std::string, llvm::Value*> literal_aurora_cache_;

    /* Check if an AST expression evaluates to a string type */
    bool expr_is_string_type(const ASTNode* node);
};

/* ════════════════════════════════════════════════════════════
   LLVMCodegen — LLVM Module-level utilities
   ════════════════════════════════════════════════════════════
   Standalone wrapper for object-file emission and .aura -> .obj
   convenience functions. Uses Codegen internally for IR gen.
   ════════════════════════════════════════════════════════════ */
class LLVMCodegen {
public:
    /* Create with own context, module, builder (default) */
    LLVMCodegen();

    /* Wrap existing LLVM objects */
    LLVMCodegen(llvm::LLVMContext& ctx,
                std::unique_ptr<llvm::Module>& module,
                std::unique_ptr<llvm::IRBuilder<>>& builder);

    /* Generate LLVM IR from AST using standard Codegen */
    void generate(const ASTNode* root);

    /* Emit .obj file from the current module */
    bool emit_object_file(const std::string& obj_path);

    /* Run LLVM O3 + cleanup passes */
    void run_optimization_passes();

    /* Verify module is well-formed */
    bool verify_module() const;

    /* Write IR to stream or file */
    void dump_ir(std::ostream& os) const;
    bool write_ir(const std::string& path) const;

    /* Access module */
    llvm::Module* module();

private:
    bool                     ctx_external_ { false };
    std::unique_ptr<llvm::LLVMContext>      owned_ctx_;   /* owned when ctx_external_==false */
    llvm::LLVMContext*       ctx_          { nullptr };
    std::unique_ptr<llvm::Module>           module_;
    std::unique_ptr<llvm::IRBuilder<>>      builder_;
};

/* ── Convenience free functions ── */

/* Full pipeline: .aura source -> .obj file (single step) */
bool compile_aura_to_object(const std::string& source_path,
                            const std::string& obj_path,
                            bool optimize = true);

/* JIT compile and execute "main" from a compiled LLVM module.
   Takes ownership of ctx and module. */
int jit_execute_main(std::unique_ptr<llvm::LLVMContext> ctx,
                     std::unique_ptr<llvm::Module> module);