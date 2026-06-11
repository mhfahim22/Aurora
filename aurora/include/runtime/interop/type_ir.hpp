#pragma once
#include "common/types.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <cstdint>

enum class InteropTypeKind {
    Unknown,

    Void,
    Bool,

    Int8, Int16, Int32, Int64,
    UInt8, UInt16, UInt32, UInt64,
    Float16, Float32, Float64,

    Char,
    String,
    CString,

    Pointer,
    Reference,

    Array,
    Slice,

    Struct,
    Union,
    Enum,

    Class,
    Interface,
    Object,

    Function,
    Callback,

    List,
    Map,
    Set,
    Tuple,

    Optional,
    Result,
    Any,
    Variant,
    Future,

    Alias,
    Generic,
};

enum class InteropLang {
    Unknown,
    Aurora,
    C,
    Cpp,
    Python,
    JavaScript,
    Rust,
    Java,
    CSharp,
    Go,
    Zig,
};

struct InteropField {
    std::string name;
    std::string type_ref;
    bool is_optional{false};
};

struct InteropFnParam {
    std::string name;
    std::string type_ref;
};

struct InteropType {
    InteropTypeKind kind{InteropTypeKind::Unknown};
    std::string name;
    std::string original_name;

    int64_t array_size{0};

    std::string element_type;

    std::vector<InteropField> fields;
    std::vector<std::string> enum_variants;
    std::vector<InteropFnParam> fn_params;
    std::string fn_return;

    std::string key_type;
    std::string value_type;

    std::string inner_type;
    std::string error_type;
    std::string alias_for;

    bool is_signed{true};
    bool is_mutable{true};
    bool is_nullable{false};

    std::unordered_map<std::string, std::string> attrs;

    std::string to_string() const;

    /* ════════════════════════════════════════════════════════════
       Construction — each returns a new InteropType
       ════════════════════════════════════════════════════════════ */
    static InteropType make_primitive(InteropTypeKind kind, const std::string& name);
    static InteropType make_int(int bits, bool is_signed);
    static InteropType make_float(int bits);
    static InteropType make_struct(const std::string& name, const std::vector<InteropField>& fields);
    static InteropType make_enum(const std::string& name, const std::vector<std::string>& variants);
    static InteropType make_array(const std::string& elem, int64_t size);
    static InteropType make_list(const std::string& elem);
    static InteropType make_map(const std::string& key, const std::string& val);
    static InteropType make_optional(const std::string& inner);
    static InteropType make_result(const std::string& ok, const std::string& err);
    static InteropType make_pointer(const std::string& pointee);
    static InteropType make_function(const std::vector<InteropFnParam>& params, const std::string& ret);

    /* ════════════════════════════════════════════════════════════
       Fast path: pre-built constant types (zero-cost construct)
       Returns const ref — no string allocation, no copy.
       ════════════════════════════════════════════════════════════ */
    static const InteropType& prebuilt(InteropTypeKind kind);
};

struct InteropTypeRegistry {
    std::unordered_map<std::string, InteropType> types;

    void register_type(const std::string& name, const InteropType& type);
    const InteropType* get_type(const std::string& name) const;
    bool has_type(const std::string& name) const;
    void add_mapping(const std::string& lang_type, const std::string& ir_type);
};
