#include "../../include/tools/lsp.hpp"
#include "compiler/parser.hpp"
#include <iostream>
#include <sstream>
#include <regex>
#include <stdexcept>
#include <algorithm>

/* ════════════════════════════════════════════════════════════
   DocumentState implementation
   ════════════════════════════════════════════════════════════ */

void DocumentState::update(const std::string& newText) {
    text = newText;
    dirty = true;
}

void DocumentState::reparse() {
    if (!dirty) return;
    dirty = false;

    Lexer lexer;
    lines = lexer.lex(text);

    try {
        Parser parser(lines);
        ast = parser.parse();
        parseError.clear();
    } catch (const std::exception& e) {
        ast.reset();
        parseError = e.what();
    }
}

const LexedLine* DocumentState::find_line(int line) const {
    if (line < 0 || line >= (int)lines.size()) return nullptr;
    return &lines[line];
}

const Token* DocumentState::find_token(int line, int col) const {
    auto* ll = find_line(line);
    if (!ll) return nullptr;
    for (auto& t : ll->tokens) {
        if (t.col <= col && col < t.col + (int)t.value.size())
            return &t;
    }
    return nullptr;
}

std::string DocumentState::get_word_at(int line, int col) const {
    auto* ll = find_line(line);
    if (!ll) return "";

    std::istringstream ss(text);
    std::string cur_line;
    int l = 0;
    while (l < line && std::getline(ss, cur_line)) l++;
    if (l != line) return "";
    std::getline(ss, cur_line);

    int start = col;
    while (start > 0 && (isalnum(cur_line[start-1]) || cur_line[start-1] == '_')) start--;
    int end = col;
    while (end < (int)cur_line.size() && (isalnum(cur_line[end]) || cur_line[end] == '_')) end++;
    if (start < end) return cur_line.substr(start, end - start);
    return "";
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

/* ════════════════════════════════════════════════════════════
   Message routing
   ════════════════════════════════════════════════════════════ */

void LspServer::handle_message(const std::string& json) {
    JsonValue msg = JsonParser::parse(json);
    std::string method = msg.has("method") ? msg.get("method").as_string() : "";
    if (msg.has("id")) current_msg_id_ = msg.get("id").as_string();
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
    } else if (method == "textDocument/semanticTokens/full") {
        response = handle_semantic_tokens(JsonParser::stringify(params));
    } else if (method == "textDocument/formatting") {
        response = handle_formatting(JsonParser::stringify(params));
    } else if (method == "textDocument/rename") {
        response = handle_rename(JsonParser::stringify(params));
    } else if (method == "textDocument/foldingRange") {
        response = handle_folding_range(JsonParser::stringify(params));
    } else if (method == "textDocument/codeAction") {
        response = handle_code_action(JsonParser::stringify(params));
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

/* ════════════════════════════════════════════════════════════
   Initialize / Shutdown
   ════════════════════════════════════════════════════════════ */

std::string LspServer::handle_initialize(const std::string& params) {
    initialized_ = true;

    JsonValue p = JsonParser::parse(params);
    workspace_root_ = p.get("rootUri").as_string();
    if (workspace_root_.empty()) workspace_root_ = p.get("rootPath").as_string();

    std::string caps = R"({
        "textDocumentSync":{"openClose":true,"change":2,"save":true},
        "completionProvider":{"triggerCharacters":[".","@"," "]},
        "definitionProvider":true,
        "hoverProvider":true,
        "referencesProvider":true,
        "documentSymbolProvider":true,
        "signatureHelpProvider":{"triggerCharacters":["("]},
        "semanticTokensProvider":{
            "legend":{
                "tokenTypes":["variable","function","method","class","interface","enum","struct","parameter","property","keyword","comment","string","number","operator","type","namespace","macro","modifier","event","decorator"],
                "tokenModifiers":["declaration","definition","readonly","static","abstract","deprecated","async","modification","documentation"]
            },
            "full":true,
            "range":false
        },
        "documentFormattingProvider":true,
        "renameProvider":{"prepareProvider":false},
        "foldingRangeProvider":true,
        "codeActionProvider":true
    })";

    JsonBuilder result;
    result.add_raw("capabilities", caps);
    JsonBuilder info;
    info.add("name", "aurora-lsp");
    info.add("version", "1.0.0");
    result.add_raw("serverInfo", info.str());
    return make_response(current_msg_id_, result.str());
}

