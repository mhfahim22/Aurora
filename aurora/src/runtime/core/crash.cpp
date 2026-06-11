#include "runtime/crash.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

/* ── Windows SEH filter function called on any unhandled exception ── */
static LONG WINAPI aurora_seh_handler(EXCEPTION_POINTERS* ep) {
    static LONG volatile reentry_guard = 0;
    if (InterlockedExchange(&reentry_guard, 1)) return EXCEPTION_CONTINUE_SEARCH;

    char crash_dir[260] = "crash";
    char prog_name[260] = "aurora_program";

    HMODULE hMod = GetModuleHandleA(NULL);
    if (hMod) {
        GetModuleFileNameA(hMod, prog_name, sizeof(prog_name) - 1);
        char* last_slash = strrchr(prog_name, '\\');
        char* last_fwd   = strrchr(prog_name, '/');
        char* base = (last_slash && last_slash > last_fwd) ? last_slash + 1 :
                     (last_fwd  && last_fwd  > last_slash) ? last_fwd  + 1 : prog_name;
        char* dot = strrchr(base, '.');
        if (dot) *dot = '\0';
        if (base != prog_name) memmove(prog_name, base, strlen(base) + 1);
    }

    if (strstr(prog_name, "aurorac") || strstr(prog_name, "voss") ||
        strstr(prog_name, "aurora_bindgen") || strstr(prog_name, "aurora_lsp")) {
        strcpy(crash_dir, "output\\crash");
    }

    uint32_t code = ep ? ep->ExceptionRecord->ExceptionCode : 0;
    void*    addr = ep ? ep->ExceptionRecord->ExceptionAddress : NULL;

    aurora_write_crash_dump(crash_dir, prog_name, code, addr);

    /* Try to write a minidump too */
    char dmp_path[520];
    snprintf(dmp_path, sizeof(dmp_path), "%s\\%s.dmp", crash_dir, prog_name);
    HANDLE hFile = CreateFileA(dmp_path, GENERIC_WRITE, 0, NULL,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        if (ep) {
            MINIDUMP_EXCEPTION_INFORMATION mei;
            mei.ThreadId          = GetCurrentThreadId();
            mei.ExceptionPointers = ep;
            mei.ClientPointers    = FALSE;
            MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                              hFile, MiniDumpNormal, &mei, NULL, NULL);
        } else {
            MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                              hFile, MiniDumpNormal, NULL, NULL, NULL);
        }
        CloseHandle(hFile);
    }

    _exit(code ? (int)code : 1);
    return EXCEPTION_EXECUTE_HANDLER;
}

/* ── CRT-level error callbacks ── */
static void __cdecl aurora_purecall_handler(void) {
    aurora_seh_handler(NULL);
}
static void __cdecl aurora_invalid_param_handler(const wchar_t*,
                                                  const wchar_t*,
                                                  const wchar_t*,
                                                  unsigned, uintptr_t) {
    aurora_seh_handler(NULL);
}

#else
/* ── POSIX signal handlers ── */
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

static void aurora_signal_handler(int sig, siginfo_t* info, void* ucontext) {
    static int volatile reentry_guard = 0;
    if (__sync_lock_test_and_set(&reentry_guard, 1)) _exit(128 + sig);

    uint32_t code = (uint32_t)sig;
    void*    addr = info ? info->si_addr : NULL;

    char prog_name[260] = "aurora_program";
    FILE* f = fopen("/proc/self/comm", "r");
    if (f) {
        size_t n = fread(prog_name, 1, sizeof(prog_name) - 1, f);
        if (n > 0) prog_name[n] = '\0';
        char* nl = (char*)memchr(prog_name, '\n', n);
        if (nl) *nl = '\0';
        fclose(f);
    }

    aurora_write_crash_dump("crash", prog_name, code, addr);
    _exit(128 + sig);
}
#endif

/* ════════════════════════════════════════════════════════════
   Public API
   ════════════════════════════════════════════════════════════ */

void aurora_install_crash_handler(void) {
#ifdef _WIN32
    SetUnhandledExceptionFilter(aurora_seh_handler);
    _set_purecall_handler(aurora_purecall_handler);
    _set_invalid_parameter_handler(aurora_invalid_param_handler);
#else
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = aurora_signal_handler;
    sa.sa_flags     = SA_SIGINFO;

    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGFPE,  &sa, NULL);
    sigaction(SIGILL,  &sa, NULL);
    sigaction(SIGBUS,  &sa, NULL);
