#pragma once
#include "runtime/interop/type_ir.hpp"
#include <functional>

enum class InteropCost : uint8_t {
    ZeroCost,       /* Same ABI layout — compiler eliminates (e.g. i32 ↔ int32_t) */
    CastCost,       /* Numeric cast — ~1 CPU cycle (e.g. i32 → i64) */
    IndirectionCost, /* One pointer dereference (e.g. class method via vtable) */
    MarshalCost,    /* Needs conversion but no allocation (e.g. struct field reorder) */
    AllocCost,      /* Needs heap allocation (e.g. string/list/dict copy) */
    UnknownCost,    /* Cost not yet determined */
};

inline const char* interop_cost_name(InteropCost c) {
    switch (c) {
        case InteropCost::ZeroCost:         return "zero-cost";
        case InteropCost::CastCost:         return "cast";
        case InteropCost::IndirectionCost:  return "indirection";
        case InteropCost::MarshalCost:      return "marshal";
        case InteropCost::AllocCost:        return "alloc";
        case InteropCost::UnknownCost:      return "unknown";
    }
    return "unknown";
}

struct MappedType {
    InteropType ir_type;
    std::string native_type;
    InteropCost cost{InteropCost::UnknownCost};
    bool is_trivially_castable{false};   /* same ABI layout → reinterpret_cast is safe */
    size_t native_size{0};
    std::string marshal_to_c;
    std::string marshal_from_c;
};

class TypeMapper {
public:
    virtual ~TypeMapper() = default;
    virtual InteropLang lang() const = 0;
    virtual const char* lang_name() const = 0;

    virtual MappedType map_to_ir(const std::string& native_type, const InteropTypeRegistry& reg) const = 0;
    virtual std::string map_from_ir(const InteropType& ir, const InteropTypeRegistry& reg) const = 0;

    virtual void register_builtins(InteropTypeRegistry& reg) const = 0;
};

struct AuroraMapper : TypeMapper {
    InteropLang lang() const override { return InteropLang::Aurora; }
    const char* lang_name() const override { return "aurora"; }
    MappedType map_to_ir(const std::string& native_type, const InteropTypeRegistry& reg) const override;
    std::string map_from_ir(const InteropType& ir, const InteropTypeRegistry& reg) const override;
    void register_builtins(InteropTypeRegistry& reg) const override;
};

struct CMapper : TypeMapper {
    InteropLang lang() const override { return InteropLang::C; }
    const char* lang_name() const override { return "c"; }
    MappedType map_to_ir(const std::string& native_type, const InteropTypeRegistry& reg) const override;
    std::string map_from_ir(const InteropType& ir, const InteropTypeRegistry& reg) const override;
    void register_builtins(InteropTypeRegistry& reg) const override;
};

struct CppMapper : TypeMapper {
    InteropLang lang() const override { return InteropLang::Cpp; }
    const char* lang_name() const override { return "c++"; }
    MappedType map_to_ir(const std::string& native_type, const InteropTypeRegistry& reg) const override;
    std::string map_from_ir(const InteropType& ir, const InteropTypeRegistry& reg) const override;
    void register_builtins(InteropTypeRegistry& reg) const override;
};

struct PythonMapper : TypeMapper {
    InteropLang lang() const override { return InteropLang::Python; }
    const char* lang_name() const override { return "python"; }
    MappedType map_to_ir(const std::string& native_type, const InteropTypeRegistry& reg) const override;
    std::string map_from_ir(const InteropType& ir, const InteropTypeRegistry& reg) const override;
    void register_builtins(InteropTypeRegistry& reg) const override;
};

struct JavaScriptMapper : TypeMapper {
    InteropLang lang() const override { return InteropLang::JavaScript; }
    const char* lang_name() const override { return "javascript"; }
    MappedType map_to_ir(const std::string& native_type, const InteropTypeRegistry& reg) const override;
    std::string map_from_ir(const InteropType& ir, const InteropTypeRegistry& reg) const override;
    void register_builtins(InteropTypeRegistry& reg) const override;
};

struct RustMapper : TypeMapper {
    InteropLang lang() const override { return InteropLang::Rust; }
    const char* lang_name() const override { return "rust"; }
    MappedType map_to_ir(const std::string& native_type, const InteropTypeRegistry& reg) const override;
    std::string map_from_ir(const InteropType& ir, const InteropTypeRegistry& reg) const override;
    void register_builtins(InteropTypeRegistry& reg) const override;
};

struct TypeMappingEngine {
    InteropTypeRegistry registry;
    std::unordered_map<InteropLang, TypeMapper*> mappers;

    void register_mapper(TypeMapper* mapper);
    MappedType translate(InteropLang from_lang, const std::string& from_type, InteropLang to_lang) const;
    std::string generate_marshal_code(const MappedType& from, const MappedType& to, const std::string& var_name) const;

    void init_builtins();
};
