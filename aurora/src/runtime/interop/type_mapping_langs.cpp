#include "runtime/interop/type_mapping.hpp"
#include <cstdlib>
#include <cstring>

/* ═══════════════════════════════════════════════════════════════
   C Mapper
   ═══════════════════════════════════════════════════════════════ */

void CMapper::register_builtins(InteropTypeRegistry& reg) const {
    reg.add_mapping("void", "void");
    reg.add_mapping("_Bool", "bool");
    reg.add_mapping("char", "i8");
    reg.add_mapping("signed char", "i8");
    reg.add_mapping("unsigned char", "u8");
    reg.add_mapping("short", "i16");
    reg.add_mapping("unsigned short", "u16");
    reg.add_mapping("int", "i32");
    reg.add_mapping("unsigned int", "u32");
    reg.add_mapping("long", "i64");
    reg.add_mapping("unsigned long", "u64");
    reg.add_mapping("long long", "i64");
    reg.add_mapping("unsigned long long", "u64");
    reg.add_mapping("float", "f32");
    reg.add_mapping("double", "f64");
    reg.add_mapping("long double", "f64");
    reg.add_mapping("size_t", "u64");
    reg.add_mapping("ssize_t", "i64");
    reg.add_mapping("intptr_t", "i64");
    reg.add_mapping("uintptr_t", "u64");
    reg.add_mapping("ptrdiff_t", "i64");
    reg.add_mapping("wchar_t", "i32");
    reg.add_mapping("char16_t", "u16");
    reg.add_mapping("char32_t", "u32");
    reg.add_mapping("char*", "cstring");
    reg.add_mapping("const char*", "cstring");
    reg.add_mapping("char**", "pointer");
    reg.add_mapping("void*", "pointer");
    reg.add_mapping("FILE*", "pointer");
}

MappedType CMapper::map_to_ir(const std::string& native_type, const InteropTypeRegistry& reg) const {
    MappedType result;
    result.native_type = native_type;
    std::string t = native_type;

    while (!t.empty() && t.front() == ' ') t.erase(0, 1);

    /* Check for pointer types before stripping trailing markers */
    if (t.find('*') != std::string::npos) {
        std::string base = t;
        while (!base.empty() && (base.back() == ' ' || base.back() == '*')) base.pop_back();
        auto* ir = reg.get_type(base);
        if (ir) {
            result.ir_type = InteropType::make_pointer(ir->name);
            return result;
        }
        result.ir_type = InteropType::make_primitive(InteropTypeKind::Pointer, "pointer");
        return result;
    }

    while (!t.empty() && t.back() == ' ') t.pop_back();

    auto* ir = reg.get_type(t);
    if (ir) {
        result.ir_type = *ir;
        if (t == "int" || t == "float" || t == "double" || t == "char" ||
            t.find("int") == 0 || t.find("char") == 0) {
            result.is_trivially_castable = true;
            result.cost = InteropCost::ZeroCost;
        }
        return result;
    }

    result.ir_type.kind = InteropTypeKind::Unknown;
    result.ir_type.name = t;
    return result;
}

std::string CMapper::map_from_ir(const InteropType& ir, const InteropTypeRegistry& reg) const {
    switch (ir.kind) {
        case InteropTypeKind::Void:      return "void";
        case InteropTypeKind::Bool:      return "_Bool";
        case InteropTypeKind::Int8:      return "int8_t";
        case InteropTypeKind::Int16:     return "int16_t";
        case InteropTypeKind::Int32:     return "int32_t";
        case InteropTypeKind::Int64:     return "int64_t";
        case InteropTypeKind::UInt8:     return "uint8_t";
        case InteropTypeKind::UInt16:    return "uint16_t";
        case InteropTypeKind::UInt32:    return "uint32_t";
        case InteropTypeKind::UInt64:    return "uint64_t";
        case InteropTypeKind::Float32:   return "float";
        case InteropTypeKind::Float64:   return "double";
        case InteropTypeKind::Char:      return "char";
        case InteropTypeKind::String:    return "char*";
        case InteropTypeKind::CString:   return "char*";
        case InteropTypeKind::Pointer:
        case InteropTypeKind::Reference: return "void*";
        case InteropTypeKind::Struct:    return "struct " + ir.name;
        case InteropTypeKind::Union:     return "union " + ir.name;
        case InteropTypeKind::Enum:      return ir.name;
        case InteropTypeKind::Array:     return ir.element_type + "*";
        case InteropTypeKind::Alias:     return ir.alias_for;
        default:                         return "void*";
    }
}

/* ═══════════════════════════════════════════════════════════════
   C++ Mapper
   ═══════════════════════════════════════════════════════════════ */

void CppMapper::register_builtins(InteropTypeRegistry& reg) const {
    reg.add_mapping("void", "void");
    reg.add_mapping("bool", "bool");
    reg.add_mapping("char", "i8");
    reg.add_mapping("signed char", "i8");
    reg.add_mapping("unsigned char", "u8");
    reg.add_mapping("short", "i16");
    reg.add_mapping("unsigned short", "u16");
    reg.add_mapping("int", "i32");
    reg.add_mapping("unsigned int", "u32");
#ifdef _WIN32
    reg.add_mapping("long", "i32");
    reg.add_mapping("unsigned long", "u32");
#else
    reg.add_mapping("long", "i64");
    reg.add_mapping("unsigned long", "u64");
#endif
    reg.add_mapping("long long", "i64");
    reg.add_mapping("unsigned long long", "u64");
    reg.add_mapping("float", "f32");
    reg.add_mapping("double", "f64");
    reg.add_mapping("long double", "f64");
    reg.add_mapping("std::string", "string");
    reg.add_mapping("std::wstring", "string");
    reg.add_mapping("std::string_view", "string");
    reg.add_mapping("std::wstring_view", "string");
    reg.add_mapping("std::vector", "list");
    reg.add_mapping("std::deque", "list");
    reg.add_mapping("std::list", "list");
    reg.add_mapping("std::forward_list", "list");
    reg.add_mapping("std::map", "map");
    reg.add_mapping("std::unordered_map", "map");
    reg.add_mapping("std::set", "set");
    reg.add_mapping("std::unordered_set", "set");
    reg.add_mapping("std::optional", "optional");
    reg.add_mapping("std::variant", "variant");
    reg.add_mapping("std::any", "any");
    reg.add_mapping("std::shared_ptr", "pointer");
    reg.add_mapping("std::unique_ptr", "pointer");
    reg.add_mapping("std::weak_ptr", "pointer");
    reg.add_mapping("std::function", "function");
    reg.add_mapping("std::pair", "tuple");
    reg.add_mapping("std::tuple", "tuple");
    reg.add_mapping("std::array", "array");
    reg.add_mapping("std::span", "slice");
    reg.add_mapping("std::byte", "u8");
    reg.add_mapping("std::filesystem::path", "string");
    reg.add_mapping("std::thread", "pointer");
    reg.add_mapping("std::mutex", "pointer");
    reg.add_mapping("std::condition_variable", "pointer");
    reg.add_mapping("std::future", "future");
    reg.add_mapping("std::shared_future", "future");
    reg.add_mapping("std::promise", "pointer");
    reg.add_mapping("std::chrono::duration", "i64");
    reg.add_mapping("std::chrono::time_point", "i64");
    reg.add_mapping("std::stack", "list");
    reg.add_mapping("std::queue", "list");
    reg.add_mapping("std::priority_queue", "list");
}

