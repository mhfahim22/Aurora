/* ════════════════════════════════════════════════════════════
   aurora_dap — Debug Adapter Protocol server for Aurora.

   Speaks the Debug Adapter Protocol over stdio:
     * initialize / launch / setBreakpoints / configurationDone
     * continue / next / stepIn / stepOut / pause
     * stackTrace / scopes / variables / threads
     * disconnect / terminate

   "launch" compiles the target source with aurorac using --dap
   (which instruments every statement with aurora_dap_trap(line)),
   then spawns the resulting executable with AURORA_DAP_CTRL set to
   a control directory.  The child's runtime hooks coordinate with
   this server through small files in that directory:
     breakpoints.txt   <- server writes line breakpoints
     cmd.txt           <- server writes continue/next/step/exit
     events.jsonl      -> child appends stopped/output events
     state.txt         -> child writes current stack snapshot

   Example DAP session (JSON-RPC, Content-Length framed):
     {"command":"initialize","arguments":{"adapterID":"aurora"},"seq":1,"type":"request"}
     {"command":"launch","arguments":{"program":"prog.aura"},"seq":2,"type":"request"}
     {"command":"setBreakpoints","arguments":{"source":{"path":"prog.aura"},"breakpoints":[{"line":5}]},"seq":3,"type":"request"}
     {"command":"configurationDone","arguments":{},"seq":4,"type":"request"}
     {"command":"continue","arguments":{"threadId":1},"seq":5,"type":"request"}
   ════════════════════════════════════════════════════════════ */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iostream>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#endif

namespace {

/* ── minimal JSON ── */
struct JVal {
    enum Type { NUL, BOOL, NUM, STR, ARR, OBJ } type = NUL;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<JVal> arr;
    std::vector<std::pair<std::string, JVal>> obj;

