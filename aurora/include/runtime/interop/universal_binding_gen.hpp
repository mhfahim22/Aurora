#pragma once
#include "runtime/interop/type_ir.hpp"
#include "runtime/interop/type_mapping.hpp"
#include "runtime/interop/eco_type_ir_mapper.hpp"
#include "runtime/interop/universal_resolver.hpp"
#include <string>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <vector>

/* ════════════════════════════════════════════════════════════
   Universal .au Binding Generator — Phase 5.4
   ════════════════════════════════════════════════════════════
   Generates unified Aurora .au binding files with Type IR
   cost annotations for any ecosystem package.

   Supports all 6 bidirectional marshaling directions:
     Aurora ↔ Python, Aurora ↔ Rust, Aurora ↔ QuickJS
   ════════════════════════════════════════════════════════════ */

enum class BindDirection {
    ToAurora,   /* Foreign language → Aurora */
    FromAurora, /* Aurora → Foreign language */
    Bidirectional
};

struct BindingGenOptions {
    bool include_cost_annotations{true};
    bool include_marshal_stubs{true};
    bool include_dependency_info{true};
    bool strict_unknown_cost{false};
    BindDirection direction{BindDirection::Bidirectional};
};

class UniversalBindingGenerator {
public:
    explicit UniversalBindingGenerator(const EcosystemTypeIRMapper& mapper,
                                        BindingGenOptions opts = {})
        : mapper_(mapper), opts_(opts) {}

    /* ── Generate a complete .au binding file ── */
    std::string generate(const UnifiedPackageInfo& pkg) const {
        std::ostringstream au;
        auto now = std::time(nullptr);
        struct tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &now);
#else
        localtime_r(&now, &tm_buf);
#endif

        au << "/* ════════════════════════════════════════════════════════════\n";
        au << "   Universal Bridge — Auto-generated Aurora FFI Bindings\n";
        au << "   Package: " << pkg.name << "@" << pkg.version << "\n";
        au << "   Ecosystem: " << ecosystem_name(pkg.ecosystem) << "\n";
        if (!pkg.description.empty())
            au << "   Description: " << pkg.description << "\n";
        au << "   Direction: " << direction_name(opts_.direction) << "\n";
        au << "   Generated: " << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << "\n";
        au << "   ════════════════════════════════════════════════════════════ */\n\n";

        /* ── Top-level extern block ── */
        au << "extern \"" << ecosystem_bridge_name(pkg.ecosystem)
           << "\" \"" << safe_ident(pkg.name) << "\"\n\n";

        /* ── Module entry ── */
        au << generate_module_entry(pkg);

        /* ── Type declarations ── */
        au << generate_type_declarations(pkg);

        /* ── Function stubs ── */
        au << generate_function_stubs(pkg);

        /* ── Bidirectional marshal stubs ── */
        if (opts_.include_marshal_stubs) {
            au << generate_bidirectional_marshal_stubs(pkg);
        }

        /* ── Dependency info ── */
        if (opts_.include_dependency_info && !pkg.dependencies.empty()) {
            au << "/* Cross-ecosystem dependencies */\n";
            for (auto& dep : pkg.dependencies) {
                au << "// depends: " << ecosystem_name(dep.ecosystem)
                   << ":" << dep.name << "@" << dep.version << "\n";
            }
            au << "\n";
        }

        au << "/* ── Usage ──\n";
        au << "   To use this package from Aurora:\n";
        au << "     import \"" << pkg.name << "\"\n";
        au << "   \n";
        au << "   Direction: " << direction_name(opts_.direction) << "\n";
        if (opts_.include_cost_annotations)
            au << "   Cost annotations show performance impact of each type crossing.\n";
        au << "*/\n";