MappedType CppMapper::map_to_ir(const std::string& native_type, const InteropTypeRegistry& reg) const {
    MappedType result;
    result.native_type = native_type;
    std::string t = native_type;

    while (!t.empty() && (t.back() == ' ' || t.back() == '&')) t.pop_back();

    if (t.find("std::vector<") == 0 && t.back() == '>') {
        std::string elem = t.substr(12);
        elem.pop_back();
        result.ir_type = InteropType::make_list(elem);
        return result;
    }

    if (t.find("std::map<") == 0 && t.back() == '>') {
        std::string inner = t.substr(9);
        inner.pop_back();
        auto comma = inner.find(", ");
        if (comma == std::string::npos) comma = inner.find(',');
        if (comma != std::string::npos) {
            std::string key = inner.substr(0, comma);
            std::string val = inner.substr(comma + 1);
            while (!key.empty() && key.front() == ' ') key.erase(0, 1);
            while (!val.empty() && val.front() == ' ') val.erase(0, 1);
            result.ir_type = InteropType::make_map(key, val);
        }
        return result;
    }

    if (t.find("std::optional<") == 0 && t.back() == '>') {
        std::string inner = t.substr(14);
        inner.pop_back();
        result.ir_type = InteropType::make_optional(inner);
        return result;
    }

    if (t.find("std::array<") == 0 && t.back() == '>') {
        std::string inner = t.substr(11);
        inner.pop_back();
        auto comma = inner.find(", ");
        if (comma == std::string::npos) comma = inner.find(',');
        if (comma != std::string::npos) {
            std::string elem = inner.substr(0, comma);
            std::string size_str = inner.substr(comma + 1);
            while (!elem.empty() && elem.front() == ' ') elem.erase(0, 1);
            while (!size_str.empty() && size_str.front() == ' ') size_str.erase(0, 1);
            int64_t arr_size = 0;
            char* end_ptr = nullptr;
            arr_size = strtoll(size_str.c_str(), &end_ptr, 10);
            result.ir_type = InteropType::make_array(elem, arr_size);
        }
        return result;
    }

    if (t.find("std::pair<") == 0 && t.back() == '>') {
        std::string inner = t.substr(10);
        inner.pop_back();
        auto comma = inner.find(", ");
        if (comma == std::string::npos) comma = inner.find(',');
        if (comma != std::string::npos) {
            std::string first = inner.substr(0, comma);
            std::string second = inner.substr(comma + 1);
            while (!first.empty() && first.front() == ' ') first.erase(0, 1);
            while (!second.empty() && second.front() == ' ') second.erase(0, 1);
            result.ir_type = InteropType::make_struct("pair", {
                {"first", first}, {"second", second}
            });
            result.ir_type.kind = InteropTypeKind::Tuple;
        }
        return result;
    }

    if (t.find("std::tuple<") == 0 && t.back() == '>') {
        std::string inner = t.substr(11);
        inner.pop_back();
        result.ir_type.kind = InteropTypeKind::Tuple;
        result.ir_type.name = t;
        return result;
    }

    if (t.find("std::deque<") == 0 && t.back() == '>') {
        std::string elem = t.substr(11);
        elem.pop_back();
        result.ir_type = InteropType::make_list(elem);
        return result;
    }

    if (t.find("std::list<") == 0 && t.back() == '>') {
        std::string elem = t.substr(10);
        elem.pop_back();
        result.ir_type = InteropType::make_list(elem);
        return result;
    }

    if (t.find("std::span<") == 0 && t.back() == '>') {
        std::string elem = t.substr(10);
        elem.pop_back();
        auto comma = elem.find(", ");
        if (comma != std::string::npos) elem = elem.substr(0, comma);
        result.ir_type.kind = InteropTypeKind::Slice;
        result.ir_type.element_type = elem;
        return result;
    }

    if (t.find("std::string_view") == 0) {
        result.ir_type = InteropType::make_primitive(InteropTypeKind::String, "string");
        return result;
    }

    if (t.find("std::filesystem::path") == 0) {
        result.ir_type = InteropType::make_primitive(InteropTypeKind::String, "string");
        return result;
    }

    if (t.find("std::function<") == 0 && t.back() == '>') {
        result.ir_type.kind = InteropTypeKind::Function;
        result.ir_type.name = t;
        return result;
    }

    auto* ir = reg.get_type(t);
    if (ir) {
        result.ir_type = *ir;
        if (t == "int" || t == "float" || t == "double" || t == "bool" ||
            t.find("int") == 0 || t.find("char") == 0) {
            result.is_trivially_castable = true;
        }
        return result;
    }

    if (t.find('*') != std::string::npos) {
        result.ir_type = InteropType::make_primitive(InteropTypeKind::Pointer, "pointer");
        return result;
    }

    result.ir_type.kind = InteropTypeKind::Unknown;
    result.ir_type.name = t;
    return result;
}

