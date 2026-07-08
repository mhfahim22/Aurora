#pragma once
/* ════════════════════════════════════════════════════════════
   Aurora Codegen — Built-in Functions (Declarations)
   ════════════════════════════════════════════════════════════
   Central registration point for all built-in function codegen.
   To add a new built-in:
     1. Add runtime impl in aurora/src/runtime/builtins/
     2. Add declaration here
     3. Add codegen handler in codegen_builtins.cpp
   ════════════════════════════════════════════════════════════ */

#include <llvm/IR/Function.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <string>

/* ── Built-in function LLVM handles ── */
struct BuiltinFunctions {
    /* Math/Array builtins */
    llvm::Function* len     { nullptr };
    llvm::Function* strlen_fn { nullptr };
    llvm::Function* sum     { nullptr };
    llvm::Function* min     { nullptr };
    llvm::Function* max     { nullptr };
    llvm::Function* range   { nullptr };

    /* I/O builtins */
    llvm::Function* outputln_int    { nullptr };
    llvm::Function* outputln_float  { nullptr };
    llvm::Function* outputln_str    { nullptr };
    llvm::Function* outputln_bool   { nullptr };
    llvm::Function* outputN         { nullptr };
    llvm::Function* outputf         { nullptr };
    llvm::Function* input           { nullptr };

    /* String builtins */
    llvm::Function* upper           { nullptr };
    llvm::Function* lower           { nullptr };
    llvm::Function* trim            { nullptr };
    llvm::Function* replace_fn      { nullptr };
    llvm::Function* split_fn        { nullptr };
    llvm::Function* join_fn         { nullptr };
    llvm::Function* has_str         { nullptr };
    llvm::Function* starts          { nullptr };
    llvm::Function* ends            { nullptr };
    llvm::Function* reverse_str     { nullptr };

    /* Math builtins */
    llvm::Function* sqrt_fn         { nullptr };
    llvm::Function* abs_fn          { nullptr };
    llvm::Function* floor_fn        { nullptr };
    llvm::Function* ceil_fn         { nullptr };
    llvm::Function* round_fn        { nullptr };
    llvm::Function* pow_fn          { nullptr };
    llvm::Function* clamp_fn        { nullptr };
    llvm::Function* min2            { nullptr };
    llvm::Function* max2            { nullptr };
    llvm::Function* rand_fn         { nullptr };

    /* File builtins */
    llvm::Function* read_file       { nullptr };
    llvm::Function* write_file      { nullptr };
    llvm::Function* append_file     { nullptr };
    llvm::Function* file_exists     { nullptr };
    llvm::Function* delete_file     { nullptr };
    llvm::Function* copy_file       { nullptr };
    llvm::Function* move_file       { nullptr };

    /* Path builtins */
    llvm::Function* cwd_fn          { nullptr };
    llvm::Function* cd_fn           { nullptr };
    llvm::Function* dirname_fn      { nullptr };
    llvm::Function* basename_fn     { nullptr };
    llvm::Function* ext_fn          { nullptr };

    /* Time builtins */
    llvm::Function* now_fn          { nullptr };
    llvm::Function* stamp_fn        { nullptr };
    llvm::Function* sleep_fn        { nullptr };

    /* OS builtins */
    llvm::Function* os_fn           { nullptr };
    llvm::Function* cpu_fn          { nullptr };
    llvm::Function* mem_fn          { nullptr };
    llvm::Function* env_fn          { nullptr };
    llvm::Function* run_fn          { nullptr };
    llvm::Function* exit_fn         { nullptr };

    /* JSON builtins */
    llvm::Function* encode_json     { nullptr };
    llvm::Function* decode_json     { nullptr };

    /* HTTP builtins */
    llvm::Function* http_get        { nullptr };
    llvm::Function* http_post       { nullptr };

