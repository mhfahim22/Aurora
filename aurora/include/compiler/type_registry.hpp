#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>

/* ── Struct field info ── */
struct StructFieldInfo {
    std::string name;
    std::string type_name;     /* field type annotation (e.g. "int", "float", "Point") */
    std::string default_value;
    int         position{ 0 };
    bool        is_string{ false };
    bool        is_float { false };
};

/* ── Full struct definition ── */
struct StructInfo {
    std::string name;
    bool        is_union{ false };   /* true for extern union declarations */
    bool        is_opaque{ false };  /* true for opaque (forward-declared) extern structs */
    std::vector<StructFieldInfo> fields;

    int field_index(const std::string& fname) const {
        for (int i = 0; i < (int)fields.size(); i++)
            if (fields[i].name == fname) return i;
        return -1;
    }

    bool has_field(const std::string& fname) const {
        return field_index(fname) >= 0;
    }
};

/* ── Enum variant ── */
struct EnumVariantInfo {
    std::string name;
    int         value{ 0 };
};

/* ── Full enum definition ── */
struct EnumInfo {
    std::string name;
    std::vector<EnumVariantInfo> variants;
};

/* ── Interface method signature ── */
struct InterfaceMethodInfo {
    std::string              name;
    std::vector<std::string> params;
    std::string              return_type;
};

/* ── Full interface definition ── */
struct InterfaceInfo {
    std::string name;
    std::string parent_name;
    std::vector<InterfaceMethodInfo> methods;
};

/* ════════════════════════════════════════════════════════════
   TypeRegistry — struct, enum, interface, type alias registry
   ════════════════════════════════════════════════════════════ */
class TypeRegistry {
public:
    /* ── Struct ── */
    void register_struct(StructInfo info) {
        std::string name = info.name;
        structs_[name] = std::move(info);
    }

    bool has_struct(const std::string& name) const {
        return structs_.count(name) > 0;
    }

    const StructInfo* get_struct(const std::string& name) const {
        auto it = structs_.find(name);
        return (it != structs_.end()) ? &it->second : nullptr;
    }

    /* ── Enum ── */
    void register_enum(EnumInfo info) {
        std::string name = info.name;
        enums_[name] = std::move(info);
    }

    bool has_enum(const std::string& name) const {
        return enums_.count(name) > 0;
    }

    const EnumInfo* get_enum(const std::string& name) const {
        auto it = enums_.find(name);
        return (it != enums_.end()) ? &it->second : nullptr;
    }

    /* ── Interface ── */
    void register_interface(InterfaceInfo info) {
        std::string name = info.name;
        interfaces_[name] = std::move(info);
    }

    bool has_interface(const std::string& name) const {
        return interfaces_.count(name) > 0;
    }

    const InterfaceInfo* get_interface(const std::string& name) const {
        auto it = interfaces_.find(name);
        return (it != interfaces_.end()) ? &it->second : nullptr;
    }

    /* ── Type alias ── */
    void register_alias(const std::string& alias, const std::string& base) {
        type_aliases_[alias] = base;
    }

    bool is_alias(const std::string& name) const {
        return type_aliases_.count(name) > 0;
    }

    std::string resolve_alias(const std::string& name) const {
        auto it = type_aliases_.find(name);
        if (it != type_aliases_.end()) return it->second;
        return name;
    }

    /* ── General query ── */
    bool is_user_type(const std::string& name) const {
        return has_struct(name) || has_enum(name) || has_interface(name);
    }

private:
    std::unordered_map<std::string, StructInfo>    structs_;
    std::unordered_map<std::string, EnumInfo>      enums_;
    std::unordered_map<std::string, InterfaceInfo> interfaces_;
    std::unordered_map<std::string, std::string>   type_aliases_;
};

inline TypeRegistry& global_type_registry() {
    static TypeRegistry reg;
    return reg;
}