    JVal get(const std::string& key) const {
        if (type != OBJ) return {};
        for (auto& kv : obj) if (kv.first == key) return kv.second;
        return {};
    }
    std::string str_or(const std::string& def) const { return type == STR ? s : def; }
    int int_or(int def) const { return type == NUM ? (int)n : def; }
    bool bool_or(bool def) const { return type == BOOL ? b : def; }
    const std::vector<JVal>& items() const { return arr; }
};

class JParser {
public:
    static JVal parse(const std::string& in) { JParser p(in); return p.v(); }
private:
    JParser(const std::string& in) : s(in), pos(0) {}
    void ws() { while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r')) pos++; }
    JVal v() {
        ws();
        if (pos >= s.size()) return {};
        char c = s[pos];
        if (c == '{') return obj();
        if (c == '[') return arr();
        if (c == '"') return str();
        if (c == 't') { pos += 4; return {JVal::BOOL, true}; }
        if (c == 'f') { pos += 5; return {JVal::BOOL, false}; }
        if (c == 'n') { pos += 4; return {}; }
        return num();
    }
    JVal obj() {
        JVal o; o.type = JVal::OBJ; pos++;
        ws(); if (pos < s.size() && s[pos] == '}') { pos++; return o; }
        while (pos < s.size()) {
            JVal k = str();
            ws(); if (pos < s.size() && s[pos] == ':') pos++;
            JVal val = v();
            o.obj.emplace_back(k.s, val);
            ws();
            if (pos < s.size() && s[pos] == ',') { pos++; continue; }
            if (pos < s.size() && s[pos] == '}') { pos++; break; }
            break;
        }
        return o;
    }
    JVal arr() {
        JVal a; a.type = JVal::ARR; pos++;
        ws(); if (pos < s.size() && s[pos] == ']') { pos++; return a; }
        while (pos < s.size()) {
            a.arr.push_back(v());
            ws();
            if (pos < s.size() && s[pos] == ',') { pos++; continue; }
            if (pos < s.size() && s[pos] == ']') { pos++; break; }
            break;
        }
        return a;
    }
    JVal str() {
        JVal r; r.type = JVal::STR; pos++;
        while (pos < s.size() && s[pos] != '"') {
            if (s[pos] == '\\' && pos + 1 < s.size()) {
                char c = s[++pos];
                switch (c) {
                    case 'n': r.s += '\n'; break;
                    case 't': r.s += '\t'; break;
                    case 'r': r.s += '\r'; break;
                    case '"': r.s += '"'; break;
                    case '\\': r.s += '\\'; break;
                    default: r.s += c;
                }
                pos++;
            } else r.s += s[pos++];
        }
        if (pos < s.size()) pos++;
        return r;
    }
    JVal num() {
        JVal r; r.type = JVal::NUM;
        size_t start = pos;
        if (pos < s.size() && (s[pos] == '-' || s[pos] == '+')) pos++;
        while (pos < s.size() && (isdigit((unsigned char)s[pos]) || s[pos] == '.' || s[pos] == 'e' || s[pos] == 'E' || s[pos] == '-' || s[pos] == '+')) pos++;
        r.n = atof(s.substr(start, pos - start).c_str());
        return r;
    }
    const std::string& s;
    size_t pos;
};

std::string jesc(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    return out;
}

/* ── stdio message framing (DAP uses LSP framing) ── */
std::string read_msg() {
    std::string header;
    int len = 0;
    while (std::getline(std::cin, header)) {
        if (header == "\r" || header.empty()) break;
        if (header.rfind("Content-Length: ", 0) == 0)
            len = atoi(header.c_str() + 16);
    }
    if (len <= 0) return "";
    std::string body(len, '\0');
    std::cin.read(&body[0], len);
    return body;
}

void send_msg(const std::string& json) {
    std::string framed = "Content-Length: " + std::to_string(json.size()) + "\r\n\r\n" + json;
    std::cout << framed;
    std::cout.flush();
}

std::string response(const std::string& seq, const std::string& cmd,
                     const std::string& body /* raw JSON object, "" for success */) {
    std::string msg = "{\"seq\":" + seq + ",\"type\":\"response\",\"command\":\"" + cmd +
                      "\",\"request_seq\":" + seq + ",\"success\":true";
    if (!body.empty()) msg += ",\"body\":" + body;
    msg += "}";
    return msg;
}

std::string event(const std::string& seq, const std::string& ev, const std::string& body) {
    std::string msg = "{\"seq\":" + seq + ",\"type\":\"event\",\"event\":\"" + ev + "\"";
    if (!body.empty()) msg += ",\"body\":" + body;
    msg += "}";
    return msg;
}

/* ── control channel helpers ── */
std::string g_ctrl_dir;
int g_seq = 1;
bool g_child_alive = false;

#ifdef _WIN32
HANDLE g_child = INVALID_HANDLE_VALUE;
#else
pid_t g_child = -1;
#endif

std::string ctrl_path(const std::string& f) { return g_ctrl_dir + "/" + f; }

void write_file(const std::string& path, const std::string& text) {
    FILE* f = fopen(path.c_str(), "wb");
    if (f) { fwrite(text.data(), 1, text.size(), f); fclose(f); }
}

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return "";
    std::stringstream ss; ss << in.rdbuf();
    return ss.str();
}

std::string get_env(const char* name) {
    const char* v = getenv(name);
    return v ? std::string(v) : "";
}

#ifdef _WIN32
void spawn_child(const std::string& exe, const std::string& ctrl) {
    SetEnvironmentVariableA("AURORA_DAP_CTRL", ctrl.c_str());
    std::string cmd = "\"" + exe + "\"";
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread);
        g_child = pi.hProcess;
        g_child_alive = true;
    }
}
void kill_child() {
    if (g_child != INVALID_HANDLE_VALUE) {
        TerminateProcess(g_child, 0);
        CloseHandle(g_child);
        g_child = INVALID_HANDLE_VALUE;
    }
    g_child_alive = false;
}
bool child_running() {
    if (!g_child_alive || g_child == INVALID_HANDLE_VALUE) return false;
    DWORD ec = 0;
    if (GetExitCodeProcess(g_child, &ec) && ec == STILL_ACTIVE) return true;
    g_child_alive = false;
    return false;
}
#else
void spawn_child(const std::string& exe, const std::string& ctrl) {
    g_child = fork();
    if (g_child == 0) {
        setenv("AURORA_DAP_CTRL", ctrl.c_str(), 1);
        execl(exe.c_str(), exe.c_str(), (char*)nullptr);
        _exit(127);
    }
    if (g_child > 0) g_child_alive = true;
}
void kill_child() {
    if (g_child > 0) { kill(g_child, SIGKILL); waitpid(g_child, nullptr, 0); g_child = -1; }
    g_child_alive = false;
}
bool child_running() {
    if (g_child <= 0) return false;
    int st = 0;
    pid_t r = waitpid(g_child, &st, WNOHANG);
    if (r == 0) return true;
    g_child_alive = false;
    return false;
}
#endif

