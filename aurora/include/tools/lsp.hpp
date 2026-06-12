#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <sstream>
#include <memory>

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

/* ── Simple JSON parser (recursive-descent) ── */
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
struct LspPosition { int line; int character; };
struct LspRange { LspPosition start; LspPosition end; };
struct LspLocation { std::string uri; LspRange range; };
struct LspDiagnostic {
    LspRange range;
    int severity; /* 1=error,2=warning,3=info,4=hint */
    std::string message;
    std::string source;
};
struct LspCompletionItem {
    std::string label;
    std::string detail;
    std::string documentation;
    std::string insertText;
    int kind; /* 1=text,2=method,3=function,4=constructor,5=field,6=variable,7=class,8=interface,9=module,10=property,11=unit,12=value,13=enum,14=keyword,15=snippet,16=color,17=file,18=reference,19=folder,20=enumMember,21=constant,22=struct,23=event,24=operator,25=typeParameter */
};
struct LspSymbol {
    std::string name;
    std::string kind; /* "function", "class", "variable", "module", etc */
    LspRange range;
    LspRange selectionRange;
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
    std::string handle_completion(const std::string& params);
    std::string handle_definition(const std::string& params);
    std::string handle_hover(const std::string& params);
    std::string handle_references(const std::string& params);
    std::string handle_document_symbol(const std::string& params);
    std::string handle_signature_help(const std::string& params);
    void handle_did_open(const std::string& params);
    void handle_did_change(const std::string& params);
    void handle_did_save(const std::string& params);
    void handle_did_close(const std::string& params);

    /* Analysis */
    void update_diagnostics(const std::string& uri, const std::string& text);
    std::vector<LspDiagnostic> analyze(const std::string& text);
    std::vector<LspCompletionItem> get_completions(const std::string& text, int line, int col);
    std::vector<std::string> get_keywords();
    std::vector<LspSymbol> get_symbols(const std::string& text);

    /* JSON helpers */
    std::string get_json_field(const std::string& json, const std::string& field);
    std::string escape_json(const std::string& s);

    /* State */
    std::map<std::string, std::string> open_documents_; /* uri -> text */
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
