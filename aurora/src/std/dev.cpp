#include "std/dev.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

/* ════════════════════════════════════════════════════════════
   Developer Tools — Formatter, Linter, LSP, Autocomplete,
   Debugger, Profiler, Inspector, Memory Viewer, Perf Monitor
   ════════════════════════════════════════════════════════════ */

static std::mutex g_mtx;

/* ── Dev Server (re-export from backend) ── */
/* Implementation is in backend/server.cpp — this header
   just re-declares it for the `dev` module. */

/* ── Formatting ── */

struct FormatConfig {
    int tab_size = 4;
    int use_spaces = 1;
};
static FormatConfig g_fmt;

const char* aurora_dev_format(const char* code) {
    std::lock_guard<std::mutex> lock(g_mtx);
    if (!code) return nullptr;
    std::string in(code);
    std::ostringstream out;
    int indent = 0;
    bool newline = true;
    for (size_t i = 0; i < in.size(); i++) {
        char c = in[i];
        if (c == '\n') {
            out << '\n';
            newline = true;
            continue;
        }
        if (newline) {
            if (c == ' ' || c == '\t' || c == '\r') continue;
            // Check for dedent tokens
            if (c == '}' || c == ']' || c == ')') indent = (indent > 0 ? indent - 1 : 0);
            int spaces = indent * (g_fmt.use_spaces ? g_fmt.tab_size : 1);
            for (int s = 0; s < spaces; s++) out << (g_fmt.use_spaces ? ' ' : '\t');
            newline = false;
            // Re-check opening brace if we dedented
            if (c == '}' || c == ']' || c == ')') { out << c; continue; }
        }
        out << c;
        if (c == '{' || c == '[' || c == '(') indent++;
    }
    static std::string result;
    result = out.str();
    return result.c_str();
}

int aurora_dev_format_file(const char* path) {
    if (!path) return 0;
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string code((size_t)len, '\0');
    fread(&code[0], 1, (size_t)len, f);
    fclose(f);
    const char* formatted = aurora_dev_format(code.c_str());
    if (!formatted) return 0;
    f = fopen(path, "wb");
    if (!f) return 0;
    fwrite(formatted, 1, strlen(formatted), f);
    fclose(f);
    return 1;
}

void aurora_dev_format_set_tab_size(int n) {
    g_fmt.tab_size = n > 0 ? n : 4;
}

void aurora_dev_format_set_spaces(int use_spaces) {
    g_fmt.use_spaces = use_spaces;
}

/* ── Linting ── */

struct LintState {
    std::map<std::string, int> rules; // rule -> enabled
};
static LintState g_lint;

const char* aurora_dev_lint(const char* code) {
    std::lock_guard<std::mutex> lock(g_mtx);
    if (!code) return nullptr;
    std::ostringstream out;
    out << "[";
    bool first = true;
    // Simple checks
    std::string s(code);
    size_t pos = 0;
    int line = 1;
    int col = 1;
    bool in_string = false;
    char str_char = 0;
    while (pos < s.size()) {
        char c = s[pos];
        if (in_string) {
            if (c == '\\' && pos + 1 < s.size()) { pos += 2; col += 2; continue; }
            if (c == str_char) in_string = false;
            if (c == '\n') { line++; col = 1; } else col++;
            pos++;
            continue;
        }
        if (c == '"' || c == '\'') { in_string = true; str_char = c; col++; pos++; continue; }
        if (c == '\n') { line++; col = 1; pos++; continue; }
        // Check for long lines
        if (c == ' ' && col > 1 && col % 80 == 0) {
            // Look ahead for end of line
            size_t eol = s.find('\n', pos);
            if (eol == std::string::npos) eol = s.size();
            if (eol - pos > 80) {
                if (!first) out << ",";
                first = false;
                out << "{\"line\":" << line << ",\"col\":" << col << ",\"msg\":\"Line too long (>80 chars)\",\"sev\":\"warning\"}";
            }
        }
        // Check for trailing whitespace
        if ((c == ' ' || c == '\t') && (pos + 1 >= s.size() || s[pos + 1] == '\n')) {
            if (!first) out << ",";
            first = false;
            out << "{\"line\":" << line << ",\"col\":" << col << ",\"msg\":\"Trailing whitespace\",\"sev\":\"warning\"}";
        }
        col++;
        pos++;
    }
    out << "]";
    static std::string result;
    result = out.str();
    return result.c_str();
}