/* ── launch: run the program under aurorac's JIT with DAP traps ── */
bool compile_and_launch(const std::string& program) {
    std::string aurorac = get_env("AURORA_COMPILER");
    if (aurorac.empty()) {
#ifdef _WIN32
        aurorac = "aurorac.exe";
#else
        aurorac = "aurorac";
#endif
    }
    /* Pre-arm a "pause" so the child stops at its first line (entry) */
    write_file(ctrl_path("cmd.txt"), "pause");

#ifdef _WIN32
    /* Build command: aurorac --dap --run <program> */
    std::string cmd = aurorac + " --dap --run \"" + program + "\"";
    SetEnvironmentVariableA("AURORA_DAP_CTRL", g_ctrl_dir.c_str());
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (!CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        std::cerr << "[aurora_dap] failed to spawn aurorac\n";
        return false;
    }
    CloseHandle(pi.hThread);
    g_child = pi.hProcess;
    g_child_alive = true;
#else
    write_file(ctrl_path("cmd.txt"), "pause");
    g_child = fork();
    if (g_child == 0) {
        setenv("AURORA_DAP_CTRL", g_ctrl_dir.c_str(), 1);
        execl(aurorac.c_str(), aurorac.c_str(), "--dap", "--run", program.c_str(), (char*)nullptr);
        _exit(127);
    }
    if (g_child <= 0) {
        std::cerr << "[aurora_dap] failed to spawn aurorac\n";
        return false;
    }
    g_child_alive = true;
#endif
    return true;
}

/* Monitor events.jsonl, forwarding stopped events. Returns the latest
   stopped event line or empty. */
std::string poll_events() {
    static size_t offset = 0;
    std::string data = read_file(ctrl_path("events.jsonl"));
    if (offset > data.size()) offset = 0;
    if (offset == data.size()) return "";
    std::string tail = data.substr(offset);
    offset = data.size();
    std::string result;
    size_t pos = 0;
    while (pos < tail.size()) {
        size_t nl = tail.find('\n', pos);
        std::string line = tail.substr(pos, (nl == std::string::npos ? tail.size() : nl) - pos);
        if (!line.empty()) {
            if (line.find("\"stopped\"") != std::string::npos ||
                line.find("\"type\":\"stopped\"") != std::string::npos)
                result = line;
            else if (line.find("\"exited\"") != std::string::npos ||
                     line.find("\"type\":\"output\"") != std::string::npos)
                result = line;
        }
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
    return result;
}

/* Reset the event monitor offset (e.g. after launch, so the entry
   stop event is consumed by the server's synthetic event). */
void reset_events() {
    poll_events();
}

/* Parse a stopped event into (line, reason, fn) */
void parse_stopped(const std::string& ev, int& line, std::string& reason, std::string& fn) {
    line = 0; reason = "breakpoint"; fn = "";
    JVal j = JParser::parse(ev);
    line = j.get("line").int_or(0);
    reason = j.get("reason").str_or("breakpoint");
    fn = j.get("fn").str_or("");
}

} // namespace

