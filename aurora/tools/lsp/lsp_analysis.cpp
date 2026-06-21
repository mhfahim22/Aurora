#include "../../include/tools/lsp.hpp"
#include "compiler/keywords.hpp"
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

/* ════════════════════════════════════════════════════════════
   AST-walking diagnostics
   ════════════════════════════════════════════════════════════ */

std::vector<LspDiagnostic> LspServer::analyze(DocumentState& doc) {
    std::vector<LspDiagnostic> diags;
    const auto& lines = doc.lines;

    if (lines.empty()) return diags;

    /* ── Report parse errors from the real parser ── */
    if (!doc.parseError.empty()) {
        std::regex err_line_re(R"(line[:\s]+(\d+))");
        std::smatch m;
        int err_line = 0;
        if (std::regex_search(doc.parseError, m, err_line_re)) {
            err_line = std::stoi(m[1]) - 1;
        }
        int col_end = 1;
        if (err_line >= 0 && err_line < (int)lines.size()) {
            auto& toks = lines[err_line].tokens;
            if (!toks.empty()) col_end = toks.back().col + (int)toks.back().value.size();
        }
        add_diag(diags, err_line, 0, col_end, 1, doc.parseError, "aurorac");
    }

    /* ── Use the real AST for semantic analysis ── */
    if (doc.ast) {
        std::set<std::string> defined_funcs;
        std::set<std::string> defined_classes;
        std::map<std::string, int> defined_vars;
        std::map<std::string, int> used_vars;
        bool in_function = false;
        int func_start_line = -1;
        std::string func_name;
        bool has_return = false;

        int function_depth = 0;
        walk_ast(doc.ast.get(), [&](ASTNode* n) {
            if (n->type == NodeType::Function) {
                defined_funcs.insert(n->value);
                if (function_depth > 0) {
                    /* nested function — don't report false positive */
                } else {
                    in_function = true;
                    func_start_line = n->src_line;
                    func_name = n->value;
                    has_return = false;
                }
                function_depth++;
            }
            if (n->type == NodeType::Return) {
                has_return = true;
            }
            if (n->type == NodeType::Class) {
                defined_classes.insert(n->value);
            }
        });

        /* Check for unused variables using the AST */
        walk_ast(doc.ast.get(), [&](ASTNode* n) {
            if (n->type == NodeType::Var && n->src_line > 0) {
                std::string vname = n->value;
                const auto& keywords = aurora_keywords();
                if (keywords.find(vname) == keywords.end() && !vname.empty()) {
                    if (defined_vars.find(vname) == defined_vars.end()) {
                        defined_vars[vname] = n->src_line;
                    }
                }
            }
        });

        /* Collect var uses with count */
        std::map<std::string, int> use_count;
        walk_ast(doc.ast.get(), [&](ASTNode* n) {
            if (n->type == NodeType::Var && n->src_line > 0) {
                std::string vname = n->value;
                const auto& keywords = aurora_keywords();
                if (keywords.find(vname) == keywords.end() && !vname.empty()) {
                    used_vars[vname] = n->src_line;
                    use_count[vname]++;
                }
            }
        });

        /* Report unused variables (must be used more than once — definition alone doesn't count) */
        for (auto& [vname, def_line] : defined_vars) {
            if (use_count[vname] < 2) {
                add_diag(diags, def_line - 1, 0, 1, 3, "Unused variable '" + vname + "'", "aurora-lint");
            }
        }
    }

    /* ── Lexer-based structural checks ── */
    bool in_class = false;
    int class_start_line = -1;
    bool in_fn = false;
    int fn_start_line = -1;

    for (size_t i = 0; i < lines.size(); i++) {
        const auto& ll = lines[i];
        const auto& toks = ll.tokens;
        if (toks.empty()) continue;

        /* Check brace balance per line */
        int open_br = 0, close_br = 0;
        for (auto& t : toks) {
            if (t.is_operator('{')) open_br++;
            if (t.is_operator('}')) close_br++;
        }

        /* Track function/class structure from lexed tokens */
        if (toks.size() >= 2 && toks[0].is_keyword("function") && toks[1].is_identifier()) {
            if (in_fn) {
                add_diag(diags, fn_start_line, 0, 1, 1, "Missing 'end function' for previous function", "aurora-lint");
            }
            in_fn = true;
            fn_start_line = (int)i;
        }
        if (in_fn) {
            bool found_end = false;
            for (auto& t : toks) {
                if (t.is_keyword("end")) { found_end = true; break; }
            }
            if (found_end) in_fn = false;
        }

        if (toks.size() >= 2 && toks[0].is_keyword("class") && toks[1].is_identifier()) {
            if (in_class) {
                add_diag(diags, class_start_line, 0, 1, 1, "Missing 'end class' for previous class", "aurora-lint");
            }
            in_class = true;
            class_start_line = (int)i;
        }
        if (in_class) {
            for (auto& t : toks) {
                if (t.is_keyword("end")) { in_class = false; break; }
            }
        }
    }

    if (in_fn) {
        add_diag(diags, fn_start_line, 0, 1, 1, "Missing 'end function' to close function", "aurora-lint");
    }
    if (in_class) {
        add_diag(diags, class_start_line, 0, 1, 1, "Missing 'end class' to close class", "aurora-lint");
    }

    return diags;
}