    /* Collection builtins */
    llvm::Function* push_fn         { nullptr };
    llvm::Function* push_str_fn     { nullptr };
    llvm::Function* pop_fn          { nullptr };
    llvm::Function* insert_fn       { nullptr };
    llvm::Function* remove_fn       { nullptr };
    llvm::Function* clear_fn        { nullptr };
    llvm::Function* has_arr         { nullptr };
    llvm::Function* sort_fn         { nullptr };
    llvm::Function* reverse_arr     { nullptr };
    llvm::Function* unique_fn       { nullptr };

    /* Error/Confirm builtins */
    llvm::Function* error_fn        { nullptr };
    llvm::Function* ask_fn          { nullptr };

    /* Type conversion */
    llvm::Function* char_fn         { nullptr };

    /* Async builtins */
    llvm::Function* spawn_fn        { nullptr };
    llvm::Function* await_fn        { nullptr };
    llvm::Function* chan_fn         { nullptr };
    llvm::Function* send_fn         { nullptr };
    llvm::Function* recv_fn         { nullptr };

    /* Performance builtins */
    llvm::Function* measure_fn      { nullptr };
    llvm::Function* bench_fn        { nullptr };
    llvm::Function* profile_fn      { nullptr };
    llvm::Function* trace_fn        { nullptr };

    /* Reflection builtins */
    llvm::Function* fields_fn       { nullptr };
    llvm::Function* methods_fn      { nullptr };

    /* Package builtins */
    llvm::Function* install_fn      { nullptr };
    llvm::Function* update_fn       { nullptr };
    llvm::Function* search_fn       { nullptr };

    /* ── Backend / Server builtins ── */
    /* Route/Middleware */
    llvm::Function* route_group_fn      { nullptr };
    llvm::Function* middleware_fn       { nullptr };
    llvm::Function* next_fn             { nullptr };
    llvm::Function* rate_limit_fn       { nullptr };
    llvm::Function* cors_fn             { nullptr };
    llvm::Function* csrf_fn             { nullptr };

    /* Session */
    llvm::Function* session_fn          { nullptr };
    llvm::Function* session_get_fn      { nullptr };
    llvm::Function* session_set_fn      { nullptr };
    llvm::Function* session_delete_fn   { nullptr };

    /* Cookie */
    llvm::Function* cookie_get_fn       { nullptr };
    llvm::Function* cookie_set_fn       { nullptr };
    llvm::Function* cookie_delete_fn    { nullptr };

    /* Proxy/Streaming */
    llvm::Function* proxy_fn            { nullptr };
    llvm::Function* reverse_proxy_fn    { nullptr };
    llvm::Function* stream_fn           { nullptr };
    llvm::Function* stream_file_fn      { nullptr };
    llvm::Function* sse_fn              { nullptr };
    llvm::Function* webhook_fn          { nullptr };

    /* Health/Metrics */
    llvm::Function* health_fn           { nullptr };
    llvm::Function* metrics_fn          { nullptr };
    llvm::Function* trace_id_fn         { nullptr };
    llvm::Function* request_id_fn       { nullptr };
    llvm::Function* audit_fn            { nullptr };

    /* Lock/Sync */
    llvm::Function* lock_fn             { nullptr };
    llvm::Function* unlock_fn           { nullptr };
    llvm::Function* atomic_fn           { nullptr };
    llvm::Function* retry_fn            { nullptr };
    llvm::Function* timeout_fn          { nullptr };
    llvm::Function* circuit_breaker_fn  { nullptr };

    /* Pool */
    llvm::Function* pool_fn             { nullptr };
    llvm::Function* worker_pool_fn      { nullptr };
    llvm::Function* batch_fn            { nullptr };
    llvm::Function* paginate_fn         { nullptr };

    /* DB/ORM */
    llvm::Function* index_fn            { nullptr };
    llvm::Function* migrate_fn          { nullptr };
    llvm::Function* seed_fn             { nullptr };
    llvm::Function* model_fn            { nullptr };
    llvm::Function* schema_fn           { nullptr };
    llvm::Function* validate_fn         { nullptr };
    llvm::Function* sanitize_fn         { nullptr };

