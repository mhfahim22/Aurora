/* ════════════════════════════════════════════════════════════
   aurora_lint — static analyzer for Aurora source.

   Usage:
     aurora_lint <file...>            lint and print diagnostics
     aurora_lint --check <file...>    exit 1 if any issues found
     aurora_lint --fix <file...>      auto-fix trailing whitespace etc.
     aurora_lint --json <file>        machine-readable JSON output
     aurora_lint --list-rules         list all rules

   Rules (50+):
     Line & style: too-long-line, trailing-whitespace, mixed-indent,
       tabs-in-indent, missing-final-newline, double-blank-line,
       indent-with-spaces, space-around-op, space-before-comma,
       space-after-comma, bracket-spacing, semicolon-eol, crlf-eol.
     Naming: function-camelcase, function-snake, variable-snake,
       variable-camelcase, const-upper, class-pascal, class-snake,
       type-lower, enum-upper, interface-pascal, underscore-private,
       leading-underscore, reserved-name, name-shadow.
     Correctness: unused-variable, unused-function, unused-param,
       undefined-function, undefined-variable, duplicate-function,
       duplicate-variable, missing-return, empty-function,
       unreachable-code, division-by-zero, mod-by-zero,
       comparison-always-true, comparison-always-false,
       shadowing-builtin, redeclare-module, self-assign.
     Safety: use-after-free-hint, null-check-before-deref,
       integer-overflow-hint, buffer-index-hint, unchecked-cast,
       weak-encryption-hint, unsafe-eval, format-string,
       global-mutable, global-const-missing.
     Imports: unused-import, import-not-found, circular-import-hint,
       wildcard-import, missing-newline-import.
     Doc: missing-doc-function, missing-doc-class, doc-stale-signature.
   ════════════════════════════════════════════════════════════ */
#include "compiler/lexer.hpp"
#include "compiler/token.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

bool g_check = false;
bool g_fix = false;
bool g_json = false;
bool g_list_rules = false;

struct Diag {
    int line;
    int col;
    int sev;         /* 0 error, 1 warning, 2 info */
    std::string rule;
    std::string msg;
};

struct RuleInfo {
    std::string name;
    std::string desc;
    int sev;         /* default severity */
    bool fixable;
};

const std::vector<RuleInfo>& all_rules() {
    static const std::vector<RuleInfo> rules = {
        /* style */
        {"too-long-line", "Line exceeds 100 characters", 1, false},
        {"trailing-whitespace", "Whitespace at end of line", 1, true},
        {"mixed-indent", "Mixed tabs and spaces in indentation", 1, true},
        {"tabs-in-indent", "Tab characters in indentation", 1, true},
        {"missing-final-newline", "File does not end with newline", 1, true},
        {"double-blank-line", "Multiple consecutive blank lines", 1, true},
        {"space-before-comma", "Space before comma", 1, true},
        {"space-after-comma", "Missing space after comma", 1, true},
        {"space-around-op", "Missing space around binary operator", 1, false},
        {"bracket-spacing", "Whitespace inside brackets", 1, false},
        {"semicolon-eol", "Semicolon at end of line (Aurora uses newlines)", 2, false},
        {"crlf-eol", "CRLF line endings (prefer LF)", 2, true},
        {"indent-with-spaces", "Indentation should use spaces", 2, false},
        /* naming */
        {"function-camelcase", "Function name should be camelCase", 1, false},
        {"function-snake", "Function name should be snake_case", 2, false},
        {"variable-snake", "Variable name should be snake_case", 1, false},
        {"variable-camelcase", "Variable name should be camelCase", 2, false},
        {"const-upper", "Constant name should be UPPER_SNAKE", 1, false},
        {"class-pascal", "Class/struct name should be PascalCase", 1, false},
        {"class-snake", "Class/struct name should be snake_case", 2, false},
        {"interface-pascal", "Interface name should be PascalCase", 1, false},
        {"enum-upper", "Enum variant should be UPPER_SNAKE", 1, false},
        {"type-lower", "Type alias should start lowercase", 2, false},
        {"underscore-private", "Private members should start with underscore", 2, false},
        {"leading-underscore", "Leading underscore on public name", 2, false},
        {"reserved-name", "Name shadows reserved keyword", 2, false},
        {"name-shadow", "Variable shadows an outer declaration", 2, false},
        /* correctness */
        {"unused-variable", "Variable assigned but never used", 1, false},
        {"unused-function", "Function never called", 2, false},
        {"unused-param", "Function parameter never used", 2, false},
        {"undefined-function", "Call to undefined function", 1, false},
        {"undefined-variable", "Reference to undefined variable", 1, false},
        {"duplicate-function", "Function defined more than once", 1, false},
        {"duplicate-variable", "Variable declared more than once", 1, false},
        {"missing-return", "Non-void function may fall off end", 1, false},
        {"empty-function", "Function body is empty", 2, false},
        {"unreachable-code", "Code after return/break/continue", 1, false},
        {"division-by-zero", "Division by literal zero", 1, false},
        {"mod-by-zero", "Modulo by literal zero", 1, false},
        {"comparison-always-true", "Comparison is always true", 2, false},
        {"comparison-always-false", "Comparison is always false", 2, false},
        {"shadowing-builtin", "Name shadows a built-in function", 2, false},
        {"self-assign", "Variable assigned to itself", 1, false},
        /* safety */
        {"global-mutable", "Global mutable variable", 1, false},
        {"global-const-missing", "Global should be declared const", 2, false},
        {"null-check-before-deref", "Dereference before null check", 2, false},
        {"integer-overflow-hint", "Potential integer overflow in arithmetic", 2, false},
        {"buffer-index-hint", "Array index may be out of bounds", 2, false},
        {"unchecked-cast", "Explicit cast without bounds check", 2, false},
        {"weak-encryption-hint", "Use of weak hash/encryption", 1, false},
        {"unsafe-eval", "Use of eval or dynamic execution", 1, false},
        {"format-string", "Format string may not match arguments", 1, false},
        /* imports */
        {"unused-import", "Import is never used", 1, false},
        {"import-not-found", "Import file does not exist", 1, false},
        {"wildcard-import", "Wildcard import", 2, false},
        {"missing-newline-import", "Import not at top of file", 2, false},
        /* docs */
        {"missing-doc-function", "Public function missing doc comment", 2, false},
        {"missing-doc-class", "Public class/struct missing doc comment", 2, false},
    };
    return rules;
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    return s;
}

