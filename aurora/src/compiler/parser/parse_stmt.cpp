#include "compiler/parser.hpp"
#include <stdexcept>
#include <sstream>

/* Forward declaration for pattern parsing */
static ASTNode::Ptr parse_pattern_from_tokens(const std::vector<Token>& toks, int& idx, int ln);

/* ── Helper: parse attributed function definition ──
   Handles common pattern: @attribute function name(params): body (brace or indent).
   Sets up stmt->args, stmt->body, advances, and returns the completed node.
   The Parser& is needed for advance(), parse_block(), parse_brace_block(). */
static ASTNode::Ptr parse_attributed_fn_body(Parser& parser, ASTNode::Ptr stmt,
                                              const std::vector<Token>& toks, int& idx,
                                              int cnt, int ln, int ci) {
    /* Parse function name */
    if (idx >= cnt || !(toks[idx].is_identifier() || toks[idx].is(TokenKind::Keyword)))
        throw std::runtime_error("Line " + std::to_string(ln) + ": expected function name");
    stmt->value = toks[idx].value;
    idx++;

    /* Parse params: (a, b, c) */
    if (idx < cnt && toks[idx].is_operator('(')) {
        idx++;
        ASTNode* ptail = nullptr;
        while (idx < cnt && !toks[idx].is_operator(')')) {
            if (toks[idx].is_operator(',')) { idx++; continue; }
            auto p = make_node(NodeType::Var, toks[idx++].value);
            ASTNode* raw = p.get();
            if (!stmt->args) { stmt->args = std::move(p); ptail = raw; }
            else             { ptail->next = std::move(p); ptail = raw; }
        }
        if (idx < cnt) idx++;
        else throw std::runtime_error("Line " + std::to_string(ln) + ": missing ')' in function parameters");
    }
    if (idx < cnt && toks[idx].is_operator(':')) idx++;

    bool has_brace = (idx < cnt && toks[idx].is_operator('{'));
    if (has_brace) {
        idx++;
        if (idx < cnt) parser.require_token_end(toks, idx, "attributed function definition");
    } else {
        parser.require_token_end(toks, idx, "attributed function definition");
    }
    parser.advance();

    if (has_brace)
        stmt->body = parser.parse_brace_block(1);
    else
        stmt->body = parser.parse_block(ci);
    return stmt;
}