std::string CppMapper::map_from_ir(const InteropType& ir, const InteropTypeRegistry& reg) const {
    switch (ir.kind) {
        case InteropTypeKind::Void:      return "void";
        case InteropTypeKind::Bool:      return "bool";
        case InteropTypeKind::Int8:      return "int8_t";
        case InteropTypeKind::Int16:     return "int16_t";
        case InteropTypeKind::Int32:     return "int32_t";
        case InteropTypeKind::Int64:     return "int64_t";
        case InteropTypeKind::UInt8:     return "uint8_t";
        case InteropTypeKind::UInt16:    return "uint16_t";
        case InteropTypeKind::UInt32:    return "uint32_t";
        case InteropTypeKind::UInt64:    return "uint64_t";
        case InteropTypeKind::Float32:   return "float";
        case InteropTypeKind::Float64:   return "double";
        case InteropTypeKind::CString:   return "const char*";
        case InteropTypeKind::String:    return "std::string";
        case InteropTypeKind::List:      return "std::vector<" + ir.element_type + ">";
        case InteropTypeKind::Map:       return "std::map<" + ir.key_type + ", " + ir.value_type + ">";
        case InteropTypeKind::Set:       return "std::set<" + ir.element_type + ">";
        case InteropTypeKind::Optional:  return "std::optional<" + ir.inner_type + ">";
        case InteropTypeKind::Pointer:
        case InteropTypeKind::Reference: return "void*";
        case InteropTypeKind::Struct:    return ir.name;
        case InteropTypeKind::Alias:     return ir.alias_for;
        default:                         return "auto";
    }
}

/* ═══════════════════════════════════════════════════════════════
   Python Mapper
   ═══════════════════════════════════════════════════════════════ */

void PythonMapper::register_builtins(InteropTypeRegistry& reg) const {
    reg.add_mapping("None", "void");
    reg.add_mapping("NoneType", "void");
    reg.add_mapping("bool", "bool");
    reg.add_mapping("int", "i64");
    reg.add_mapping("float", "f64");
    reg.add_mapping("str", "string");
    reg.add_mapping("bytes", "list");
    reg.add_mapping("bytearray", "list");
    reg.add_mapping("list", "list");
    reg.add_mapping("tuple", "tuple");
    reg.add_mapping("dict", "map");
    reg.add_mapping("set", "set");
    reg.add_mapping("frozenset", "set");
    reg.add_mapping("type", "any");
    reg.add_mapping("object", "any");
    reg.add_mapping("Any", "any");
    reg.add_mapping("Optional", "optional");
    reg.add_mapping("Union", "variant");
}

MappedType PythonMapper::map_to_ir(const std::string& native_type, const InteropTypeRegistry& reg) const {
    MappedType result;
    result.native_type = native_type;
    std::string t = native_type;

    while (!t.empty() && t.front() == ' ') t.erase(0, 1);
    while (!t.empty() && t.back() == ' ') t.pop_back();

    if (t.find("list[") == 0 && t.back() == ']') {
        std::string elem = t.substr(5, t.size() - 6);
        result.ir_type = InteropType::make_list(elem);
        return result;
    }

    if (t.find("dict[") == 0 && t.back() == ']') {
        std::string inner = t.substr(5, t.size() - 6);
        auto comma = inner.find(", ");
        if (comma == std::string::npos) comma = inner.find(',');
        if (comma != std::string::npos) {
            std::string key = inner.substr(0, comma);
            std::string val = inner.substr(comma + 1);
            while (!key.empty() && key.front() == ' ') key.erase(0, 1);
            while (!val.empty() && val.front() == ' ') val.erase(0, 1);
            result.ir_type = InteropType::make_map(key, val);
        }
        return result;
    }

    if (t.find("Optional[") == 0 && t.back() == ']') {
        std::string inner = t.substr(9, t.size() - 10);
        result.ir_type = InteropType::make_optional(inner);
        return result;
    }

    if (t.find("Union[") == 0 && t.back() == ']') {
        result.ir_type.kind = InteropTypeKind::Variant;
        result.ir_type.name = t;
        return result;
    }

    if (t.find("tuple[") == 0 && t.back() == ']') {
        result.ir_type.kind = InteropTypeKind::Tuple;
        result.ir_type.name = t;
        return result;
    }

    auto* ir = reg.get_type(t);
    if (ir) {
        result.ir_type = *ir;
        return result;
    }

    result.ir_type.kind = InteropTypeKind::Unknown;
    result.ir_type.name = t;
    return result;
}

std::string PythonMapper::map_from_ir(const InteropType& ir, const InteropTypeRegistry& reg) const {
    switch (ir.kind) {
        case InteropTypeKind::Void:      return "None";
        case InteropTypeKind::Bool:      return "bool";
        case InteropTypeKind::Int8:      return "int";
        case InteropTypeKind::Int16:     return "int";
        case InteropTypeKind::Int32:     return "int";
        case InteropTypeKind::Int64:     return "int";
        case InteropTypeKind::UInt8:     return "int";
        case InteropTypeKind::UInt16:    return "int";
        case InteropTypeKind::UInt32:    return "int";
        case InteropTypeKind::UInt64:    return "int";
        case InteropTypeKind::Float32:   return "float";
        case InteropTypeKind::Float64:   return "float";
        case InteropTypeKind::Char:      return "str";
        case InteropTypeKind::String:    return "str";
        case InteropTypeKind::CString:   return "str";
        case InteropTypeKind::Pointer:   return "int";
        case InteropTypeKind::List:      return "list[" + ir.element_type + "]";
        case InteropTypeKind::Map:       return "dict[" + ir.key_type + ", " + ir.value_type + "]";
        case InteropTypeKind::Set:       return "set[" + ir.element_type + "]";
        case InteropTypeKind::Tuple:     return "tuple";
        case InteropTypeKind::Optional:  return "Optional[" + ir.inner_type + "]";
        case InteropTypeKind::Any:       return "Any";
        case InteropTypeKind::Alias:     return ir.alias_for;
        default:                         return "Any";
    }
}

/* ═══════════════════════════════════════════════════════════════
   JavaScript Mapper
   ═══════════════════════════════════════════════════════════════ */

void JavaScriptMapper::register_builtins(InteropTypeRegistry& reg) const {
    reg.add_mapping("undefined", "void");
    reg.add_mapping("null", "void");
    reg.add_mapping("boolean", "bool");
    reg.add_mapping("number", "f64");
    reg.add_mapping("bigint", "i64");
    reg.add_mapping("string", "string");
    reg.add_mapping("symbol", "any");
    reg.add_mapping("object", "any");
    reg.add_mapping("Array", "list");
    reg.add_mapping("Map", "map");
    reg.add_mapping("Set", "set");
    reg.add_mapping("Promise", "future");
    reg.add_mapping("Function", "function");
    reg.add_mapping("any", "any");
    reg.add_mapping("unknown", "any");
}

