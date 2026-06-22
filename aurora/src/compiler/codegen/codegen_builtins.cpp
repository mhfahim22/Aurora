#include "compiler/codegen_builtins.hpp"
#include "compiler/ast.hpp"

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>

static llvm::Type* i64_ty(llvm::LLVMContext& ctx) {
    return llvm::Type::getInt64Ty(ctx);
}

static llvm::Type* i8_ty(llvm::LLVMContext& ctx) {
    return llvm::Type::getInt8Ty(ctx);
}

static llvm::Type* void_ty(llvm::LLVMContext& ctx) {
    return llvm::Type::getVoidTy(ctx);
}

static llvm::Type* ptr_ty(llvm::LLVMContext& ctx) {
    return llvm::PointerType::getUnqual(ctx);
}

void register_builtins(llvm::Module* module, llvm::LLVMContext& ctx,
                       BuiltinFunctions& builtins) {
    auto* i64   = i64_ty(ctx);
    auto* v     = void_ty(ctx);
    auto* ptr   = ptr_ty(ctx);
    auto* dbl   = llvm::Type::getDoubleTy(ctx);

    /* ── Math/Array builtins ── */
    builtins.len   = llvm::Function::Create(
        llvm::FunctionType::get(i64, { i64 }, false),
        llvm::Function::ExternalLinkage, "aurora_builtin_len", module);
    builtins.strlen_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_strlen", module);
    builtins.sum   = llvm::Function::Create(
        llvm::FunctionType::get(i64, { i64 }, false),
        llvm::Function::ExternalLinkage, "aurora_builtin_sum", module);
    builtins.min   = llvm::Function::Create(
        llvm::FunctionType::get(i64, { i64 }, false),
        llvm::Function::ExternalLinkage, "aurora_builtin_min", module);
    builtins.max   = llvm::Function::Create(
        llvm::FunctionType::get(i64, { i64 }, false),
        llvm::Function::ExternalLinkage, "aurora_builtin_max", module);
    builtins.range = llvm::Function::Create(
        llvm::FunctionType::get(i64, { i64, i64 }, false),
        llvm::Function::ExternalLinkage, "aurora_builtin_range", module);

    /* ── I/O builtins ── */
    builtins.outputln_int = llvm::Function::Create(
        llvm::FunctionType::get(v, { i64 }, false),
        llvm::Function::ExternalLinkage, "aurora_outputln_int", module);
    builtins.outputln_float = llvm::Function::Create(
        llvm::FunctionType::get(v, { dbl }, false),
        llvm::Function::ExternalLinkage, "aurora_outputln_float", module);
    builtins.outputln_str = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_outputln_str", module);
    builtins.outputln_bool = llvm::Function::Create(
        llvm::FunctionType::get(v, { i64 }, false),
        llvm::Function::ExternalLinkage, "aurora_outputln_bool", module);
    builtins.outputN = llvm::Function::Create(
        llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "aurora_outputN", module);
    builtins.outputf = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, i64, ptr }, false),
        llvm::Function::ExternalLinkage, "aurora_outputf", module);
    builtins.input = llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "aurora_input", module);

    /* ── String builtins ── */
    builtins.upper = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_upper", module);
    builtins.lower = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_lower", module);
    builtins.trim = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_trim", module);
    builtins.replace_fn = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr, ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_replace", module);
    builtins.split_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_split", module);
    builtins.join_fn = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { i64, ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_join", module);
    builtins.has_str = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_has_str", module);
    builtins.starts = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_starts", module);
    builtins.ends = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_ends", module);
    builtins.reverse_str = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_reverse_str", module);

    /* ── Math builtins ── */
    builtins.abs_fn = llvm::Function::Create(
        llvm::FunctionType::get(dbl, { dbl }, false),
        llvm::Function::ExternalLinkage, "builtin_abs_val", module);
    builtins.sqrt_fn = llvm::Function::Create(
        llvm::FunctionType::get(dbl, { dbl }, false),
        llvm::Function::ExternalLinkage, "builtin_sqrt", module);
    builtins.floor_fn = llvm::Function::Create(
        llvm::FunctionType::get(dbl, { dbl }, false),
        llvm::Function::ExternalLinkage, "builtin_floor_val", module);
    builtins.ceil_fn = llvm::Function::Create(
        llvm::FunctionType::get(dbl, { dbl }, false),
        llvm::Function::ExternalLinkage, "builtin_ceil_val", module);
    builtins.round_fn = llvm::Function::Create(
        llvm::FunctionType::get(dbl, { dbl }, false),
        llvm::Function::ExternalLinkage, "builtin_round_val", module);
    builtins.pow_fn = llvm::Function::Create(
        llvm::FunctionType::get(dbl, { dbl, dbl }, false),
        llvm::Function::ExternalLinkage, "builtin_pow_val", module);
    builtins.clamp_fn = llvm::Function::Create(
        llvm::FunctionType::get(dbl, { dbl, dbl, dbl }, false),
        llvm::Function::ExternalLinkage, "builtin_clamp", module);
    builtins.min2 = llvm::Function::Create(
        llvm::FunctionType::get(dbl, { dbl, dbl }, false),
        llvm::Function::ExternalLinkage, "builtin_min2", module);
    builtins.max2 = llvm::Function::Create(
        llvm::FunctionType::get(dbl, { dbl, dbl }, false),
        llvm::Function::ExternalLinkage, "builtin_max2", module);
    builtins.rand_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, {}, false),
        llvm::Function::ExternalLinkage, "builtin_rand", module);

    /* ── File builtins ── */
    builtins.read_file = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_read_file", module);
    builtins.write_file = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_write_file", module);
    builtins.append_file = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_append_file", module);
    builtins.file_exists = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_file_exists", module);
    builtins.delete_file = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_delete_file", module);
    builtins.copy_file = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_copy_file", module);
    builtins.move_file = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_move_file", module);

    /* ── Path builtins ── */
    builtins.cwd_fn = llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "builtin_cwd", module);
    builtins.cd_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_cd", module);
    builtins.dirname_fn = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_dirname", module);
    builtins.basename_fn = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_basename", module);
    builtins.ext_fn = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_ext", module);

    /* ── Time builtins ── */
    builtins.now_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, {}, false),
        llvm::Function::ExternalLinkage, "builtin_now", module);
    builtins.stamp_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, {}, false),
        llvm::Function::ExternalLinkage, "builtin_stamp", module);
    builtins.sleep_fn = llvm::Function::Create(
        llvm::FunctionType::get(v, { i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_sleep", module);

    /* ── OS builtins ── */
    builtins.os_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_os", module);
    builtins.cpu_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, {}, false),
        llvm::Function::ExternalLinkage, "builtin_cpu_count", module);
    builtins.mem_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, {}, false),
        llvm::Function::ExternalLinkage, "builtin_memory_usage", module);
    builtins.env_fn = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_env", module);
    builtins.run_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_run", module);
    builtins.exit_fn = llvm::Function::Create(
        llvm::FunctionType::get(v, { i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_exit", module);

    /* ── JSON builtins ── */
    builtins.encode_json = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_encode_json", module);
    builtins.decode_json = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_decode_json", module);

    /* ── HTTP builtins ── */
    builtins.http_get = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, ptr, i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_http_get", module);
    builtins.http_post = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, ptr, ptr, i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_http_post", module);

    /* ── Collection builtins ── */
    builtins.push_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { i64, i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_push", module);
    builtins.push_str_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { i64, ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_push_str", module);
    builtins.pop_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_pop", module);
    builtins.insert_fn = llvm::Function::Create(
        llvm::FunctionType::get(v, { i64, i64, i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_insert", module);
    builtins.remove_fn = llvm::Function::Create(
        llvm::FunctionType::get(v, { i64, i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_remove", module);
    builtins.clear_fn = llvm::Function::Create(
        llvm::FunctionType::get(v, { i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_clear", module);
    builtins.has_arr = llvm::Function::Create(
        llvm::FunctionType::get(i64, { i64, i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_has_arr", module);
    builtins.sort_fn = llvm::Function::Create(
        llvm::FunctionType::get(v, { i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_sort", module);
    builtins.reverse_arr = llvm::Function::Create(
        llvm::FunctionType::get(v, { i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_reverse_arr", module);
    builtins.unique_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_unique", module);

    /* ── Error / Confirm builtins ── */
    builtins.error_fn = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_error", module);
    builtins.ask_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_ask", module);

    /* ── Type conversion ── */
    builtins.char_fn = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_char", module);

    /* ── Async builtins ── */
    builtins.spawn_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_spawn", module);
    builtins.await_fn = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_await", module);
    builtins.chan_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_chan", module);
    builtins.send_fn = llvm::Function::Create(
        llvm::FunctionType::get(v, { i64, i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_send", module);
    builtins.recv_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_recv", module);

    /* ── Fiber builtins ── */
    builtins.fiber_create_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_fiber_create", module);
    builtins.fiber_resume_fn = llvm::Function::Create(
        llvm::FunctionType::get(v, { i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_fiber_resume", module);
    builtins.fiber_yield_fn = llvm::Function::Create(
        llvm::FunctionType::get(v, {}, false),
        llvm::Function::ExternalLinkage, "builtin_fiber_yield", module);
    builtins.fiber_is_done_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_fiber_is_done", module);
    builtins.fiber_get_result_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_fiber_get_result", module);
    builtins.fiber_destroy_fn = llvm::Function::Create(
        llvm::FunctionType::get(v, { i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_fiber_destroy", module);

    /* ── Event bus builtins ── */
    builtins.event_on_fn = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_event_on", module);
    builtins.event_off_fn = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_event_off", module);
    builtins.event_emit_fn = llvm::Function::Create(
        llvm::FunctionType::get(v, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_event_emit", module);

    /* ── Performance builtins ── */
    builtins.measure_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, {}, false),
        llvm::Function::ExternalLinkage, "builtin_measure", module);
    builtins.bench_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_bench", module);
    builtins.profile_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_profile", module);
    builtins.trace_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_trace", module);

    /* ── Reflection builtins ── */
    builtins.fields_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_fields", module);
    builtins.methods_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_methods", module);

    /* ── Package builtins ── */
    builtins.install_fn = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_install", module);
    builtins.update_fn = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_update", module);
    builtins.search_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_search", module);

    /* ── Backend / Server builtins ── */
    /* Route/Middleware */
    builtins.route_group_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_route_group", module);
    builtins.middleware_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_middleware", module);
    builtins.next_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, {}, false),
        llvm::Function::ExternalLinkage, "builtin_next", module);
    builtins.rate_limit_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { i64, i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_rate_limit", module);
    builtins.cors_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, {}, false),
        llvm::Function::ExternalLinkage, "builtin_cors", module);
    builtins.csrf_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, {}, false),
        llvm::Function::ExternalLinkage, "builtin_csrf", module);

    /* Session */
    builtins.session_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, {}, false),
        llvm::Function::ExternalLinkage, "builtin_session", module);
    builtins.session_get_fn = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_session_get", module);
    builtins.session_set_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_session_set", module);
    builtins.session_delete_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_session_delete", module);

    /* Cookie */
    builtins.cookie_get_fn = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_cookie_get", module);
    builtins.cookie_set_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_cookie_set", module);
    builtins.cookie_delete_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_cookie_delete", module);

    /* Proxy/Streaming */
    builtins.proxy_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_proxy", module);
    builtins.reverse_proxy_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_reverse_proxy", module);
    builtins.stream_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_stream", module);
    builtins.stream_file_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_stream_file", module);
    builtins.sse_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_sse", module);
    builtins.webhook_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_webhook", module);

    /* Health/Metrics */
    builtins.health_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, {}, false),
        llvm::Function::ExternalLinkage, "builtin_health", module);
    builtins.metrics_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, {}, false),
        llvm::Function::ExternalLinkage, "builtin_metrics", module);
    builtins.trace_id_fn = llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "builtin_trace_id", module);
    builtins.request_id_fn = llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "builtin_request_id", module);
    builtins.audit_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_audit", module);

    /* Lock/Sync */
    builtins.lock_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_lock", module);
    builtins.unlock_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_unlock", module);
    builtins.atomic_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_atomic", module);
    builtins.retry_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_retry", module);
    builtins.timeout_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_timeout", module);
    builtins.circuit_breaker_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_circuit_breaker", module);

    /* Pool */
    builtins.pool_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_pool", module);
    builtins.worker_pool_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_worker_pool", module);
    builtins.batch_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { i64, i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_batch", module);
    builtins.paginate_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_paginate", module);

    /* DB/ORM */
    builtins.index_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_index", module);
    builtins.migrate_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, {}, false),
        llvm::Function::ExternalLinkage, "builtin_migrate", module);
    builtins.seed_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, {}, false),
        llvm::Function::ExternalLinkage, "builtin_seed", module);
    builtins.model_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_model", module);
    builtins.schema_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_schema", module);
    builtins.validate_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_validate", module);
    builtins.sanitize_fn = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_sanitize", module);

    /* Throttle/Debounce */
    builtins.throttle_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_throttle", module);
    builtins.debounce_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_debounce", module);

    /* Crypto */
    builtins.sign_fn = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_sign", module);
    builtins.verify_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_verify", module);
    builtins.secret_fn = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_secret", module);
    builtins.vault_fn = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_vault", module);

    /* Compress/Serialize */
    builtins.compress_fn = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_compress", module);
    builtins.decompress_fn = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_decompress", module);
    builtins.serialize_fn = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_serialize", module);
    builtins.deserialize_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_deserialize", module);

    /* Event/PubSub */
    builtins.event_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_event", module);
    builtins.emit_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_emit", module);
    builtins.listen_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_listen", module);
    builtins.publish_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_publish", module);
    builtins.subscribe_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr, ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_subscribe", module);

    /* RPC/Cluster */
    builtins.rpc_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_rpc", module);
    builtins.discover_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_discover", module);
    builtins.cluster_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, {}, false),
        llvm::Function::ExternalLinkage, "builtin_cluster", module);
    builtins.node_id_fn = llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "builtin_node_id", module);
    builtins.leader_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, {}, false),
        llvm::Function::ExternalLinkage, "builtin_leader", module);
    builtins.shard_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_shard", module);
    builtins.replica_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, {}, false),
        llvm::Function::ExternalLinkage, "builtin_replica", module);

    /* Backup */
    builtins.backup_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_backup", module);
    builtins.restore_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_restore", module);

    /* Monitor/Profile */
    builtins.monitor_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, {}, false),
        llvm::Function::ExternalLinkage, "builtin_monitor", module);
    builtins.profile_request_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, {}, false),
        llvm::Function::ExternalLinkage, "builtin_profile_request", module);
    builtins.memory_snapshot_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, {}, false),
        llvm::Function::ExternalLinkage, "builtin_memory_snapshot", module);
    builtins.gc_collect_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, {}, false),
        llvm::Function::ExternalLinkage, "builtin_gc_collect", module);
    builtins.hot_reload_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, {}, false),
        llvm::Function::ExternalLinkage, "builtin_hot_reload", module);

    /* Plugin/FeatureFlag */
    builtins.plugin_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_plugin", module);
    builtins.feature_flag_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_feature_flag", module);

    /* Tenant */
    builtins.tenant_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_tenant", module);
    builtins.tenant_context_fn = llvm::Function::Create(
        llvm::FunctionType::get(ptr, {}, false),
        llvm::Function::ExternalLinkage, "builtin_tenant_context", module);

    /* Geo/Captcha */
    builtins.geoip_fn = llvm::Function::Create(
        llvm::FunctionType::get(ptr, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_geoip", module);
    builtins.captcha_verify_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_captcha_verify", module);

    /* Payment/Analytics */
    builtins.payment_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_payment", module);
    builtins.invoice_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_invoice", module);
    builtins.analytics_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_analytics", module);

    /* Search/AI extended */
    builtins.search_engine_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, {}, false),
        llvm::Function::ExternalLinkage, "builtin_search_engine", module);
    builtins.vector_search_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_vector_search", module);
    builtins.semantic_search_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { i64 }, false),
        llvm::Function::ExternalLinkage, "builtin_semantic_search", module);
    builtins.embed_store_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_embed_store", module);
    builtins.embed_query_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_embed_query", module);
    builtins.ai_agent_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_ai_agent", module);
    builtins.tool_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_tool", module);
    builtins.workflow_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_workflow", module);
    builtins.pipeline_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_pipeline", module);
    builtins.step_fn = llvm::Function::Create(
        llvm::FunctionType::get(i64, { ptr }, false),
        llvm::Function::ExternalLinkage, "builtin_step", module);

    /* ── AI/ML functional builtins ── */
    auto* ft_i64_void = llvm::FunctionType::get(i64, {}, false);
    auto* ft_i64_i64 = llvm::FunctionType::get(i64, { i64 }, false);
    auto* ft_i64_i64_i64 = llvm::FunctionType::get(i64, { i64, i64 }, false);
    auto* ft_i64_i64_i64_i64 = llvm::FunctionType::get(i64, { i64, i64, i64 }, false);
    auto* ft_i64_i64_dbl = llvm::FunctionType::get(i64, { i64, dbl }, false);
    auto* ft_i64_i64_ptr = llvm::FunctionType::get(i64, { i64, ptr }, false);
    auto* ft_i64_ptr = llvm::FunctionType::get(i64, { ptr }, false);
    auto* ft_i64_dbl = llvm::FunctionType::get(i64, { dbl }, false);
    auto* ft_i64_i64_i64_i64_i64 = llvm::FunctionType::get(i64, { i64, i64, i64, i64 }, false);

    /* Data loading */
    builtins.csv_fn        = llvm::Function::Create(ft_i64_ptr, llvm::Function::ExternalLinkage, "csv", module);
    builtins.data_fn       = llvm::Function::Create(ft_i64_ptr, llvm::Function::ExternalLinkage, "data", module);

    /* Data processing */
    builtins.clean_fn      = llvm::Function::Create(ft_i64_i64_i64, llvm::Function::ExternalLinkage, "clean", module);
    builtins.normalize_fn  = llvm::Function::Create(ft_i64_i64_i64, llvm::Function::ExternalLinkage, "normalize", module);
    builtins.standard_fn   = llvm::Function::Create(ft_i64_i64_i64, llvm::Function::ExternalLinkage, "standard", module);
    builtins.shuffle_fn    = llvm::Function::Create(ft_i64_i64_i64, llvm::Function::ExternalLinkage, "shuffle", module);
    builtins.split_data_fn = llvm::Function::Create(ft_i64_i64_dbl, llvm::Function::ExternalLinkage, "split_data", module);

    /* Model lifecycle */
    builtins.model_create_fn = llvm::Function::Create(ft_i64_ptr, llvm::Function::ExternalLinkage, "model_create", module);
    builtins.model_save_fn   = llvm::Function::Create(ft_i64_i64_ptr, llvm::Function::ExternalLinkage, "model_save", module);
    builtins.model_load_fn   = llvm::Function::Create(ft_i64_ptr, llvm::Function::ExternalLinkage, "model_load", module);

    /* Model config */
    builtins.set_loss_fn         = llvm::Function::Create(ft_i64_i64_ptr, llvm::Function::ExternalLinkage, "set_loss", module);
    builtins.set_optimizer_fn    = llvm::Function::Create(ft_i64_i64_ptr, llvm::Function::ExternalLinkage, "set_optimizer", module);
    builtins.set_lr_fn           = llvm::Function::Create(ft_i64_i64_dbl, llvm::Function::ExternalLinkage, "set_lr", module);
    builtins.set_batch_size_fn   = llvm::Function::Create(ft_i64_i64_i64, llvm::Function::ExternalLinkage, "set_batch_size", module);
    builtins.set_epochs_fn       = llvm::Function::Create(ft_i64_i64_i64, llvm::Function::ExternalLinkage, "set_epochs", module);
    builtins.set_validation_split_fn = llvm::Function::Create(ft_i64_i64_dbl, llvm::Function::ExternalLinkage, "set_validation_split", module);
    builtins.set_verbose_fn      = llvm::Function::Create(ft_i64_i64_i64, llvm::Function::ExternalLinkage, "set_verbose", module);
    builtins.set_early_stop_fn   = llvm::Function::Create(ft_i64_i64_i64, llvm::Function::ExternalLinkage, "set_early_stop", module);

    /* Layer creation */
    builtins.dense_fn  = llvm::Function::Create(ft_i64_i64_ptr, llvm::Function::ExternalLinkage, "dense", module);
    builtins.conv_fn   = llvm::Function::Create(ft_i64_i64_i64_i64, llvm::Function::ExternalLinkage, "conv", module);
    builtins.lstm_fn   = llvm::Function::Create(ft_i64_i64_i64, llvm::Function::ExternalLinkage, "lstm", module);
    builtins.gru_fn    = llvm::Function::Create(ft_i64_i64_i64, llvm::Function::ExternalLinkage, "gru", module);
    builtins.dropout_fn = llvm::Function::Create(ft_i64_dbl, llvm::Function::ExternalLinkage, "dropout", module);
    builtins.batchnorm_fn   = llvm::Function::Create(ft_i64_void, llvm::Function::ExternalLinkage, "batchnorm", module);
    builtins.attention_fn   = llvm::Function::Create(ft_i64_void, llvm::Function::ExternalLinkage, "attention", module);
    builtins.transformer_fn = llvm::Function::Create(ft_i64_void, llvm::Function::ExternalLinkage, "transformer", module);
    builtins.embedding_fn = llvm::Function::Create(ft_i64_i64_i64, llvm::Function::ExternalLinkage, "embedding", module);
    builtins.layernorm_fn = llvm::Function::Create(ft_i64_void, llvm::Function::ExternalLinkage, "layernorm", module);

    /* Model operations */
    builtins.add_fn     = llvm::Function::Create(ft_i64_i64_i64, llvm::Function::ExternalLinkage, "ml_add", module);
    builtins.train_fn   = llvm::Function::Create(ft_i64_i64_i64, llvm::Function::ExternalLinkage, "ml_train", module);
    builtins.fit_fn     = llvm::Function::Create(ft_i64_i64_i64_i64, llvm::Function::ExternalLinkage, "ml_fit", module);
    builtins.test_fn    = llvm::Function::Create(ft_i64_i64_i64, llvm::Function::ExternalLinkage, "ml_test", module);
    builtins.predict_fn = llvm::Function::Create(ft_i64_i64_i64, llvm::Function::ExternalLinkage, "ml_predict", module);
    builtins.retrain_fn = llvm::Function::Create(ft_i64_i64_i64, llvm::Function::ExternalLinkage, "ml_retrain", module);
}