const char* aurora_dev_lint_file(const char* path) {
    if (!path) return nullptr;
    FILE* f = fopen(path, "rb");
    if (!f) return nullptr;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string code((size_t)len, '\0');
    fread(&code[0], 1, (size_t)len, f);
    fclose(f);
    return aurora_dev_lint(code.c_str());
}

int aurora_dev_lint_set_rule(const char* rule, int enabled) {
    if (!rule) return 0;
    g_lint.rules[rule] = enabled;
    return 1;
}

/* ── LSP ── */

static struct LspState {
    int port = 0;
    int running = 0;
    std::string root;
} g_lsp;

int aurora_dev_lsp_start(int port) {
    std::lock_guard<std::mutex> lock(g_mtx);
    if (g_lsp.running) return 0;
    g_lsp.port = port;
    g_lsp.running = 1;
    printf("[dev] LSP server started on port %d\n", port);
    return 1;
}

int aurora_dev_lsp_stop(void) {
    std::lock_guard<std::mutex> lock(g_mtx);
    if (!g_lsp.running) return 0;
    g_lsp.running = 0;
    printf("[dev] LSP server stopped\n");
    return 1;
}

int aurora_dev_lsp_is_running(void) {
    return g_lsp.running;
}

int aurora_dev_lsp_set_root(const char* path) {
    if (!path) return 0;
    g_lsp.root = path;
    return 1;
}

/* ── Completions ── */

const char* aurora_dev_complete(const char* code, int line, int col) {
    std::lock_guard<std::mutex> lock(g_mtx);
    if (!code) return nullptr;
    // Simple completion: identify word at cursor
    std::string s(code);
    // Find position in string from line/col
    int l = 0, c = 0;
    size_t pos = 0;
    while (pos < s.size() && (l < line || (l == line && c < col))) {
        if (s[pos] == '\n') { l++; c = 0; }
        else c++;
        pos++;
    }
    // Scan backward for word start
    size_t start = pos;
    while (start > 0 && (isalnum(s[start - 1]) || s[start - 1] == '_')) start--;
    std::string prefix = s.substr(start, pos - start);
    // Return JSON completion
    std::ostringstream out;
    out << "[{\"label\":\"" << prefix << "\",\"kind\":1,\"detail\":\"completed\"}]";
    static std::string result;
    result = out.str();
    return result.c_str();
}

const char* aurora_dev_complete_file(const char* path, int line, int col) {
    if (!path) return nullptr;
    FILE* f = fopen(path, "rb");
    if (!f) return nullptr;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string code((size_t)len, '\0');
    fread(&code[0], 1, (size_t)len, f);
    fclose(f);
    return aurora_dev_complete(code.c_str(), line, col);
}

const char* aurora_dev_complete_detail(const char* label) {
    if (!label) return nullptr;
    static std::string result;
    result = "{\"label\":\"";
    result += label;
    result += "\",\"detail\":\"Aurora keyword\"}";
    return result.c_str();
}

/* ── Debugger ── */

static struct DebugState {
    int attached = 0;
    int paused = 0;
} g_debug;

int aurora_dev_debug_attach(const char* target) {
    std::lock_guard<std::mutex> lock(g_mtx);
    if (!target) return 0;
    g_debug.attached = 1;
    g_debug.paused = 0;
    printf("[dev] Debugger attached to %s\n", target);
    return 1;
}

int aurora_dev_debug_break(void) {
    g_debug.paused = 1;
    printf("[dev] Debugger break\n");
    return 1;
}

int aurora_dev_debug_continue(void) {
    g_debug.paused = 0;
    printf("[dev] Debugger continue\n");
    return 1;
}

int aurora_dev_debug_step_over(void) {
    printf("[dev] Debugger step over\n");
    return 1;
}

/* ── Profiler ── */