ASTNode::Ptr Parser::parse_stmt() {
    if (!skip_blanks()) return nullptr;

    const LexedLine& ll   = cur_line();
    const auto&      toks = ll.tokens;
    int              cnt  = static_cast<int>(toks.size());
    int              ci   = ll.indent;
    int              ln   = ll.line_no;

    if (cnt == 0) { advance(); return nullptr; }

    const Token& t0 = toks[0];

    /* ── abstract class / final class ── */
    if (cnt > 1 && (t0.is_keyword("abstract") || t0.is_keyword("final")) &&
        toks[1].is_keyword("class")) {
        return parse_class();
    }

    /* ── Member modifiers (private/public/protected/static/final/abstract) ── */
    if (cnt > 1 && (t0.is_keyword("private") || t0.is_keyword("public") ||
                    t0.is_keyword("protected") || t0.is_keyword("static") ||
                    t0.is_keyword("final") || t0.is_keyword("abstract"))) {
        std::string mod = t0.value;

        /* modifier function name(...):  — method with modifier */
        if (cnt > 2 && toks[1].is_keyword("function")) {
            int idx = 2;
            if (idx < cnt && (toks[idx].is_identifier() || toks[idx].is(TokenKind::Keyword))) {
                std::string fname = toks[idx].value;
                auto stmt = make_node(NodeType::Function, fname, ln);
                /* Store visibility/abstract/final modifier */
                if (mod == "private" || mod == "public" || mod == "protected")
                    stmt->visibility = mod;
                if (mod == "abstract")
                    stmt->is_abstract = true;
                if (mod == "final")
                    stmt->is_final = true;
                idx++;
                if (idx < cnt && toks[idx].is_operator('(')) {
                    idx++;
                    ASTNode* ptail = nullptr;
                    while (idx < cnt && !toks[idx].is_operator(')')) {
                        if (toks[idx].is_operator(',')) { idx++; continue; }
                        auto p = make_node(NodeType::Var, toks[idx++].value);
                        ASTNode* raw = p.get();
                        if (!stmt->args) { stmt->args = std::move(p); ptail = raw; }
                        else { ptail->next = std::move(p); ptail = raw; }
                    }
                    if (idx < cnt) idx++;
                }
                if (idx < cnt && toks[idx].is_operator(':')) idx++;

                bool fn_has_brace = (idx < cnt && toks[idx].is_operator('{'));
                if (fn_has_brace) {
                    idx++;
                    if (idx < cnt) require_token_end(toks, idx, "method definition");
                } else {
                    require_token_end(toks, idx, "method definition");
                }
                advance();

                if (fn_has_brace) {
                    stmt->body = parse_brace_block(1);
                } else {
                    stmt->body = parse_block(ll.indent);
                }
                return stmt;
            }
        }

        /* modifier name = value  — field with modifier */
        if (cnt > 3 && toks[2].is_operator('=')) {
            auto stmt  = make_node(NodeType::Assign, mod, ln);
            stmt->left = make_node(NodeType::Var, toks[1].value, ln);
            int idx = 3;
            stmt->right = parse_expr(toks, idx);
            require_token_end(toks, idx, "field definition");
            advance();
            return stmt;
        }

        /* modifier name  — field with modifier, no default */
        if (cnt > 1 && toks[1].is_identifier()) {
            auto stmt  = make_node(NodeType::Assign, mod, ln);
            stmt->left = make_node(NodeType::Var, toks[1].value, ln);
            int idx = 2;
            require_token_end(toks, idx, "field definition");
            advance();
            return stmt;
        }

        /* Just skip unknown modifier usage */
        advance();
        return make_node(NodeType::Pass, "", ln);
    }

    /* ── @performance attribute ── */
    if (t0.is_attribute("performance")) {
        /* Check if next token on same line has function keyword */
        if (cnt > 1 && toks[1].is_keyword("function")) {
            int idx = 2;
            auto stmt = make_node(NodeType::PerformanceFn, "", ln);
            stmt = parse_attributed_fn_body(*this, std::move(stmt), toks, idx, cnt, ln, ci);
            stmt->memory_meta.alloc_strategy = AllocStrategy::Stack;
            return stmt;
        }
        /* Check if next line has function keyword */
        else if (cnt == 1) {
            advance();
            if (!at_end()) {
                const LexedLine& next_ll = cur_line();
                const auto& next_toks = next_ll.tokens;
                int next_cnt = static_cast<int>(next_toks.size());
                if (next_cnt > 0 && next_toks[0].is_keyword("function") && next_cnt > 1) {
                    int idx = 1;
                    auto stmt = make_node(NodeType::PerformanceFn, "", next_ll.line_no);
                    stmt = parse_attributed_fn_body(*this, std::move(stmt), next_toks, idx, next_cnt, next_ll.line_no, next_ll.indent);
                    stmt->memory_meta.alloc_strategy = AllocStrategy::Stack;
                    return stmt;
                }
            }
            throw std::runtime_error("Line " + std::to_string(ln) + ": @performance must be followed by 'function' keyword");
        } else {
            throw std::runtime_error("Line " + std::to_string(ln) + ": @performance must be followed by 'function' keyword");
        }
    }

    /* ── @stack / @arena / @raii / @arc / @gc allocation attributes ── */
    if (t0.is_attribute("stack") || t0.is_attribute("arena") ||
        t0.is_attribute("raii")  || t0.is_attribute("arc")   ||
        t0.is_attribute("gc")) {

        NodeType alloc_node_type;
        AllocStrategy forced;
        if (t0.is_attribute("stack"))      { alloc_node_type = NodeType::StackAlloc; forced = AllocStrategy::ForcedStack; }
        else if (t0.is_attribute("arena")) { alloc_node_type = NodeType::ArenaAlloc; forced = AllocStrategy::ForcedArena; }
        else if (t0.is_attribute("raii"))  { alloc_node_type = NodeType::RAIIAlloc;  forced = AllocStrategy::ForcedRAII; }
        else if (t0.is_attribute("arc"))   { alloc_node_type = NodeType::ARCAlloc;   forced = AllocStrategy::ForcedARC; }
        else                               { alloc_node_type = NodeType::GCAlloc;    forced = AllocStrategy::ForcedGC; }

        /* Next must be an assignment: @stack x = expr */
        if (cnt > 1 && toks[1].is(TokenKind::Identifier) && cnt > 3 && toks[2].is_operator('=')) {
            auto stmt = make_node(NodeType::Assign, "", ln);
            stmt->left = make_node(NodeType::Var, toks[1].value, ln);
            int idx = 3;
            stmt->right = parse_expr(toks, idx);
            /* Set forced allocation strategy in memory metadata */
            stmt->memory_meta.forced_strategy = forced;
            stmt->left->memory_meta.forced_strategy = forced;
            stmt->right->memory_meta.forced_strategy = forced;
            require_token_end(toks, idx, "@alloc assignment");
            advance();
            return stmt;
        }
        /* Next must be a function: @stack function name(): */
        else if (cnt > 1 && toks[1].is_keyword("function")) {
            int idx = 2;
            auto stmt = make_node(NodeType::PerformanceFn, "", ln);
            stmt = parse_attributed_fn_body(*this, std::move(stmt), toks, idx, cnt, ln, ci);
            stmt->memory_meta.forced_strategy = forced;
            return stmt;
        }
        throw std::runtime_error("Line " + std::to_string(ln) + ": @" + t0.value + " must be followed by 'variable = expr' or 'function'");
    }

    /* ── reference name / pointer name (statement level) ── */
    if ((t0.is_keyword("reference") || t0.is_keyword("pointer")) && cnt > 1 && toks[1].is_identifier()) {
        NodeType nt = t0.is_keyword("reference") ? NodeType::Reference : NodeType::Pointer;
        auto stmt = make_node(nt, toks[1].value, ln);
        int idx = 2;
        require_token_end(toks, idx, (t0.value + " statement").c_str());
        advance();
        return stmt;
    }

    /* ── output(...) / output value ── */
    if ((t0.is_keyword("output") || (t0.is_identifier() && t0.value == "output"))) {
        auto stmt = make_node(NodeType::Output, "", ln);
        int idx = 1;
        /* output(expr) or output expr */
        if (idx < cnt && toks[idx].is_operator('(')) {
            idx++;
            stmt->left = parse_expr(toks, idx);
            if (idx < cnt && toks[idx].is_operator(')')) idx++;
            else throw std::runtime_error("Line " + std::to_string(ln) + ": missing ')' in output statement");
            require_token_end(toks, idx, "output statement");
        } else {
            stmt->left = parse_expr(toks, idx);
            require_token_end(toks, idx, "output statement");
        }
        advance();
        return stmt;
    }

    /* ── parallel: block ── */
    if (t0.is_keyword("parallel")) {
        auto stmt = make_node(NodeType::Parallel, "", ln);
        int idx = 1;
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, "parallel block");
        advance();
        stmt->body = parse_block(ci);
        return stmt;
    }

    /* ── thread expr ── */
    if (t0.is_keyword("thread")) {
        auto stmt = make_node(NodeType::Thread, "", ln);
        int idx = 1;
        if (idx < cnt) stmt->left = parse_expr(toks, idx);
        require_token_end(toks, idx, "thread statement");
        advance();
        return stmt;
    }

    /* ── callback name(params): block ── */
    if (t0.is_keyword("callback") && cnt > 1 && toks[1].is_identifier()) {
        auto stmt = make_node(NodeType::Callback, toks[1].value, ln);
        int idx = 2;
        if (idx < cnt && toks[idx].is_operator('(')) {
            idx++;
            ASTNode* ptail = nullptr;
            while (idx < cnt && !toks[idx].is_operator(')')) {
                if (toks[idx].is_operator(',')) { idx++; continue; }
                auto p = make_node(NodeType::Var, toks[idx++].value);
                ASTNode* raw = p.get();
                if (!stmt->args) { stmt->args = std::move(p); ptail = raw; }
                else { ptail->next = std::move(p); ptail = raw; }
            }
            if (idx < cnt) idx++;
        }
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, "callback definition");
        advance();
        stmt->body = parse_block(ci);
        return stmt;
    }

    /* ── event name: block ── */
    if (t0.is_keyword("event") && cnt > 1 && toks[1].is_identifier()) {
        auto stmt = make_node(NodeType::Event, toks[1].value, ln);
        int idx = 2;
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, "event declaration");
        advance();
        stmt->body = parse_block(ci);
        return stmt;
    }

    /* ── signal name ── */
    if (t0.is_keyword("signal") && cnt > 1 && toks[1].is_identifier()) {
        auto stmt = make_node(NodeType::Signal, toks[1].value, ln);
        int idx = 2;
        require_token_end(toks, idx, "signal statement");
        advance();
        return stmt;
    }

    /* ── emit name(args) ── */
    if (t0.is_keyword("emit") && cnt > 1 && toks[1].is_identifier()) {
        auto stmt = make_node(NodeType::Emit, toks[1].value, ln);
        int idx = 2;
        if (idx < cnt && toks[idx].is_operator('(')) {
            idx++;
            ASTNode* tail = nullptr;
            while (idx < cnt && !toks[idx].is_operator(')')) {
                if (toks[idx].is_operator(',')) { idx++; continue; }
                auto arg = parse_expr(toks, idx);
                ASTNode* raw = arg.get();
                if (!stmt->args) { stmt->args = std::move(arg); tail = raw; }
                else { tail->next = std::move(arg); tail = raw; }
            }
            if (idx < cnt) idx++;
        }
        require_token_end(toks, idx, "emit statement");
        advance();
        return stmt;
    }

    /* ── Phase 6: UI Framework keywords ── */
    if ((t0.is_keyword("component") || t0.is_keyword("page") || t0.is_keyword("layout")) && cnt > 1 && toks[1].is_identifier()) {
        NodeType nt = t0.is_keyword("component") ? NodeType::Component :
                      t0.is_keyword("page") ? NodeType::Page : NodeType::Layout;
        auto stmt = make_node(nt, toks[1].value, ln);
        int idx = 2;
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, (t0.value + " declaration").c_str());
        advance();
        stmt->body = parse_block(ci);
        return stmt;
    }

    if ((t0.is_keyword("theme") || t0.is_keyword("style")) && cnt > 1 && toks[1].is_identifier()) {
        NodeType nt = t0.is_keyword("theme") ? NodeType::Theme : NodeType::Style;
        auto stmt = make_node(nt, toks[1].value, ln);
        int idx = 2;
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, (t0.value + " declaration").c_str());
        advance();
        stmt->body = parse_block(ci);
        return stmt;
    }

    if (t0.is_keyword("route") && cnt > 1 && (toks[1].is_identifier() || toks[1].is_string())) {
        auto stmt = make_node(NodeType::Route, toks[1].value, ln);
        int idx = 2;
        /* Support: route "METHOD" "/path" — if second token is also a string,
           first token was the method, second is the path. */
        if (idx < cnt && toks[idx].is_string()) {
            stmt->left = make_node(NodeType::Str, stmt->value, ln);
            stmt->value = toks[idx].value;
            idx++;
        }
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, "route declaration");
        advance();
        if (cur_indent() > ci) stmt->body = parse_block(ci);
        return stmt;
    }

    /* ── state / properties ── */
    if ((t0.is_keyword("state") || t0.is_keyword("properties")) && cnt > 1 && toks[1].is_identifier()) {
        NodeType nt = t0.is_keyword("state") ? NodeType::State : NodeType::Properties;
        auto stmt = make_node(nt, toks[1].value, ln);
        int idx = 2;
        if (idx < cnt && toks[idx].is_operator('=')) {
            idx++;
            stmt->left = parse_expr(toks, idx);
        }
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, (t0.value + " declaration").c_str());
        advance();
        if (cur_indent() > ci) stmt->body = parse_block(ci);
        return stmt;
    }

    if (t0.is_keyword("render")) {
        auto stmt = make_node(NodeType::Render, "", ln);
        int idx = 1;
        /* render: — no expr, just block */
        if (idx < cnt && !toks[idx].is_operator(':')) {
            stmt->left = parse_expr(toks, idx);
        }
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, "render declaration");
        advance();
        if (cur_indent() > ci) stmt->body = parse_block(ci);
        return stmt;
    }

    if (t0.is_keyword("animate") && cnt > 1 && toks[1].is_identifier()) {
        auto stmt = make_node(NodeType::Animate, toks[1].value, ln);
        int idx = 2;
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, "animate declaration");
        advance();
        stmt->body = parse_block(ci);
        return stmt;
    }

    if (t0.is_keyword("transition") && cnt > 1 && toks[1].is_identifier()) {
        auto stmt = make_node(NodeType::Transition, toks[1].value, ln);
        int idx = 2;
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, "transition declaration");
        advance();
        stmt->body = parse_block(ci);
        return stmt;
    }

    /* ── api path: [...] block or inline ── */
    if (t0.is_keyword("api") && cnt > 1) {
        auto stmt = make_node(NodeType::Api, "", ln);
        int idx = 1;
        stmt->left = parse_expr(toks, idx); /* captures "/users" or name */
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, "api declaration");
        advance();
        if (cur_indent() > ci) stmt->body = parse_block(ci);
        return stmt;
    }

    /* ── Phase 7: Backend keywords ── */
    if ((t0.is_keyword("server") || t0.is_keyword("middleware") ||
         t0.is_keyword("database") || t0.is_keyword("model") || t0.is_keyword("cache") ||
         t0.is_keyword("session") || t0.is_keyword("auth")) && cnt > 1 && toks[1].is_identifier()) {
        NodeType nt;
        if (t0.is_keyword("server"))     nt = NodeType::Server;
        else if (t0.is_keyword("api"))        nt = NodeType::Api;
        else if (t0.is_keyword("middleware"))  nt = NodeType::Middleware;
        else if (t0.is_keyword("database"))    nt = NodeType::Database;
        else if (t0.is_keyword("model"))       nt = NodeType::Model;
        else if (t0.is_keyword("cache"))       nt = NodeType::Cache;
        else if (t0.is_keyword("session"))     nt = NodeType::Session;
        else                                   nt = NodeType::Auth;
        auto stmt = make_node(nt, toks[1].value, ln);
        int idx = 2;
        if (idx < cnt && toks[idx].is_operator('=')) {
            idx++; stmt->left = parse_expr(toks, idx);
        }
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, (t0.value + " declaration").c_str());
        advance();
        if (cur_indent() > ci) stmt->body = parse_block(ci);
        return stmt;
    }

    /* request(expr) / response(expr) — only match when followed by '(' or ':' */
    if ((t0.is_keyword("request") || t0.is_keyword("response") ||
         t0.is_keyword("query") || t0.is_keyword("token")) &&
        cnt > 1 && (toks[1].is_operator('(') || toks[1].is_operator(':'))) {
        NodeType nt = t0.is_keyword("request") ? NodeType::Request :
                      t0.is_keyword("response") ? NodeType::Response :
                      t0.is_keyword("query") ? NodeType::Query : NodeType::Token;
        auto stmt = make_node(nt, "", ln);
        int idx = 1;
        if (idx < cnt && toks[idx].is_operator('(')) {
            stmt->left = parse_expr(toks, idx);
        }
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, (t0.value + " declaration").c_str());
        advance();
        return stmt;
    }

    /* response.method(args) / request.method(args) — expression statement */
    if ((t0.is_keyword("response") || t0.is_keyword("request")) &&
        cnt > 2 && toks[1].is_operator('.') &&
        (toks[2].is_identifier() || toks[2].is(TokenKind::Keyword))) {
        NodeType nt = t0.is_keyword("response") ? NodeType::Response : NodeType::Request;
        auto stmt = make_node(nt, toks[2].value, ln);  /* method name in value */
        int idx = 3;
        if (idx < cnt && toks[idx].is_operator('(')) {
            idx++;
            ASTNode* tail = nullptr;
            while (idx < cnt && !toks[idx].is_operator(')')) {
                if (toks[idx].is_operator(',')) { idx++; continue; }
                auto arg = parse_expr(toks, idx);
                ASTNode* raw = arg.get();
                if (!stmt->args) { stmt->args = std::move(arg); tail = raw; }
                else             { tail->next = std::move(arg); tail = raw; }
            }
            if (idx < cnt) idx++;
            else throw std::runtime_error("Line " + std::to_string(ln) + ": missing ')' in " + t0.value + "." + toks[2].value + " call");
        }
        require_token_end(toks, idx, (t0.value + "." + toks[2].value + " call").c_str());
        advance();
        return stmt;
    }

    /* ── cors — CORS configuration ── */
    if (t0.is_keyword("cors")) {
        auto stmt = make_node(NodeType::Cors, "", ln);
        int idx = 1;
        if (idx < cnt && !toks[idx].is_operator(':')) {
            stmt->left = parse_expr(toks, idx);
        }
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, "cors declaration");
        advance();
        if (cur_indent() > ci) stmt->body = parse_block(ci);
        return stmt;
    }

    /* ── websocket path: body — WebSocket endpoint ── */
    if (t0.is_keyword("websocket") && cnt > 1 && toks[1].is_string()) {
        auto stmt = make_node(NodeType::WebSocket, toks[1].value, ln);
        int idx = 2;
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, "websocket declaration");
        advance();
        if (cur_indent() > ci) stmt->body = parse_block(ci);
        return stmt;
    }

    /* ── sse path: body — Server-Sent Events endpoint ── */
    if (t0.is_keyword("sse") && cnt > 1 && toks[1].is_string()) {
        auto stmt = make_node(NodeType::Sse, toks[1].value, ln);
        int idx = 2;
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, "sse declaration");
        advance();
        if (cur_indent() > ci) stmt->body = parse_block(ci);
        return stmt;
    }

    /* ── template name "source" — template compilation ── */
    if (t0.is_keyword("template") && cnt > 1 && toks[1].is_string()) {
        auto stmt = make_node(NodeType::Tpl, toks[1].value, ln);
        int idx = 2;
        if (idx < cnt && toks[idx].is_string()) {
            stmt->left = make_node(NodeType::Str, toks[idx].value, ln);
            idx++;
        } else if (idx < cnt && toks[idx].is_keyword("from") && idx + 1 < cnt && toks[idx + 1].is_string()) {
            /* template "name" from "./file.html" */
            idx++;
            stmt->left = make_node(NodeType::Str, toks[idx].value, ln);
            stmt->right = make_node(NodeType::Str, "file", ln);
            idx++;
        }
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, "template declaration");
        advance();
        return stmt;
    }

    /* ── validate: body — request validation block ── */
    if (t0.is_keyword("validate")) {
        auto stmt = make_node(NodeType::Validate, "", ln);
        int idx = 1;
        if (idx < cnt && !toks[idx].is_operator(':')) {
            stmt->left = parse_expr(toks, idx);
        }
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, "validate declaration");
        advance();
        if (cur_indent() > ci) stmt->body = parse_block(ci);
        return stmt;
    }

    /* ── Phase 8: Game Engine keywords ── */
    if ((t0.is_keyword("scene") || t0.is_keyword("entity") || t0.is_keyword("object") ||
         t0.is_keyword("sprite") || t0.is_keyword("camera") || t0.is_keyword("physics") ||
         t0.is_keyword("collision") || t0.is_keyword("audio") || t0.is_keyword("animation")) &&
        cnt > 1 && (toks[1].is_identifier() || toks[1].is(TokenKind::Keyword))) {
        NodeType nt;
        if (t0.is_keyword("scene"))      nt = NodeType::Scene;
        else if (t0.is_keyword("entity"))    nt = NodeType::Entity;
        else if (t0.is_keyword("object"))    nt = NodeType::Object;
        else if (t0.is_keyword("sprite"))    nt = NodeType::Sprite;
        else if (t0.is_keyword("camera"))    nt = NodeType::Camera;
        else if (t0.is_keyword("physics"))   nt = NodeType::Physics;
        else if (t0.is_keyword("collision")) nt = NodeType::Collision;
        else if (t0.is_keyword("audio"))     nt = NodeType::Audio;
        else                                 nt = NodeType::Animation;
        auto stmt = make_node(nt, toks[1].value, ln);
        int idx = 2;
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, (t0.value + " declaration").c_str());
        advance();
        if (cur_indent() > ci) stmt->body = parse_block(ci);
        return stmt;
    }

    if (t0.is_keyword("input")) {
        auto stmt = make_node(NodeType::Input, "", ln);
        int idx = 1;
        if (idx < cnt) stmt->left = parse_expr(toks, idx);
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, "input declaration");
        advance();
        if (cur_indent() > ci) stmt->body = parse_block(ci);
        return stmt;
    }

    if ((t0.is_keyword("update") || t0.is_keyword("tick"))) {
        NodeType nt = t0.is_keyword("update") ? NodeType::Update : NodeType::Tick;
        auto stmt = make_node(nt, "", ln);
        int idx = 1;
        if (idx < cnt && !toks[idx].is_operator(':')) {
            stmt->left = parse_expr(toks, idx);
        }
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, (t0.value + " declaration").c_str());
        advance();
        if (cur_indent() > ci) stmt->body = parse_block(ci);
        return stmt;
    }

    /* ── Phase 9: AI/ML & Time/Util ── */
    if ((t0.is_keyword("ai") || t0.is_keyword("train") || t0.is_keyword("predict") ||
         t0.is_keyword("tensor") || t0.is_keyword("neural")) && cnt > 1 &&
        (toks[1].is_identifier() || toks[1].is(TokenKind::Keyword))) {
        NodeType nt;
        if (t0.is_keyword("ai"))       nt = NodeType::Ai;
        else if (t0.is_keyword("train"))    nt = NodeType::Train;
        else if (t0.is_keyword("predict"))  nt = NodeType::Predict;
        else if (t0.is_keyword("tensor"))   nt = NodeType::Tensor;
        else                                nt = NodeType::Neural;
        auto stmt = make_node(nt, toks[1].value, ln);
        int idx = 2;
        if (idx < cnt && toks[idx].is_operator('=')) { idx++; stmt->left = parse_expr(toks, idx); }
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, (t0.value + " declaration").c_str());
        advance();
        if (cur_indent() > ci) stmt->body = parse_block(ci);
        return stmt;
    }

    if (t0.is_keyword("sleep")) {
        auto stmt = make_node(NodeType::Sleep, "", ln);
        int idx = 1;
        if (idx < cnt) stmt->left = parse_expr(toks, idx);
        require_token_end(toks, idx, "sleep declaration");
        advance();
        return stmt;
    }

    if (t0.is_keyword("time") && cnt == 1) {
        advance();
        return make_node(NodeType::Time, "", ln);
    }

    if (t0.is_keyword("random") && cnt == 1) {
        advance();
        return make_node(NodeType::Random, "", ln);
    }

    /* ── inline / noinline / constexpr as attribute prefixes ── */
    if ((t0.is_keyword("inline") || t0.is_keyword("noinline") || t0.is_keyword("constexpr"))) {
        NodeType nt = t0.is_keyword("inline") ? NodeType::Inline :
                       t0.is_keyword("noinline") ? NodeType::NoInline : NodeType::ConstExpr;
        auto stmt = make_node(nt, "", ln);
        int idx = 1;
        if (idx < cnt) stmt->left = parse_expr(toks, idx);
        require_token_end(toks, idx, (t0.value + " attribute").c_str());
        advance();
        return stmt;
    }

    /* ── return ── */
    if (t0.is_keyword("return")) {
        auto stmt = make_node(NodeType::Return, "", ln);
        int idx = 1;
        if (idx < cnt) {
            stmt->left = parse_expr(toks, idx);
            require_token_end(toks, idx, "return statement");
        } else {
            stmt->left = make_node(NodeType::Num, "0");
        }
        advance();
        return stmt;
    }

    /* ── extern / @cost — delegated to parse_extern() ── */
    if (t0.is_attribute("cost") || t0.is_keyword("extern")) {
        auto node = parse_extern();
        if (node) return node;
        /* @cost returned nullptr — pending_cost_ set, continue */
    }

    /* ── function definition ── */
    if (t0.is_keyword("function")) {
        if (cnt < 2 || !toks[1].is_identifier())
            throw std::runtime_error("Line " + std::to_string(ln) + ": function needs a name");
        std::string fname = (cnt > 1) ? toks[1].value : "";
        auto stmt = make_node(NodeType::Function, fname, ln);
        int idx = 2;

        /* ── Generic type parameters: function name[T, U](...) ── */
        if (idx < cnt && toks[idx].is_operator('[')) {
            idx++;
            ASTNode* tp_tail = nullptr;
            while (idx < cnt && !toks[idx].is_operator(']')) {
                if (toks[idx].is_operator(',')) { idx++; continue; }
                if (idx < cnt && (toks[idx].is_identifier() || toks[idx].is(TokenKind::Keyword))) {
                    auto tp = make_node(NodeType::TypeParam, toks[idx].value, ln);
                    ASTNode* raw = tp.get();
                    if (!stmt->template_params) { stmt->template_params = std::move(tp); tp_tail = raw; }
                    else                         { tp_tail->next = std::move(tp); tp_tail = raw; }
                }
                idx++;
            }
            if (idx < cnt && toks[idx].is_operator(']')) idx++;
        }

        /* params: function name(a, b, c): */
        if (idx < cnt && toks[idx].is_operator('(')) {
            idx++;
            ASTNode* ptail = nullptr;
            while (idx < cnt && !toks[idx].is_operator(')')) {
                if (toks[idx].is_operator(',')) { idx++; continue; }
                auto p = make_node(NodeType::Var, toks[idx++].value, ln);
                /* optional : type annotation (e.g., a: Int, b: T) */
                if (idx < cnt && toks[idx].is_operator(':')) {
                    idx++;
                    if (idx < cnt && (toks[idx].is_identifier() || toks[idx].is(TokenKind::Keyword))) {
                        p->right = make_node(NodeType::Var, toks[idx].value, ln);
                        idx++;
                    }
                }
                ASTNode* raw = p.get();
                if (!stmt->args) { stmt->args = std::move(p); ptail = raw; }
                else             { ptail->next = std::move(p); ptail = raw; }
            }
            if (idx < cnt) idx++;  /* ')' */
            else throw std::runtime_error("Line " + std::to_string(ln) + ": missing ')' in function parameters");
        }
        /* ── Optional return type: function name(...): RetType ── */
        if (idx < cnt && toks[idx].is_operator(':')) {
            idx++;
            if (idx < cnt && (toks[idx].is_identifier() || toks[idx].is(TokenKind::Keyword))) {
                stmt->left = make_node(NodeType::Var, toks[idx].value, ln);
                idx++;
            }
        }

        bool fn_has_brace = (idx < cnt && toks[idx].is_operator('{'));

        /* ── inline body on same line (single-expression body) ── */
        if (fn_has_brace) {
            idx++;  /* skip { */
            if (idx < cnt && !toks[idx].is_operator('}')) {
                if (toks[idx].is_keyword("return")) {
                    idx++;
                    auto body = make_node(NodeType::Return, "", ln);
                    if (idx < cnt && !toks[idx].is_operator('}'))
                        body->left = parse_expr(toks, idx);
                    if (idx < cnt && toks[idx].is_operator('}')) idx++;
                    stmt->body = std::move(body);
                } else {
                    auto body = parse_expr(toks, idx);
                    if (idx < cnt && toks[idx].is_operator('}')) idx++;
                    stmt->body = std::move(body);
                }
                advance();
                return stmt;
            }
            /* just { } — empty body, fall through to multi-line */
        } else if (idx < cnt) {
            /* function f1() return expr   or   function f1() expr */
            if (toks[idx].is_keyword("return")) {
                idx++;
                auto body = make_node(NodeType::Return, "", ln);
                if (idx < cnt) {
                    body->left = parse_expr(toks, idx);
                    require_token_end(toks, idx, "inline function body");
                } else {
                    body->left = make_node(NodeType::Num, "0");
                }
                stmt->body = std::move(body);
            } else {
                stmt->body = parse_expr(toks, idx);
                require_token_end(toks, idx, "inline function body");
            }
            advance();
            return stmt;
        }

        /* ── multi-line body ── */
        advance();

        if (fn_has_brace) {
            stmt->body = parse_brace_block(1);
        } else {
            stmt->body = parse_block(ci);
        }
        return stmt;
    }

    /* ── namespace Name: ── */
    if (t0.is_keyword("namespace")) {
        if (cnt < 2 || !toks[1].is_identifier())
            throw std::runtime_error("Line " + std::to_string(ln) + ": namespace needs a name");
        auto stmt = make_node(NodeType::NamespaceDecl, toks[1].value, ln);
        int idx = 2;
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, "namespace declaration");
        advance();
        stmt->body = parse_block(ci);
        return stmt;
    }

    /* ── module name (supports dotted paths: module com.example) ── */
    if (t0.is_keyword("module")) {
        if (cnt < 2 || !toks[1].is_identifier())
            throw std::runtime_error("Line " + std::to_string(ln) + ": module needs a name");
        std::string mname = toks[1].value;
        int idx = 2;
        while (idx < cnt && toks[idx].is_operator('.') && idx + 1 < cnt && toks[idx+1].is_identifier()) {
            mname += "." + toks[idx+1].value;
            idx += 2;
        }
        auto stmt = make_node(NodeType::ModuleDecl, mname, ln);
        require_token_end(toks, idx, "module declaration");
        advance();
        return stmt;
    }

    /* ── package name (supports dotted paths: package com.example) ── */
    if (t0.is_keyword("package")) {
        if (cnt < 2 || !toks[1].is_identifier())
            throw std::runtime_error("Line " + std::to_string(ln) + ": package needs a name");
        std::string pname = toks[1].value;
        int idx = 2;
        while (idx < cnt && toks[idx].is_operator('.') && idx + 1 < cnt && toks[idx+1].is_identifier()) {
            pname += "." + toks[idx+1].value;
            idx += 2;
        }
        auto stmt = make_node(NodeType::PackageDecl, pname, ln);
        require_token_end(toks, idx, "package declaration");
        advance();
        return stmt;
    }

    /* ── alias Name = Target ── */
    if (t0.is_keyword("alias") && cnt >= 4 && toks[2].is_operator('=')) {
        auto stmt = make_node(NodeType::AliasDecl, toks[1].value, ln);
        stmt->left = make_node(NodeType::Var, toks[3].value, ln);
        int idx = 4;
        require_token_end(toks, idx, "alias declaration");
        advance();
        return stmt;
    }

    /* ── constant / mutable as declaration prefix ── */
    if ((t0.is_keyword("constant") || t0.is_keyword("mutable")) && cnt > 2 && toks[2].is_operator('=')) {
        std::string qual = t0.value;
        auto stmt  = make_node(NodeType::Assign, "", ln);
        stmt->is_mutable = (qual == "mutable");  /* use dedicated field instead of overloading value */
        stmt->left = make_node(NodeType::Var, toks[1].value, ln);
        int idx = 3;
        stmt->right = parse_expr(toks, idx);
        require_token_end(toks, idx, (qual + " declaration").c_str());
        advance();
        return stmt;
    }

    /* ── unsafe: block ── */
    if (t0.is_keyword("unsafe")) {
        int idx = 1;
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, "unsafe block");
        advance();
        auto stmt = make_node(NodeType::UnsafeBlock, "", ln);
        stmt->body = parse_block(ci);
        return stmt;
    }

    /* ── safe: block ── */
    if (t0.is_keyword("safe")) {
        int idx = 1;
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, "safe block");
        advance();
        auto stmt = make_node(NodeType::SafeBlock, "", ln);
        stmt->body = parse_block(ci);
        return stmt;
    }

    /* ── global :: name ── */
    if (t0.is_keyword("global") && cnt > 2 && toks[1].is_operator(':')) {
        int idx = 2;
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        if (idx < cnt && (toks[idx].is_identifier() || toks[idx].is(TokenKind::Keyword))) {
            auto stmt = make_node(NodeType::Var, "global::" + toks[idx].value, ln);
            idx++;
            require_token_end(toks, idx, "global access");
            advance();
            return stmt;
        }
        advance();
        return make_node(NodeType::Pass, "", ln);
    }

    /* ── outer :: name ── */
    if (t0.is_keyword("outer") && cnt > 2 && toks[1].is_operator(':')) {
        int idx = 2;
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        if (idx < cnt && (toks[idx].is_identifier() || toks[idx].is(TokenKind::Keyword))) {
            auto stmt = make_node(NodeType::Var, "outer::" + toks[idx].value, ln);
            idx++;
            require_token_end(toks, idx, "outer access");
            advance();
            return stmt;
        }
        advance();
        return make_node(NodeType::Pass, "", ln);
    }

    /* ── struct ── */
    if (t0.is_keyword("struct"))
        return parse_struct();

    /* ── enum ── */
    if (t0.is_keyword("enum"))
        return parse_enum();

    /* ── interface ── */
    if (t0.is_keyword("interface"))
        return parse_interface();

    /* ── type alias (but not type(...) function call) ── */
    if (t0.is_keyword("type") && !(cnt > 1 && toks[1].is_operator('(')))
        return parse_type_alias();

    /* ── class ── */
    if (t0.is_keyword("class"))
        return parse_class();

    /* ── if / elseif / else chain ── */
    if (t0.is_keyword("if")) {
        int idx = 1;
        auto stmt  = make_node(NodeType::If, "", ln);
        stmt->left = parse_expr(toks, idx);
        if (idx < cnt && toks[idx].is_operator(':')) idx++;

        bool if_has_brace = (idx < cnt && toks[idx].is_operator('{'));
        if (if_has_brace) {
            idx++;
            if (idx < cnt) require_token_end(toks, idx, "if condition");
        } else {
            require_token_end(toks, idx, "if condition");
        }
        advance();

        if (if_has_brace) {
            stmt->body = parse_brace_block(1);
        } else {
            stmt->body = parse_block(ci);
        }

        ASTNode::Ptr* slot = &stmt->orelse;
        while (!at_end()) {
            if (!skip_blanks()) break;
            if (cur_indent() != ci) break;

            const auto& ctoks = cur_line().tokens;
            if (ctoks.empty()) { advance(); continue; }

            const Token& kw = ctoks[0];

            if (kw.is_keyword("elseif")) {
                int eidx = 1;
                auto elif  = make_node(NodeType::If, "elseif", cur_line().line_no);
                elif->left = parse_expr(ctoks, eidx);
                if (eidx < static_cast<int>(ctoks.size()) && ctoks[eidx].is_operator(':')) eidx++;

                bool elif_has_brace = (eidx < static_cast<int>(ctoks.size()) && ctoks[eidx].is_operator('{'));
                if (elif_has_brace) {
                    eidx++;
                    if (eidx < static_cast<int>(ctoks.size())) require_token_end(ctoks, eidx, "elseif condition");
                } else {
                    require_token_end(ctoks, eidx, "elseif condition");
                }
                advance();

                if (elif_has_brace) {
                    elif->body = parse_brace_block(1);
                } else {
                    elif->body = parse_block(ci);
                }

                ASTNode* raw = elif.get();
                *slot = std::move(elif);
                slot  = &raw->orelse;
            } else if (kw.is_keyword("else")) {
                int else_ln = cur_line().line_no;
                int eidx = 1;
                if (eidx < static_cast<int>(ctoks.size()) && ctoks[eidx].is_operator(':')) eidx++;

                bool else_has_brace = (eidx < static_cast<int>(ctoks.size()) && ctoks[eidx].is_operator('{'));
                if (else_has_brace) {
                    eidx++;
                    if (eidx < static_cast<int>(ctoks.size())) require_token_end(ctoks, eidx, "else statement");
                } else {
                    require_token_end(ctoks, eidx, "else statement");
                }
                advance();

                auto els  = make_node(NodeType::Else, "", else_ln);
                if (else_has_brace) {
                    els->body = parse_brace_block(1);
                } else {
                    els->body = parse_block(ci);
                }

                *slot     = std::move(els);
                break;
            } else {
                break;
            }
        }
        return stmt;
    }

    if (t0.is_keyword("elseif") || t0.is_keyword("else")) {
        throw std::runtime_error("Line " + std::to_string(ln) + ": '" + t0.value + "' without matching if");
    }

    /* ── lambda [name](params): body ── */
    if (t0.is_keyword("lambda")) {
        int idx = 1;
        std::string lname;
        /* If next token is an identifier (not '(' or ':'), use it as name */
        if (idx < cnt && toks[idx].is_identifier() && !toks[idx].is_operator('(')) {
            lname = toks[idx++].value;
        } else {
            static int lambda_counter = 0;
            lname = "__lambda_" + std::to_string(++lambda_counter);
        }
        auto stmt = make_node(NodeType::Lambda, lname, ln);
        if (idx < cnt && toks[idx].is_operator('(')) {
            idx++;
            ASTNode* ptail = nullptr;
            while (idx < cnt && !toks[idx].is_operator(')')) {
                if (toks[idx].is_operator(',')) { idx++; continue; }
                auto p = make_node(NodeType::Var, toks[idx++].value);
                ASTNode* raw = p.get();
                if (!stmt->args) { stmt->args = std::move(p); ptail = raw; }
                else             { ptail->next = std::move(p); ptail = raw; }
            }
            if (idx < cnt) idx++;
            else throw std::runtime_error("Line " + std::to_string(ln) + ": missing ')' in lambda parameters");
        }
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, "lambda definition");
        advance();
        stmt->body = parse_block(ci);
        return stmt;
    }

    /* ── match expr: case val: ... default: ... ── */
    if (t0.is_keyword("match") || t0.is_keyword("switch")) {
        int idx = 1;
        auto stmt = make_node(NodeType::Match, "", ln);
        stmt->left = parse_expr(toks, idx);
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, "match/switch expression");
        int match_indent = ci;
        advance();

        /* Cases stored in stmt->args (case nodes linked via next) */
        ASTNode::Ptr* slot = &stmt->args;
        bool seen_default = false;
        while (!at_end()) {
            if (!skip_blanks()) break;
            if (cur_indent() <= match_indent) break;

            const auto& ctoks = cur_line().tokens;
            int cln = cur_line().line_no;
            int ci_case = cur_indent();

            if (seen_default) {
                throw std::runtime_error("Line " + std::to_string(cln) + ": case after default");
            }

            if (ctoks[0].is_keyword("case")) {
                int cidx = 1;
                ASTNode::Ptr pattern = nullptr;
                /* Try to parse as a pattern (struct, array, variable, literal) */
                if (cidx < static_cast<int>(ctoks.size())) {
                    if (ctoks[cidx].is_operator('[')) {
                        /* Array pattern: [a, b, c] */
                        pattern = parse_pattern_from_tokens(ctoks, cidx, cln);
                    } else if (ctoks[cidx].is_identifier() || ctoks[cidx].is(TokenKind::Keyword) || ctoks[cidx].is_number()) {
                        /* Check if it's a struct pattern: Name(...) or a simple literal/variable */
                        if (ctoks[cidx].is_number()) {
                            pattern = make_node(NodeType::Num, ctoks[cidx].value, cln);
                            cidx++;
                        } else {
                            /* Identifier or keyword — could be a variable, wildcard, or struct pattern */
                            pattern = parse_pattern_from_tokens(ctoks, cidx, cln);
                        }
                    }
                }
                if (cidx < static_cast<int>(ctoks.size()) && ctoks[cidx].is_operator(':')) cidx++;
                require_token_end(ctoks, cidx, "case value");
                advance();

                std::string case_val = "";
                if (pattern && pattern->type == NodeType::Num) {
                    case_val = pattern->value;
                }
                auto case_node = make_node(NodeType::Case, case_val, cln);
                if (pattern) {
                    case_node->args = std::move(pattern);
                }
                case_node->body = parse_block(ci_case);
                ASTNode* raw = case_node.get();
                *slot = std::move(case_node);
                slot = &raw->next;
            } else if (ctoks[0].is_keyword("default")) {
                int didx = 1;
                if (didx < static_cast<int>(ctoks.size()) && ctoks[didx].is_operator(':')) didx++;
                require_token_end(ctoks, didx, "default");
                advance();
                auto def_node = make_node(NodeType::Case, "default", cln);
                def_node->body = parse_block(ci_case);
                *slot = std::move(def_node);
                seen_default = true;
                break;
            } else {
                break;
            }
        }
        return stmt;
    }

    /* ── while ── */
    if (t0.is_keyword("while")) {
        int idx = 1;
        auto stmt  = make_node(NodeType::While, "", ln);
        stmt->left = parse_expr(toks, idx);
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        bool has_brace = (idx < cnt && toks[idx].is_operator('{'));
        if (has_brace) { idx++; if (idx < cnt) require_token_end(toks, idx, "while condition"); }
        else           { require_token_end(toks, idx, "while condition"); }
        advance();
        stmt->body = has_brace ? parse_brace_block(1) : parse_block(ci);
        return stmt;
    }

    /* ── for x in expr: ── */
    if (t0.is_keyword("for")) {
        if (cnt < 4 || !toks[1].is_identifier() || !toks[2].is_keyword("in")) {
            std::string ctx;
            if (cnt > 1) ctx = " (got '" + toks[1].value + "')";
            throw std::runtime_error("Line " + std::to_string(ln) + ": expected 'for name in expression'" + ctx);
        }
        std::string var = (cnt > 1) ? toks[1].value : "";
        auto stmt  = make_node(NodeType::For, var, ln);
        int idx = 3;   /* skip var and 'in' */
        stmt->left = parse_expr(toks, idx);
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        bool has_brace = (idx < cnt && toks[idx].is_operator('{'));
        if (has_brace) { idx++; if (idx < cnt) require_token_end(toks, idx, "for statement"); }
        else           { require_token_end(toks, idx, "for statement"); }
        advance();
        stmt->body = has_brace ? parse_brace_block(1) : parse_block(ci);
        return stmt;
    }

    /* ── loop ── */
    if (t0.is_keyword("loop")) {
        auto stmt = make_node(NodeType::Loop, "", ln);
        int idx = 1;
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        bool has_brace = (idx < cnt && toks[idx].is_operator('{'));
        if (has_brace) { idx++; if (idx < cnt) require_token_end(toks, idx, "loop"); }
        else           { require_token_end(toks, idx, "loop"); }
        advance();
        stmt->body = has_brace ? parse_brace_block(1) : parse_block(ci);
        return stmt;
    }

    /* ── repeat ... until cond ── */
    if (t0.is_keyword("repeat")) {
        int idx = 1;
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, "repeat");
        advance();
        auto body = parse_block(ci);

        if (at_end() || !skip_blanks() || cur_indent() != ci) {
            throw std::runtime_error("Line " + std::to_string(ln) + ": repeat without until");
        }
        const auto& utoks = cur_line().tokens;
        if (!utoks[0].is_keyword("until")) {
            throw std::runtime_error("Line " + std::to_string(ln) + ": expected 'until' after repeat body");
        }
        int uidx = 1;
        auto stmt = make_node(NodeType::Repeat, "", ln);
        stmt->body = std::move(body);
        stmt->left = parse_expr(utoks, uidx);
        require_token_end(utoks, uidx, "until condition");
        advance();
        return stmt;
    }

    /* ── break / continue / skip ── */
    if (t0.is_keyword("break"))    { advance(); return make_node(NodeType::Break, "", ln); }
    if (t0.is_keyword("continue")) { advance(); return make_node(NodeType::Continue, "", ln); }
    if (t0.is_keyword("skip"))     { advance(); return make_node(NodeType::Skip, "", ln); }

    /* ── spawn expr ── */
    if (t0.is_keyword("spawn")) {
        auto stmt = make_node(NodeType::Spawn, "", ln);
        int idx = 1;
        stmt->left = (idx < cnt) ? parse_expr(toks, idx) : nullptr;
        if (stmt->left) require_token_end(toks, idx, "spawn expression");
        advance();
        return stmt;
    }

    /* ── wait expr / await expr ── */
    if (t0.is_keyword("wait") || t0.is_keyword("await")) {
        auto stmt = make_node(NodeType::Wait, "", ln);
        int idx = 1;
        stmt->left = (idx < cnt) ? parse_expr(toks, idx) : nullptr;
        if (stmt->left) require_token_end(toks, idx, "wait expression");
        advance();
        return stmt;
    }

    /* ── async block / function ── */
    if (t0.is_keyword("async")) {
        if (cnt > 1 && toks[1].is_keyword("function")) {
            auto stmt = make_node(NodeType::Async, "", ln);
            int idx = 2;
            if (idx < cnt && (toks[idx].is_identifier() || toks[idx].is(TokenKind::Keyword))) {
                std::string fname = toks[idx].value;
                auto fn = make_node(NodeType::Function, fname, ln);
                idx++;
                if (idx < cnt && toks[idx].is_operator('(')) {
                    idx++;
                    ASTNode* ptail = nullptr;
                    while (idx < cnt && !toks[idx].is_operator(')')) {
                        if (toks[idx].is_operator(',')) { idx++; continue; }
                        auto p = make_node(NodeType::Var, toks[idx++].value);
                        ASTNode* raw = p.get();
                        if (!fn->args) { fn->args = std::move(p); ptail = raw; }
                        else           { ptail->next = std::move(p); ptail = raw; }
                    }
                    if (idx < cnt) idx++;
                    else throw std::runtime_error("Line " + std::to_string(ln) + ": missing ')' in async function");
                }
                if (idx < cnt && toks[idx].is_operator(':')) idx++;
                require_token_end(toks, idx, "async function definition");
                advance();
                fn->body = parse_block(ci);
                stmt->body = std::move(fn);
                return stmt;
            }
        }
        /* Plain async block */
        auto stmt = make_node(NodeType::Async, "", ln);
        int idx = 1;
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, "async block");
        advance();
        stmt->body = parse_block(ci);
        return stmt;
    }

    /* ── throw ── */
    if (t0.is_keyword("throw")) {
        auto stmt = make_node(NodeType::Throw, "", ln);
        int idx = 1;
        stmt->left = (idx < cnt) ? parse_expr(toks, idx) : nullptr;
        if (stmt->left) require_token_end(toks, idx, "throw statement");
        advance();
        return stmt;
    }

    /* ── try / catch / finally ── */
    if (t0.is_keyword("try")) {
        auto stmt = make_node(NodeType::Try, "", ln);
        int idx = 1;
        if (idx < cnt && toks[idx].is_operator(':')) idx++;
        require_token_end(toks, idx, "try statement");
        advance();
        stmt->body = parse_block(ci);

        /* Parse catch block (optional) */
        if (!at_end() && skip_blanks() && cur_indent() == ci &&
            cur_line().tokens[0].is_keyword("catch")) {
            const auto& catch_toks = cur_line().tokens;
            int cidx = 1;
            std::string catch_var = "";

            /* Support catch(err) or catch err: syntax */
            if (cidx < static_cast<int>(catch_toks.size()) && catch_toks[cidx].is_operator('(')) {
                cidx++;
                if (cidx < static_cast<int>(catch_toks.size()) && catch_toks[cidx].is_identifier()) {
                    catch_var = catch_toks[cidx].value;
                    cidx++;
                }
                if (cidx < static_cast<int>(catch_toks.size()) && catch_toks[cidx].is_operator(')')) cidx++;
            } else if (cidx < static_cast<int>(catch_toks.size()) && catch_toks[cidx].is_identifier()) {
                catch_var = catch_toks[cidx].value;
                cidx++;
            }

            if (cidx < static_cast<int>(catch_toks.size()) && catch_toks[cidx].is_operator(':')) cidx++;
            require_token_end(catch_toks, cidx, "catch clause");

            /* Store catch variable name in stmt->value if present */
            if (!catch_var.empty()) stmt->value = catch_var;

            advance();
            stmt->orelse = parse_block(ci);
        }

        /* Parse finally / ensure block (optional) */
        if (!at_end() && skip_blanks() && cur_indent() == ci &&
            (cur_line().tokens[0].is_keyword("finally") ||
             cur_line().tokens[0].is_keyword("ensure"))) {
            const auto& fin_toks = cur_line().tokens;
            int fidx = 1;
            if (fidx < static_cast<int>(fin_toks.size()) && fin_toks[fidx].is_operator(':')) fidx++;
            require_token_end(fin_toks, fidx, "finally clause");
            advance();
            /* Store finally body in stmt->right (Ensure node) */
            auto finally_node = make_node(NodeType::Ensure, "", cur_line().line_no);
            finally_node->body = parse_block(ci);
            stmt->right = std::move(finally_node);
        }

        return stmt;
    }

    /* ── import / from ── */
    if (t0.is_keyword("import") || t0.is_keyword("from")) {
        std::string import_path;
        if (cnt > 1) {
            /* support import "file.aura" and import file */
            if (toks[1].is_string()) {
                import_path = toks[1].value;
            } else {
                /* For import filename, collect remaining tokens */
                for (int i = 1; i < cnt; i++)
                    import_path += toks[i].value;
            }
        }
        advance();
        return make_node(NodeType::Import, import_path, ln);
    }

    /* ════════════════════════════════════════════
       Phase 2 — Memory Management statements
       ════════════════════════════════════════════

       Syntax examples:
           move x              — transfer ownership, x becomes invalid
           drop x              — explicit destructor call (auto-inserted at scope end too)
           delete x            — manual free (unsafe, low-level)
           shared x            — create a shared (ref-counted) handle to x
           weak x              — create a weak (non-owning) handle to x
           borrow x            — temporary immutable reference

       All of these can also appear on the RHS of an assignment:
           y = move x
           y = shared x
           y = weak x
           y = borrow x
       Those cases are already handled in parse_factor().
       Here we handle the standalone statement forms.
    */

    /* ── move x ── */
    if (t0.is_keyword("move") && cnt > 1) {
        require_token_end(toks, 2, "move statement");
        auto stmt = make_node(NodeType::Move, toks[1].value, ln);
        advance(); return stmt;
    }

    /* ── drop x ── */
    if (t0.is_keyword("drop") && cnt > 1) {
        require_token_end(toks, 2, "drop statement");
        auto stmt = make_node(NodeType::Drop, toks[1].value, ln);
        advance(); return stmt;
    }

    /* ── borrow x ── */
    if (t0.is_keyword("borrow") && cnt > 1) {
        require_token_end(toks, 2, "borrow statement");
        auto stmt = make_node(NodeType::Borrow, toks[1].value, ln);
        advance(); return stmt;
    }

    /* ── shared x / weak x (standalone statement form) ── */
    if (t0.is_keyword("shared") && cnt == 2) {
        auto stmt = make_node(NodeType::SharedRef, toks[1].value, ln);
        advance(); return stmt;
    }

    if (t0.is_keyword("weak") && cnt == 2) {
        auto stmt = make_node(NodeType::WeakRef, toks[1].value, ln);
        advance(); return stmt;
    }

    /* ── delete x ── */
    if (t0.is_keyword("delete")) {
        auto stmt = make_node(NodeType::Delete, "", ln);
        int idx = 1;
        stmt->left = (idx < cnt) ? parse_expr(toks, idx) : nullptr;
        if (stmt->left) require_token_end(toks, idx, "delete statement");
        advance();
        return stmt;
    }

    /* ── copy x  /  copy x → used as expression in assignments ── */
    if (t0.is_keyword("copy")) {
        auto stmt = make_node(NodeType::Copy, "", ln);
        int idx = 1;
        if (idx < cnt)
            stmt->left = parse_expr(toks, idx);
        if (stmt->left) require_token_end(toks, idx, "copy statement");
        advance();
        return stmt;
    }

    /* ── free x ── */
    if (t0.is_keyword("free")) {
        auto stmt = make_node(NodeType::Free, "", ln);
        int idx = 1;
        if (idx < cnt)
            stmt->left = parse_expr(toks, idx);
        if (stmt->left) require_token_end(toks, idx, "free statement");
        advance();
        return stmt;
    }

    /* ── array index assign: name[expr] = expr ── */
    /* But first check if it's a generic call name[Type1, Type2](args) — if so, fall through */
    bool is_generic_call = false;
    if (t0.is(TokenKind::Identifier) && cnt > 4 && toks[1].is_operator('[')) {
        int ci = 2;
        is_generic_call = true;
        while (ci < cnt && !toks[ci].is_operator(']')) {
            if (toks[ci].is_operator(',')) { ci++; continue; }
            if (!toks[ci].is_identifier() && !toks[ci].is(TokenKind::Keyword)) { is_generic_call = false; break; }
            ci++;
        }
        if (is_generic_call)
            is_generic_call = (ci < cnt && toks[ci].is_operator(']') && ci + 1 < cnt && toks[ci + 1].is_operator('('));
    }
    if (!is_generic_call && t0.is(TokenKind::Identifier) && cnt > 3 && toks[1].is_operator('[')) {
        auto stmt  = make_node(NodeType::IndexAssign, t0.value, ln);
        int idx = 2;
        stmt->left = parse_expr(toks, idx);
        if (idx < cnt && toks[idx].is_operator(']')) idx++;
        else throw std::runtime_error(format_error_with_hint(ln, toks[1].col, "missing ']' to close index", "every opening '[' needs a matching ']'. Add ']' after the index expression."));
        if (idx < cnt && toks[idx].is_operator('=')) idx++;
        else throw std::runtime_error("Line " + std::to_string(ln) + ": expected '=' in index assignment");
        stmt->right= parse_expr(toks, idx);
        require_token_end(toks, idx, "index assignment");
        advance();
        return stmt;
    }
    /* ── shared y = x / weak y = x (assignment form) ── */
    if ((t0.is_keyword("shared") || t0.is_keyword("weak")) && cnt > 3
        && toks[1].is(TokenKind::Identifier) && toks[2].is_operator('=')) {
        auto stmt = make_node(NodeType::Assign, "", ln);
        stmt->left = make_node(NodeType::Var, toks[1].value, ln);
        int idx = 3;
        auto ref = make_node(t0.is_keyword("shared") ? NodeType::SharedRef : NodeType::WeakRef,
                             toks[3].value, ln);
        stmt->right = std::move(ref);
        idx = 4;
        require_token_end(toks, idx, "shared/weak assignment");
        advance();
        return stmt;
    }

    /* ── compound assignment: x += expr, x -= expr, x *= expr, x /= expr ── */
    if (t0.is(TokenKind::Identifier) && cnt > 3 && toks[1].is(TokenKind::Operator)) {
        const std::string& op1 = toks[1].value;
        std::string binop = "";
        int idx = 2;
        if (op1 == "+" && toks[2].is_operator('=')) {
            binop = "+"; idx = 3;
        } else if (op1 == "-" && toks[2].is_operator('=')) {
            binop = "-"; idx = 3;
        } else if (op1 == "*" && toks[2].is_operator('=')) {
            binop = "*"; idx = 3;
        } else if (op1 == "/" && toks[2].is_operator('=')) {
            binop = "/"; idx = 3;
        }

        if (!binop.empty()) {
            /* Desugar: x += expr → x = x + expr */
            auto stmt  = make_node(NodeType::Assign, "", ln);
            stmt->left = make_node(NodeType::Var, t0.value, ln);
            auto bin   = make_node(NodeType::BinOp, binop, ln);
            bin->left  = make_node(NodeType::Var, t0.value, ln);
            bin->right = parse_expr(toks, idx);
            stmt->right = std::move(bin);
            require_token_end(toks, idx, "compound assignment");
            advance();
            return stmt;
        }
    }

    /* ── assignment: name = expr ── */
    if ((t0.is(TokenKind::Identifier) || t0.is(TokenKind::Keyword)) &&
        cnt > 2 && toks[1].is_operator('=') && (cnt < 3 || !toks[2].is_operator('='))) {
        auto stmt  = make_node(NodeType::Assign, "", ln);
        stmt->left = make_node(NodeType::Var, t0.value, ln);
        int idx = 2;
        stmt->right= parse_expr(toks, idx);
        require_token_end(toks, idx, "assignment");
        advance();
        return stmt;
    }

    /* ── new ClassName(args) as statement ── */
    if (t0.is_keyword("new")) {
        int idx = 1;
        if (idx < cnt && (toks[idx].is_identifier() || toks[idx].is(TokenKind::Keyword))) {
            auto stmt = make_node(NodeType::New, toks[idx].value, ln);
            idx++;
            if (idx < cnt && toks[idx].is_operator('(')) {
                idx++;
                ASTNode* tail = nullptr;
                while (idx < cnt && !toks[idx].is_operator(')')) {
                    if (toks[idx].is_operator(',')) { idx++; continue; }
                    auto arg = parse_expr(toks, idx);
                    ASTNode* raw = arg.get();
                    if (!stmt->args) { stmt->args = std::move(arg); tail = raw; }
                    else             { tail->next = std::move(arg); tail = raw; }
                }
                if (idx < cnt) idx++;
                else throw std::runtime_error("Line " + std::to_string(ln) + ": missing ')' in constructor call");
            }
            require_token_end(toks, idx, "new statement");
            advance();
            return stmt;
        }
    }

    /* ── generic call as statement: name[Type1, Type2](args) ── */
    if (t0.is(TokenKind::Identifier) && cnt > 4 && toks[1].is_operator('[')) {
        int ci = 2;
        bool looks_like_generic = true;
        while (ci < cnt && !toks[ci].is_operator(']')) {
            if (toks[ci].is_operator(',')) { ci++; continue; }
            if (!toks[ci].is_identifier() && !toks[ci].is(TokenKind::Keyword)) { looks_like_generic = false; break; }
            ci++;
        }
        if (looks_like_generic && ci < cnt && toks[ci].is_operator(']') && ci + 1 < cnt && toks[ci + 1].is_operator('(')) {
            int idx = 0;
            auto call = parse_factor(toks, idx);
            require_token_end(toks, idx, "generic function call");
            advance();
            return call;
        }
    }

    /* ── redirect(url, code) — shortcut for response.redirect ── */
    if (t0.is_keyword("redirect") && cnt > 2 && toks[1].is_operator('(')) {
        auto stmt = make_node(NodeType::Response, "redirect", ln);
        int idx = 2;
        ASTNode* tail = nullptr;
        while (idx < cnt && !toks[idx].is_operator(')')) {
            if (toks[idx].is_operator(',')) { idx++; continue; }
            auto arg = parse_expr(toks, idx);
            ASTNode* raw = arg.get();
            if (!stmt->args) { stmt->args = std::move(arg); tail = raw; }
            else             { tail->next = std::move(arg); tail = raw; }
        }
        if (idx < cnt) idx++;
        else throw std::runtime_error("Line " + std::to_string(ln) + ": missing ')' in redirect call");
        require_token_end(toks, idx, "redirect call");
        advance();
        return stmt;
    }

    /* ── function call as statement ── */
    if ((t0.is(TokenKind::Identifier) || t0.is(TokenKind::Keyword)) && cnt > 1 && toks[1].is_operator('(')) {
        int idx = 0;
        auto call = parse_factor(toks, idx);
        require_token_end(toks, idx, "function call statement");
        advance();
        return call;
    }

    /* ── obj.field = expr  (field assignment) ── */
    if (t0.is(TokenKind::Identifier) && cnt > 3
        && toks[1].is_operator('.')
        && (toks[2].is_identifier() || toks[2].is(TokenKind::Keyword))
        && toks[3].is_operator('=')
        && (cnt < 5 || !toks[4].is_operator('='))) {
        /* build:  Assign{ left=Attribute{obj,field}, right=expr } */
        auto attr  = make_node(NodeType::Attribute, toks[2].value, ln);
        attr->left = make_node(NodeType::Var, t0.value, ln);
        auto stmt  = make_node(NodeType::Assign, "", ln);
        stmt->left = std::move(attr);
        int idx = 4;
        stmt->right = parse_expr(toks, idx);
        require_token_end(toks, idx, "field assignment");
        advance();
        return stmt;
    }

    /* ── obj.method(args)  (method call as statement) ── */
    if (t0.is(TokenKind::Identifier) && cnt > 3
        && toks[1].is_operator('.')
        && toks[2].is_identifier()
        && toks[3].is_operator('(')) {
        int idx = 0;
        auto call = parse_factor(toks, idx);
        require_token_end(toks, idx, "method call statement");
        advance();
        return call;
    }

    /* ── using Name = BaseType (type alias statement) ── */
    if (t0.is_keyword("using") && cnt >= 4 && toks[2].is_operator('=')) {
        auto stmt = make_node(NodeType::TypeAlias, toks[1].value, ln);
        stmt->left = make_node(NodeType::Var, toks[3].value, ln);
        int idx = 4;
        require_token_end(toks, idx, "using declaration");
        advance();
        return stmt;
    }

    /* ── yield expr (generator yield) ── */
    if (t0.is_keyword("yield")) {
        auto stmt = make_node(NodeType::Yield, "", ln);
        int idx = 1;
        if (idx < cnt && !toks[idx].is_operator(':')) {
            stmt->left = parse_expr(toks, idx);
        }
        require_token_end(toks, idx, "yield statement");
        advance();
        return stmt;
    }

    /* ── convert/clone as statement (function call) ── */
    if ((t0.is_keyword("convert") || t0.is_keyword("clone")) && cnt > 1 && toks[1].is_operator('(')) {
        int idx = 0;
        auto call = parse_factor(toks, idx);
        require_token_end(toks, idx, "function call");
        advance();
        return call;
    }

    /* ── panic [expr] — halt execution ── */
    if (t0.is_keyword("panic")) {
        auto stmt = make_node(NodeType::Panic, "", ln);
        int idx = 1;
        if (idx < cnt) stmt->left = parse_expr(toks, idx);
        require_token_end(toks, idx, "panic statement");
        advance();
        return stmt;
    }

    /* ── debug expr — debug output ── */
    if (t0.is_keyword("debug")) {
        auto stmt = make_node(NodeType::Debug, "", ln);
        int idx = 1;
        if (idx < cnt) stmt->left = parse_expr(toks, idx);
        require_token_end(toks, idx, "debug statement");
        advance();
        return stmt;
    }

    /* ── log expr — log output ── */
    if (t0.is_keyword("log")) {
        auto stmt = make_node(NodeType::Log, "", ln);
        int idx = 1;
        if (idx < cnt) stmt->left = parse_expr(toks, idx);
        require_token_end(toks, idx, "log statement");
        advance();
        return stmt;
    }

    /* ── pass (no-op) ── */
    if (t0.is_keyword("pass")) {
        advance();
        return make_node(NodeType::Pass, "", ln);
    }

    /* ── end (block terminator, no-op) ── */
    if (t0.is_keyword("end")) {
        advance();
        return make_node(NodeType::Pass, "", ln);
    }

    /* ── on_open / on_message / on_close — websocket event markers ── */
    if (t0.is_keyword("on_open") || t0.is_keyword("on_message") || t0.is_keyword("on_close")) {
        advance();
        return make_node(NodeType::Pass, "", ln);
    }

    std::ostringstream msg;
    msg << "Line " << ln << ": unsupported statement starting with '" << t0.value << "'";
    throw std::runtime_error(msg.str());
}