#endif
}

int aurora_capture_stack(void** frames, int max_frames) {
#ifdef _WIN32
    return CaptureStackBackTrace(0, max_frames, frames, NULL);
#else
    return backtrace(frames, max_frames);
#endif
}

void aurora_write_crash_dump(const char* crash_dir,
                             const char* crash_name,
                             uint32_t    exception_code,
                             void*       exception_address) {
    char path[520];
    char time_buf[64];

#ifdef _WIN32
    CreateDirectoryA(crash_dir, NULL);
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(time_buf, sizeof(time_buf),
             "%04d-%02d-%02d_%02d-%02d-%02d",
             st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond);
#else
    mkdir(crash_dir, 0755);
    time_t now = time(NULL);
    struct tm tm_now_buf;
    struct tm* tm_now = localtime_r(&now, &tm_now_buf);
    if (!tm_now) tm_now = &tm_now_buf;
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d_%H-%M-%S", tm_now);
#endif

    /* Build report path: <crash_dir>/<name>_<timestamp>.rpt */
    snprintf(path, sizeof(path), "%s/%s_%s.rpt", crash_dir, crash_name, time_buf);
    FILE* fp = fopen(path, "w");
    if (!fp) {
        snprintf(path, sizeof(path), "%s/%s_crash.rpt", crash_dir, crash_name);
        fp = fopen(path, "w");
    }
    if (!fp) return;

    fprintf(fp, "========================================\n");
    fprintf(fp, "  Aurora Crash Dump Report\n");
    fprintf(fp, "========================================\n");
    fprintf(fp, "Timestamp:       %s\n", time_buf);

    if (exception_code) {
        fprintf(fp, "Exception Code:  0x%08X", exception_code);
#ifdef _WIN32
        switch (exception_code) {
            case EXCEPTION_ACCESS_VIOLATION:     fprintf(fp, " (ACCESS_VIOLATION)"); break;
            case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:fprintf(fp, " (ARRAY_BOUNDS_EXCEEDED)"); break;
            case EXCEPTION_BREAKPOINT:           fprintf(fp, " (BREAKPOINT)"); break;
            case EXCEPTION_DATATYPE_MISALIGNMENT:fprintf(fp, " (DATATYPE_MISALIGNMENT)"); break;
            case EXCEPTION_FLT_DIVIDE_BY_ZERO:   fprintf(fp, " (FLT_DIVIDE_BY_ZERO)"); break;
            case EXCEPTION_FLT_INVALID_OPERATION:fprintf(fp, " (FLT_INVALID_OPERATION)"); break;
            case EXCEPTION_FLT_OVERFLOW:         fprintf(fp, " (FLT_OVERFLOW)"); break;
            case EXCEPTION_ILLEGAL_INSTRUCTION:  fprintf(fp, " (ILLEGAL_INSTRUCTION)"); break;
            case EXCEPTION_INT_DIVIDE_BY_ZERO:   fprintf(fp, " (INT_DIVIDE_BY_ZERO)"); break;
            case EXCEPTION_INT_OVERFLOW:         fprintf(fp, " (INT_OVERFLOW)"); break;
            case EXCEPTION_STACK_OVERFLOW:       fprintf(fp, " (STACK_OVERFLOW)"); break;
        }
#endif
        fprintf(fp, "\n");
    }
    if (exception_address) {
        fprintf(fp, "Exception Addr:  0x%p\n", exception_address);
    }

    fprintf(fp, "\n--- Stack Backtrace ---\n");
    void* frames[128];
    int n_frames = aurora_capture_stack(frames, 128);
    for (int i = 0; i < n_frames; i++) {
        fprintf(fp, "  #%02d  0x%p\n", i, frames[i]);
    }

#ifdef _WIN32
    HMODULE hMod = GetModuleHandleA(NULL);
    if (hMod) {
        char mod_path[260];
        GetModuleFileNameA(hMod, mod_path, sizeof(mod_path));
        fprintf(fp, "\n--- Module Info ---\n");
        fprintf(fp, "Executable:      %s\n", mod_path);
        fprintf(fp, "PID:             %lu\n", GetCurrentProcessId());
        fprintf(fp, "Thread ID:       %lu\n", GetCurrentThreadId());
    }
#endif

    fprintf(fp, "\n========================================\n");
    fprintf(fp, "  End of Crash Report\n");
    fprintf(fp, "========================================\n");
    fclose(fp);
}
