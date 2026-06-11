#pragma once
#include "runtime/interop/type_ir.hpp"
#include "runtime/interop/type_mapping.hpp"
#include "runtime/interop/eco_type_ir_mapper.hpp"
#include "runtime/interop/universal_resolver.hpp"
#include <string>
#include <sstream>
#include <iomanip>
#include <ctime>

/* ════════════════════════════════════════════════════════════
   Universal .au Binding Generator — Phase 5.4
   ════════════════════════════════════════════════════════════
   Generates unified Aurora .au binding files with Type IR
   cost annotations for any ecosystem package.
   ════════════════════════════════════════════════════════════ */

struct BindingGenOptions {
    bool include_cost_annotations{true};
    bool include_marshal_stubs{true};
    bool include_dependency_info{true};
    bool strict_unknown_cost{false}; /* Error on UnknownCost */
};

class UniversalBindingGenerator {
public:
    explicit UniversalBindingGenerator(const EcosystemTypeIRMapper& mapper,
                                        BindingGenOptions opts = {})
        : mapper_(mapper), opts_(opts) {}

    /* ── Generate complete .au binding file for a package ── */
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
        au << "   Generated: " << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << "\n";
        au << "   ════════════════════════════════════════════════════════════ */\n\n";

        /* External FFI block */
        au << "extern \"" << ecosystem_name(pkg.ecosystem) << "_" << pkg.name << "\"\n\n";

        /* Module entry point */
        au << "/* Module initialization */\n";
        switch (pkg.ecosystem) {
            case Ecosystem::PyPI:
                au << "function PyInit_" << pkg.name << "() -> pointer\n\n";
                break;
            case Ecosystem::Npm:
                au << "function napi_register_module_v1(env: pointer, exports: pointer) -> pointer\n\n";
                break;
            case Ecosystem::Cargo:
                au << "function " << pkg.name << "_init() -> pointer\n\n";
                break;
            case Ecosystem::Native:
                au << "function " << pkg.name << "_load() -> pointer\n\n";
                break;
            default: break;
        }

        /* Package metadata */
        au << "/* Package metadata */\n";
        au << "const " << safe_ident(pkg.name) << "_version = \"" << pkg.version << "\"\n";
        if (!pkg.description.empty())
            au << "const " << safe_ident(pkg.name) << "_summary = \"" << pkg.description << "\"\n";
        au << "\n";

        /* Type declarations with cost annotations */
        au << generate_type_declarations(pkg);

        /* Function stubs */
        au << generate_function_stubs(pkg);

        /* Marshal stubs (optional) */
        if (opts_.include_marshal_stubs) {
            au << generate_marshal_stubs(pkg);
        }

        /* Dependency info (optional) */
        if (opts_.include_dependency_info && !pkg.dependencies.empty()) {
            au << "/* Cross-ecosystem dependencies */\n";
            for (auto& dep : pkg.dependencies) {
                au << "// depends: " << ecosystem_name(dep.ecosystem)
                   << ":" << dep.name << "@" << dep.version << "\n";
            }
            au << "\n";
        }

        /* Usage summary */
        au << "/* ── Usage ──\n";
        au << "   To use this package from Aurora:\n";
        au << "     import \"" << pkg.name << "\"\n";
        au << "   \n";
        au << "   This binding maps " << ecosystem_name(pkg.ecosystem)
           << " types to Aurora via C ABI FFI.\n";
        if (opts_.include_cost_annotations)
            au << "   Cost annotations show performance impact of each type crossing.\n";
        au << "*/\n";