static llvm::Value* to_ptr(llvm::IRBuilder<>& builder, llvm::LLVMContext& ctx,
                            llvm::Value* val) {
    if (val->getType()->isPointerTy()) return val;
    return builder.CreateIntToPtr(val, ptr_ty(ctx), "i64toptr");
}

llvm::Value* codegen_builtin_call(
    const std::string& name,
    const ASTNode* node,
    llvm::IRBuilder<>& builder,
    llvm::LLVMContext& ctx,
    const BuiltinFunctions& builtins,
    std::function<llvm::Value*(const ASTNode*)> gen_expr,
    llvm::Module* module)
{
    auto* i64 = i64_ty(ctx);
    auto* v   = void_ty(ctx);
    auto* ptr = ptr_ty(ctx);
    auto* dbl = llvm::Type::getDoubleTy(ctx);

    llvm::Value* ret = nullptr;

    ret = codegen_builtin_section1(name, node, builder, ctx, builtins, gen_expr, module);
    if (ret) return ret;

    ret = codegen_builtin_section2(name, node, builder, ctx, builtins, gen_expr, module);
    if (ret) return ret;

    ret = codegen_builtin_section3(name, node, builder, ctx, builtins, gen_expr, module);
    if (ret) return ret;

    ret = codegen_builtin_section4(name, node, builder, ctx, builtins, gen_expr, module);
    if (ret) return ret;

    ret = codegen_builtin_section5(name, node, builder, ctx, builtins, gen_expr, module);
    if (ret) return ret;

    return nullptr;
}
