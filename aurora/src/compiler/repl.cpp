#include "compiler/repl.hpp"
#include "compiler/lexer.hpp"
#include "compiler/parser.hpp"
#include "compiler/keywords.hpp"
#include "compiler/typechecker.hpp"
#include "compiler/memory_analyzer.hpp"
#include "compiler/codegen.hpp"
#include "common/aurora_version.hpp"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <algorithm>

#if defined(_WIN32)
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#ifndef AURORA_REPL_VERSION
#define AURORA_REPL_VERSION "1.0.0"
#endif

namespace {

#if defined(_WIN32)
HANDLE g_hIn  = nullptr;
HANDLE g_hOut = nullptr;
#endif

/* ────────────────────────────────────────────────────────────
   History
   ──────────────────────────────────────────────────────────── */
std::vector<std::string> g_history;
size_t                   g_history_pos = 0;

std::string history_file_path() {
#if defined(_WIN32)
    const char* ud = getenv("USERPROFILE");
    return std::string(ud ? ud : "") + "\\.aurora_history";
#else
    const char* hd = getenv("HOME");
    return std::string(hd ? hd : "") + "/.aurora_history";
#endif
}

void history_load() {
    std::ifstream in(history_file_path());
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line != ":q")
            g_history.push_back(line);
    }
    if (g_history.size() > 500)
        g_history.erase(g_history.begin(), g_history.end() - 500);
    g_history_pos = g_history.size();
}

void history_add(const std::string& line) {
    std::string s = line;
    size_t st = s.find_first_not_of(" \t\r\n");
    if (st == std::string::npos) return;
    size_t en = s.find_last_not_of(" \t\r\n");
    s = s.substr(st, en - st + 1);
    if (s.empty()) return;
    if (!g_history.empty() && g_history.back() == s) return;
    g_history.push_back(s);
    if (g_history.size() > 500)
        g_history.erase(g_history.begin());
    g_history_pos = g_history.size();
}

void history_save() {
    std::ofstream out(history_file_path(), std::ios::trunc);
    for (auto& h : g_history)
        out << h << "\n";
}

/* ────────────────────────────────────────────────────────────
   Completion candidates: keywords + builtin function names
   ──────────────────────────────────────────────────────────── */
