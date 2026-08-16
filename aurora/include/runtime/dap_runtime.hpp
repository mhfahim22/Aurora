#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/* ════════════════════════════════════════════════════════════
   DAP (Debug Adapter Protocol) runtime hooks
   Compiled into aurora_runtime and resolved by the JIT / linker
   when a program is compiled with `--dap`.

   Control channel (file-based, set via env AURORA_DAP_CTRL):
     <ctrl>/breakpoints.txt   breakpoint line set (one per line)
     <ctrl>/events.jsonl      program → DAP server event log (append)
     <ctrl>/cmd.txt           DAP server → program command file
     <ctrl>/state.txt         current function stack + line (latest hit)
   ════════════════════════════════════════════════════════════ */

/* Prepare the debug session. Returns 1 when a session is active. */
int aurora_dap_init(void);

/* Called at every statement boundary. If `line` is a breakpoint or a
   step/pause target, reports a `stopped` event and blocks until a
   command (continue/next/step/exit) is written to cmd.txt. */
void aurora_dap_trap(int64_t line);

/* Function entry/exit for call-stack tracking. */
void aurora_dap_enter(const char* fn);
void aurora_dap_exit(void);

/* Register a variable value into the debug scope (name must be stable). */
void aurora_dap_var(const char* name, double value);

/* Report program output to the DAP server's console. */
void aurora_dap_print(const char* text);

#ifdef __cplusplus
}
#endif