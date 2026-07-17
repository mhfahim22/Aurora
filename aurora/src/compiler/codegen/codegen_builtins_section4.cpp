#include "compiler/codegen_builtins.hpp"
#include "compiler/ast.hpp"

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>

static llvm::Type* i64_ty(llvm::LLVMContext& ctx) { return llvm::Type::getInt64Ty(ctx); }
static llvm::Type* i8_ty(llvm::LLVMContext& ctx) { return llvm::Type::getInt8Ty(ctx); }
static llvm::Type* void_ty(llvm::LLVMContext& ctx) { return llvm::Type::getVoidTy(ctx); }
static llvm::Type* ptr_ty(llvm::LLVMContext& ctx) { return llvm::PointerType::getUnqual(ctx); }

static llvm::Value* to_ptr(llvm::IRBuilder<>& builder, llvm::LLVMContext& ctx,
                            llvm::Value* val) {
    if (val->getType()->isPointerTy()) return val;
    return builder.CreateIntToPtr(val, ptr_ty(ctx), "i64toptr");
}

llvm::Value* codegen_builtin_section4(
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
    /* ════════════════════════════════════════════
       Backend / Server built-in dispatch
       ════════════════════════════════════════════ */

    /* ── route_group(path) ── */
    if (name == "route_group" && node->args) {
        llvm::Value* p = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.route_group_fn, { p }, "route_group_ret");
    }

    /* ── middleware(fn) ── */
    if (name == "middleware" && node->args) {
        llvm::Value* fn = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.middleware_fn, { fn }, "middleware_ret");
    }

    /* ── next() ── */
    if (name == "next" && !node->args) {
        return builder.CreateCall(builtins.next_fn, {}, "next_ret");
    }

    /* ── rate_limit(max, window) ── */
    if (name == "rate_limit" && node->args && node->args->next) {
        llvm::Value* mx = gen_expr(node->args.get());
        llvm::Value* wd = gen_expr(node->args->next.get());
        return builder.CreateCall(builtins.rate_limit_fn, { mx, wd }, "rate_limit_ret");
    }

    /* ── cors() ── */
    if (name == "cors" && !node->args) {
        return builder.CreateCall(builtins.cors_fn, {}, "cors_ret");
    }

    /* ── csrf() ── */
    if (name == "csrf" && !node->args) {
        return builder.CreateCall(builtins.csrf_fn, {}, "csrf_ret");
    }

    /* ── session() ── */
    if (name == "session" && !node->args) {
        return builder.CreateCall(builtins.session_fn, {}, "session_ret");
    }

    /* ── session_get(key) ── */
    if (name == "session_get" && node->args) {
        llvm::Value* k = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.session_get_fn, { k }, "session_get_ret");
    }

    /* ── session_set(key, value) ── */
    if (name == "session_set" && node->args && node->args->next) {
        llvm::Value* k = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* v = to_ptr(builder, ctx, gen_expr(node->args->next.get()));
        return builder.CreateCall(builtins.session_set_fn, { k, v }, "session_set_ret");
    }

    /* ── session_delete(key) ── */
    if (name == "session_delete" && node->args) {
        llvm::Value* k = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.session_delete_fn, { k }, "session_delete_ret");
    }

    /* ── cookie_get(name) ── */
    if (name == "cookie_get" && node->args) {
        llvm::Value* n = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.cookie_get_fn, { n }, "cookie_get_ret");
    }

    /* ── cookie_set(name, value) ── */
    if (name == "cookie_set" && node->args && node->args->next) {
        llvm::Value* n = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* v = to_ptr(builder, ctx, gen_expr(node->args->next.get()));
        return builder.CreateCall(builtins.cookie_set_fn, { n, v }, "cookie_set_ret");
    }

    /* ── cookie_delete(name) ── */
    if (name == "cookie_delete" && node->args) {
        llvm::Value* n = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.cookie_delete_fn, { n }, "cookie_delete_ret");
    }

    /* ── proxy(url) ── */
    if (name == "proxy" && node->args) {
        llvm::Value* u = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.proxy_fn, { u }, "proxy_ret");
    }

    /* ── reverse_proxy(url) ── */
    if (name == "reverse_proxy" && node->args) {
        llvm::Value* u = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.reverse_proxy_fn, { u }, "reverse_proxy_ret");
    }

    /* ── stream(data) ── */
    if (name == "stream" && node->args) {
        llvm::Value* d = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.stream_fn, { d }, "stream_ret");
    }

    /* ── stream_file(path) ── */
    if (name == "stream_file" && node->args) {
        llvm::Value* p = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.stream_file_fn, { p }, "stream_file_ret");
    }

    /* ── sse(path) ── */
    if (name == "sse" && node->args) {
        llvm::Value* p = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.sse_fn, { p }, "sse_ret");
    }

    /* ── webhook(path) ── */
    if (name == "webhook" && node->args) {
        llvm::Value* p = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.webhook_fn, { p }, "webhook_ret");
    }

    /* ── health() ── */
    if (name == "health" && !node->args) {
        return builder.CreateCall(builtins.health_fn, {}, "health_ret");
    }

    /* ── metrics() ── */
    if (name == "metrics" && !node->args) {
        return builder.CreateCall(builtins.metrics_fn, {}, "metrics_ret");
    }

    /* ── trace_id() ── */
    if (name == "trace_id" && !node->args) {
        return builder.CreateCall(builtins.trace_id_fn, {}, "trace_id_ret");
    }

    /* ── request_id() ── */
    if (name == "request_id" && !node->args) {
        return builder.CreateCall(builtins.request_id_fn, {}, "request_id_ret");
    }

    /* ── audit(data) ── */
    if (name == "audit" && node->args) {
        llvm::Value* d = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.audit_fn, { d }, "audit_ret");
    }

    /* ── lock(key) ── */
    if (name == "lock" && node->args) {
        llvm::Value* k = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.lock_fn, { k }, "lock_ret");
    }

    /* ── unlock(key) ── */
    if (name == "unlock" && node->args) {
        llvm::Value* k = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.unlock_fn, { k }, "unlock_ret");
    }

    /* ── atomic(fn) ── */
    if (name == "atomic" && node->args) {
        llvm::Value* fn = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.atomic_fn, { fn }, "atomic_ret");
    }

    /* ── retry(fn) ── */
    if (name == "retry" && node->args) {
        llvm::Value* fn = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.retry_fn, { fn }, "retry_ret");
    }

    /* ── timeout(fn, ms) ── */
    if (name == "timeout" && node->args && node->args->next) {
        llvm::Value* fn = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* ms = gen_expr(node->args->next.get());
        return builder.CreateCall(builtins.timeout_fn, { fn, ms }, "timeout_ret");
    }

    /* ── circuit_breaker(fn) ── */
    if (name == "circuit_breaker" && node->args) {
        llvm::Value* fn = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.circuit_breaker_fn, { fn }, "circuit_breaker_ret");
    }

    /* ── pool(size) ── */
    if (name == "pool" && node->args) {
        llvm::Value* sz = gen_expr(node->args.get());
        return builder.CreateCall(builtins.pool_fn, { sz }, "pool_ret");
    }

    /* ── worker_pool(size) ── */
    if (name == "worker_pool" && node->args) {
        llvm::Value* sz = gen_expr(node->args.get());
        return builder.CreateCall(builtins.worker_pool_fn, { sz }, "worker_pool_ret");
    }

    /* ── batch(list, size) ── */
    if (name == "batch" && node->args && node->args->next) {
        llvm::Value* lst = gen_expr(node->args.get());
        llvm::Value* sz = gen_expr(node->args->next.get());
        return builder.CreateCall(builtins.batch_fn, { lst, sz }, "batch_ret");
    }

    /* ── paginate(data) ── */
    if (name == "paginate" && node->args) {
        llvm::Value* d = gen_expr(node->args.get());
        return builder.CreateCall(builtins.paginate_fn, { d }, "paginate_ret");
    }

    /* ── index(table, field) ── */
    if (name == "index" && node->args && node->args->next) {
        llvm::Value* tbl = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* fld = to_ptr(builder, ctx, gen_expr(node->args->next.get()));
        return builder.CreateCall(builtins.index_fn, { tbl, fld }, "index_ret");
    }

    /* ── migrate() ── */
    if (name == "migrate" && !node->args) {
        return builder.CreateCall(builtins.migrate_fn, {}, "migrate_ret");
    }

    /* ── seed() ── */
    if (name == "seed" && !node->args) {
        return builder.CreateCall(builtins.seed_fn, {}, "seed_ret");
    }

    /* ── model(name) ── */
    if (name == "model" && node->args) {
        llvm::Value* n = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.model_fn, { n }, "model_ret");
    }

    /* ── schema(definition) ── */
    if (name == "schema" && node->args) {
        llvm::Value* d = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.schema_fn, { d }, "schema_ret");
    }

    /* ── validate(schema, data) ── */
    if (name == "validate" && node->args && node->args->next) {
        llvm::Value* s = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* d = gen_expr(node->args->next.get());
        return builder.CreateCall(builtins.validate_fn, { s, d }, "validate_ret");
    }

    /* ── sanitize(data) / sanitize(data, mode) ── */
    if (name == "sanitize" && node->args) {
        llvm::Value* d = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* mode = llvm::ConstantInt::get(i64, 0, true);
        if (node->args->next)
            mode = gen_expr(node->args->next.get());
        return builder.CreateCall(builtins.sanitize_fn, { d, mode }, "sanitize_ret");
    }

    /* ── template(name, source) ── */
    if (name == "template" && node->args && node->args->next) {
        llvm::Value* n = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* s = to_ptr(builder, ctx, gen_expr(node->args->next.get()));
        return builder.CreateCall(builtins.template_fn, { n, s }, "template_ret");
    }

    /* ── render(name, context) ── */
    if (name == "render" && node->args && node->args->next) {
        llvm::Value* n = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* c = to_ptr(builder, ctx, gen_expr(node->args->next.get()));
        return builder.CreateCall(builtins.render_fn, { n, c }, "render_ret");
    }

    /* ── throttle(limit) ── */
    if (name == "throttle" && node->args) {
        llvm::Value* lim = gen_expr(node->args.get());
        return builder.CreateCall(builtins.throttle_fn, { lim }, "throttle_ret");
    }

    /* ── debounce(ms) ── */
    if (name == "debounce" && node->args) {
        llvm::Value* ms = gen_expr(node->args.get());
        return builder.CreateCall(builtins.debounce_fn, { ms }, "debounce_ret");
    }

    /* ── sign(data) ── */
    if (name == "sign" && node->args) {
        llvm::Value* d = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.sign_fn, { d }, "sign_ret");
    }

    /* ── verify(data, signature) ── */
    if (name == "verify" && node->args && node->args->next) {
        llvm::Value* d = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* s = to_ptr(builder, ctx, gen_expr(node->args->next.get()));
        return builder.CreateCall(builtins.verify_fn, { d, s }, "verify_ret");
    }

    /* ── secret(name) ── */
    if (name == "secret" && node->args) {
        llvm::Value* n = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.secret_fn, { n }, "secret_ret");
    }

    /* ── vault(name) ── */
    if (name == "vault" && node->args) {
        llvm::Value* n = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.vault_fn, { n }, "vault_ret");
    }

    /* ── compress(data) ── */
    if (name == "compress" && node->args) {
        llvm::Value* d = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.compress_fn, { d }, "compress_ret");
    }

    /* ── decompress(data) ── */
    if (name == "decompress" && node->args) {
        llvm::Value* d = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.decompress_fn, { d }, "decompress_ret");
    }

    /* ── serialize(data) ── */
    if (name == "serialize" && node->args) {
        llvm::Value* d = gen_expr(node->args.get());
        return builder.CreateCall(builtins.serialize_fn, { d }, "serialize_ret");
    }

    /* ── deserialize(data) ── */
    if (name == "deserialize" && node->args) {
        llvm::Value* d = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.deserialize_fn, { d }, "deserialize_ret");
    }

    /* ── event(name) ── */
    if (name == "event" && node->args) {
        llvm::Value* n = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.event_fn, { n }, "event_ret");
    }

    /* ── emit(name, data) ── */
    if (name == "emit" && node->args && node->args->next) {
        llvm::Value* n = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* d = to_ptr(builder, ctx, gen_expr(node->args->next.get()));
        return builder.CreateCall(builtins.emit_fn, { n, d }, "emit_ret");
    }

    /* ── listen(name, fn) ── */
    if (name == "listen" && node->args && node->args->next) {
        llvm::Value* n = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* fn = to_ptr(builder, ctx, gen_expr(node->args->next.get()));
        return builder.CreateCall(builtins.listen_fn, { n, fn }, "listen_ret");
    }

    /* ── publish(topic, data) ── */
    if (name == "publish" && node->args && node->args->next) {
        llvm::Value* t = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* d = to_ptr(builder, ctx, gen_expr(node->args->next.get()));
        return builder.CreateCall(builtins.publish_fn, { t, d }, "publish_ret");
    }

    /* ── subscribe(topic, fn) ── */
    if (name == "subscribe" && node->args && node->args->next) {
        llvm::Value* t = to_ptr(builder, ctx, gen_expr(node->args.get()));
        llvm::Value* fn = to_ptr(builder, ctx, gen_expr(node->args->next.get()));
        return builder.CreateCall(builtins.subscribe_fn, { t, fn }, "subscribe_ret");
    }

    /* ── rpc(service) ── */
    if (name == "rpc" && node->args) {
        llvm::Value* s = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.rpc_fn, { s }, "rpc_ret");
    }

    /* ── discover(service) ── */
    if (name == "discover" && node->args) {
        llvm::Value* s = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.discover_fn, { s }, "discover_ret");
    }

    /* ── cluster() ── */
    if (name == "cluster" && !node->args) {
        return builder.CreateCall(builtins.cluster_fn, {}, "cluster_ret");
    }

    /* ── node_id() ── */
    if (name == "node_id" && !node->args) {
        return builder.CreateCall(builtins.node_id_fn, {}, "node_id_ret");
    }

    /* ── leader() ── */
    if (name == "leader" && !node->args) {
        return builder.CreateCall(builtins.leader_fn, {}, "leader_ret");
    }

    /* ── shard(key) ── */
    if (name == "shard" && node->args) {
        llvm::Value* k = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.shard_fn, { k }, "shard_ret");
    }

    /* ── replica() ── */
    if (name == "replica" && !node->args) {
        return builder.CreateCall(builtins.replica_fn, {}, "replica_ret");
    }

    /* ── backup(path) ── */
    if (name == "backup" && node->args) {
        llvm::Value* p = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.backup_fn, { p }, "backup_ret");
    }

    /* ── restore(path) ── */
    if (name == "restore" && node->args) {
        llvm::Value* p = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.restore_fn, { p }, "restore_ret");
    }

    /* ── monitor() ── */
    if (name == "monitor" && !node->args) {
        return builder.CreateCall(builtins.monitor_fn, {}, "monitor_ret");
    }

    /* ── profile_request() ── */
    if (name == "profile_request" && !node->args) {
        return builder.CreateCall(builtins.profile_request_fn, {}, "profile_request_ret");
    }

    /* ── memory_snapshot() ── */
    if (name == "memory_snapshot" && !node->args) {
        return builder.CreateCall(builtins.memory_snapshot_fn, {}, "memory_snapshot_ret");
    }

    /* ── gc_collect() ── */
    if (name == "gc_collect" && !node->args) {
        return builder.CreateCall(builtins.gc_collect_fn, {}, "gc_collect_ret");
    }

    /* ── hot_reload() ── */
    if (name == "hot_reload" && !node->args) {
        return builder.CreateCall(builtins.hot_reload_fn, {}, "hot_reload_ret");
    }

    /* ── plugin(name) ── */
    if (name == "plugin" && node->args) {
        llvm::Value* n = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.plugin_fn, { n }, "plugin_ret");
    }

    /* ── feature_flag(name) ── */
    if (name == "feature_flag" && node->args) {
        llvm::Value* n = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.feature_flag_fn, { n }, "feature_flag_ret");
    }

    /* ── tenant(id) ── */
    if (name == "tenant" && node->args) {
        llvm::Value* id = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.tenant_fn, { id }, "tenant_ret");
    }

    /* ── tenant_context() ── */
    if (name == "tenant_context" && !node->args) {
        return builder.CreateCall(builtins.tenant_context_fn, {}, "tenant_context_ret");
    }

    /* ── geoip(ip) ── */
    if (name == "geoip" && node->args) {
        llvm::Value* ip = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.geoip_fn, { ip }, "geoip_ret");
    }

    /* ── captcha_verify(token) ── */
    if (name == "captcha_verify" && node->args) {
        llvm::Value* tok = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.captcha_verify_fn, { tok }, "captcha_verify_ret");
    }

    /* ── payment(provider) ── */
    if (name == "payment" && node->args) {
        llvm::Value* prov = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.payment_fn, { prov }, "payment_ret");
    }

    /* ── invoice(data) ── */
    if (name == "invoice" && node->args) {
        llvm::Value* d = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.invoice_fn, { d }, "invoice_ret");
    }

    /* ── analytics(event) ── */
    if (name == "analytics" && node->args) {
        llvm::Value* e = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.analytics_fn, { e }, "analytics_ret");
    }

    /* ── search_engine() ── */
    if (name == "search_engine" && !node->args) {
        return builder.CreateCall(builtins.search_engine_fn, {}, "search_engine_ret");
    }

    /* ── vector_search(data) ── */
    if (name == "vector_search" && node->args) {
        llvm::Value* d = gen_expr(node->args.get());
        return builder.CreateCall(builtins.vector_search_fn, { d }, "vector_search_ret");
    }

    /* ── semantic_search(data) ── */
    if (name == "semantic_search" && node->args) {
        llvm::Value* d = gen_expr(node->args.get());
        return builder.CreateCall(builtins.semantic_search_fn, { d }, "semantic_search_ret");
    }

    /* ── embed_store(data) ── */
    if (name == "embed_store" && node->args) {
        llvm::Value* d = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.embed_store_fn, { d }, "embed_store_ret");
    }

    /* ── embed_query(query) ── */
    if (name == "embed_query" && node->args) {
        llvm::Value* q = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.embed_query_fn, { q }, "embed_query_ret");
    }

    /* ── ai_agent(name) ── */
    if (name == "ai_agent" && node->args) {
        llvm::Value* n = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.ai_agent_fn, { n }, "ai_agent_ret");
    }

    /* ── tool(name) ── */
    if (name == "tool" && node->args) {
        llvm::Value* n = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.tool_fn, { n }, "tool_ret");
    }

    /* ── workflow(name) ── */
    if (name == "workflow" && node->args) {
        llvm::Value* n = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.workflow_fn, { n }, "workflow_ret");
    }

    /* ── pipeline(name) ── */
    if (name == "pipeline" && node->args) {
        llvm::Value* n = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.pipeline_fn, { n }, "pipeline_ret");
    }

    /* ── step(name) ── */
    if (name == "step" && node->args) {
        llvm::Value* n = to_ptr(builder, ctx, gen_expr(node->args.get()));
        return builder.CreateCall(builtins.step_fn, { n }, "step_ret");
    }

    /* ════════════════════════════════════════════
       Inlined list access — emit GEP+load/store instead of function call
       List struct layout (x64): data* @ +0, cap:i32 @ +8, len:i32 @ +12
       ════════════════════════════════════════════ */

    /* i64 list_get_unchecked(i8* list, i32 idx) — inline GEP+load with aligned access */
    if (name == "list_get_unchecked" && node->args && node->args->next) {
        llvm::Value* list_val = gen_expr(node->args.get());
        llvm::Value* idx_val = gen_expr(node->args->next.get());
        if (!list_val || !idx_val) return llvm::Constant::getNullValue(i64);
        llvm::Value* list_ptr = to_ptr(builder, ctx, list_val);
        if (!idx_val->getType()->isIntegerTy(32))
            idx_val = builder.CreateTrunc(idx_val, llvm::Type::getInt32Ty(ctx), "idx_trunc");
        auto* i64_ty_local = llvm::Type::getInt64Ty(ctx);
        auto* i8ptr_ty_local = llvm::PointerType::getUnqual(ctx);
        auto* data_ptr_raw = builder.CreateLoad(i8ptr_ty_local, list_ptr);
        auto* data_ptr = builder.CreateBitCast(data_ptr_raw, llvm::PointerType::get(i64_ty_local, 0), "data_bc");
        auto* gep = builder.CreateGEP(i64_ty_local, data_ptr, { idx_val }, "list_get.gep");
        auto* load = builder.CreateLoad(i64_ty_local, gep, "list_get.val");
        load->setAlignment(llvm::Align(8));
        return load;
    }

    /* void list_set_unchecked(i8* list, i32 idx, i64 val) — inline GEP+store with aligned access */
    if (name == "list_set_unchecked" && node->args && node->args->next && node->args->next->next) {
        llvm::Value* list_val = gen_expr(node->args.get());
        llvm::Value* idx_val = gen_expr(node->args->next.get());
        llvm::Value* val_val = gen_expr(node->args->next->next.get());
        if (!list_val || !idx_val || !val_val) return nullptr;
        llvm::Value* list_ptr = to_ptr(builder, ctx, list_val);
        if (!idx_val->getType()->isIntegerTy(32))
            idx_val = builder.CreateTrunc(idx_val, llvm::Type::getInt32Ty(ctx), "idx_trunc");
        auto* i64_ty_local = llvm::Type::getInt64Ty(ctx);
        auto* i8ptr_ty_local = llvm::PointerType::getUnqual(ctx);
        if (val_val->getType()->isDoubleTy() || val_val->getType()->isFloatTy()) {
            val_val = builder.CreateFPToSI(val_val, i64_ty_local, "fptosi");
        } else if (!val_val->getType()->isIntegerTy(64)) {
            val_val = builder.CreateSExt(val_val, i64_ty_local, "val_sext");
        }
        auto* data_ptr_raw = builder.CreateLoad(i8ptr_ty_local, list_ptr);
        auto* data_ptr = builder.CreateBitCast(data_ptr_raw, llvm::PointerType::get(i64_ty_local, 0), "data_bc");
        auto* gep = builder.CreateGEP(i64_ty_local, data_ptr, { idx_val }, "list_set.gep");
        auto* store = builder.CreateStore(val_val, gep);
        store->setAlignment(llvm::Align(8));
        return llvm::UndefValue::get(void_ty(ctx));
    }



    /* double list_get_double(i8* list, i32 idx) — inline GEP+load<double> (no i64 boxing) */
    if (name == "list_get_double" && node->args && node->args->next) {
        llvm::Value* list_val = gen_expr(node->args.get());
        llvm::Value* idx_val = gen_expr(node->args->next.get());
        if (!list_val || !idx_val) return llvm::Constant::getNullValue(i64);
        llvm::Value* list_ptr = to_ptr(builder, ctx, list_val);
        if (!idx_val->getType()->isIntegerTy(32))
            idx_val = builder.CreateTrunc(idx_val, llvm::Type::getInt32Ty(ctx), "idx_trunc");
        auto* f64_ty = llvm::Type::getDoubleTy(ctx);
        auto* i8ptr_ty_local = llvm::PointerType::getUnqual(ctx);
        auto* data_ptr_raw = builder.CreateLoad(i8ptr_ty_local, list_ptr);
        auto* data_ptr = builder.CreateBitCast(data_ptr_raw, llvm::PointerType::get(f64_ty, 0), "data_bc");
        auto* gep = builder.CreateGEP(f64_ty, data_ptr, { idx_val }, "list_get_f64.gep");
        auto* load = builder.CreateLoad(f64_ty, gep, "list_get_f64.val");
        load->setAlignment(llvm::Align(8));
        return load;
    }

    /* void list_set_double(i8* list, i32 idx, double val) — inline GEP+store<double> (no i64 boxing) */
    if (name == "list_set_double" && node->args && node->args->next && node->args->next->next) {
        llvm::Value* list_val = gen_expr(node->args.get());
        llvm::Value* idx_val = gen_expr(node->args->next.get());
        llvm::Value* val_val = gen_expr(node->args->next->next.get());
        if (!list_val || !idx_val || !val_val) return nullptr;
        llvm::Value* list_ptr = to_ptr(builder, ctx, list_val);
        if (!idx_val->getType()->isIntegerTy(32))
            idx_val = builder.CreateTrunc(idx_val, llvm::Type::getInt32Ty(ctx), "idx_trunc");
        auto* f64_ty = llvm::Type::getDoubleTy(ctx);
        auto* i8ptr_ty_local = llvm::PointerType::getUnqual(ctx);
        if (val_val->getType()->isIntegerTy()) {
            val_val = builder.CreateSIToFP(val_val, f64_ty, "itof");
        } else if (val_val->getType()->isFloatTy()) {
            val_val = builder.CreateFPExt(val_val, f64_ty, "ftof");
        }
        auto* data_ptr_raw = builder.CreateLoad(i8ptr_ty_local, list_ptr);
        auto* data_ptr = builder.CreateBitCast(data_ptr_raw, llvm::PointerType::get(f64_ty, 0), "data_bc");
        auto* gep = builder.CreateGEP(f64_ty, data_ptr, { idx_val }, "list_set_f64.gep");
        auto* store = builder.CreateStore(val_val, gep);
        store->setAlignment(llvm::Align(8));
        return llvm::UndefValue::get(void_ty(ctx));
    }

    /* ── Float64Array builtins ── */

    /* double f64array_get(ptr arr, i64 i) — inline GEP+load<double> flat array (no boxing) */
    if (name == "f64array_get" && node->args && node->args->next) {
        llvm::Value* arr_val = gen_expr(node->args.get());
        llvm::Value* idx_val = gen_expr(node->args->next.get());
        if (!arr_val || !idx_val) return llvm::Constant::getNullValue(dbl);
        auto* arr_ptr = to_ptr(builder, ctx, arr_val);
        auto* data_ptr = builder.CreateLoad(ptr, arr_ptr, "f64a.data");
        auto* gep = builder.CreateGEP(dbl, data_ptr, { idx_val }, "f64a.get.gep");
        auto* load = builder.CreateLoad(dbl, gep, "f64a.get.val");
        load->setAlignment(llvm::Align(8));
        return load;
    }

    /* void f64array_set(ptr arr, i64 i, double v) — inline GEP+store<double> (no boxing) */
    if (name == "f64array_set" && node->args && node->args->next && node->args->next->next) {
        llvm::Value* arr_val = gen_expr(node->args.get());
        llvm::Value* idx_val = gen_expr(node->args->next.get());
        llvm::Value* val_val = gen_expr(node->args->next->next.get());
        if (!arr_val || !idx_val || !val_val) return nullptr;
        auto* arr_ptr = to_ptr(builder, ctx, arr_val);
        if (val_val->getType()->isIntegerTy()) {
            val_val = builder.CreateSIToFP(val_val, dbl, "itof");
        } else if (val_val->getType()->isFloatTy()) {
            val_val = builder.CreateFPExt(val_val, dbl, "ftof");
        }
        auto* data_ptr = builder.CreateLoad(ptr, arr_ptr, "f64a.data");
        auto* gep = builder.CreateGEP(dbl, data_ptr, { idx_val }, "f64a.set.gep");
        auto* store = builder.CreateStore(val_val, gep);
        store->setAlignment(llvm::Align(8));
        return llvm::UndefValue::get(v);
    }

    /* void list_matmul(i8* a, i8* b, i8* c, i32 n) — delegate to aurora_list_matmul C helper */
    if (name == "list_matmul" && node->args && node->args->next && node->args->next->next && node->args->next->next->next) {
        llvm::Value* a_val = gen_expr(node->args.get());
        llvm::Value* b_val = gen_expr(node->args->next.get());
        llvm::Value* c_val = gen_expr(node->args->next->next.get());
        llvm::Value* n_val = gen_expr(node->args->next->next->next.get());
        if (!a_val || !b_val || !c_val || !n_val) return nullptr;
        if (n_val->getType()->getIntegerBitWidth() > 32)
            n_val = builder.CreateTrunc(n_val, llvm::Type::getInt32Ty(ctx), "n_trunc");
        auto* fn = module->getFunction("aurora_list_matmul");
        if (!fn) {
            fn = llvm::Function::Create(
                llvm::FunctionType::get(v, { ptr, ptr, ptr, llvm::Type::getInt32Ty(ctx) }, false),
                llvm::Function::ExternalLinkage, "aurora_list_matmul", module);
        }
        builder.CreateCall(fn, { a_val, b_val, c_val, n_val });
        return llvm::UndefValue::get(v);
    }

    return nullptr;
}