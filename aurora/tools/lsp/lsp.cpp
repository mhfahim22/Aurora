/* ════════════════════════════════════════════════════════════
   lsp.cpp — Aurora Language Server Protocol implementation
   Communicates via stdin/stdout using JSON-RPC 2.0.
   ════════════════════════════════════════════════════════════ */

#include "../../include/tools/lsp.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <set>
#include <regex>
#include <cstdio>

/* ════════════════════════════════════════════════════════════
   JsonBuilder
   ════════════════════════════════════════════════════════════ */

void JsonBuilder::comma() { if (!first_) ss_ << ","; first_ = false; }

void JsonBuilder::add(const std::string& key, const std::string& val) {
    comma(); ss_ << "\"" << key << "\":\"" << val << "\"";
}

void JsonBuilder::add_int(const std::string& key, int val) {
    comma(); ss_ << "\"" << key << "\":" << val;
}

void JsonBuilder::add_bool(const std::string& key, bool val) {
    comma(); ss_ << "\"" << key << "\":" << (val ? "true" : "false");
}

void JsonBuilder::add_null(const std::string& key) {
    comma(); ss_ << "\"" << key << "\":null";
}

void JsonBuilder::add_raw(const std::string& key, const std::string& raw) {
    comma(); ss_ << "\"" << key << "\":" << raw;
}

/* ════════════════════════════════════════════════════════════
   LspServer implementation
   ════════════════════════════════════════════════════════════ */

LspServer::LspServer() {}

