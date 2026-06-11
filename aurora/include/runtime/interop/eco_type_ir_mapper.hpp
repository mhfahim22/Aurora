#pragma once
#include "runtime/interop/type_ir.hpp"
#include "runtime/interop/type_mapping.hpp"
#include "runtime/interop/universal_resolver.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>

/* ════════════════════════════════════════════════════════════
   Ecosystem Type IR Mapper — Phase 5.3
   ════════════════════════════════════════════════════════════
   Maps common types from each ecosystem to Aurora Type IR,
   using Phase 1 type mapping system. Annotates with cost tiers.
   ════════════════════════════════════════════════════════════ */

class EcosystemTypeIRMapper {
public:
    EcosystemTypeIRMapper() {
        init_pypi_types();
        init_npm_types();
        init_cargo_types();
        init_native_types();
    }

    MappedType map_to_ir(const std::string& eco_type, Ecosystem eco) const {
        MappedType result;
        result.native_type = eco_type;

        for (auto& e : entries_) {
            if (e.eco == eco && e.name == eco_type) {
                result.ir_type = e.type;
                result.cost = calculate_cost(e.type.kind);
                result.is_trivially_castable = (result.cost == InteropCost::ZeroCost);
                result.native_size = estimate_size(e.type.kind);
                return result;
            }
        }

        result.ir_type = InteropType::make_pointer("void");
        result.ir_type.name = eco_type;
        result.cost = InteropCost::UnknownCost;
        result.is_trivially_castable = false;
        result.native_size = sizeof(void*);
        return result;
    }

    std::string to_aurora(const std::string& eco_type, Ecosystem eco) const {
        for (auto& e : entries_) {
            if (e.eco == eco && e.name == eco_type)
                return e.aurora_name;
        }
        return "pointer";
    }

    std::string generate_marshal_code(const MappedType& mapped,
                                       const std::string& var_name,
                                       bool to_aurora_direction) const
    {
        std::ostringstream code;

        switch (mapped.cost) {
            case InteropCost::ZeroCost:
                code << "    // Zero-cost: " << var_name << " has same ABI layout\n";
                break;

            case InteropCost::CastCost:
                code << "    " << var_name << " = (" << mapped.native_type << ")" << var_name << ";\n";
                break;

            case InteropCost::IndirectionCost:
                code << "    // Indirection: " << var_name << " through vtable/interface\n";
                code << "    void* " << var_name << "_ptr = " << var_name << ";\n";
                break;

            case InteropCost::AllocCost:
                if (to_aurora_direction) {
                    code << "    // Alloc-cost: convert " << mapped.native_type
                         << " to Aurora String (heap alloc)\n";
                    code << "    aurora_string_t* " << var_name
                         << "_aur = aurora_string_from_c(" << var_name << ");\n";
                } else {
                    code << "    // Alloc-cost: convert Aurora String to "
                         << mapped.native_type << " (heap alloc)\n";
                }
                code << "    // ⚠ Cost: " << cost_name(mapped.cost) << "\n";
                break;

            case InteropCost::UnknownCost:
                code << "    // ⚠ Unknown cost for " << var_name
                     << " — compile error in strict mode\n";
                code << "    // User must annotate explicitly\n";
                break;

            default:
                break;
        }

        return code.str();
    }

    static const char* cost_name(InteropCost c) {
        switch (c) {
            case InteropCost::ZeroCost:        return "zero";
            case InteropCost::CastCost:        return "cast";
            case InteropCost::IndirectionCost: return "indirection";
            case InteropCost::MarshalCost:     return "marshal";
            case InteropCost::AllocCost:       return "alloc";
            case InteropCost::UnknownCost:     return "unknown";
        }
        return "unknown";
    }

private:
    struct EcoTypeEntry {
        Ecosystem eco;
        std::string name;
        InteropType type;
        std::string aurora_name;
    };

    std::vector<EcoTypeEntry> entries_;

    void add(Ecosystem eco, const std::string& name,
             const InteropType& type, const std::string& aurora)
    {
        entries_.push_back({eco, name, type, aurora});
    }

    static InteropCost calculate_cost(InteropTypeKind kind) {
        switch (kind) {
            case InteropTypeKind::Bool:
            case InteropTypeKind::Int8:
            case InteropTypeKind::Int16:
            case InteropTypeKind::Int32:
            case InteropTypeKind::Int64:
            case InteropTypeKind::UInt8:
            case InteropTypeKind::UInt16:
            case InteropTypeKind::UInt32:
            case InteropTypeKind::UInt64:
            case InteropTypeKind::Float32:
            case InteropTypeKind::Float64:
                return InteropCost::ZeroCost;

            case InteropTypeKind::CString:
                return InteropCost::IndirectionCost;

            case InteropTypeKind::String:
                return InteropCost::AllocCost;

            case InteropTypeKind::List:
            case InteropTypeKind::Map:
            case InteropTypeKind::Set:
                return InteropCost::AllocCost;

            case InteropTypeKind::Pointer:
            case InteropTypeKind::Reference:
                return InteropCost::IndirectionCost;

            default:
                return InteropCost::UnknownCost;
        }
    }

