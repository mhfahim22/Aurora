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
#include <cstdlib>
#include <cmath>

/* ════════════════════════════════════════════════════════════
   JsonValue implementation
   ════════════════════════════════════════════════════════════ */

JsonValue JsonValue::get(const std::string& key) const {
    if (type == Object) {
        auto it = members.find(key);
        if (it != members.end()) return it->second;
    }
    return {};
}

JsonValue JsonValue::get(int index) const {
    if (type == Array && index >= 0 && index < (int)items.size())
        return items[index];
    return {};
}

std::string JsonValue::as_string(const std::string& def) const {
    if (type == StringVal) return str;
    if (type == Number) return std::to_string((long long)num);
    return def;
}

int JsonValue::as_int(int def) const {
    return type == Number ? (int)num : def;
}

bool JsonValue::as_bool(bool def) const {
    return type == Bool ? boolean : def;
}

bool JsonValue::has(const std::string& key) const {
    return type == Object && members.find(key) != members.end();
}

/* ════════════════════════════════════════════════════════════
   Recursive-descent JSON parser
   ════════════════════════════════════════════════════════════ */

void JsonParser::skip_ws() {
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' ||
                              s[pos] == '\n' || s[pos] == '\r'))
        pos++;
}

JsonValue JsonParser::parse_value() {
    skip_ws();
    if (pos >= s.size()) return {};
    if (s[pos] == '"') return parse_string();
    if (s[pos] == '{') return parse_object();
    if (s[pos] == '[') return parse_array();
    if (s[pos] == 't' || s[pos] == 'f' || s[pos] == 'n')
        return parse_bool_or_null();
    return parse_number();
}

JsonValue JsonParser::parse_string() {
    JsonValue v;
    v.type = JsonValue::StringVal;
    pos++; /* skip opening " */
    while (pos < s.size() && s[pos] != '"') {
        if (s[pos] == '\\') {
            pos++;
            if (pos >= s.size()) break;
            switch (s[pos]) {
                case '"': v.str += '"'; break;
                case '\\': v.str += '\\'; break;
                case '/': v.str += '/'; break;
                case 'b': v.str += '\b'; break;
                case 'f': v.str += '\f'; break;
                case 'n': v.str += '\n'; break;
                case 'r': v.str += '\r'; break;
                case 't': v.str += '\t'; break;
                default: v.str += s[pos]; break;
            }
            pos++;
        } else {
            v.str += s[pos++];
        }
    }
    if (pos < s.size()) pos++; /* skip closing " */
    return v;
}

JsonValue JsonParser::parse_number() {
    JsonValue v;
    v.type = JsonValue::Number;
    size_t start = pos;
    if (pos < s.size() && s[pos] == '-') pos++;
    while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') pos++;
    if (pos < s.size() && s[pos] == '.') {
        pos++;
        while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') pos++;
    }
    if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) {
        pos++;
        if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) pos++;
        while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') pos++;
    }
    v.num = std::strtod(s.substr(start, pos - start).c_str(), nullptr);
    return v;
}

JsonValue JsonParser::parse_object() {
    JsonValue v;
    v.type = JsonValue::Object;
    pos++; /* skip { */
    skip_ws();
    if (pos < s.size() && s[pos] == '}') { pos++; return v; }
    while (pos < s.size()) {
        skip_ws();
        JsonValue key = parse_string();
        skip_ws();
        if (pos < s.size() && s[pos] == ':') pos++;
        JsonValue val = parse_value();
        v.members[key.str] = val;
        skip_ws();
        if (pos < s.size() && s[pos] == '}') { pos++; return v; }
        if (pos < s.size() && s[pos] == ',') { pos++; }
    }
    return v;
}

JsonValue JsonParser::parse_array() {
    JsonValue v;
    v.type = JsonValue::Array;
    pos++; /* skip [ */
    skip_ws();
    if (pos < s.size() && s[pos] == ']') { pos++; return v; }
    while (pos < s.size()) {
        v.items.push_back(parse_value());
        skip_ws();
        if (pos < s.size() && s[pos] == ']') { pos++; return v; }
        if (pos < s.size() && s[pos] == ',') { pos++; }
    }
    return v;
}