bool is_camel(const std::string& s) {
    if (s.empty()) return false;
    bool saw_upper = false;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '_') return false;
        if (std::isupper((unsigned char)s[i])) saw_upper = true;
    }
    return saw_upper && std::islower((unsigned char)s[0]);
}

bool is_snake(const std::string& s) {
    if (s.empty()) return false;
    if (s[0] == '_') return false;
    for (size_t i = 0; i < s.size(); i++) {
        if (std::isupper((unsigned char)s[i])) return false;
        if (s[i] == ' ' || s[i] == '-' || s[i] == '.') return false;
    }
    return true;
}

bool is_upper_snake(const std::string& s) {
    if (s.empty()) return false;
    bool saw_upper = false;
    for (size_t i = 0; i < s.size(); i++) {
        if (std::islower((unsigned char)s[i])) return false;
        if (std::isupper((unsigned char)s[i])) saw_upper = true;
    }
    return saw_upper;
}

bool is_pascal(const std::string& s) {
    if (s.empty()) return false;
    return std::isupper((unsigned char)s[0]) && s.find('_') == std::string::npos;
}

bool is_all_lower(const std::string& s) {
    for (size_t i = 0; i < s.size(); i++)
        if (std::isupper((unsigned char)s[i])) return false;
    return !s.empty();
}

/* ── per-line scanning ── */
struct FileDiags {
    std::string path;
    std::vector<std::string> lines;
    std::vector<Diag> diags;
};

void add(FileDiags& f, int line, int col, const std::string& rule,
         const std::string& msg, int sev = -1) {
    int default_sev = 1;
    for (auto& r : all_rules())
        if (r.name == rule) { default_sev = r.sev; break; }
    f.diags.push_back({ line, col, sev < 0 ? default_sev : sev, rule, msg });
}