    /* Template */
    llvm::Function* template_fn          { nullptr };
    llvm::Function* render_fn            { nullptr };

    /* Throttle/Debounce */
    llvm::Function* throttle_fn         { nullptr };
    llvm::Function* debounce_fn         { nullptr };

    /* Crypto */
    llvm::Function* sign_fn             { nullptr };
    llvm::Function* verify_fn           { nullptr };
    llvm::Function* secret_fn           { nullptr };
    llvm::Function* vault_fn            { nullptr };

    /* Compress/Serialize */
    llvm::Function* compress_fn         { nullptr };
    llvm::Function* decompress_fn       { nullptr };
    llvm::Function* serialize_fn        { nullptr };
    llvm::Function* deserialize_fn      { nullptr };

    /* Fiber builtins */
    llvm::Function* fiber_create_fn     { nullptr };
    llvm::Function* fiber_resume_fn     { nullptr };
    llvm::Function* fiber_yield_fn      { nullptr };
    llvm::Function* fiber_is_done_fn    { nullptr };
    llvm::Function* fiber_get_result_fn { nullptr };
    llvm::Function* fiber_destroy_fn    { nullptr };

    /* Event bus builtins */
    llvm::Function* event_on_fn         { nullptr };
    llvm::Function* event_off_fn        { nullptr };
    llvm::Function* event_emit_fn       { nullptr };

    /* Event/PubSub */
    llvm::Function* event_fn            { nullptr };
    llvm::Function* emit_fn             { nullptr };
    llvm::Function* listen_fn           { nullptr };
    llvm::Function* publish_fn          { nullptr };
    llvm::Function* subscribe_fn        { nullptr };

    /* RPC/Cluster */
    llvm::Function* rpc_fn              { nullptr };
    llvm::Function* discover_fn         { nullptr };
    llvm::Function* cluster_fn          { nullptr };
    llvm::Function* node_id_fn          { nullptr };
    llvm::Function* leader_fn           { nullptr };
    llvm::Function* shard_fn            { nullptr };
    llvm::Function* replica_fn          { nullptr };

    /* Backup */
    llvm::Function* backup_fn           { nullptr };
    llvm::Function* restore_fn          { nullptr };

    /* Monitor/Profile */
    llvm::Function* monitor_fn          { nullptr };
    llvm::Function* profile_request_fn  { nullptr };
    llvm::Function* memory_snapshot_fn  { nullptr };
    llvm::Function* gc_collect_fn       { nullptr };
    llvm::Function* hot_reload_fn       { nullptr };

    /* Plugin/FeatureFlag */
    llvm::Function* plugin_fn           { nullptr };
    llvm::Function* feature_flag_fn     { nullptr };

    /* Tenant */
    llvm::Function* tenant_fn           { nullptr };
    llvm::Function* tenant_context_fn   { nullptr };

    /* Geo/Captcha */
    llvm::Function* geoip_fn            { nullptr };
    llvm::Function* captcha_verify_fn   { nullptr };

    /* Payment/Analytics */
    llvm::Function* payment_fn          { nullptr };
    llvm::Function* invoice_fn          { nullptr };
    llvm::Function* analytics_fn        { nullptr };

    /* Search/AI extended */
    llvm::Function* search_engine_fn    { nullptr };
    llvm::Function* vector_search_fn    { nullptr };
    llvm::Function* semantic_search_fn  { nullptr };
    llvm::Function* embed_store_fn      { nullptr };
    llvm::Function* embed_query_fn      { nullptr };
    llvm::Function* ai_agent_fn         { nullptr };
    llvm::Function* tool_fn             { nullptr };
    llvm::Function* workflow_fn         { nullptr };
    llvm::Function* pipeline_fn         { nullptr };
    llvm::Function* step_fn             { nullptr };