JsonValue JsonParser::parse_bool_or_null() {
    JsonValue v;
    if (s.substr(pos, 4) == "true") {
        v.type = JsonValue::Bool; v.boolean = true; pos += 4;
    } else if (s.substr(pos, 5) == "false") {
        v.type = JsonValue::Bool; v.boolean = false; pos += 5;
    } else if (s.substr(pos, 4) == "null") {
        pos += 4;
    }
    return v;
}

JsonValue JsonParser::parse(const std::string& json) {
    JsonParser p(json);
    return p.parse_value();
}

static void json_stringify_value(std::ostringstream& out, const JsonValue& v) {
    switch (v.type) {
        case JsonValue::Null: out << "null"; break;
        case JsonValue::Bool: out << (v.boolean ? "true" : "false"); break;
        case JsonValue::Number: out << v.num; break;
        case JsonValue::StringVal:
            out << '"';
            for (char c : v.str) {
                switch (c) {
                    case '"': out << "\\\""; break;
                    case '\\': out << "\\\\"; break;
                    case '\n': out << "\\n"; break;
                    case '\r': out << "\\r"; break;
                    case '\t': out << "\\t"; break;
                    default: out << c;
                }
            }
            out << '"';
            break;
        case JsonValue::Array:
            out << '[';
            for (size_t i = 0; i < v.items.size(); i++) {
                if (i > 0) out << ',';
                json_stringify_value(out, v.items[i]);
            }
            out << ']';
            break;
        case JsonValue::Object:
            out << '{';
            for (auto it = v.members.begin(); it != v.members.end(); ++it) {
                if (it != v.members.begin()) out << ',';
                out << '"' << it->first << '"' << ':';
                json_stringify_value(out, it->second);
            }
            out << '}';
            break;
    }
}

std::string JsonParser::stringify(const JsonValue& v) {
    std::ostringstream out;
    json_stringify_value(out, v);
    return out.str();
}

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

    /* Build diagnostic JSON array using JsonValue */
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

    /* Send textDocument/publishDiagnostics notification */
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

/* ════════════════════════════════════════════════════════════
   LSP message handlers
   ════════════════════════════════════════════════════════════ */

void LspServer::handle_message(const std::string& json) {
    JsonValue msg = JsonParser::parse(json);
    std::string method = msg.get("method").as_string();
    current_msg_id_ = msg.get("id").as_string();
    JsonValue params = msg.get("params");

    std::string response;

    if (method == "initialize") {
        response = handle_initialize(JsonParser::stringify(params));
    } else if (method == "shutdown") {
        response = handle_shutdown();
    } else if (method == "exit") {
        shutdown_ = true;
        return;
    } else if (method == "textDocument/completion") {
        response = handle_completion(JsonParser::stringify(params));
    } else if (method == "textDocument/definition") {
        response = handle_definition(JsonParser::stringify(params));
    } else if (method == "textDocument/hover") {
        response = handle_hover(JsonParser::stringify(params));
    } else if (method == "textDocument/references") {
        response = handle_references(JsonParser::stringify(params));
    } else if (method == "textDocument/documentSymbol") {
        response = handle_document_symbol(JsonParser::stringify(params));
    } else if (method == "textDocument/signatureHelp") {
        response = handle_signature_help(JsonParser::stringify(params));
    } else if (method == "textDocument/didOpen") {
        handle_did_open(JsonParser::stringify(params));
        return;
    } else if (method == "textDocument/didChange") {
        handle_did_change(JsonParser::stringify(params));
        return;
    } else if (method == "textDocument/didSave") {
        handle_did_save(JsonParser::stringify(params));
        return;
    } else if (method == "textDocument/didClose") {
        handle_did_close(JsonParser::stringify(params));
        return;
    } else if (method == "initialized") {
        return;
    } else {
        if (!current_msg_id_.empty())
            response = make_response(current_msg_id_, "null");
        else
            return;
    }

    if (!current_msg_id_.empty() && !response.empty())
        send_message(response);
}

