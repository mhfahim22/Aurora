#include "../../include/tools/lsp.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <set>
#include <regex>
#include <cstdio>
#include <cstdlib>

/* ── Analysis helper ── */

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
    std::map<std::string, int> var_defs;
    std::map<std::string, int> var_uses;
    std::set<std::string> vars_all;

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
                bool is_def = false;
                std::smatch dm;
                if (std::regex_search(ln, dm, var_def_re) && dm[1] == w) {
                    is_def = true;
                }
                if (!is_def && var_defs.find(w) != var_defs.end() && var_defs[w] > (int)i) {
                    add_diag(diags, (int)i, (int)m.position(0), (int)(m.position(0) + m.length(0)), 2, "Variable '" + w + "' used before assignment (defined at line " + std::to_string(var_defs[w] + 1) + ")", "aurora-lint");
                } else if (!is_def && var_defs.find(w) == var_defs.end() && vars_all.find(w) != vars_all.end()) {
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

    for (auto& kw : get_keywords()) {
        LspCompletionItem ci;
        ci.label = kw;
        ci.kind = 14;
        ci.detail = "keyword";
        ci.insertText = kw;
        items.push_back(ci);
    }

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
        ci.kind = 3;
        ci.detail = desc;
        ci.documentation = desc;
        ci.insertText = name;
        items.push_back(ci);
    }

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
                        ci.kind = 6;
                        ci.detail = "identifier";
                        ci.insertText = w;
                        items.push_back(ci);
                    }
                }
                start = m[0].second;
            }
        }
    }

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

    JsonValue diag_arr;
    diag_arr.type = JsonValue::Array;
    for (auto& d : diags) {
        JsonValue diag;
        diag.type = JsonValue::Object;

        JsonValue range;
        range.type = JsonValue::Object;
        JsonValue start, end;
        start.type = JsonValue::Object;
        start.members["line"] = JsonValue();
        start.members["line"].type = JsonValue::Number;
        start.members["line"].num = d.range.start.line;
        start.members["character"] = JsonValue();
        start.members["character"].type = JsonValue::Number;
        start.members["character"].num = d.range.start.character;
        end.type = JsonValue::Object;
        end.members["line"] = JsonValue();
        end.members["line"].type = JsonValue::Number;
        end.members["line"].num = d.range.end.line;
        end.members["character"] = JsonValue();
        end.members["character"].type = JsonValue::Number;
        end.members["character"].num = d.range.end.character;
        range.members["start"] = start;
        range.members["end"] = end;
        diag.members["range"] = range;

        JsonValue sev;
        sev.type = JsonValue::Number;
        sev.num = d.severity;
        diag.members["severity"] = sev;

        JsonValue msg_val;
        msg_val.type = JsonValue::StringVal;
        msg_val.str = d.message;
        diag.members["message"] = msg_val;

        JsonValue src_val;
        src_val.type = JsonValue::StringVal;
        src_val.str = d.source;
        diag.members["source"] = src_val;

        diag_arr.items.push_back(diag);
    }

    std::string diag_json = JsonParser::stringify(diag_arr);

    JsonValue notif_params;
    notif_params.type = JsonValue::Object;
    JsonValue uri_val;
    uri_val.type = JsonValue::StringVal;
    uri_val.str = uri;
    notif_params.members["uri"] = uri_val;
    notif_params.members["diagnostics"] = diag_arr;

    std::string params_str = JsonParser::stringify(notif_params);
    send_message(make_notification("textDocument/publishDiagnostics", params_str));
}