MappedType JavaScriptMapper::map_to_ir(const std::string& native_type, const InteropTypeRegistry& reg) const {
    MappedType result;
    result.native_type = native_type;
    std::string t = native_type;

    while (!t.empty() && t.front() == ' ') t.erase(0, 1);
    while (!t.empty() && t.back() == ' ') t.pop_back();

    if (t.find("Array<") == 0 && t.back() == '>') {
        std::string elem = t.substr(6, t.size() - 7);
        result.ir_type = InteropType::make_list(elem);
        return result;
    }

    if (t.find("Promise<") == 0 && t.back() == '>') {
        std::string inner = t.substr(8, t.size() - 9);
        result.ir_type.kind = InteropTypeKind::Future;
        result.ir_type.inner_type = inner;
        return result;
    }

    if (t.find("Map<") == 0 && t.back() == '>') {
        std::string inner = t.substr(4, t.size() - 5);
        auto comma = inner.find(", ");
        if (comma == std::string::npos) comma = inner.find(',');
        if (comma != std::string::npos) {
            std::string key = inner.substr(0, comma);
            std::string val = inner.substr(comma + 1);
            while (!key.empty() && key.front() == ' ') key.erase(0, 1);
            while (!val.empty() && val.front() == ' ') val.erase(0, 1);
            result.ir_type = InteropType::make_map(key, val);
        }
        return result;
    }

    auto* ir = reg.get_type(t);
    if (ir) {
        result.ir_type = *ir;
        return result;
    }

    result.ir_type.kind = InteropTypeKind::Unknown;
    result.ir_type.name = t;
    return result;
}

std::string JavaScriptMapper::map_from_ir(const InteropType& ir, const InteropTypeRegistry& reg) const {
    switch (ir.kind) {
        case InteropTypeKind::Void:      return "undefined";
        case InteropTypeKind::Bool:      return "boolean";
        case InteropTypeKind::Int8:
        case InteropTypeKind::Int16:
        case InteropTypeKind::Int32:
        case InteropTypeKind::Int64:
        case InteropTypeKind::UInt8:
        case InteropTypeKind::UInt16:
        case InteropTypeKind::UInt32:
        case InteropTypeKind::UInt64:    return "number";
        case InteropTypeKind::Float16:
        case InteropTypeKind::Float32:
        case InteropTypeKind::Float64:   return "number";
        case InteropTypeKind::Char:
        case InteropTypeKind::String:
        case InteropTypeKind::CString:   return "string";
        case InteropTypeKind::Pointer:   return "number";
        case InteropTypeKind::List:      return "Array<" + ir.element_type + ">";
        case InteropTypeKind::Map:       return "Map<" + ir.key_type + ", " + ir.value_type + ">";
        case InteropTypeKind::Set:       return "Set<" + ir.element_type + ">";
        case InteropTypeKind::Future:    return "Promise<" + ir.inner_type + ">";
        case InteropTypeKind::Any:       return "any";
        case InteropTypeKind::Alias:     return ir.alias_for;
        default:                         return "any";
    }
}

/* ═══════════════════════════════════════════════════════════════
   Rust Mapper
   ═══════════════════════════════════════════════════════════════ */

void RustMapper::register_builtins(InteropTypeRegistry& reg) const {
    reg.add_mapping("()", "void");
    reg.add_mapping("bool", "bool");
    reg.add_mapping("i8", "i8");
    reg.add_mapping("i16", "i16");
    reg.add_mapping("i32", "i32");
    reg.add_mapping("i64", "i64");
    reg.add_mapping("i128", "i64");
    reg.add_mapping("u8", "u8");
    reg.add_mapping("u16", "u16");
    reg.add_mapping("u32", "u32");
    reg.add_mapping("u64", "u64");
    reg.add_mapping("u128", "u64");
    reg.add_mapping("f32", "f32");
    reg.add_mapping("f64", "f64");
    reg.add_mapping("char", "u32");
    reg.add_mapping("String", "string");
    reg.add_mapping("&str", "string");
    reg.add_mapping("*const u8", "cstring");
    reg.add_mapping("*mut u8", "pointer");
    reg.add_mapping("*const ()", "pointer");
    reg.add_mapping("*mut ()", "pointer");
    reg.add_mapping("Vec", "list");
    reg.add_mapping("HashMap", "map");
    reg.add_mapping("BTreeMap", "map");
    reg.add_mapping("HashSet", "set");
    reg.add_mapping("Option", "optional");
    reg.add_mapping("Result", "result");
    reg.add_mapping("Box", "pointer");
    reg.add_mapping("Rc", "pointer");
    reg.add_mapping("Arc", "pointer");
    reg.add_mapping("Cell", "pointer");
    reg.add_mapping("RefCell", "pointer");
    reg.add_mapping("Pin", "pointer");
    reg.add_mapping("dyn", "any");
    reg.add_mapping("impl", "any");
}

MappedType RustMapper::map_to_ir(const std::string& native_type, const InteropTypeRegistry& reg) const {
    MappedType result;
    result.native_type = native_type;
    std::string t = native_type;

    while (!t.empty() && t.front() == ' ') t.erase(0, 1);
    while (!t.empty() && t.back() == ' ') t.pop_back();

    if (t.find("Vec<") == 0 && t.back() == '>') {
        std::string elem = t.substr(4, t.size() - 5);
        result.ir_type = InteropType::make_list(elem);
        return result;
    }

    if (t.find("HashMap<") == 0 && t.back() == '>') {
        std::string inner = t.substr(8, t.size() - 9);
        auto comma = inner.find(", ");
        if (comma == std::string::npos) comma = inner.find(',');
        if (comma != std::string::npos) {
            std::string key = inner.substr(0, comma);
            std::string val = inner.substr(comma + 1);
            while (!key.empty() && key.front() == ' ') key.erase(0, 1);
            while (!val.empty() && val.front() == ' ') val.erase(0, 1);
            result.ir_type = InteropType::make_map(key, val);
        }
        return result;
    }

    if (t.find("Option<") == 0 && t.back() == '>') {
        std::string inner = t.substr(7, t.size() - 8);
        result.ir_type = InteropType::make_optional(inner);
        return result;
    }

    if (t.find("Result<") == 0 && t.back() == '>') {
        std::string inner = t.substr(7, t.size() - 8);
        auto comma = inner.find(", ");
        if (comma == std::string::npos) comma = inner.find(',');
        if (comma != std::string::npos) {
            std::string ok = inner.substr(0, comma);
            std::string err = inner.substr(comma + 1);
            while (!ok.empty() && ok.front() == ' ') ok.erase(0, 1);
            while (!err.empty() && err.front() == ' ') err.erase(0, 1);
            result.ir_type = InteropType::make_result(ok, err);
        }
        return result;
    }

    if (t.find("Box<") == 0 && t.back() == '>') {
        std::string inner = t.substr(4, t.size() - 5);
        result.ir_type = InteropType::make_pointer(inner);
        return result;
    }

    if (t.find("Arc<") == 0 && t.back() == '>') {
        std::string inner = t.substr(4, t.size() - 5);
        result.ir_type = InteropType::make_pointer(inner);
        result.ir_type.attrs["rc"] = "atomic";
        return result;
    }

    if (t.find("Rc<") == 0 && t.back() == '>') {
        std::string inner = t.substr(3, t.size() - 4);
        result.ir_type = InteropType::make_pointer(inner);
        result.ir_type.attrs["rc"] = "non-atomic";
        return result;
    }

    if (t.find("&") == 0 || t.find("*const") == 0 || t.find("*mut") == 0) {
        result.ir_type = InteropType::make_primitive(InteropTypeKind::Pointer, "pointer");
        return result;
    }

    if (t.find("[") == 0 && t.find("]") != std::string::npos) {
        result.ir_type.kind = InteropTypeKind::Slice;
        auto bracket = t.find(']');
        std::string elem = t.substr(1, bracket - 1);
        result.ir_type.element_type = elem;
        return result;
    }

    if (t.find("HashSet<") == 0 && t.back() == '>') {
        std::string elem = t.substr(8, t.size() - 9);
        result.ir_type = InteropType::make_list(elem);
        result.ir_type.kind = InteropTypeKind::Set;
        return result;
    }

    auto* ir = reg.get_type(t);
    if (ir) {
        result.ir_type = *ir;
        return result;
    }

    result.ir_type.kind = InteropTypeKind::Unknown;
    result.ir_type.name = t;
    return result;
}

