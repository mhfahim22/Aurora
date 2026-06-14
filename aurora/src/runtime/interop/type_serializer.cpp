#include "runtime/interop/type_serializer.hpp"
#include <sstream>
#include <cassert>

static std::string json_escape(const std::string& s) {
    std::string r;
    r.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"':  r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n";  break;
            case '\t': r += "\\t";  break;
            default:   r += c;
        }
    }
    return r;
}

static std::string kind_to_json(InteropTypeKind k) {
    switch (k) {
        case InteropTypeKind::Unknown:   return "\"unknown\"";
        case InteropTypeKind::Void:      return "\"void\"";
        case InteropTypeKind::Bool:      return "\"bool\"";
        case InteropTypeKind::Int8:      return "\"i8\"";
        case InteropTypeKind::Int16:     return "\"i16\"";
        case InteropTypeKind::Int32:     return "\"i32\"";
        case InteropTypeKind::Int64:     return "\"i64\"";
        case InteropTypeKind::UInt8:     return "\"u8\"";
        case InteropTypeKind::UInt16:    return "\"u16\"";
        case InteropTypeKind::UInt32:    return "\"u32\"";
        case InteropTypeKind::UInt64:    return "\"u64\"";
        case InteropTypeKind::Float16:   return "\"f16\"";
        case InteropTypeKind::Float32:   return "\"f32\"";
        case InteropTypeKind::Float64:   return "\"f64\"";
        case InteropTypeKind::Char:      return "\"char\"";
        case InteropTypeKind::String:    return "\"string\"";
        case InteropTypeKind::CString:   return "\"cstring\"";
        case InteropTypeKind::Pointer:   return "\"pointer\"";
        case InteropTypeKind::Reference: return "\"ref\"";
        case InteropTypeKind::Array:     return "\"array\"";
        case InteropTypeKind::Slice:     return "\"slice\"";
        case InteropTypeKind::Struct:    return "\"struct\"";
        case InteropTypeKind::Union:     return "\"union\"";
        case InteropTypeKind::Enum:      return "\"enum\"";
        case InteropTypeKind::Class:     return "\"class\"";
        case InteropTypeKind::Interface: return "\"interface\"";
        case InteropTypeKind::Object:    return "\"object\"";
        case InteropTypeKind::Function:  return "\"function\"";
        case InteropTypeKind::Callback:  return "\"callback\"";
        case InteropTypeKind::List:      return "\"list\"";
        case InteropTypeKind::Map:       return "\"map\"";
        case InteropTypeKind::Set:       return "\"set\"";
        case InteropTypeKind::Tuple:     return "\"tuple\"";
        case InteropTypeKind::Optional:  return "\"optional\"";
        case InteropTypeKind::Result:    return "\"result\"";
        case InteropTypeKind::Any:       return "\"any\"";
        case InteropTypeKind::Variant:   return "\"variant\"";
        case InteropTypeKind::Future:    return "\"future\"";
        case InteropTypeKind::Alias:     return "\"alias\"";
        case InteropTypeKind::Generic:   return "\"generic\"";
    }
    return "\"unknown\"";
}