void LspServer::update_diagnostics(const std::string& uri, const std::string& text) {
    if (!documents_.count(uri)) return;
    auto& doc = documents_[uri];
    auto diags = analyze(doc);

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

/* ════════════════════════════════════════════════════════════
   AST-walking completions
   ════════════════════════════════════════════════════════════ */

std::vector<LspCompletionItem> LspServer::get_completions(DocumentState& doc, int line, int col) {
    std::vector<LspCompletionItem> items;
    std::set<std::string> seen;

    /* Keywords from the real keyword table */
    for (auto& kw : aurora_keywords()) {
        if (seen.count(kw)) continue;
        seen.insert(kw);
        LspCompletionItem ci;
        ci.label = kw;
        ci.kind = 14;
        ci.detail = "keyword";
        ci.insertText = kw;
        items.push_back(ci);
    }

    /* Builtin functions */
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
        {"sizeof", "Size of type in bytes"},
        {"typeof", "Get type name as string"},
        {"convert", "Convert between types"},
        {"read_file", "Read file contents"},
        {"write_file", "Write file contents"},
        {"file_exists", "Check if file exists"},
        {"abs", "Absolute value"},
        {"sqrt", "Square root"},
        {"floor", "Floor value"},
        {"ceil", "Ceiling value"},
        {"round", "Round value"},
        {"pow", "Power"},
        {"clamp", "Clamp value"},
    };
    for (auto& [name, desc] : builtins) {
        if (seen.count(name)) continue;
        seen.insert(name);
        LspCompletionItem ci;
        ci.label = name;
        ci.kind = 3;
        ci.detail = desc;
        ci.documentation = desc;
        ci.insertText = name;
        items.push_back(ci);
    }

    /* Identifiers from lexed tokens across all open documents */
    /* TODO: cache this — rebuilding on every keystroke is O(N) across all docs */
    for (auto& [uri, d] : documents_) {
        for (auto& ll : d.lines) {
            for (auto& t : ll.tokens) {
                if (t.type == TokenType::Identifier) {
                    if (seen.count(t.value)) continue;
                    seen.insert(t.value);
                    LspCompletionItem ci;
                    ci.label = t.value;
                    ci.kind = 6;
                    ci.detail = "identifier";
                    ci.insertText = t.value;
                    items.push_back(ci);
                }
            }
        }
    }

    /* Function names from AST */
    if (doc.ast) {
        walk_ast(doc.ast.get(), [&](ASTNode* n) {
            if (n->type == NodeType::Function) {
                if (seen.count(n->value)) return;
                seen.insert(n->value);
                LspCompletionItem ci;
                ci.label = n->value;
                ci.kind = 2;
                ci.detail = "function";
                ci.insertText = n->value;
                items.push_back(ci);
            }
            if (n->type == NodeType::Class) {
                if (seen.count(n->value)) return;
                seen.insert(n->value);
                LspCompletionItem ci;
                ci.label = n->value;
                ci.kind = 7;
                ci.detail = "class";
                ci.insertText = n->value;
                items.push_back(ci);
            }
        });
    }

    std::sort(items.begin(), items.end(), [](auto& a, auto& b) {
        if (a.kind != b.kind) return a.kind < b.kind;
        return a.label < b.label;
    });

    return items;
}