std::string RustMapper::map_from_ir(const InteropType& ir, const InteropTypeRegistry& reg) const {
    switch (ir.kind) {
        case InteropTypeKind::Void:      return "()";
        case InteropTypeKind::Bool:      return "bool";
        case InteropTypeKind::Int8:      return "i8";
        case InteropTypeKind::Int16:     return "i16";
        case InteropTypeKind::Int32:     return "i32";
        case InteropTypeKind::Int64:     return "i64";
        case InteropTypeKind::UInt8:     return "u8";
        case InteropTypeKind::UInt16:    return "u16";
        case InteropTypeKind::UInt32:    return "u32";
        case InteropTypeKind::UInt64:    return "u64";
        case InteropTypeKind::Float32:   return "f32";
        case InteropTypeKind::Float64:   return "f64";
        case InteropTypeKind::String:    return "String";
        case InteropTypeKind::CString:   return "*const u8";
        case InteropTypeKind::Pointer:
        case InteropTypeKind::Reference: return "*mut ()";
        case InteropTypeKind::List:      return "Vec<" + ir.element_type + ">";
        case InteropTypeKind::Map:       return "HashMap<" + ir.key_type + ", " + ir.value_type + ">";
        case InteropTypeKind::Set:       return "HashSet<" + ir.element_type + ">";
        case InteropTypeKind::Optional:  return "Option<" + ir.inner_type + ">";
        case InteropTypeKind::Result:    return "Result<" + ir.inner_type + ", " + ir.error_type + ">";
        case InteropTypeKind::Array:     return "[" + ir.element_type + "; " + std::to_string(ir.array_size) + "]";
        case InteropTypeKind::Alias:     return ir.alias_for;
        default:                         return ir.name.empty() ? "()" : ir.name;
    }
}

/* ═══════════════════════════════════════════════════════════════
   Java Mapper
   ═══════════════════════════════════════════════════════════════ */

void JavaMapper::register_builtins(InteropTypeRegistry& reg) const {
    reg.add_mapping("void", "void");
    reg.add_mapping("boolean", "bool");
    reg.add_mapping("byte", "i8");
    reg.add_mapping("short", "i16");
    reg.add_mapping("int", "i32");
    reg.add_mapping("long", "i64");
    reg.add_mapping("float", "f32");
    reg.add_mapping("double", "f64");
    reg.add_mapping("char", "u16");
    reg.add_mapping("java.lang.String", "string");
    reg.add_mapping("String", "string");
    reg.add_mapping("java.lang.Object", "any");
    reg.add_mapping("Object", "any");
    reg.add_mapping("java.util.List", "list");
    reg.add_mapping("java.util.Map", "map");
    reg.add_mapping("java.util.Set", "set");
    reg.add_mapping("java.util.Optional", "optional");
    reg.add_mapping("int[]", "array");
    reg.add_mapping("byte[]", "array");
    reg.add_mapping("void*", "pointer");
}

MappedType JavaMapper::map_to_ir(const std::string& native_type, const InteropTypeRegistry& reg) const {
    MappedType result;
    result.native_type = native_type;
    std::string t = native_type;

    while (!t.empty() && t.front() == ' ') t.erase(0, 1);
    while (!t.empty() && t.back() == ' ') t.pop_back();

    if (t.find("java.util.List<") == 0 && t.back() == '>') {
        std::string elem = t.substr(15, t.size() - 16);
        result.ir_type = InteropType::make_list(elem);
        return result;
    }
    if (t.find("List<") == 0 && t.back() == '>') {
        std::string elem = t.substr(5, t.size() - 6);
        result.ir_type = InteropType::make_list(elem);
        return result;
    }

    if (t.find("java.util.Map<") == 0 && t.back() == '>') {
        std::string inner = t.substr(15, t.size() - 16);
        auto comma = inner.find(", ");
        if (comma == std::string::npos) comma = inner.find(',');
        if (comma != std::string::npos) {
            std::string key = inner.substr(0, comma);
            std::string val = inner.substr(comma + 1);
            while (!key.empty() && key.front() == ' ') key.erase(0, 1);
            while (!val.empty() && val.front() == ' ') val.erase(0, 1);
            result.ir_type = InteropType::make_map(key, val);
        }
        return result;
    }
    if (t.find("Map<") == 0 && t.back() == '>') {
        std::string inner = t.substr(4, t.size() - 5);
        auto comma = inner.find(", ");
        if (comma == std::string::npos) comma = inner.find(',');
        if (comma != std::string::npos) {
            std::string key = inner.substr(0, comma);
            std::string val = inner.substr(comma + 1);
            while (!key.empty() && key.front() == ' ') key.erase(0, 1);
            while (!val.empty() && val.front() == ' ') val.erase(0, 1);
            result.ir_type = InteropType::make_map(key, val);
        }
        return result;
    }

    if (t.find("java.util.Optional<") == 0 && t.back() == '>') {
        std::string inner = t.substr(19, t.size() - 20);
        result.ir_type = InteropType::make_optional(inner);
        return result;
    }

    if (t.find("java.util.Set<") == 0 && t.back() == '>') {
        std::string elem = t.substr(14, t.size() - 15);
        result.ir_type = InteropType::make_list(elem);
        result.ir_type.kind = InteropTypeKind::Set;
        return result;
    }

    auto* ir = reg.get_type(t);
    if (ir) {
        result.ir_type = *ir;
        if (t == "int" || t == "long" || t == "float" || t == "double" || t == "boolean" || t == "byte" || t == "short" || t == "char") {
            result.is_trivially_castable = true;
            result.cost = InteropCost::ZeroCost;
        }
        return result;
    }

    if (t.back() == ']' && t.find('[') != std::string::npos) {
        result.ir_type.kind = InteropTypeKind::Array;
        result.ir_type.name = t;
        result.cost = InteropCost::AllocCost;
        return result;
    }

    result.ir_type.kind = InteropTypeKind::Unknown;
    result.ir_type.name = t;
    return result;
}

