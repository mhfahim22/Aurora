#pragma once
#include "runtime/interop/ffi_ownership.hpp"
#include "runtime/interop/type_ir.hpp"
#include <string>
#include <vector>
#include <sstream>

/* ════════════════════════════════════════════════════════════
   RAII Guard Generator — Phase 4.4
   ════════════════════════════════════════════════════════════
   Auto-generates C++ RAII wrappers that manage Aurora object
   lifetimes via extern "C" FFI thunks.
   ════════════════════════════════════════════════════════════ */

struct RAIIGuardConfig {
    std::string namespace_name{"aurora"};
    bool generate_move_ops{true};
    bool generate_copy_ops{false};
    bool generate_equality{true};
    bool use_intrusive_ptr{false}; /* Use intrusive_ptr instead of unique_ptr */
};

struct RAIIFieldInfo {
    std::string name;
    InteropTypeKind type_kind;
    bool is_trivially_copyable;
};

struct RAIIMethodInfo {
    std::string name;
    std::vector<RAIIFieldInfo> params;
    RAIIFieldInfo ret;
    bool is_const{false};
    bool is_static{false};
    bool is_virtual{false};
    bool is_constructor{false};
    bool is_destructor{false};
};

struct RAIIClassInfo {
    std::string name;
    std::string c_type;      /* extern "C" handle type */
    std::string destroy_fn;  /* destructor function name */
    bool is_trivially_copyable{false};
    std::vector<RAIIFieldInfo> fields;
    std::vector<RAIIMethodInfo> methods;
};

class RAIIGuardGenerator {
public:
    explicit RAIIGuardGenerator(const RAIIGuardConfig& cfg) : config_(cfg) {}

    /* ── Generate RAII C++ class from FFI declarations ── */
    std::string generate(const RAIIClassInfo& cls) const {
        std::ostringstream h;

        h << "// Auto-generated RAII wrapper for " << cls.name << "\n";
        h << "// ⚡ Zero-cost if trivially copyable; alloc-cost otherwise\n\n";
        if (!config_.namespace_name.empty())
            h << "namespace " << config_.namespace_name << " {\n\n";

        /* Forward declaration */
        h << "struct " << cls.name << ";\n\n";

        /* The class itself */
        if (cls.is_trivially_copyable) {
            generate_trivially_copyable(h, cls);
        } else {
            generate_handle_based(h, cls);
        }

        if (!config_.namespace_name.empty())
            h << "} // namespace " << config_.namespace_name << "\n";

        return h.str();
    }

    /* ── Generate extern "C" thunk declarations that RAII wrappers call ── */
    std::string generate_thunk_declarations(const RAIIClassInfo& cls) const {
        std::ostringstream h;
        h << "// extern \"C\" FFI thunks for " << cls.name << "\n";
        h << "extern \"C\" {\n";

        if (!cls.destroy_fn.empty()) {
            h << "    void " << cls.destroy_fn << "(void* obj);\n";
        }

        for (auto& m : cls.methods) {
            if (m.is_constructor || m.is_destructor || m.is_static)
                continue;
            h << "    // " << m.name << "\n";
        }

        h << "} // extern \"C\"\n";
        return h.str();
    }

private:
    RAIIGuardConfig config_;

    /* ── Trivially copyable: stored by value, zero-cost ── */
    void generate_trivially_copyable(std::ostringstream& h, const RAIIClassInfo& cls) const {
        h << "struct " << cls.name << " {\n";

        /* Fields (public) */
        for (auto& f : cls.fields) {
            h << "    " << interop_type_to_c(f.type_kind) << " " << f.name << "{};\n";
        }
        h << "\n";

        /* Methods */
        for (auto& m : cls.methods) {
            if (m.is_constructor) continue;
            if (m.is_destructor) continue;
            h << "    " << (m.is_static ? "static " : "")
              << interop_type_to_c(m.ret.type_kind) << " "
              << m.name << "(";
            for (size_t i = 0; i < m.params.size(); i++) {
                if (i > 0) h << ", ";
                h << interop_type_to_c(m.params[i].type_kind) << " " << m.params[i].name;
            }
            h << ")";
            if (m.is_const) h << " const";
            h << " { /* call thunk */ }\n";
        }

        h << "};\n";
    }