/* ════════════════════════════════════════════════════════════
   AST-walking symbols
   ════════════════════════════════════════════════════════════ */

std::vector<LspSymbol> LspServer::get_symbols(DocumentState& doc) {
    std::vector<LspSymbol> symbols;

    if (!doc.ast) return symbols;

    /* Types and symbol kinds mapping */
    struct SymbolInfo {
        NodeType type;
        int kind;
    };

    walk_ast(doc.ast.get(), [&](ASTNode* n) {
        int kind = 0;
        std::string kind_str;

        switch (n->type) {
            case NodeType::Function:
                kind = 12; kind_str = "function"; break;
            case NodeType::Class:
                kind = 5; kind_str = "class"; break;
            case NodeType::InterfaceDecl:
                kind = 11; kind_str = "interface"; break;
            case NodeType::EnumDecl:
                kind = 10; kind_str = "enum"; break;
            case NodeType::StructDecl:
                kind = 23; kind_str = "struct"; break;
            case NodeType::ExternFn:
                kind = 12; kind_str = "function"; break;
            case NodeType::Lambda:
                kind = 12; kind_str = "function"; break;
            case NodeType::NamespaceDecl:
                kind = 2; kind_str = "namespace"; break;
            case NodeType::Var:
                /* Check if this is a variable definition at indent start */
                if (n->src_line > 0 && n->value.size() > 0 && !n->left) {
                    kind = 13; kind_str = "variable";
                } else {
                    return;
                }
                break;
            default:
                return;
        }

        if (n->value.empty()) return;

        LspSymbol sym;
        sym.name = n->value;
        sym.kind = kind;
        sym.range.start = {n->src_line - 1, 0};
        sym.range.end = {n->src_line - 1, (int)n->value.size()};
        sym.selectionRange = sym.range;
        symbols.push_back(sym);
    });

    return symbols;
}

/* ════════════════════════════════════════════════════════════
   Definition / Hover / References / Signature using AST + lexer
   ════════════════════════════════════════════════════════════ */

std::string LspServer::get_definition_location(DocumentState& doc, int line, int col) {
    std::string word = doc.get_word_at(line, col);
    if (word.empty()) return "";

    if (doc.ast) {
        ASTNode* def = find_def(doc.ast.get(), word);
        if (def && def->src_line > 0) {
            JsonBuilder jb;
            jb.add("uri", escape_json(doc.uri));
            std::string rng = "{\"start\":{\"line\":" + std::to_string(def->src_line - 1) +
                              ",\"character\":0}" +
                              ",\"end\":{\"line\":" + std::to_string(def->src_line - 1) +
                              ",\"character\":" + std::to_string((int)def->value.size()) + "}}";
            jb.add_raw("range", rng);
            return jb.str();
        }
    }

    /* Fallback to lexer-based search across open docs */
    for (auto& [uri, d] : documents_) {
        for (auto& ll : d.lines) {
            const auto& toks = ll.tokens;
            if (toks.size() >= 2) {
                bool is_def = false;
                if (toks[0].is_keyword("function") && toks[1].is_identifier() && toks[1].value == word) is_def = true;
                if (toks[0].is_keyword("class") && toks[1].is_identifier() && toks[1].value == word) is_def = true;
                if (toks[0].is_keyword("struct") && toks[1].is_identifier() && toks[1].value == word) is_def = true;
                if (toks[0].is_keyword("enum") && toks[1].is_identifier() && toks[1].value == word) is_def = true;
                if (toks[0].is_keyword("interface") && toks[1].is_identifier() && toks[1].value == word) is_def = true;

                if (is_def) {
                    JsonBuilder jb;
                    jb.add("uri", escape_json(uri));
                    std::string rng = "{\"start\":{\"line\":" + std::to_string(ll.line_no - 1) +
                                      ",\"character\":0}" +
                                      ",\"end\":{\"line\":" + std::to_string(ll.line_no - 1) +
                                      ",\"character\":" + std::to_string(toks[1].col + (int)toks[1].value.size()) + "}}";
                    jb.add_raw("range", rng);
                    return jb.str();
                }
            }
        }
    }

    return "";
}

