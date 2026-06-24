#include "bridge_shared.h"

void gen_jvm_au_binding(const std::string& pkg, const JsonValue& json,
                         const std::string& ver, std::ostream& os)
{
    if (pkg.empty()) return;
    std::string safe = pkg;
    for (auto& c : safe) if (c == '-' || c == '.' || c == '/') c = '_';
    std::string cls = pkg;
    if (!cls.empty()) cls[0] = (char)toupper((unsigned char)cls[0]);
    std::string desc = json.type == JsonValue::Null ? "" : json.nested_str({"info", "summary"});
    if (desc.empty()) desc = json.nested_str({"description"});

    os << "/* JVM Bridge — " << pkg << "@" << ver;
    if (!desc.empty()) os << " - " << desc;
    os << " */\n";
    os << "@cost(zero)\n";
    os << "extern \"jvm_" << safe << "\" function JVM_Init_" << safe << "(jvm_path: cstring) -> i32\n";
    os << "@cost(zero)\n";
    os << "extern \"jvm_" << safe << "\" function " << safe << "_find_class(name: cstring) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"jvm_" << safe << "\" function " << safe << "_new_instance(cls: pointer) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"jvm_" << safe << "\" function " << safe << "_call_static_i32(cls: pointer, method: cstring, arg: i32) -> i32\n";
    os << "@cost(alloc)\n";
    os << "extern \"jvm_" << safe << "\" function " << safe << "_call_static_f64(cls: pointer, method: cstring, arg: f64) -> f64\n";
    os << "@cost(alloc)\n";
    os << "extern \"jvm_" << safe << "\" function " << safe << "_call_static_str(cls: pointer, method: cstring, arg: cstring) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"jvm_" << safe << "\" function " << safe << "_call_static_void(cls: pointer, method: cstring) -> i32\n";
    os << "@cost(alloc)\n";
    os << "extern \"jvm_" << safe << "\" function " << safe << "_call_static_ret_str(cls: pointer, method: cstring) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"jvm_" << safe << "\" function " << safe << "_call_i32(obj: pointer, method: cstring) -> i32\n";
    os << "@cost(alloc)\n";
    os << "extern \"jvm_" << safe << "\" function " << safe << "_call_f64(obj: pointer, method: cstring) -> f64\n";
    os << "@cost(alloc)\n";
    os << "extern \"jvm_" << safe << "\" function " << safe << "_call_str(obj: pointer, method: cstring) -> pointer\n";
    os << "@cost(zero)\n";
    os << "extern \"jvm_" << safe << "\" function " << safe << "_get_field_i32(obj: pointer, field: cstring) -> i32\n";
    os << "@cost(zero)\n";
    os << "extern \"jvm_" << safe << "\" function " << safe << "_get_field_f64(obj: pointer, field: cstring) -> f64\n";
    os << "@cost(zero)\n";
    os << "extern \"jvm_" << safe << "\" function " << safe << "_delete_global_ref(obj: pointer) -> i32\n";
    os << "@cost(zero)\n";
    os << "extern \"jvm_" << safe << "\" function " << safe << "_get_last_error() -> cstring\n";
}

void gen_jvm_c_wrapper(const std::string& pkg, const std::string& dir)
{
    (void)pkg;
    (void)dir;
    /* JVM bridge requires JDK's jni.h at compile time.
       Skipping auto-generated C wrapper — the .au binding is sufficient.
       Users with JDK can compile manually:
         gcc -shared -o <pkg>_jvm.dll <pkg>_jvm.c
                -I%JAVA_HOME%/include -I%JAVA_HOME%/include/win32 */
}