std::string LspServer::handle_shutdown() {
    shutdown_ = true;
    return make_response(current_msg_id_, "null");
}

/* ════════════════════════════════════════════════════════════
   Document lifecycle handlers
   ════════════════════════════════════════════════════════════ */

void LspServer::handle_did_open(const std::string& params) {
    JsonValue p = JsonParser::parse(params);
    std::string uri = p.get("textDocument").get("uri").as_string();
    std::string text = p.get("textDocument").get("text").as_string();

    if (!uri.empty()) {
        DocumentState doc;
        doc.uri = uri;
        doc.update(text);
        doc.reparse();
        documents_[uri] = std::move(doc);
        update_diagnostics(uri, text);
    }
}

void LspServer::handle_did_change(const std::string& params) {
    JsonValue p = JsonParser::parse(params);
    std::string uri = p.get("textDocument").get("uri").as_string();

    if (!uri.empty() && documents_.count(uri)) {
        JsonValue changes = p.get("contentChanges");
        if (changes.type == JsonValue::Array && !changes.items.empty()) {
            documents_[uri].update(changes.items.back().get("text").as_string());
            documents_[uri].reparse();
        }
        update_diagnostics(uri, documents_[uri].text);
    }
}

void LspServer::handle_did_save(const std::string& params) {
    JsonValue p = JsonParser::parse(params);
    std::string uri = p.get("textDocument").get("uri").as_string();
    if (!uri.empty() && documents_.count(uri)) {
        documents_[uri].reparse();
        update_diagnostics(uri, documents_[uri].text);
    }
}

void LspServer::handle_did_close(const std::string& params) {
    JsonValue p = JsonParser::parse(params);
    std::string uri = p.get("textDocument").get("uri").as_string();
    if (!uri.empty()) documents_.erase(uri);
}

/* ════════════════════════════════════════════════════════════
   Feature handlers (delegate to analysis)
   ════════════════════════════════════════════════════════════ */

std::string LspServer::handle_completion(const std::string& params) {
    JsonValue p = JsonParser::parse(params);
    std::string uri = p.get("textDocument").get("uri").as_string();
    int line = p.get("position").get("line").as_int();
    int col = p.get("position").get("character").as_int();

    if (!documents_.count(uri)) return make_response(current_msg_id_, "null");
    auto& doc = documents_[uri];
    auto items = get_completions(doc, line, col);

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

    if (!documents_.count(uri)) return make_response(current_msg_id_, "null");
    auto& doc = documents_[uri];
    std::string loc = get_definition_location(doc, line, col);
    if (!loc.empty())
        return make_response(current_msg_id_, loc);
    return make_response(current_msg_id_, "null");
}

std::string LspServer::handle_hover(const std::string& params) {
    JsonValue p = JsonParser::parse(params);
    std::string uri = p.get("textDocument").get("uri").as_string();
    int line = p.get("position").get("line").as_int();
    int col = p.get("position").get("character").as_int();

    if (!documents_.count(uri)) return make_response(current_msg_id_, "null");
    auto& doc = documents_[uri];
    return get_hover_info(doc, line, col);
}

std::string LspServer::handle_references(const std::string& params) {
    JsonValue p = JsonParser::parse(params);
    std::string uri = p.get("textDocument").get("uri").as_string();
    int line = p.get("position").get("line").as_int();
    int col = p.get("position").get("character").as_int();

    if (!documents_.count(uri)) return make_response(current_msg_id_, "null");
    auto& doc = documents_[uri];
    auto refs = get_all_references(doc, line, col);

    std::string refs_json = "[";
    for (size_t i = 0; i < refs.size(); i++) {
        if (i > 0) refs_json += ",";
        auto& r = refs[i];
        JsonBuilder jb;
        jb.add("uri", escape_json(r.uri));
        std::string rng = "{\"start\":{\"line\":" + std::to_string(r.range.start.line) +
                          ",\"character\":" + std::to_string(r.range.start.character) + "}" +
                          ",\"end\":{\"line\":" + std::to_string(r.range.end.line) +
                          ",\"character\":" + std::to_string(r.range.end.character) + "}}";
        jb.add_raw("range", rng);
        refs_json += jb.str();
    }
    refs_json += "]";
    return make_response(current_msg_id_, refs_json);
}