std::string LspServer::get_hover_info(DocumentState& doc, int line, int col) {
    std::string word = doc.get_word_at(line, col);
    if (word.empty()) return make_response(current_msg_id_, "{\"contents\":[]}");

    std::string hover_text = "```\n" + word + "\n```";

    /* Check if it's a keyword */
    if (is_aurora_keyword(word)) {
        hover_text = "**" + word + "** (keyword)";
        JsonBuilder jb;
        jb.add("contents", escape_json(hover_text));
        return make_response(current_msg_id_, jb.str());
    }

    /* Look up in AST */
    if (doc.ast) {
        walk_ast(doc.ast.get(), [&](ASTNode* n) {
            if (n->value == word && n->type == NodeType::Function) {
                std::string sig = "function " + word + "(...)";
                hover_text += "\n---\n" + sig + "\n*Defined at line " + std::to_string(n->src_line) + "*";
            } else if (n->value == word && n->type == NodeType::Class) {
                std::string sig = "class " + word;
                hover_text += "\n---\n" + sig + "\n*Defined at line " + std::to_string(n->src_line) + "*";
            } else if (n->value == word && n->type == NodeType::Var) {
                hover_text += "\n---\n*Variable defined at line " + std::to_string(n->src_line) + "*";
            }
        });
    }

    /* Fallback lexer search using boolean flag */
    bool found = false;
    for (auto& [uri, d] : documents_) {
        if (found) break;
        for (auto& ll : d.lines) {
            if (found) break;
            const auto& toks = ll.tokens;
            for (size_t i = 0; i < toks.size(); i++) {
                if (toks[i].is_identifier() && toks[i].value == word && i > 0) {
                    if (toks[i-1].is_keyword("function") || toks[i-1].is_keyword("class") ||
                        toks[i-1].is_keyword("struct") || toks[i-1].is_keyword("enum")) {
                        hover_text += "\n---\n*Defined at line " + std::to_string(ll.line_no) + "*";
                        found = true;
                        break;
                    }
                }
            }
        }
    }

    JsonBuilder jb;
    jb.add("contents", escape_json(hover_text));
    return make_response(current_msg_id_, jb.str());
}

std::vector<LspLocation> LspServer::get_all_references(DocumentState& doc, int line, int col) {
    std::vector<LspLocation> refs;
    std::string word = doc.get_word_at(line, col);
    if (word.empty()) return refs;

    if (doc.ast) {
        auto nodes = find_all_refs(doc.ast.get(), word);
        for (auto* n : nodes) {
            if (n->src_line > 0) {
                LspLocation loc;
                loc.uri = doc.uri;
                loc.range.start = {n->src_line - 1, 0};
                loc.range.end = {n->src_line - 1, (int)n->value.size()};
                refs.push_back(loc);
            }
        }
    }

    /* Also add lexer-based references across all docs */
    for (auto& [uri, d] : documents_) {
        for (auto& ll : d.lines) {
            for (auto& t : ll.tokens) {
                if (t.type == TokenType::Identifier && t.value == word) {
                    LspLocation loc;
                    loc.uri = uri;
                    loc.range.start = {ll.line_no - 1, t.col};
                    loc.range.end = {ll.line_no - 1, t.col + (int)t.value.size()};
                    /* Deduplicate */
                    bool dup = false;
                    for (auto& r : refs) {
                        if (r.uri == loc.uri && r.range.start.line == loc.range.start.line &&
                            r.range.start.character == loc.range.start.character) {
                            dup = true; break;
                        }
                    }
                    if (!dup) refs.push_back(loc);
                }
            }
        }
    }

    return refs;
}

