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

    /* void aurora_coverage_trace(i8* file, i64 line) */
    fn_coverage_trace_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i8ptr_ty(), i64_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_coverage_trace", module_.get());

    /* void aurora_print_str(i8* ptr) — prints AuroraStr* */
    fn_print_str_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { i8ptr_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_print_str", module_.get());

    /* i8* aurora_str_from_cstr(i8*) — wraps C string in AuroraStr */
    fn_str_from_cstr_ = llvm::Function::Create(
        llvm::FunctionType::get(i8ptr_ty(), { i8ptr_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_str_from_cstr", module_.get());

    /* i8* aurora_str_as_cstr(i8*) — get C string pointer from AuroraStr */
    llvm::Function::Create(
        llvm::FunctionType::get(i8ptr_ty(), { i8ptr_ty() }, false),
        llvm::Function::ExternalLinkage, "aurora_str_as_cstr", module_.get());

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
    fn_float_to_str_ = llvm::Function::Create(
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
    /* void map_set(i8*, i8*, i64) — second param is cstring */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { ptr, ptr, i64_ty() }, false),
        llvm::Function::ExternalLinkage, "map_set", module_.get());
    extern_string_info_["map_set"] = { {1}, false, true };
    /* i64 map_get(i8*, i8*) — second param is cstring */
    llvm::Function::Create(
        llvm::FunctionType::get(i64_ty(), { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "map_get", module_.get());
    extern_string_info_["map_get"] = { {1}, false, true };
    /* i32 map_has(i8*, i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32_ty, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "map_has", module_.get());
    /* void map_free(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { ptr }, false),
        llvm::Function::ExternalLinkage, "map_free", module_.get());
    /* void map_copy(i8*, i8*) — copy all entries from src to dst */
    llvm::Function::Create(
        llvm::FunctionType::get(void_ty(), { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "map_copy", module_.get());
    /* i8* map_keys(i8*) — return list of string keys */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "map_keys", module_.get());
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
    auto* i32 = llvm::Type::getInt32Ty(*ctx);
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
    /* http_response_set_body_n(i8*, i8*, i64) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr, i64 }, false),
        llvm::Function::ExternalLinkage, "aurora_http_response_set_body_n", mod);
    /* http_response_send(i8*, i64) → i32 */
    http_resp_send_ = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, i64 }, false),
        llvm::Function::ExternalLinkage, "aurora_http_response_send", mod);
    /* http_response_send_chunked(i8*, i64, i32) → i64 */
    llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, i64, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_http_response_send_chunked", mod);
    /* http_response_set_content_type(i8*, i8*) */
    http_resp_ct_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_http_response_set_content_type", mod);
    /* server_run(i8*) — starts server accept loop */
    server_run_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_server_run", mod);
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

    /* http_get_field(i8*, i8*) → i8* - get a field from HTTP request */
    http_get_field_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_http_get_field", mod);
    /* http_get_param(i8*, i8*) → i8* - get a route param from HTTP request */
    http_get_param_ = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_http_get_param", mod);
    /* http_response_set_json(i8*, i8*) - set JSON response */
    http_set_json_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_http_response_set_json", mod);
    /* http_str_as_cstr(i8*) → i8* - AuroraStr* to const char* */
    /* (already declared earlier, but store a member pointer) */

    /* ── Todo Store ── */
    /* aurora_todo_list() → i8* */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_todo_list", mod);
    /* aurora_todo_create(i8*) → i8* */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_todo_create", mod);
    /* aurora_todo_get(i8*) → i8* */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_todo_get", mod);
    /* aurora_todo_update(i8*, i8*, i64) → i8* */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, ptr, i64 }, false),
        llvm::Function::ExternalLinkage, "aurora_todo_update", mod);
    /* aurora_todo_delete(i8*) → i8* */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_todo_delete", mod);
    /* Mark todo functions: first params (id) are cstrings,
       return values are cstrings that need wrapping. */
    extern_string_info_["aurora_todo_list"]   = { {}, true, false };
    extern_string_info_["aurora_todo_create"] = { {0}, true, false };
    extern_string_info_["aurora_todo_get"]    = { {0}, true, false };
    extern_string_info_["aurora_todo_update"] = { {0, 1}, true, false };
    extern_string_info_["aurora_todo_delete"] = { {0}, true, false };

    /* ── Connection Pool ── */
    /* aurora_db_pool_create(i8*, i32, i32) → i8* */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, i32, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_db_pool_create", mod);
    /* aurora_db_pool_acquire(i8*, i32) → i8* */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_db_pool_acquire", mod);
    /* aurora_db_pool_release(i8*, i8*) → void */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_pool_release", mod);
    /* aurora_db_pool_query(i8*, i8*) → i8* */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_pool_query", mod);
    /* aurora_db_pool_query_free(i8*) → void */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_pool_query_free", mod);
    /* aurora_db_pool_destroy(i8*) → void */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_pool_destroy", mod);
    /* aurora_db_pool_active_count(i8*) → i32 */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_pool_active_count", mod);
    /* aurora_db_pool_idle_count(i8*) → i32 */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_pool_idle_count", mod);
    extern_string_info_["aurora_db_pool_create"]  = { {0}, false, false };
    extern_string_info_["aurora_db_pool_acquire"] = { {}, false, false };
    extern_string_info_["aurora_db_pool_release"] = { {}, false, false };
    extern_string_info_["aurora_db_pool_query"]   = { {1}, true, false };
    extern_string_info_["aurora_db_pool_query_free"] = { {}, false, false };
    extern_string_info_["aurora_db_pool_destroy"] = { {}, false, false };
    extern_string_info_["aurora_db_pool_active_count"] = { {}, false, false };
    extern_string_info_["aurora_db_pool_idle_count"] = { {}, false, false };

    /* ── Matrix Math (4x4) ── */
    auto* f32 = llvm::Type::getFloatTy(*ctx);
    /* i8* aurora_mat4_new() */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_mat4_new", mod);
    /* void aurora_mat4_free(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_mat4_free", mod);
    /* void aurora_mat4_identity(i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_mat4_identity", mod);
    /* void aurora_mat4_copy(i8*, i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_mat4_copy", mod);
    /* void aurora_mat4_mul(i8*, i8*, i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_mat4_mul", mod);
    /* void aurora_mat4_translate(i8*, f32, f32, f32) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, f32, f32, f32 }, false),
        llvm::Function::ExternalLinkage, "aurora_mat4_translate", mod);
    /* void aurora_mat4_rotate(i8*, f32, f32, f32, f32) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, f32, f32, f32, f32 }, false),
        llvm::Function::ExternalLinkage, "aurora_mat4_rotate", mod);
    /* void aurora_mat4_scale(i8*, f32, f32, f32) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, f32, f32, f32 }, false),
        llvm::Function::ExternalLinkage, "aurora_mat4_scale", mod);
    /* void aurora_mat4_perspective(i8*, f32, f32, f32, f32) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, f32, f32, f32, f32 }, false),
        llvm::Function::ExternalLinkage, "aurora_mat4_perspective", mod);
    /* void aurora_mat4_lookat(i8*, f32, f32, f32,  f32, f32, f32,  f32, f32, f32) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, f32, f32, f32,  f32, f32, f32,  f32, f32, f32 }, false),
        llvm::Function::ExternalLinkage, "aurora_mat4_lookat", mod);
    extern_string_info_["aurora_mat4_new"]         = { {}, false, false };
    extern_string_info_["aurora_mat4_free"]        = { {}, false, false };
    extern_string_info_["aurora_mat4_identity"]    = { {}, false, false };
    extern_string_info_["aurora_mat4_copy"]        = { {}, false, false };
    extern_string_info_["aurora_mat4_mul"]         = { {}, false, false };
    extern_string_info_["aurora_mat4_translate"]   = { {}, false, false };
    extern_string_info_["aurora_mat4_rotate"]      = { {}, false, false };
    extern_string_info_["aurora_mat4_scale"]       = { {}, false, false };
    extern_string_info_["aurora_mat4_perspective"] = { {}, false, false };
    extern_string_info_["aurora_mat4_lookat"]      = { {}, false, false };

    /* ── Pre-built cube vertex data ── */
    /* i8* aurora_gl_cube_vertices() */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_gl_cube_vertices", mod);
    /* i32 aurora_gl_cube_vertex_count() */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_gl_cube_vertex_count", mod);
    /* i32 aurora_gl_cube_vertex_stride() */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_gl_cube_vertex_stride", mod);

    /* ── Lit cube helpers ── */
    /* i8* aurora_gl_lit_cube_vertices() */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_gl_lit_cube_vertices", mod);
    /* i32 aurora_gl_lit_cube_vertex_count() */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_gl_lit_cube_vertex_count", mod);
    /* i32 aurora_gl_lit_cube_vertex_stride() */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_gl_lit_cube_vertex_stride", mod);

    /* ── UV cube helpers ── */
    /* i8* aurora_gl_uv_cube_vertices() */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_gl_uv_cube_vertices", mod);
    /* i32 aurora_gl_uv_cube_vertex_count() */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_gl_uv_cube_vertex_count", mod);
    /* i32 aurora_gl_uv_cube_vertex_stride() */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_gl_uv_cube_vertex_stride", mod);

    /* ── GLFW cursor helpers ── */
    /* double aurora_glfw_get_cursor_x(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getDoubleTy(ctx_), { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_glfw_get_cursor_x", mod);
    /* double aurora_glfw_get_cursor_y(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getDoubleTy(ctx_), { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_glfw_get_cursor_y", mod);

    /* ── Generic i32-pair helper ── */
    /* i64 aurora_glfw_get_i32_pair(ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_glfw_get_i32_pair", mod);
    /* Convenience window helpers */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_glfw_window_width", mod);
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_glfw_window_height", mod);
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_glfw_framebuffer_width", mod);
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_glfw_framebuffer_height", mod);
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_glfw_window_pos_x", mod);
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_glfw_window_pos_y", mod);

    /* ── GL buffer/VAO helpers ── */
    /* u32 aurora_gl_gen_buffer() */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_gl_gen_buffer", mod);
    /* u32 aurora_gl_gen_vertex_array() */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_gl_gen_vertex_array", mod);

    /* ── Image helpers ── */
    /* void* aurora_image_load(cstring, i32*, i32*, i32*) */
    {
        auto* i32ptr = llvm::PointerType::getUnqual(i32);
        llvm::Function::Create(
            llvm::FunctionType::get(ptr, { ptr, i32ptr, i32ptr, i32ptr }, false),
            llvm::Function::ExternalLinkage, "aurora_image_load", mod);
    }
    /* void aurora_image_free(void*) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_image_free", mod);
    /* u32 aurora_image_create_gl_texture(cstring) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_image_create_gl_texture", mod);

    /* ── OBJ helpers ── */
    /* void* aurora_obj_load(cstring) */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_obj_load", mod);
    /* i32 aurora_obj_vertex_count(void*) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_obj_vertex_count", mod);
    /* float* aurora_obj_vertex_data(void*) */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_obj_vertex_data", mod);
    /* void aurora_obj_free(void*) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_obj_free", mod);

    /* ══════════════════════════════════════════
       Phase 19 — OpenGL & Game Support
       ══════════════════════════════════════════ */

    /* ── Lighting (10) ── */
    /* ptr aurora_light_create(i32, f32, f32, f32, f32) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { i32, f32, f32, f32, f32 }, false),
        llvm::Function::ExternalLinkage, "aurora_light_create", mod);
    /* void aurora_light_destroy(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_light_destroy", mod);
    /* void aurora_light_set_position(ptr, f32, f32, f32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, f32, f32, f32 }, false),
        llvm::Function::ExternalLinkage, "aurora_light_set_position", mod);
    /* void aurora_light_set_direction(ptr, f32, f32, f32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, f32, f32, f32 }, false),
        llvm::Function::ExternalLinkage, "aurora_light_set_direction", mod);
    /* void aurora_light_set_color(ptr, f32, f32, f32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, f32, f32, f32 }, false),
        llvm::Function::ExternalLinkage, "aurora_light_set_color", mod);
    /* void aurora_light_set_intensity(ptr, f32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, f32 }, false),
        llvm::Function::ExternalLinkage, "aurora_light_set_intensity", mod);
    /* void aurora_light_set_range(ptr, f32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, f32 }, false),
        llvm::Function::ExternalLinkage, "aurora_light_set_range", mod);
    /* void aurora_light_set_spot_angle(ptr, f32, f32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, f32, f32 }, false),
        llvm::Function::ExternalLinkage, "aurora_light_set_spot_angle", mod);
    /* i32 aurora_light_get_count() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_light_get_count", mod);
    /* ptr aurora_light_get(i32) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_light_get", mod);

    /* ── Tilemap (10) ── */
    /* ptr aurora_tilemap_create(i32, i32, i32, i32, i32) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { i32, i32, i32, i32, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_tilemap_create", mod);
    /* void aurora_tilemap_destroy(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_tilemap_destroy", mod);
    /* void aurora_tilemap_set_tile(ptr, i32, i32, i32, i32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, i32, i32, i32, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_tilemap_set_tile", mod);
    /* i32 aurora_tilemap_get_tile(ptr, i32, i32, i32) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr, i32, i32, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_tilemap_get_tile", mod);
    /* i32 aurora_tilemap_get_width(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_tilemap_get_width", mod);
    /* i32 aurora_tilemap_get_height(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_tilemap_get_height", mod);
    /* i32 aurora_tilemap_get_cols(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_tilemap_get_cols", mod);
    /* i32 aurora_tilemap_get_rows(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_tilemap_get_rows", mod);
    /* i32 aurora_tilemap_is_solid(ptr, i32, i32, i32) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr, i32, i32, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_tilemap_is_solid", mod);
    /* void aurora_tilemap_set_property(ptr, ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_tilemap_set_property", mod);

    /* ── Mesh primitives (9) ── */
    /* ptr aurora_mesh_create_plane(f32, f32) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { f32, f32 }, false),
        llvm::Function::ExternalLinkage, "aurora_mesh_create_plane", mod);
    /* ptr aurora_mesh_create_sphere(f32, i32) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { f32, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_mesh_create_sphere", mod);
    /* ptr aurora_mesh_create_cylinder(f32, f32, i32) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { f32, f32, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_mesh_create_cylinder", mod);
    /* ptr aurora_mesh_create_capsule(f32, f32, i32) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { f32, f32, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_mesh_create_capsule", mod);
    /* i32 aurora_mesh_get_vertex_count(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_mesh_get_vertex_count", mod);
    /* ptr aurora_mesh_get_vertex_data(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_mesh_get_vertex_data", mod);
    /* i32 aurora_mesh_get_index_count(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_mesh_get_index_count", mod);
    /* ptr aurora_mesh_get_index_data(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_mesh_get_index_data", mod);
    /* void aurora_mesh_destroy(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_mesh_destroy", mod);

    /* ── GL shader helpers (15) ── */
    /* void aurora_gl_shader_source(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_gl_shader_source", mod);
    /* i32 aurora_gl_compile_shader(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_gl_compile_shader", mod);
    /* ptr aurora_gl_create_shader(i32) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_gl_create_shader", mod);
    /* ptr aurora_gl_create_program() */
    llvm::Function::Create(llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_gl_create_program", mod);
    /* void aurora_gl_attach_shader(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_gl_attach_shader", mod);
    /* i32 aurora_gl_link_program(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_gl_link_program", mod);
    /* void aurora_gl_use_program(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_gl_use_program", mod);
    /* void aurora_gl_delete_shader(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_gl_delete_shader", mod);
    /* void aurora_gl_delete_program(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_gl_delete_program", mod);
    /* void aurora_gl_get_shader_iv(ptr, i32, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, i32, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_gl_get_shader_iv", mod);
    /* void aurora_gl_get_program_iv(ptr, i32, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, i32, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_gl_get_program_iv", mod);
    /* ptr aurora_gl_get_shader_info_log(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_gl_get_shader_info_log", mod);
    /* ptr aurora_gl_get_program_info_log(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_gl_get_program_info_log", mod);
    /* ptr aurora_gl_compile_program(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_gl_compile_program", mod);
    /* void aurora_gl_free_string(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_gl_free_string", mod);

    /* ── Sprite2D (9) ── */
    /* void aurora_sprite2d_init() */
    llvm::Function::Create(llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_sprite2d_init", mod);
    /* void aurora_sprite2d_set_viewport(i32, i32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { i32, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_sprite2d_set_viewport", mod);
    /* i32 aurora_sprite2d_load_texture(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_sprite2d_load_texture", mod);
    /* i32 aurora_sprite2d_create_texture(ptr, i32, i32) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr, i32, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_sprite2d_create_texture", mod);
    /* void aurora_sprite2d_draw(i32, f32, f32, f32, f32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { i32, f32, f32, f32, f32 }, false),
        llvm::Function::ExternalLinkage, "aurora_sprite2d_draw", mod);
    /* void aurora_sprite2d_flush() */
    llvm::Function::Create(llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_sprite2d_flush", mod);
    /* void aurora_sprite2d_clear(f32, f32, f32, f32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { f32, f32, f32, f32 }, false),
        llvm::Function::ExternalLinkage, "aurora_sprite2d_clear", mod);
    /* void aurora_sprite2d_delete_texture(i32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_sprite2d_delete_texture", mod);
    /* void aurora_sprite2d_shutdown() */
    llvm::Function::Create(llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_sprite2d_shutdown", mod);

    /* ══════════════════════════════════════════
       Phase 20 — Plugin System
       ══════════════════════════════════════════ */

    /* ── Plugin Management (7) ── */
    /* i32 aurora_plugin_load(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_plugin_load", mod);
    /* i32 aurora_plugin_unload(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_plugin_unload", mod);
    /* i32 aurora_plugin_unload_all() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_plugin_unload_all", mod);
    /* i32 aurora_plugin_get_count() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_plugin_get_count", mod);
    /* ptr aurora_plugin_get_name(i32) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_plugin_get_name", mod);
    /* i32 aurora_plugin_is_loaded(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_plugin_is_loaded", mod);
    /* i32 aurora_plugin_scan(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_plugin_scan", mod);

    /* ── Plugin Metadata (3) ── */
    /* ptr aurora_plugin_get_info(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_plugin_get_info", mod);
    /* i32 aurora_plugin_get_abi(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_plugin_get_abi", mod);
    /* ptr aurora_plugin_get_function(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_plugin_get_function", mod);

    /* ── Reflection Query (8) ── */
    /* i32 aurora_reflection_get_type_count() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_reflection_get_type_count", mod);
    /* ptr aurora_reflection_get_type_name(i32) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_reflection_get_type_name", mod);
    /* i32 aurora_reflection_get_field_count(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_reflection_get_field_count", mod);
    /* void aurora_reflection_get_field_info(ptr, i32, ptr, ptr, ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, i32, ptr, ptr, ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_reflection_get_field_info", mod);
    /* i32 aurora_reflection_get_method_count(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_reflection_get_method_count", mod);
    /* void aurora_reflection_get_method_info(ptr, i32, ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, i32, ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_reflection_get_method_info", mod);
    /* ptr aurora_reflection_get_method_pointer(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_reflection_get_method_pointer", mod);

    /* ── Version (2) ── */
    /* i32 aurora_version_abi() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_version_abi", mod);
    /* ptr aurora_version_string() */
    llvm::Function::Create(llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_version_string", mod);

    /* ══════════════════════════════════════════
       Phase 21 — Package Manager
       ══════════════════════════════════════════ */

    /* ── Package Management (6) ── */
    /* i32 aurora_pkg_install(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_pkg_install", mod);
    /* i32 aurora_pkg_remove(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_pkg_remove", mod);
    /* i32 aurora_pkg_update(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_pkg_update", mod);
    /* i32 aurora_pkg_publish(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_pkg_publish", mod);
    /* i32 aurora_pkg_search(ptr, ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr, ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_pkg_search", mod);
    /* i32 aurora_pkg_list_installed(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_pkg_list_installed", mod);

    /* ── Registry (2) ── */
    /* void aurora_pkg_set_registry(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_pkg_set_registry", mod);
    /* ptr aurora_pkg_get_registry() */
    llvm::Function::Create(llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_pkg_get_registry", mod);

    /* ── Authentication (1) ── */
    /* i32 aurora_pkg_login(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_pkg_login", mod);

    /* ── Lock File (3) ── */
    /* i32 aurora_pkg_lock_init() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_pkg_lock_init", mod);
    /* i32 aurora_pkg_lock_save() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_pkg_lock_save", mod);
    /* i32 aurora_pkg_lock_load() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_pkg_lock_load", mod);

    /* ── Dependency Resolution (3) ── */
    /* i32 aurora_pkg_dep_resolve(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_pkg_dep_resolve", mod);
    /* i32 aurora_pkg_dep_get_count() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_pkg_dep_get_count", mod);
    /* ptr aurora_pkg_dep_get_name(i32) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_pkg_dep_get_name", mod);

    /* ── Offline Cache (3) ── */
    /* i32 aurora_pkg_cache_list(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_pkg_cache_list", mod);
    /* void aurora_pkg_cache_clear() */
    llvm::Function::Create(llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_pkg_cache_clear", mod);
    /* ptr aurora_pkg_cache_path() */
    llvm::Function::Create(llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_pkg_cache_path", mod);

    /* ── Phase 23: Hot Reload (22) ── */

    auto* s = ptr; // string = i8*

    /* i32 aurora_hotreload_watch(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_hotreload_watch", mod);
    /* void aurora_hotreload_unwatch(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_hotreload_unwatch", mod);
    /* ptr aurora_hotreload_poll() */
    llvm::Function::Create(llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_hotreload_poll", mod);
    /* void aurora_hotreload_clear() */
    llvm::Function::Create(llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_hotreload_clear", mod);

    /* void aurora_hotreload_ui_set_rebuild_fn(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_hotreload_ui_set_rebuild_fn", mod);
    /* void aurora_hotreload_ui_rebuild(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_hotreload_ui_rebuild", mod);
    /* void aurora_hotreload_ui_preserve_state(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { s, s }, false),
        llvm::Function::ExternalLinkage, "aurora_hotreload_ui_preserve_state", mod);
    /* ptr aurora_hotreload_ui_get_state(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_hotreload_ui_get_state", mod);

    /* i32 aurora_hotreload_code_reload(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_hotreload_code_reload", mod);
    /* i32 aurora_hotreload_code_is_stale(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_hotreload_code_is_stale", mod);
    /* void aurora_hotreload_code_set_reload_fn(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_hotreload_code_set_reload_fn", mod);
    /* ptr aurora_hotreload_code_get_version(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_hotreload_code_get_version", mod);

    /* void aurora_hotreload_asset_reload(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_hotreload_asset_reload", mod);
    /* void aurora_hotreload_asset_set_reload_fn(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_hotreload_asset_set_reload_fn", mod);
    /* i32 aurora_hotreload_asset_is_dirty(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_hotreload_asset_is_dirty", mod);

    /* void aurora_hotreload_state_save(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { s, s }, false),
        llvm::Function::ExternalLinkage, "aurora_hotreload_state_save", mod);
    /* ptr aurora_hotreload_state_load(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_hotreload_state_load", mod);
    /* void aurora_hotreload_state_clear() */
    llvm::Function::Create(llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_hotreload_state_clear", mod);

    /* void aurora_hotreload_console_open() */
    llvm::Function::Create(llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_hotreload_console_open", mod);
    /* void aurora_hotreload_console_close() */
    llvm::Function::Create(llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_hotreload_console_close", mod);
    /* void aurora_hotreload_console_log(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_hotreload_console_log", mod);
    /* ptr aurora_hotreload_console_exec(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_hotreload_console_exec", mod);

    /* ── Phase 24: Testing Framework (30) ── */

    /* i32 aurora_test_describe(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { s, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_test_describe", mod);
    /* i32 aurora_test_it(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { s, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_test_it", mod);
    /* i32 aurora_test_run() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_test_run", mod);

    /* void aurora_test_assert(i32, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { i32, s }, false),
        llvm::Function::ExternalLinkage, "aurora_test_assert", mod);
    /* void aurora_test_assert_eq_int(i32, i32, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { i32, i32, s }, false),
        llvm::Function::ExternalLinkage, "aurora_test_assert_eq_int", mod);
    /* void aurora_test_assert_eq_float(f64, f64, f64, ptr) */
    {
        auto* d = llvm::Type::getDoubleTy(*ctx);
        llvm::Function::Create(llvm::FunctionType::get(v, { d, d, d, s }, false),
            llvm::Function::ExternalLinkage, "aurora_test_assert_eq_float", mod);
    }
    /* void aurora_test_assert_eq_str(ptr, ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { s, s, s }, false),
        llvm::Function::ExternalLinkage, "aurora_test_assert_eq_str", mod);
    /* void aurora_test_assert_true(i32, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { i32, s }, false),
        llvm::Function::ExternalLinkage, "aurora_test_assert_true", mod);
    /* void aurora_test_assert_false(i32, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { i32, s }, false),
        llvm::Function::ExternalLinkage, "aurora_test_assert_false", mod);
    /* void aurora_test_assert_null(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, s }, false),
        llvm::Function::ExternalLinkage, "aurora_test_assert_null", mod);

    /* void aurora_test_setup(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_test_setup", mod);
    /* void aurora_test_teardown(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_test_teardown", mod);
    /* i32 aurora_test_integration(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { s, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_test_integration", mod);

    /* i32 aurora_test_widget(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { s, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_test_widget", mod);
    /* ptr aurora_test_find_widget(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_test_find_widget", mod);
    /* i32 aurora_test_click(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_test_click", mod);

    /* i32 aurora_test_bench(ptr, ptr, i32) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { s, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_test_bench", mod);
    /* void aurora_test_bench_start() */
    llvm::Function::Create(llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_test_bench_start", mod);
    /* void aurora_test_bench_end() */
    llvm::Function::Create(llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_test_bench_end", mod);
    /* f64 aurora_test_bench_result() */
    {
        auto* d = llvm::Type::getDoubleTy(*ctx);
        llvm::Function::Create(llvm::FunctionType::get(d, {}, false),
            llvm::Function::ExternalLinkage, "aurora_test_bench_result", mod);
    }

    /* i32 aurora_test_snapshot(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { s, s }, false),
        llvm::Function::ExternalLinkage, "aurora_test_snapshot", mod);
    /* i32 aurora_test_snapshot_update(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { s, s }, false),
        llvm::Function::ExternalLinkage, "aurora_test_snapshot_update", mod);
    /* i32 aurora_test_snapshot_delete(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_test_snapshot_delete", mod);

    /* i32 aurora_test_coverage_start() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_test_coverage_start", mod);
    /* i32 aurora_test_coverage_stop() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_test_coverage_stop", mod);
    /* ptr aurora_test_coverage_report() */
    llvm::Function::Create(llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_test_coverage_report", mod);

    /* i32 aurora_test_pass_count() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_test_pass_count", mod);
    /* i32 aurora_test_fail_count() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_test_fail_count", mod);
    /* i32 aurora_test_total_count() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_test_total_count", mod);
    /* ptr aurora_test_results() */
    llvm::Function::Create(llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_test_results", mod);

    /* ── Phase 25: Developer Tools (34) ── */

    /* ptr aurora_dev_format(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_dev_format", mod);
    /* i32 aurora_dev_format_file(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_dev_format_file", mod);
    /* void aurora_dev_format_set_tab_size(i32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_dev_format_set_tab_size", mod);
    /* void aurora_dev_format_set_spaces(i32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_dev_format_set_spaces", mod);

    /* ptr aurora_dev_lint(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_dev_lint", mod);
    /* ptr aurora_dev_lint_file(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_dev_lint_file", mod);
    /* i32 aurora_dev_lint_set_rule(ptr, i32) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { s, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_dev_lint_set_rule", mod);

    /* i32 aurora_dev_lsp_start(i32) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_dev_lsp_start", mod);
    /* i32 aurora_dev_lsp_stop() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_dev_lsp_stop", mod);
    /* i32 aurora_dev_lsp_is_running() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_dev_lsp_is_running", mod);
    /* i32 aurora_dev_lsp_set_root(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_dev_lsp_set_root", mod);

    /* ptr aurora_dev_complete(ptr, i32, i32) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { s, i32, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_dev_complete", mod);
    /* ptr aurora_dev_complete_file(ptr, i32, i32) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { s, i32, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_dev_complete_file", mod);
    /* ptr aurora_dev_complete_detail(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_dev_complete_detail", mod);

    /* i32 aurora_dev_debug_attach(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_dev_debug_attach", mod);
    /* i32 aurora_dev_debug_break() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_dev_debug_break", mod);
    /* i32 aurora_dev_debug_continue() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_dev_debug_continue", mod);
    /* i32 aurora_dev_debug_step_over() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_dev_debug_step_over", mod);

    /* i32 aurora_dev_profiler_start() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_dev_profiler_start", mod);
    /* i32 aurora_dev_profiler_stop() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_dev_profiler_stop", mod);
    /* ptr aurora_dev_profiler_report() */
    llvm::Function::Create(llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_dev_profiler_report", mod);
    /* void aurora_dev_profiler_reset() */
    llvm::Function::Create(llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_dev_profiler_reset", mod);

    /* ptr aurora_dev_inspector_tree() */
    llvm::Function::Create(llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_dev_inspector_tree", mod);
    /* ptr aurora_dev_inspector_select(i32, i32) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { i32, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_dev_inspector_select", mod);
    /* void aurora_dev_inspector_refresh() */
    llvm::Function::Create(llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_dev_inspector_refresh", mod);

    /* ptr aurora_dev_memory_stats() */
    llvm::Function::Create(llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_dev_memory_stats", mod);
    /* ptr aurora_dev_memory_snapshot() */
    llvm::Function::Create(llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_dev_memory_snapshot", mod);
    /* i32 aurora_dev_memory_leak_check() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_dev_memory_leak_check", mod);

    /* i32 aurora_dev_perf_start() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_dev_perf_start", mod);
    /* i32 aurora_dev_perf_stop() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_dev_perf_stop", mod);
    /* f64 aurora_dev_perf_fps() */
    {
        auto* d = llvm::Type::getDoubleTy(*ctx);
        llvm::Function::Create(llvm::FunctionType::get(d, {}, false),
            llvm::Function::ExternalLinkage, "aurora_dev_perf_fps", mod);
    }
    /* f64 aurora_dev_perf_frame_time() */
    {
        auto* d = llvm::Type::getDoubleTy(*ctx);
        llvm::Function::Create(llvm::FunctionType::get(d, {}, false),
            llvm::Function::ExternalLinkage, "aurora_dev_perf_frame_time", mod);
    }
    /* ptr aurora_dev_perf_report() */
    llvm::Function::Create(llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_dev_perf_report", mod);

    /* ── Phase 27: Security Module (30) ── */

    /* i32 aurora_sec_sandbox_init() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_sec_sandbox_init", mod);
    /* i32 aurora_sec_sandbox_allow_path(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_sec_sandbox_allow_path", mod);
    /* i32 aurora_sec_sandbox_check_path(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_sec_sandbox_check_path", mod);
    /* void aurora_sec_sandbox_destroy() */
    llvm::Function::Create(llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_sec_sandbox_destroy", mod);

    /* i32 aurora_sec_permission_check(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_sec_permission_check", mod);
    /* i32 aurora_sec_permission_request(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_sec_permission_request", mod);
    /* ptr aurora_sec_permission_list() */
    llvm::Function::Create(llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_sec_permission_list", mod);
    /* i32 aurora_sec_permission_revoke(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_sec_permission_revoke", mod);

    /* ptr aurora_sec_storage_open(ptr, ptr, i32) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { s, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_sec_storage_open", mod);
    /* i32 aurora_sec_storage_set(ptr, ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr, s, s }, false),
        llvm::Function::ExternalLinkage, "aurora_sec_storage_set", mod);
    /* ptr aurora_sec_storage_get(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { ptr, s }, false),
        llvm::Function::ExternalLinkage, "aurora_sec_storage_get", mod);
    /* i32 aurora_sec_storage_remove(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr, s }, false),
        llvm::Function::ExternalLinkage, "aurora_sec_storage_remove", mod);
    /* void aurora_sec_storage_close(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_sec_storage_close", mod);

    /* i32 aurora_sec_generate_key(ptr, i32) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_sec_generate_key", mod);
    /* i32 aurora_sec_generate_iv(ptr, i32) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_sec_generate_iv", mod);
    /* i32 aurora_sec_encrypt(ptr, i32, ptr, ptr, i32, ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr, i32, ptr, ptr, i32, ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_sec_encrypt", mod);
    /* i32 aurora_sec_decrypt(ptr, i32, ptr, ptr, i32, ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr, i32, ptr, ptr, i32, ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_sec_decrypt", mod);
    /* i32 aurora_sec_pbkdf2(ptr, ptr, i32, i32, ptr, i32) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { s, ptr, i32, i32, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_sec_pbkdf2", mod);

    /* ptr aurora_sec_cert_load(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_sec_cert_load", mod);
    /* ptr aurora_sec_cert_info(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_sec_cert_info", mod);
    /* i32 aurora_sec_cert_verify(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr, s }, false),
        llvm::Function::ExternalLinkage, "aurora_sec_cert_verify", mod);
    /* void aurora_sec_cert_free(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_sec_cert_free", mod);

    /* ptr aurora_sec_sha256(ptr, i32) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_sec_sha256", mod);
    /* ptr aurora_sec_hmac_sha256(ptr, i32, ptr, i32) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { ptr, i32, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_sec_hmac_sha256", mod);
    /* ptr aurora_sec_hash_password(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_sec_hash_password", mod);
    /* i32 aurora_sec_verify_password(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { s, s }, false),
        llvm::Function::ExternalLinkage, "aurora_sec_verify_password", mod);

    /* ptr aurora_sec_token_generate(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { s, s }, false),
        llvm::Function::ExternalLinkage, "aurora_sec_token_generate", mod);
    /* i32 aurora_sec_token_verify(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { s, s }, false),
        llvm::Function::ExternalLinkage, "aurora_sec_token_verify", mod);
    /* ptr aurora_sec_basic_auth(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { s, s }, false),
        llvm::Function::ExternalLinkage, "aurora_sec_basic_auth", mod);
    /* ptr aurora_sec_bearer_auth(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { s }, false),
        llvm::Function::ExternalLinkage, "aurora_sec_bearer_auth", mod);
    /* double aurora_anim_ease_linear(double) */
    {
        auto* d = llvm::Type::getDoubleTy(*ctx);
        auto fn = [&](const char* name) {
            llvm::Function::Create(llvm::FunctionType::get(d, { d }, false),
                llvm::Function::ExternalLinkage, name, mod);
        };
        fn("aurora_anim_ease_linear"); fn("aurora_anim_ease_in_quad");
        fn("aurora_anim_ease_out_quad"); fn("aurora_anim_ease_in_out_quad");
        fn("aurora_anim_ease_in_cubic"); fn("aurora_anim_ease_out_cubic");
        fn("aurora_anim_ease_in_out_cubic"); fn("aurora_anim_ease_in_quart");
        fn("aurora_anim_ease_out_quart"); fn("aurora_anim_ease_in_out_quart");
        fn("aurora_anim_ease_in_elastic"); fn("aurora_anim_ease_out_elastic");
        fn("aurora_anim_ease_in_out_elastic"); fn("aurora_anim_ease_in_bounce");
        fn("aurora_anim_ease_out_bounce"); fn("aurora_anim_ease_in_out_bounce");
        fn("aurora_anim_ease_in_back"); fn("aurora_anim_ease_out_back");
        fn("aurora_anim_ease_in_out_back"); fn("aurora_anim_ease_apply");
        /* tween (7) */
        /* ptr aurora_anim_tween_new(ptr, double, double, double) */
        llvm::Function::Create(llvm::FunctionType::get(ptr, { ptr, d, d, d }, false),
            llvm::Function::ExternalLinkage, "aurora_anim_tween_new", mod);
        /* void aurora_anim_tween_on_update(ptr, ptr) */
        llvm::Function::Create(llvm::FunctionType::get(v, { ptr, ptr }, false),
            llvm::Function::ExternalLinkage, "aurora_anim_tween_on_update", mod);
        llvm::Function::Create(llvm::FunctionType::get(v, { ptr, ptr }, false),
            llvm::Function::ExternalLinkage, "aurora_anim_tween_on_done", mod);
        /* void aurora_anim_tween_update(ptr, double) */
        llvm::Function::Create(llvm::FunctionType::get(v, { ptr, d }, false),
            llvm::Function::ExternalLinkage, "aurora_anim_tween_update", mod);
        /* i32 aurora_anim_tween_is_done(ptr) */
        llvm::Function::Create(llvm::FunctionType::get(i32, { ptr }, false),
            llvm::Function::ExternalLinkage, "aurora_anim_tween_is_done", mod);
        /* double aurora_anim_tween_value(ptr) */
        llvm::Function::Create(llvm::FunctionType::get(d, { ptr }, false),
            llvm::Function::ExternalLinkage, "aurora_anim_tween_value", mod);
        /* void aurora_anim_tween_free(ptr) */
        llvm::Function::Create(llvm::FunctionType::get(v, { ptr }, false),
            llvm::Function::ExternalLinkage, "aurora_anim_tween_free", mod);
        /* seq (7) */
        llvm::Function::Create(llvm::FunctionType::get(ptr, {}, false),
            llvm::Function::ExternalLinkage, "aurora_anim_seq_new", mod);
        llvm::Function::Create(llvm::FunctionType::get(v, { ptr, ptr }, false),
            llvm::Function::ExternalLinkage, "aurora_anim_seq_add", mod);
        llvm::Function::Create(llvm::FunctionType::get(v, { ptr, ptr }, false),
            llvm::Function::ExternalLinkage, "aurora_anim_seq_on_update", mod);
        llvm::Function::Create(llvm::FunctionType::get(v, { ptr, ptr }, false),
            llvm::Function::ExternalLinkage, "aurora_anim_seq_on_done", mod);
        llvm::Function::Create(llvm::FunctionType::get(v, { ptr, d }, false),
            llvm::Function::ExternalLinkage, "aurora_anim_seq_update", mod);
        llvm::Function::Create(llvm::FunctionType::get(i32, { ptr }, false),
            llvm::Function::ExternalLinkage, "aurora_anim_seq_is_done", mod);
        llvm::Function::Create(llvm::FunctionType::get(v, { ptr }, false),
            llvm::Function::ExternalLinkage, "aurora_anim_seq_free", mod);
        /* ctrl (5) */
        llvm::Function::Create(llvm::FunctionType::get(ptr, {}, false),
            llvm::Function::ExternalLinkage, "aurora_anim_ctrl_new", mod);
        llvm::Function::Create(llvm::FunctionType::get(v, { ptr, ptr }, false),
            llvm::Function::ExternalLinkage, "aurora_anim_ctrl_add", mod);
        llvm::Function::Create(llvm::FunctionType::get(v, { ptr, d }, false),
            llvm::Function::ExternalLinkage, "aurora_anim_ctrl_update", mod);
        llvm::Function::Create(llvm::FunctionType::get(i32, { ptr }, false),
            llvm::Function::ExternalLinkage, "aurora_anim_ctrl_is_done", mod);
        llvm::Function::Create(llvm::FunctionType::get(v, { ptr }, false),
            llvm::Function::ExternalLinkage, "aurora_anim_ctrl_free", mod);
        /* kf (5) */
        llvm::Function::Create(llvm::FunctionType::get(ptr, {}, false),
            llvm::Function::ExternalLinkage, "aurora_anim_kf_new", mod);
        llvm::Function::Create(llvm::FunctionType::get(v, { ptr, d, d }, false),
            llvm::Function::ExternalLinkage, "aurora_anim_kf_set_key", mod);
        llvm::Function::Create(llvm::FunctionType::get(d, { ptr, d }, false),
            llvm::Function::ExternalLinkage, "aurora_anim_kf_evaluate", mod);
        llvm::Function::Create(llvm::FunctionType::get(v, { ptr }, false),
            llvm::Function::ExternalLinkage, "aurora_anim_kf_free", mod);
        /* animation_create / animation_update */
        llvm::Function::Create(llvm::FunctionType::get(ptr, { ptr, ptr, i32, d }, false),
            llvm::Function::ExternalLinkage, "aurora_animation_create", mod);
        llvm::Function::Create(llvm::FunctionType::get(v, { ptr, d }, false),
            llvm::Function::ExternalLinkage, "aurora_animation_update", mod);
    }

    /* ── Audio helpers ── */
    /* i32 aurora_audio_init() */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_audio_init", mod);
    /* void aurora_audio_shutdown() */
    llvm::Function::Create(
        llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_audio_shutdown", mod);
    /* ptr aurora_audio_src_new() */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_audio_src_new", mod);
    /* i32 aurora_audio_src_load_file(ptr, ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_src_load_file", mod);
    /* i32 aurora_audio_src_load_mem(ptr, ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_src_load_mem", mod);
    /* void aurora_audio_src_play(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_src_play", mod);
    /* void aurora_audio_src_stop(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_src_stop", mod);
    /* void aurora_audio_src_pause(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_src_pause", mod);
    /* void aurora_audio_src_resume(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_src_resume", mod);
    /* i32 aurora_audio_src_is_playing(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_src_is_playing", mod);
    /* void aurora_audio_src_free(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_src_free", mod);
    /* void aurora_audio_src_set_volume(ptr, double) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, llvm::Type::getDoubleTy(*ctx) }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_src_set_volume", mod);
    /* double aurora_audio_src_get_volume(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getDoubleTy(*ctx), { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_src_get_volume", mod);
    /* void aurora_audio_src_set_pitch(ptr, double) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, llvm::Type::getDoubleTy(*ctx) }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_src_set_pitch", mod);
    /* double aurora_audio_src_get_pitch(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getDoubleTy(*ctx), { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_src_get_pitch", mod);
    /* void aurora_audio_src_set_looping(ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_src_set_looping", mod);
    /* i32 aurora_audio_src_get_looping(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_src_get_looping", mod);
    /* void aurora_audio_src_set_time(ptr, double) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, llvm::Type::getDoubleTy(*ctx) }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_src_set_time", mod);
    /* double aurora_audio_src_get_time(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getDoubleTy(*ctx), { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_src_get_time", mod);
    /* double aurora_audio_src_get_duration(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getDoubleTy(*ctx), { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_src_get_duration", mod);
    /* void aurora_audio_src_set_reverb(ptr, double, double) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, llvm::Type::getDoubleTy(*ctx), llvm::Type::getDoubleTy(*ctx) }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_src_set_reverb", mod);
    /* void aurora_audio_src_set_echo(ptr, double, double) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, llvm::Type::getDoubleTy(*ctx), llvm::Type::getDoubleTy(*ctx) }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_src_set_echo", mod);
    /* void aurora_audio_src_set_lowpass(ptr, double) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, llvm::Type::getDoubleTy(*ctx) }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_src_set_lowpass", mod);
    /* void aurora_audio_src_set_highpass(ptr, double) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, llvm::Type::getDoubleTy(*ctx) }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_src_set_highpass", mod);
    /* void aurora_audio_src_clear_effects(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_src_clear_effects", mod);
    /* ptr aurora_audio_rec_new(i32, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { i32, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_rec_new", mod);
    /* void aurora_audio_rec_start(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_rec_start", mod);
    /* i32 aurora_audio_rec_read(ptr, ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_rec_read", mod);
    /* void aurora_audio_rec_stop(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_rec_stop", mod);
    /* void aurora_audio_rec_free(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_rec_free", mod);
    /* Legacy compat */
    /* i32 aurora_audio_play_file(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_play_file", mod);
    /* void aurora_audio_play(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_play", mod);
    /* void aurora_audio_play_tone(i32, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { i32, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_play_tone", mod);
    /* void aurora_audio_stop_all() */
    llvm::Function::Create(
        llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_audio_stop_all", mod);

    /* ── Phase 12: Video helpers ── */
    /* i32 aurora_video_init() */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_video_init", mod);
    /* void aurora_video_shutdown() */
    llvm::Function::Create(
        llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_video_shutdown", mod);
    /* ptr aurora_video_player_new() */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_video_player_new", mod);
    /* i32 aurora_video_player_open(ptr, ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_video_player_open", mod);
    /* void aurora_video_player_play(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_video_player_play", mod);
    /* void aurora_video_player_pause(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_video_player_pause", mod);
    /* void aurora_video_player_resume(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_video_player_resume", mod);
    /* void aurora_video_player_stop(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_video_player_stop", mod);
    /* i32 aurora_video_player_is_playing(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_video_player_is_playing", mod);
    /* i32 aurora_video_player_has_ended(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_video_player_has_ended", mod);
    /* void aurora_video_player_free(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_video_player_free", mod);
    /* void aurora_video_player_set_time(ptr, double) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, llvm::Type::getDoubleTy(*ctx) }, false),
        llvm::Function::ExternalLinkage, "aurora_video_player_set_time", mod);
    /* double aurora_video_player_get_time(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getDoubleTy(*ctx), { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_video_player_get_time", mod);
    /* double aurora_video_player_get_duration(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getDoubleTy(*ctx), { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_video_player_get_duration", mod);
    /* i32 aurora_video_player_get_width(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_video_player_get_width", mod);
    /* i32 aurora_video_player_get_height(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_video_player_get_height", mod);
    /* double aurora_video_player_get_fps(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getDoubleTy(*ctx), { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_video_player_get_fps", mod);
    /* void aurora_video_player_set_volume(ptr, double) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, llvm::Type::getDoubleTy(*ctx) }, false),
        llvm::Function::ExternalLinkage, "aurora_video_player_set_volume", mod);
    /* double aurora_video_player_get_volume(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getDoubleTy(*ctx), { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_video_player_get_volume", mod);
    /* void aurora_video_player_set_looping(ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_video_player_set_looping", mod);
    /* i32 aurora_video_player_get_looping(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_video_player_get_looping", mod);
    /* i32 aurora_video_player_subtitle_load(ptr, ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_video_player_subtitle_load", mod);
    /* ptr aurora_video_player_subtitle_get_text(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_video_player_subtitle_get_text", mod);
    /* i32 aurora_video_player_read_frame(ptr, ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_video_player_read_frame", mod);
    /* void aurora_video_player_decode(ptr, double) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, llvm::Type::getDoubleTy(*ctx) }, false),
        llvm::Function::ExternalLinkage, "aurora_video_player_decode", mod);

    /* ── Phase 13: Networking helpers ── */
    /* i32 aurora_net_http_get(ptr, ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_net_http_get", mod);
    /* i32 aurora_net_http_post(ptr, ptr, ptr, ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr, ptr, ptr, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_net_http_post", mod);
    /* i32 aurora_net_http_put(ptr, ptr, ptr, ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr, ptr, ptr, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_net_http_put", mod);
    /* i32 aurora_net_http_delete(ptr, ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_net_http_delete", mod);
    /* i32 aurora_net_http_patch(ptr, ptr, ptr, ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr, ptr, ptr, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_net_http_patch", mod);
    /* i32 aurora_net_http_head(ptr, ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_net_http_head", mod);
    /* i32 aurora_net_http_get_ex(ptr, ptr, ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr, ptr, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_net_http_get_ex", mod);
    /* i32 aurora_net_http_post_ex(ptr, ptr, ptr, ptr, ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr, ptr, ptr, ptr, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_net_http_post_ex", mod);
    /* i32 aurora_net_http_status(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_net_http_status", mod);
    /* i32 aurora_net_url_encode(ptr, ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_net_url_encode", mod);
    /* i32 aurora_net_url_decode(ptr, ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_net_url_decode", mod);
    /* i32 aurora_net_dns_lookup(ptr, ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_net_dns_lookup", mod);
    /* i64 aurora_net_tcp_connect(ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_net_tcp_connect", mod);
    /* i32 aurora_net_tcp_send(i64, ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { i64, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_net_tcp_send", mod);
    /* i32 aurora_net_tcp_recv(i64, ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { i64, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_net_tcp_recv", mod);
    /* void aurora_net_tcp_close(i64) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { i64 }, false),
        llvm::Function::ExternalLinkage, "aurora_net_tcp_close", mod);
    /* i64 aurora_net_udp_socket() */
    llvm::Function::Create(
        llvm::FunctionType::get(i64, {}, false),
        llvm::Function::ExternalLinkage, "aurora_net_udp_socket", mod);
    /* i32 aurora_net_udp_sendto(i64, ptr, i32, ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { i64, ptr, i32, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_net_udp_sendto", mod);
    /* i32 aurora_net_udp_recvfrom(i64, ptr, i32, ptr, i32, ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { i64, ptr, i32, ptr, i32, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_net_udp_recvfrom", mod);
    /* void aurora_net_udp_close(i64) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { i64 }, false),
        llvm::Function::ExternalLinkage, "aurora_net_udp_close", mod);
    /* i64 aurora_net_ws_connect(ptr, ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_net_ws_connect", mod);
    /* i32 aurora_net_ws_send(i64, ptr, i32, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { i64, ptr, i32, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_net_ws_send", mod);
    /* i32 aurora_net_ws_recv(i64, ptr, i32, ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { i64, ptr, i32, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_net_ws_recv", mod);
    /* void aurora_net_ws_close(i64) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { i64 }, false),
        llvm::Function::ExternalLinkage, "aurora_net_ws_close", mod);
    /* ptr aurora_net_multipart_begin(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_net_multipart_begin", mod);
    /* ptr aurora_net_multipart_add_field(ptr, ptr, ptr, ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, ptr, ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_net_multipart_add_field", mod);
    /* ptr aurora_net_multipart_add_file(ptr, ptr, ptr, ptr, ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, ptr, ptr, ptr, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_net_multipart_add_file", mod);
    /* ptr aurora_net_multipart_end(ptr, ptr, ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_net_multipart_end", mod);
    /* i32 aurora_net_download(ptr, ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_net_download", mod);
    /* void aurora_net_auth_basic(ptr, ptr, ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_net_auth_basic", mod);
    /* void aurora_net_auth_bearer(ptr, ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_net_auth_bearer", mod);
    /* i32 aurora_net_resolve_host(ptr, ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_net_resolve_host", mod);
    /* i32 aurora_send_all(i64, ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { i64, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_send_all", mod);

    /* ── Phase 14: Serialization helpers ── */
    /* ptr aurora_serialize_json(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_serialize_json", mod);
    /* ptr aurora_deserialize_json(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_deserialize_json", mod);
    /* ptr aurora_serialize_binary(ptr, ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_serialize_binary", mod);
    /* ptr aurora_deserialize_binary(ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_deserialize_binary", mod);
    /* ptr aurora_serialize(ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_serialize", mod);
    /* ptr aurora_deserialize(ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_deserialize", mod);
    /* i32 aurora_serialize_to_file(ptr, ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_serialize_to_file", mod);
    /* ptr aurora_deserialize_from_file(ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_deserialize_from_file", mod);
    /* i32 aurora_serial_detect_format(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_serial_detect_format", mod);

    /* ── Phase 15: Database / SQLite ── */
    auto* f64 = llvm::Type::getDoubleTy(*ctx);
    /* ptr aurora_db_sqlite_open(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_sqlite_open", mod);
    /* void aurora_db_sqlite_close(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_sqlite_close", mod);
    /* ptr aurora_db_sqlite_exec(ptr, ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_sqlite_exec", mod);
    /* ptr aurora_db_sqlite_query_json(ptr, ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_sqlite_query_json", mod);
    /* i32 aurora_db_sqlite_execute(ptr, ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_sqlite_execute", mod);
    /* ptr aurora_db_sqlite_prepare(ptr, ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_sqlite_prepare", mod);
    /* void aurora_db_sqlite_bind_int(ptr, i32, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, i32, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_db_sqlite_bind_int", mod);
    /* void aurora_db_sqlite_bind_double(ptr, i32, f64) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, i32, f64 }, false),
        llvm::Function::ExternalLinkage, "aurora_db_sqlite_bind_double", mod);
    /* void aurora_db_sqlite_bind_text(ptr, i32, ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, i32, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_sqlite_bind_text", mod);
    /* void aurora_db_sqlite_bind_null(ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_db_sqlite_bind_null", mod);
    /* i32 aurora_db_sqlite_step(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_sqlite_step", mod);
    /* i32 aurora_db_sqlite_column_count(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_sqlite_column_count", mod);
    /* ptr aurora_db_sqlite_column_name(ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_db_sqlite_column_name", mod);
    /* i32 aurora_db_sqlite_column_type(ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_db_sqlite_column_type", mod);
    /* ptr aurora_db_sqlite_column_text(ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_db_sqlite_column_text", mod);
    /* i32 aurora_db_sqlite_column_int(ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_db_sqlite_column_int", mod);
    /* f64 aurora_db_sqlite_column_double(ptr, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(f64, { ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_db_sqlite_column_double", mod);
    /* void aurora_db_sqlite_reset(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_sqlite_reset", mod);
    /* void aurora_db_sqlite_finalize(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_sqlite_finalize", mod);
    /* i32 aurora_db_sqlite_begin(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_sqlite_begin", mod);
    /* i32 aurora_db_sqlite_commit(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_sqlite_commit", mod);
    /* i32 aurora_db_sqlite_rollback(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_sqlite_rollback", mod);
    /* i64 aurora_db_sqlite_last_insert_rowid(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_sqlite_last_insert_rowid", mod);
    /* i32 aurora_db_sqlite_changes(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_sqlite_changes", mod);
    /* i32 aurora_db_sqlite_errcode(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_sqlite_errcode", mod);
    /* ptr aurora_db_sqlite_errmsg(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_sqlite_errmsg", mod);
    /* ptr aurora_db_sqlite_prep_query_json(ptr) */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_db_sqlite_prep_query_json", mod);

    /* ── Phase 18: Desktop Integration ── */
    /* void aurora_desktop_init() */
    llvm::Function::Create(llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_desktop_init", mod);
    /* void aurora_desktop_shutdown() */
    llvm::Function::Create(llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_desktop_shutdown", mod);
    /* ptr aurora_desktop_tray_create(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_desktop_tray_create", mod);
    /* void aurora_desktop_tray_destroy(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_desktop_tray_destroy", mod);
    /* void aurora_desktop_tray_set_tooltip(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_desktop_tray_set_tooltip", mod);
    /* void aurora_desktop_tray_set_icon(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_desktop_tray_set_icon", mod);
    /* void aurora_desktop_tray_add_menu_item(ptr, i32, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, i32, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_desktop_tray_add_menu_item", mod);
    /* void aurora_desktop_tray_add_menu_separator(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_desktop_tray_add_menu_separator", mod);
    /* void aurora_desktop_tray_show_balloon(ptr, ptr, ptr, i32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, ptr, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_desktop_tray_show_balloon", mod);
    /* void aurora_desktop_tray_set_callback(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_desktop_tray_set_callback", mod);
    /* void aurora_desktop_tray_set_visible(ptr, i32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_desktop_tray_set_visible", mod);
    /* i32 aurora_desktop_notification_show(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_desktop_notification_show", mod);
    /* void aurora_desktop_notification_hide() */
    llvm::Function::Create(llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_desktop_notification_hide", mod);
    /* i32 aurora_desktop_clipboard_set_text(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_desktop_clipboard_set_text", mod);
    /* ptr aurora_desktop_clipboard_get_text() */
    llvm::Function::Create(llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_desktop_clipboard_get_text", mod);
    /* ptr aurora_desktop_drop_target_create(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_desktop_drop_target_create", mod);
    /* void aurora_desktop_drop_target_destroy(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_desktop_drop_target_destroy", mod);
    /* i32 aurora_desktop_assoc_register(ptr, ptr, ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr, ptr, ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_desktop_assoc_register", mod);
    /* i32 aurora_desktop_assoc_unregister(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_desktop_assoc_unregister", mod);
    /* i32 aurora_desktop_assoc_is_registered(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_desktop_assoc_is_registered", mod);
    /* i32 aurora_desktop_startup_set(ptr, ptr, i32) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr, ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_desktop_startup_set", mod);
    /* i32 aurora_desktop_startup_is_enabled(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_desktop_startup_is_enabled", mod);
    /* i32 aurora_desktop_window_set_effect(ptr, i32) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_desktop_window_set_effect", mod);
    /* i32 aurora_desktop_window_set_dark_mode(ptr, i32) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_desktop_window_set_dark_mode", mod);
    /* i32 aurora_desktop_window_set_round_corners(ptr, i32) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_desktop_window_set_round_corners", mod);
    /* i32 aurora_desktop_hotkey_register(i32, i32, i32, i32, i32, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { i32, i32, i32, i32, i32, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_desktop_hotkey_register", mod);
    /* void aurora_desktop_hotkey_unregister(i32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_desktop_hotkey_unregister", mod);

    /* ── Phase 17: Mobile Widgets ── */
    /* void mw_init() */
    llvm::Function::Create(llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "mw_init", mod);
    /* void mw_shutdown() */
    llvm::Function::Create(llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "mw_shutdown", mod);
    /* ptr mw_create(i32) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { i32 }, false),
        llvm::Function::ExternalLinkage, "mw_create", mod);
    /* void mw_destroy(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "mw_destroy", mod);
    /* void mw_add_child(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "mw_add_child", mod);
    /* void mw_remove_child(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "mw_remove_child", mod);
    /* void mw_set_pos(ptr, f32, f32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, f32, f32 }, false),
        llvm::Function::ExternalLinkage, "mw_set_pos", mod);
    /* void mw_set_size(ptr, f32, f32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, f32, f32 }, false),
        llvm::Function::ExternalLinkage, "mw_set_size", mod);
    /* f32 mw_get_width(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(f32, { ptr }, false),
        llvm::Function::ExternalLinkage, "mw_get_width", mod);
    /* f32 mw_get_height(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(f32, { ptr }, false),
        llvm::Function::ExternalLinkage, "mw_get_height", mod);
    /* void mw_layout(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "mw_layout", mod);
    /* void mw_set_align(ptr, i32, i32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, i32, i32 }, false),
        llvm::Function::ExternalLinkage, "mw_set_align", mod);
    /* void mw_set_spacing(ptr, f32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, f32 }, false),
        llvm::Function::ExternalLinkage, "mw_set_spacing", mod);
    /* void mw_set_padding(ptr, f32, f32, f32, f32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, f32, f32, f32, f32 }, false),
        llvm::Function::ExternalLinkage, "mw_set_padding", mod);
    /* void mw_set_margin(ptr, f32, f32, f32, f32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, f32, f32, f32, f32 }, false),
        llvm::Function::ExternalLinkage, "mw_set_margin", mod);
    /* void mw_set_bg_color(ptr, f32, f32, f32, f32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, f32, f32, f32, f32 }, false),
        llvm::Function::ExternalLinkage, "mw_set_bg_color", mod);
    /* void mw_set_text_color(ptr, f32, f32, f32, f32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, f32, f32, f32, f32 }, false),
        llvm::Function::ExternalLinkage, "mw_set_text_color", mod);
    /* void mw_set_font_size(ptr, f32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, f32 }, false),
        llvm::Function::ExternalLinkage, "mw_set_font_size", mod);
    /* void mw_set_text(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "mw_set_text", mod);
    /* ptr mw_get_text(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "mw_get_text", mod);
    /* void mw_set_enabled(ptr, i32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "mw_set_enabled", mod);
    /* void mw_set_visible(ptr, i32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "mw_set_visible", mod);
    /* void mw_set_image(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "mw_set_image", mod);
    /* void mw_set_value(ptr, f32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, f32 }, false),
        llvm::Function::ExternalLinkage, "mw_set_value", mod);
    /* void mw_set_selected(ptr, i32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "mw_set_selected", mod);
    /* i32 mw_get_type(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "mw_get_type", mod);
    /* void mw_add_item(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "mw_add_item", mod);
    /* void mw_remove_item(ptr, i32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "mw_remove_item", mod);
    /* void mw_clear_items(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "mw_clear_items", mod);
    /* void mw_set_callback(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "mw_set_callback", mod);
    /* i32 mw_handle_touch(ptr, f32, f32, i32) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr, f32, f32, i32 }, false),
        llvm::Function::ExternalLinkage, "mw_handle_touch", mod);
    /* void mw_set_scroll_pos(ptr, f32, f32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, f32, f32 }, false),
        llvm::Function::ExternalLinkage, "mw_set_scroll_pos", mod);
    /* void mw_get_scroll_pos(ptr, ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "mw_get_scroll_pos", mod);
    /* void mw_render(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "mw_render", mod);

    /* ── Phase 16: Mobile Runtime ── */

    /* ── Android ── */
    /* i32 aurora_android_screen_width() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_android_screen_width", mod);
    /* i32 aurora_android_screen_height() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_android_screen_height", mod);
    /* i32 aurora_android_orientation() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_android_orientation", mod);
    /* void aurora_android_set_orientation(i32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_android_set_orientation", mod);
    /* i32 aurora_android_touch_count() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_android_touch_count", mod);
    /* i32 aurora_android_touch_get(i32, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { i32, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_android_touch_get", mod);
    /* void aurora_android_touch_clear() */
    llvm::Function::Create(llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_android_touch_clear", mod);
    /* i32 aurora_android_key_pressed(i32) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_android_key_pressed", mod);
    /* ptr aurora_android_ime_text() */
    llvm::Function::Create(llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_android_ime_text", mod);
    /* void aurora_android_sensors_enable(i32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_android_sensors_enable", mod);
    /* void aurora_android_sensors_disable(i32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_android_sensors_disable", mod);
    /* i32 aurora_android_sensor_data(i32, ptr, ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { i32, ptr, ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_android_sensor_data", mod);
    /* i32 aurora_android_check_permission(ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_android_check_permission", mod);
    /* void aurora_android_toast(ptr, i32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_android_toast", mod);
    /* ptr aurora_android_device_model() */
    llvm::Function::Create(llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_android_device_model", mod);
    /* ptr aurora_android_device_manufacturer() */
    llvm::Function::Create(llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_android_device_manufacturer", mod);
    /* ptr aurora_android_os_version() */
    llvm::Function::Create(llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_android_os_version", mod);
    /* f32 aurora_android_density() */
    llvm::Function::Create(llvm::FunctionType::get(f32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_android_density", mod);
    /* i32 aurora_android_dp_to_px(i32) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_android_dp_to_px", mod);
    /* i32 aurora_android_px_to_dp(i32) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_android_px_to_dp", mod);

    /* ── iOS ── */
    /* f32 aurora_ios_screen_width() */
    llvm::Function::Create(llvm::FunctionType::get(f32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_ios_screen_width", mod);
    /* f32 aurora_ios_screen_height() */
    llvm::Function::Create(llvm::FunctionType::get(f32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_ios_screen_height", mod);
    /* f32 aurora_ios_screen_scale() */
    llvm::Function::Create(llvm::FunctionType::get(f32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_ios_screen_scale", mod);
    /* i32 aurora_ios_orientation() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_ios_orientation", mod);
    /* void aurora_ios_set_orientation(i32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_ios_set_orientation", mod);
    /* i32 aurora_ios_touch_count() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_ios_touch_count", mod);
    /* i32 aurora_ios_touch_get(i32, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(i32, { i32, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_ios_touch_get", mod);
    /* ptr aurora_ios_path_for_resource(ptr, ptr) */
    llvm::Function::Create(llvm::FunctionType::get(ptr, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_ios_path_for_resource", mod);
    /* ptr aurora_ios_documents_path() */
    llvm::Function::Create(llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_ios_documents_path", mod);
    /* ptr aurora_ios_cache_path() */
    llvm::Function::Create(llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_ios_cache_path", mod);
    /* ptr aurora_ios_device_model() */
    llvm::Function::Create(llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_ios_device_model", mod);
    /* ptr aurora_ios_os_version() */
    llvm::Function::Create(llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_ios_os_version", mod);
    /* i32 aurora_ios_is_ipad() */
    llvm::Function::Create(llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_ios_is_ipad", mod);
    /* void aurora_ios_haptic(i32) */
    llvm::Function::Create(llvm::FunctionType::get(v, { i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_ios_haptic", mod);

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

    /* route_register(i8* method, i8* path, i8* handler) */
    route_register_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_route_register", mod);

    /* style_apply(i8*, i8*) */
    style_apply_ = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_style_apply", mod);

    /* ── Component runtime ── */
    /* AuroraComponent* aurora_component_create(i8* name, i32 x, i32 y, i32 w, i32 h) */
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
    /* void aurora_component_set_widget_type(AuroraComponent*, i32) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_component_set_widget_type", mod);
    /* void aurora_component_set_update_fn(AuroraComponent*, void(*)(void*,double)) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_component_set_update_fn", mod);

    /* ── Win32 UI ── */
    /* int aurora_ui_win32_init(i8* title, i32 w, i32 h) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr, i32, i32 }, false),
        llvm::Function::ExternalLinkage, "aurora_ui_win32_init", mod);
    /* int aurora_ui_win32_create_control(AuroraComponent*) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_ui_win32_create_control", mod);
    /* void aurora_ui_win32_destroy_control(AuroraComponent*) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_ui_win32_destroy_control", mod);
    /* void aurora_ui_win32_set_text(AuroraComponent*, i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_ui_win32_set_text", mod);
    /* i8* aurora_ui_win32_get_text(AuroraComponent*) */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_ui_win32_get_text", mod);
    /* void aurora_ui_win32_listbox_add(AuroraComponent*, i8*) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_ui_win32_listbox_add", mod);
    /* void aurora_ui_win32_listbox_clear(AuroraComponent*) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_ui_win32_listbox_clear", mod);
    /* i32 aurora_ui_win32_listbox_selected(AuroraComponent*) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_ui_win32_listbox_selected", mod);
    /* i32 aurora_ui_win32_listbox_count(AuroraComponent*) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_ui_win32_listbox_count", mod);
    /* void aurora_ui_win32_mount(AuroraComponent*) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_ui_win32_mount", mod);
    /* void aurora_ui_win32_sync_tree(AuroraComponent*) */
    llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_ui_win32_sync_tree", mod);
    /* int aurora_ui_win32_run() */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_ui_win32_run", mod);
    /* int aurora_ui_win32_pump() */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_ui_win32_pump", mod);
    /* void aurora_ui_win32_shutdown() */
    llvm::Function::Create(
        llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_ui_win32_shutdown", mod);
    /* int aurora_ui_win32_event_type() */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_ui_win32_event_type", mod);
    /* i8* aurora_ui_win32_event_source() */
    llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_ui_win32_event_source", mod);
    /* int aurora_ui_win32_event_data() */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_ui_win32_event_data", mod);

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
