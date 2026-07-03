#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Dev Server (1) — re-export from backend ── */
void aurora_dev_server(int64_t port, const char* src_dir);

/* ── Formatting (4) ── */
const char* aurora_dev_format(const char* code);
int         aurora_dev_format_file(const char* path);
void        aurora_dev_format_set_tab_size(int n);
void        aurora_dev_format_set_spaces(int use_spaces);

/* ── Linting (3) ── */
const char* aurora_dev_lint(const char* code);
const char* aurora_dev_lint_file(const char* path);
int         aurora_dev_lint_set_rule(const char* rule, int enabled);

/* ── LSP (4) ── */
int         aurora_dev_lsp_start(int port);
int         aurora_dev_lsp_stop(void);
int         aurora_dev_lsp_is_running(void);
int         aurora_dev_lsp_set_root(const char* path);

/* ── Completions (3) ── */
const char* aurora_dev_complete(const char* code, int line, int col);
const char* aurora_dev_complete_file(const char* path, int line, int col);
const char* aurora_dev_complete_detail(const char* label);

/* ── Debugger (4) ── */
int         aurora_dev_debug_attach(const char* target);
int         aurora_dev_debug_break(void);
int         aurora_dev_debug_continue(void);
int         aurora_dev_debug_step_over(void);

/* ── Profiler (4) ── */
int         aurora_dev_profiler_start(void);
int         aurora_dev_profiler_stop(void);
const char* aurora_dev_profiler_report(void);
void        aurora_dev_profiler_reset(void);

/* ── Inspector (3) ── */
const char* aurora_dev_inspector_tree(void);
const char* aurora_dev_inspector_select(int x, int y);
void        aurora_dev_inspector_refresh(void);

/* ── Memory Viewer (3) ── */
const char* aurora_dev_memory_stats(void);
const char* aurora_dev_memory_snapshot(void);
int         aurora_dev_memory_leak_check(void);

/* ── Performance Monitor (5) ── */
int         aurora_dev_perf_start(void);
int         aurora_dev_perf_stop(void);
double      aurora_dev_perf_fps(void);
double      aurora_dev_perf_frame_time(void);
const char* aurora_dev_perf_report(void);

#ifdef __cplusplus
}
#endif
