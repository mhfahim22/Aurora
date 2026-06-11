#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "runtime/string.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

/* ════════════════════════════════════════════════════════════
   Aurora Runtime — I/O Built-in Functions
   ════════════════════════════════════════════════════════════
   Extended I/O builtins beyond basic output().
   Separated from io.cpp for maintainability.
   ════════════════════════════════════════════════════════════ */

/* Forward declarations */
extern "C" {
    void aurora_print_int(int64_t val);
    void aurora_print_float(double val);
    void aurora_print_str(const char* str);
    void aurora_print_bool(int64_t val);
    void* aurora_alloc(size_t size);
}

extern "C" {

/* ── outputln(value) — print without newline ── */
void aurora_outputln_int(int64_t val) {
    char buf[32];
    int len = 0;
    if (val < 0) { buf[len++] = '-'; val = -val; }
    if (val == 0) { buf[len++] = '0'; }
    else {
        char tmp[20]; int tl = 0;
        while (val > 0) { tmp[tl++] = '0' + (int)(val % 10); val /= 10; }
        for (int i = tl - 1; i >= 0; i--) buf[len++] = tmp[i];
    }
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;
    WriteFile(hOut, buf, len, &written, nullptr);
#else
    write(STDOUT_FILENO, buf, len);
#endif
}

void aurora_outputln_float(double val) {
    char buf[64];
    int len = 0;
    if (val < 0) { buf[len++] = '-'; val = -val; }
    int64_t ip = (int64_t)val;
    double fr = val - (double)ip;
    if (ip == 0) { buf[len++] = '0'; }
    else {
        char tmp[20]; int tl = 0;
        while (ip > 0) { tmp[tl++] = '0' + (int)(ip % 10); ip /= 10; }
        for (int i = tl - 1; i >= 0; i--) buf[len++] = tmp[i];
    }
    buf[len++] = '.';
    for (int i = 0; i < 6; i++) {
        fr *= 10; int d = (int)fr; buf[len++] = '0' + d; fr -= d;
    }
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;
    WriteFile(hOut, buf, len, &written, nullptr);
#else
    write(STDOUT_FILENO, buf, len);
#endif
}

void aurora_outputln_str(const char* str) {
    AuroraStr* s = (AuroraStr*)str;
    if (!s || !s->ptr) return;
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;
    WriteFile(hOut, s->ptr, (DWORD)s->len, &written, nullptr);
#else
    write(STDOUT_FILENO, s->ptr, s->len);
#endif
}

void aurora_outputln_bool(int64_t val) {
    if (val) {
        AuroraStr* t = aurora_str_from_cstr("true");
        aurora_outputln_str((const char*)t);
    } else {
        AuroraStr* f = aurora_str_from_cstr("false");
        aurora_outputln_str((const char*)f);
    }
}

/* ── outputN() — just print a newline ── */
void aurora_outputN() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;
    WriteFile(hOut, "\n", 1, &written, nullptr);
#else
    write(STDOUT_FILENO, "\n", 1);
#endif
}

/* ── outputf(fmt, ...) — formatted output ──
   Supports: %d (int), %f (float), %s (string), %b (bool)
   Limited to 8 arguments for simplicity. */
void aurora_outputf(const char* fmt, int64_t argc, int64_t* argv) {
    AuroraStr* fmt_s = (AuroraStr*)fmt;
    if (!fmt_s || !fmt_s->ptr) return;

    int arg_idx = 0;
    const char* p = fmt_s->ptr;
    const char* end = fmt_s->ptr + fmt_s->len;

#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;

    while (p < end) {
        if (*p == '%' && (p + 1 < end)) {
            p++;
            char spec = *p;
            if (spec == 'd' && arg_idx < argc) {
                /* Print integer */
                char buf[32]; int len = 0;
                int64_t val = argv[arg_idx++];
                if (val < 0) { buf[len++] = '-'; val = -val; }
                if (val == 0) { buf[len++] = '0'; }
                else {
                    char tmp[20]; int tl = 0;
                    while (val > 0) { tmp[tl++] = '0' + (int)(val % 10); val /= 10; }
                    for (int i = tl - 1; i >= 0; i--) buf[len++] = tmp[i];
                }
                WriteFile(hOut, buf, len, &written, nullptr);
            } else if (spec == 's' && arg_idx < argc) {
                /* Print string (AuroraStr*) */
                AuroraStr* s = (AuroraStr*)(uintptr_t)argv[arg_idx++];
                if (s && s->ptr) {
                    WriteFile(hOut, s->ptr, (DWORD)s->len, &written, nullptr);
                }
            } else if (spec == 'b' && arg_idx < argc) {
                /* Print bool */
                int64_t val = argv[arg_idx++];
                if (val) WriteFile(hOut, "true", 4, &written, nullptr);
                else     WriteFile(hOut, "false", 5, &written, nullptr);
            } else if (spec == '%') {
                WriteFile(hOut, "%", 1, &written, nullptr);
            } else {
                /* Unknown spec, print as-is */
                WriteFile(hOut, p - 1, 2, &written, nullptr);
            }
        } else {
            WriteFile(hOut, p, 1, &written, nullptr);
        }
        p++;
    }
#else
    while (p < end) {
        if (*p == '%' && (p + 1 < end)) {
            p++;
            char spec = *p;
            if (spec == 'd' && arg_idx < argc) {
                printf("%lld", (long long)argv[arg_idx++]);
            } else if (spec == 's' && arg_idx < argc) {
                AuroraStr* s = (AuroraStr*)(uintptr_t)argv[arg_idx++];
                if (s && s->ptr) printf("%.*s", (int)s->len, s->ptr);
            } else if (spec == 'b' && arg_idx < argc) {
                printf("%s", argv[arg_idx++] ? "true" : "false");
            } else if (spec == '%') {
                putchar('%');
            } else {
                putchar('%');
                putchar(spec);
            }
        } else {
            putchar(*p);
        }
        p++;
    }
#endif
}

/* ── input() — read a line from stdin, return AuroraStr* ── */
const char* aurora_input() {
    static char buf[4096];
#ifdef _WIN32
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD read = 0;
    if (!ReadFile(hIn, buf, sizeof(buf) - 1, &read, nullptr)) {
        buf[0] = '\0';
    } else {
        buf[read] = '\0';
        while (read > 0 && (buf[read-1] == '\n' || buf[read-1] == '\r'))
            buf[--read] = '\0';
    }
#else
    if (!fgets(buf, sizeof(buf), stdin)) {
        buf[0] = '\0';
    } else {
        int len = strlen(buf);
        while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r'))
            buf[--len] = '\0';
    }
#endif
    size_t len = strlen(buf);
    AuroraStr* s = aurora_str_new(len);
    if (len > 0) {
        memcpy(s->ptr, buf, len);
    }
    s->ptr[len] = '\0';
    return (const char*)s;
}

/* ── print() — alias for output() ── */
void aurora_print_value(int64_t val) {
    aurora_print_int(val);
}

void aurora_print_value_f(double val) {
    aurora_print_float(val);
}

void aurora_print_value_s(const char* val) {
    aurora_print_str(val);
}

} /* extern "C" */
