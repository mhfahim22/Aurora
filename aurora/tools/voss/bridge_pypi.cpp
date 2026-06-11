#include "bridge_shared.h"

/* Known PyPI package name → Python import name aliases.
   Some PyPI packages have different import names from their package name. */

const char* pypi_import_alias(const std::string& pkg) {
    static const struct { const char* pypi; const char* imp; } aliases[] = {
        {"Pillow", "PIL"},
        {"opencv-python", "cv2"},
        {"opencv-contrib-python", "cv2"},
        {"scikit-learn", "sklearn"},
        {"scikit-image", "skimage"},
        {"beautifulsoup4", "bs4"},
        {"yfinance", "yfinance"},
        {"python-dateutil", "dateutil"},
        {"pyyaml", "yaml"},
        {"python-dotenv", "dotenv"},
        {"python-multipart", "multipart"},
        {"PyGObject", "gi"},
        {"google-cloud-storage", "google.cloud.storage"},
        {"google-cloud-bigquery", "google.cloud.bigquery"},
        {NULL, NULL}
    };
    for (int i = 0; aliases[i].pypi; i++) {
        if (pkg == aliases[i].pypi) return aliases[i].imp;
    }
    return pkg.c_str();
}

void gen_pypi_au_binding(const std::string& pkg, const JsonValue& json,
                          const std::string& ver, std::ostream& os)
{
    std::string safe = pkg;
    for (auto& c : safe) if (c == '-') c = '_';
    std::string desc = json.nested_str({"info", "summary"});

    os << "/* ════════════════════════════════════════════════════════════\n";
    os << "   PyPI Bridge — Auto-generated Aurora FFI Bindings\n";
    os << "   Package: " << pkg << "@" << ver << "\n";
    os << "   Python import: " << pypi_import_alias(pkg) << "\n";
    os << "   ════════════════════════════════════════════════════════════ */\n\n";
    os << "/* Module init */\n";
    os << "@cost(zero)\n";
    os << "extern \"pypi_" << safe << "\" function PyInit_" << safe << "() -> pointer\n\n";
    os << "/* FFI entry points */\n";
    os << "@cost(zero)\n";
    os << "extern \"pypi_" << safe << "\" function " << safe << "_import() -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"pypi_" << safe << "\" function " << safe << "_call(mod: pointer, fn: cstring, args: pointer) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"pypi_" << safe << "\" function " << safe << "_call1(mod: pointer, fn: cstring, arg: pointer) -> pointer\n";
    os << "@cost(zero)\n";
    os << "extern \"pypi_" << safe << "\" function " << safe << "_free(handle: pointer)\n\n";
    os << "/* Value conversion helpers */\n";
    os << "@cost(alloc)\n";
    os << "extern \"pypi_" << safe << "\" function " << safe << "_str(s: cstring) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"pypi_" << safe << "\" function " << safe << "_int(v: i64) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"pypi_" << safe << "\" function " << safe << "_float(v: f64) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"pypi_" << safe << "\" function " << safe << "_call_kw(mod: pointer, fn: cstring, args: pointer, kwargs: pointer) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"pypi_" << safe << "\" function " << safe << "_getattr(obj: pointer, name: cstring) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"pypi_" << safe << "\" function " << safe << "_call2(mod: pointer, fn: cstring, arg1: pointer, arg2: pointer) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"pypi_" << safe << "\" function " << safe << "_call3(mod: pointer, fn: cstring, arg1: pointer, arg2: pointer, arg3: pointer) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"pypi_" << safe << "\" function " << safe << "_call4(mod: pointer, fn: cstring, a1: pointer, a2: pointer, a3: pointer, a4: pointer) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"pypi_" << safe << "\" function " << safe << "_call5(mod: pointer, fn: cstring, a1: pointer, a2: pointer, a3: pointer, a4: pointer, a5: pointer) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"pypi_" << safe << "\" function " << safe << "_call6(mod: pointer, fn: cstring, a1: pointer, a2: pointer, a3: pointer, a4: pointer, a5: pointer, a6: pointer) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"pypi_" << safe << "\" function " << safe << "_list(items: pointer, count: i32) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"pypi_" << safe << "\" function " << safe << "_tuple(items: pointer, count: i32) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"pypi_" << safe << "\" function " << safe << "_tuple2(a: pointer, b: pointer) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"pypi_" << safe << "\" function " << safe << "_tuple3(a: pointer, b: pointer, c: pointer) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"pypi_" << safe << "\" function " << safe << "_tuple4(a: pointer, b: pointer, c: pointer, d: pointer) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"pypi_" << safe << "\" function " << safe << "_list2(a: pointer, b: pointer) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"pypi_" << safe << "\" function " << safe << "_list3(a: pointer, b: pointer, c: pointer) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"pypi_" << safe << "\" function " << safe << "_list4(a: pointer, b: pointer, c: pointer, d: pointer) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"pypi_" << safe << "\" function " << safe << "_to_cstr(obj: pointer) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"pypi_" << safe << "\" function " << safe << "_dict() -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"pypi_" << safe << "\" function " << safe << "_dict_set(d: pointer, key: cstring, val: pointer) -> i32\n";
    os << "/* Diagnostics */\n";
    os << "@cost(zero)\n";
    os << "extern \"pypi_" << safe << "\" function " << safe << "_get_perf_stats() -> cstring\n";
    os << "@cost(zero)\n";
    os << "extern \"pypi_" << safe << "\" function " << safe << "_get_last_error() -> cstring\n\n";
    os << "/* Usage:\n";
    os << "     import \"" << pkg << "\"\n";
    os << "     let py_mod = " << safe << "_import()\n";
    os << "     let args = " << safe << "_tuple(0, 0)    // no-arg tuple\n";
    os << "     let result = " << safe << "_call(py_mod, \"func_name\", args)\n";
    os << "     -- Multi-arg: call2/3/4/5/6(mod, \"fn\", a1, a2, ...)\n";
    os << "     -- Chaining:  let obj = " << safe << "_getattr(mod, \"method\")\n";
    os << "     -- Keywords:  " << safe << "_call_kw(mod, \"fn\", args, kwargs)\n";
    os << "     " << safe << "_free(result)\n";
    os << "     " << safe << "_free(py_mod)\n";
    os << "*/\n";
}