static InteropTypeKind kind_from_json(const std::string& s) {
    if (s == "void")     return InteropTypeKind::Void;
    if (s == "bool")     return InteropTypeKind::Bool;
    if (s == "i8")       return InteropTypeKind::Int8;
    if (s == "i16")      return InteropTypeKind::Int16;
    if (s == "i32")      return InteropTypeKind::Int32;
    if (s == "i64")      return InteropTypeKind::Int64;
    if (s == "u8")       return InteropTypeKind::UInt8;
    if (s == "u16")      return InteropTypeKind::UInt16;
    if (s == "u32")      return InteropTypeKind::UInt32;
    if (s == "u64")      return InteropTypeKind::UInt64;
    if (s == "f16")      return InteropTypeKind::Float16;
    if (s == "f32")      return InteropTypeKind::Float32;
    if (s == "f64")      return InteropTypeKind::Float64;
    if (s == "char")     return InteropTypeKind::Char;
    if (s == "string")   return InteropTypeKind::String;
    if (s == "cstring")  return InteropTypeKind::CString;
    if (s == "pointer")  return InteropTypeKind::Pointer;
    if (s == "ref")      return InteropTypeKind::Reference;
    if (s == "array")    return InteropTypeKind::Array;
    if (s == "slice")    return InteropTypeKind::Slice;
    if (s == "struct")   return InteropTypeKind::Struct;
    if (s == "union")    return InteropTypeKind::Union;
    if (s == "enum")     return InteropTypeKind::Enum;
    if (s == "class")    return InteropTypeKind::Class;
    if (s == "interface") return InteropTypeKind::Interface;
    if (s == "object")   return InteropTypeKind::Object;
    if (s == "function") return InteropTypeKind::Function;
    if (s == "callback") return InteropTypeKind::Callback;
    if (s == "list")     return InteropTypeKind::List;
    if (s == "map")      return InteropTypeKind::Map;
    if (s == "set")      return InteropTypeKind::Set;
    if (s == "tuple")    return InteropTypeKind::Tuple;
    if (s == "optional") return InteropTypeKind::Optional;
    if (s == "result")   return InteropTypeKind::Result;
    if (s == "any")      return InteropTypeKind::Any;
    if (s == "variant")  return InteropTypeKind::Variant;
    if (s == "future")   return InteropTypeKind::Future;
    if (s == "alias")    return InteropTypeKind::Alias;
    if (s == "generic")  return InteropTypeKind::Generic;
    return InteropTypeKind::Unknown;
}

std::string TypeSerializer::to_json(const InteropType& type) {
    std::ostringstream os;
    os << "{\n";
    os << "  \"kind\": " << kind_to_json(type.kind) << ",\n";
    if (!type.name.empty())
        os << "  \"name\": \"" << json_escape(type.name) << "\",\n";
    if (!type.original_name.empty())
        os << "  \"original_name\": \"" << json_escape(type.original_name) << "\",\n";
    if (type.array_size > 0)
        os << "  \"array_size\": " << type.array_size << ",\n";
    if (!type.element_type.empty())
        os << "  \"element_type\": \"" << json_escape(type.element_type) << "\",\n";
    if (!type.key_type.empty())
        os << "  \"key_type\": \"" << json_escape(type.key_type) << "\",\n";
    if (!type.value_type.empty())
        os << "  \"value_type\": \"" << json_escape(type.value_type) << "\",\n";
    if (!type.inner_type.empty())
        os << "  \"inner_type\": \"" << json_escape(type.inner_type) << "\",\n";
    if (!type.error_type.empty())
        os << "  \"error_type\": \"" << json_escape(type.error_type) << "\",\n";
    if (!type.alias_for.empty())
        os << "  \"alias_for\": \"" << json_escape(type.alias_for) << "\",\n";
    if (!type.fn_return.empty())
        os << "  \"fn_return\": \"" << json_escape(type.fn_return) << "\",\n";
    os << "  \"is_signed\": " << (type.is_signed ? "true" : "false") << ",\n";
    os << "  \"is_nullable\": " << (type.is_nullable ? "true" : "false") << "\n";

    if (!type.fields.empty()) {
        os << "  \"fields\": [\n";
        for (size_t i = 0; i < type.fields.size(); i++) {
            auto& f = type.fields[i];
            os << "    { \"name\": \"" << json_escape(f.name) << "\", "
               << "\"type_ref\": \"" << json_escape(f.type_ref) << "\", "
               << "\"is_optional\": " << (f.is_optional ? "true" : "false") << " }";
            if (i + 1 < type.fields.size()) os << ",";
            os << "\n";
        }
        os << "  ],\n";
    }

    if (!type.enum_variants.empty()) {
        os << "  \"enum_variants\": [\n";
        for (size_t i = 0; i < type.enum_variants.size(); i++) {
            os << "    \"" << json_escape(type.enum_variants[i]) << "\"";
            if (i + 1 < type.enum_variants.size()) os << ",";
            os << "\n";
        }
        os << "  ],\n";
    }

    if (!type.fn_params.empty()) {
        os << "  \"fn_params\": [\n";
        for (size_t i = 0; i < type.fn_params.size(); i++) {
            auto& p = type.fn_params[i];
            os << "    { \"name\": \"" << json_escape(p.name) << "\", "
               << "\"type_ref\": \"" << json_escape(p.type_ref) << "\" }";
            if (i + 1 < type.fn_params.size()) os << ",";
            os << "\n";
        }
        os << "  ],\n";
    }

    os << "}";
    return os.str();
}