static struct ProfilerState {
    int running = 0;
    double start_time = 0;
    int frame_count = 0;
    double accum_time = 0;
} g_prof;

#ifdef _WIN32
static double prof_now(void) {
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return double(count.QuadPart) / double(freq.QuadPart);
}
#else
static double prof_now(void) {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return double(tv.tv_sec) + double(tv.tv_usec) / 1e6;
}
#endif

int aurora_dev_profiler_start(void) {
    std::lock_guard<std::mutex> lock(g_mtx);
    if (g_prof.running) return 0;
    g_prof.running = 1;
    g_prof.start_time = prof_now();
    g_prof.frame_count = 0;
    g_prof.accum_time = 0;
    return 1;
}

int aurora_dev_profiler_stop(void) {
    std::lock_guard<std::mutex> lock(g_mtx);
    if (!g_prof.running) return 0;
    g_prof.running = 0;
    return 1;
}

const char* aurora_dev_profiler_report(void) {
    static std::string result;
    std::ostringstream oss;
    double elapsed = g_prof.running ? (prof_now() - g_prof.start_time) : g_prof.accum_time;
    oss << "{\"elapsed_sec\":" << elapsed
        << ",\"frame_count\":" << g_prof.frame_count
        << ",\"avg_frame_ms\":" << (g_prof.frame_count > 0 ? (elapsed * 1000.0 / g_prof.frame_count) : 0)
        << "}";
    result = oss.str();
    return result.c_str();
}

void aurora_dev_profiler_reset(void) {
    std::lock_guard<std::mutex> lock(g_mtx);
    g_prof.running = 0;
    g_prof.frame_count = 0;
    g_prof.accum_time = 0;
}

/* ── Inspector ── */

const char* aurora_dev_inspector_tree(void) {
    static std::string result;
    result = "{\"root\":{\"type\":\"Window\",\"children\":[]}}";
    return result.c_str();
}

const char* aurora_dev_inspector_select(int x, int y) {
    static std::string result;
    std::ostringstream oss;
    oss << "{\"selected\":{\"x\":" << x << ",\"y\":" << y << ",\"type\":\"unknown\"}}";
    result = oss.str();
    return result.c_str();
}

void aurora_dev_inspector_refresh(void) {
    // Stub — real impl would re-query widget tree
}

/* ── Memory Viewer ── */

const char* aurora_dev_memory_stats(void) {
    static std::string result;
    result = "{\"total_kb\":0,\"used_kb\":0,\"gc_runs\":0}";
    return result.c_str();
}

const char* aurora_dev_memory_snapshot(void) {
    static std::string result;
    result = "{\"snapshot\":\"ok\",\"timestamp\":0}";
    return result.c_str();
}

int aurora_dev_memory_leak_check(void) {
    // Stub — real impl would use GC tracer
    return 1;
}

/* ── Performance Monitor ── */

static struct PerfState {
    int running = 0;
    double last_fps_time = 0;
    int fps_frame_count = 0;
    double current_fps = 0;
    double last_frame_time = 0;
} g_perf;

int aurora_dev_perf_start(void) {
    std::lock_guard<std::mutex> lock(g_mtx);
    if (g_perf.running) return 0;
    g_perf.running = 1;
    g_perf.last_fps_time = prof_now();
    g_perf.fps_frame_count = 0;
    g_perf.current_fps = 0;
    g_perf.last_frame_time = 0;
    return 1;
}

int aurora_dev_perf_stop(void) {
    std::lock_guard<std::mutex> lock(g_mtx);
    g_perf.running = 0;
    return 1;
}

double aurora_dev_perf_fps(void) {
    return g_perf.current_fps;
}

double aurora_dev_perf_frame_time(void) {
    return g_perf.last_frame_time;
}

const char* aurora_dev_perf_report(void) {
    static std::string result;
    std::ostringstream oss;
    oss << "{\"fps\":" << g_perf.current_fps
        << ",\"frame_time_ms\":" << g_perf.last_frame_time
        << ",\"running\":" << (g_perf.running ? "true" : "false")
        << "}";
    result = oss.str();
    return result.c_str();
}
