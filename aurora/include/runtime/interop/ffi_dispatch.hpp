#pragma once
#include <string>
#include <type_traits>
#include <stdexcept>
#include <cstdint>

/* ── Forward declare C FFI dispatch functions from ffi.cpp ── */
extern "C" {
void* aurora_dl_open(const char* libname);
void* aurora_dl_sym(void* lib, const char* name);
void* aurora_dl_resolve(const char* libname, const char* name);
void* aurora_ecosystem_resolve(const char* ecs_name, const char* func_name);
void  aurora_dl_close(void* lib);
void  aurora_dl_cleanup_all(void);
const char* aurora_dl_error(void);
void* aurora_dl_try_open(const char* name);
void* aurora_callback_create(void* handler, void* user_data);
void  aurora_callback_destroy(void* trampoline);
}

/* ════════════════════════════════════════════════════════════
   FFI Dispatch — C++ type-safe wrapper around aurora_dl_*
   ════════════════════════════════════════════════════════════
   - LoadLibrary/GetProcAddress (Win) or dlopen/dlsym (POSIX)
   - Type-safe function pointer resolution via template
   - Runtime call trampolines with platform ABI
   - Connects generated .au bindings to native DLL resolution
   ════════════════════════════════════════════════════════════ */

/* ── Low-level FFI library handle ── */
class FFILibrary {
public:
    FFILibrary() = default;

    explicit FFILibrary(const std::string& path) {
        open(path);
    }

    ~FFILibrary() { close(); }
    FFILibrary(FFILibrary&& other) noexcept
        : handle_(other.handle_) { other.handle_ = nullptr; }
    FFILibrary& operator=(FFILibrary&& other) noexcept {
        if (this != &other) { close(); handle_ = other.handle_; other.handle_ = nullptr; }
        return *this;
    }

    FFILibrary(const FFILibrary&) = delete;
    FFILibrary& operator=(const FFILibrary&) = delete;

    bool open(const std::string& path) {
        close();
        handle_ = aurora_dl_try_open(path.c_str());
        return handle_ != nullptr;
    }

    void close() {
        if (handle_) { aurora_dl_close(handle_); handle_ = nullptr; }
    }

    bool is_open() const { return handle_ != nullptr; }

    /* ── Resolve symbol — use resolve_fn<T> for type-safety ── */
    void* sym(const std::string& name) const {
        if (!handle_) return nullptr;
        return aurora_dl_sym(handle_, name.c_str());
    }

    /* ── Type-safe function pointer resolution ── */
    template <typename T>
    T* resolve_fn(const std::string& name) const {
        static_assert(std::is_function_v<T>, "T must be a function type");
        return reinterpret_cast<T*>(sym(name));
    }

    std::string error() const { return aurora_dl_error(); }

private:
    void* handle_{nullptr};
};

/* ── Platform calling convention flags (for codegen) ── */
enum class FFICallingConvention : uint8_t {
    Default,   /* Platform default (cdecl on x86, one convention on x64) */
    CDecl,     /* __cdecl (x86 only) */
    StdCall,   /* __stdcall (x86 only) */
    FastCall,  /* __fastcall (x86 only) */
    ThisCall,  /* __thiscall (x86 only) */
    VectorCall /* __vectorcall */
};

/* ── Resolve calling convention name for codegen ── */
inline const char* ffi_conv_name(FFICallingConvention conv) {
    switch (conv) {
        case FFICallingConvention::CDecl:     return "__cdecl";
        case FFICallingConvention::StdCall:   return "__stdcall";
        case FFICallingConvention::FastCall:  return "__fastcall";
        case FFICallingConvention::ThisCall:  return "__thiscall";
        case FFICallingConvention::VectorCall:return "__vectorcall";
        default:                              return "";
    }
}

/* ── FFI function descriptor — used by codegen to emit calls ── */
struct FFIFunction {
    std::string lib_name;        /* e.g. "python3.dll", "libnode.so" */
    std::string func_name;       /* e.g. "PyImport_ImportModule" */
    std::string return_type;     /* e.g. "pointer", "i64", "f64" */
    std::vector<std::string> param_types;
    FFICallingConvention conv{FFICallingConvention::Default};
    bool is_variadic{false};
};

