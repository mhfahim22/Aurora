#include "runtime/interop/type_mapping.hpp"
#include <algorithm>
#include <cstring>

/* ═══════════════════════════════════════════════════════════════
   TypeMappingEngine
   ═══════════════════════════════════════════════════════════════ */

void TypeMappingEngine::register_mapper(TypeMapper* mapper) {
    mapper->register_builtins(registry);
    mappers[mapper->lang()] = mapper;
}

MappedType TypeMappingEngine::translate(InteropLang from_lang, const std::string& from_type, InteropLang to_lang) const {
    auto from_it = mappers.find(from_lang);
    auto to_it = mappers.find(to_lang);
    if (from_it == mappers.end() || to_it == mappers.end())
        return {};
    MappedType ir_result = from_it->second->map_to_ir(from_type, registry);
    std::string native = to_it->second->map_from_ir(ir_result.ir_type, registry);
    MappedType result;
    result.ir_type = ir_result.ir_type;
    result.native_type = native;
    result.is_trivially_castable = ir_result.is_trivially_castable;
    result.cost = ir_result.cost;
    result.native_size = ir_result.native_size;
    return result;
}

std::string TypeMappingEngine::generate_marshal_code(const MappedType& from, const MappedType& to, const std::string& var_name) const {
    std::string code;
    InteropTypeKind fk = from.ir_type.kind;
    InteropTypeKind tk = to.ir_type.kind;

    if (from.cost == InteropCost::ZeroCost && to.cost == InteropCost::ZeroCost) {
        code = "    /* ZERO-COST: " + var_name + " (" + from.native_type + " ↔ " + to.native_type + ") */\n";
        code += "    /* compiler eliminates this — same ABI layout */\n";
        return code;
    }

    if (fk == tk && from.is_trivially_castable && to.is_trivially_castable) {
        code = "    /* ZERO-COST (trivially castable): " + var_name + " */\n";
        code += "    " + var_name + "_out = " + var_name + ";\n";
        return code;
    }

    if (fk == InteropTypeKind::String && tk == InteropTypeKind::CString) {
        code = "    /* ALLOC-COST (string→cstring copy): " + var_name + " */\n";
        code += "    const char* " + var_name + "_cstr = " + var_name + ".c_str();\n";
        return code;
    }

    if (fk == InteropTypeKind::CString && tk == InteropTypeKind::String) {
        code = "    /* ALLOC-COST (cstring→string copy): " + var_name + " */\n";
        code += "    std::string " + var_name + "_str(" + var_name + ");\n";
        return code;
    }

    if (from.cost == InteropCost::AllocCost || to.cost == InteropCost::AllocCost) {
        code = "    /* ALLOC-COST: " + var_name + " (" + from.native_type + " → " + to.native_type + ") */\n";
        if (fk == InteropTypeKind::List && tk == InteropTypeKind::Array) {
            code += "    for (size_t _i = 0; _i < " + var_name + ".size() && _i < " +
                    std::to_string(to.ir_type.array_size) + "; _i++) {\n";
            code += "        " + var_name + "_out[_i] = " + var_name + "[_i];\n";
            code += "    }\n";
        } else if (fk == InteropTypeKind::Array && tk == InteropTypeKind::List) {
            code += "    for (size_t _i = 0; _i < " + std::to_string(from.ir_type.array_size) + "; _i++) {\n";
            code += "        " + var_name + "_out.push_back(" + var_name + "[_i]);\n";
            code += "    }\n";
        } else if (fk == InteropTypeKind::Map && tk == InteropTypeKind::Struct) {
            for (auto& f : to.ir_type.fields) {
                code += "    " + var_name + "_out." + f.name + " = " + var_name + "[\"" + f.name + "\"];\n";
            }
        } else {
            code += "    /* generic alloc-cost marshal */\n";
        }
        return code;
    }

    if (from.cost == InteropCost::IndirectionCost || to.cost == InteropCost::IndirectionCost) {
        code = "    /* INDIRECTION-COST (one pointer deref): " + var_name + " */\n";
        code += "    " + var_name + "_out = " + var_name + ";\n";
        return code;
    }

    if (fk == InteropTypeKind::Optional && tk == InteropTypeKind::Pointer) {
        code = "    /* CAST-COST (optional→pointer): " + var_name + " */\n";
        code += "    void* " + var_name + "_ptr = " + var_name + ".has_value() ? &" + var_name + ".value() : nullptr;\n";
        return code;
    }

    code = "    /* UNKNOWN-COST: no automatic marshal for " + var_name;
    code += " (" + from.native_type + " -> " + to.native_type + ") */\n";
    code += "    /* COMPILE ERROR: explicit marshal required */\n";
    return code;
}

