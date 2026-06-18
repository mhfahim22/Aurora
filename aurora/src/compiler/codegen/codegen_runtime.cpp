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

    /* ── Audio helpers ── */
    /* i32 aurora_audio_init() */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, {}, false),
        llvm::Function::ExternalLinkage, "aurora_audio_init", mod);
    /* i32 aurora_audio_play_file(cstring) */
    llvm::Function::Create(
        llvm::FunctionType::get(i32, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_audio_play_file", mod);
    /* void aurora_audio_shutdown() */
    llvm::Function::Create(
        llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_audio_shutdown", mod);

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
