/* ════════════════════════════════════════════════════════════
   DAP runtime — file-based debug control channel.

   When AURORA_DAP_CTRL is set, these functions coordinate with a
   DAP server:
     <ctrl>/breakpoints.txt  line-number breakpoint set
     <ctrl>/events.jsonl     append-only event log (stopped events)
     <ctrl>/cmd.txt          command file (continue / next / step / exit)
     <ctrl>/state.txt        current call stack + line snapshot

   When AURORA_DAP_CTRL is unset, every hook is a no-op so normal
   builds are unaffected.
   ════════════════════════════════════════════════════════════ */
#include "runtime/dap_runtime.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

namespace {

const std::string& ctrl_dir() {
    static std::string dir = []() {
        const char* d = getenv("AURORA_DAP_CTRL");
        return d ? std::string(d) : std::string();
    }();
    return dir;
}

bool active() { return !ctrl_dir().empty(); }

std::string bp_path()    { return ctrl_dir() + "/breakpoints.txt"; }
std::string events_path(){ return ctrl_dir() + "/events.jsonl"; }
std::string cmd_path()   { return ctrl_dir() + "/cmd.txt"; }
std::string state_path() { return ctrl_dir() + "/state.txt"; }

void sleep_ms(int ms) {
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep(ms * 1000);
#endif
}

void write_file(const std::string& path, const std::string& text) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return;
    fwrite(text.data(), 1, text.size(), f);
    fclose(f);
}

std::string read_file(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return std::string();
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string out((size_t)len, '\0');
    if (len > 0) fread(&out[0], 1, (size_t)len, f);
    fclose(f);
    return out;
}

void append_file(const std::string& path, const std::string& text) {
    FILE* f = fopen(path.c_str(), "ab");
    if (!f) return;
    fwrite(text.data(), 1, text.size(), f);
    fclose(f);
}

std::vector<int> load_breakpoints() {
    std::vector<int> bps;
    std::string body = read_file(bp_path());
    size_t pos = 0;
    while (pos <= body.size()) {
        size_t nl = body.find('\n', pos);
        std::string line = body.substr(pos, (nl == std::string::npos ? body.size() : nl) - pos);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
            line.pop_back();
        if (!line.empty()) {
            char* end = nullptr;
            long v = strtol(line.c_str(), &end, 10);
            if (end && *end == '\0') bps.push_back((int)v);
        }
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
    return bps;
}

void clear_command() {
    FILE* f = fopen(cmd_path().c_str(), "wb");
    if (f) fclose(f); /* truncate */
}

/* Wait for a command from the DAP server. Returns the command word. */
std::string wait_command() {
    std::string cmd;
    while (true) {
        cmd = read_file(cmd_path());
        /* strip whitespace */
        while (!cmd.empty() && (cmd.back() == '\r' || cmd.back() == '\n' || cmd.back() == ' '))
            cmd.pop_back();
        if (!cmd.empty()) {
            clear_command();
            return cmd;
        }
        sleep_ms(10);
    }
}

/* ── call stack ── */
struct Frame { std::string fn; };
static std::vector<Frame> g_stack;
static int64_t g_last_line = 0;
static std::vector<std::string> g_vars;

} // namespace

extern "C" {

int aurora_dap_init(void) {
    return active() ? 1 : 0;
}

void aurora_dap_enter(const char* fn) {
    if (!active()) return;
    g_stack.push_back({ fn ? fn : "?" });
    g_vars.clear();   /* fresh locals per frame */
}

void aurora_dap_exit(void) {
    if (!active()) return;
    if (!g_stack.empty()) g_stack.pop_back();
}

void aurora_dap_var(const char* name, double value) {
    if (!active() || !name) return;
    char buf[128];
    snprintf(buf, sizeof(buf), "%s=%.6g", name, value);
    g_vars.push_back(buf);
    if (g_vars.size() > 512) g_vars.erase(g_vars.begin());
}

void aurora_dap_print(const char* text) {
    if (!active() || !text) return;
    std::string line = "{\"type\":\"output\",\"text\":\"";
    for (const char* p = text; *p; p++) {
        if (*p == '"' || *p == '\\') line += '\\';
        line += *p;
    }
    line += "\"}\n";
    append_file(events_path(), line);
}

void aurora_dap_trap(int64_t line) {
    if (!active()) return;
    g_last_line = line;

    /* Re-read breakpoints each time (cheap for a debugger; keeps the
       DAP server's setBreakpoints live without extra IPC). */
    static int last_mtime = 0;
    std::vector<int> bps;
    /* cache the breakpoints per mtime to avoid file churn */
    static std::vector<int> cached_bps;
    static int cached_mtime = 0;
    int mtime = 0;
#ifdef _WIN32
    HANDLE h = CreateFileA(bp_path().c_str(), GENERIC_READ, FILE_SHARE_READ,
                           nullptr, OPEN_EXISTING, 0, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        FILETIME ft;
        if (GetFileTime(h, nullptr, nullptr, &ft))
            mtime = (int)(ft.dwLowDateTime ^ ft.dwHighDateTime);
        CloseHandle(h);
    }
#else
    struct stat st;
    if (::stat(bp_path().c_str(), &st) == 0) mtime = (int)st.st_mtime;
#endif
    if (mtime != cached_mtime) {
        cached_mtime = mtime;
        cached_bps = load_breakpoints();
    }
    bps = cached_bps;

    bool is_bp = false;
    for (int b : bps)
        if ((int)line == b) { is_bp = true; break; }

    /* Read command file for step/pause directives */
    std::string pending = read_file(cmd_path());
    while (!pending.empty() && (pending.back() == '\r' || pending.back() == '\n'))
        pending.pop_back();
    bool step = (pending == "next" || pending == "step");
    bool pause = (pending == "pause");
    bool stop = is_bp || step || pause;

    if (pending == "continue") { clear_command(); return; }   /* no stop on continue */

    if (!stop) return;

    /* Consume the triggering command so wait_command() blocks fresh */
    if (step || pause) clear_command();

    /* Report stopped event */
    std::string ev = "{\"type\":\"stopped\",\"line\":";
    ev += std::to_string((long long)line);
    ev += ",\"reason\":\"";
    ev += is_bp ? "breakpoint" : (step ? "step" : "pause");
    ev += "\"";
    if (!g_stack.empty())
        ev += ",\"fn\":\"" + g_stack.back().fn + "\"";
    if (!g_vars.empty()) {
        ev += ",\"vars\":[";
        for (size_t i = 0; i < g_vars.size(); i++) {
            if (i) ev += ",";
            ev += "\"" + g_vars[i] + "\"";
        }
        ev += "]";
    }
    ev += "}\n";
    append_file(events_path(), ev);

    /* Write state snapshot */
    {
        std::string st = "line=" + std::to_string((long long)line);
        for (size_t i = 0; i < g_stack.size(); i++) st += " " + g_stack[i].fn;
        for (size_t i = 0; i < g_vars.size(); i++) st += " " + g_vars[i];
        write_file(state_path(), st + "\n");
    }

    /* Block until a command arrives */
    std::string cmd = wait_command();
    if (cmd == "exit") {
        write_file(ctrl_dir() + "/done.txt", "exited\n");
        std::exit(0);
    }
    if (cmd == "next" || cmd == "step") {
        /* One-shot step: the DAP server will write a temporary
           breakpoint for the next line, or we simply return and
           resume. Returning resumes execution. */
        return;
    }
    /* "continue" (or anything else) resumes */
}

} // extern "C"