std::string LspServer::handle_initialize(const std::string& params) {
    initialized_ = true;

    JsonValue p = JsonParser::parse(params);
    workspace_root_ = p.get("rootUri").as_string();

    /* Capabilities — use object-style textDocumentSync per spec */
    std::string caps = R"({
        "textDocumentSync":{"openClose":true,"change":2,"save":true},
        "completionProvider":{"triggerCharacters":[".","@"]},
        "definitionProvider":true,
        "hoverProvider":true,
        "referencesProvider":true,
        "documentSymbolProvider":true,
        "signatureHelpProvider":{"triggerCharacters":["("]}
    })";

    JsonBuilder result;
    result.add_raw("capabilities", caps);
    JsonBuilder info;
    info.add("name", "aurora-lsp");
    info.add("version", "0.1.0");
    result.add_raw("serverInfo", info.str());
    return make_response(current_msg_id_, result.str());
}

std::string LspServer::handle_shutdown() {
    shutdown_ = true;
    return make_response(current_msg_id_, "null");
}

std::string LspServer::handle_completion(const std::string& params) {
    JsonValue p = JsonParser::parse(params);
    std::string uri = p.get("textDocument").get("uri").as_string();
    int line = p.get("position").get("line").as_int();
    int col = p.get("position").get("character").as_int();

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
    return make_response(current_msg_id_, result.str());
}

std::string LspServer::handle_definition(const std::string& params) {
    JsonValue p = JsonParser::parse(params);
    std::string uri = p.get("textDocument").get("uri").as_string();
    int line = p.get("position").get("line").as_int();
    int col = p.get("position").get("character").as_int();

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
        std::regex def_re(R"((?:^|\s+)(?:function\s+|class\s+|struct\s+|enum\s+|interface\s+|extern\s+(?:\"[^\"]*\"\s+)?function\s+)?)" + word +
                          R"(\s*(?:\(|=|:))");
        for (auto& [doc_uri, doc_text] : open_documents_) {
            std::istringstream ds(doc_text);
            std::string ln;
            int ln_num = 0;
            while (std::getline(ds, ln)) {
                std::smatch dm;
                if (std::regex_search(ln, dm, def_re)) {
                    JsonBuilder jb;
                    jb.add("uri", escape_json(doc_uri));
                    std::string rng = "{\"start\":{\"line\":" + std::to_string(ln_num) +
                                      ",\"character\":0}" +
                                      ",\"end\":{\"line\":" + std::to_string(ln_num) +
                                      ",\"character\":" + std::to_string((int)ln.length()) + "}}";
                    jb.add_raw("range", rng);
                    return make_response(current_msg_id_, jb.str());
                }
                ln_num++;
            }
        }
    }

    return make_response(current_msg_id_, "null");
}