/* ── FFI module — represents a loaded .au binding at runtime ── */
/* A module can be:
 *   - A native DLL loaded via FFILibrary (for C/C++ libs)
 *   - A Python module bridged via CPython API
 *   - A Node.js module bridged via N-API
 *   - A Rust crate bridged via C ABI
 */

enum class FFIModuleType : uint8_t {
    Native,   /* DLL/SO loaded via LoadLibrary/dlopen */
    PyPI,     /* Python module via embedded CPython */
    NPM,      /* Node.js module via embedded Node.js */
    Cargo     /* Rust crate via C ABI */
};

class FFIModule {
public:
    FFIModule(const std::string& name, FFIModuleType type)
        : name_(name), type_(type) {}

    /* ── Load the module ── */
    bool load() {
        if (loaded_) return true;

        switch (type_) {
            case FFIModuleType::Native:
                return load_native();
            case FFIModuleType::PyPI:
                return load_pypi();
            case FFIModuleType::NPM:
                return load_npm();
            case FFIModuleType::Cargo:
                return load_cargo();
        }
        return false;
    }

    void unload() {
        lib_.close();
        loaded_ = false;
    }

    bool is_loaded() const { return loaded_; }
    const std::string& name() const { return name_; }
    FFIModuleType type() const { return type_; }

    /* ── Call a function in this module ── */
    template <typename Ret, typename... Args>
    Ret call(const std::string& func_name, Args... args) const {
        auto* fn = lib_.resolve_fn<Ret(Args...)>(func_name);
        if (!fn) {
            throw std::runtime_error("FFI: symbol '" + func_name +
                                     "' not found in " + name_);
        }
        return fn(args...);
    }

    /* ── Check if a symbol exists ── */
    bool has(const std::string& name) const {
        return lib_.sym(name) != nullptr;
    }

    std::string error() const { return lib_.error(); }

private:
    std::string name_;
    FFIModuleType type_{FFIModuleType::Native};
    FFILibrary lib_;
    bool loaded_{false};

    bool load_native() {
        /* For Native type, the name is the DLL path */
        loaded_ = lib_.open(name_);
        return loaded_;
    }

    bool load_pypi() {
        /* PyPI: load the embedded CPython DLL, then import the module
         * Python DLL is usually python3XX.dll or python3.dll */
        loaded_ = lib_.open("python3.dll");
        if (!loaded_) loaded_ = lib_.open("python3*.dll");
        return loaded_;
    }

    bool load_npm() {
        /* npm: load Node.js DLL (node.dll on Windows) */
        loaded_ = lib_.open("node.dll");
        if (!loaded_) loaded_ = lib_.open("libnode.so");
        return loaded_;
    }

    bool load_cargo() {
        /* Cargo: load the compiled .dll/.so from target/release */
        loaded_ = lib_.open(name_);
        return loaded_;
    }
};

/* ── Convenience: create an FFI module from a .au file path ── */
inline FFIModule load_binding(const std::string& au_path,
                               const std::string& ecosystem = "native")
{
    /* Map ecosystem string to module type */
    FFIModuleType mt = FFIModuleType::Native;
    if (ecosystem == "pypi")      mt = FFIModuleType::PyPI;
    else if (ecosystem == "npm")  mt = FFIModuleType::NPM;
    else if (ecosystem == "cargo")mt = FFIModuleType::Cargo;

    /* Derive library path from .au filename:
     *   numpy.au → lib/numpy.dll (or the ecosystem bridge dir) */
    std::string lib_path = au_path;
    auto dot = lib_path.rfind('.');
    if (dot != std::string::npos) lib_path = lib_path.substr(0, dot);

#ifdef _WIN32
    lib_path += ".dll";
#elif __APPLE__
    lib_path += ".dylib";
#else
    lib_path += ".so";
#endif

    FFIModule mod(lib_path, mt);
    if (!mod.load()) {
        /* Fallback: try ecosystem bridge naming */
        lib_path = au_path;
        if (dot != std::string::npos) lib_path = lib_path.substr(0, dot);
        lib_path += "_" + ecosystem;
#ifdef _WIN32
        lib_path += ".dll";
#elif __APPLE__
        lib_path += ".dylib";
#else
        lib_path += ".so";
#endif
        mod = FFIModule(lib_path, mt);
        mod.load();
    }
    return mod;
}
