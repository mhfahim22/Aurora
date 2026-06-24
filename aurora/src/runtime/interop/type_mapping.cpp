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
   Language-specific mappers moved to type_mapping_langs.cpp
   ═══════════════════════════════════════════════════════════════ */