    static size_t estimate_size(InteropTypeKind kind) {
        switch (kind) {
            case InteropTypeKind::Bool:
            case InteropTypeKind::Int8:
            case InteropTypeKind::UInt8:   return 1;
            case InteropTypeKind::Int16:
            case InteropTypeKind::UInt16:
            case InteropTypeKind::Float16: return 2;
            case InteropTypeKind::Int32:
            case InteropTypeKind::UInt32:
            case InteropTypeKind::Float32: return 4;
            case InteropTypeKind::Int64:
            case InteropTypeKind::UInt64:
            case InteropTypeKind::Float64: return 8;
            case InteropTypeKind::Pointer:
            case InteropTypeKind::Reference: return 8;
            default: return 8;
        }
    }

    void init_pypi_types() {
        using IK = InteropTypeKind;
        add(Ecosystem::PyPI, "int",       InteropType::prebuilt(IK::Int64),   "i64");
        add(Ecosystem::PyPI, "float",     InteropType::prebuilt(IK::Float64), "f64");
        add(Ecosystem::PyPI, "str",       InteropType::prebuilt(IK::String),  "String");
        add(Ecosystem::PyPI, "bool",      InteropType::prebuilt(IK::Bool),    "bool");
        add(Ecosystem::PyPI, "bytes",     InteropType::prebuilt(IK::Array),   "[]u8");
        add(Ecosystem::PyPI, "list",      InteropType::make_list("any"),      "List");
        add(Ecosystem::PyPI, "dict",      InteropType::make_map("string", "any"), "Map");
        add(Ecosystem::PyPI, "tuple",     InteropType::prebuilt(IK::Tuple),   "Tuple");
        add(Ecosystem::PyPI, "NoneType",  InteropType::prebuilt(IK::Void),    "void");
        add(Ecosystem::PyPI, "PyObject*", InteropType::make_pointer("void"),  "pointer");
    }

    void init_npm_types() {
        using IK = InteropTypeKind;
        add(Ecosystem::Npm, "number",    InteropType::prebuilt(IK::Float64), "f64");
        add(Ecosystem::Npm, "string",    InteropType::prebuilt(IK::String),  "String");
        add(Ecosystem::Npm, "boolean",   InteropType::prebuilt(IK::Bool),    "bool");
        add(Ecosystem::Npm, "object",    InteropType::make_pointer("void"),  "pointer");
        add(Ecosystem::Npm, "Array",     InteropType::make_list("any"),      "List");
        add(Ecosystem::Npm, "Promise",   InteropType::prebuilt(IK::Future),  "Future");
        add(Ecosystem::Npm, "null",      InteropType::prebuilt(IK::Void),    "void");
        add(Ecosystem::Npm, "undefined", InteropType::prebuilt(IK::Void),    "void");
        add(Ecosystem::Npm, "Buffer",    InteropType::prebuilt(IK::Array),   "[]u8");
        add(Ecosystem::Npm, "Function",  InteropType::prebuilt(IK::Function),"Function");
        add(Ecosystem::Npm, "Error",     InteropType::prebuilt(IK::Result),  "Result");
    }

    void init_cargo_types() {
        using IK = InteropTypeKind;
        add(Ecosystem::Cargo, "i8",       InteropType::prebuilt(IK::Int8),   "i8");
        add(Ecosystem::Cargo, "i16",      InteropType::prebuilt(IK::Int16),  "i16");
        add(Ecosystem::Cargo, "i32",      InteropType::prebuilt(IK::Int32),  "i32");
        add(Ecosystem::Cargo, "i64",      InteropType::prebuilt(IK::Int64),  "i64");
        add(Ecosystem::Cargo, "u8",       InteropType::prebuilt(IK::UInt8),  "u8");
        add(Ecosystem::Cargo, "u16",      InteropType::prebuilt(IK::UInt16), "u16");
        add(Ecosystem::Cargo, "u32",      InteropType::prebuilt(IK::UInt32), "u32");
        add(Ecosystem::Cargo, "u64",      InteropType::prebuilt(IK::UInt64), "u64");
        add(Ecosystem::Cargo, "f32",      InteropType::prebuilt(IK::Float32),"f32");
        add(Ecosystem::Cargo, "f64",      InteropType::prebuilt(IK::Float64),"f64");
        add(Ecosystem::Cargo, "bool",     InteropType::prebuilt(IK::Bool),   "bool");
        add(Ecosystem::Cargo, "char",     InteropType::prebuilt(IK::Char),   "char");
        add(Ecosystem::Cargo, "String",   InteropType::prebuilt(IK::String), "String");
        add(Ecosystem::Cargo, "&str",     InteropType::prebuilt(IK::CString),"cstring");
        add(Ecosystem::Cargo, "Vec",      InteropType::make_list("any"),     "List");
        add(Ecosystem::Cargo, "HashMap",  InteropType::make_map("string", "any"), "Map");
        add(Ecosystem::Cargo, "Option",   InteropType::make_optional("any"), "Optional");
        add(Ecosystem::Cargo, "Result",   InteropType::make_result("any", "any"), "Result");
        add(Ecosystem::Cargo, "Box",      InteropType::make_pointer("void"), "pointer");
        add(Ecosystem::Cargo, "Arc",      InteropType::make_pointer("void"), "shared");
        add(Ecosystem::Cargo, "Rc",       InteropType::make_pointer("void"), "shared");
        add(Ecosystem::Cargo, "Box<dyn",  InteropType::prebuilt(IK::Object), "object");
    }

