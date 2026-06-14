/* ════════════════════════════════════════════════════════════
   lsp.cpp — Aurora Language Server Protocol implementation
   Communicates via stdin/stdout using JSON-RPC 2.0.
   ════════════════════════════════════════════════════════════ */

#include "../../include/tools/lsp.hpp"
#include <iostream>
#include <sstream>
#include <regex>

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
