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
   Uses buffered output for minimal syscall overhead.
   ════════════════════════════════════════════════════════════ */

/* ── Buffered stdout (4KB buffer, flush on \n or full) ── */
#define IO_BUF_SIZE 4096
static char  io_data[IO_BUF_SIZE];
static int   io_pos;

static void io_flush(void) {
    if (io_pos == 0) return;
#ifdef _WIN32
    static HANDLE hOut;
    if (!hOut) hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;
    WriteFile(hOut, io_data, (DWORD)io_pos, &written, nullptr);
#else
    write(STDOUT_FILENO, io_data, io_pos);
#endif
    io_pos = 0;
}

static void io_write(const char* data, int len) {
    for (int i = 0; i < len; ) {
        int avail = IO_BUF_SIZE - io_pos;
        if (avail == 0) { io_flush(); avail = IO_BUF_SIZE; }
        int chunk = len - i;
        if (chunk > avail) chunk = avail;
        memcpy(io_data + io_pos, data + i, chunk);
        io_pos += chunk;
        i += chunk;
    }
}

static void io_write_char(char c) {
    if (io_pos >= IO_BUF_SIZE) io_flush();
    io_data[io_pos++] = c;
    if (c == '\n') io_flush();
}

/* Fast integer-to-string: write directly to buffer, right-to-left */
static void io_write_int(int64_t val) {
    if (val == INT64_MIN) {
        io_write("-9223372036854775808", 20);
        return;
    }
    char buf[24];
    int pos = 24;
    bool neg = false;
    if (val < 0) { neg = true; val = -val; }
    do {
        buf[--pos] = '0' + (int)(val % 10);
        val /= 10;
    } while (val > 0);
    if (neg) buf[--pos] = '-';
    io_write(buf + pos, 24 - pos);
}

/* Forward-declare arena allocator from memory.cpp */
extern "C" void* aurora_alloc(size_t size);

extern "C" {

void aurora_print_int(int64_t val) {
    io_write_int(val);
    io_write_char('\n');
}

void aurora_print_float(double val) {
    if (val < 0) { io_write_char('-'); val = -val; }
    int64_t int_part = (int64_t)val;
    double  frac     = val - (double)int_part;
    io_write_int(int_part);
    io_write_char('.');
    for (int i = 0; i < 6; i++) {
        frac *= 10;
        int digit = (int)frac;
        io_write_char('0' + digit);
        frac -= digit;
    }
    io_write_char('\n');
}

void aurora_print_str(const char* str) {
    auto* s = reinterpret_cast<const AuroraStr*>(str);
    if (!s || !s->ptr) { io_write("null\n", 5); return; }
    io_write(s->ptr, (int)s->len);
    io_write_char('\n');
}

void aurora_print_bool(int64_t val) {
    if (val) io_write("true\n", 5);
    else     io_write("false\n", 6);
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
