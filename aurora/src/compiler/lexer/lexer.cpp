#include "compiler/lexer.hpp"
#include "compiler/keywords.hpp"
#include <sstream>
#include <cctype>
#include <cstring>
#include <string>

static std::string ascii_lower(std::string s) {
    for (char& ch : s)
        ch = (char)std::tolower((unsigned char)ch);
    return s;
}

void Lexer::strip_cr(std::string& line) {
    for (char& c : line) if (c == '\r') c = ' ';
}

int Lexer::get_indent(const std::string& line) {
    int c = 0;
    for (char ch : line) {
        if      (ch == ' ')  c++;
        else if (ch == '\t') c += 4;
        else break;
    }
    return c;
}

LexedLine Lexer::lex_line(const std::string& raw, int line_no) {
    LexedLine result;
    result.line_no = line_no;

    std::string line = raw;
    strip_cr(line);
    result.indent = get_indent(line);

    const char* p = line.c_str();
    int col = 0;

    /* ── continue multi-line block comment ── */
    if (block_comment_continue_) {
        const char* end = std::strstr(p, "*/");
        if (end) {
            block_comment_continue_ = false;
            p = end + 2;
            col = (int)(p - line.c_str());
        } else {
            return result;
        }
    }

    while (*p) {
        /* skip whitespace */
        while (*p == ' ' || *p == '\t' || *p == '\r') { p++; col++; }
        if (*p == '\0' || *p == '#') break;

        Token t;
        t.line = line_no;
        t.col  = col;

        /* ── three-char operators ── */
        if (p[0]=='.' && p[1]=='.' && p[2]=='.') {
            t.type  = TokenType::Operator;
            t.value = "...";
            p += 3; col += 3;
            goto push_token;
        }
        /* ── two-char operators ── */
        if ((p[0]=='=' && p[1]=='=') || (p[0]=='!' && p[1]=='=') ||
            (p[0]=='<' && p[1]=='=') || (p[0]=='>' && p[1]=='=') ||
            (p[0]=='-' && p[1]=='>') || (p[0]=='*' && p[1]=='*') ||
            (p[0]=='/' && p[1]=='/') || (p[0]=='.' && p[1]=='.') ||
            (p[0]=='<' && p[1]=='<') || (p[0]=='>' && p[1]=='>')) {
            t.type  = TokenType::Operator;
            t.value = std::string(p, 2);
            p += 2; col += 2;
        }
        /* ── attribute: @identifier ── */
        else if (*p == '@') {
            p++; col++;
            if (std::isalpha((unsigned char)*p) || *p == '_') {
                t.type = TokenType::Attribute;
                while (std::isalnum((unsigned char)*p) || *p == '_') {
                    t.value += *p++; col++;
                }
            } else {
                t.type  = TokenType::Operator;
                t.value = "@";
            }
        }
        /* ── block comment  slash-asterisk ... asterisk-slash ── */
        else if (p[0]=='/' && p[1]=='*') {
            p += 2; col += 2;
            while (*p) {
                if (p[0]=='*' && p[1]=='/') { p += 2; col += 2; goto next_char; }
                p++; col++;
            }
            block_comment_continue_ = true;
            return result;
        }
        /* ── single-char operators ── */
        else if (std::strchr("+-*/=()[]{}:;<>,.@!%&|^~?", *p)) {
            t.type  = TokenType::Operator;
            t.value = std::string(1, *p);
            p++; col++;
        }
        /* ── float or int number ── */
        else if (std::isdigit((unsigned char)*p)) {
            bool is_float = false;
            while (std::isdigit((unsigned char)*p)) { t.value += *p++; col++; }
            if (*p == '.' && std::isdigit((unsigned char)*(p+1))) {
                is_float = true;
                t.value += *p++; col++;
                while (std::isdigit((unsigned char)*p)) { t.value += *p++; col++; }
            }
            t.type = is_float ? TokenType::Float : TokenType::Number;
        }
        /* ── string: single or double quote, with escapes ── */
        else if (*p == '"' || *p == '\'') {
            char quote = *p++;  col++;
            t.type = TokenType::String;
            while (*p && *p != quote) {
                if (*p == '\\' && *(p+1)) {
                    p++; col++;
                    switch (*p) {
                        case 'n':  t.value += '\n'; break;
                        case 't':  t.value += '\t'; break;
                        case 'r':  t.value += '\r'; break;
                        case '\\': t.value += '\\'; break;
                        case '"':  t.value += '"';  break;
                        case '\'': t.value += '\''; break;
                        default:   t.value += *p;   break;
                    }
                } else {
                    t.value += *p;
                }
                p++; col++;
            }
            if (*p) { p++; col++; }
        }
        /* ── identifier or keyword ── */
        else if (std::isalpha((unsigned char)*p) || *p == '_') {
            while (std::isalnum((unsigned char)*p) || *p == '_') {
                t.value += *p++; col++;
            }
            std::string lowered = ascii_lower(t.value);
            if (is_aurora_keyword(lowered)) {
                t.type  = TokenType::Keyword;
            } else {
                t.type = TokenType::Identifier;
            }
        }
        /* ── unknown ── */
        else {
            t.type  = TokenType::Unknown;
            t.value = std::string(1, *p++);
            col++;
        }

        result.tokens.push_back(std::move(t));
        continue;
    push_token:
        result.tokens.push_back(std::move(t));
        continue;
    next_char:;
    }
    return result;
}

std::vector<LexedLine> Lexer::lex(const std::string& source) {
    std::vector<LexedLine> result;
    std::istringstream ss(source);
    std::string line;
    int line_no = 1;

    /* multi-line bracket state: << acts like ( across lines, closed by >> */
    int ml_line = -1;
    int ml_tok  = -1;

    while (std::getline(ss, line)) {
        if (ml_line >= 0) {
            auto lexed = lex_line(line, line_no);

            int close_idx = -1;
            for (int i = 0; i < (int)lexed.tokens.size(); ++i) {
                if (lexed.tokens[i].value == ">>") { close_idx = i; break; }
            }

            auto& dst = result[ml_line].tokens;

            if (close_idx >= 0) {
                lexed.tokens[close_idx].value = ")";
                for (int i = 0; i <= close_idx; ++i)
                    dst.push_back(std::move(lexed.tokens[i]));
                ml_line = -1;
            } else {
                for (auto& t : lexed.tokens)
                    dst.push_back(std::move(t));
            }
            line_no++;
            continue;
        }

        auto lexed = lex_line(line, line_no);

        for (int i = 0; i < (int)lexed.tokens.size(); ++i) {
            if (lexed.tokens[i].value == "<<") {
                int close_idx = -1;
                for (int j = i + 1; j < (int)lexed.tokens.size(); ++j) {
                    if (lexed.tokens[j].value == ">>") { close_idx = j; break; }
                }

                if (close_idx >= 0) {
                    lexed.tokens[i].value = "(";
                    lexed.tokens[close_idx].value = ")";
                } else {
                    lexed.tokens[i].value = "(";
                    ml_line = (int)result.size();
                    ml_tok  = i;
                }
                break;
            }
        }

        result.push_back(std::move(lexed));
        line_no++;
    }

    return result;
}