std::vector<std::string> completion_candidates() {
    std::vector<std::string> out;
    for (auto& kw : aurora_keywords())
        out.push_back(kw);
    static const char* builtins[] = {
        "output","outputln","input","len","str_repeat","sum","min","max","range",
        "typeof","sizeof","convert","clone","debug","panic",
        "upper","lower","trim","replace","split","join","has","starts","ends",
        "reverse","strlen","strcat","substr","index",
        "abs","sqrt","floor","ceil","round","pow","clamp","rand","pi",
        "sin","cos","tan","str","int","float","bool",
        "push","pop","insert","remove","clear","sort","unique","map","filter",
        "reduce","find","any","all","list_get","list_set","list_push","list_len",
        "list_free","map_get","map_set","map_has","map_free","f64array_new",
        "f64array_get","f64array_set","f64array_len","file_exists",
        "sleep","time","random","outputf","print",
        "json_parse","json_stringify","json_get","json_set",
        "server_start","server_route","server_stop",
        "app_window","app_label","app_button","app_run",
    };
    for (auto b : builtins) out.push_back(b);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

/* ────────────────────────────────────────────────────────────
   :doc — look up a symbol across libc/*.auf + the REPL source
   ──────────────────────────────────────────────────────────── */
void repl_doc(const std::string& name, const std::string& source_path) {
    std::vector<std::string> search;
    search.push_back(source_path);
    for (auto& f : {
        "libc/json.auf", "libc/math.auf", "libc/string.auf", "libc/collections.auf",
        "libc/server.auf", "libc/stdlib.auf", "libc/fs.auf", "libc/time.auf",
        "libc/datetime.auf", "libc/regex.auf", "libc/uuid.auf", "libc/url.auf",
        "libc/decimal.auf", "libc/app.auf", "libc/gui.auf", "libc/widget.auf",
        "libc/db.auf", "libc/crypto.auf", "libc/security.auf",
    }) {
        std::ifstream probe(f);
        if (probe) search.push_back(f);
    }

    std::string needle = name;
    for (auto& path : search) {
        std::ifstream in(path);
        if (!in) continue;
        std::vector<std::string> file_lines;
        std::string line;
        std::string pending_doc;
        while (std::getline(in, line)) {
            std::string t = line;
            size_t st = t.find_first_not_of(" \t");
            if (st != std::string::npos) t = t.substr(st);
            if (t.rfind("##", 0) == 0) {
                pending_doc += t + "\n";
                continue;
            }
            bool is_decl =
                (t.rfind("function ", 0) == 0 ||
                 t.rfind("extern function ", 0) == 0 ||
                 t.rfind("struct ", 0) == 0 ||
                 t.rfind("class ", 0) == 0 ||
                 t.rfind("enum ", 0) == 0 ||
                 t.rfind("interface ", 0) == 0 ||
                 t.rfind("type ", 0) == 0);
            if (!is_decl) { if (!t.empty()) pending_doc.clear(); continue; }
            std::string sig = t;
            size_t np = sig.find_first_of("(:<");
            if (np != std::string::npos) sig = sig.substr(0, np);
            size_t ls = sig.find_last_of(" \t");
            std::string sym = (ls == std::string::npos) ? sig : sig.substr(ls + 1);
            if (sym == needle) {
                if (!pending_doc.empty())
                    std::cout << pending_doc;
                std::cout << path << ":\n  " << t << "\n";
                return;
            }
            pending_doc.clear();
        }
    }
    /* Builtin fallback */
    static const std::vector<std::pair<std::string, std::string>> builtin_docs = {
        {"len",    "len(x)          -> int     Length of a list, map, string or array"},
        {"typeof", "typeof(x)       -> string  Type name of an expression at compile time"},
        {"sizeof", "sizeof(x)       -> int     Byte size of a value at compile time"},
        {"output", "output(x)                Prints a value without a trailing newline"},
        {"outputln","outputln(x)             Prints a value followed by a newline"},
        {"input",  "input()          -> string Reads a line from standard input"},
        {"range",  "range(n) / range(a,b) -> list of integers"},
        {"abs",    "abs(x)           -> number Absolute value"},
        {"push",   "push(list, v)            Appends to a list"},
        {"pop",    "pop(list)        -> value Removes and returns the last element"},
        {"sort",   "sort(list)                Sorts a list in place"},
        {"reverse","reverse(s)       -> string Reverses a string"},
        {"split",  "split(s, sep)    -> list   Splits a string"},
        {"join",   "join(list, sep)  -> string Joins a list into a string"},
        {"trim",   "trim(s)          -> string Trims surrounding whitespace"},
        {"upper",  "upper(s)         -> string Uppercases a string"},
        {"lower",  "lower(s)         -> string Lowercases a string"},
        {"str",    "str(x)           -> string Converts a value to string"},
        {"int",    "int(x)           -> int    Converts a value to int"},
        {"float",  "float(x)         -> float  Converts a value to float"},
        {"bool",   "bool(x)          -> bool   Converts a value to bool"},
        {"random", "random()         -> int    Returns a random integer"},
        {"time",   "time()           -> int    Current unix timestamp (seconds)"},
        {"sleep",  "sleep(ms)                 Sleeps for ms milliseconds"},
    };
    for (auto& b : builtin_docs) {
        if (b.first == name) {
            std::cout << "builtin: " << b.second << "\n";
            return;
        }
    }
    std::cout << ":doc: no symbol '" << name << "' found (checked libc/*.auf)\n";
}

/* ────────────────────────────────────────────────────────────
   Line editor: history + tab completion (Windows / POSIX)
   Returns the edited line (without trailing newline).
   On non-tty stdin this degrades to std::getline.
   ──────────────────────────────────────────────────────────── */
bool g_tty = false;
bool g_eof = false;

void term_set_raw(bool raw) {
#if defined(_WIN32)
    if (!g_hIn) return;
    DWORD mode;
    if (GetConsoleMode(g_hIn, &mode)) {
        if (raw) {
            mode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
            mode |= ENABLE_PROCESSED_INPUT;
        } else {
            mode |= (ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
        }
        SetConsoleMode(g_hIn, mode);
    }
#else
    static termios saved;
    static bool has_saved = false;
    if (raw) {
        termios rawt;
        tcgetattr(STDIN_FILENO, &rawt);
        saved = rawt;
        has_saved = true;
        rawt.c_lflag &= ~(ICANON | ECHO);
        rawt.c_cc[VMIN] = 1;
        rawt.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &rawt);
    } else {
        if (has_saved)
            tcsetattr(STDIN_FILENO, TCSANOW, &saved);
    }
#endif
}

std::string read_console_line() {
    /* detect tty */
#if defined(_WIN32)
    g_hIn  = GetStdHandle(STD_INPUT_HANDLE);
    g_hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    g_tty  = g_hIn && (GetFileType(g_hIn) == FILE_TYPE_CHAR);
#else
    g_tty = isatty(STDIN_FILENO);
#endif

    if (!g_tty)
        return "";  /* caller falls back to getline */

    std::string line;
    size_t cursor = 0;
    int history_index = static_cast<int>(g_history.size());

    term_set_raw(true);

    auto redraw = [&]() {
        std::cout << "\r\x1b[K" << line << "\r\x1b[" << (cursor + 1) << "C";
        std::cout.flush();
    };
    std::cout << ">> " << std::flush;
    redraw();

#if defined(_WIN32)
    while (true) {
        INPUT_RECORD rec;
        DWORD n = 0;
        if (!ReadConsoleInputW(g_hIn, &rec, 1, &n) || n == 0) continue;
        if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown) continue;
        wchar_t ch = rec.Event.KeyEvent.uChar.UnicodeChar;
        WORD vk  = rec.Event.KeyEvent.wVirtualKeyCode;
        if (vk == VK_RETURN) break;
        if (vk == VK_TAB) {
            /* completion */
            std::string word;
            size_t ws = line.find_last_of(" \t(");
            if (ws == std::string::npos) word = line; else word = line.substr(ws + 1);
            if (!word.empty()) {
                auto cands = completion_candidates();
                std::vector<std::string> matches;
                for (auto& c : cands)
                    if (c.rfind(word, 0) == 0) matches.push_back(c);
                if (matches.size() == 1) {
                    size_t ws2 = (ws == std::string::npos) ? 0 : ws + 1;
                    line = line.substr(0, ws2) + matches[0];
                    cursor = line.size();
                    redraw();
                } else if (matches.size() > 1) {
                    std::cout << "\n";
                    for (auto& m : matches) std::cout << m << "  ";
                    std::cout << "\n";
                    std::cout << ">> " << std::flush;
                    redraw();
                }
            }
            continue;
        }
        if (vk == VK_LEFT) {
            if (cursor > 0) { cursor--; redraw(); }
            continue;
        }
        if (vk == VK_RIGHT) {
            if (cursor < line.size()) { cursor++; redraw(); }
            continue;
        }
        if (vk == VK_HOME)  { cursor = 0; redraw(); continue; }
        if (vk == VK_END)   { cursor = line.size(); redraw(); continue; }
        if (vk == VK_UP) {
            if (history_index > 0) {
                if (history_index == (int)g_history.size()) {
                    /* save current line as draft? just replace */
                }
                history_index--;
                line = g_history[history_index];
                cursor = line.size();
                redraw();
            }
            continue;
        }
        if (vk == VK_DOWN) {
            if (history_index < (int)g_history.size() - 1) {
                history_index++;
                line = g_history[history_index];
            } else {
                history_index = (int)g_history.size();
                line.clear();
            }
            cursor = line.size();
            redraw();
            continue;
        }
        if (vk == VK_BACK) {
            if (cursor > 0) {
                line.erase(cursor - 1, 1);
                cursor--;
                redraw();
            }
            continue;
        }
        if (vk == VK_DELETE) {
            if (cursor < line.size()) {
                line.erase(cursor, 1);
                redraw();
            }
            continue;
        }
        if (ch == 3) {  /* Ctrl+C */
            line.clear();
            std::cout << "^C\n";
            term_set_raw(false);
            throw std::runtime_error("interrupt");
        }
        if (ch == 4) {  /* Ctrl+D */
            term_set_raw(false);
            std::cout << "\n";
            g_eof = true;
            return "";
        }
        if (ch >= 32 && ch != 127) {
            line.insert(cursor, 1, (char)ch);
            cursor++;
            redraw();
        }
    }
#else
    char c;
    while (read(STDIN_FILENO, &c, 1) == 1) {
        if (c == '\n') break;
        if (c == 3) {  /* Ctrl+C */
            line.clear();
            std::cout << "^C\n";
            term_set_raw(false);
            throw std::runtime_error("interrupt");
        }
        if (c == 4) {  /* Ctrl+D */
            term_set_raw(false);
            std::cout << "\n";
            g_eof = true;
            return "";
        }
        if (c == 27) {  /* escape sequence */
            char s[2];
            if (read(STDIN_FILENO, s, 1) != 1) continue;
            if (s[0] == '[') {
                char code;
                if (read(STDIN_FILENO, &code, 1) != 1) continue;
                if (code == 'A') {  /* up */
                    if (history_index > 0) {
                        history_index--;
                        line = g_history[history_index];
                        cursor = line.size();
                    }
                    redraw();
                } else if (code == 'B') {  /* down */
                    if (history_index < (int)g_history.size() - 1) {
                        history_index++;
                        line = g_history[history_index];
                    } else {
                        history_index = (int)g_history.size();
                        line.clear();
                    }
                    cursor = line.size();
                    redraw();
                } else if (code == 'C') {  /* right */
                    if (cursor < line.size()) { cursor++; redraw(); }
                } else if (code == 'D') {  /* left */
                    if (cursor > 0) { cursor--; redraw(); }
                }
            }
            continue;
        }
        if (c == 127 || c == 8) {  /* backspace */
            if (cursor > 0) {
                line.erase(cursor - 1, 1);
                cursor--;
                redraw();
            }
            continue;
        }
        if (c == '\t') {
            std::string word;
            size_t ws = line.find_last_of(" \t(");
            if (ws == std::string::npos) word = line; else word = line.substr(ws + 1);
            if (!word.empty()) {
                auto cands = completion_candidates();
                std::vector<std::string> matches;
                for (auto& cc : cands)
                    if (cc.rfind(word, 0) == 0) matches.push_back(cc);
                if (matches.size() == 1) {
                    size_t ws2 = (ws == std::string::npos) ? 0 : ws + 1;
                    line = line.substr(0, ws2) + matches[0];
                    cursor = line.size();
                    redraw();
                } else if (matches.size() > 1) {
                    std::cout << "\n";
                    for (auto& m : matches) std::cout << m << "  ";
                    std::cout << "\n";
                    std::cout << ">> " << std::flush;
                    redraw();
                }
            }
            continue;
        }
        if (c >= 32 && c != 127) {
            line.insert(cursor, 1, c);
            cursor++;
            redraw();
        }
    }
#endif

    term_set_raw(false);
    std::cout << "\n";
    return line;
}

