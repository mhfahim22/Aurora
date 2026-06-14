#pragma once
// leak_detector.hpp — Per-allocation leak detection with backtrace capture
// Phase 8: Memory Safety

#include <cstdint>
#include <cstddef>

/* Maximum number of backtrace frames captured per allocation */
#define AURORA_LEAK_BACKTRACE_DEPTH 16

/* Maximum number of tracked live allocations */
#define AURORA_LEAK_MAX_RECORDS 1048576

#ifdef __cplusplus
extern "C" {
#endif

/* ════════════════════════════════════════════════════════════
   Leak Detector Public API
   ════════════════════════════════════════════════════════════ */

/* Initialize the leak detector (allocates internal hash table) */
void aurora_leak_init(void);

/* Shutdown the leak detector and free internal resources */
void aurora_leak_shutdown(void);

/* Enable/disable tracking */
void aurora_leak_enable(void);
void aurora_leak_disable(void);
int  aurora_leak_is_enabled(void);

/* Track an allocation */
void aurora_leak_track(void* ptr, size_t size, const char* allocator);

/* Untrack a deallocation */
void aurora_leak_untrack(void* ptr);

/* Print a leak report to stderr — lists all untracked allocations */
void aurora_leak_report(void);

/* Generate leak report as JSON string (caller must free with aurora_free) */
char* aurora_leak_report_json(void);

/* Clear all tracked records without reporting */
void aurora_leak_clear(void);

/* Get number of current (presumed leaked) allocations */
size_t aurora_leak_count(void);

/* Get total bytes currently tracked */
size_t aurora_leak_bytes(void);

/* Get cumulative allocation count since init */
uint64_t aurora_leak_total_allocated(void);

/* Get cumulative free count since init */
uint64_t aurora_leak_total_freed(void);

/* Set minimum allocation size to track (filter out small allocations) */
void aurora_leak_set_min_size(size_t bytes);

/* Set maximum number of records (beyond this, new allocations are not tracked) */
void aurora_leak_set_max_records(size_t max);

/* Register an exit-time leak report (uses atexit) */
void aurora_leak_auto_report(void);

#ifdef __cplusplus
}
#endif
