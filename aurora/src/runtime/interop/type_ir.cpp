#include "runtime/interop/type_ir.hpp"
#include <sstream>
#include <cassert>

static const char* kind_name(InteropTypeKind k) {
    switch (k) {
        case InteropTypeKind::Unknown:   return "unknown";
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
        case InteropTypeKind::Reference: return "ref";
        case InteropTypeKind::Array:     return "array";
        case InteropTypeKind::Slice:     return "slice";
        case InteropTypeKind::Struct:    return "struct";
        case InteropTypeKind::Union:     return "union";
        case InteropTypeKind::Enum:      return "enum";
        case InteropTypeKind::Class:     return "class";
        case InteropTypeKind::Interface: return "interface";
        case InteropTypeKind::Object:    return "object";
        case InteropTypeKind::Function:  return "function";
        case InteropTypeKind::Callback:  return "callback";
        case InteropTypeKind::List:      return "list";
        case InteropTypeKind::Map:       return "map";
        case InteropTypeKind::Set:       return "set";
        case InteropTypeKind::Tuple:     return "tuple";
        case InteropTypeKind::Optional:  return "optional";
        case InteropTypeKind::Result:    return "result";
        case InteropTypeKind::Any:       return "any";
        case InteropTypeKind::Variant:   return "variant";
        case InteropTypeKind::Future:    return "future";
        case InteropTypeKind::Alias:     return "alias";
        case InteropTypeKind::Generic:   return "generic";
    }
    return "unknown";
}

std::string InteropType::to_string() const {
    std::ostringstream os;
    switch (kind) {
        case InteropTypeKind::Void:
        case InteropTypeKind::Bool:
        case InteropTypeKind::Int8: case InteropTypeKind::Int16:
        case InteropTypeKind::Int32: case InteropTypeKind::Int64:
        case InteropTypeKind::UInt8: case InteropTypeKind::UInt16:
        case InteropTypeKind::UInt32: case InteropTypeKind::UInt64:
        case InteropTypeKind::Float16: case InteropTypeKind::Float32: case InteropTypeKind::Float64:
        case InteropTypeKind::Char:
        case InteropTypeKind::String:
        case InteropTypeKind::CString:
        case InteropTypeKind::Pointer:
        case InteropTypeKind::Any:
            os << kind_name(kind);
            break;

        case InteropTypeKind::Reference:
            os << "&" << inner_type;
            break;

        case InteropTypeKind::Array:
            os << "[" << element_type << "; " << array_size << "]";
            break;

        case InteropTypeKind::Slice:
            os << "[]" << element_type;
            break;

        case InteropTypeKind::Struct:
        case InteropTypeKind::Union:
        case InteropTypeKind::Class:
            os << kind_name(kind) << " " << name << " {";
            for (auto& f : fields) {
                os << " " << f.name << ": " << f.type_ref;
                if (f.is_optional) os << "?";
            }
            os << " }";
            break;

        case InteropTypeKind::Enum:
            os << "enum " << name << " {";
            for (auto& v : enum_variants) os << " " << v;
            os << " }";
            break;

        case InteropTypeKind::Function:
            os << "fn(";
            for (size_t i = 0; i < fn_params.size(); i++) {
                if (i > 0) os << ", ";
                os << fn_params[i].name << ": " << fn_params[i].type_ref;
            }
            os << ")";
            if (!fn_return.empty() && fn_return != "void")
                os << " -> " << fn_return;
            break;

        case InteropTypeKind::Callback:
            os << "callback(";
            for (size_t i = 0; i < fn_params.size(); i++) {
                if (i > 0) os << ", ";
                os << fn_params[i].type_ref;
            }
            os << ")";
            if (!fn_return.empty() && fn_return != "void")
                os << " -> " << fn_return;
            break;

        case InteropTypeKind::List:
            os << "list<" << element_type << ">";
            break;
        case InteropTypeKind::Map:
            os << "map<" << key_type << ", " << value_type << ">";
            break;
        case InteropTypeKind::Set:
            os << "set<" << element_type << ">";
            break;
        case InteropTypeKind::Tuple:
            os << "tuple(";
            for (auto& f : fields) {
                if (&f != &fields[0]) os << ", ";
                os << f.type_ref;
            }
            os << ")";
            break;
        case InteropTypeKind::Optional:
            os << inner_type << "?";
            break;
        case InteropTypeKind::Result:
            os << "result<" << inner_type << ", " << error_type << ">";
            break;
        case InteropTypeKind::Variant:
            os << "variant {";
            for (auto& v : enum_variants) os << " " << v;
            os << " }";
            break;
        case InteropTypeKind::Future:
            os << "future<" << inner_type << ">";
            break;
        case InteropTypeKind::Alias:
            os << name << " = " << alias_for;
            break;
        case InteropTypeKind::Generic:
            os << "$" << name;
            break;
        default:
            os << "?";
            break;
    }
    return os.str();
}

InteropType InteropType::make_primitive(InteropTypeKind kind, const std::string& name) {
    /* Start from pre-built snapshot (zeroes all unused fields, no per-field init) */
    InteropType t = prebuilt(kind);
    t.name = name;
    return t;
}