/* ── parse_pattern_from_tokens — parse a pattern from tokens starting at idx
      Patterns:
        Num          → NodeType::Num (literal integer)
        identifier   → NodeType::Var (variable binding; "_" = wildcard)
        Name(...)    → NodeType::Call (struct pattern with field patterns in args)
        [...]        → NodeType::Array (array pattern with element patterns in args)
      Updates idx to point past the pattern.                                      ── */
static ASTNode::Ptr parse_pattern_from_tokens(const std::vector<Token>& toks, int& idx, int ln) {
    if (idx >= static_cast<int>(toks.size())) return nullptr;

    const Token& t = toks[idx];

    /* Number literal */
    if (t.is_number()) {
        idx++;
        return make_node(NodeType::Num, t.value, ln);
    }

    /* Identifier or keyword */
    if (t.is_identifier() || t.is(TokenKind::Keyword)) {
        std::string name = t.value;
        idx++;

        /* Struct pattern: Name(...) */
        if (idx < static_cast<int>(toks.size()) && toks[idx].is_operator('(')) {
            idx++;
            auto struct_pat = make_node(NodeType::Call, name, ln);
            ASTNode* tail = nullptr;
            while (idx < static_cast<int>(toks.size()) && !toks[idx].is_operator(')')) {
                if (toks[idx].is_operator(',')) { idx++; continue; }
                auto fp = parse_pattern_from_tokens(toks, idx, ln);
                if (fp) {
                    ASTNode* raw = fp.get();
                    if (!struct_pat->args) { struct_pat->args = std::move(fp); tail = raw; }
                    else                   { tail->next = std::move(fp); tail = raw; }
                }
            }
            if (idx < static_cast<int>(toks.size()) && toks[idx].is_operator(')')) idx++;
            return struct_pat;
        }

        /* Variable binding or wildcard */
        return make_node(NodeType::Var, name, ln);
    }

    /* Array pattern: [...] */
    if (t.is_operator('[')) {
        idx++;
        auto arr_pat = make_node(NodeType::Array, "", ln);
        ASTNode* tail = nullptr;
        while (idx < static_cast<int>(toks.size()) && !toks[idx].is_operator(']')) {
            if (toks[idx].is_operator(',')) { idx++; continue; }
            auto ep = parse_pattern_from_tokens(toks, idx, ln);
            if (ep) {
                ASTNode* raw = ep.get();
                if (!arr_pat->args) { arr_pat->args = std::move(ep); tail = raw; }
                else                { tail->next = std::move(ep); tail = raw; }
            }
        }
        if (idx < static_cast<int>(toks.size()) && toks[idx].is_operator(']')) idx++;
        return arr_pat;
    }

    return nullptr;
}