int main(int argc, char** argv) {
    /* Create a unique control directory */
#ifdef _WIN32
    char tmp[MAX_PATH + 32];
    GetTempPathA(MAX_PATH, tmp);
    g_ctrl_dir = std::string(tmp) + "\\aurora_dap_" + std::to_string(GetCurrentProcessId());
    CreateDirectoryA(g_ctrl_dir.c_str(), nullptr);
#else
    char tmpl[] = "/tmp/aurora_dap_XXXXXX";
    g_ctrl_dir = mkdtemp(tmpl);
#endif

    bool launched = false;
    bool exited = false;
    std::string launched_program;

    while (true) {
        std::string raw = read_msg();
        if (raw.empty()) break;
        JVal msg = JParser::parse(raw);
        std::string type = msg.get("type").str_or("");
        std::string cmd = msg.get("command").str_or("");
        std::string seq = msg.get("seq").type == JVal::NUM
            ? std::to_string((int)msg.get("seq").n)
            : "0";
        JVal args = msg.get("arguments");

        if (type != "request") continue;

        if (cmd == "initialize") {
            send_msg(response(seq, cmd,
                "{\"supportsConfigurationDoneRequest\":true,"
                "\"supportsSetVariable\":false,\"supportsEvaluate\":true,"
                "\"supportsFunctionBreakpoints\":false,"
                "\"supportsConditionalBreakpoints\":false,"
                "\"supportsTerminateRequest\":true,"
                "\"supportTerminateDebuggee\":true,"
                "\"supportsStepping\":true,"
                "\"supportsLoadedSourcesRequest\":false,"
                "\"supportsModulesRequest\":false}"));
        }
        else if (cmd == "launch") {
            std::string program = args.get("program").str_or("");
            if (program.empty()) program = args.get("source").str_or("");
            if (program.empty()) {
                send_msg(response(seq, cmd, ""));
            } else {
                launched_program = program;
                bool ok = compile_and_launch(program);
                if (!ok) {
                    std::string err = "{\"message\":\"compile failed\",\"body\":{}}";
                    send_msg(response(seq, cmd, ""));
                    continue;
                }
                send_msg(response(seq, cmd, "{\"pid\":" + std::to_string(
#ifdef _WIN32
                    (long long)GetCurrentProcessId()
#else
                    (long long)getpid()
#endif
                ) + "}"));
                if (ok) {
                    launched = true;
                    reset_events();   /* consume the child's entry stop event */
                    /* initial stopped event at program start (line 1) */
                    send_msg(event(std::to_string(g_seq++), "initialized", ""));
                    send_msg(event(std::to_string(g_seq++), "stopped",
                        "{\"reason\":\"entry\",\"threadId\":1,\"allThreadsStopped\":true}"));
                }
            }
        }
        else if (cmd == "setBreakpoints") {
            JVal src = args.get("source");
            std::string path = src.get("path").str_or("");
            if (path.empty()) path = src.get("name").str_or("");
            JVal bps = args.get("breakpoints");
            std::string bp_file;
            for (auto& b : bps.items())
                bp_file += std::to_string(b.get("line").int_or(0)) + "\n";
            if (!g_ctrl_dir.empty()) write_file(ctrl_path("breakpoints.txt"), bp_file);
            /* report verified breakpoints back */
            std::string body = "{\"breakpoints\":[";
            for (size_t i = 0; i < bps.items().size(); i++) {
                if (i) body += ",";
                body += "{\"verified\":true,\"line\":" + std::to_string(bps.items()[i].get("line").int_or(0)) + "}";
            }
            body += "]}";
            send_msg(response(seq, cmd, body));
        }
        else if (cmd == "configurationDone") {
            send_msg(response(seq, cmd, ""));
        }
        else if (cmd == "continue") {
            write_file(ctrl_path("cmd.txt"), "continue");
            send_msg(response(seq, cmd, "{\"allThreadsContinued\":true}"));
            /* wait briefly for the child to hit the next breakpoint or exit */
            std::string ev;
            for (int i = 0; i < 500 && !exited; i++) {
                ev = poll_events();
                if (!ev.empty()) break;
#ifdef _WIN32
                Sleep(10);
#else
                usleep(10000);
#endif
                if (!child_running() && launched) { exited = true; break; }
            }
            if (exited) {
                send_msg(event(std::to_string(g_seq++), "exited", "{\"exitCode\":0}"));
                send_msg(event(std::to_string(g_seq++), "terminated", "{}"));
            } else if (!ev.empty()) {
                int line; std::string reason, fn;
                parse_stopped(ev, line, reason, fn);
                send_msg(event(std::to_string(g_seq++), "stopped",
                    "{\"reason\":\"" + reason + "\",\"threadId\":1,\"allThreadsStopped\":true}"));
            }
        }
        else if (cmd == "next" || cmd == "stepIn" || cmd == "stepOut") {
            write_file(ctrl_path("cmd.txt"), "step");
            send_msg(response(seq, cmd, ""));
            std::string ev;
            for (int i = 0; i < 500 && !exited; i++) {
                ev = poll_events();
                if (!ev.empty()) break;
#ifdef _WIN32
                Sleep(10);
#else
                usleep(10000);
#endif
                if (!child_running() && launched) { exited = true; break; }
            }
            if (exited) {
                send_msg(event(std::to_string(g_seq++), "exited", "{\"exitCode\":0}"));
                send_msg(event(std::to_string(g_seq++), "terminated", "{}"));
            } else if (!ev.empty()) {
                int line; std::string reason, fn;
                parse_stopped(ev, line, reason, fn);
                reason = "step";
                send_msg(event(std::to_string(g_seq++), "stopped",
                    "{\"reason\":\"" + reason + "\",\"threadId\":1,\"allThreadsStopped\":true}"));
            }
        }
        else if (cmd == "pause") {
            write_file(ctrl_path("cmd.txt"), "pause");
            send_msg(response(seq, cmd, ""));
        }
        else if (cmd == "stackTrace") {
            std::string st = read_file(ctrl_path("state.txt"));
            int line = 1;
            std::string fn = "main";
            std::vector<std::string> frames;
            std::string line_tok;
                {
                    size_t p = 0;
                    while (p <= st.size()) {
                        size_t sp = st.find(' ', p);
                        std::string tok = st.substr(p, (sp == std::string::npos ? st.size() : sp) - p);
                        while (!tok.empty() && (tok.back() == '\r' || tok.back() == '\n'))
                            tok.pop_back();
                        if (tok.rfind("line=", 0) == 0) line_tok = tok.substr(5);
                        else if (!tok.empty() && tok.find('=') == std::string::npos) {
                            if (frames.empty()) fn = tok;
                            frames.push_back(tok);
                        }
                        if (sp == std::string::npos) break;
                        p = sp + 1;
                    }
                    if (!line_tok.empty()) line = atoi(line_tok.c_str());
                }
            std::string frames_body = "{\"stackFrames\":[";
            if (frames.empty()) frames.push_back("main");
            for (size_t i = 0; i < frames.size(); i++) {
                if (i) frames_body += ",";
                frames_body += "{\"id\":" + std::to_string((int)i) +
                    ",\"name\":\"" + jesc(frames[i]) + "\",\"line\":" +
                    std::to_string(line) + ",\"column\":1,\"source\":{\"name\":\"" +
                    jesc(launched_program) + "\",\"path\":\"" + jesc(launched_program) + "\"}}";
            }
            frames_body += "],\"totalFrames\":" + std::to_string((int)frames.size()) + "}";
            send_msg(response(seq, cmd, frames_body));
        }
        else if (cmd == "scopes") {
            send_msg(response(seq, cmd,
                "{\"scopes\":[{\"name\":\"Locals\",\"variablesReference\":1,\"expensive\":false}]}"));
        }
        else if (cmd == "variables") {
            std::string st = read_file(ctrl_path("state.txt"));
            /* parse vars="name=value" tokens from state line after "line=N" */
            std::string vars_body = "{\"variables\":[";
            size_t p = 0;
            bool first = true;
            while (p <= st.size()) {
                size_t sp = st.find(' ', p);
                std::string tok = st.substr(p, (sp == std::string::npos ? st.size() : sp) - p);
                while (!tok.empty() && (tok.back() == '\r' || tok.back() == '\n'))
                    tok.pop_back();
                if (tok.rfind("line=", 0) != 0 && !tok.empty()) {
                    size_t eq = tok.find('=');
                    if (eq != std::string::npos) {
                        if (!first) vars_body += ",";
                        vars_body += "{\"name\":\"" + jesc(tok.substr(0, eq)) +
                                     "\",\"value\":\"" + jesc(tok.substr(eq + 1)) +
                                     "\",\"variablesReference\":0}";
                        first = false;
                    }
                }
                if (sp == std::string::npos) break;
                p = sp + 1;
            }
            vars_body += "]}";
            send_msg(response(seq, cmd, vars_body));
        }
        else if (cmd == "threads") {
            send_msg(response(seq, cmd, "{\"threads\":[{\"id\":1,\"name\":\"main\"}]}"));
        }
        else if (cmd == "evaluate") {
            send_msg(response(seq, cmd,
                "{\"result\":\"<eval not supported>\",\"variablesReference\":0}"));
        }
        else if (cmd == "disconnect" || cmd == "terminate") {
            write_file(ctrl_path("cmd.txt"), "exit");
            kill_child();
            send_msg(response(seq, cmd, ""));
            break;
        }
        else {
            send_msg(response(seq, cmd, ""));
        }
    }

    kill_child();
#ifdef _WIN32
    /* best-effort cleanup of control files */
    for (const char* f : { "breakpoints.txt", "cmd.txt", "events.jsonl", "state.txt", "done.txt", "prog.exe" })
        DeleteFileA(ctrl_path(f).c_str());
    RemoveDirectoryA(g_ctrl_dir.c_str());
#else
    for (const char* f : { "breakpoints.txt", "cmd.txt", "events.jsonl", "state.txt", "done.txt", "prog" })
        unlink(ctrl_path(f).c_str());
    rmdir(g_ctrl_dir.c_str());
#endif
    return 0;
}