std::string JavaMapper::map_from_ir(const InteropType& ir, const InteropTypeRegistry& reg) const {
    switch (ir.kind) {
        case InteropTypeKind::Void:      return "void";
        case InteropTypeKind::Bool:      return "boolean";
        case InteropTypeKind::Int8:      return "byte";
        case InteropTypeKind::Int16:     return "short";
        case InteropTypeKind::Int32:     return "int";
        case InteropTypeKind::Int64:     return "long";
        case InteropTypeKind::UInt8:     return "byte";
        case InteropTypeKind::UInt16:    return "char";
        case InteropTypeKind::UInt32:    return "int";
        case InteropTypeKind::UInt64:    return "long";
        case InteropTypeKind::Float32:   return "float";
        case InteropTypeKind::Float64:   return "double";
        case InteropTypeKind::Char:      return "char";
        case InteropTypeKind::String:    return "String";
        case InteropTypeKind::CString:   return "String";
        case InteropTypeKind::Pointer:   return "long";
        case InteropTypeKind::List:      return "java.util.List<" + ir.element_type + ">";
        case InteropTypeKind::Map:       return "java.util.Map<" + ir.key_type + ", " + ir.value_type + ">";
        case InteropTypeKind::Set:       return "java.util.Set<" + ir.element_type + ">";
        case InteropTypeKind::Optional:  return "java.util.Optional<" + ir.inner_type + ">";
        case InteropTypeKind::Array:     return ir.element_type + "[]";
        case InteropTypeKind::Any:       return "java.lang.Object";
        case InteropTypeKind::Alias:     return ir.alias_for;
        default:                         return "java.lang.Object";
    }
}

/* ═══════════════════════════════════════════════════════════════
   C# Mapper
   ═══════════════════════════════════════════════════════════════ */

void CSharpMapper::register_builtins(InteropTypeRegistry& reg) const {
    reg.add_mapping("void", "void");
    reg.add_mapping("bool", "bool");
    reg.add_mapping("byte", "u8");
    reg.add_mapping("sbyte", "i8");
    reg.add_mapping("short", "i16");
    reg.add_mapping("ushort", "u16");
    reg.add_mapping("int", "i32");
    reg.add_mapping("uint", "u32");
    reg.add_mapping("long", "i64");
    reg.add_mapping("ulong", "u64");
    reg.add_mapping("float", "f32");
    reg.add_mapping("double", "f64");
    reg.add_mapping("decimal", "f64");
    reg.add_mapping("char", "u16");
    reg.add_mapping("string", "string");
    reg.add_mapping("object", "any");
    reg.add_mapping("System.String", "string");
    reg.add_mapping("System.Object", "any");
    reg.add_mapping("System.Collections.Generic.List", "list");
    reg.add_mapping("System.Collections.Generic.Dictionary", "map");
    reg.add_mapping("System.Collections.Generic.HashSet", "set");
    reg.add_mapping("System.Nullable", "optional");
    reg.add_mapping("System.IntPtr", "pointer");
    reg.add_mapping("System.Threading.Tasks.Task", "future");
}

MappedType CSharpMapper::map_to_ir(const std::string& native_type, const InteropTypeRegistry& reg) const {
    MappedType result;
    result.native_type = native_type;
    std::string t = native_type;

    while (!t.empty() && t.front() == ' ') t.erase(0, 1);
    while (!t.empty() && t.back() == ' ') t.pop_back();

    if (t.find("List<") == 0 && t.back() == '>') {
        std::string elem = t.substr(5, t.size() - 6);
        result.ir_type = InteropType::make_list(elem);
        return result;
    }

    if (t.find("Dictionary<") == 0 && t.back() == '>') {
        std::string inner = t.substr(11, t.size() - 12);
        auto comma = inner.find(", ");
        if (comma == std::string::npos) comma = inner.find(',');
        if (comma != std::string::npos) {
            std::string key = inner.substr(0, comma);
            std::string val = inner.substr(comma + 1);
            while (!key.empty() && key.front() == ' ') key.erase(0, 1);
            while (!val.empty() && val.front() == ' ') val.erase(0, 1);
            result.ir_type = InteropType::make_map(key, val);
        }
        return result;
    }

    if (t.find("Nullable<") == 0 && t.back() == '>') {
        std::string inner = t.substr(9, t.size() - 10);
        result.ir_type = InteropType::make_optional(inner);
        return result;
    }

    if (t.find("HashSet<") == 0 && t.back() == '>') {
        std::string elem = t.substr(8, t.size() - 9);
        result.ir_type = InteropType::make_list(elem);
        result.ir_type.kind = InteropTypeKind::Set;
        return result;
    }

    if (t.find("Task<") == 0 && t.back() == '>') {
        std::string inner = t.substr(5, t.size() - 6);
        result.ir_type.kind = InteropTypeKind::Future;
        result.ir_type.inner_type = inner;
        return result;
    }

    if (t.back() == '?' && t.size() > 1) {
        std::string inner = t.substr(0, t.size() - 1);
        result.ir_type = InteropType::make_optional(inner);
        return result;
    }

    if (t.find("[]") != std::string::npos) {
        std::string elem = t.substr(0, t.find("[]"));
        result.ir_type = InteropType::make_array(elem, 0);
        result.cost = InteropCost::AllocCost;
        return result;
    }

    auto* ir = reg.get_type(t);
    if (ir) {
        result.ir_type = *ir;
        if (t == "int" || t == "long" || t == "float" || t == "double" || t == "bool" || t == "byte" || t == "short" || t == "char") {
            result.is_trivially_castable = true;
            result.cost = InteropCost::ZeroCost;
        }
        return result;
    }

    if (t.find('*') != std::string::npos) {
        result.ir_type = InteropType::make_primitive(InteropTypeKind::Pointer, "pointer");
        return result;
    }

    result.ir_type.kind = InteropTypeKind::Unknown;
    result.ir_type.name = t;
    return result;
}