std::string LspServer::handle_hover(const std::string& params) {
    JsonValue p = JsonParser::parse(params);
    std::string uri = p.get("textDocument").get("uri").as_string();
    int line = p.get("position").get("line").as_int();
    int col = p.get("position").get("character").as_int();

    /* Get word under cursor */
    std::string text = open_documents_[uri];
    std::istringstream stream(text);
    std::string ln;
    int cur_line = 0;
    while (cur_line < line && std::getline(stream, ln)) cur_line++;
    if (cur_line == line) {
        std::string word;
        int start = col;
        while (start > 0 && (isalnum(ln[start-1]) || ln[start-1] == '_')) start--;
        int end = col;
        while (end < (int)ln.size() && (isalnum(ln[end]) || ln[end] == '_')) end++;
        if (start < end) word = ln.substr(start, end - start);

        if (!word.empty()) {
            std::string hover_text = "```\n" + word + "\n```";

            /* Look up word as a function definition */
            std::regex func_def_re(R"((?:^|\s*)function\s+)" + word +
                                   R"(\s*\(([^)]*)\)\s*(?::\s*(\w+))?)");
            for (auto& [doc_uri, doc_text] : open_documents_) {
                std::istringstream ds(doc_text);
                std::string dl;
                int dln = 0;
                while (std::getline(ds, dl)) {
                    std::smatch fm;
                    if (std::regex_search(dl, fm, func_def_re)) {
                        std::string sig = "function " + word + "(" + fm[1].str() + ")";
                        if (fm[2].matched) sig += ": " + fm[2].str();
                        hover_text += "\n---\n" + sig + "\n*Defined at " + doc_uri + ":" + std::to_string(dln + 1) + "*";
                        break;
                    }
                    dln++;
                }
                if (hover_text.find("*Defined") != std::string::npos) break;
            }

            /* Look up as variable definition */
            if (hover_text.find("*Defined") == std::string::npos) {
                std::regex var_def_re(R"((?:^|\s*)(?:let\s+|var\s+|const\s+)?)" +
                                      word + R"(\s*(?::\s*\w+)?\s*=[^=])");
                for (auto& [doc_uri, doc_text] : open_documents_) {
                    std::istringstream ds(doc_text);
                    std::string dl;
                    int dln = 0;
                    while (std::getline(ds, dl)) {
                        std::smatch vm;
                        if (std::regex_search(dl, vm, var_def_re)) {
                            hover_text += "\n---\n*Variable defined at " + doc_uri + ":" + std::to_string(dln + 1) + "*";
                            break;
                        }
                        dln++;
                    }
                    if (hover_text.find("*Variable defined") != std::string::npos) break;
                }
            }

            JsonBuilder jb;
            jb.add("contents", escape_json(hover_text));
            return make_response(current_msg_id_, jb.str());
        }
    }

    return make_response(current_msg_id_, "{\"contents\":[]}");
}

std::string LspServer::handle_references(const std::string& params) {
    JsonValue p = JsonParser::parse(params);
    std::string uri = p.get("textDocument").get("uri").as_string();
    int line = p.get("position").get("line").as_int();
    int col = p.get("position").get("character").as_int();

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
            std::istringstream ds(doc_text);
            std::string ln;
            int ln_num = 0;
            while (std::getline(ds, ln)) {
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

    return make_response(current_msg_id_, refs_json);
}

std::string LspServer::handle_document_symbol(const std::string& params) {
    JsonValue p = JsonParser::parse(params);
    std::string uri = p.get("textDocument").get("uri").as_string();

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

    return make_response(current_msg_id_, sym_json);
}

std::string LspServer::handle_signature_help(const std::string& params) {
    JsonValue p = JsonParser::parse(params);
    std::string uri = p.get("textDocument").get("uri").as_string();
    int line = p.get("position").get("line").as_int();
    int col = p.get("position").get("character").as_int();

    std::string text = open_documents_[uri];
    std::istringstream stream(text);
    std::string ln;
    int cur_line = 0;
    while (cur_line < line && std::getline(stream, ln)) cur_line++;

    if (cur_line != line) {
        return make_response(current_msg_id_, "null");
    }

    /* Find the function name before the cursor — look backwards for '(' */
    int paren = (int)ln.rfind('(', col);
    if (paren == std::string::npos) {
        return make_response(current_msg_id_, "null");
    }

    /* Extract function name before '(' */
    int name_end = paren;
    int name_start = name_end;
    while (name_start > 0 && (isalnum(ln[name_start-1]) || ln[name_start-1] == '_'))
        name_start--;

    std::string func_name = ln.substr(name_start, name_end - name_start);

    if (func_name.empty() || func_name == " ") {
        return make_response(current_msg_id_, "null");
    }

    /* Count how many commas before this paren to determine active parameter */
    /* First, find the matching opening paren for this closing paren if nested */
    /* Start from the beginning of this call's argument list */
    int active_param = 0;
    int depth = 0;
    bool found_start = false;
    for (int i = paren + 1; i < col; i++) {
        if (i >= (int)ln.size()) break;
        if (ln[i] == '(') { depth++; found_start = true; }
        else if (ln[i] == ')') { depth--; }
        else if (ln[i] == ',' && depth == 0) { active_param++; }
    }

    /* Search for function definition across open documents */
    std::regex func_re(R"((?:^|\s*)function\s+)" + func_name +
                       R"(\s*\(([^)]*)\)\s*(?::\s*(\w+))?)");

    for (auto& [doc_uri, doc_text] : open_documents_) {
        std::istringstream ds(doc_text);
        std::string dl;
        int dln = 0;
        while (std::getline(ds, dl)) {
            std::smatch fm;
            if (std::regex_search(dl, fm, func_re)) {
                std::string params_str = fm[1].str();
                std::string return_type = fm[2].matched ? fm[2].str() : "";

                /* Parse individual parameters */
                std::vector<std::string> param_names;
                std::vector<std::string> param_types;
                std::istringstream ps(params_str);
                std::string param;
                while (std::getline(ps, param, ',')) {
                    /* Trim */
                    size_t s = param.find_first_not_of(" ");
                    size_t e = param.find_last_not_of(" ");
                    if (s != std::string::npos && e != std::string::npos)
                        param = param.substr(s, e - s + 1);
                    else
                        param.clear();

                    /* Split on ':' to get name and type */
                    auto colon_pos = param.find(':');
                    if (colon_pos != std::string::npos) {
                        param_names.push_back(param.substr(0, colon_pos));
                        // trim type
                        std::string ptype = param.substr(colon_pos + 1);
                        size_t ts = ptype.find_first_not_of(" ");
                        if (ts != std::string::npos) ptype = ptype.substr(ts);
                        param_types.push_back(ptype);
                    } else {
                        param_names.push_back(param);
                        param_types.push_back("");
                    }
                }

                /* Build signature information */
                std::string label = func_name + "(";
                for (size_t i = 0; i < param_names.size(); i++) {
                    if (i > 0) label += ", ";
                    label += param_names[i];
                    if (!param_types[i].empty()) label += ": " + param_types[i];
                }
                label += ")";
                if (!return_type.empty()) label += ": " + return_type;

                std::string sig_json = R"({
                    "signatures":[{
                        "label":")" + escape_json(label) + R"(",
                        "parameters":[)";

                for (size_t i = 0; i < param_names.size(); i++) {
                    if (i > 0) sig_json += ",";
                    std::string plabel = param_names[i];
                    if (!param_types[i].empty()) plabel += ": " + param_types[i];
                    sig_json += R"({"label":")" + escape_json(plabel) + R"("})";
                }
                sig_json += R"(],
                        "activeParameter":)" + std::to_string(active_param) + R"(
                    }],
                    "activeSignature":0
                })";

                return make_response(current_msg_id_, sig_json);
            }
            dln++;
        }
    }

    return make_response(current_msg_id_, "null");
}