std::string LspServer::handle_document_symbol(const std::string& params) {
    JsonValue p = JsonParser::parse(params);
    std::string uri = p.get("textDocument").get("uri").as_string();

    if (!documents_.count(uri)) return make_response(current_msg_id_, "null");
    auto& doc = documents_[uri];
    auto symbols = get_symbols(doc);

    std::string sym_json = "[";
    for (size_t i = 0; i < symbols.size(); i++) {
        auto& s = symbols[i];
        if (i > 0) sym_json += ",";
        JsonBuilder jb;
        jb.add("name", escape_json(s.name));
        jb.add_int("kind", s.kind);
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

    if (!documents_.count(uri)) return make_response(current_msg_id_, "null");
    auto& doc = documents_[uri];
    return get_signature_info(doc, line, col);
}

std::string LspServer::handle_semantic_tokens(const std::string& params) {
    JsonValue p = JsonParser::parse(params);
    std::string uri = p.get("textDocument").get("uri").as_string();

    if (!documents_.count(uri)) return make_response(current_msg_id_, "null");
    auto& doc = documents_[uri];
    auto tokens = get_semantic_tokens(doc);

    std::string data = "[";
    for (size_t i = 0; i < tokens.size(); i++) {
        if (i > 0) data += ",";
        data += std::to_string(tokens[i].line) + ",";
        data += std::to_string(tokens[i].startChar) + ",";
        data += std::to_string(tokens[i].length) + ",";
        data += std::to_string(tokens[i].tokenType) + ",";
        data += std::to_string(tokens[i].tokenModifiers);
    }
    data += "]";

    JsonBuilder result;
    result.add_raw("data", data);
    return make_response(current_msg_id_, result.str());
}

std::string LspServer::handle_formatting(const std::string& params) {
    JsonValue p = JsonParser::parse(params);
    std::string uri = p.get("textDocument").get("uri").as_string();

    if (!documents_.count(uri)) return make_response(current_msg_id_, "null");
    auto& doc = documents_[uri];
    doc.reparse();
    auto edits = get_formatting_edits(doc);

    std::string edits_json = "[";
    for (size_t i = 0; i < edits.size(); i++) {
        if (i > 0) edits_json += ",";
        auto& e = edits[i];
        JsonBuilder jb;
        std::string rng = "{\"start\":{\"line\":" + std::to_string(e.range.start.line) +
                          ",\"character\":" + std::to_string(e.range.start.character) + "}" +
                          ",\"end\":{\"line\":" + std::to_string(e.range.end.line) +
                          ",\"character\":" + std::to_string(e.range.end.character) + "}}";
        jb.add_raw("range", rng);
        jb.add("newText", escape_json(e.newText));
        edits_json += jb.str();
    }
    edits_json += "]";
    return make_response(current_msg_id_, edits_json);
}

std::string LspServer::handle_rename(const std::string& params) {
    JsonValue p = JsonParser::parse(params);
    std::string uri = p.get("textDocument").get("uri").as_string();
    int line = p.get("position").get("line").as_int();
    int col = p.get("position").get("character").as_int();
    std::string newName = p.get("newName").as_string();

    if (!documents_.count(uri)) return make_response(current_msg_id_, "null");
    auto& doc = documents_[uri];
    std::string word = doc.get_word_at(line, col);
    if (word.empty()) return make_response(current_msg_id_, "null");

    auto refs = get_all_references(doc, line, col);

    std::string edits_json = "[";
    bool first = true;
    for (auto& r : refs) {
        if (!first) edits_json += ",";
        first = false;
        JsonBuilder jb;
        std::string rng = "{\"start\":{\"line\":" + std::to_string(r.range.start.line) +
                          ",\"character\":" + std::to_string(r.range.start.character) + "}" +
                          ",\"end\":{\"line\":" + std::to_string(r.range.end.line) +
                          ",\"character\":" + std::to_string(r.range.end.character) + "}}";
        jb.add_raw("range", rng);
        jb.add("newText", escape_json(newName));
        edits_json += jb.str();
    }
    edits_json += "]";

    std::string changes = "{\"" + escape_json(uri) + "\":" + edits_json + "}";
    JsonBuilder result;
    result.add_raw("changes", changes);
    return make_response(current_msg_id_, result.str());
}

std::string LspServer::handle_folding_range(const std::string& params) {
    JsonValue p = JsonParser::parse(params);
    std::string uri = p.get("textDocument").get("uri").as_string();

    if (!documents_.count(uri)) return make_response(current_msg_id_, "null");
    auto& doc = documents_[uri];
    auto ranges = get_folding_ranges(doc);

    std::string json = "[";
    for (size_t i = 0; i < ranges.size(); i++) {
        if (i > 0) json += ",";
        JsonBuilder jb;
        jb.add_int("startLine", ranges[i].startLine);
        jb.add_int("endLine", ranges[i].endLine);
        if (!ranges[i].kind.empty())
            jb.add("kind", ranges[i].kind);
        json += jb.str();
    }
    json += "]";
    return make_response(current_msg_id_, json);
}

std::string LspServer::handle_code_action(const std::string& params) {
    JsonValue p = JsonParser::parse(params);
    std::string uri = p.get("textDocument").get("uri").as_string();

    if (!documents_.count(uri)) return make_response(current_msg_id_, "null");
    auto& doc = documents_[uri];

    std::string diagMsg;
    JsonValue context = p.get("context");
    JsonValue diags = context.get("diagnostics");
    if (diags.type == JsonValue::Array && !diags.items.empty()) {
        diagMsg = diags.items[0].get("message").as_string();
    }

    auto actions = get_code_actions(doc, diagMsg);
    std::string json = "[";
    for (size_t i = 0; i < actions.size(); i++) {
        if (i > 0) json += ",";
        auto& a = actions[i];
        JsonBuilder jb;
        jb.add("title", escape_json(a.title));
        jb.add("kind", escape_json(a.kind));
        json += jb.str();
    }
    json += "]";
    return make_response(current_msg_id_, json);
}

/* ════════════════════════════════════════════════════════════
   AST walking helpers
   ════════════════════════════════════════════════════════════ */

void LspServer::walk_ast(ASTNode* node, std::function<void(ASTNode*)> fn) {
    if (!node) return;
    fn(node);
    walk_ast(node->left.get(), fn);
    walk_ast(node->right.get(), fn);
    walk_ast(node->body.get(), fn);
    walk_ast(node->orelse.get(), fn);
    walk_ast(node->next.get(), fn);
    walk_ast(node->args.get(), fn);
}

ASTNode* LspServer::find_def(ASTNode* root, const std::string& name) {
    ASTNode* result = nullptr;
    walk_ast(root, [&](ASTNode* n) {
        if (result) return;
        if (n->type == NodeType::Function && n->value == name) {
            result = n;
        } else if (n->type == NodeType::Class && n->value == name) {
            result = n;
        } else if (n->type == NodeType::Var && n->value == name && n->left && n->left->type == NodeType::Assign) {
            result = n;
        }
    });
    return result;
}

std::vector<ASTNode*> LspServer::find_all_refs(ASTNode* root, const std::string& name) {
    std::vector<ASTNode*> refs;
    walk_ast(root, [&](ASTNode* n) {
        if (n->value == name &&
            (n->type == NodeType::Var || n->type == NodeType::Call ||
             n->type == NodeType::Function || n->type == NodeType::Class))
            refs.push_back(n);
    });
    return refs;
}

std::string LspServer::node_type_name(NodeType t) {
    switch (t) {
        case NodeType::Function: return "function";
        case NodeType::Class: return "class";
        case NodeType::InterfaceDecl: return "interface";
        case NodeType::EnumDecl: return "enum";
        case NodeType::StructDecl: return "struct";
        case NodeType::Var: return "variable";
        case NodeType::Call: return "function";
        case NodeType::Lambda: return "function";
        case NodeType::ExternFn: return "function";
        case NodeType::Import: return "module";
        case NodeType::NamespaceDecl: return "namespace";
        default: return "variable";
    }
}

int LspServer::node_to_symbol_kind(NodeType t) {
    switch (t) {
        case NodeType::Function: return 12;
        case NodeType::Class: return 5;
        case NodeType::InterfaceDecl: return 11;
        case NodeType::EnumDecl: return 10;
        case NodeType::StructDecl: return 23;
        case NodeType::Lambda: return 12;
        case NodeType::ExternFn: return 12;
        case NodeType::Var: return 13;
        case NodeType::NamespaceDecl: return 2;
        case NodeType::ModuleDecl: return 2;
        default: return 13;
    }
}

/* ════════════════════════════════════════════════════════════
   run — main event loop
   ════════════════════════════════════════════════════════════ */

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