    /* ── AI/ML functional builtins ── */
    llvm::Function* csv_fn              { nullptr };
    llvm::Function* data_fn             { nullptr };
    llvm::Function* clean_fn            { nullptr };
    llvm::Function* normalize_fn        { nullptr };
    llvm::Function* standard_fn         { nullptr };
    llvm::Function* shuffle_fn          { nullptr };
    llvm::Function* split_data_fn       { nullptr };
    llvm::Function* model_create_fn     { nullptr };
    llvm::Function* model_save_fn       { nullptr };
    llvm::Function* model_load_fn       { nullptr };
    llvm::Function* set_loss_fn         { nullptr };
    llvm::Function* set_optimizer_fn    { nullptr };
    llvm::Function* set_lr_fn           { nullptr };
    llvm::Function* set_batch_size_fn   { nullptr };
    llvm::Function* set_epochs_fn       { nullptr };
    llvm::Function* set_validation_split_fn { nullptr };
    llvm::Function* set_verbose_fn      { nullptr };
    llvm::Function* set_early_stop_fn   { nullptr };
    llvm::Function* dense_fn            { nullptr };
    llvm::Function* conv_fn             { nullptr };
    llvm::Function* lstm_fn             { nullptr };
    llvm::Function* gru_fn              { nullptr };
    llvm::Function* dropout_fn          { nullptr };
    llvm::Function* batchnorm_fn        { nullptr };
    llvm::Function* attention_fn        { nullptr };
    llvm::Function* transformer_fn      { nullptr };
    llvm::Function* embedding_fn        { nullptr };
    llvm::Function* layernorm_fn        { nullptr };
    llvm::Function* add_fn              { nullptr };
    llvm::Function* train_fn            { nullptr };
    llvm::Function* fit_fn              { nullptr };
    llvm::Function* test_fn             { nullptr };
    llvm::Function* predict_fn          { nullptr };
    llvm::Function* retrain_fn          { nullptr };
};

/* ── Register all built-in functions in the LLVM module ── */
void register_builtins(llvm::Module* module, llvm::LLVMContext& ctx,
                       BuiltinFunctions& builtins);

/* ── Try to codegen a built-in call. Returns nullptr if not a built-in. ── */
llvm::Value* codegen_builtin_call(
    const std::string& name,
    const struct ASTNode* node,
    llvm::IRBuilder<>& builder,
    llvm::LLVMContext& ctx,
    const BuiltinFunctions& builtins,
    std::function<llvm::Value*(const ASTNode*)> gen_expr,
    llvm::Module* module = nullptr);

/* ── Section dispatch functions ── */
llvm::Value* codegen_builtin_section1(
    const std::string& name, const struct ASTNode* node,
    llvm::IRBuilder<>& builder, llvm::LLVMContext& ctx,
    const BuiltinFunctions& builtins,
    std::function<llvm::Value*(const ASTNode*)> gen_expr,
    llvm::Module* module);

llvm::Value* codegen_builtin_section2(
    const std::string& name, const struct ASTNode* node,
    llvm::IRBuilder<>& builder, llvm::LLVMContext& ctx,
    const BuiltinFunctions& builtins,
    std::function<llvm::Value*(const ASTNode*)> gen_expr,
    llvm::Module* module);

llvm::Value* codegen_builtin_section3(
    const std::string& name, const struct ASTNode* node,
    llvm::IRBuilder<>& builder, llvm::LLVMContext& ctx,
    const BuiltinFunctions& builtins,
    std::function<llvm::Value*(const ASTNode*)> gen_expr,
    llvm::Module* module);

llvm::Value* codegen_builtin_section4(
    const std::string& name, const struct ASTNode* node,
    llvm::IRBuilder<>& builder, llvm::LLVMContext& ctx,
    const BuiltinFunctions& builtins,
    std::function<llvm::Value*(const ASTNode*)> gen_expr,
    llvm::Module* module);

llvm::Value* codegen_builtin_section5(
    const std::string& name, const struct ASTNode* node,
    llvm::IRBuilder<>& builder, llvm::LLVMContext& ctx,
    const BuiltinFunctions& builtins,
    std::function<llvm::Value*(const ASTNode*)> gen_expr,
    llvm::Module* module);