std::string CSharpMapper::map_from_ir(const InteropType& ir, const InteropTypeRegistry& reg) const {
    switch (ir.kind) {
        case InteropTypeKind::Void:      return "void";
        case InteropTypeKind::Bool:      return "bool";
        case InteropTypeKind::Int8:      return "sbyte";
        case InteropTypeKind::Int16:     return "short";
        case InteropTypeKind::Int32:     return "int";
        case InteropTypeKind::Int64:     return "long";
        case InteropTypeKind::UInt8:     return "byte";
        case InteropTypeKind::UInt16:    return "ushort";
        case InteropTypeKind::UInt32:    return "uint";
        case InteropTypeKind::UInt64:    return "ulong";
        case InteropTypeKind::Float32:   return "float";
        case InteropTypeKind::Float64:   return "double";
        case InteropTypeKind::Char:      return "char";
        case InteropTypeKind::String:    return "string";
        case InteropTypeKind::CString:   return "string";
        case InteropTypeKind::Pointer:   return "System.IntPtr";
        case InteropTypeKind::List:      return "System.Collections.Generic.List<" + ir.element_type + ">";
        case InteropTypeKind::Map:       return "System.Collections.Generic.Dictionary<" + ir.key_type + ", " + ir.value_type + ">";
        case InteropTypeKind::Set:       return "System.Collections.Generic.HashSet<" + ir.element_type + ">";
        case InteropTypeKind::Optional:  return ir.inner_type + "?";
        case InteropTypeKind::Future:    return "System.Threading.Tasks.Task<" + ir.inner_type + ">";
        case InteropTypeKind::Array:     return ir.element_type + "[]";
        case InteropTypeKind::Any:       return "object";
        case InteropTypeKind::Alias:     return ir.alias_for;
        default:                         return "object";
    }
}

/* ═══════════════════════════════════════════════════════════════
   Go Mapper
   ═══════════════════════════════════════════════════════════════ */

void GoMapper::register_builtins(InteropTypeRegistry& reg) const {
    reg.add_mapping("void", "void");
    reg.add_mapping("bool", "bool");
    reg.add_mapping("int8", "i8");
    reg.add_mapping("int16", "i16");
    reg.add_mapping("int32", "i32");
    reg.add_mapping("int64", "i64");
    reg.add_mapping("int", "i64");
    reg.add_mapping("uint8", "u8");
    reg.add_mapping("uint16", "u16");
    reg.add_mapping("uint32", "u32");
    reg.add_mapping("uint64", "u64");
    reg.add_mapping("uint", "u64");
    reg.add_mapping("uintptr", "u64");
    reg.add_mapping("float32", "f32");
    reg.add_mapping("float64", "f64");
    reg.add_mapping("byte", "u8");
    reg.add_mapping("rune", "i32");
    reg.add_mapping("string", "string");
    reg.add_mapping("error", "any");
    reg.add_mapping("interface{}", "any");
    reg.add_mapping("any", "any");
    reg.add_mapping("unsafe.Pointer", "pointer");
    reg.add_mapping("[]byte", "list");
    reg.add_mapping("map", "map");
    reg.add_mapping("chan", "future");
}

MappedType GoMapper::map_to_ir(const std::string& native_type, const InteropTypeRegistry& reg) const {
    MappedType result;
    result.native_type = native_type;
    std::string t = native_type;

    while (!t.empty() && t.front() == ' ') t.erase(0, 1);
    while (!t.empty() && t.back() == ' ') t.pop_back();

    if (t.find("[]") == 0 && t.size() > 2) {
        std::string elem = t.substr(2);
        if (elem.find("[]") == 0) {
            result.ir_type = InteropType::make_array(elem.substr(2), 0);
            result.ir_type.kind = InteropTypeKind::Array;
        } else {
            result.ir_type = InteropType::make_list(elem);
        }
        return result;
    }

    if (t.find("map[") == 0) {
        size_t bracket = t.find(']', 4);
        if (bracket != std::string::npos) {
            std::string key = t.substr(4, bracket - 4);
            std::string val = t.substr(bracket + 1);
            while (!val.empty() && val.front() == ' ') val.erase(0, 1);
            result.ir_type = InteropType::make_map(key, val);
        }
        return result;
    }

    if (t.find("chan ") == 0) {
        std::string inner = t.substr(5);
        result.ir_type.kind = InteropTypeKind::Future;
        result.ir_type.inner_type = inner;
        return result;
    }

    if (t.find("func(") == 0) {
        result.ir_type.kind = InteropTypeKind::Function;
        result.ir_type.name = t;
        return result;
    }

    if (t.find("struct{") == 0) {
        result.ir_type.kind = InteropTypeKind::Struct;
        result.ir_type.name = t;
        return result;
    }

    if (t.find("interface{") == 0) {
        result.ir_type.kind = InteropTypeKind::Interface;
        result.ir_type.name = t;
        return result;
    }

    if (t.find('*') == 0) {
        std::string inner = t.substr(1);
        result.ir_type = InteropType::make_pointer(inner);
        return result;
    }

    auto* ir = reg.get_type(t);
    if (ir) {
        result.ir_type = *ir;
        return result;
    }

    result.ir_type.kind = InteropTypeKind::Unknown;
    result.ir_type.name = t;
    return result;
}

