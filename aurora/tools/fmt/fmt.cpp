/* ════════════════════════════════════════════════════════════
   aurora_fmt — Aurora source code formatter (gofmt-style).

   Usage:
     aurora_fmt <file...>          format in place
     aurora_fmt --check <file...>  exit 1 if any file differs
     aurora_fmt --stdout <file>    print formatted source to stdout
     aurora_fmt --tab-size N       set indentation width (default 4)

   The formatter re-emits each source line from the lexer's tokens,
   normalizing spacing around operators and punctuation while
   preserving comments and block structure.
   ════════════════════════════════════════════════════════════ */
#include "compiler/lexer.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

bool g_check = false;
bool g_stdout = false;
int  g_tab_size = 4;

/* ── spacing normalization ── */
bool is_open_bracket(const std::string& v) { return v == "(" || v == "[" || v == "{"; }
bool is_close_bracket(const std::string& v) { return v == ")" || v == "]" || v == "}"; }
bool is_comma_or_semi(const std::string& v) { return v == "," || v == ";"; }

/* Comma/semicolon: no space before, one after. */
bool wants_space_after(const Token& prev, const Token& tok) {
    if (is_comma_or_semi(tok.value)) return true;
    if (is_close_bracket(tok.value)) return false;
    if (tok.value == ".") return false;
    if (prev.value == ".") return false;
    if (tok.is_operator(":") && !prev.is_keyword("fn")) return true;
    if (is_open_bracket(tok.value)) return true;
    /* keyword then identifier: e.g. `function name`, `return x` */
    if (tok.kind == TokenKind::Identifier && prev.kind == TokenKind::Keyword) return true;
    if (tok.kind == TokenKind::Number && prev.kind == TokenKind::Keyword) return true;
    if (tok.kind == TokenKind::String && prev.kind == TokenKind::Keyword) return true;
    if (tok.kind == TokenKind::Attribute && prev.kind == TokenKind::Identifier) return true;
    if (tok.is_keyword("end") || tok.is_keyword("else") || tok.is_keyword("elif") ||
        tok.is_keyword("elseif") || tok.is_keyword("finally") || tok.is_keyword("case") ||
        tok.is_keyword("catch") || tok.is_keyword("when")) return true;
    return false;
}

/* Operators get spaces unless it's a unary sign or deref. */
bool is_binary_operator(const Token& t) {
    if (t.kind != TokenKind::Operator) return false;
    const std::string& v = t.value;
    if (v == "." || v == "," || v == ";") return false;
    if (v == "(" || v == ")" || v == "[" || v == "]" || v == "{") return false;
    if (v == ":") return false;   /* type annotation colon — no space before */
    return true;
}

std::string format_line(const LexedLine& ll) {
    std::string out;

    const auto& toks = ll.tokens;
    std::string prev_value;
    TokenKind prev_kind = TokenKind::Unknown;
    bool first_tok = true;

    for (size_t i = 0; i < toks.size(); i++) {
        const Token& t = toks[i];
        if (t.is(TokenKind::Newline)) break;
        if (t.is_unknown()) continue;
        if (t.is(TokenKind::Indent) || t.is(TokenKind::Dedent)) continue;

        const std::string v = t.value;
        if (v.empty()) continue;

        /* spacing before this token */
        if (!first_tok) {
            bool sp = false;
            if (is_binary_operator(t)) sp = true;
            else if (is_binary_operator(Token{prev_kind, prev_value, 0, 0})) sp = true;
            else if (is_comma_or_semi(t.value) || is_close_bracket(t.value) || t.value == ".")
                sp = false;
            else if (t.value == ":") sp = false;   /* type annotation: name: Type */
            else if (t.kind == TokenKind::String) sp = true;
            else if (t.kind == TokenKind::Number || t.kind == TokenKind::Float) {
                sp = !(prev_value == "." || prev_value == "(" || prev_value == "[");
            }
            else if (t.kind == TokenKind::Identifier && prev_value != "." &&
                     prev_kind != TokenKind::Operator)
                sp = true;
            else if (prev_value == ":" && (t.kind == TokenKind::Identifier ||
                                           t.kind == TokenKind::Keyword))
                sp = true;
            else if (t.kind == TokenKind::Keyword) sp = true;
            else if (t.kind == TokenKind::Attribute) sp = true;
            if (is_open_bracket(v)) sp = !(prev_kind == TokenKind::Identifier ||
                                           prev_kind == TokenKind::Number ||
                                           prev_kind == TokenKind::Keyword);
            if (sp) out += ' ';
        }

        out += v;
        prev_value = v;
        prev_kind = t.kind;
        first_tok = false;
    }
    return out;
}