std::string TypeSerializer::to_json_registry(const InteropTypeRegistry& reg) {
    std::ostringstream os;
    os << "{\n  \"types\": {\n";
    bool first = true;
    for (auto& [name, type] : reg.types) {
        if (!first) os << ",\n";
        first = false;
        os << "    \"" << json_escape(name) << "\": " << to_json(type);
    }
    os << "\n  }\n}\n";
    return os.str();
}

/* ── Minimal JSON parser for TypeSerializer output ── */
static std::string json_trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::string json_unwrap_string(const std::string& s) {
    if (s.size() < 2) return s;
    size_t start = 0, end = s.size();
    if (s[start] == '"') start++;
    if (s[end - 1] == '"') end--;
    if (start >= end) return "";
    std::string inner = s.substr(start, end - start);
    std::string r;
    for (size_t i = 0; i < inner.size(); i++) {
        if (inner[i] == '\\' && i + 1 < inner.size()) {
            switch (inner[i + 1]) {
                case '"': r += '"'; i++; break;
                case '\\': r += '\\'; i++; break;
                case 'n': r += '\n'; i++; break;
                case 't': r += '\t'; i++; break;
                default: r += inner[i];
            }
        } else {
            r += inner[i];
        }
    }
    return r;
}

static bool json_parse_bool(const std::string& s) {
    std::string t = json_trim(s);
    return t == "true";
}

/* Extract value for a named key from a JSON object string.
   Returns empty string if not found. */
static std::string json_extract(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r'))
        pos++;
    if (pos >= json.size()) return "";
    if (json[pos] == '"') {
        size_t end = pos + 1;
        while (end < json.size()) {
            if (json[end] == '\\') end += 2;
            else if (json[end] == '"') break;
            else end++;
        }
        if (end < json.size()) end++;
        return json.substr(pos, end - pos);
    }
    if (json[pos] == '{' || json[pos] == '[') {
        char open = json[pos];
        char close = (open == '{') ? '}' : ']';
        int depth = 0;
        size_t end = pos;
        while (end < json.size()) {
            if (json[end] == open) depth++;
            else if (json[end] == close) { depth--; if (depth == 0) break; }
            else if (json[end] == '"') {
                end++;
                while (end < json.size()) {
                    if (json[end] == '\\') end += 2;
                    else if (json[end] == '"') break;
                    else end++;
                }
            }
            end++;
        }
        if (end < json.size()) end++;
        return json.substr(pos, end - pos);
    }
    /* Number or boolean */
    size_t end = pos;
    while (end < json.size() && (json[end] != ',' && json[end] != '}' && json[end] != ']' && json[end] != '\n'))
        end++;
    return json_trim(json.substr(pos, end - pos));
}

static std::vector<std::string> json_parse_array(const std::string& arr_json) {
    std::vector<std::string> items;
    std::string t = json_trim(arr_json);
    if (t.empty() || t[0] != '[') return items;
    size_t i = 1;
    while (i < t.size() && t[i] != ']') {
        while (i < t.size() && (t[i] == ' ' || t[i] == '\t' || t[i] == '\n' || t[i] == '\r' || t[i] == ','))
            i++;
        if (i >= t.size() || t[i] == ']') break;
        if (t[i] == '"') {
            size_t start = i;
            i++;
            while (i < t.size()) {
                if (t[i] == '\\') i += 2;
                else if (t[i] == '"') break;
                else i++;
            }
            if (i < t.size()) i++;
            items.push_back(t.substr(start, i - start));
        } else if (t[i] == '{') {
            int depth = 0;
            size_t start = i;
            while (i < t.size()) {
                if (t[i] == '{') depth++;
                else if (t[i] == '}') { depth--; if (depth == 0) break; }
                else if (t[i] == '"') {
                    i++;
                    while (i < t.size()) {
                        if (t[i] == '\\') i += 2;
                        else if (t[i] == '"') break;
                        else i++;
                    }
                }
                i++;
            }
            if (i < t.size()) i++;
            items.push_back(t.substr(start, i - start));
        } else {
            size_t start = i;
            while (i < t.size() && t[i] != ',' && t[i] != ']' && t[i] != '\n')
                i++;
            items.push_back(json_trim(t.substr(start, i - start)));
        }
    }
    return items;
}