        return au.str();
    }

    /* ── Generate Type IR + cost annotations for common types ── */
    std::string generate_type_declarations(const UnifiedPackageInfo& pkg) const {
        std::ostringstream t;
        t << "/* Type declarations (Phase 1 Type IR) */\n";

        /* Generate type declarations based on ecosystem */
        switch (pkg.ecosystem) {
            case Ecosystem::PyPI:
                t << "type Py_int    = i64    @cost(zero)     // Python int → i64 (same ABI)\n";
                t << "type Py_float  = f64    @cost(zero)     // Python float → f64 (same ABI)\n";
                t << "type Py_str    = String @cost(alloc)    // Python str → String (heap alloc)\n";
                t << "type Py_bool   = bool   @cost(zero)     // Python bool → bool\n";
                t << "type Py_bytes  = []u8   @cost(alloc)    // Python bytes → []u8\n";
                t << "type Py_list   = List   @cost(alloc)    // Python list → List (heap alloc)\n";
                t << "type Py_dict   = Map    @cost(alloc)    // Python dict → Map (heap alloc)\n";
                t << "type Py_object = pointer @cost(indirection) // Python object → void*\n";
                break;

            case Ecosystem::Npm:
                t << "type JS_number  = f64   @cost(zero)     // JS number → f64 (same ABI)\n";
                t << "type JS_string  = String @cost(alloc)   // JS string → String (heap alloc)\n";
                t << "type JS_boolean = bool  @cost(zero)     // JS boolean → bool\n";
                t << "type JS_object  = pointer @cost(indirection) // JS object → void*\n";
                t << "type JS_array   = List  @cost(alloc)    // JS Array → List (heap alloc)\n";
                t << "type JS_Buffer  = []u8  @cost(alloc)    // Node Buffer → []u8\n";
                break;

            case Ecosystem::Cargo:
                t << "type Rs_i32 = i32 @cost(zero)       // Rust i32 → i32\n";
                t << "type Rs_i64 = i64 @cost(zero)       // Rust i64 → i64\n";
                t << "type Rs_f64 = f64 @cost(zero)       // Rust f64 → f64\n";
                t << "type Rs_bool = bool @cost(zero)     // Rust bool → bool\n";
                t << "type Rs_String = String @cost(alloc) // Rust String → String (heap alloc)\n";
                t << "type Rs_Vec = List @cost(alloc)     // Rust Vec → List (heap alloc)\n";
                t << "type Rs_Option = Optional @cost(zero) // Rust Option → Optional\n";
                t << "type Rs_Result = Result @cost(zero)  // Rust Result → Result\n";
                break;

            case Ecosystem::Native:
                t << "type Native_int   = i32    @cost(zero)     // C int → i32\n";
                t << "type Native_uint  = u32    @cost(zero)     // unsigned int → u32\n";
                t << "type Native_long  = i64    @cost(zero)     // long → i64\n";
                t << "type Native_float = f32    @cost(zero)     // float → f32\n";
                t << "type Native_double= f64    @cost(zero)     // double → f64\n";
                t << "type Native_ptr   = pointer @cost(indirection) // void* → pointer\n";
                t << "type Native_str   = cstring @cost(indirection) // const char* → cstring\n";
                break;

            default: break;
        }

        t << "\n";
        return t.str();
    }

    /* ── Generate function stubs ── */
    std::string generate_function_stubs(const UnifiedPackageInfo& pkg) const {
        std::ostringstream f;
        f << "/* FFI function signatures */\n";

        switch (pkg.ecosystem) {
            case Ecosystem::PyPI:
                f << "function " << safe_ident(pkg.name) << "_import() -> pointer\n";
                f << "function " << safe_ident(pkg.name) << "_call(fn_name: cstring, args: pointer) -> pointer\n";
                f << "function " << safe_ident(pkg.name) << "_free(handle: pointer)\n";
                break;

            case Ecosystem::Npm:
                f << "function " << safe_ident(pkg.name) << "_require() -> pointer\n";
                f << "function " << safe_ident(pkg.name) << "_invoke(module: pointer, fn: cstring, args: pointer) -> pointer\n";
                break;

            case Ecosystem::Cargo:
                f << "function " << safe_ident(pkg.name) << "_create() -> pointer\n";
                f << "function " << safe_ident(pkg.name) << "_method(obj: pointer, method_id: i32, args: pointer) -> pointer\n";
                f << "function " << safe_ident(pkg.name) << "_drop(obj: pointer)\n";
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

    /* ── Generate marshal stub code ── */
    std::string generate_marshal_stubs(const UnifiedPackageInfo& pkg) const {
        std::ostringstream m;
        m << "/* Marshal stubs (auto-generated by Type IR mapper) */\n";

        /* PyObject marshal */
        if (pkg.ecosystem == Ecosystem::PyPI) {
            MappedType py_str = mapper_.map_to_ir("str", Ecosystem::PyPI);
            MappedType py_int = mapper_.map_to_ir("int", Ecosystem::PyPI);

            m << "// Marshal: Py_str → Aurora String\n";
            m << mapper_.generate_marshal_code(py_str, "py_str_val", true);

            m << "// Marshal: Py_int → Aurora i64\n";
            m << mapper_.generate_marshal_code(py_int, "py_int_val", true);
        }

        /* napi marshal */
        if (pkg.ecosystem == Ecosystem::Npm) {
            MappedType js_str = mapper_.map_to_ir("string", Ecosystem::Npm);
            MappedType js_num = mapper_.map_to_ir("number", Ecosystem::Npm);

            m << "// Marshal: JS_string → Aurora String\n";
            m << mapper_.generate_marshal_code(js_str, "js_str_val", true);

            m << "// Marshal: JS_number → Aurora f64\n";
            m << mapper_.generate_marshal_code(js_num, "js_num_val", true);
        }

        /* Cargo marshal */
        if (pkg.ecosystem == Ecosystem::Cargo) {
            MappedType rs_str = mapper_.map_to_ir("String", Ecosystem::Cargo);
            MappedType rs_i64 = mapper_.map_to_ir("i64", Ecosystem::Cargo);

            m << "// Marshal: Rs_String → Aurora String\n";
            m << mapper_.generate_marshal_code(rs_str, "rs_str_val", true);

            m << "// Marshal: Rs_i64 → Aurora i64\n";
            m << mapper_.generate_marshal_code(rs_i64, "rs_i64_val", true);
        }

        /* Native marshal */
        if (pkg.ecosystem == Ecosystem::Native) {
            MappedType n_int = mapper_.map_to_ir("int", Ecosystem::Native);
            MappedType n_double = mapper_.map_to_ir("double", Ecosystem::Native);

            m << "// Marshal: Native int → Aurora i32\n";
            m << mapper_.generate_marshal_code(n_int, "n_int_val", true);

            m << "// Marshal: Native double → Aurora f64\n";
            m << mapper_.generate_marshal_code(n_double, "n_double_val", true);
        }

        m << "\n";
        return m.str();
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
};
