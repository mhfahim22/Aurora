#include "bridge_shared.h"

void gen_go_au_binding(const std::string& pkg, const JsonValue& json,
                        const std::string& ver, std::ostream& os)
{
    if (pkg.empty()) return;
    std::string safe = pkg;
    for (auto& c : safe) if (c == '-' || c == '.' || c == '/') c = '_';
    std::string desc = json.type == JsonValue::Null ? "" : json.nested_str({"info", "summary"});
    if (desc.empty()) desc = json.nested_str({"description"});

    os << "/* ════════════════════════════════════════════════════════════\n";
    os << "   Go Bridge — Auto-generated Aurora FFI Bindings\n";
    os << "   Package: " << pkg << "@" << ver << "\n";
    os << "   Go module: " << pkg << "\n";
    os << "   ════════════════════════════════════════════════════════════ */\n\n";
    os << "/* Init — load Go shared library */\n";
    os << "@cost(zero)\n";
    os << "extern \"go_" << safe << "\" function Go_Init_" << safe << "(so_path: cstring) -> i32\n\n";
    os << "/* Call exported Go functions */\n";
    os << "@cost(alloc)\n";
    os << "extern \"go_" << safe << "\" function " << safe << "_call_i32(fn: cstring, arg: i32) -> i32\n";
    os << "@cost(alloc)\n";
    os << "extern \"go_" << safe << "\" function " << safe << "_call_f64(fn: cstring, arg: f64) -> f64\n";
    os << "@cost(alloc)\n";
    os << "extern \"go_" << safe << "\" function " << safe << "_call_str(fn: cstring, arg: cstring) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"go_" << safe << "\" function " << safe << "_call_void(fn: cstring) -> i32\n";
    os << "@cost(alloc)\n";
    os << "extern \"go_" << safe << "\" function " << safe << "_call_ret_str(fn: cstring) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"go_" << safe << "\" function " << safe << "_call_i32_ret_i32(fn: cstring, a: i32) -> i32\n\n";
    os << "/* Memory management */\n";
    os << "@cost(zero)\n";
    os << "extern \"go_" << safe << "\" function " << safe << "_free_str(s: pointer) -> i32\n\n";
    os << "/* Diagnostics */\n";
    os << "@cost(zero)\n";
    os << "extern \"go_" << safe << "\" function " << safe << "_get_last_error() -> cstring\n\n";
    os << "/* Usage:\n";
    os << "     import \"" << pkg << "\"\n";
    os << "     let rc = Go_Init_" << safe << "(\"path/to/gobridge.so\")\n";
    os << "     if rc == 0 then\n";
    os << "         let result = " << safe << "_call_ret_str(\"ExportedFunction\")\n";
    os << "         let val = " << safe << "_call_i32(\"Add\", 42)\n";
    os << "     end\n";
    os << "*/\n";
}

void gen_go_c_wrapper(const std::string& pkg, const std::string& dir)
{
    std::string safe = pkg;
    for (auto& c : safe) if (c == '-' || c == '.' || c == '/') c = '_';

    std::ostringstream cw;
    cw << "/* Auto-generated Go bridge DLL for " << pkg << " */\n";
#ifdef _WIN32
    cw << "#define WIN32_LEAN_AND_MEAN\n#include <windows.h>\n#include <stdio.h>\n#include <string.h>\n#include <stdlib.h>\n#include <stdint.h>\n#define EXPORT __declspec(dllexport)\n#define DLOPEN LoadLibraryA\n#define DLSYM GetProcAddress\n#define DLCLOSE FreeLibrary\n#define SO_EXT \".dll\"\n";
#else
    cw << "#include <dlfcn.h>\n#include <stdio.h>\n#include <string.h>\n#include <stdlib.h>\n#include <stdint.h>\n#define EXPORT __attribute__((visibility(\"default\")))\n#define DLOPEN dlopen\n#define DLSYM dlsym\n#define DLCLOSE dlclose\n#define SO_EXT \".so\"\n";
#endif
    cw << "\nstatic void* g_so = NULL;\nstatic int g_inited = 0;\nstatic char g_last_error[4096] = \"\";\n";
    cw << "static void set_last_error(const char* m){strncpy(g_last_error,m,sizeof(g_last_error)-1);g_last_error[sizeof(g_last_error)-1]=0;}\n\n";
    cw << "EXPORT int " << safe << "_init(const char* so_path) {\n";
    cw << "    if (g_inited) return 0;\n";
    cw << "    g_so = (void*)DLOPEN(so_path);\n";
    cw << "    if (!g_so) { set_last_error(\"Failed to load Go shared library\"); return -1; }\n";
    cw << "    g_inited = 1;\n    return 0;\n}\n\n";
    cw << "static void* get_fn(const char* name) {\n";
    cw << "    if (!g_so) { set_last_error(\"Go library not loaded\"); return NULL; }\n";
    cw << "    void* fn = (void*)DLSYM(g_so, name);\n";
    cw << "    if (!fn) { set_last_error(\"Symbol not found\"); return NULL; }\n";
    cw << "    return fn;\n}\n\n";
    cw << "typedef int (*fn_i32_i32_t)(int);\n";
    cw << "typedef double (*fn_f64_f64_t)(double);\n";
    cw << "typedef char* (*fn_cstr_cstr_t)(const char*);\n";
    cw << "typedef int (*fn_void_i32_t)();\n";
    cw << "typedef char* (*fn_void_cstr_t)();\n";
    cw << "typedef void (*fn_cstr_void_t)(char*);\n\n";
    cw << "EXPORT int " << safe << "_call_i32(const char* fn, int arg) {\n";
    cw << "    fn_i32_i32_t f = (fn_i32_i32_t)get_fn(fn);\n";
    cw << "    if (!f) return 0;\n    return f(arg);\n}\n\n";
    cw << "EXPORT double " << safe << "_call_f64(const char* fn, double arg) {\n";
    cw << "    fn_f64_f64_t f = (fn_f64_f64_t)get_fn(fn);\n";
    cw << "    if (!f) return 0.0;\n    return f(arg);\n}\n\n";
    cw << "EXPORT char* " << safe << "_call_str(const char* fn, const char* arg) {\n";
    cw << "    fn_cstr_cstr_t f = (fn_cstr_cstr_t)get_fn(fn);\n";
    cw << "    if (!f) return NULL;\n    return f(arg);\n}\n\n";
    cw << "EXPORT int " << safe << "_call_void(const char* fn) {\n";
    cw << "    fn_void_i32_t f = (fn_void_i32_t)get_fn(fn);\n";
    cw << "    if (!f) return -1;\n    return f();\n}\n\n";
    cw << "EXPORT char* " << safe << "_call_ret_str(const char* fn) {\n";
    cw << "    fn_void_cstr_t f = (fn_void_cstr_t)get_fn(fn);\n";
    cw << "    if (!f) return NULL;\n    return f();\n}\n\n";
    cw << "EXPORT int " << safe << "_call_i32_ret_i32(const char* fn, int a) {\n";
    cw << "    fn_i32_i32_t f = (fn_i32_i32_t)get_fn(fn);\n";
    cw << "    if (!f) return 0;\n    return f(a);\n}\n\n";
    cw << "EXPORT int " << safe << "_free_str(char* s) {\n";
    cw << "    if (s) free(s);\n    return 0;\n}\n\n";
    cw << "EXPORT char* " << safe << "_get_last_error(void) { return g_last_error; }\n";

    std::string wrapper_path = dir + "/" + pkg + "_go.c";
    write_file(wrapper_path, cw.str());
}