/* ────────────────────────────────────────────────────────────
   Helpers (moved from main.cpp)
   ──────────────────────────────────────────────────────────── */
bool repl_line_needs_continuation(const std::string& line) {
    std::string s = line;
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return false;
    if (start > 0) s = s.substr(start);
    size_t end = s.find_last_not_of(" \t\r\n");
    if (end != std::string::npos) s = s.substr(0, end + 1);
    if (s.empty()) return false;
    if (s.back() == ':') return true;

    bool in_str = false;
    int parens = 0, brackets = 0, braces = 0;
    for (char c : s) {
        if (c == '"') { in_str = !in_str; continue; }
        if (in_str) continue;
        if (c == '(') parens++;
        else if (c == ')') parens--;
        else if (c == '[') brackets++;
        else if (c == ']') brackets--;
        else if (c == '{') braces++;
        else if (c == '}') braces--;
    }
    return parens > 0 || brackets > 0 || braces > 0;
}

int repl_detect_indent(const std::string& raw) {
    size_t pos = 0;
    while (pos < raw.size()) {
        size_t nl = raw.find('\n', pos);
        std::string l = raw.substr(pos, nl - pos);
        if (!l.empty() && l.find_first_not_of(" \t\r") != std::string::npos) {
            size_t first_nonws = l.find_first_not_of(" \t");
            if (first_nonws != std::string::npos && first_nonws > 0)
                return (int)first_nonws;
            return 0;
        }
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
    return 2;
}

std::string repl_indent_body(const std::string& raw, int indent) {
    std::string indent_str(static_cast<size_t>(indent), ' ');
    std::string out;
    size_t pos = 0;
    while (pos < raw.size()) {
        size_t nl = raw.find('\n', pos);
        std::string l = raw.substr(pos, nl - pos);
        if (!l.empty() && l.find_first_not_of(" \t\r") != std::string::npos)
            out += indent_str + l + "\n";
        else
            out += "\n";
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
    return out;
}

} // namespace

/* ────────────────────────────────────────────────────────────
   Public entry point
   ──────────────────────────────────────────────────────────── */
int run_repl(const ReplOptions& opts) {
    bool term = true;
#if defined(_WIN32)
    term = false;
#endif

    std::cout << (term ? "\x1b[1m" : "") << "Aurora "
              << (term ? "\x1b[32m" : "") << "REPL" << (term ? "\x1b[0m" : "") << " v"
              << AURORA_REPL_VERSION << " (exit with :q or Ctrl+C, help with :help)\n";

    history_load();

    /* Compile and JIT-run a complete snippet */
    auto exec_repl = [&](const std::string& code) {
        int user_indent = repl_detect_indent(code);
        std::string indent_str(static_cast<size_t>(user_indent), ' ');
        std::string wrapped =
            "function main():\n" +
            repl_indent_body(code, user_indent) +
            indent_str + "return 0\n";

        try {
            Lexer lexer;
            auto lines = lexer.lex(wrapped);

            Parser parser(lines, opts.strict_indent);
            ASTNode::Ptr ast = parser.parse();

            if (parser.had_error()) {
                for (const auto& err : parser.errors())
                    std::cerr << "\n" << err;
                std::cerr << "\n";
                return;
            }

            MemoryAnalyzer memory_analyzer;
            memory_analyzer.analyse(ast.get());
            memory_analyzer.apply_to_ast(ast.get());

            if (memory_analyzer.has_errors()) {
                std::cerr << "REPL: compilation errors\n";
                return;
            }

            auto ctx = std::make_unique<llvm::LLVMContext>();
            auto module = std::make_unique<llvm::Module>("aurora_repl", *ctx);
            auto builder = std::make_unique<llvm::IRBuilder<>>(*ctx);
            Codegen codegen(*ctx, module, builder);
            codegen.set_source_file(opts.source_path);
            codegen.set_coverage_enabled(opts.enable_coverage);
            codegen.set_dap_enabled(opts.enable_dap);
            std::unique_ptr<llvm::DIBuilder> debug_builder;
            if (opts.enable_debug && !opts.source_path.empty()) {
                debug_builder = std::make_unique<llvm::DIBuilder>(*module);
                codegen.set_debug_builder(debug_builder.get());
                codegen.set_debug_enabled(true);
            }
            codegen.generate(ast.get());

            int exit_code = jit_execute_main(std::move(ctx), std::move(module));
            if (exit_code != 0 && exit_code != -1)
                std::cout << "exit: " << exit_code << "\n";

        } catch (const std::exception& e) {
            std::cerr << "REPL error: " << e.what() << "\n";
        }
    };

    std::string buffer;
    bool continuation = false;

    while (true) {
        std::cout << (continuation ? "> " : ">> ") << std::flush;

        std::string line;
        if (g_tty) {
            try {
                line = read_console_line();
            } catch (const std::runtime_error&) {
                /* Ctrl+C — clear current buffer and continue */
                buffer.clear();
                continuation = false;
                continue;
            }
            if (g_eof) break;
        } else {
            if (!std::getline(std::cin, line)) {
                std::cout << "\n";
                break;
            }
        }
        if (g_tty && line.empty()) continue;

        /* trim CR */
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line == "exit" || line == ":q") {
            history_save();
            break;
        }

        if (!continuation) {
            if (line.empty()) continue;

            if (line == ":help" || line == ":h") {
                std::cout << "Aurora REPL commands:\n"
                          << "  :q / exit     quit\n"
                          << "  :paste        paste multi-line code (terminate with blank line)\n"
                          << "  :type <expr>  show the inferred type of an expression\n"
                          << "  :doc <name>   show doc comment for a symbol\n"
                          << "  :load <file>  execute code from a file\n"
                          << "  :history      list session history\n"
                          << "  :help         this help\n"
                          << "  Up/Down       history navigation, Tab = completion\n";
                continue;
            }
            if (line == ":history") {
                for (size_t i = 0; i < g_history.size(); i++)
                    std::cout << (i + 1) << ": " << g_history[i] << "\n";
                continue;
            }
            if (line.rfind(":type ", 0) == 0) {
                std::string expr = line.substr(6);
                exec_repl("outputln(typeof(" + expr + "))");
                continue;
            }
            if (line.rfind(":doc ", 0) == 0) {
                std::string name = line.substr(5);
                size_t sp = name.find_first_of(" \t");
                if (sp != std::string::npos) name = name.substr(0, sp);
                repl_doc(name, opts.source_path);
                continue;
            }
            if (line.rfind(":load ", 0) == 0) {
                std::string path = line.substr(6);
                std::ifstream in(path);
                if (!in) {
                    std::cerr << ":load: cannot open " << path << "\n";
                    continue;
                }
                std::stringstream ss;
                ss << in.rdbuf();
                exec_repl(ss.str());
                continue;
            }
            if (line == ":paste") {
                std::string paste_buf, pline;
                while (std::getline(std::cin, pline) && !pline.empty())
                    paste_buf += pline + "\n";
                if (!paste_buf.empty()) {
                    exec_repl(paste_buf);
                    history_add(paste_buf);
                }
                continue;
            }

            if (repl_line_needs_continuation(line)) {
                buffer = line + "\n";
                continuation = true;
                continue;
            }

            exec_repl(line);
            history_add(line);
        } else {
            if (line.empty()) {
                exec_repl(buffer);
                history_add(buffer);
                buffer.clear();
                continuation = false;
                continue;
            }
            buffer += line + "\n";
        }
    }
    return 0;
}
