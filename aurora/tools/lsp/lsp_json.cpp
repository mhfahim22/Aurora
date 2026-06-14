#include "../../include/tools/lsp.hpp"
#include <sstream>
#include <cstdlib>
#include <cmath>

/* ── JsonValue helpers ── */

JsonValue JsonValue::get(const std::string& key) const {
    if (type == Object) {
        auto it = members.find(key);
        if (it != members.end()) return it->second;
    }
    return {};
}

JsonValue JsonValue::get(int index) const {
    if (type == Array && index >= 0 && index < (int)items.size())
        return items[index];
    return {};
}

std::string JsonValue::as_string(const std::string& def) const {
    if (type == StringVal) return str;
    if (type == Number) return std::to_string((long long)num);
    return def;
}

int JsonValue::as_int(int def) const {
    return type == Number ? (int)num : def;
}

bool JsonValue::as_bool(bool def) const {
    return type == Bool ? boolean : def;
}

bool JsonValue::has(const std::string& key) const {
    return type == Object && members.find(key) != members.end();
}

/* ── JsonParser implementation ── */

void JsonParser::skip_ws() {
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' ||
                              s[pos] == '\n' || s[pos] == '\r'))
        pos++;
}

JsonValue JsonParser::parse_value() {
    skip_ws();
    if (pos >= s.size()) return {};
    if (s[pos] == '"') return parse_string();
    if (s[pos] == '{') return parse_object();
    if (s[pos] == '[') return parse_array();
    if (s[pos] == 't' || s[pos] == 'f' || s[pos] == 'n')
        return parse_bool_or_null();
    return parse_number();
}

JsonValue JsonParser::parse_string() {
    JsonValue v;
    v.type = JsonValue::StringVal;
    pos++;
    while (pos < s.size() && s[pos] != '"') {
        if (s[pos] == '\\') {
            pos++;
            if (pos >= s.size()) break;
            switch (s[pos]) {
                case '"': v.str += '"'; break;
                case '\\': v.str += '\\'; break;
                case '/': v.str += '/'; break;
                case 'b': v.str += '\b'; break;
                case 'f': v.str += '\f'; break;
                case 'n': v.str += '\n'; break;
                case 'r': v.str += '\r'; break;
                case 't': v.str += '\t'; break;
                default: v.str += s[pos]; break;
            }
            pos++;
        } else {
            v.str += s[pos++];
        }
    }
    if (pos < s.size()) pos++;
    return v;
}

JsonValue JsonParser::parse_number() {
    JsonValue v;
    v.type = JsonValue::Number;
    size_t start = pos;
    if (pos < s.size() && s[pos] == '-') pos++;
    while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') pos++;
    if (pos < s.size() && s[pos] == '.') {
        pos++;
        while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') pos++;
    }
    if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) {
        pos++;
        if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) pos++;
        while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') pos++;
    }
    v.num = std::strtod(s.substr(start, pos - start).c_str(), nullptr);
    return v;
}

JsonValue JsonParser::parse_object() {
    JsonValue v;
    v.type = JsonValue::Object;
    pos++;
    skip_ws();
    if (pos < s.size() && s[pos] == '}') { pos++; return v; }
    while (pos < s.size()) {
        skip_ws();
        JsonValue key = parse_string();
        skip_ws();
        if (pos < s.size() && s[pos] == ':') pos++;
        JsonValue val = parse_value();
        v.members[key.str] = val;
        skip_ws();
        if (pos < s.size() && s[pos] == '}') { pos++; return v; }
        if (pos < s.size() && s[pos] == ',') { pos++; }
    }
    return v;
}

JsonValue JsonParser::parse_array() {
    JsonValue v;
    v.type = JsonValue::Array;
    pos++;
    skip_ws();
    if (pos < s.size() && s[pos] == ']') { pos++; return v; }
    while (pos < s.size()) {
        v.items.push_back(parse_value());
        skip_ws();
        if (pos < s.size() && s[pos] == ']') { pos++; return v; }
        if (pos < s.size() && s[pos] == ',') { pos++; }
    }
    return v;
}

JsonValue JsonParser::parse_bool_or_null() {
    JsonValue v;
    if (s.substr(pos, 4) == "true") {
        v.type = JsonValue::Bool; v.boolean = true; pos += 4;
    } else if (s.substr(pos, 5) == "false") {
        v.type = JsonValue::Bool; v.boolean = false; pos += 5;
    } else if (s.substr(pos, 4) == "null") {
        pos += 4;
    }
    return v;
}

JsonValue JsonParser::parse(const std::string& json) {
    JsonParser p(json);
    return p.parse_value();
}

static void json_stringify_value(std::ostringstream& out, const JsonValue& v) {
    switch (v.type) {
        case JsonValue::Null: out << "null"; break;
        case JsonValue::Bool: out << (v.boolean ? "true" : "false"); break;
        case JsonValue::Number: out << v.num; break;
        case JsonValue::StringVal:
            out << '"';
            for (char c : v.str) {
                switch (c) {
                    case '"': out << "\\\""; break;
                    case '\\': out << "\\\\"; break;
                    case '\n': out << "\\n"; break;
                    case '\r': out << "\\r"; break;
                    case '\t': out << "\\t"; break;
                    default: out << c;
                }
            }
            out << '"';
            break;
        case JsonValue::Array:
            out << '[';
            for (size_t i = 0; i < v.items.size(); i++) {
                if (i > 0) out << ',';
                json_stringify_value(out, v.items[i]);
            }
            out << ']';
            break;
        case JsonValue::Object:
            out << '{';
            for (auto it = v.members.begin(); it != v.members.end(); ++it) {
                if (it != v.members.begin()) out << ',';
                out << '"' << it->first << '"' << ':';
                json_stringify_value(out, it->second);
            }
            out << '}';
            break;
    }
}

std::string JsonParser::stringify(const JsonValue& v) {
    std::ostringstream out;
    json_stringify_value(out, v);
    return out.str();
}

/* ── JsonBuilder implementation ── */

void JsonBuilder::comma() { if (!first_) ss_ << ","; first_ = false; }

void JsonBuilder::add(const std::string& key, const std::string& val) {
    comma(); ss_ << "\"" << key << "\":\"" << val << "\"";
}

void JsonBuilder::add_int(const std::string& key, int val) {
    comma(); ss_ << "\"" << key << "\":" << val;
}

void JsonBuilder::add_bool(const std::string& key, bool val) {
    comma(); ss_ << "\"" << key << "\":" << (val ? "true" : "false");
}

void JsonBuilder::add_null(const std::string& key) {
    comma(); ss_ << "\"" << key << "\":null";
}

void JsonBuilder::add_raw(const std::string& key, const std::string& raw) {
    comma(); ss_ << "\"" << key << "\":" << raw;
}