static std::string json_extract_field(const std::string& json, const std::string& fname) {
    std::string key_str = "\"name\": \"" + fname + "\"";
    size_t pos = json.find(key_str);
    if (pos == std::string::npos) return "";
    size_t brace_start = pos;
    while (brace_start > 0 && json[brace_start] != '{') brace_start--;
    size_t brace_end = brace_start;
    int depth = 0;
    while (brace_end < json.size()) {
        if (json[brace_end] == '{') depth++;
        else if (json[brace_end] == '}') { depth--; if (depth == 0) break; }
        brace_end++;
    }
    if (brace_end < json.size()) brace_end++;
    return json.substr(brace_start, brace_end - brace_start);
}

static InteropType json_parse_type(const std::string& json) {
    InteropType t;
    std::string j = json_trim(json);
    if (j.empty()) return t;

    std::string kind_str = json_extract(j, "kind");
    if (!kind_str.empty()) {
        std::string k = json_unwrap_string(kind_str);
        t.kind = kind_from_json(k);
    }

    std::string name_str = json_extract(j, "name");
    if (!name_str.empty()) t.name = json_unwrap_string(name_str);

    std::string orig_str = json_extract(j, "original_name");
    if (!orig_str.empty()) t.original_name = json_unwrap_string(orig_str);

    std::string arr_size_str = json_extract(j, "array_size");
    if (!arr_size_str.empty()) t.array_size = std::stoll(arr_size_str);

    std::string elem_str = json_extract(j, "element_type");
    if (!elem_str.empty()) t.element_type = json_unwrap_string(elem_str);

    std::string key_str = json_extract(j, "key_type");
    if (!key_str.empty()) t.key_type = json_unwrap_string(key_str);

    std::string val_str = json_extract(j, "value_type");
    if (!val_str.empty()) t.value_type = json_unwrap_string(val_str);

    std::string inner_str = json_extract(j, "inner_type");
    if (!inner_str.empty()) t.inner_type = json_unwrap_string(inner_str);

    std::string err_str = json_extract(j, "error_type");
    if (!err_str.empty()) t.error_type = json_unwrap_string(err_str);

    std::string alias_str = json_extract(j, "alias_for");
    if (!alias_str.empty()) t.alias_for = json_unwrap_string(alias_str);

    std::string fn_ret_str = json_extract(j, "fn_return");
    if (!fn_ret_str.empty()) t.fn_return = json_unwrap_string(fn_ret_str);

    std::string signed_str = json_extract(j, "is_signed");
    if (!signed_str.empty()) t.is_signed = json_parse_bool(signed_str);

    std::string nullable_str = json_extract(j, "is_nullable");
    if (!nullable_str.empty()) t.is_nullable = json_parse_bool(nullable_str);

    /* Parse fields */
    std::string fields_json = json_extract(j, "fields");
    if (!fields_json.empty()) {
        auto items = json_parse_array(fields_json);
        for (auto& item : items) {
            InteropField f;
            std::string fn = json_extract(item, "name");
            if (!fn.empty()) f.name = json_unwrap_string(fn);
            std::string ft = json_extract(item, "type_ref");
            if (!ft.empty()) f.type_ref = json_unwrap_string(ft);
            std::string fo = json_extract(item, "is_optional");
            if (!fo.empty()) f.is_optional = json_parse_bool(fo);
            t.fields.push_back(f);
        }
    }

    /* Parse enum variants */
    std::string variants_json = json_extract(j, "enum_variants");
    if (!variants_json.empty()) {
        auto items = json_parse_array(variants_json);
        for (auto& item : items) {
            t.enum_variants.push_back(json_unwrap_string(item));
        }
    }

    /* Parse fn_params */
    std::string params_json = json_extract(j, "fn_params");
    if (!params_json.empty()) {
        auto items = json_parse_array(params_json);
        for (auto& item : items) {
            InteropFnParam p;
            std::string pn = json_extract(item, "name");
            if (!pn.empty()) p.name = json_unwrap_string(pn);
            std::string pt = json_extract(item, "type_ref");
            if (!pt.empty()) p.type_ref = json_unwrap_string(pt);
            t.fn_params.push_back(p);
        }
    }

    return t;
}

