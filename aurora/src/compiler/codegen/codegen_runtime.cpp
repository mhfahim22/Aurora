#include "compiler/codegen.hpp"
#include "compiler/class_oop.hpp"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/Support/raw_ostream.h>
#include <stdexcept>
#include <sstream>

/* ════════════════════════════════════════════════════════════
   Runtime helper declarations
   ════════════════════════════════════════════════════════════

   These functions are implemented in aurora_runtime.c and linked
   at the final link step.  We just declare their signatures here
   so the IR can call them.
   ════════════════════════════════════════════════════════════ */
void Codegen::declare_runtime_helpers() {
    /* void drop_glue(i8* ptr) */
    fn_drop_glue_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i8ptr_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_drop_glue", module_.get());

    /* void refcount_inc(i8* ptr) */
    fn_refcount_inc_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i8ptr_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_refcount_inc", module_.get());

    /* void refcount_dec(i8* ptr) */
    fn_refcount_dec_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i8ptr_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_refcount_dec", module_.get());

    /* i8* weak_lock(i8* ptr) — returns null if object is dead */
    fn_weak_lock_ = llvm::Function::Create(
        llvm::FunctionType::get(i8ptr_ty(), { i8ptr_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_weak_lock", module_.get());

    /* void weak_release(i8* ptr) */
    fn_weak_release_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i8ptr_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_weak_release", module_.get());

    /* void aurora_gc_register_root(i8* ptr) */
    fn_gc_register_root_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i8ptr_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_gc_register_root", module_.get());

    /* void aurora_gc_unregister_root(i8* ptr) */
    fn_gc_unregister_root_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i8ptr_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_gc_unregister_root", module_.get());

    /* i8* aurora_arena_alloc(i64 size) */
    fn_arena_alloc_ = llvm::Function::Create(
        llvm::FunctionType::get(i8ptr_ty(), { i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_arena_alloc", module_.get());

    /* i8* aurora_gc_alloc(i64 size) */
    fn_gc_alloc_ = llvm::Function::Create(
        llvm::FunctionType::get(i8ptr_ty(), { i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_gc_alloc", module_.get());

    /* void aurora_gc_free(i8* ptr) */
    fn_gc_free_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i8ptr_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_gc_free", module_.get());

    /* void aurora_print_int(i64 val) */
    fn_printf_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_print_int", module_.get());

    /* void aurora_print_str(i8* ptr) — prints AuroraStr* */
    fn_print_str_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i8ptr_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_print_str", module_.get());

    /* i8* aurora_str_from_cstr(i8*) — wraps C string in AuroraStr */
    fn_str_from_cstr_ = llvm::Function::Create(
        llvm::FunctionType::get(i8ptr_ty(), { i8ptr_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_str_from_cstr", module_.get());

    /* i8* aurora_str_new(i64) — allocate new AuroraStr with given capacity */
    fn_str_new_ = llvm::Function::Create(
        llvm::FunctionType::get(i8ptr_ty(), { i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_str_new", module_.get());

    /* void aurora_str_free(i8*) — free an AuroraStr */
    fn_str_free_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i8ptr_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_str_free", module_.get());

    /* void aurora_print_float(double val) */
    fn_print_float_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { llvm::Type::getDoubleTy(ctx_) }, false),
        llvm::Function::ExternalLinkage, "aurora_print_float", module_.get());

    /* i8* aurora_str_append(i8*, i8*) — append b to a, reuse a's buffer */
    fn_str_append_ = llvm::Function::Create(
        llvm::FunctionType::get(i8ptr_ty(), { i8ptr_ty(), i8ptr_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_str_append", module_.get());

    /* void aurora_str_reserve(AuroraStr*, i64) — ensure capacity */
    fn_str_reserve_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i8ptr_ty(), i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_str_reserve", module_.get());

    /* AuroraStr* aurora_str_concat(AuroraStr* a, AuroraStr* b) */
    fn_str_concat_ = llvm::Function::Create(
        llvm::FunctionType::get(i8ptr_ty(), { i8ptr_ty(), i8ptr_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_str_concat", module_.get());

    /* AuroraStr* aurora_substr(AuroraStr*, i64 start, i64 len) */
    llvm::Function::Create(
        llvm::FunctionType::get(i8ptr_ty(), { i8ptr_ty(), i64_ty(), i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_substr", module_.get());

    /* i64 aurora_str_index(AuroraStr*, AuroraStr*) */
    llvm::Function::Create(
        llvm::FunctionType::get(i64_ty(), { i8ptr_ty(), i8ptr_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_str_index", module_.get());

    /* AuroraStr* aurora_int_to_str(i64) — convert integer to AuroraStr */
    fn_int_to_str_ = llvm::Function::Create(
        llvm::FunctionType::get(i8ptr_ty(), { i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_int_to_str", module_.get());

    /* AuroraStr* aurora_float_to_str(double) — convert float to AuroraStr */
    llvm::Function::Create(
        llvm::FunctionType::get(i8ptr_ty(), { llvm::Type::getDoubleTy(ctx_) }, false),
        llvm::Function::ExternalLinkage, "aurora_float_to_str", module_.get());

    /* ── Array runtime ── */
    auto* dbl = llvm::Type::getDoubleTy(ctx_);
    auto* ptr = i8ptr_ty();

    /* i64 aurora_array_new(i64 cap) */
    fn_array_new_ = llvm::Function::Create(
        llvm::FunctionType::get(i64_ty(), { i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_array_new", module_.get());

    /* void aurora_array_reserve(i64 arr, i64 cap) */
    fn_array_reserve_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i64_ty(), i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_array_reserve", module_.get());

    /* void aurora_array_push_int(i64 arr, i64 val) */
    fn_array_push_int_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i64_ty(), i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_array_push_int", module_.get());

    /* void aurora_array_push_float(i64 arr, double val) */
    fn_array_push_flt_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i64_ty(), dbl }, false),
        llvm::Function::ExternalLinkage, "aurora_array_push_float", module_.get());

    /* void aurora_array_push_str(i64 arr, i8* str) */
    fn_array_push_str_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i64_ty(), ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_array_push_str", module_.get());

    /* void aurora_array_push_array(i64 arr, i64 nested) */
    fn_array_push_arr_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i64_ty(), i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_array_push_array", module_.get());

    /* i64 aurora_array_get_int(i64 arr, i64 idx) */
    fn_array_get_int_ = llvm::Function::Create(
        llvm::FunctionType::get(i64_ty(), { i64_ty(), i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_array_get_int", module_.get());

    /* double aurora_array_get_float(i64 arr, i64 idx) */
    fn_array_get_flt_ = llvm::Function::Create(
        llvm::FunctionType::get(dbl, { i64_ty(), i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_array_get_float", module_.get());

    /* i8* aurora_array_get_str(i64 arr, i64 idx) */
    fn_array_get_str_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { i64_ty(), i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_array_get_str", module_.get());

    /* i64 aurora_array_get_tag(i64 arr, i64 idx) */
    fn_array_get_tag_ = llvm::Function::Create(
        llvm::FunctionType::get(i64_ty(), { i64_ty(), i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_array_get_tag", module_.get());

    /* void aurora_array_set_int(i64 arr, i64 idx, i64 val) */
    fn_array_set_int_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i64_ty(), i64_ty(), i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_array_set_int", module_.get());

    /* void aurora_array_set_float(i64 arr, i64 idx, double val) */
    fn_array_set_flt_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i64_ty(), i64_ty(), dbl }, false),
        llvm::Function::ExternalLinkage, "aurora_array_set_float", module_.get());

    /* void aurora_array_set_str(i64 arr, i64 idx, i8* val) */
    fn_array_set_str_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i64_ty(), i64_ty(), ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_array_set_str", module_.get());

    /* i64 aurora_array_len(i64 arr) */
    fn_array_len_ = llvm::Function::Create(
        llvm::FunctionType::get(i64_ty(), { i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_array_len", module_.get());

    /* void aurora_array_print(i64 arr) */
    fn_array_print_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_array_print", module_.get());

    /* void aurora_array_free(i64 arr) */
    fn_array_free_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_array_free", module_.get());

    /* i64 aurora_array_contains_int(i64 arr, i64 val) */
    fn_array_contains_int_ = llvm::Function::Create(
        llvm::FunctionType::get(i64_ty(), { i64_ty(), i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_array_contains_int", module_.get());

    /* ── Async runtime ── */
    auto* fnptr_ty = llvm::PointerType::getUnqual(ctx_);

    /* AuroraTask* aurora_task_create(i8* (*func)(i8*), i8* arg) */
    fn_task_create_ = llvm::Function::Create(
        llvm::FunctionType::get(fnptr_ty, { fnptr_ty, fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_task_create", module_.get());

    /* void aurora_task_destroy(AuroraTask*) */
    fn_task_destroy_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_task_destroy", module_.get());

    /* i32 aurora_task_is_done(AuroraTask*) */
    fn_task_is_done_ = llvm::Function::Create(
        llvm::FunctionType::get(i64_ty(), { fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_task_is_done", module_.get());

    /* i8* aurora_task_get_result(AuroraTask*) */
    fn_task_get_result_ = llvm::Function::Create(
        llvm::FunctionType::get(fnptr_ty, { fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_task_get_result", module_.get());

    /* void aurora_task_set_result(AuroraTask*, i8* result) */
    fn_task_set_result_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { fnptr_ty, fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_task_set_result", module_.get());

    /* void aurora_spawn(AuroraTask*) */
    fn_spawn_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_spawn", module_.get());

    /* void aurora_wait(AuroraTask*) */
    fn_wait_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_wait", module_.get());

    /* ── Channel runtime ── */
    auto* chan_i32 = llvm::Type::getInt32Ty(ctx_);
    /* AuroraChannel* aurora_chan_create(i32 capacity) */
    llvm::Function::Create(
        llvm::FunctionType::get(fnptr_ty, { chan_i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_chan_create", module_.get());

    /* void aurora_chan_destroy(AuroraChannel*) */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_chan_destroy", module_.get());

    /* void aurora_chan_send(AuroraChannel*, i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { fnptr_ty, fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_chan_send", module_.get());

    /* i8* aurora_chan_recv(AuroraChannel*) */
    llvm::Function::Create(
        llvm::FunctionType::get(fnptr_ty, { fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_chan_recv", module_.get());

    /* ── Event bus runtime ── */
    /* void aurora_event_bus_init() */
    fn_event_bus_init_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), {}, false),
        llvm::Function::ExternalLinkage, "aurora_event_bus_init", module_.get());
    /* void aurora_event_on(i8*, i8* (i8*)*, i8*) */
    fn_event_on_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { fnptr_ty, fnptr_ty, fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_event_on", module_.get());
    /* void aurora_event_off(i8*, i8* (i8*)*) */
    fn_event_off_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { fnptr_ty, fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_event_off", module_.get());
    /* void aurora_event_emit(i8*, i8*) */
    fn_event_emit_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { fnptr_ty, fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_event_emit", module_.get());
    /* void aurora_event_bus_shutdown() */
    fn_event_bus_shutdown_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), {}, false),
        llvm::Function::ExternalLinkage, "aurora_event_bus_shutdown", module_.get());

    /* ── Fiber runtime ── */
    /* AuroraFiber* aurora_fiber_create(i8* (i8*)*, i8*) */
    fn_fiber_create_ = llvm::Function::Create(
        llvm::FunctionType::get(fnptr_ty, { fnptr_ty, fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_fiber_create", module_.get());
    /* void aurora_fiber_destroy(AuroraFiber*) */
    fn_fiber_destroy_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_fiber_destroy", module_.get());
    /* void aurora_fiber_yield() */
    fn_fiber_yield_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), {}, false),
        llvm::Function::ExternalLinkage, "aurora_fiber_yield", module_.get());
    /* void aurora_fiber_resume(AuroraFiber*) */
    fn_fiber_resume_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_fiber_resume", module_.get());
    /* i32 aurora_fiber_is_done(AuroraFiber*) */
    fn_fiber_is_done_ = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getInt32Ty(ctx_), { fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_fiber_is_done", module_.get());
    /* i8* aurora_fiber_get_result(AuroraFiber*) */
    fn_fiber_get_result_ = llvm::Function::Create(
        llvm::FunctionType::get(fnptr_ty, { fnptr_ty }, false),
        llvm::Function::ExternalLinkage, "aurora_fiber_get_result", module_.get());

    /* ── Phase 2: Collection type constructors ── */
    auto* i32_ty = llvm::Type::getInt32Ty(ctx_);
    /* i8* list_new() */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "list_new", module_.get());
    /* void list_push(i8*, i64) */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { ptr, i64_ty() }, false),
        llvm::Function::ExternalLinkage, "list_push", module_.get());
    /* i64 list_get(i8*, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(i64_ty(), { ptr, i32_ty }, false),
        llvm::Function::ExternalLinkage, "list_get", module_.get());
    /* i32 list_len(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32_ty, { ptr }, false),
        llvm::Function::ExternalLinkage, "list_len", module_.get());
    /* void list_free(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { ptr }, false),
        llvm::Function::ExternalLinkage, "list_free", module_.get());
    /* i8* map_new() */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "map_new", module_.get());
    /* void map_set(i8*, i8*, i64) */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { ptr, ptr, i64_ty() }, false),
        llvm::Function::ExternalLinkage, "map_set", module_.get());
    /* i64 map_get(i8*, i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(i64_ty(), { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "map_get", module_.get());
    /* i32 map_has(i8*, i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32_ty, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "map_has", module_.get());
    /* void map_free(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { ptr }, false),
        llvm::Function::ExternalLinkage, "map_free", module_.get());
    /* i8* set_new() */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "set_new", module_.get());
    /* void set_add(i8*, i64) */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { ptr, i64_ty() }, false),
        llvm::Function::ExternalLinkage, "set_add", module_.get());
    /* i32 set_has(i8*, i64) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32_ty, { ptr, i64_ty() }, false),
        llvm::Function::ExternalLinkage, "set_has", module_.get());
    /* void set_free(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { ptr }, false),
        llvm::Function::ExternalLinkage, "set_free", module_.get());
    /* i8* vector_new(double, double, double) */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { dbl, dbl, dbl }, false),
        llvm::Function::ExternalLinkage, "vector_new", module_.get());
    /* double vector_x(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(dbl, { ptr }, false),
        llvm::Function::ExternalLinkage, "vector_x", module_.get());
    /* double vector_y(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(dbl, { ptr }, false),
        llvm::Function::ExternalLinkage, "vector_y", module_.get());
    /* double vector_z(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(dbl, { ptr }, false),
        llvm::Function::ExternalLinkage, "vector_z", module_.get());
    /* i8* stack_new() */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "stack_new", module_.get());
    /* void stack_push(i8*, i64) */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { ptr, i64_ty() }, false),
        llvm::Function::ExternalLinkage, "stack_push", module_.get());
    /* i64 stack_pop(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(i64_ty(), { ptr }, false),
        llvm::Function::ExternalLinkage, "stack_pop", module_.get());
    /* i32 stack_empty(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32_ty, { ptr }, false),
        llvm::Function::ExternalLinkage, "stack_empty", module_.get());
    /* void stack_free(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { ptr }, false),
        llvm::Function::ExternalLinkage, "stack_free", module_.get());
    /* i8* queue_new() */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "queue_new", module_.get());
    /* void queue_enqueue(i8*, i64) */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { ptr, i64_ty() }, false),
        llvm::Function::ExternalLinkage, "queue_enqueue", module_.get());
    /* i64 queue_dequeue(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(i64_ty(), { ptr }, false),
        llvm::Function::ExternalLinkage, "queue_dequeue", module_.get());
    /* i32 queue_empty(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32_ty, { ptr }, false),
        llvm::Function::ExternalLinkage, "queue_empty", module_.get());
    /* void queue_free(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { ptr }, false),
        llvm::Function::ExternalLinkage, "queue_free", module_.get());
    /* i8* json_new() */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "json_new", module_.get());
    /* void json_set(i8*, i8*, i64) */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { ptr, ptr, i64_ty() }, false),
        llvm::Function::ExternalLinkage, "json_set", module_.get());
    /* i8* json_get(i8*, i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "json_get", module_.get());
    /* void json_free(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { ptr }, false),
        llvm::Function::ExternalLinkage, "json_free", module_.get());
    /* void aurora_panic(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_panic", module_.get());

    /* ── AI/Tensor runtime helpers ── */
    declare_ai_runtime_helpers();

    /* ── Domain runtime helper declarations ── */
    declare_domain_runtime_helpers();

    /* ── Built-in functions (registered via codegen_builtins.cpp) ── */
    register_builtins(module_.get(), ctx_, builtins_);
}

/* ════════════════════════════════════════════════════════════
   Domain-specific runtime helper declarations
   ════════════════════════════════════════════════════════════ */
void Codegen::declare_domain_runtime_helpers() {
    auto* ctx = &ctx_;
    auto* mod = module_.get();
    auto* v   = llvm::Type::getVoidTy(*ctx);
    auto* i64 = llvm::Type::getInt64Ty(*ctx);
    auto* ptr = llvm::PointerType::getUnqual(*ctx);

    /* ── Game Engine ── */
    /* scene_init() / scene_shutdown() */
    scene_init_ = llvm::Function::Create(
        llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_scene_init", mod);
    scene_shutdown_ = llvm::Function::Create(
        llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_scene_shutdown", mod);

    /* entity_create(i64 type) → i8* */
    entity_create_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { i64 }, false),
        llvm::Function::ExternalLinkage, "aurora_entity_create", mod);
    /* entity_destroy(i8*) */
    entity_destroy_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_entity_destroy", mod);
    /* entity_set_pos(i8*, double, double, double) */
    entity_set_pos_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, llvm::Type::getDoubleTy(*ctx), llvm::Type::getDoubleTy(*ctx), llvm::Type::getDoubleTy(*ctx) }, false),
        llvm::Function::ExternalLinkage, "aurora_entity_set_pos", mod);
    /* entity_get_pos(i8*, double*, double*, double*) */
    entity_get_pos_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr, ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_entity_get_pos", mod);
    /* entity_set_velocity(i8*, double, double, double) */
    entity_set_velocity_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, llvm::Type::getDoubleTy(*ctx), llvm::Type::getDoubleTy(*ctx), llvm::Type::getDoubleTy(*ctx) }, false),
        llvm::Function::ExternalLinkage, "aurora_entity_set_velocity", mod);
    /* entity_get_velocity(i8*, double*, double*, double*) */
    entity_get_velocity_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr, ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_entity_get_velocity", mod);

    /* sprite_create(i8*, double, double) → i8* */
    sprite_create_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, llvm::Type::getDoubleTy(*ctx), llvm::Type::getDoubleTy(*ctx) }, false),
        llvm::Function::ExternalLinkage, "aurora_sprite_create", mod);

    /* camera_create(double, double, double) → i8* */
    camera_create_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { llvm::Type::getDoubleTy(*ctx), llvm::Type::getDoubleTy(*ctx), llvm::Type::getDoubleTy(*ctx) }, false),
        llvm::Function::ExternalLinkage, "aurora_camera_create", mod);

    /* physics_init() / physics_step(double) */
    physics_init_ = llvm::Function::Create(
        llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_physics_init", mod);
    physics_step_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { llvm::Type::getDoubleTy(*ctx) }, false),
        llvm::Function::ExternalLinkage, "aurora_physics_step", mod);
    /* physics_set_gravity(double, double, double) */
    physics_set_gravity_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { llvm::Type::getDoubleTy(*ctx), llvm::Type::getDoubleTy(*ctx), llvm::Type::getDoubleTy(*ctx) }, false),
        llvm::Function::ExternalLinkage, "aurora_physics_set_gravity", mod);

    /* collision_check(i8*, i8*) → i32 */
    collision_check_ = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_collision_check", mod);

    /* audio_play(i8*) */
    audio_play_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_play", mod);
    /* audio_play_tone(i64, i64) */
    audio_play_tone_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { i64, i64 }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_play_tone", mod);

    /* animation_play(i8*, i8*, i64 duration) */
    animation_play_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr, i64 }, false),
        llvm::Function::ExternalLinkage, "aurora_animation_play", mod);

    /* engine_frame_start() / engine_frame_end() */
    engine_frame_start_ = llvm::Function::Create(
        llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_engine_frame_start", mod);
    engine_frame_end_ = llvm::Function::Create(
        llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_engine_frame_end", mod);
    /* engine_render() */
    engine_render_ = llvm::Function::Create(
        llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_engine_render", mod);
    /* engine_is_key_down(i32) → i32 */
    engine_is_key_down_ = llvm::Function::Create(
        llvm::FunctionType::get(i64, { i64 }, false),
        llvm::Function::ExternalLinkage, "aurora_engine_is_key_down", mod);
    /* engine_delta_time() → double */
    engine_delta_time_ = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getDoubleTy(*ctx), {}, false),
        llvm::Function::ExternalLinkage, "aurora_engine_delta_time", mod);
    /* engine_poll_input() */
    engine_poll_input_ = llvm::Function::Create(
        llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_engine_poll_input", mod);

    /* ── Backend Framework ── */
    /* server_init(i64 port) → i8* */
    server_init_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { i64 }, false),
        llvm::Function::ExternalLinkage, "aurora_server_init", mod);
    server_start_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_server_start", mod);
    server_stop_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_server_stop", mod);
    /* server_accept_and_handle(i8*, i8*) */
    server_accept_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_server_accept_and_handle", mod);

    /* database_connect(i8*) → i8* */
    db_connect_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_connect", mod);
    db_query_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_query", mod);
    db_close_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_close", mod);

    /* cache_init() → i8* */
    cache_init_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_cache_init", mod);
    cache_set_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_cache_set", mod);
    cache_get_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_cache_get", mod);

    /* session_create() → i8* */
    session_create_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_session_create", mod);
    session_destroy_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_session_destroy", mod);

    /* auth_login(i8*, i8*) → i32 */
    auth_login_ = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_auth_login", mod);

    /* http_parse_request(i8*) → i8* */
    http_parse_req_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_http_parse_request", mod);
    /* http_response_new() → i8* */
    http_resp_new_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_http_response_new", mod);
    /* http_response_set_status(i8*, i32, i8*) */
    http_resp_status_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, i64, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_http_response_set_status", mod);
    /* http_response_set_body(i8*, i8*) */
    http_resp_body_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_http_response_set_body", mod);
    /* http_response_send(i8*, i64) → i32 */
    http_resp_send_ = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, i64 }, false),
        llvm::Function::ExternalLinkage, "aurora_http_response_send", mod);
    /* router_new() → i8* */
    router_new_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_router_new", mod);
    /* route_add(i8*, i8*, i8*, i8*) */
    route_add_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr, ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_route_add", mod);
    /* route_dispatch(i8*, i8*, i8*) → i32 */
    route_dispatch_ = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_route_dispatch", mod);

    /* ── UI Framework ── */
    /* component_init() / component_render() */
    ui_init_ = llvm::Function::Create(
        llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_ui_init", mod);
    ui_shutdown_ = llvm::Function::Create(
        llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_ui_shutdown", mod);
    ui_render_ = llvm::Function::Create(
        llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_ui_render", mod);

    /* route_register(i8*, i8*) */
    route_register_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_route_register", mod);

    /* style_apply(i8*, i8*) */
    style_apply_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_style_apply", mod);

    /* ── Component runtime ── */
    /* AuroraComponent* aurora_component_create(i8* name, i32 x, i32 y, i32 w, i32 h) */
    auto* i32 = llvm::Type::getInt32Ty(ctx_);
    comp_create_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, i32, i32, i32, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_component_create", mod);
    /* void aurora_component_destroy(AuroraComponent*) */
    comp_destroy_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_component_destroy", mod);
    /* void aurora_component_add_child(AuroraComponent*, AuroraComponent*) */
    comp_add_child_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_component_add_child", mod);
    /* void aurora_component_set_render_fn(AuroraComponent*, void(*)(void*,i32,i32,i32,i32)) */
    comp_set_render_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_component_set_render_fn", mod);
    /* void aurora_component_set_state(AuroraComponent*, void*) */
    comp_set_state_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_component_set_state", mod);
    /* void aurora_component_mount(AuroraComponent*) */
    comp_mount_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_component_mount", mod);
    /* void aurora_component_set_pos(AuroraComponent*, i32, i32) */
    comp_set_pos_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, i32, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_component_set_pos", mod);
    /* void aurora_component_set_size(AuroraComponent*, i32, i32) */
    comp_set_size_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, i32, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_component_set_size", mod);
    /* void aurora_component_show(AuroraComponent*) */
    comp_show_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_component_show", mod);
    /* void aurora_component_hide(AuroraComponent*) */
    comp_hide_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_component_hide", mod);
    /* void aurora_component_render_tree(AuroraComponent*) */
    comp_render_tree_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_component_render_tree", mod);
    /* void aurora_component_update_tree(AuroraComponent*, f64) */
    comp_update_tree_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, llvm::Type::getDoubleTy(ctx_) }, false),
        llvm::Function::ExternalLinkage, "aurora_component_update_tree", mod);

    /* ── Utility ── */
    /* aurora_sleep(i64 ms) */
    fn_sleep_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { i64 }, false),
        llvm::Function::ExternalLinkage, "aurora_sleep", mod);

    /* aurora_time() → i64 */
    fn_time_ = llvm::Function::Create(
        llvm::FunctionType::get(i64, {}, false),
        llvm::Function::ExternalLinkage, "aurora_time", mod);

    /* aurora_random() → i64 */
    fn_random_ = llvm::Function::Create(
        llvm::FunctionType::get(i64, {}, false),
        llvm::Function::ExternalLinkage, "aurora_random", mod);
}