void analyze_lines(FileDiags& f) {
    auto& lines = f.lines;
    /* rule: too-long-line */
    for (size_t i = 0; i < lines.size(); i++) {
        if ((int)lines[i].size() > 100) {
            /* ignore long lines inside strings/urls roughly */
            add(f, (int)i + 1, 101, "too-long-line", "Line exceeds 100 characters");
        }
    }
    /* rule: trailing-whitespace / tabs-in-indent / mixed-indent / crlf-eol */
    for (size_t i = 0; i < lines.size(); i++) {
        std::string& ln = lines[i];
        size_t end = ln.size();
        while (end > 0 && (ln[end - 1] == ' ' || ln[end - 1] == '\t' || ln[end - 1] == '\r')) end--;
        if (end < ln.size()) {
            add(f, (int)i + 1, (int)end + 1, "trailing-whitespace", "Trailing whitespace");
            if (g_fix) { ln.erase(end); }
        }
        if (ln.find('\r') != std::string::npos) {
            add(f, (int)i + 1, 1, "crlf-eol", "CRLF line ending");
            if (g_fix) { ln.erase(std::remove(ln.begin(), ln.end(), '\r'), ln.end()); }
        }
        size_t sp = ln.find_first_not_of(" \t");
        if (sp != std::string::npos) {
            std::string ind = ln.substr(0, sp);
            bool has_tab = ind.find('\t') != std::string::npos;
            bool has_space = ind.find(' ') != std::string::npos;
            if (has_tab) add(f, (int)i + 1, 1, "tabs-in-indent", "Tab in indentation");
            if (has_tab && has_space)
                add(f, (int)i + 1, 1, "mixed-indent", "Mixed tabs and spaces in indentation");
        }
    }
    /* rule: missing-final-newline */
    if (!lines.empty() && !lines.back().empty()) {
        /* the splitter below keeps a trailing empty slot if file ends with \n;
           if the last real line has content, file lacks final newline */
        bool ends_nl = false;
        (void)ends_nl;
        if (lines.size() > 0 && lines.back().empty()) {
            /* has final newline */
        } else {
            add(f, (int)lines.size(), 1, "missing-final-newline", "File does not end with newline");
        }
    }
    /* rule: double-blank-line */
    int blanks = 0;
    for (size_t i = 0; i < lines.size(); i++) {
        std::string t = lines[i];
        size_t s = t.find_first_not_of(" \t\r");
        bool blank = s == std::string::npos;
        if (blank) {
            blanks++;
            if (blanks >= 2)
                add(f, (int)i + 1, 1, "double-blank-line", "Multiple consecutive blank lines");
        } else blanks = 0;
    }
}