bool is_block_opener(const std::string& kw) {
    return kw == "function" || kw == "fn" || kw == "if" || kw == "elif" ||
           kw == "elseif" || kw == "else" || kw == "for" || kw == "while" ||
           kw == "class" || kw == "struct" || kw == "enum" || kw == "interface" ||
           kw == "try" || kw == "case" || kw == "match" || kw == "union" ||
           kw == "namespace" || kw == "block";
}

bool is_block_closer(const std::string& kw) {
    return kw == "end" || kw == "else" || kw == "elif" || kw == "elseif" ||
           kw == "case" || kw == "catch" || kw == "finally" || kw == "when";
}

std::string first_real_token(const LexedLine& ll) {
    for (auto& t : ll.tokens) {
        if (t.is(TokenKind::Newline) || t.is(TokenKind::Indent) ||
            t.is(TokenKind::Dedent)) continue;
        if (t.is_unknown()) continue;
        return t.value;
    }
    return "";
}

bool line_has_block_close(const LexedLine& ll) {
    std::string ft = first_real_token(ll);
    if (is_block_closer(ft)) return true;
    /* brace style: closing } at line start */
    return ft == "}";
}

bool line_has_block_open(const LexedLine& ll) {
    std::string ft = first_real_token(ll);
    if (ft == "function" || ft == "fn" || ft == "class" || ft == "struct" ||
        ft == "enum" || ft == "interface" || ft == "union" || ft == "namespace" ||
        ft == "try" || ft == "match") return true;
    if (ft == "if" || ft == "elif" || ft == "elseif" || ft == "for" ||
        ft == "while" || ft == "case") return true;
    return false;
}

std::string format_source(const std::string& src) {
    Lexer lexer;
    auto lines = lexer.lex(src);
    std::string out;
    int indent = 0;
    for (auto& ll : lines) {
        bool blank = ll.empty();
        bool comment = false;
        std::string comment_text;
        if (!ll.tokens.empty()) {
            for (auto& t : ll.tokens) {
                if (t.is(TokenKind::Unknown) && !t.value.empty() &&
                    (t.value[0] == '#' || t.value[0] == '/')) {
                    comment = true;
                    comment_text += t.value;
                }
            }
        }
        if (blank) {
            out += "\n";
            continue;
        }

        /* adjust indent for closing keywords before emitting */
        bool closer = line_has_block_close(ll);
        bool opener = line_has_block_open(ll);
        int emit_indent = indent;
        if (closer) emit_indent = indent > 0 ? indent - 1 : 0;

        std::string fmt;
        if (comment) {
            for (int i = 0; i < emit_indent * g_tab_size; i++) fmt += ' ';
            fmt += comment_text;
        } else {
            std::string body = format_line(ll);
            for (int i = 0; i < emit_indent * g_tab_size; i++) fmt += ' ';
            fmt += body;
        }
        out += fmt;
        out += "\n";

        /* update indent for next line */
        if (closer) indent = emit_indent;
        if (opener) indent++;
        if (indent < 0) indent = 0;
    }
    return out;
}

bool process_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) { std::cerr << "aurora_fmt: cannot open " << path << "\n"; return false; }
    std::stringstream ss; ss << in.rdbuf();
    std::string src = ss.str();

    std::string formatted = format_source(src);

    /* normalize CRLF so comparison is line-ending agnostic */
    auto normalize = [](std::string s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) if (c != '\r') out += c;
        return out;
    };
    std::string src_norm = normalize(src);
    std::string fmt_norm = normalize(formatted);

    if (g_stdout) {
        std::cout << formatted;
        return true;
    }
    if (fmt_norm == src_norm) {
        std::cout << path << " unchanged\n";
        return true;
    }
    if (g_check) {
        std::cerr << path << " would be reformatted\n";
        return false;
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) { std::cerr << "aurora_fmt: cannot write " << path << "\n"; return false; }
    out << formatted;
    std::cout << path << " formatted\n";
    return true;
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> files;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--check") g_check = true;
        else if (a == "--stdout") g_stdout = true;
        else if (a == "--fix") { /* default action — format in place */ }
        else if (a == "--tab-size" && i + 1 < argc) { g_tab_size = atoi(argv[++i]); if (g_tab_size < 1) g_tab_size = 4; }
        else if (a == "--help" || a == "-h") {
            std::cout << "usage: aurora_fmt [--check] [--stdout] [--tab-size N] <file...>\n";
            return 0;
        }
        else files.push_back(a);
    }
    if (files.empty()) {
        std::cerr << "aurora_fmt: no input files\n";
        return 2;
    }
    bool ok = true;
    for (auto& f : files)
        if (!process_file(f)) ok = false;
    if (g_check && !ok) return 1;
    if (!g_check && !g_stdout && !ok) return 1;
    return ok ? 0 : 1;
}