InteropType TypeSerializer::from_json(const std::string& json) {
    return json_parse_type(json);
}

InteropTypeRegistry TypeSerializer::from_json_registry(const std::string& json) {
    InteropTypeRegistry reg;
    std::string j = json_trim(json);
    std::string types_json = json_extract(j, "types");
    if (types_json.empty()) return reg;

    auto entries = json_parse_array(types_json);
    /* The registry is stored as key-value pairs: "name": { type object } */
    /* Parse using a simpler approach: find all "name": {...} patterns */
    size_t i = 1; /* skip opening { */
    while (i < types_json.size()) {
        while (i < types_json.size() && (types_json[i] == ' ' || types_json[i] == '\t' || types_json[i] == '\n' || types_json[i] == '\r' || types_json[i] == ','))
            i++;
        if (i >= types_json.size() || types_json[i] == '}') break;

        /* Read key */
        if (types_json[i] != '"') { i++; continue; }
        size_t key_start = i;
        i++;
        while (i < types_json.size()) {
            if (types_json[i] == '\\') i += 2;
            else if (types_json[i] == '"') break;
            else i++;
        }
        if (i >= types_json.size()) break;
        i++; /* skip closing quote */
        std::string key = json_unwrap_string(types_json.substr(key_start, i - key_start));

        /* skip colon */
        while (i < types_json.size() && types_json[i] != ':') i++;
        if (i < types_json.size()) i++;
        while (i < types_json.size() && (types_json[i] == ' ' || types_json[i] == '\t' || types_json[i] == '\n' || types_json[i] == '\r'))
            i++;

        /* Read value (type object) */
        if (i < types_json.size() && types_json[i] == '{') {
            int depth = 0;
            size_t val_start = i;
            while (i < types_json.size()) {
                if (types_json[i] == '{') depth++;
                else if (types_json[i] == '}') { depth--; if (depth == 0) break; }
                else if (types_json[i] == '"') {
                    i++;
                    while (i < types_json.size()) {
                        if (types_json[i] == '\\') i += 2;
                        else if (types_json[i] == '"') break;
                        else i++;
                    }
                }
                i++;
            }
            if (i < types_json.size()) i++;
            std::string type_json = types_json.substr(val_start, i - val_start);
            InteropType parsed = json_parse_type(type_json);
            reg.register_type(key, parsed);
        }
    }

    return reg;
}

std::string TypeSerializer::to_schema() {
    return R"({
  "$schema": "https://json-schema.org/draft-07/schema#",
  "title": "Aurora Interop Type IR",
  "type": "object",
  "properties": {
    "kind": {
      "type": "string",
      "enum": [
        "void", "bool", "i8", "i16", "i32", "i64",
        "u8", "u16", "u32", "u64",
        "f16", "f32", "f64",
        "char", "string", "cstring", "pointer", "ref",
        "array", "slice", "struct", "union", "enum",
        "class", "interface", "object",
        "function", "callback",
        "list", "map", "set", "tuple",
        "optional", "result", "any", "variant", "future",
        "alias", "generic", "unknown"
      ]
    },
    "name": { "type": "string" },
    "element_type": { "type": "string" },
    "key_type": { "type": "string" },
    "value_type": { "type": "string" },
    "inner_type": { "type": "string" },
    "error_type": { "type": "string" },
    "alias_for": { "type": "string" },
    "array_size": { "type": "integer" },
    "is_signed": { "type": "boolean" },
    "is_nullable": { "type": "boolean" },
    "fields": {
      "type": "array",
      "items": {
        "type": "object",
        "properties": {
          "name": { "type": "string" },
          "type_ref": { "type": "string" },
          "is_optional": { "type": "boolean" }
        }
      }
    },
    "enum_variants": {
      "type": "array",
      "items": { "type": "string" }
    },
    "fn_params": {
      "type": "array",
      "items": {
        "type": "object",
        "properties": {
          "name": { "type": "string" },
          "type_ref": { "type": "string" }
        }
      }
    },
    "fn_return": { "type": "string" }
  },
  "required": ["kind"]
})";
}