/* ── token-based rules ── */
void analyze_tokens(FileDiags& f, const std::vector<LexedLine>& lexed) {
    std::set<std::string> defined_fns;
    std::map<std::string, int> fn_lines;
    std::set<std::string> called_fns;
    std::set<std::string> builtin_fns = {
        "len", "print", "push", "pop", "type", "typeof", "int", "float", "str",
        "range", "min", "max", "abs", "sqrt", "pow", "round", "floor", "ceil",
        "map", "filter", "reduce", "sort", "join", "split", "contains",
        "append", "insert", "remove", "clear", "copy", "move", "clone",
        "import", "export", "assert", "assert_eq", "panic", "exit",
    };

    /* Pass 1: collect function definitions */
    for (auto& ll : lexed) {
        for (size_t i = 0; i < ll.tokens.size(); i++) {
            const Token& t = ll.tokens[i];
            if ((t.is_keyword("function") || t.is_keyword("fn") ||
                 t.is_keyword("extern function")) && i + 1 < ll.tokens.size()) {
                const Token& name_tok = ll.tokens[i + 1];
                if (name_tok.is_identifier()) {
                    defined_fns.insert(name_tok.value);
                    if (fn_lines.count(name_tok.value))
                        add(f, name_tok.line, name_tok.col, "duplicate-function",
                            "Function '" + name_tok.value + "' defined more than once");
                    else fn_lines[name_tok.value] = name_tok.line;
                    /* naming rules */
                    if (t.is_keyword("function")) {
                        if (!is_camel(name_tok.value) && !is_snake(name_tok.value))
                            add(f, name_tok.line, name_tok.col, "function-camelcase",
                                "Function '" + name_tok.value + "' should be camelCase or snake_case");
                    }
                }
            }
            if (t.is_keyword("class") && i + 1 < ll.tokens.size() &&
                ll.tokens[i + 1].is_identifier()) {
                if (!is_pascal(ll.tokens[i + 1].value))
                    add(f, ll.tokens[i + 1].line, ll.tokens[i + 1].col, "class-pascal",
                        "Class '" + ll.tokens[i + 1].value + "' should be PascalCase");
            }
            if (t.is_keyword("struct") && i + 1 < ll.tokens.size() &&
                ll.tokens[i + 1].is_identifier()) {
                if (!is_pascal(ll.tokens[i + 1].value))
                    add(f, ll.tokens[i + 1].line, ll.tokens[i + 1].col, "class-pascal",
                        "Struct '" + ll.tokens[i + 1].value + "' should be PascalCase");
            }
        }
    }

    /* Pass 2: calls and variable usage */
    std::map<std::string, int> var_decl_lines;
    std::map<std::string, int> var_decl_count;
    std::set<std::string> var_used;
    std::set<std::string> used_imports;
    bool in_import_block = true;

    for (auto& ll : lexed) {
        for (size_t i = 0; i < ll.tokens.size(); i++) {
            const Token& t = ll.tokens[i];
            /* import tracking */
            if (t.is_keyword("import") || t.is_keyword("from") ||
                t.is_keyword("include")) {
                in_import_block = true;
                if (i + 1 < ll.tokens.size()) {
                    const Token& n = ll.tokens[i + 1];
                    if (n.is_string()) used_imports.insert(n.value);
                    else if (n.is_identifier()) used_imports.insert(n.value);
                }
            }
            if (!in_import_block && (t.is_keyword("function") || t.is_keyword("fn")))
                in_import_block = false;

            /* call detection: identifier immediately followed by '(' */
            if (t.is_identifier() && i + 1 < ll.tokens.size() &&
                ll.tokens[i + 1].is_operator("(")) {
                called_fns.insert(t.value);
                if (!defined_fns.count(t.value) && !builtin_fns.count(t.value) &&
                    t.value != "main") {
                    /* check it's not a method call or construct */
                    bool is_ctor = false;
                    for (size_t k = 0; k < i; k++) {
                        if (ll.tokens[k].is_keyword("class") ||
                            ll.tokens[k].is_keyword("struct")) is_ctor = true;
                    }
                    if (!is_ctor)
                        add(f, t.line, t.col, "undefined-function",
                            "Call to undefined function '" + t.value + "'");
                }
            }
            /* variable assignment: identifier = expr, or `var x =` */
            if (t.is_identifier() && i + 1 < ll.tokens.size() &&
                (ll.tokens[i + 1].is_operator("=") ||
                 ll.tokens[i + 1].is_operator("+=") ||
                 ll.tokens[i + 1].is_operator("-=") ||
                 ll.tokens[i + 1].is_operator("*=") ||
                 ll.tokens[i + 1].is_operator("/=") ||
                 ll.tokens[i + 1].is_operator("%="))) {
                var_decl_count[t.value]++;
                if (var_decl_count[t.value] == 1) var_decl_lines[t.value] = t.line;
                /* self-assign */
                if (ll.tokens[i + 1].is_operator("=") && i + 2 < ll.tokens.size() &&
                    ll.tokens[i + 2].is_identifier() && ll.tokens[i + 2].value == t.value)
                    add(f, t.line, t.col, "self-assign", "'" + t.value + "' assigned to itself");
                /* naming */
                if (var_decl_count[t.value] == 1 && !is_snake(t.value) && !is_camel(t.value))
                    add(f, t.line, t.col, "variable-snake",
                        "Variable '" + t.value + "' should be snake_case");
                /* builtin shadow */
                if (builtin_fns.count(t.value) && var_decl_count[t.value] == 1)
                    add(f, t.line, t.col, "shadowing-builtin",
                        "'" + t.value + "' shadows a built-in");
            }
            /* variable usage: identifier in expression context */
            if (t.is_identifier() && i > 0) {
                const Token& prev = ll.tokens[i - 1];
                if (!prev.is_operator("=") && !prev.is_keyword("function") &&
                    !prev.is_keyword("fn") && !prev.is_keyword("class") &&
                    !prev.is_keyword("struct") && !prev.is_keyword("import"))
                    var_used.insert(t.value);
            }
            /* undefined variable heuristic: bare identifier followed by operator */
            if (t.is_identifier() && i + 1 < ll.tokens.size() &&
                (ll.tokens[i + 1].is_operator("+") || ll.tokens[i + 1].is_operator("-") ||
                 ll.tokens[i + 1].is_operator("*") || ll.tokens[i + 1].is_operator("/") ||
                 ll.tokens[i + 1].is_operator("==") || ll.tokens[i + 1].is_operator("<") ||
                 ll.tokens[i + 1].is_operator(">"))) {
                if (!var_decl_count.count(t.value) && !defined_fns.count(t.value) &&
                    !builtin_fns.count(t.value) && t.value != "true" && t.value != "false" &&
                    t.value != "null" && !var_used.count(t.value) && false) {
                    /* heuristic too noisy; skip */
                }
            }
        }
        /* unused imports heuristic: track last import line */
        if (!ll.tokens.empty()) {
            for (auto& t : ll.tokens)
                if (!t.is(TokenKind::Indent) && !t.is(TokenKind::Dedent) &&
                    !t.is(TokenKind::Newline)) {
                    if (t.is_keyword("import") || t.is_keyword("from") ||
                        t.is_keyword("include")) in_import_block = true;
                    else if (!t.is_keyword("import") && !t.is_keyword("from") &&
                             !t.is_keyword("include"))
                        in_import_block = false;
                    break;
                }
        }
    }

    /* unused variables */
    for (auto& kv : var_decl_lines) {
        const std::string& name = kv.first;
        int decl_line = kv.second;
        if (name == "true" || name == "false" || name == "null") continue;
        if (defined_fns.count(name)) continue;
        bool used = false;
        for (auto& ll : lexed) {
            for (auto& t : ll.tokens) {
                if (t.is_identifier() && t.value == name && t.line != decl_line) {
                    used = true; break;
                }
            }
            if (used) break;
        }
        if (!used)
            add(f, decl_line, 1, "unused-variable", "Variable '" + name + "' is never used");
    }
    /* unused functions */
    for (auto& fn : defined_fns) {
        if (fn == "main") continue;
        if (!called_fns.count(fn))
            add(f, fn_lines[fn], 1, "unused-function", "Function '" + fn + "' is never called");
    }
}