std::string LspServer::escape_json(const std::string& s) {
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

std::string LspServer::read_message() {
    /* Read Content-Length header */
    std::string header;
    int content_length = 0;
    while (std::getline(std::cin, header)) {
        if (header == "\r" || header.empty()) break;
        if (header.rfind("Content-Length: ", 0) == 0) {
            content_length = std::stoi(header.substr(16));
        }
    }
    if (content_length <= 0) return "";
    std::string body(content_length, '\0');
    std::cin.read(&body[0], content_length);
    return body;
}

void LspServer::send_message(const std::string& json) {
    std::string msg = "Content-Length: " + std::to_string(json.size()) + "\r\n\r\n" + json;
    std::cout << msg;
    std::cout.flush();
}

std::string LspServer::make_response(const std::string& id, const std::string& result) {
    JsonBuilder jb;
    jb.add("jsonrpc", "2.0");
    jb.add("id", id);
    jb.add_raw("result", result);
    return jb.str();
}

std::string LspServer::make_error(const std::string& id, int code, const std::string& msg) {
    JsonBuilder jb;
    jb.add("jsonrpc", "2.0");
    jb.add("id", id);
    std::string err = "{\"code\":" + std::to_string(code) + ",\"message\":\"" + escape_json(msg) + "\"}";
    jb.add_raw("error", err);
    return jb.str();
}

std::string LspServer::make_notification(const std::string& method, const std::string& params) {
    JsonBuilder jb;
    jb.add("jsonrpc", "2.0");
    jb.add("method", method);
    jb.add_raw("params", params);
    return jb.str();
}

std::string LspServer::get_json_field(const std::string& json, const std::string& field) {
    std::regex re("\"" + field + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch m;
    if (std::regex_search(json, m, re)) return m[1];
    std::regex re2("\"" + field + "\"\\s*:\\s*([0-9]+)");
    if (std::regex_search(json, m, re2)) return m[1];
    std::regex re3("\"" + field + "\"\\s*:\\s*(true|false|null)");
    if (std::regex_search(json, m, re3)) return m[1];
    return "";
}

std::vector<std::string> LspServer::get_keywords() {
    return {
        "function", "return", "if", "else", "elseif", "while", "for", "loop",
        "break", "continue", "match", "case", "default", "switch",
        "class", "extends", "implements", "interface", "enum", "struct",
        "public", "private", "protected", "static", "final", "abstract",
        "try", "catch", "finally", "throw", "panic", "ensure",
        "import", "from", "namespace", "module", "package", "extern",
        "async", "await", "spawn", "parallel", "thread",
        "true", "false", "null", "and", "or", "not",
        "new", "self", "super", "lambda", "type",
        "output", "debug", "log", "pass",
        "move", "copy", "shared", "weak", "borrow", "drop", "delete",
        "safe", "unsafe", "const", "mutable", "reference", "pointer",
        "event", "signal", "emit", "callback",
        "scene", "entity", "sprite", "camera", "physics",
        "component", "render", "state", "properties",
        "server", "api", "route", "middleware",
        "ai", "tensor", "train", "predict",
        "int", "float", "string", "bool", "void", "list", "map", "set", "array", "json"
    };
}

/* ════════════════════════════════════════════════════════════
   Analysis
   ════════════════════════════════════════════════════════════ */

static void add_diag(std::vector<LspDiagnostic>& diags, int line, int col_start, int col_end, int severity, const std::string& msg, const std::string& source) {
    LspDiagnostic d;
    d.range.start = {line, col_start};
    d.range.end = {line, col_end};
    d.severity = severity;
    d.message = msg;
    d.source = source;
    diags.push_back(d);
}

std::vector<LspDiagnostic> LspServer::analyze(const std::string& text) {
    std::vector<LspDiagnostic> diags;
    std::istringstream stream(text);
    std::string line;
    int line_num = 0;
    std::vector<std::string> lines;

    /* Collect all lines first for multi-line analysis */
    {
        std::string l;
        std::istringstream s(text);
        while (std::getline(s, l)) lines.push_back(l);
    }

    /* ── Pass 1: collect declarations, definitions, and uses ── */
    std::set<int> class_lines;
    std::set<int> func_lines;
    std::map<std::string, int> var_defs;      /* variable -> line defined */
    std::map<std::string, int> var_uses;      /* variable -> line last used */
    std::set<std::string> vars_all;            /* all variable names seen */

    std::regex func_def_re(R"(^\s*function\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\()");
    std::regex class_def_re(R"(^\s*class\s+([a-zA-Z_][a-zA-Z0-9_]*))");
    std::regex var_def_re(R"(^\s*(?:let\s+|var\s+|const\s+)?([a-zA-Z_][a-zA-Z0-9_]*)\s*(?::\s*\w+)?\s*=[^=])");
    std::regex var_use_re(R"(\b([a-zA-Z_][a-zA-Z0-9_]*)\b)");
    std::regex return_re(R"(\breturn\b)");
    std::regex string_lit_re(R"("(?:[^"\\]|\\.)*")");
    std::regex int_lit_re(R"(\b\d+\b)");
    std::regex output_re(R"(\boutput\s*\()");

    for (size_t i = 0; i < lines.size(); i++) {
        const std::string& ln = lines[i];
        std::smatch m;

        if (std::regex_search(ln, m, func_def_re)) {
            func_lines.insert((int)i);
        }
        if (std::regex_search(ln, m, class_def_re)) {
            class_lines.insert((int)i);
        }
        if (std::regex_search(ln, m, var_def_re)) {
            std::string vname = m[1];
            var_defs[vname] = (int)i;
            vars_all.insert(vname);
        }

        auto start = ln.cbegin();
        while (std::regex_search(start, ln.cend(), m, var_use_re)) {
            std::string w = m[1];
            static const std::set<std::string> keywords = {
                "function", "return", "if", "else", "elseif", "while", "for", "loop",
                "break", "continue", "match", "case", "default", "switch",
                "class", "extends", "implements", "interface", "enum", "struct",
                "public", "private", "protected", "static", "final", "abstract",
                "try", "catch", "finally", "throw", "panic", "ensure",
                "import", "from", "namespace", "module", "package", "extern",
                "async", "await", "spawn", "parallel", "thread",
                "true", "false", "null", "and", "or", "not",
                "new", "self", "super", "lambda", "type",
                "output", "debug", "log", "pass",
                "move", "copy", "shared", "weak", "borrow", "drop", "delete",
                "safe", "unsafe", "const", "mutable", "reference", "pointer",
                "event", "signal", "emit", "callback",
                "scene", "entity", "sprite", "camera", "physics",
                "component", "render", "state", "properties",
                "server", "api", "route", "middleware",
                "ai", "tensor", "train", "predict",
                "int", "float", "string", "bool", "void", "list", "map", "set", "array", "json",
                "let", "var"
            };
            if (keywords.find(w) == keywords.end()) {
                if (var_defs.find(w) == var_defs.end()) {
                    /* Not defined yet, might be a use-before-def, but only flag if defined later */
                }
                var_uses[w] = (int)i;
                vars_all.insert(w);
            }
            start = m[0].second;
        }
    }

    /* ── Run aurorac parse-only check if available ── */
    {
        std::string aurorac_cmd;
#ifdef _WIN32
        aurorac_cmd = "where aurorac.exe >nul 2>nul && (echo AURORAC_AVAILABLE) || (echo AURORAC_NOT_FOUND)";
#else
        aurorac_cmd = "which aurorac >/dev/null 2>&1 && echo AURORAC_AVAILABLE || echo AURORAC_NOT_FOUND";
#endif
        std::string check_result;
#ifdef _WIN32
        FILE* pipe = _popen(aurorac_cmd.c_str(), "r");
#else
        FILE* pipe = popen(aurorac_cmd.c_str(), "r");
#endif
        if (pipe) {
            char buf[128];
            while (fgets(buf, sizeof(buf), pipe)) check_result += buf;
#ifdef _WIN32
            _pclose(pipe);
#else
            pclose(pipe);
#endif
        }
        if (check_result.find("AURORAC_AVAILABLE") != std::string::npos) {
            /* Write text to temp file and run aurorac in parse-only mode */
            std::string tmpfile = ".aura_lsp_tmp.aura";
            {
                std::ofstream tmp(tmpfile);
                tmp << text;
            }
            std::string parse_cmd = "aurorac --parse-only \"" + tmpfile + "\" 2>&1 || aurorac -fsyntax-only \"" + tmpfile + "\" 2>&1";
            std::string parse_out;
#ifdef _WIN32
            FILE* p2 = _popen(parse_cmd.c_str(), "r");
#else
            FILE* p2 = popen(parse_cmd.c_str(), "r");
#endif
            if (p2) {
                char buf[4096];
                while (fgets(buf, sizeof(buf), p2)) parse_out += buf;
#ifdef _WIN32
                _pclose(p2);
#else
                pclose(p2);
#endif
            }
            if (!parse_out.empty()) {
                /* Parse compiler output for line:col: message style errors */
                std::regex err_re(R"((\d+):(\d+):\s*(.*)|error:\s*(.*)|Error:\s*(.*))");
                std::smatch em;
                std::istringstream err_stream(parse_out);
                std::string err_line;
                while (std::getline(err_stream, err_line)) {
                    if (std::regex_search(err_line, em, err_re)) {
                        int err_line_num = 0;
                        if (em[1].matched) err_line_num = std::stoi(em[1]) - 1;
                        std::string msg = em[3].matched ? em[3].str() : (em[4].matched ? em[4].str() : em[5].str());
                        add_diag(diags, err_line_num, 0, err_line_num < (int)lines.size() ? (int)lines[err_line_num].length() : 1, 1, msg, "aurorac");
                    } else if (!err_line.empty()) {
                        /* Just in case, add line as diagnostic */
                        add_diag(diags, 0, 0, lines.empty() ? 1 : (int)lines[0].length(), 2, err_line, "aurorac");
                    }
                }
            }
            std::remove(tmpfile.c_str());
        }
    }

    /* ── Pass 2: line-by-line checks + multi-line structural checks ── */
    bool in_class = false;
    int class_start_line = -1;
    bool in_function = false;
    int func_start_line = -1;
    bool has_return = false;
    bool non_void_func = false;

    for (size_t i = 0; i < lines.size(); i++) {
        const std::string& ln = lines[i];
        std::smatch m;

        /* ── Unbalanced braces per line ── */
        int open_br = 0, close_br = 0;
        for (char c : ln) {
            if (c == '{') open_br++;
            if (c == '}') close_br++;
        }

        /* ── Extra space checks ── */
        if (ln.find("( ") != std::string::npos) {
            add_diag(diags, (int)i, 0, (int)ln.length(), 3, "Extra space after '('", "aurora-lint");
        }
        if (ln.find(" )") != std::string::npos && ln.find("()") == std::string::npos) {
            add_diag(diags, (int)i, 0, (int)ln.length(), 3, "Extra space before ')'", "aurora-lint");
        }

        /* ── Track function bodies for missing end function ── */
        if (std::regex_search(ln, m, func_def_re)) {
            if (in_function) {
                add_diag(diags, func_start_line, 0, (int)lines[func_start_line].length(), 1, "Missing 'end function' for previous function", "aurora-lint");
            }
            in_function = true;
            func_start_line = (int)i;
            has_return = false;
            non_void_func = ln.find(": void") == std::string::npos && ln.find("):") != std::string::npos;
        }
        if (in_function && ln.find("end function") != std::string::npos) {
            if (!has_return && non_void_func) {
                add_diag(diags, func_start_line, 0, (int)lines[func_start_line].length(), 2, "Function may be missing a 'return' statement", "aurora-lint");
            }
            in_function = false;
            func_start_line = -1;
            non_void_func = false;
        }
        if (std::regex_search(ln, m, return_re)) {
            has_return = true;
        }

        /* ── Track class bodies for missing end class ── */
        if (std::regex_search(ln, m, class_def_re)) {
            if (in_class) {
                add_diag(diags, class_start_line, 0, (int)lines[class_start_line].length(), 1, "Missing 'end class' for previous class", "aurora-lint");
            }
            in_class = true;
            class_start_line = (int)i;
        }
        if (in_class && ln.find("end class") != std::string::npos) {
            in_class = false;
            class_start_line = -1;
        }

        /* ── Type checking hints ── */
        if (std::regex_search(ln, m, var_def_re)) {
            std::string vname = m[1];
            size_t var_end = m.position(0) + m.length(0);
            std::string rest = ln.substr(var_end);
            /* Check if variable is declared as int but assigned a string literal */
            std::regex type_annotation_re(R"(([a-zA-Z_][a-zA-Z0-9_]*)\s*:\s*(int|float|string|bool)\s*=)");
            std::smatch tm;
            if (std::regex_search(ln, tm, type_annotation_re)) {
                std::string vn = tm[1];
                std::string vtype = tm[2];
                if (vtype == "int" || vtype == "float") {
                    if (std::regex_search(rest, string_lit_re)) {
                        add_diag(diags, (int)i, (int)tm.position(0), (int)ln.length(), 2, "Assigning string value to " + vtype + " variable '" + vn + "'", "aurora-lint");
                    }
                }
                if (vtype == "string") {
                    if (std::regex_search(rest, int_lit_re) && rest.find('"') == std::string::npos) {
                        add_diag(diags, (int)i, (int)tm.position(0), (int)ln.length(), 2, "Assigning numeric literal to string variable '" + vn + "'", "aurora-lint");
                    }
                }
            }
            /* Check output() argument type mismatches */
            if (std::regex_search(ln, output_re)) {
                size_t out_end = m.position(0) + m.length(0);
                std::string args_rest = ln.substr(out_end);
            }
        }

        /* ── Variables used before assignment ── */
        auto use_start = ln.cbegin();
        while (std::regex_search(use_start, ln.cend(), m, var_use_re)) {
            std::string w = m[1];
            static const std::set<std::string> kw = {
                "function", "return", "if", "else", "elseif", "while", "for", "loop",
                "break", "continue", "match", "case", "default", "switch",
                "class", "extends", "implements", "interface", "enum", "struct",
                "public", "private", "protected", "static", "final", "abstract",
                "try", "catch", "finally", "throw", "panic", "ensure",
                "import", "from", "namespace", "module", "package", "extern",
                "async", "await", "spawn", "parallel", "thread",
                "true", "false", "null", "and", "or", "not",
                "new", "self", "super", "lambda", "type",
                "output", "debug", "log", "pass",
                "move", "copy", "shared", "weak", "borrow", "drop", "delete",
                "safe", "unsafe", "const", "mutable", "reference", "pointer",
                "event", "signal", "emit", "callback",
                "scene", "entity", "sprite", "camera", "physics",
                "component", "render", "state", "properties",
                "server", "api", "route", "middleware",
                "ai", "tensor", "train", "predict",
                "int", "float", "string", "bool", "void", "list", "map", "set", "array", "json",
                "let", "var"
            };
            if (kw.find(w) == kw.end()) {
                /* Check if this use is actually in a definition pattern (var = ...) */
                bool is_def = false;
                std::smatch dm;
                if (std::regex_search(ln, dm, var_def_re) && dm[1] == w) {
                    is_def = true;
                }
                if (!is_def && var_defs.find(w) != var_defs.end() && var_defs[w] > (int)i) {
                    /* Used before defined */
                    add_diag(diags, (int)i, (int)m.position(0), (int)(m.position(0) + m.length(0)), 2, "Variable '" + w + "' used before assignment (defined at line " + std::to_string(var_defs[w] + 1) + ")", "aurora-lint");
                } else if (!is_def && var_defs.find(w) == var_defs.end() && vars_all.find(w) != vars_all.end()) {
                    /* Variable used but never defined in this file — could be global, just note */
                }
            }
            use_start = m[0].second;
        }
    }

    /* ── Final structural checks ── */
    if (in_function) {
        add_diag(diags, func_start_line, 0, (int)lines[func_start_line].length(), 1, "Missing 'end function' to close function", "aurora-lint");
    }
    if (in_class) {
        add_diag(diags, class_start_line, 0, (int)lines[class_start_line].length(), 1, "Missing 'end class' to close class", "aurora-lint");
    }

    /* ── Unused variables ── */
    for (auto& [vname, def_line] : var_defs) {
        if (var_uses.find(vname) == var_uses.end() || var_uses[vname] == def_line) {
            /* Variable defined but never used (or only used on its definition line) */
            bool used_elsewhere = false;
            for (auto& [un, ul] : var_uses) {
                if (un == vname && ul != def_line) {
                    used_elsewhere = true;
                    break;
                }
            }
            if (!used_elsewhere) {
                add_diag(diags, def_line, 0, (int)lines[def_line].length(), 3, "Unused variable '" + vname + "'", "aurora-lint");
            }
        }
    }

    return diags;
}

std::vector<LspCompletionItem> LspServer::get_completions(const std::string& text, int line, int col) {
    std::vector<LspCompletionItem> items;

    /* Add language keywords */
    for (auto& kw : get_keywords()) {
        LspCompletionItem ci;
        ci.label = kw;
        ci.kind = 14; /* keyword */
        ci.detail = "keyword";
        ci.insertText = kw;
        items.push_back(ci);
    }

    /* Add built-in functions */
    std::vector<std::pair<std::string, std::string>> builtins = {
        {"output", "Print value to stdout"},
        {"debug", "Print debug message"},
        {"log", "Log a message"},
        {"len", "Get length of array/string"},
        {"sum", "Sum of array elements"},
        {"min", "Minimum of array"},
        {"max", "Maximum of array"},
        {"range", "Create range of integers"},
        {"sleep", "Sleep for milliseconds"},
        {"time", "Get current timestamp"},
        {"random", "Get random number"},
        {"panic", "Panic with error message"},
        {"input", "Read line from stdin"},
        {"move", "Transfer ownership"},
        {"copy", "Copy value"},
        {"sizeof", "Size of type in bytes"},
        {"typeof", "Get type name as string"},
        {"convert", "Convert between types"},
    };
    for (auto& [name, desc] : builtins) {
        LspCompletionItem ci;
        ci.label = name;
        ci.kind = 3; /* function */
        ci.detail = desc;
        ci.documentation = desc;
        ci.insertText = name;
        items.push_back(ci);
    }

    /* Extract identifiers from open document */
    for (auto& [uri, content] : open_documents_) {
        std::istringstream stream(content);
        std::string ln;
        std::regex word_re("\\b([a-zA-Z_][a-zA-Z0-9_]*)\\b");
        std::set<std::string> seen;
        while (std::getline(stream, ln)) {
            std::smatch m;
            auto start = ln.cbegin();
            while (std::regex_search(start, ln.cend(), m, word_re)) {
                std::string w = m[1];
                if (w.size() > 1 && !std::count(get_keywords().begin(), get_keywords().end(), w)) {
                    if (seen.insert(w).second) {
                        LspCompletionItem ci;
                        ci.label = w;
                        ci.kind = 6; /* variable */
                        ci.detail = "identifier";
                        ci.insertText = w;
                        items.push_back(ci);
                    }
                }
                start = m[0].second;
            }
        }
    }

    /* Sort: keywords first, then alphabetically */
    std::sort(items.begin(), items.end(), [](auto& a, auto& b) {
        if (a.kind != b.kind) return a.kind < b.kind;
        return a.label < b.label;
    });

    return items;
}

std::vector<LspSymbol> LspServer::get_symbols(const std::string& text) {
    std::vector<LspSymbol> symbols;
    std::istringstream stream(text);
    std::string line;
    int line_num = 0;

    std::regex func_re("^\\s*function\\s+([a-zA-Z_][a-zA-Z0-9_]*)\\s*\\((.*)\\)");
    std::regex class_re("^\\s*class\\s+([a-zA-Z_][a-zA-Z0-9_]*)");
    std::regex struct_re("^\\s*struct\\s+([a-zA-Z_][a-zA-Z0-9_]*)");
    std::regex enum_re("^\\s*enum\\s+([a-zA-Z_][a-zA-Z0-9_]*)");
    std::regex interface_re("^\\s*interface\\s+([a-zA-Z_][a-zA-Z0-9_]*)");
    std::regex extern_re("^\\s*extern\\s+(?:\\\"[^\\\"]+\\\"\\s+)?function\\s+([a-zA-Z_][a-zA-Z0-9_]*)\\s*\\(");
    std::regex var_re("^\\s*([a-zA-Z_][a-zA-Z0-9_]*)\\s*=");

    while (std::getline(stream, line)) {
        std::smatch m;

        if (std::regex_search(line, m, func_re)) {
            symbols.push_back({m[1], "function", {{line_num, 0}, {line_num, (int)m[0].length()}}, {{line_num, 0}, {line_num, (int)m[0].length()}}});
        } else if (std::regex_search(line, m, class_re)) {
            symbols.push_back({m[1], "class", {{line_num, 0}, {line_num, (int)m[0].length()}}, {{line_num, 0}, {line_num, (int)m[0].length()}}});
        } else if (std::regex_search(line, m, struct_re)) {
            symbols.push_back({m[1], "struct", {{line_num, 0}, {line_num, (int)m[0].length()}}, {{line_num, 0}, {line_num, (int)m[0].length()}}});
        } else if (std::regex_search(line, m, enum_re)) {
            symbols.push_back({m[1], "enum", {{line_num, 0}, {line_num, (int)m[0].length()}}, {{line_num, 0}, {line_num, (int)m[0].length()}}});
        } else if (std::regex_search(line, m, interface_re)) {
            symbols.push_back({m[1], "interface", {{line_num, 0}, {line_num, (int)m[0].length()}}, {{line_num, 0}, {line_num, (int)m[0].length()}}});
        } else if (std::regex_search(line, m, extern_re)) {
            symbols.push_back({m[1], "function", {{line_num, 0}, {line_num, (int)m[0].length()}}, {{line_num, 0}, {line_num, (int)m[0].length()}}});
        } else if (std::regex_search(line, m, var_re)) {
            symbols.push_back({m[1], "variable", {{line_num, 0}, {line_num, (int)m[0].length()}}, {{line_num, 0}, {line_num, (int)m[0].length()}}});
        }

        line_num++;
    }

    return symbols;
}

void LspServer::update_diagnostics(const std::string& uri, const std::string& text) {
    auto diags = analyze(text);

    /* Build diagnostic JSON array */
    std::string diag_json = "[";
    for (size_t i = 0; i < diags.size(); i++) {
        auto& d = diags[i];
        JsonBuilder jb;
        JsonBuilder range;
        range.add_int("start", d.range.start.line);
        range.add_int("start", d.range.start.character);
        /* Nested position objects */
        std::string rng = "{\"start\":{\"line\":" + std::to_string(d.range.start.line) +
                          ",\"character\":" + std::to_string(d.range.start.character) + "}" +
                          ",\"end\":{\"line\":" + std::to_string(d.range.end.line) +
                          ",\"character\":" + std::to_string(d.range.end.character) + "}}";
        JsonBuilder diag_jb;
        diag_jb.add_raw("range", rng);
        diag_jb.add_int("severity", d.severity);
        diag_jb.add("message", escape_json(d.message));
        diag_jb.add("source", d.source);
        if (i > 0) diag_json += ",";
        diag_json += diag_jb.str();
    }
    diag_json += "]";

    /* Send textDocument/publishDiagnostics notification */
    JsonBuilder params;
    params.add("uri", uri);
    params.add_raw("diagnostics", diag_json);
    send_message(make_notification("textDocument/publishDiagnostics", params.str()));
}

/* ════════════════════════════════════════════════════════════
   LSP message handlers
   ════════════════════════════════════════════════════════════ */

void LspServer::handle_message(const std::string& json) {
    std::string method = get_json_field(json, "method");
    std::string id = get_json_field(json, "id");
    std::string params;
    auto pos = json.find("\"params\"");
    if (pos != std::string::npos) {
        /* Extract raw params object */
        auto colon = json.find(':', pos + 8);
        if (colon != std::string::npos) {
            /* Find matching braces */
            auto start = json.find_first_not_of(" \t\r\n", colon + 1);
            if (start != std::string::npos && json[start] == '{') {
                int depth = 1;
                size_t end = start + 1;
                while (end < json.size() && depth > 0) {
                    if (json[end] == '{') depth++;
                    else if (json[end] == '}') depth--;
                    end++;
                }
                params = json.substr(start, end - start);
            }
        }
    }

    std::string response;

    if (method == "initialize") {
        response = handle_initialize(params);
    } else if (method == "shutdown") {
        response = handle_shutdown();
    } else if (method == "textDocument/completion") {
        response = handle_completion(params);
    } else if (method == "textDocument/definition") {
        response = handle_definition(params);
    } else if (method == "textDocument/hover") {
        response = handle_hover(params);
    } else if (method == "textDocument/references") {
        response = handle_references(params);
    } else if (method == "textDocument/documentSymbol") {
        response = handle_document_symbol(params);
    } else if (method == "textDocument/didOpen") {
        handle_did_open(params);
        return; /* No response for notifications */
    } else if (method == "textDocument/didChange") {
        handle_did_change(params);
        return;
    } else if (method == "textDocument/didSave") {
        handle_did_save(params);
        return;
    } else if (method == "textDocument/didClose") {
        handle_did_close(params);
        return;
    } else if (method == "initialized") {
        return; /* No response needed */
    } else {
        /* Unknown method — return null */
        if (!id.empty())
            response = make_response(id, "null");
        else
            return;
    }

    if (!id.empty() && !response.empty())
        send_message(response);
}

std::string LspServer::handle_initialize(const std::string& params) {
    initialized_ = true;

    /* Extract rootUri */
    auto pos = params.find("\"rootUri\"");
    if (pos != std::string::npos) {
        auto colon = params.find(':', pos + 9);
        auto start = params.find('"', colon + 1);
        auto end = params.find('"', start + 1);
        if (start != std::string::npos && end != std::string::npos)
            workspace_root_ = params.substr(start + 1, end - start - 1);
    }

    /* Capabilities */
    std::string caps = R"({
        "textDocumentSync":2,
        "completionProvider":{"triggerCharacters":[".","@"]},
        "definitionProvider":true,
        "hoverProvider":true,
        "referencesProvider":true,
        "documentSymbolProvider":true
    })";

    JsonBuilder result;
    result.add_raw("capabilities", caps);
    result.add("serverInfo", "{\"name\":\"aurora-lsp\",\"version\":\"0.1.0\"}");
    return make_response(get_json_field(params, "id"), result.str());
}

std::string LspServer::handle_shutdown() {
    return make_response("null", "null");
}

std::string LspServer::handle_completion(const std::string& params) {
    /* Extract position */
    int line = 0, col = 0;
    std::smatch m;
    std::regex line_re("\"line\"\\s*:\\s*(\\d+)");
    std::regex col_re("\"character\"\\s*:\\s*(\\d+)");
    if (std::regex_search(params, m, line_re)) line = std::stoi(m[1]);
    if (std::regex_search(params, m, col_re)) col = std::stoi(m[1]);

    /* Extract URI */
    std::string uri;
    std::regex uri_re("\"uri\"\\s*:\\s*\"([^\"]+)\"");
    if (std::regex_search(params, m, uri_re)) uri = m[1];

    std::string text = open_documents_[uri];
    auto items = get_completions(text, line, col);

    std::string items_json = "[";
    for (size_t i = 0; i < items.size(); i++) {
        auto& ci = items[i];
        if (i > 0) items_json += ",";
        JsonBuilder jb;
        jb.add("label", escape_json(ci.label));
        jb.add_int("kind", ci.kind);
        jb.add("detail", escape_json(ci.detail));
        if (!ci.documentation.empty())
            jb.add("documentation", escape_json(ci.documentation));
        if (!ci.insertText.empty())
            jb.add("insertText", escape_json(ci.insertText));
        items_json += jb.str();
    }
    items_json += "]";

    JsonBuilder result;
    result.add_bool("isIncomplete", false);
    result.add_raw("items", items_json);
    return make_response(get_json_field(params, "id"), result.str());
}

std::string LspServer::handle_definition(const std::string& params) {
    std::string uri;
    std::smatch m;
    std::regex uri_re("\"uri\"\\s*:\\s*\"([^\"]+)\"");
    if (std::regex_search(params, m, uri_re)) uri = m[1];

    int line = 0, col = 0;
    std::regex line_re("\"line\"\\s*:\\s*(\\d+)");
    std::regex col_re("\"character\"\\s*:\\s*(\\d+)");
    if (std::regex_search(params, m, line_re)) line = std::stoi(m[1]);
    if (std::regex_search(params, m, col_re)) col = std::stoi(m[1]);

    /* Get word under cursor */
    std::string text = open_documents_[uri];
    std::string word;
    {
        std::istringstream stream(text);
        std::string ln;
        int cur_line = 0;
        while (cur_line < line && std::getline(stream, ln)) cur_line++;
        if (cur_line == line) {
            int start = col;
            while (start > 0 && (isalnum(ln[start-1]) || ln[start-1] == '_')) start--;
            int end = col;
            while (end < (int)ln.size() && (isalnum(ln[end]) || ln[end] == '_')) end++;
            if (start < end) word = ln.substr(start, end - start);
        }
    }

    if (!word.empty()) {
        /* Search for definition across all open documents */
        /* Match patterns: "word =", "function word(", "class word", "word =" */
        std::regex def_re(R"((?:^|\s+)(?:function\s+|class\s+|struct\s+|enum\s+|interface\s+|extern\s+(?:\"[^\"]*\"\s+)?function\s+)?)" + word +
                          R"(\s*(?:\(|=|:))");
        for (auto& [doc_uri, doc_text] : open_documents_) {
            std::istringstream stream(doc_text);
            std::string ln;
            int ln_num = 0;
            while (std::getline(stream, ln)) {
                std::smatch dm;
                if (std::regex_search(ln, dm, def_re)) {
                    JsonBuilder jb;
                    jb.add("uri", escape_json(doc_uri));
                    std::string rng = "{\"start\":{\"line\":" + std::to_string(ln_num) +
                                      ",\"character\":0}" +
                                      ",\"end\":{\"line\":" + std::to_string(ln_num) +
                                      ",\"character\":" + std::to_string((int)ln.length()) + "}}";
                    jb.add_raw("range", rng);
                    return make_response(get_json_field(params, "id"), jb.str());
                }
                ln_num++;
            }
        }
    }

    return make_response(get_json_field(params, "id"), "null");
}

std::string LspServer::handle_hover(const std::string& params) {
    std::string uri;
    std::smatch m;
    std::regex uri_re("\"uri\"\\s*:\\s*\"([^\"]+)\"");
    if (std::regex_search(params, m, uri_re)) uri = m[1];

    int line = 0, col = 0;
    std::regex line_re("\"line\"\\s*:\\s*(\\d+)");
    std::regex col_re("\"character\"\\s*:\\s*(\\d+)");
    if (std::regex_search(params, m, line_re)) line = std::stoi(m[1]);
    if (std::regex_search(params, m, col_re)) col = std::stoi(m[1]);

    /* Get word under cursor */
    std::string text = open_documents_[uri];
    std::istringstream stream(text);
    std::string ln;
    int cur_line = 0;
    while (cur_line < line && std::getline(stream, ln)) cur_line++;
    if (cur_line == line) {
        /* Extract word at col */
        std::string word;
        int start = col;
        while (start > 0 && (isalnum(ln[start-1]) || ln[start-1] == '_')) start--;
        int end = col;
        while (end < (int)ln.size() && (isalnum(ln[end]) || ln[end] == '_')) end++;
        if (start < end) word = ln.substr(start, end - start);

        if (!word.empty()) {
            JsonBuilder jb;
            jb.add("contents", "```\n" + word + "\n```");
            return make_response(get_json_field(params, "id"), jb.str());
        }
    }

    return make_response(get_json_field(params, "id"), "{\"contents\":[]}");
}

std::string LspServer::handle_references(const std::string& params) {
    std::string uri;
    std::smatch m;
    std::regex uri_re("\"uri\"\\s*:\\s*\"([^\"]+)\"");
    if (std::regex_search(params, m, uri_re)) uri = m[1];

    int line = 0, col = 0;
    std::regex line_re("\"line\"\\s*:\\s*(\\d+)");
    std::regex col_re("\"character\"\\s*:\\s*(\\d+)");
    if (std::regex_search(params, m, line_re)) line = std::stoi(m[1]);
    if (std::regex_search(params, m, col_re)) col = std::stoi(m[1]);

    /* Get word under cursor */
    std::string text = open_documents_[uri];
    std::string word;
    {
        std::istringstream stream(text);
        std::string ln;
        int cur_line = 0;
        while (cur_line < line && std::getline(stream, ln)) cur_line++;
        if (cur_line == line) {
            int start = col;
            while (start > 0 && (isalnum(ln[start-1]) || ln[start-1] == '_')) start--;
            int end = col;
            while (end < (int)ln.size() && (isalnum(ln[end]) || ln[end] == '_')) end++;
            if (start < end) word = ln.substr(start, end - start);
        }
    }

    std::string refs_json = "[";
    bool first = true;
    if (!word.empty()) {
        std::regex word_re("\\b" + word + "\\b");
        for (auto& [doc_uri, doc_text] : open_documents_) {
            std::istringstream stream(doc_text);
            std::string ln;
            int ln_num = 0;
            while (std::getline(stream, ln)) {
                std::smatch rm;
                std::string::const_iterator search_start = ln.cbegin();
                while (std::regex_search(search_start, ln.cend(), rm, word_re)) {
                    int char_start = (int)(rm.position());
                    int char_end   = char_start + (int)rm.length();
                    if (!first) refs_json += ",";
                    first = false;
                    JsonBuilder jb;
                    jb.add("uri", escape_json(doc_uri));
                    std::string rng = "{\"start\":{\"line\":" + std::to_string(ln_num) +
                                      ",\"character\":" + std::to_string(char_start) + "}" +
                                      ",\"end\":{\"line\":" + std::to_string(ln_num) +
                                      ",\"character\":" + std::to_string(char_end) + "}}";
                    jb.add_raw("range", rng);
                    refs_json += jb.str();
                    search_start = rm[0].second;
                }
                ln_num++;
            }
        }
    }
    refs_json += "]";

    return make_response(get_json_field(params, "id"), refs_json);
}

std::string LspServer::handle_document_symbol(const std::string& params) {
    std::string uri;
    std::smatch m;
    std::regex uri_re("\"uri\"\\s*:\\s*\"([^\"]+)\"");
    if (std::regex_search(params, m, uri_re)) uri = m[1];

    std::string text = open_documents_[uri];
    auto symbols = get_symbols(text);

    std::string sym_json = "[";
    for (size_t i = 0; i < symbols.size(); i++) {
        auto& s = symbols[i];
        if (i > 0) sym_json += ",";
        JsonBuilder jb;
        jb.add("name", escape_json(s.name));
        jb.add("kind", s.kind);
        std::string rng = "{\"start\":{\"line\":" + std::to_string(s.range.start.line) +
                          ",\"character\":" + std::to_string(s.range.start.character) + "}" +
                          ",\"end\":{\"line\":" + std::to_string(s.range.end.line) +
                          ",\"character\":" + std::to_string(s.range.end.character) + "}}";
        jb.add_raw("range", rng);
        jb.add_raw("selectionRange", rng);
        sym_json += jb.str();
    }
    sym_json += "]";

    return make_response(get_json_field(params, "id"), sym_json);
}

void LspServer::handle_did_open(const std::string& params) {
    std::regex uri_re("\"uri\"\\s*:\\s*\"([^\"]+)\"");
    std::regex text_re("\"text\"\\s*:\\s*\"([^\"]+)\"");
    std::smatch m;
    std::string uri, text;

    if (std::regex_search(params, m, uri_re)) uri = m[1];
    if (std::regex_search(params, m, text_re)) text = m[1];

    if (!uri.empty()) {
        open_documents_[uri] = text;
        update_diagnostics(uri, text);
    }
}

void LspServer::handle_did_change(const std::string& params) {
    std::regex uri_re("\"uri\"\\s*:\\s*\"([^\"]+)\"");
    std::smatch m;
    std::string uri;

    if (std::regex_search(params, m, uri_re)) uri = m[1];

    if (!uri.empty() && open_documents_.count(uri)) {
        /* Find the full text in the params */
        auto pos = params.find("\"text\"");
        if (pos != std::string::npos) {
            auto start = params.find('"', pos + 7);
            if (start != std::string::npos) {
                start++;
                auto end = params.find('"', start);
                if (end != std::string::npos) {
                    open_documents_[uri] = params.substr(start, end - start);
                    /* For simplicity, use the entire params as text if extraction fails */
                }
            }
        }
        update_diagnostics(uri, open_documents_[uri]);
    }
}

void LspServer::handle_did_save(const std::string& params) {
    std::regex uri_re("\"uri\"\\s*:\\s*\"([^\"]+)\"");
    std::smatch m;
    std::string uri;
    if (std::regex_search(params, m, uri_re)) uri = m[1];
    if (!uri.empty() && open_documents_.count(uri))
        update_diagnostics(uri, open_documents_[uri]);
}

void LspServer::handle_did_close(const std::string& params) {
    std::regex uri_re("\"uri\"\\s*:\\s*\"([^\"]+)\"");
    std::smatch m;
    std::string uri;
    if (std::regex_search(params, m, uri_re)) uri = m[1];
    if (!uri.empty()) open_documents_.erase(uri);
}

void LspServer::run() {
    /* Send initialized notification on startup */
    while (true) {
        std::string msg = read_message();
        if (msg.empty()) break;
        handle_message(msg);
    }
}

/* ════════════════════════════════════════════════════════════
   main
   ════════════════════════════════════════════════════════════ */

int main() {
    LspServer server;
    server.run();
    return 0;
}
