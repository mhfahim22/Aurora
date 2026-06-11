#include "compiler/parser.hpp"
#include <stdexcept>
#include <sstream>

bool Parser::skip_blanks() {
    while (!at_end() && cur_line().is_blank()) advance();
    return !at_end();
}

ASTNode::Ptr Parser::parse() {
    return parse_block(-1);
}

void Parser::require_token_end(const std::vector<Token>& toks, int idx, const char* context) const {
    if (idx >= (int)toks.size()) return;

    const Token& t = toks[idx];
    std::string msg = "unexpected token '" + t.value + "' in " + context;

    /* Provide helpful hints based on the unexpected token */
    std::string hint;
    if (t.value == "=") {
        hint = "did you mean '==' for comparison? '=' is only for assignment.";
    } else if (t.value == ":") {
        hint = "the colon ':' starts a new block. Check your indentation.";
    } else if (t.is_keyword("end")) {
        hint = "Aurora uses indentation for blocks, not 'end'.";
    } else if (t.value == ";") {
        hint = "Aurora doesn't use semicolons. Remove it.";
    } else if (t.value == "{" || t.value == "}") {
        /* braces are now allowed for block delimiters */
        return;
    }

    if (!hint.empty()) {
        throw std::runtime_error(format_error_with_hint(t.line, t.col, msg, hint));
    } else {
        throw std::runtime_error(format_error(t.line, t.col, msg));
    }
}

ASTNode::Ptr Parser::parse_block(int parent_indent) {
    if (!at_end() && !cur_line().is_blank()) {
        const auto& toks = cur_line().tokens;
        if (!toks.empty() && toks[0].is_operator('{')) {
            advance();
            return parse_brace_block(1);
        }
    }

    ASTNode::Ptr head = nullptr;
    ASTNode*     tail = nullptr;

    while (!at_end()) {
        if (!skip_blanks()) break;
        if (cur_indent() <= parent_indent) break;

        auto stmt = parse_stmt();
        if (!stmt) continue;

        ASTNode* raw = stmt.get();
        if (!head) { head = std::move(stmt); tail = raw; }
        else       { tail->next = std::move(stmt); tail = raw; }
    }
    return head;
}

ASTNode::Ptr Parser::parse_brace_block(int brace_depth) {
    ASTNode::Ptr head = nullptr;
    ASTNode*     tail = nullptr;

    while (!at_end()) {
        if (!skip_blanks()) break;

        const auto& toks = cur_line().tokens;
        if (toks.empty()) { advance(); continue; }

        if (toks.size() == 1 && toks[0].is_operator('}') && brace_depth <= 1) {
            advance();
            return head;
        }

        brace_depth++;
        auto stmt = parse_stmt();
        brace_depth--;

        if (stmt) {
            ASTNode* raw = stmt.get();
            if (!head) { head = std::move(stmt); tail = raw; }
            else       { tail->next = std::move(stmt); tail = raw; }
        }
    }
    return head;
}