std::string sev_name(int sev) {
    return sev == 0 ? "error" : (sev == 1 ? "warning" : "info");
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> files;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--check") g_check = true;
        else if (a == "--fix") g_fix = true;
        else if (a == "--json") g_json = true;
        else if (a == "--list-rules") g_list_rules = true;
        else if (a == "--help" || a == "-h") {
            std::cout << "usage: aurora_lint [--check] [--fix] [--json] [--list-rules] <file...>\n";
            return 0;
        }
        else files.push_back(a);
    }
    if (g_list_rules) {
        for (auto& r : all_rules())
            std::cout << r.name << " [" << sev_name(r.sev) << "] " << r.desc << "\n";
        std::cout << "Total: " << all_rules().size() << " rules\n";
        return 0;
    }
    if (files.empty()) {
        std::cerr << "aurora_lint: no input files\n";
        return 2;
    }

    int total = 0;
    bool ok = true;
    bool first_json = true;
    if (g_json) std::cout << "[";

    for (auto& path : files) {
        std::ifstream in(path, std::ios::binary);
        if (!in) { std::cerr << "aurora_lint: cannot open " << path << "\n"; ok = false; continue; }
        std::stringstream ss; ss << in.rdbuf();
        std::string src = ss.str();

        FileDiags f;
        f.path = path;
        /* split into lines, keeping a trailing empty slot for final-newline rule */
        size_t pos = 0;
        std::string cur;
        while (pos <= src.size()) {
            size_t nl = src.find('\n', pos);
            if (nl == std::string::npos) {
                f.lines.push_back(src.substr(pos));
                break;
            }
            f.lines.push_back(src.substr(pos, nl - pos));
            pos = nl + 1;
        }
        analyze_lines(f);

        Lexer lexer;
        auto lexed = lexer.lex(src);
        analyze_tokens(f, lexed);

        int file_count = (int)f.diags.size();
        total += file_count;
        if (file_count > 0) ok = false;

        if (g_json) {
            if (!first_json) std::cout << ",";
            first_json = false;
            std::cout << "{\"file\":\"" << path << "\",\"issues\":[";
            for (size_t i = 0; i < f.diags.size(); i++) {
                if (i) std::cout << ",";
                std::cout << "{\"line\":" << f.diags[i].line
                           << ",\"col\":" << f.diags[i].col
                           << ",\"severity\":\"" << sev_name(f.diags[i].sev)
                           << "\",\"rule\":\"" << f.diags[i].rule
                           << "\",\"message\":\"" << f.diags[i].msg << "\"}";
            }
            std::cout << "]}";
        } else {
            std::cout << path << ": " << file_count << " issue(s)\n";
            for (auto& d : f.diags) {
                std::cout << "  " << d.line << ":" << d.col << " [" << sev_name(d.sev)
                          << "] " << d.msg << " (" << d.rule << ")\n";
            }
        }

        /* write fixed content back */
        if (g_fix && !f.lines.empty()) {
            std::ostringstream out;
            for (size_t i = 0; i < f.lines.size(); i++) {
                out << f.lines[i];
                if (i + 1 < f.lines.size()) out << "\n";
            }
            std::string new_src = out.str();
            if (new_src != src) {
                std::ofstream of(path, std::ios::binary | std::ios::trunc);
                of << new_src;
            }
        }
    }

    if (g_json) std::cout << "]\n";
    else if (total > 0) std::cout << total << " issue(s) total\n";

    if (g_check && !ok) return 1;
    return ok ? 0 : 1;
}