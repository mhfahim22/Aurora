#pragma once
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>

/* ════════════════════════════════════════════════════════════
   Minimal recursive-descent JSON parser
   Handles: null, bool, number, string, array, object
   Used by: voss bridge (PyPI/npm/Cargo API responses)
   ════════════════════════════════════════════════════════════ */

struct JsonValue {
    enum Type { Null, Bool, Number, String, Array, Object } type{Null};
    bool    bool_val{false};
    double  num_val{0.0};
    std::string str_val;
    std::vector<JsonValue> arr;
    std::vector<std::pair<std::string, JsonValue>> obj;

    /* ── Field accessors ── */
    const JsonValue* get(const std::string& key) const {
        if (type != Object) return nullptr;
        for (auto& [k, v] : obj)
            if (k == key) return &v;
        return nullptr;
    }

    std::string get_string(const std::string& key, const std::string& fallback = "") const {
        auto* v = get(key);
        if (v && v->type == String) return v->str_val;
        if (v && v->type == Number) return std::to_string(v->num_val);
        return fallback;
    }

    /* ── Nested access: parse({"info", "version"}) ── */
    const JsonValue* nested(std::initializer_list<const char*> keys) const {
        const JsonValue* cur = this;
        for (auto* k : keys) {
            if (!cur || cur->type != Object) return nullptr;
            cur = cur->get(k);
        }
        return cur;
    }

    std::string nested_str(std::initializer_list<const char*> keys,
                            const std::string& fallback = "") const {
        auto* v = nested(keys);
        if (v && v->type == String) return v->str_val;
        if (v && v->type == Number) return std::to_string(v->num_val);
        return fallback;
    }
};

class JsonParser {
public:
    static JsonValue parse(const std::string& input) {
        JsonParser p(input);
        return p.parse_value();
    }

private:
    const std::string& s;
    size_t pos{0};

    JsonParser(const std::string& input) : s(input) {}

    void skip_ws() {
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' ||
                                  s[pos] == '\n' || s[pos] == '\r'))
            pos++;
    }

    char peek() { skip_ws(); return pos < s.size() ? s[pos] : '\0'; }
    char next() { skip_ws(); return pos < s.size() ? s[pos++] : '\0'; }

    /* ── Expect a specific char ── */
    bool expect(char c) {
        skip_ws();
        if (pos < s.size() && s[pos] == c) { pos++; return true; }
        return false;
    }

    /* ── Parse string "..." ── */
    std::string parse_string() {
        if (next() != '"') return {};
        std::string out;
        while (pos < s.size()) {
            char c = s[pos++];
            if (c == '"') return out;
            if (c == '\\' && pos < s.size()) {
                char esc = s[pos++];
                switch (esc) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u': { /* skip unicode */ out += "?"; break; }
                    default: out += esc; break;
                }
            } else {
                out += c;
            }
        }
        return out;
    }

    /* ── Parse number ── */
    double parse_number() {
        skip_ws();
        size_t start = pos;
        if (pos < s.size() && s[pos] == '-') pos++;
        while (pos < s.size() && (s[pos] >= '0' && s[pos] <= '9')) pos++;
        if (pos < s.size() && s[pos] == '.') {
            pos++;
            while (pos < s.size() && (s[pos] >= '0' && s[pos] <= '9')) pos++;
        }
        if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) {
            pos++;
            if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) pos++;
            while (pos < s.size() && (s[pos] >= '0' && s[pos] <= '9')) pos++;
        }
        return std::strtod(s.c_str() + start, nullptr);
    }

    /* ── Parse value ── */
    JsonValue parse_value() {
        char c = peek();
        switch (c) {
            case '"': {
                JsonValue v;
                v.type = JsonValue::String;
                v.str_val = parse_string();
                return v;
            }
            case '{': return parse_object();
            case '[': return parse_array();
            case 't': {
                pos += 4; /* true */
                JsonValue v;
                v.type = JsonValue::Bool;
                v.bool_val = true;
                return v;
            }
            case 'f': {
                pos += 5; /* false */
                JsonValue v;
                v.type = JsonValue::Bool;
                v.bool_val = false;
                return v;
            }
            case 'n': {
                pos += 4; /* null */
                return {};
            }
            default: {
                if (c == '-' || (c >= '0' && c <= '9')) {
                    JsonValue v;
                    v.type = JsonValue::Number;
                    v.num_val = parse_number();
                    return v;
                }
                return {};
            }
        }
    }

    /* ── Parse {"key": value, ...} ── */
    JsonValue parse_object() {
        JsonValue v;
        v.type = JsonValue::Object;
        if (!expect('{')) return v;
        if (peek() == '}') { next(); return v; }

        while (true) {
            std::string key = parse_string();
            if (!expect(':')) break;
            JsonValue val = parse_value();
            v.obj.push_back({key, val});

            if (peek() == '}') { next(); return v; }
            if (!expect(',')) break;
        }
        /* Skip to end on error */
        while (pos < s.size() && s[pos] != '}') pos++;
        if (pos < s.size()) pos++;
        return v;
    }

    /* ── Parse [value, ...] ── */
    JsonValue parse_array() {
        JsonValue v;
        v.type = JsonValue::Array;
        if (!expect('[')) return v;
        if (peek() == ']') { next(); return v; }

        while (true) {
            v.arr.push_back(parse_value());
            if (peek() == ']') { next(); return v; }
            if (!expect(',')) break;
        }
        while (pos < s.size() && s[pos] != ']') pos++;
        if (pos < s.size()) pos++;
        return v;
    }
};