    void init_native_types() {
        using IK = InteropTypeKind;
        add(Ecosystem::Native, "void",     InteropType::prebuilt(IK::Void),    "void");
        add(Ecosystem::Native, "bool",     InteropType::prebuilt(IK::Bool),    "bool");
        add(Ecosystem::Native, "char",     InteropType::prebuilt(IK::Int8),    "i8");
        add(Ecosystem::Native, "short",    InteropType::prebuilt(IK::Int16),   "i16");
        add(Ecosystem::Native, "int",      InteropType::prebuilt(IK::Int32),   "i32");
        add(Ecosystem::Native, "long",     InteropType::prebuilt(IK::Int32),   "i32");
        add(Ecosystem::Native, "long long",InteropType::prebuilt(IK::Int64),   "i64");
        add(Ecosystem::Native, "unsigned char",  InteropType::prebuilt(IK::UInt8),  "u8");
        add(Ecosystem::Native, "unsigned short", InteropType::prebuilt(IK::UInt16), "u16");
        add(Ecosystem::Native, "unsigned int",   InteropType::prebuilt(IK::UInt32), "u32");
        add(Ecosystem::Native, "unsigned long",  InteropType::prebuilt(IK::UInt32), "u32");
        add(Ecosystem::Native, "unsigned long long", InteropType::prebuilt(IK::UInt64), "u64");
        add(Ecosystem::Native, "float",    InteropType::prebuilt(IK::Float32), "f32");
        add(Ecosystem::Native, "double",   InteropType::prebuilt(IK::Float64), "f64");
        add(Ecosystem::Native, "HANDLE",   InteropType::make_pointer("void"),  "pointer");
        add(Ecosystem::Native, "DWORD",    InteropType::prebuilt(IK::UInt32),  "u32");
        add(Ecosystem::Native, "LPVOID",   InteropType::make_pointer("void"),  "pointer");
        add(Ecosystem::Native, "LPCSTR",   InteropType::prebuilt(IK::CString), "cstring");
        add(Ecosystem::Native, "LPWSTR",   InteropType::prebuilt(IK::CString), "cstring");
        add(Ecosystem::Native, "size_t",   InteropType::prebuilt(IK::UInt64),  "u64");
        add(Ecosystem::Native, "HRESULT",  InteropType::prebuilt(IK::Int32),   "i32");
        add(Ecosystem::Native, "BOOL",     InteropType::prebuilt(IK::Bool),    "bool");
        add(Ecosystem::Native, "BYTE",     InteropType::prebuilt(IK::UInt8),   "u8");
        add(Ecosystem::Native, "WORD",     InteropType::prebuilt(IK::UInt16),  "u16");
        add(Ecosystem::Native, "INT",      InteropType::prebuilt(IK::Int32),   "i32");
        add(Ecosystem::Native, "UINT",     InteropType::prebuilt(IK::UInt32),  "u32");
        add(Ecosystem::Native, "WPARAM",   InteropType::prebuilt(IK::UInt64),  "u64");
        add(Ecosystem::Native, "LPARAM",   InteropType::prebuilt(IK::Int64),   "i64");
        add(Ecosystem::Native, "LRESULT",  InteropType::prebuilt(IK::Int64),   "i64");
        add(Ecosystem::Native, "FLOAT",    InteropType::prebuilt(IK::Float32), "f32");
        add(Ecosystem::Native, "COLORREF", InteropType::prebuilt(IK::UInt32),  "u32");
        add(Ecosystem::Native, "POINT",    InteropType::prebuilt(IK::Struct),  "struct");
        add(Ecosystem::Native, "RECT",     InteropType::prebuilt(IK::Struct),  "struct");
    }
};
