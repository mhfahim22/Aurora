#ifndef AURORA_CRASH_H
#define AURORA_CRASH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Install the crash handler (SetUnhandledExceptionFilter on Windows,
   signal handlers on Unix). Safe to call multiple times. ── */
void aurora_install_crash_handler(void);

/* ── Generate a crash dump file manually (can be called from aurora_panic).
   `crash_dir` – directory to write dump into (e.g. "crash" or "output/crash").
   `crash_name` – base name for the dump file (e.g. program name).
   `exception_code` – Windows exception code or 0 for non-SEH crash.
   `exception_address` – address of the faulting instruction or NULL.      ── */
void aurora_write_crash_dump(const char* crash_dir,
                             const char* crash_name,
                             uint32_t    exception_code,
                             void*       exception_address);

/* ── Retrieve the top N stack return addresses (for use in dump). ── */
int aurora_capture_stack(void** frames, int max_frames);

#ifdef __cplusplus
}
#endif

#endif /* AURORA_CRASH_H */