void TypeMappingEngine::init_builtins() {
    registry.register_type("void", InteropType::make_primitive(InteropTypeKind::Void, "void"));
    registry.register_type("bool", InteropType::make_primitive(InteropTypeKind::Bool, "bool"));
    registry.register_type("i8", InteropType::make_int(8, true));
    registry.register_type("i16", InteropType::make_int(16, true));
    registry.register_type("i32", InteropType::make_int(32, true));
    registry.register_type("i64", InteropType::make_int(64, true));
    registry.register_type("u8", InteropType::make_int(8, false));
    registry.register_type("u16", InteropType::make_int(16, false));
    registry.register_type("u32", InteropType::make_int(32, false));
    registry.register_type("u64", InteropType::make_int(64, false));
    registry.register_type("f16", InteropType::make_float(16));
    registry.register_type("f32", InteropType::make_float(32));
    registry.register_type("f64", InteropType::make_float(64));
    registry.register_type("char", InteropType::make_primitive(InteropTypeKind::Char, "char"));
    registry.register_type("string", InteropType::make_primitive(InteropTypeKind::String, "string"));
    registry.register_type("cstring", InteropType::make_primitive(InteropTypeKind::CString, "cstring"));
    registry.register_type("pointer", InteropType::make_primitive(InteropTypeKind::Pointer, "pointer"));
    registry.register_type("any", InteropType::make_primitive(InteropTypeKind::Any, "any"));
}

/* ═══════════════════════════════════════════════════════════════
   Aurora Mapper
   ═══════════════════════════════════════════════════════════════ */

void AuroraMapper::register_builtins(InteropTypeRegistry& reg) const {
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
    reg.add_mapping("f32", "f32");
    reg.add_mapping("f64", "f64");
    reg.add_mapping("string", "string");
    reg.add_mapping("cstring", "cstring");
    reg.add_mapping("char", "char");
    reg.add_mapping("pointer", "pointer");
    reg.add_mapping("any", "any");
}

MappedType AuroraMapper::map_to_ir(const std::string& native_type, const InteropTypeRegistry& reg) const {
    MappedType result;
    result.native_type = native_type;
    std::string t = native_type;

    auto* ir = reg.get_type(t);
    if (ir) {
        result.ir_type = *ir;
        if (t == "i8" || t == "u8" || t == "i16" || t == "u16" || t == "i32" ||
            t == "u32" || t == "i64" || t == "u64" || t == "f32" || t == "f64" ||
            t == "bool" || t == "char" || t == "pointer" || t == "cstring") {
            result.is_trivially_castable = true;
            result.cost = InteropCost::ZeroCost;
            result.native_size = 8;
        }
        if (t == "i8" || t == "u8") result.native_size = 1;
        else if (t == "i16" || t == "u16") result.native_size = 2;
        else if (t == "i32" || t == "u32" || t == "f32") result.native_size = 4;
        else if (t == "i64" || t == "u64" || t == "f64" || t == "pointer" || t == "cstring") result.native_size = 8;
        else if (t == "bool") result.native_size = 1;
        else if (t == "char") result.native_size = 4;
        return result;
    }

    /* Handle "int" synonym for Aurora — maps to i32 */
    if (t == "int") {
        result.ir_type = InteropType::prebuilt(InteropTypeKind::Int32);
        result.is_trivially_castable = true;
        result.cost = InteropCost::ZeroCost;
        result.native_size = 4;
        return result;
    }

    if (t.find("[]") != std::string::npos) {
        auto pos = t.find("[]");
        std::string before = t.substr(0, pos);
        std::string elem = before.empty() ? t.substr(pos + 2) : before;
        result.ir_type = InteropType::make_list(elem);
        result.cost = InteropCost::AllocCost;
        return result;
    }

    if (t.find("[") != std::string::npos && t.find("]") != std::string::npos) {
        auto obracket = t.find('[');
        auto cbracket = t.find(']');
        std::string before = t.substr(0, obracket);
        std::string after = t.substr(cbracket + 1);
        std::string elem = before.empty() ? after : before;
        result.ir_type = InteropType::make_array(elem, 0);
        result.cost = InteropCost::AllocCost;
        return result;
    }

    result.ir_type.kind = InteropTypeKind::Unknown;
    result.ir_type.name = t;
    result.cost = InteropCost::UnknownCost;
    return result;
}