void LspServer::handle_did_open(const std::string& params) {
    JsonValue p = JsonParser::parse(params);
    std::string uri = p.get("textDocument").get("uri").as_string();
    std::string text = p.get("textDocument").get("text").as_string();

    if (!uri.empty()) {
        open_documents_[uri] = text;
        update_diagnostics(uri, text);
    }
}

void LspServer::handle_did_change(const std::string& params) {
    JsonValue p = JsonParser::parse(params);
    std::string uri = p.get("textDocument").get("uri").as_string();

    if (!uri.empty() && open_documents_.count(uri)) {
        /* Get full text from contentChanges array (last entry has the full text) */
        JsonValue changes = p.get("contentChanges");
        if (changes.type == JsonValue::Array && !changes.items.empty()) {
            open_documents_[uri] = changes.items.back().get("text").as_string();
        }
        update_diagnostics(uri, open_documents_[uri]);
    }
}

void LspServer::handle_did_save(const std::string& params) {
    JsonValue p = JsonParser::parse(params);
    std::string uri = p.get("textDocument").get("uri").as_string();
    if (!uri.empty() && open_documents_.count(uri))
        update_diagnostics(uri, open_documents_[uri]);
}

void LspServer::handle_did_close(const std::string& params) {
    JsonValue p = JsonParser::parse(params);
    std::string uri = p.get("textDocument").get("uri").as_string();
    if (!uri.empty()) open_documents_.erase(uri);
}

void LspServer::run() {
    while (true) {
        std::string msg = read_message();
        if (msg.empty()) break;
        handle_message(msg);
        if (shutdown_) break;
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
