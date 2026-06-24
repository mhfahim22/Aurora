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
   Aurora Runtime — I/O
   ════════════════════════════════════════════════════════════
   Platform-level print functions used by codegen-emitted code.
   These write directly to STDOUT via platform APIs to avoid
   the CRT startup overhead and to stay dependency-free.
   ════════════════════════════════════════════════════════════ */

/* Forward-declare arena allocator from memory.cpp */
extern "C" void* aurora_alloc(size_t size);

extern "C" {

void aurora_print_int(int64_t val) {
    char buf[32];
    int len = 0;
    if (val < 0) {
        buf[len++] = '-';
        if (val == INT64_MIN) {
            buf[len++] = '9';
            int64_t rem = 223372036854775808LL;
            char tmp[20]; int tlen = 0;
            while (rem > 0) { tmp[tlen++] = '0' + (int)(rem % 10); rem /= 10; }
            for (int i = tlen - 1; i >= 0; i--) buf[len++] = tmp[i];
        } else {
            val = -val;
            if (val == 0) { buf[len++] = '0'; }
            else {
                char tmp[20]; int tlen = 0;
                while (val > 0) { tmp[tlen++] = '0' + (int)(val % 10); val /= 10; }
                for (int i = tlen - 1; i >= 0; i--) buf[len++] = tmp[i];
            }
        }
    } else if (val == 0) {
        buf[len++] = '0';
    } else {
        char tmp[20]; int tlen = 0;
        while (val > 0) { tmp[tlen++] = '0' + (int)(val % 10); val /= 10; }
        for (int i = tlen - 1; i >= 0; i--) buf[len++] = tmp[i];
    }
    buf[len++] = '\n';
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;
    WriteFile(hOut, buf, (DWORD)len, &written, nullptr);
#else
    write(STDOUT_FILENO, buf, len);
#endif
}

void aurora_print_float(double val) {
    char buf[64];
    int len = 0;
    if (val < 0) { buf[len++] = '-'; val = -val; }
    int64_t int_part = (int64_t)val;
    double  frac     = val - (double)int_part;
    if (int_part == 0) {
        buf[len++] = '0';
    } else {
        char tmp[20]; int tlen = 0;
        int64_t ip = int_part;
        while (ip > 0) { tmp[tlen++] = '0' + (int)(ip % 10); ip /= 10; }
        for (int i = tlen - 1; i >= 0; i--) buf[len++] = tmp[i];
    }
    buf[len++] = '.';
    for (int i = 0; i < 6; i++) {
        frac *= 10;
        int digit = (int)frac;
        buf[len++] = '0' + digit;
        frac -= digit;
    }
    buf[len++] = '\n';
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;
    WriteFile(hOut, buf, (DWORD)len, &written, nullptr);
#else
    write(STDOUT_FILENO, buf, len);
#endif
}

void aurora_print_str(const char* str) {
    auto* s = reinterpret_cast<const AuroraStr*>(str);
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;
    if (!s || !s->ptr) {
        WriteFile(hOut, "null\n", 5, &written, nullptr);
        return;
    }
    WriteFile(hOut, s->ptr, (DWORD)s->len, &written, nullptr);
    WriteFile(hOut, "\n", 1, &written, nullptr);
#else
    if (!s || !s->ptr) {
        write(STDOUT_FILENO, "null\n", 5);
        return;
    }
    write(STDOUT_FILENO, s->ptr, s->len);
    write(STDOUT_FILENO, "\n", 1);
#endif
}



void aurora_panic(const char* msg) {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;
    const char* prefix = "PANIC: ";
    WriteFile(hOut, prefix, (DWORD)strlen(prefix), &written, nullptr);
    if (msg) {
        int len = 0;
        while (msg[len]) len++;
        WriteFile(hOut, msg, (DWORD)len, &written, nullptr);
    }
    WriteFile(hOut, "\n", 1, &written, nullptr);
    ExitProcess(1);
#else
    const char* prefix = "PANIC: ";
    write(STDERR_FILENO, prefix, strlen(prefix));
    if (msg) {
        write(STDERR_FILENO, msg, strlen(msg));
    }
    write(STDERR_FILENO, "\n", 1);
    _exit(1);
#endif
}

void aurora_print_bool(int64_t val) {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;
    if (val) {
        WriteFile(hOut, "true\n", 5, &written, nullptr);
    } else {
        WriteFile(hOut, "false\n", 6, &written, nullptr);
    }
#else
    if (val) {
        write(STDOUT_FILENO, "true\n", 5);
    } else {
        write(STDOUT_FILENO, "false\n", 6);
    }
#endif
}

/* ── File IO ── */

int64_t aurora_file_read(const char* path, char* buf, int64_t buf_size) {
    if (!path || !buf || buf_size <= 0) return -1;
#ifdef _WIN32
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return -1;
    DWORD bytesRead = 0;
    BOOL ok = ReadFile(hFile, buf, (DWORD)(buf_size - 1), &bytesRead, nullptr);
    CloseHandle(hFile);
    if (!ok) return -1;
    buf[bytesRead] = '\0';
    return (int64_t)bytesRead;
#else
    FILE* f = fopen(path, "r");
    if (!f) return -1;
    size_t read = fread(buf, 1, (size_t)(buf_size - 1), f);
    fclose(f);
    buf[read] = '\0';
    return (int64_t)read;
#endif
}

const char* aurora_file_read_all(const char* path) {
    if (!path) return "";
#ifdef _WIN32
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return "";
    DWORD fileSize = GetFileSize(hFile, nullptr);
    if (fileSize == INVALID_FILE_SIZE) { CloseHandle(hFile); return ""; }
    char* buf = (char*)aurora_alloc(fileSize + 1);
    if (!buf) { CloseHandle(hFile); return ""; }
    DWORD bytesRead = 0;
    ReadFile(hFile, buf, fileSize, &bytesRead, nullptr);
    CloseHandle(hFile);
    buf[bytesRead] = '\0';
    return buf;
#else
    FILE* f = fopen(path, "r");
    if (!f) return "";
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)aurora_alloc(size + 1);
    if (!buf) { fclose(f); return ""; }
    size_t read = fread(buf, 1, size, f);
    fclose(f);
    buf[read] = '\0';
    return buf;
#endif
}

int64_t aurora_file_write(const char* path, const char* content) {
    if (!path || !content) return 0;
#ifdef _WIN32
    HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0,
                               nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return 0;
    int64_t len = 0;
    while (content[len]) len++;
    DWORD written = 0;
    WriteFile(hFile, content, (DWORD)len, &written, nullptr);
    CloseHandle(hFile);
    return (int64_t)written;
#else
    FILE* f = fopen(path, "w");
    if (!f) return 0;
    int64_t len = 0;
    while (content[len]) len++;
    size_t written = fwrite(content, 1, len, f);
    fclose(f);
    return (int64_t)written;
#endif
}

int64_t aurora_file_exists(const char* path) {
    if (!path) return 0;
#ifdef _WIN32
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return 0;
    CloseHandle(hFile);
    return 1;
#else
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    fclose(f);
    return 1;
#endif
}

} /* extern "C" */