InteropType InteropType::make_int(int bits, bool is_signed) {
    InteropType t;
    t.is_signed = is_signed;
    switch (bits) {
        case 8:  t.kind = is_signed ? InteropTypeKind::Int8  : InteropTypeKind::UInt8;  break;
        case 16: t.kind = is_signed ? InteropTypeKind::Int16 : InteropTypeKind::UInt16; break;
        case 32: t.kind = is_signed ? InteropTypeKind::Int32 : InteropTypeKind::UInt32; break;
        case 64: t.kind = is_signed ? InteropTypeKind::Int64 : InteropTypeKind::UInt64; break;
        default: t.kind = InteropTypeKind::Int32; break;
    }
    t.name = kind_name(t.kind);
    return t;
}

InteropType InteropType::make_float(int bits) {
    InteropType t;
    switch (bits) {
        case 16: t.kind = InteropTypeKind::Float16; break;
        case 32: t.kind = InteropTypeKind::Float32; break;
        case 64: t.kind = InteropTypeKind::Float64; break;
        default: t.kind = InteropTypeKind::Float64; break;
    }
    t.name = kind_name(t.kind);
    return t;
}

InteropType InteropType::make_struct(const std::string& name, const std::vector<InteropField>& fields_) {
    InteropType t;
    t.kind = InteropTypeKind::Struct;
    t.name = name;
    t.fields = fields_;
    return t;
}

InteropType InteropType::make_enum(const std::string& name, const std::vector<std::string>& variants) {
    InteropType t;
    t.kind = InteropTypeKind::Enum;
    t.name = name;
    t.enum_variants = variants;
    return t;
}

InteropType InteropType::make_array(const std::string& elem, int64_t size) {
    InteropType t;
    t.kind = InteropTypeKind::Array;
    t.element_type = elem;
    t.array_size = size;
    return t;
}

InteropType InteropType::make_list(const std::string& elem) {
    InteropType t;
    t.kind = InteropTypeKind::List;
    t.element_type = elem;
    return t;
}

InteropType InteropType::make_map(const std::string& key, const std::string& val) {
    InteropType t;
    t.kind = InteropTypeKind::Map;
    t.key_type = key;
    t.value_type = val;
    return t;
}

InteropType InteropType::make_optional(const std::string& inner) {
    InteropType t;
    t.kind = InteropTypeKind::Optional;
    t.inner_type = inner;
    return t;
}

InteropType InteropType::make_result(const std::string& ok, const std::string& err) {
    InteropType t;
    t.kind = InteropTypeKind::Result;
    t.inner_type = ok;
    t.error_type = err;
    return t;
}

InteropType InteropType::make_pointer(const std::string& pointee) {
    InteropType t;
    t.kind = pointee.empty() ? InteropTypeKind::Pointer : InteropTypeKind::Reference;
    t.inner_type = pointee;
    return t;
}

InteropType InteropType::make_function(const std::vector<InteropFnParam>& params, const std::string& ret) {
    InteropType t;
    t.kind = InteropTypeKind::Function;
    t.fn_params = params;
    t.fn_return = ret;
    return t;
}

/* ═══════════════════════════════════════════════════════════════
   Pre-built type table — zero-cost construction for primitives
   ═══════════════════════════════════════════════════════════════ */
static InteropType g_prebuilt_types[] = {
    {InteropTypeKind::Unknown, "unknown"},
    {InteropTypeKind::Void,    "void"},
    {InteropTypeKind::Bool,    "bool"},
    {InteropTypeKind::Int8,    "i8"},
    {InteropTypeKind::Int16,   "i16"},
    {InteropTypeKind::Int32,   "i32"},
    {InteropTypeKind::Int64,   "i64"},
    {InteropTypeKind::UInt8,   "u8"},
    {InteropTypeKind::UInt16,  "u16"},
    {InteropTypeKind::UInt32,  "u32"},
    {InteropTypeKind::UInt64,  "u64"},
    {InteropTypeKind::Float16, "f16"},
    {InteropTypeKind::Float32, "f32"},
    {InteropTypeKind::Float64, "f64"},
    {InteropTypeKind::Char,    "char"},
    {InteropTypeKind::String,  "string"},
    {InteropTypeKind::CString, "cstring"},
    {InteropTypeKind::Pointer, "pointer"},
    {InteropTypeKind::Any,     "any"},
};

const InteropType& InteropType::prebuilt(InteropTypeKind kind) {
    size_t idx = static_cast<size_t>(kind);
    if (idx < sizeof(g_prebuilt_types)/sizeof(g_prebuilt_types[0]))
        return g_prebuilt_types[idx];
    return g_prebuilt_types[sizeof(g_prebuilt_types)/sizeof(g_prebuilt_types[0]) - 1];
}

void InteropTypeRegistry::register_type(const std::string& name, const InteropType& type) {
    types[name] = type;
}

const InteropType* InteropTypeRegistry::get_type(const std::string& name) const {
    auto it = types.find(name);
    if (it != types.end()) return &it->second;
    return nullptr;
}

bool InteropTypeRegistry::has_type(const std::string& name) const {
    return types.find(name) != types.end();
}

void InteropTypeRegistry::add_mapping(const std::string& lang_type, const std::string& ir_type) {
    /* Look up in prebuilt types first */
    for (int k = 0; k <= static_cast<int>(InteropTypeKind::Any); k++) {
        auto pk = static_cast<InteropTypeKind>(k);
        if (InteropType::prebuilt(pk).name == ir_type) {
            types[lang_type] = InteropType::prebuilt(pk);
            return;
        }
    }
    /* Fallback: create an alias entry */
    InteropType alias;
    alias.kind = InteropTypeKind::Alias;
    alias.name = lang_type;
    alias.alias_for = ir_type;
    types[lang_type] = alias;
}