std::string GoMapper::map_from_ir(const InteropType& ir, const InteropTypeRegistry& reg) const {
    switch (ir.kind) {
        case InteropTypeKind::Void:      return "void";
        case InteropTypeKind::Bool:      return "bool";
        case InteropTypeKind::Int8:      return "int8";
        case InteropTypeKind::Int16:     return "int16";
        case InteropTypeKind::Int32:     return "int32";
        case InteropTypeKind::Int64:     return "int64";
        case InteropTypeKind::UInt8:     return "uint8";
        case InteropTypeKind::UInt16:    return "uint16";
        case InteropTypeKind::UInt32:    return "uint32";
        case InteropTypeKind::UInt64:    return "uint64";
        case InteropTypeKind::Float32:   return "float32";
        case InteropTypeKind::Float64:   return "float64";
        case InteropTypeKind::Char:      return "rune";
        case InteropTypeKind::String:    return "string";
        case InteropTypeKind::CString:   return "string";
        case InteropTypeKind::Pointer:   return "unsafe.Pointer";
        case InteropTypeKind::List:      return "[]" + ir.element_type;
        case InteropTypeKind::Map:       return "map[" + ir.key_type + "]" + ir.value_type;
        case InteropTypeKind::Optional:  return "*" + ir.inner_type;
        case InteropTypeKind::Future:    return "chan " + ir.inner_type;
        case InteropTypeKind::Array:     return "[]" + ir.element_type;
        case InteropTypeKind::Any:       return "interface{}";
        case InteropTypeKind::Alias:     return ir.alias_for;
        default:                         return "interface{}";
    }
}

/* ═══════════════════════════════════════════════════════════════
   Zig Mapper
   ═══════════════════════════════════════════════════════════════ */

void ZigMapper::register_builtins(InteropTypeRegistry& reg) const {
    reg.add_mapping("void", "void");
    reg.add_mapping("bool", "bool");
    reg.add_mapping("i8", "i8");
    reg.add_mapping("i16", "i16");
    reg.add_mapping("i32", "i32");
    reg.add_mapping("i64", "i64");
    reg.add_mapping("u8", "u8");
    reg.add_mapping("u16", "u16");
    reg.add_mapping("u32", "u32");
    reg.add_mapping("u64", "u64");
    reg.add_mapping("f16", "f16");
    reg.add_mapping("f32", "f32");
    reg.add_mapping("f64", "f64");
    reg.add_mapping("isize", "i64");
    reg.add_mapping("usize", "u64");
    reg.add_mapping("comptime_int", "i64");
    reg.add_mapping("comptime_float", "f64");
    reg.add_mapping("c_short", "i16");
    reg.add_mapping("c_ushort", "u16");
    reg.add_mapping("c_int", "i32");
    reg.add_mapping("c_uint", "u32");
    reg.add_mapping("c_long", "i64");
    reg.add_mapping("c_ulong", "u64");
    reg.add_mapping("c_longlong", "i64");
    reg.add_mapping("c_ulonglong", "u64");
    reg.add_mapping("c_void", "void");
    reg.add_mapping("[]const u8", "string");
    reg.add_mapping("[]u8", "list");
    reg.add_mapping("?*anyopaque", "pointer");
    reg.add_mapping("*anyopaque", "pointer");
    reg.add_mapping("anyopaque", "any");
    reg.add_mapping("anyerror", "any");
    reg.add_mapping("type", "any");
}

MappedType ZigMapper::map_to_ir(const std::string& native_type, const InteropTypeRegistry& reg) const {
    MappedType result;
    result.native_type = native_type;
    std::string t = native_type;

    while (!t.empty() && t.front() == ' ') t.erase(0, 1);
    while (!t.empty() && t.back() == ' ') t.pop_back();

    if (t.find("[]const u8") == 0) {
        result.ir_type = InteropType::make_primitive(InteropTypeKind::String, "string");
        return result;
    }

    if (t.find("[]") == 0 && t.size() > 2) {
        std::string elem = t.substr(2);
        result.ir_type = InteropType::make_list(elem);
        return result;
    }

    if (t.find("?") == 0) {
        std::string inner = t.substr(1);
        if (inner.find("*anyopaque") == 0 || inner.find("*") == 0) {
            result.ir_type = InteropType::make_primitive(InteropTypeKind::Pointer, "pointer");
            result.ir_type.is_nullable = true;
        } else {
            result.ir_type = InteropType::make_optional(inner);
        }
        return result;
    }

    if (t.find("*const ") == 0) {
        std::string inner = t.substr(7);
        result.ir_type = InteropType::make_pointer(inner);
        result.ir_type.kind = InteropTypeKind::Pointer;
        return result;
    }

    if (t.find("*") == 0) {
        std::string inner = t.substr(1);
        result.ir_type = InteropType::make_pointer(inner);
        return result;
    }

    if (t.find("struct {") == 0) {
        result.ir_type.kind = InteropTypeKind::Struct;
        result.ir_type.name = t;
        return result;
    }

    if (t.find("union {") == 0) {
        result.ir_type.kind = InteropTypeKind::Union;
        result.ir_type.name = t;
        return result;
    }

    if (t.find("enum {") == 0) {
        result.ir_type.kind = InteropTypeKind::Enum;
        result.ir_type.name = t;
        return result;
    }

    if (t.find("fn(") == 0) {
        result.ir_type.kind = InteropTypeKind::Function;
        result.ir_type.name = t;
        return result;
    }

    auto* ir = reg.get_type(t);
    if (ir) {
        result.ir_type = *ir;
        return result;
    }

    result.ir_type.kind = InteropTypeKind::Unknown;
    result.ir_type.name = t;
    return result;
}

std::string ZigMapper::map_from_ir(const InteropType& ir, const InteropTypeRegistry& reg) const {
    switch (ir.kind) {
        case InteropTypeKind::Void:      return "void";
        case InteropTypeKind::Bool:      return "bool";
        case InteropTypeKind::Int8:      return "i8";
        case InteropTypeKind::Int16:     return "i16";
        case InteropTypeKind::Int32:     return "i32";
        case InteropTypeKind::Int64:     return "i64";
        case InteropTypeKind::UInt8:     return "u8";
        case InteropTypeKind::UInt16:    return "u16";
        case InteropTypeKind::UInt32:    return "u32";
        case InteropTypeKind::UInt64:    return "u64";
        case InteropTypeKind::Float16:   return "f16";
        case InteropTypeKind::Float32:   return "f32";
        case InteropTypeKind::Float64:   return "f64";
        case InteropTypeKind::Char:      return "u21";
        case InteropTypeKind::String:    return "[]const u8";
        case InteropTypeKind::CString:   return "[*c]u8";
        case InteropTypeKind::Pointer:   return ir.is_nullable ? "?*anyopaque" : "*anyopaque";
        case InteropTypeKind::List:      return "[]" + ir.element_type;
        case InteropTypeKind::Map:       return "std.AutoHashMap(" + ir.key_type + ", " + ir.value_type + ")";
        case InteropTypeKind::Optional:  return "?" + ir.inner_type;
        case InteropTypeKind::Any:       return "anyopaque";
        case InteropTypeKind::Alias:     return ir.alias_for;
        default:                         return "anyopaque";
    }
}
