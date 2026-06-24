#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <sstream>
#include <memory>
#include "compiler/ast.hpp"
#include "compiler/lexer.hpp"

/* ── JSON value type ── */
struct JsonValue {
    enum Type { Null, Bool, Number, StringVal, Array, Object };
    Type type = Null;
    std::string str;
    double num = 0;
    bool boolean = false;
    std::vector<JsonValue> items;
    std::map<std::string, JsonValue> members;
    JsonValue get(const std::string& key) const;
    JsonValue get(int index) const;
    std::string as_string(const std::string& def = "") const;
    int as_int(int def = 0) const;
    bool as_bool(bool def = false) const;
    bool has(const std::string& key) const;
};

/* ── Simple JSON parser ── */
class JsonParser {
public:
    static JsonValue parse(const std::string& json);
    static std::string stringify(const JsonValue& v);
private:
    JsonParser(const std::string& input) : s(input), pos(0) {}
    void skip_ws();
    JsonValue parse_value();
    JsonValue parse_string();
    JsonValue parse_number();
    JsonValue parse_object();
    JsonValue parse_array();
    JsonValue parse_bool_or_null();
    const std::string& s;
    size_t pos;
};

/* ── LSP types ── */
struct LspPosition { uint32_t line; uint32_t character; };
struct LspRange { LspPosition start; LspPosition end; };
struct LspLocation { std::string uri; LspRange range; };
struct LspDiagnostic {
    LspRange range;
    int severity;
    std::string message;
    std::string source;
};
struct LspCompletionItem {
    std::string label;
    std::string detail;
    std::string documentation;
    std::string insertText;
    int kind;
};
struct LspSymbol {
    std::string name;
    int kind = 0;
    LspRange range;
    LspRange selectionRange;
};
struct LspFoldingRange {
    int startLine;
    int endLine;
    std::string kind;
};
struct LspTextEdit {
    LspRange range;
    std::string newText;
};
struct LspCodeAction {
    std::string title;
    std::string kind;
    std::vector<LspTextEdit> edits;
    std::string diagnosticMessage;
};

/* ── Document state: caches lexed tokens + parsed AST per document ── */
struct DocumentState {
    std::string uri;
    std::string text;
    std::vector<LexedLine> lines;
    ASTNode::Ptr ast;
    std::string parseError;
    bool dirty = true;

    void update(const std::string& newText);
    void reparse();
    const LexedLine* find_line(int line) const;
    const Token* find_token(int line, int col) const;
    std::string get_word_at(int line, int col) const;
};

/* ── Semantic token types ── */
struct SemanticToken {
    int line;
    int startChar;
    int length;
    int tokenType;
    int tokenModifiers;
};

/* ── LSP Server ── */
class LspServer {
public:
    LspServer();
    void run();

private:
    /* JSON-RPC */
    std::string read_message();
    void send_message(const std::string& json);
    std::string make_response(const std::string& id, const std::string& result);
    std::string make_error(const std::string& id, int code, const std::string& msg);
    std::string make_notification(const std::string& method, const std::string& params);

    /* LSP handlers */
    void handle_message(const std::string& json);
    std::string handle_initialize(const std::string& params);
    std::string handle_shutdown();

    /* Text document handlers */
    void handle_did_open(const std::string& params);
    void handle_did_change(const std::string& params);
    void handle_did_save(const std::string& params);
    void handle_did_close(const std::string& params);

    /* Language features */
    std::string handle_completion(const std::string& params);
    std::string handle_definition(const std::string& params);
    std::string handle_hover(const std::string& params);
    std::string handle_references(const std::string& params);
    std::string handle_document_symbol(const std::string& params);
    std::string handle_signature_help(const std::string& params);
    std::string handle_semantic_tokens(const std::string& params);
    std::string handle_formatting(const std::string& params);
    std::string handle_rename(const std::string& params);
    std::string handle_folding_range(const std::string& params);
    std::string handle_code_action(const std::string& params);

    /* Analysis using real AST */
    void update_diagnostics(const std::string& uri, const std::string& text);
    std::vector<LspDiagnostic> analyze(DocumentState& doc);
    std::vector<LspCompletionItem> get_completions(DocumentState& doc, int line, int col);
    std::vector<LspSymbol> get_symbols(DocumentState& doc);
    std::vector<SemanticToken> get_semantic_tokens(DocumentState& doc);
    std::vector<LspFoldingRange> get_folding_ranges(DocumentState& doc);
    std::vector<LspCodeAction> get_code_actions(DocumentState& doc, const std::string& diagMsg);
    std::vector<LspTextEdit> get_formatting_edits(DocumentState& doc);
    std::string get_definition_location(DocumentState& doc, int line, int col);
    std::string get_hover_info(DocumentState& doc, int line, int col);
    std::string get_signature_info(DocumentState& doc, int line, int col);
    std::vector<LspLocation> get_all_references(DocumentState& doc, int line, int col);

    /* AST walking helpers */
    static void walk_ast(ASTNode* node, std::function<void(ASTNode*)> fn);
    static ASTNode* find_def(ASTNode* root, const std::string& name);
    static std::vector<ASTNode*> find_all_refs(ASTNode* root, const std::string& name);
    static std::string node_type_name(NodeType t);
    static int node_to_symbol_kind(NodeType t);

    /* JSON helpers */
    std::string get_json_field(const std::string& json, const std::string& field);
    std::string escape_json(const std::string& s);

    /* State */
    std::map<std::string, DocumentState> documents_;
    bool initialized_ = false;
    bool shutdown_ = false;
    std::string workspace_root_;
    std::string current_msg_id_;
};

/* ── Simple JSON builder ── */
class JsonBuilder {
public:
    JsonBuilder() : first_(true) { ss_ << "{"; }
    void add(const std::string& key, const std::string& val);
    void add_int(const std::string& key, int val);
    void add_bool(const std::string& key, bool val);
    void add_null(const std::string& key);
    void add_raw(const std::string& key, const std::string& raw);
    std::string str() const { return ss_.str() + "}"; }
private:
    std::ostringstream ss_;
    bool first_;
    void comma();
};