    /* ── Handle-based: stored as unique_ptr / intrusive_ptr ── */
    void generate_handle_based(std::ostringstream& h, const RAIIClassInfo& cls) const {
        std::string handle_type = cls.c_type.empty() ? "void*" : cls.c_type;

        h << "class " << cls.name << " {\n";
        h << "public:\n";

        /* Constructor from native handle */
        h << "    explicit " << cls.name << "(" << handle_type << " handle)\n";
        h << "        : handle_(handle, &" << (cls.destroy_fn.empty() ? "destroy_default" : cls.destroy_fn) << ") {}\n\n";

        /* Destructor - automatic via unique_ptr deleter */
        if (!cls.destroy_fn.empty()) {
            h << "    ~" << cls.name << "() = default;\n\n";
        }

        /* Move ops */
        if (config_.generate_move_ops) {
            h << "    " << cls.name << "(" << cls.name << "&&) = default;\n";
            h << "    " << cls.name << "& operator=(" << cls.name << "&&) = default;\n\n";
        }

        /* Copy ops (opt-in, often expensive) */
        if (config_.generate_copy_ops) {
            h << "    " << cls.name << "(const " << cls.name << "&) = delete;\n";
            h << "    " << cls.name << "& operator=(const " << cls.name << "&) = delete;\n\n";
        } else {
            h << "    " << cls.name << "(const " << cls.name << "&) = delete;\n";
            h << "    " << cls.name << "& operator=(const " << cls.name << "&) = delete;\n\n";
        }

        /* Raw handle access */
        h << "    " << handle_type << " get() const { return handle_.get(); }\n";
        h << "    explicit operator bool() const { return handle_ != nullptr; }\n\n";

        /* Methods */
        for (auto& m : cls.methods) {
            if (m.is_constructor || m.is_destructor) continue;
            if (m.is_static) {
                h << "    static " << interop_type_to_c(m.ret.type_kind) << " "
                  << m.name << "(";
                for (size_t i = 0; i < m.params.size(); i++) {
                    if (i > 0) h << ", ";
                    h << interop_type_to_c(m.params[i].type_kind) << " " << m.params[i].name;
                }
                h << ") { /* call static thunk */ }\n\n";
            } else {
                h << "    " << interop_type_to_c(m.ret.type_kind) << " "
                  << m.name << "(";
                for (size_t i = 0; i < m.params.size(); i++) {
                    if (i > 0) h << ", ";
                    h << interop_type_to_c(m.params[i].type_kind) << " " << m.params[i].name;
                }
                h << ")";
                if (m.is_const) h << " const";
                h << " { /* call instance thunk */ }\n\n";
            }
        }

        /* Equality operators */
        if (config_.generate_equality) {
            h << "    bool operator==(const " << cls.name << "& other) const { return handle_.get() == other.handle_.get(); }\n";
            h << "    bool operator!=(const " << cls.name << "& other) const { return handle_.get() != other.handle_.get(); }\n";
        }

        h << "private:\n";
        h << "    struct Deleter {\n";
        h << "        void operator()(" << handle_type << " p) const {\n";
        if (!cls.destroy_fn.empty())
            h << "            " << cls.destroy_fn << "(p);\n";
        else
            h << "            (void)p; // no-op: borrowed reference\n";
        h << "        }\n";
        h << "    };\n";
        h << "    std::unique_ptr<std::remove_pointer_t<" << handle_type << ">, Deleter> handle_;\n";
        h << "};\n";
    }

    /* ── Map InteropTypeKind to C type name ── */
    static std::string interop_type_to_c(InteropTypeKind kind) {
        switch (kind) {
            case InteropTypeKind::Void:    return "void";
            case InteropTypeKind::Bool:    return "bool";
            case InteropTypeKind::Int8:    return "int8_t";
            case InteropTypeKind::Int16:   return "int16_t";
            case InteropTypeKind::Int32:   return "int32_t";
            case InteropTypeKind::Int64:   return "int64_t";
            case InteropTypeKind::UInt8:   return "uint8_t";
            case InteropTypeKind::UInt16:  return "uint16_t";
            case InteropTypeKind::UInt32:  return "uint32_t";
            case InteropTypeKind::UInt64:  return "uint64_t";
            case InteropTypeKind::Float16: return "uint16_t"; /* storage only */
            case InteropTypeKind::Float32: return "float";
            case InteropTypeKind::Float64: return "double";
            case InteropTypeKind::Char:    return "char";
            case InteropTypeKind::CString: return "const char*";
            case InteropTypeKind::Pointer: return "void*";
            case InteropTypeKind::Reference: return "void*";
            default:                       return "void*";
        }
    }
};