std::string AuroraMapper::map_from_ir(const InteropType& ir, const InteropTypeRegistry& reg) const {
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
        case InteropTypeKind::Char:      return "char";
        case InteropTypeKind::String:    return "string";
        case InteropTypeKind::CString:   return "cstring";
        case InteropTypeKind::Pointer:   return "pointer";
        case InteropTypeKind::Any:       return "any";
        case InteropTypeKind::List:      return "[]" + ir.element_type;
        case InteropTypeKind::Map:       return "map<" + ir.key_type + ", " + ir.value_type + ">";
        case InteropTypeKind::Optional:  return ir.inner_type + "?";
        case InteropTypeKind::Array:     return "[" + ir.element_type + "; " + std::to_string(ir.array_size) + "]";
        case InteropTypeKind::Struct:    return ir.name;
        case InteropTypeKind::Enum:      return ir.name;
        case InteropTypeKind::Alias:     return ir.alias_for;
        default:                         return ir.name.empty() ? "unknown" : ir.name;
    }
}

/* ═══════════════════════════════════════════════════════════════
   C Mapper
   ═══════════════════════════════════════════════════════════════ */

void CMapper::register_builtins(InteropTypeRegistry& reg) const {
    reg.add_mapping("void", "void");
    reg.add_mapping("_Bool", "bool");
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
    reg.add_mapping("size_t", "u64");
    reg.add_mapping("ssize_t", "i64");
    reg.add_mapping("int8_t", "i8");
    reg.add_mapping("int16_t", "i16");
    reg.add_mapping("int32_t", "i32");
    reg.add_mapping("int64_t", "i64");
    reg.add_mapping("uint8_t", "u8");
    reg.add_mapping("uint16_t", "u16");
    reg.add_mapping("uint32_t", "u32");
    reg.add_mapping("uint64_t", "u64");
    reg.add_mapping("char*", "cstring");
    reg.add_mapping("void*", "pointer");
}

MappedType CMapper::map_to_ir(const std::string& native_type, const InteropTypeRegistry& reg) const {
    MappedType result;
    result.native_type = native_type;

    std::string t = native_type;
    while (!t.empty() && t.back() == ' ') t.pop_back();

    auto* ir = reg.get_type(t);
    if (ir) {
        result.ir_type = *ir;
        if (ir->kind == InteropTypeKind::Alias) {
            auto* resolved = reg.get_type(ir->alias_for);
            if (resolved) result.ir_type = *resolved;
        }
        result.is_trivially_castable = true;
        result.cost = InteropCost::ZeroCost;
        return result;
    }

    if (t.find('*') != std::string::npos) {
        result.ir_type = InteropType::make_primitive(InteropTypeKind::Pointer, "pointer");
        result.native_type = t;
        result.is_trivially_castable = true;
        result.cost = InteropCost::ZeroCost;
        return result;
    }

    if (t.find("struct ") == 0) {
        result.ir_type.kind = InteropTypeKind::Struct;
        result.ir_type.name = t.substr(7);
        result.cost = InteropCost::ZeroCost;
        return result;
    }

    if (t.find("enum ") == 0) {
        result.ir_type.kind = InteropTypeKind::Enum;
        result.ir_type.name = t.substr(5);
        result.ir_type.is_signed = false;
        result.cost = InteropCost::ZeroCost;
        return result;
    }

    if (t.find("union ") == 0) {
        result.ir_type.kind = InteropTypeKind::Union;
        result.ir_type.name = t.substr(6);
        result.cost = InteropCost::ZeroCost;
        return result;
    }

    result.ir_type.kind = InteropTypeKind::Unknown;
    result.ir_type.name = t;
    result.cost = InteropCost::UnknownCost;
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
    reg.add_mapping("std::vector", "list");
    reg.add_mapping("std::map", "map");
    reg.add_mapping("std::unordered_map", "map");
    reg.add_mapping("std::set", "set");
    reg.add_mapping("std::optional", "optional");
    reg.add_mapping("std::variant", "variant");
    reg.add_mapping("std::any", "any");
    reg.add_mapping("std::shared_ptr", "pointer");
    reg.add_mapping("std::unique_ptr", "pointer");
    reg.add_mapping("std::function", "function");
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
        case InteropTypeKind::Pointer:   return "int"; /* cast to pointer */
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