std::string LspServer::get_signature_info(DocumentState& doc, int line, int col) {
    std::string text = doc.text;
    std::string ln;
    {
        std::istringstream ss(text);
        int l = 0;
        while (l < line && std::getline(ss, ln)) l++;
        if (l != line) std::getline(ss, ln);
    }

    /* Find the function name before cursor */
    int paren = (int)ln.rfind('(', col);
    if (paren == std::string::npos) return make_response(current_msg_id_, "null");

    int name_end = paren;
    int name_start = name_end;
    while (name_start > 0 && (isalnum(ln[name_start-1]) || ln[name_start-1] == '_'))
        name_start--;

    std::string func_name = ln.substr(name_start, name_end - name_start);
    if (func_name.empty()) return make_response(current_msg_id_, "null");

    /* Count active parameter */
    int active_param = 0;
    int depth = 0;
    for (int i = paren + 1; i < col; i++) {
        if (i >= (int)ln.size()) break;
        if (ln[i] == '(') depth++;
        else if (ln[i] == ')') depth--;
        else if (ln[i] == ',' && depth == 0) active_param++;
    }

    /* Look up function def in AST */
    if (doc.ast) {
        std::string sig_label;
        std::vector<std::string> param_labels;
        walk_ast(doc.ast.get(), [&](ASTNode* n) {
            if (n->type == NodeType::Function && n->value == func_name && sig_label.empty()) {
                sig_label = "function " + func_name + "(";
                /* Walk args */
                ASTNode* arg = n->args.get();
                while (arg) {
                    std::string pl = arg->value;
                    param_labels.push_back(pl);
                    if (sig_label.back() != '(') sig_label += ", ";
                    sig_label += pl;
                    arg = arg->next.get();
                }
                sig_label += ")";
            }
        });

        if (!sig_label.empty()) {
            std::string sig_json = R"({
                "signatures":[{
                    "label":")" + escape_json(sig_label) + R"(",
                    "parameters":[)";
            for (size_t i = 0; i < param_labels.size(); i++) {
                if (i > 0) sig_json += ",";
                sig_json += R"({"label":")" + escape_json(param_labels[i]) + R"("})";
            }
            sig_json += R"(],
                    "activeParameter":)" + std::to_string(active_param) + R"(
                }],
                "activeSignature":0
            })";
            return make_response(current_msg_id_, sig_json);
        }
    }

    /* Fallback lexer search */
    for (auto& [uri, d] : documents_) {
        for (auto& ll : d.lines) {
            const auto& toks = ll.tokens;
            for (size_t i = 0; i + 1 < toks.size(); i++) {
                if (toks[i].is_keyword("function") && toks[i+1].is_identifier() && toks[i+1].value == func_name) {
                    std::string label = "function " + func_name + "(";
                    std::vector<std::string> params;
                    for (size_t j = i + 2; j < toks.size(); j++) {
                        if (toks[j].is_operator('(')) continue;
                        if (toks[j].is_operator(')')) break;
                        if (toks[j].is_operator(',')) continue;
                        if (toks[j].type == TokenType::Identifier) {
                            params.push_back(toks[j].value);
                            if (label.back() != '(') label += ", ";
                            label += toks[j].value;
                        }
                    }
                    label += ")";

                    std::string sig_json = R"({
                        "signatures":[{
                            "label":")" + escape_json(label) + R"(",
                            "parameters":[)";
                    for (size_t k = 0; k < params.size(); k++) {
                        if (k > 0) sig_json += ",";
                        sig_json += R"({"label":")" + escape_json(params[k]) + R"("})";
                    }
                    sig_json += R"(],
                            "activeParameter":)" + std::to_string(active_param) + R"(
                        }],
                        "activeSignature":0
                    })";
                    return make_response(current_msg_id_, sig_json);
                }
            }
        }
    }

    return make_response(current_msg_id_, "null");
}
