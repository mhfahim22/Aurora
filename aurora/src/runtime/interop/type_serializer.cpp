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