        return au.str();
    }

    /* ── Generate type declarations with cost annotations ── */
    std::string generate_type_declarations(const UnifiedPackageInfo& pkg) const {
        std::ostringstream t;
        t << "/* Type declarations with cost annotations */\n";

        switch (pkg.ecosystem) {
            case Ecosystem::PyPI:
                t << "type Py_int    = i64    @cost(zero)     // Python int → i64\n";
                t << "type Py_float  = f64    @cost(zero)     // Python float → f64\n";
                t << "type Py_str    = String @cost(alloc)    // Python str → String (heap alloc)\n";
                t << "type Py_bool   = bool   @cost(zero)     // Python bool → bool\n";
                t << "type Py_bytes  = []u8   @cost(alloc)    // Python bytes → []u8\n";
                t << "type Py_list   = List   @cost(alloc)    // Python list ↔ List\n";
                t << "type Py_dict   = Map    @cost(alloc)    // Python dict ↔ Map\n";
                t << "type Py_object = pointer @cost(indirection) // Python object → void*\n";
                if (opts_.direction == BindDirection::Bidirectional || opts_.direction == BindDirection::FromAurora) {
                    t << "// Aurora → Python marshal helpers\n";
                    t << "type Au_to_Py = pointer @cost(alloc) // Aurora value → PyObject*\n";
                }
                if (opts_.direction == BindDirection::Bidirectional || opts_.direction == BindDirection::ToAurora) {
                    t << "// Python → Aurora marshal helpers\n";
                    t << "type Py_to_Au = i64 @cost(alloc) // PyObject* → Aurora value\n";
                }
                break;

            case Ecosystem::Npm:
                t << "type JS_number  = f64   @cost(zero)     // JS number → f64\n";
                t << "type JS_string  = String @cost(alloc)   // JS string → String\n";
                t << "type JS_boolean = bool  @cost(zero)     // JS boolean → bool\n";
                t << "type JS_object  = pointer @cost(indirection) // JS object → void*\n";
                t << "type JS_array   = List  @cost(alloc)    // JS Array ↔ List\n";
                t << "type JS_Buffer  = []u8  @cost(alloc)    // Node Buffer → []u8\n";
                break;

            case Ecosystem::Cargo:
                t << "type Rs_i32     = i32    @cost(zero)     // Rust i32\n";
                t << "type Rs_i64     = i64    @cost(zero)     // Rust i64\n";
                t << "type Rs_f64     = f64    @cost(zero)     // Rust f64\n";
                t << "type Rs_bool    = bool   @cost(zero)     // Rust bool\n";
                t << "type Rs_String  = String @cost(alloc)    // Rust String\n";
                t << "type Rs_Vec     = List   @cost(alloc)    // Rust Vec ↔ List\n";
                t << "type Rs_Option  = Optional @cost(zero)   // Rust Option<T>\n";
                t << "type Rs_Result  = Result  @cost(zero)   // Rust Result<T,E>\n";
                t << "type Rs_Slice_i64 = { ptr: i64, len: i64 } @cost(zero) // Rust &[i64]\n";
                break;

            case Ecosystem::Native:
                t << "type Native_int   = i32    @cost(zero)\n";
                t << "type Native_long  = i64    @cost(zero)\n";
                t << "type Native_float = f32    @cost(zero)\n";
                t << "type Native_double= f64    @cost(zero)\n";
                t << "type Native_ptr   = pointer @cost(indirection)\n";
                t << "type Native_str   = cstring @cost(indirection)\n";
                break;

            default: break;
        }

        t << "\n";
        return t.str();
    }

    /* ── Generate bidirectional marshal stubs for all 6 directions ── */
    std::string generate_bidirectional_marshal_stubs(const UnifiedPackageInfo& pkg) const {
        std::ostringstream m;
        m << "/* ════════════════════════════════════════════════════════════\n";
        m << "   Bidirectional Marshal Stubs\n";
        m << "   All 6 directions where applicable:\n";
        m << "   1. Aurora → " << ecosystem_name(pkg.ecosystem) << "\n";
        m << "   2. " << ecosystem_name(pkg.ecosystem) << " → Aurora\n";
        m << "   ════════════════════════════════════════════════════════════ */\n\n";

        if (opts_.direction == BindDirection::ToAurora || opts_.direction == BindDirection::Bidirectional) {
            m << generate_to_aurora_stubs(pkg);
        }
        if (opts_.direction == BindDirection::FromAurora || opts_.direction == BindDirection::Bidirectional) {
            m << generate_from_aurora_stubs(pkg);
        }

        return m.str();
    }

    /* ── Generate stubs for marshaling FROM foreign language TO Aurora ── */
    std::string generate_to_aurora_stubs(const UnifiedPackageInfo& pkg) const {
        std::ostringstream m;
        m << "// ── Direction: " << ecosystem_name(pkg.ecosystem) << " → Aurora ──\n";

        switch (pkg.ecosystem) {
            case Ecosystem::PyPI: {
                m << "// Python int → Aurora i64\n";
                m << mapper_.generate_marshal_code(
                    mapper_.map_to_ir("int", Ecosystem::PyPI), "py_val", true);
                m << "// Python float → Aurora f64\n";
                m << mapper_.generate_marshal_code(
                    mapper_.map_to_ir("float", Ecosystem::PyPI), "py_float", true);
                m << "// Python str → Aurora String\n";
                m << mapper_.generate_marshal_code(
                    mapper_.map_to_ir("str", Ecosystem::PyPI), "py_str", true);
                m << "// Python dict → Aurora Map\n";
                m << "//   @cost(alloc) — each key/value crossed individually\n";
                m << "extern \"python\" function _bridge_dict_keys(dict: pointer) -> pointer\n";
                m << "extern \"python\" function _bridge_dict_get(dict: pointer, key: i64) -> i64\n";
                m << "extern \"python\" function _bridge_dict_size(dict: pointer) -> i64\n";
                break;
            }
            case Ecosystem::Cargo: {
                m << "// Rust i64 → Aurora i64 (zero-cost, same ABI)\n";
                m << mapper_.generate_marshal_code(
                    mapper_.map_to_ir("i64", Ecosystem::Cargo), "rs_val", true);
                m << "// Rust f64 → Aurora f64 (zero-cost, same ABI)\n";
                m << mapper_.generate_marshal_code(
                    mapper_.map_to_ir("f64", Ecosystem::Cargo), "rs_f64", true);
                m << "// Rust String → Aurora String (heap alloc)\n";
                m << mapper_.generate_marshal_code(
                    mapper_.map_to_ir("String", Ecosystem::Cargo), "rs_str", true);
                m << "// Rust Option<T> → Aurora Optional (discriminant check)\n";
                m << "//   @cost(zero) — discriminant is a register test\n";
                break;
            }
            case Ecosystem::Npm: {
                m << "// JS number → Aurora f64 (zero-cost)\n";
                m << mapper_.generate_marshal_code(
                    mapper_.map_to_ir("number", Ecosystem::Npm), "js_num", true);
                m << "// JS string → Aurora String (alloc)\n";
                m << mapper_.generate_marshal_code(
                    mapper_.map_to_ir("string", Ecosystem::Npm), "js_str", true);
                m << "// JS boolean → Aurora bool (zero-cost)\n";
                m << mapper_.generate_marshal_code(
                    mapper_.map_to_ir("boolean", Ecosystem::Npm), "js_bool", true);
                break;
            }
            default: break;
        }

        m << "\n";
        return m.str();
    }

    /* ── Generate stubs for marshaling FROM Aurora TO foreign language ── */
    std::string generate_from_aurora_stubs(const UnifiedPackageInfo& pkg) const {
        std::ostringstream m;
        m << "// ── Direction: Aurora → " << ecosystem_name(pkg.ecosystem) << " ──\n";

        switch (pkg.ecosystem) {
            case Ecosystem::PyPI: {
                m << "// Aurora i64 → Python int (zero-cost cast)\n";
                m << mapper_.generate_marshal_code(
                    mapper_.map_to_ir("int", Ecosystem::PyPI), "au_int", false);
                m << "// Aurora f64 → Python float (zero-cost cast)\n";
                m << mapper_.generate_marshal_code(
                    mapper_.map_to_ir("float", Ecosystem::PyPI), "au_float", false);
                m << "// Aurora String → Python str (alloc)\n";
                m << mapper_.generate_marshal_code(
                    mapper_.map_to_ir("str", Ecosystem::PyPI), "au_str", false);
                break;
            }
            case Ecosystem::Cargo: {
                m << "// Aurora i64 → Rust i64 (zero-cost, same ABI)\n";
                m << mapper_.generate_marshal_code(
                    mapper_.map_to_ir("i64", Ecosystem::Cargo), "au_val", false);
                m << "// Aurora f64 → Rust f64 (zero-cost, same ABI)\n";
                m << mapper_.generate_marshal_code(
                    mapper_.map_to_ir("f64", Ecosystem::Cargo), "au_f64", false);
                m << "// Aurora String → Rust String (ownership transfer)\n";
                m << mapper_.generate_marshal_code(
                    mapper_.map_to_ir("String", Ecosystem::Cargo), "au_str", false);
                break;
            }
            case Ecosystem::Npm: {
                m << "// Aurora f64 → JS number (zero-cost)\n";
                m << mapper_.generate_marshal_code(
                    mapper_.map_to_ir("number", Ecosystem::Npm), "au_num", false);
                m << "// Aurora String → JS string (alloc)\n";
                m << mapper_.generate_marshal_code(
                    mapper_.map_to_ir("string", Ecosystem::Npm), "au_str", false);
                break;
            }
            default: break;
        }

        m << "\n";
        return m.str();
    }

    /* ── Generate function stubs ── */
    std::string generate_function_stubs(const UnifiedPackageInfo& pkg) const {
        std::ostringstream f;
        f << "/* FFI function signatures */\n";

        switch (pkg.ecosystem) {
            case Ecosystem::PyPI:
                f << "function " << safe_ident(pkg.name) << "_import() -> pointer\n";
                f << "function " << safe_ident(pkg.name) << "_call(fn_name: cstring, args: pointer) -> pointer\n";
                f << "function " << safe_ident(pkg.name) << "_call_int(fn_name: cstring, arg: i64) -> i64\n";
                f << "function " << safe_ident(pkg.name) << "_dict_new() -> pointer\n";
                f << "function " << safe_ident(pkg.name) << "_dict_set(dict: pointer, key: i64, val: i64)\n";
                f << "function " << safe_ident(pkg.name) << "_dict_get(dict: pointer, key: i64) -> i64\n";
                f << "function " << safe_ident(pkg.name) << "_free(handle: pointer)\n";
                break;

            case Ecosystem::Npm:
                f << "function " << safe_ident(pkg.name) << "_require() -> pointer\n";
                f << "function " << safe_ident(pkg.name) << "_invoke(module: pointer, fn: cstring, args: pointer) -> pointer\n";
                f << "function " << safe_ident(pkg.name) << "_call_int(fn: cstring, arg: i64) -> i64\n";
                break;

            case Ecosystem::Cargo:
                f << "function " << safe_ident(pkg.name) << "_create() -> pointer\n";
                f << "function " << safe_ident(pkg.name) << "_method(obj: pointer, method_id: i32, args: pointer) -> pointer\n";
                f << "function " << safe_ident(pkg.name) << "_drop(obj: pointer)\n";
                f << "function " << safe_ident(pkg.name) << "_add(a: i64, b: i64) -> i64\n";
                break;

            case Ecosystem::Native:
                f << "function " << safe_ident(pkg.name) << "_load(path: cstring) -> pointer\n";
                f << "function " << safe_ident(pkg.name) << "_call(func: cstring, args: pointer) -> pointer\n";
                f << "function " << safe_ident(pkg.name) << "_free(lib: pointer)\n";
                break;

            default: break;
        }

        f << "\n";
        return f.str();
    }

    /* ── Generate module entry point ── */
    std::string generate_module_entry(const UnifiedPackageInfo& pkg) const {
        std::ostringstream e;
        e << "/* Module initialization */\n";
        switch (pkg.ecosystem) {
            case Ecosystem::PyPI:
                e << "extern \"python\" \"" << safe_ident(pkg.name)
                  << "\" function init() -> pointer\n\n";
                break;
            case Ecosystem::Npm:
                e << "extern \"quickjs\" \"" << safe_ident(pkg.name)
                  << "\" function init() -> pointer\n\n";
                break;
            case Ecosystem::Cargo:
                e << "extern \"rust\" \"" << safe_ident(pkg.name)
                  << "\" function init() -> pointer\n\n";
                break;
            case Ecosystem::Native:
                e << "extern \"" << safe_ident(pkg.name)
                  << "\" function init() -> pointer\n\n";
                break;
            default: break;
        }
        return e.str();
    }

private:
    const EcosystemTypeIRMapper& mapper_;
    BindingGenOptions opts_;

    static std::string safe_ident(const std::string& s) {
        std::string out;
        for (char c : s) {
            if (isalnum(c) || c == '_') out += c;
            else out += '_';
        }
        if (out.empty()) out = "_";
        return out;
    }

    static const char* ecosystem_bridge_name(Ecosystem e) {
        switch (e) {
            case Ecosystem::PyPI:   return "python";
            case Ecosystem::Npm:    return "quickjs";
            case Ecosystem::Cargo:  return "rust";
            case Ecosystem::Native: return "c";
            default:                return "c";
        }
    }

    static const char* direction_name(BindDirection d) {
        switch (d) {
            case BindDirection::ToAurora:       return "→ Aurora";
            case BindDirection::FromAurora:     return "Aurora →";
            case BindDirection::Bidirectional:  return "Aurora ↔";
            default:                            return "unknown";
        }
    }
};
