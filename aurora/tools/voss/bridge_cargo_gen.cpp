#include "bridge_cargo_impl.hpp"
#include "bridge_shared.h"
#include <set>
#include <fstream>
/* ── gen_cargo_au_binding ─────────────────────────────────────
   Generates the .au binding file for a Cargo crate, exposing
   standard FFI entry points plus type-instance method entries.
   ────────────────────────────────────────────────────────── */
void gen_cargo_au_binding(const std::string& pkg, const JsonValue& /*json*/,
                           const std::string& ver, std::ostream& os,
                           const std::string& extra_au) {
    os << "/* " << std::string(50, '=') << "\n"
       << "   Cargo Bridge " << char(151) << " Auto-generated Aurora FFI Bindings\n"
       << "   Package: " << pkg << "@" << ver << "\n"
       << "   " << std::string(50, '=') << " */\n\n"
       << "/* Module init */\n"
       << "@cost(zero)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_import() -> pointer\n\n"
       << "/* FFI entry points */\n"
       << "@cost(zero)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_call(mod: pointer, fn: cstring, args: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_call1(mod: pointer, fn: cstring, arg: pointer) -> pointer\n"
       << "@cost(zero)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_free(handle: pointer)\n"
       << "@cost(zero)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_free_cstr(handle: pointer)\n\n"
       << "/* Value conversion helpers */\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_str(s: cstring) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_int(v: i64) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_float(v: f64) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_call_kw(mod: pointer, fn: cstring, args: pointer, kwargs: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_getattr(obj: pointer, name: cstring) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_call2(mod: pointer, fn: cstring, arg1: pointer, arg2: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_call3(mod: pointer, fn: cstring, arg1: pointer, arg2: pointer, arg3: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_call4(mod: pointer, fn: cstring, a1: pointer, a2: pointer, a3: pointer, a4: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_call5(mod: pointer, fn: cstring, a1: pointer, a2: pointer, a3: pointer, a4: pointer, a5: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_call6(mod: pointer, fn: cstring, a1: pointer, a2: pointer, a3: pointer, a4: pointer, a5: pointer, a6: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_list(items: pointer, count: i32) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_tuple(items: pointer, count: i32) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_tuple2(a: pointer, b: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_tuple3(a: pointer, b: pointer, c: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_tuple4(a: pointer, b: pointer, c: pointer, d: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_list2(a: pointer, b: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_list3(a: pointer, b: pointer, c: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_list4(a: pointer, b: pointer, c: pointer, d: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_to_cstr(obj: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_dict() -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_dict_set(d: pointer, key: cstring, val: pointer) -> i32\n\n"
       << "/* Type instance API " << char(151) << " construct, call method, destroy */\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_construct(type_name: cstring, ctor: cstring, args: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_call_method(handle: pointer, type_name: cstring, method: cstring, args: pointer) -> pointer\n"
       << "@cost(zero)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_free_type(handle: pointer, type_name: cstring)\n"
       << "@cost(zero)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_last_error() -> cstring\n\n";

    if (!extra_au.empty()) {
        os << "/* Type instance entries (auto-discovered) */\n"
           << extra_au << "\n";
    }

    os << "/* Usage:\n"
       << "     import \"" << pkg << "\"\n"
       << "     let mod = " << pkg << "_import()\n"
       << "     let args = " << pkg << "_tuple2(a, b)\n"
       << "     let result = " << pkg << "_call(mod, \"fn_name\", args)\n"
       << "     " << pkg << "_free(result)\n"
       << "*/\n";
}
void gen_cargo_rust_wrapper(const std::string& pkg, const std::string& ver,
                             const std::string& dir,
                             const CargoDiscovery& disc)
{
    /* Cargo.toml */
    {
        std::ostringstream toml;
        toml << "[package]\n";
        toml << "name = \"" << pkg << "_bridge\"\n";
        toml << "version = \"0.1.0\"\n";
        toml << "edition = \"2021\"\n\n";
        toml << "[lib]\n";
        toml << "crate-type = [\"cdylib\"]\n\n";
        toml << "[dependencies]\n";
        toml << "serde = \"1\"\n";
        if (pkg != "serde_json") {
            toml << "serde_json = \"1\"\n";
        }
        toml << "futures = \"0.3\"\n";
        if (!disc.required_features.empty()) {
            toml << pkg << " = { version = \"" << ver << "\", features = [";
            for (size_t i = 0; i < disc.required_features.size(); i++) {
                if (i > 0) toml << ", ";
                toml << "\"" << disc.required_features[i] << "\"";
            }
            toml << "] }\n";
            std::cout << "[bridge]   features: ";
            for (size_t i = 0; i < disc.required_features.size(); i++) {
                if (i > 0) std::cout << ", ";
                std::cout << disc.required_features[i];
            }
            std::cout << "\n";
        } else {
            toml << pkg << " = \"" << ver << "\"\n";
        }
        if (write_file(dir + "/Cargo.toml", toml.str()))
            std::cout << "[bridge]   " << dir << "/Cargo.toml\n";
    }

    /* src/lib.rs */
    {
        fs::create_directories(dir + "/src");
        std::ostringstream rs;

        /***** HEADER *****/
        rs << "// Auto-generated Cargo bridge for " << pkg << "@" << ver << "\n";
        rs << "// Provides free-function + type-instance FFI API\n\n";
        rs << "use std::ffi::{CStr, CString, c_void};\n";
        rs << "use std::collections::HashMap;\n";
        rs << "use std::sync::Mutex;\n";
        rs << "use std::cell::RefCell;\n\n";

        /***** LAST ERROR (thread-local) *****/
        rs << "thread_local! {\n";
        rs << "    static LAST_ERROR: RefCell<String> = const { RefCell::new(String::new()) };\n";
        rs << "}\n\n";
        rs << "fn set_last_error(s: String) {\n";
        rs << "    LAST_ERROR.with(|e| *e.borrow_mut() = s);\n";
        rs << "}\n\n";

        /***** HELPERS *****/
        rs << "fn __deser<T: serde::de::DeserializeOwned>(v: serde_json::Value) -> Result<T, String> {\n";
        rs << "    serde_json::from_value(v).map_err(|e| e.to_string())\n";
        rs << "}\n\n";



        /***** TYPE ALIASES *****/
        rs << "type RustFn = fn(Vec<serde_json::Value>) -> Result<serde_json::Value, String>;\n";
        rs << "type MethodFn = fn(*mut c_void, Vec<serde_json::Value>) -> Result<serde_json::Value, String>;\n";
        rs << "type CtorFn = fn(Vec<serde_json::Value>) -> Result<*mut c_void, String>;\n";
        rs << "type DropFn = unsafe fn(*mut c_void);\n\n";

        /***** FREE-FUNCTION REGISTRY *****/
        rs << "fn registry() -> &'static Mutex<HashMap<String, RustFn>> {\n";
        rs << "    static REG: std::sync::OnceLock<Mutex<HashMap<String, RustFn>>> = std::sync::OnceLock::new();\n";
        rs << "    REG.get_or_init(|| {\n";
        if (!disc.registry_entries.empty()) {
            rs << "        let mut m: HashMap<String, RustFn> = HashMap::new();\n";
            rs << disc.registry_entries << "\n";
        } else {
            rs << "        let m: HashMap<String, RustFn> = HashMap::new();\n";
        }
        rs << "        Mutex::new(m)\n";
        rs << "    })\n";
        rs << "}\n\n";

        /***** TYPE METHOD REGISTRY *****/
        rs << "fn type_registry() -> &'static Mutex<HashMap<String, HashMap<String, MethodFn>>> {\n";
        rs << "    static REG: std::sync::OnceLock<Mutex<HashMap<String, HashMap<String, MethodFn>>>> = std::sync::OnceLock::new();\n";
        rs << "    REG.get_or_init(|| {\n";
        if (!disc.method_registry_init.empty()) {
            rs << "        let mut type_map: HashMap<String, HashMap<String, MethodFn>> = HashMap::new();\n";
            rs << disc.method_registry_init << "\n";
        } else {
            rs << "        let type_map: HashMap<String, HashMap<String, MethodFn>> = HashMap::new();\n";
        }
        rs << "        Mutex::new(type_map)\n";
        rs << "    })\n";
        rs << "}\n\n";

        /***** CONSTRUCTOR REGISTRY *****/
        rs << "fn ctor_registry() -> &'static Mutex<HashMap<String, HashMap<String, CtorFn>>> {\n";
        rs << "    static REG: std::sync::OnceLock<Mutex<HashMap<String, HashMap<String, CtorFn>>>> = std::sync::OnceLock::new();\n";
        rs << "    REG.get_or_init(|| {\n";
        if (!disc.ctor_registry_init.empty()) {
            rs << "        let mut ctor_map: HashMap<String, HashMap<String, CtorFn>> = HashMap::new();\n";
            rs << disc.ctor_registry_init;
        } else {
            rs << "        let ctor_map: HashMap<String, HashMap<String, CtorFn>> = HashMap::new();\n";
        }
        rs << "        Mutex::new(ctor_map)\n";
        rs << "    })\n";
        rs << "}\n\n";

        /***** DROP REGISTRY *****/
        rs << "fn drop_registry() -> &'static Mutex<HashMap<String, DropFn>> {\n";
        rs << "    static REG: std::sync::OnceLock<Mutex<HashMap<String, DropFn>>> = std::sync::OnceLock::new();\n";
        rs << "    REG.get_or_init(|| {\n";
        if (!disc.drop_registry_init.empty()) {
            rs << "        let mut m: HashMap<String, DropFn> = HashMap::new();\n";
            rs << disc.drop_registry_init;
        } else {
            rs << "        let m: HashMap<String, DropFn> = HashMap::new();\n";
        }
        rs << "        Mutex::new(m)\n";
        rs << "    })\n";
        rs << "}\n\n";

        /***** VALUE HELPERS *****/
        rs << "fn store(v: serde_json::Value) -> *mut c_void {\n";
        rs << "    Box::into_raw(Box::new(v)) as *mut c_void\n";
        rs << "}\n\n";
        rs << "unsafe fn retrieve<'a>(ptr: *mut c_void) -> &'a mut serde_json::Value {\n";
        rs << "    &mut *(ptr as *mut serde_json::Value)\n";
        rs << "}\n\n";

        /***** rust_bridge_get_fns — C-discoverable function registry *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn rust_bridge_get_fns() -> *mut HashMap<String, RustFn> {\n";
        rs << "    Box::into_raw(Box::new(registry().lock().unwrap().clone()))\n";
        rs << "}\n\n";
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn rust_bridge_type_registry() -> *mut HashMap<String, HashMap<String, MethodFn>> {\n";
        rs << "    Box::into_raw(Box::new(type_registry().lock().unwrap().clone()))\n";
        rs << "}\n\n";

        /***** _import *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_import() -> *mut c_void {\n";
        rs << "    registry(); // ensure initialized\n";
        rs << "    store(serde_json::Value::Object(serde_json::Map::new()))\n";
        rs << "}\n\n";

        /***** _free (Value handle) *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_free(ptr: *mut c_void) {\n";
        rs << "    if !ptr.is_null() {\n";
        rs << "        unsafe { drop(Box::from_raw(ptr as *mut serde_json::Value)); }\n";
        rs << "    }\n";
        rs << "}\n\n";

        /***** _free_cstr *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_free_cstr(ptr: *mut std::ffi::c_char) {\n";
        rs << "    if !ptr.is_null() {\n";
        rs << "        unsafe { drop(CString::from_raw(ptr)); }\n";
        rs << "    }\n";
        rs << "}\n\n";

        /***** VALUE CONVERTERS *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_str(s: *const std::ffi::c_char) -> *mut c_void {\n";
        rs << "    let s = unsafe { CStr::from_ptr(s) }.to_string_lossy().to_string();\n";
        rs << "    store(serde_json::Value::String(s))\n";
        rs << "}\n\n";

        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_int(v: i64) -> *mut c_void {\n";
        rs << "    store(serde_json::Value::Number(v.into()))\n";
        rs << "}\n\n";

        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_float(v: f64) -> *mut c_void {\n";
        rs << "    store(serde_json::Value::Number(\n";
        rs << "        serde_json::Number::from_f64(v).unwrap_or(serde_json::Number::from(0))))\n";
        rs << "}\n\n";

        /***** _tuple *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_tuple(items: *mut *mut c_void, count: i32) -> *mut c_void {\n";
        rs << "    let mut vec = Vec::with_capacity(count as usize);\n";
        rs << "    for i in 0..count {\n";
        rs << "        let ptr = unsafe { *items.offset(i as isize) };\n";
        rs << "        if ptr.is_null() {\n";
        rs << "            vec.push(serde_json::Value::Null);\n";
        rs << "        } else {\n";
        rs << "            vec.push(unsafe { retrieve(ptr) }.clone());\n";
        rs << "        }\n";
        rs << "    }\n";
        rs << "    store(serde_json::Value::Array(vec))\n";
        rs << "}\n\n";

        /***** _list *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_list(items: *mut *mut c_void, count: i32) -> *mut c_void {\n";
        rs << "    let mut vec = Vec::with_capacity(count as usize);\n";
        rs << "    for i in 0..count {\n";
        rs << "        let ptr = unsafe { *items.offset(i as isize) };\n";
        rs << "        if ptr.is_null() {\n";
        rs << "            vec.push(serde_json::Value::Null);\n";
        rs << "        } else {\n";
        rs << "            vec.push(unsafe { retrieve(ptr) }.clone());\n";
        rs << "        }\n";
        rs << "    }\n";
        rs << "    store(serde_json::Value::Array(vec))\n";
        rs << "}\n\n";

        /***** _list2.._list4, _tuple2.._tuple4 *****/
        auto gen_tuple_like = [&](const std::string& name, int n, bool use_list) {
            rs << "#[no_mangle]\n";
            rs << "pub extern \"C\" fn " << pkg << "_" << name << "(";
            std::string args_list = "abcdef";
            for (int i = 0; i < n; i++) {
                if (i > 0) rs << ", ";
                rs << args_list[i] << ": *mut c_void";
            }
            rs << ") -> *mut c_void {\n";
            rs << "    store(serde_json::Value::Array(vec![\n";
            for (int i = 0; i < n; i++) {
                rs << "        if " << args_list[i] << ".is_null() { serde_json::Value::Null } else { unsafe { retrieve(" << args_list[i] << ") }.clone() },\n";
            }
            rs << "    ]))\n";
            rs << "}\n\n";
        };
        gen_tuple_like("tuple2", 2, false);
        gen_tuple_like("tuple3", 3, false);
        gen_tuple_like("tuple4", 4, false);
        gen_tuple_like("list2", 2, true);
        gen_tuple_like("list3", 3, true);
        gen_tuple_like("list4", 4, true);

        /***** _dict *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_dict() -> *mut c_void {\n";
        rs << "    store(serde_json::Value::Object(serde_json::Map::new()))\n";
        rs << "}\n\n";

        /***** _dict_set *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_dict_set(d: *mut c_void, key: *const std::ffi::c_char, val: *mut c_void) -> i32 {\n";
        rs << "    if d.is_null() || key.is_null() { return -1; }\n";
        rs << "    let k = match unsafe { CStr::from_ptr(key) }.to_str() {\n";
        rs << "        Ok(s) => s.to_string(),\n";
        rs << "        Err(_) => return -1,\n";
        rs << "    };\n";
        rs << "    let v = if val.is_null() { serde_json::Value::Null } else { unsafe { retrieve(val) }.clone() };\n";
        rs << "    match unsafe { retrieve(d) } {\n";
        rs << "        serde_json::Value::Object(map) => { map.insert(k, v); 0 },\n";
        rs << "        _ => -1,\n";
        rs << "    }\n";
        rs << "}\n\n";

        /***** _to_cstr *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_to_cstr(obj: *mut c_void) -> *mut std::ffi::c_char {\n";
        rs << "    if obj.is_null() { return std::ptr::null_mut(); }\n";
        rs << "    let val = unsafe { retrieve(obj) };\n";
        rs << "    let s = serde_json::to_string(val).unwrap_or_else(|_| \"null\".to_string());\n";
        rs << "    CString::new(s).unwrap_or_default().into_raw()\n";
        rs << "}\n\n";

        /***** _getattr *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_getattr(obj: *mut c_void, name: *const std::ffi::c_char) -> *mut c_void {\n";
        rs << "    if obj.is_null() || name.is_null() { return std::ptr::null_mut(); }\n";
        rs << "    let key = match unsafe { CStr::from_ptr(name) }.to_str() { Ok(s) => s, Err(_) => return std::ptr::null_mut() };\n";
        rs << "    match unsafe { retrieve(obj) } {\n";
        rs << "        serde_json::Value::Object(map) => match map.get(key) {\n";
        rs << "            Some(v) => store(v.clone()),\n";
        rs << "            None => std::ptr::null_mut(),\n";
        rs << "        },\n";
        rs << "        _ => std::ptr::null_mut(),\n";
        rs << "    }\n";
        rs << "}\n\n";

        /***** _call (with error propagation) *****/
        auto gen_call_fn = [&](const std::string& name, const std::string& args_decl,
                                const std::string& args_vec_build) {
            rs << "#[no_mangle]\n";
            rs << "pub extern \"C\" fn " << pkg << "_" << name << "(\n";
            rs << "    _handle: *mut c_void,\n";
            rs << "    fn_name: *const std::ffi::c_char,\n";
            rs << args_decl;
            rs << ") -> *mut c_void {\n";
            rs << "    let name = match unsafe { CStr::from_ptr(fn_name) }.to_str() {\n";
            rs << "        Ok(s) => s,\n";
            rs << "        Err(_) => return std::ptr::null_mut(),\n";
            rs << "    };\n";
            rs << "    let args_vec = " << args_vec_build << ";\n";
            rs << "    let reg = registry().lock().unwrap();\n";
            rs << "    match reg.get(name) {\n";
            rs << "        Some(f) => match f(args_vec) {\n";
            rs << "            Ok(v) => store(v),\n";
            rs << "            Err(e) => { set_last_error(e); std::ptr::null_mut() }\n";
            rs << "        },\n";
            rs << "        None => { set_last_error(format!(\"unknown function: {}\", name)); std::ptr::null_mut() }\n";
            rs << "    }\n";
            rs << "}\n\n";
        };

        gen_call_fn("call",
            "    args: *mut c_void,\n",
            "if args.is_null() { vec![] } else { match unsafe { retrieve(args) } { serde_json::Value::Array(a) => a.clone(), _ => { set_last_error(\"args not array\".into()); return std::ptr::null_mut(); } } }");

        gen_call_fn("call1",
            "    a: *mut c_void,\n",
            "vec![if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() }]");

        gen_call_fn("call2",
            "    a: *mut c_void, b: *mut c_void,\n",
            "vec![if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() }, if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() }]");

        gen_call_fn("call3",
            "    a: *mut c_void, b: *mut c_void, c: *mut c_void,\n",
            "vec![if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() }, if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() }, if c.is_null() { serde_json::Value::Null } else { unsafe { retrieve(c) }.clone() }]");

        gen_call_fn("call4",
            "    a: *mut c_void, b: *mut c_void, c: *mut c_void, d: *mut c_void,\n",
            "vec![if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() }, if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() }, if c.is_null() { serde_json::Value::Null } else { unsafe { retrieve(c) }.clone() }, if d.is_null() { serde_json::Value::Null } else { unsafe { retrieve(d) }.clone() }]");

        gen_call_fn("call5",
            "    a: *mut c_void, b: *mut c_void, c: *mut c_void, d: *mut c_void, e: *mut c_void,\n",
            "vec![if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() }, if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() }, if c.is_null() { serde_json::Value::Null } else { unsafe { retrieve(c) }.clone() }, if d.is_null() { serde_json::Value::Null } else { unsafe { retrieve(d) }.clone() }, if e.is_null() { serde_json::Value::Null } else { unsafe { retrieve(e) }.clone() }]");

        gen_call_fn("call6",
            "    a: *mut c_void, b: *mut c_void, c: *mut c_void, d: *mut c_void, e: *mut c_void, f: *mut c_void,\n",
            "vec![if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() }, if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() }, if c.is_null() { serde_json::Value::Null } else { unsafe { retrieve(c) }.clone() }, if d.is_null() { serde_json::Value::Null } else { unsafe { retrieve(d) }.clone() }, if e.is_null() { serde_json::Value::Null } else { unsafe { retrieve(e) }.clone() }, if f.is_null() { serde_json::Value::Null } else { unsafe { retrieve(f) }.clone() }]");

        /***** _call_kw (with error propagation) *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_call_kw(\n";
        rs << "    _handle: *mut c_void,\n";
        rs << "    fn_name: *const std::ffi::c_char,\n";
        rs << "    args: *mut c_void,\n";
        rs << "    kwargs: *mut c_void,\n";
        rs << ") -> *mut c_void {\n";
        rs << "    let name = match unsafe { CStr::from_ptr(fn_name) }.to_str() {\n";
        rs << "        Ok(s) => s,\n";
        rs << "        Err(_) => { set_last_error(\"invalid fn_name\".into()); return std::ptr::null_mut(); }\n";
        rs << "    };\n";
        rs << "    let mut args_vec = if args.is_null() {\n";
        rs << "        vec![]\n";
        rs << "    } else {\n";
        rs << "        match unsafe { retrieve(args) } {\n";
        rs << "            serde_json::Value::Array(a) => a.clone(),\n";
        rs << "            _ => { set_last_error(\"args not array\".into()); return std::ptr::null_mut(); }\n";
        rs << "        }\n";
        rs << "    };\n";
        rs << "    if !kwargs.is_null() {\n";
        rs << "        args_vec.push(unsafe { retrieve(kwargs) }.clone());\n";
        rs << "    }\n";
        rs << "    let reg = registry().lock().unwrap();\n";
        rs << "    match reg.get(name) {\n";
        rs << "        Some(f) => match f(args_vec) { Ok(v) => store(v), Err(e) => { set_last_error(e); std::ptr::null_mut() } },\n";
        rs << "        None => { set_last_error(format!(\"unknown function: {}\", name)); std::ptr::null_mut() }\n";
        rs << "    }\n";
        rs << "}\n\n";

        /***** _construct *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_construct(\n";
        rs << "    type_name: *const std::ffi::c_char,\n";
        rs << "    ctor_name: *const std::ffi::c_char,\n";
        rs << "    args: *mut c_void,\n";
        rs << ") -> *mut c_void {\n";
        rs << "    let tn = match unsafe { CStr::from_ptr(type_name) }.to_str() {\n";
        rs << "        Ok(s) => s, Err(_) => { set_last_error(\"invalid type_name\".into()); return std::ptr::null_mut(); }\n";
        rs << "    };\n";
        rs << "    let cn = match unsafe { CStr::from_ptr(ctor_name) }.to_str() {\n";
        rs << "        Ok(s) => s, Err(_) => { set_last_error(\"invalid ctor_name\".into()); return std::ptr::null_mut(); }\n";
        rs << "    };\n";
        rs << "    let args_vec = if args.is_null() { vec![] } else {\n";
        rs << "        match unsafe { retrieve(args) } {\n";
        rs << "            serde_json::Value::Array(a) => a.clone(),\n";
        rs << "            _ => { set_last_error(\"args not array\".into()); return std::ptr::null_mut(); }\n";
        rs << "        }\n";
        rs << "    };\n";
        rs << "    let reg = ctor_registry().lock().unwrap();\n";
        rs << "    match reg.get(tn).and_then(|ctors| ctors.get(cn)) {\n";
        rs << "        Some(f) => match f(args_vec) {\n";
        rs << "            Ok(ptr) => ptr,\n";
        rs << "            Err(e) => { set_last_error(e); std::ptr::null_mut() }\n";
        rs << "        },\n";
        rs << "        None => { set_last_error(format!(\"no constructor {}::{}\", tn, cn)); std::ptr::null_mut() }\n";
        rs << "    }\n";
        rs << "}\n\n";

        /***** _call_method *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_call_method(\n";
        rs << "    handle: *mut c_void,\n";
        rs << "    type_name: *const std::ffi::c_char,\n";
        rs << "    method: *const std::ffi::c_char,\n";
        rs << "    args: *mut c_void,\n";
        rs << ") -> *mut c_void {\n";
        rs << "    if handle.is_null() { set_last_error(\"null handle\".into()); return std::ptr::null_mut(); }\n";
        rs << "    let tn = match unsafe { CStr::from_ptr(type_name) }.to_str() {\n";
        rs << "        Ok(s) => s, Err(_) => { set_last_error(\"invalid type_name\".into()); return std::ptr::null_mut(); }\n";
        rs << "    };\n";
        rs << "    let mn = match unsafe { CStr::from_ptr(method) }.to_str() {\n";
        rs << "        Ok(s) => s, Err(_) => { set_last_error(\"invalid method\".into()); return std::ptr::null_mut(); }\n";
        rs << "    };\n";
        rs << "    let args_vec = if args.is_null() { vec![] } else {\n";
        rs << "        match unsafe { retrieve(args) } {\n";
        rs << "            serde_json::Value::Array(a) => a.clone(),\n";
        rs << "            _ => { set_last_error(\"args not array\".into()); return std::ptr::null_mut(); }\n";
        rs << "        }\n";
        rs << "    };\n";
        rs << "    let reg = type_registry().lock().unwrap();\n";
        rs << "    match reg.get(tn).and_then(|methods| methods.get(mn)) {\n";
        rs << "        Some(f) => match f(handle, args_vec) {\n";
        rs << "            Ok(v) => store(v),\n";
        rs << "            Err(e) => { set_last_error(e); std::ptr::null_mut() }\n";
        rs << "        },\n";
        rs << "        None => { set_last_error(format!(\"no method {}::{}\", tn, mn)); std::ptr::null_mut() }\n";
        rs << "    }\n";
        rs << "}\n\n";

        /***** _free_type *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_free_type(handle: *mut c_void, type_name: *const std::ffi::c_char) {\n";
        rs << "    if handle.is_null() { return; }\n";
        rs << "    let tn = match unsafe { CStr::from_ptr(type_name) }.to_str() {\n";
        rs << "        Ok(s) => s,\n";
        rs << "        Err(_) => { set_last_error(\"invalid type_name\".into()); return; }\n";
        rs << "    };\n";
        rs << "    let reg = drop_registry().lock().unwrap();\n";
        rs << "    if let Some(drop_fn) = reg.get(tn) {\n";
        rs << "        unsafe { drop_fn(handle); }\n";
        rs << "    } else {\n";
        rs << "        set_last_error(format!(\"unknown type for drop: {}\", tn));\n";
        rs << "    }\n";
        rs << "}\n\n";

        /***** _last_error *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_last_error() -> *mut std::ffi::c_char {\n";
        rs << "    LAST_ERROR.with(|e| {\n";
        rs << "        let s = e.borrow().clone();\n";
        rs << "        if s.is_empty() { std::ptr::null_mut() } else { CString::new(s).unwrap_or_default().into_raw() }\n";
        rs << "    })\n";
        rs << "}\n";

        if (write_file(dir + "/src/lib.rs", rs.str()))
            std::cout << "[bridge]   " << dir << "/src/lib.rs\n";
    }
}

/* ──── gen_cargo_manual_scaffold ──────────────────────────────
   Generates a compilable Cargo.toml + src/lib.rs scaffold for
   crates whose generic/type-parameter-heavy API cannot be
   auto-discovered by the generic wrapper generator.
   ──────────────────────────────────────────────────────────── */
void gen_cargo_manual_scaffold(const std::string& pkg, const std::string& ver,
                                const std::string& dir)
{
    /* Cargo.toml */
    {
        std::ostringstream toml;
        toml << "[package]\n";
        toml << "name = \"" << pkg << "_bridge\"\n";
        toml << "version = \"0.1.0\"\n";
        toml << "edition = \"2021\"\n\n";
        toml << "[lib]\n";
        toml << "crate-type = [\"cdylib\"]\n\n";
        toml << "[dependencies]\n";
        toml << "serde = \"1\"\n";
        if (pkg != "serde_json") {
            toml << "serde_json = \"1\"\n";
        }
        toml << pkg << " = \"" << ver << "\"\n\n";
        toml << "# ── Uncomment additional deps your wrapper needs:\n";
        toml << "# serde_bytes = \"0.11\"\n";
        toml << "# chrono = { version = \"0.4\", features = [\"serde\"] }\n";
        toml << "# uuid = { version = \"1\", features = [\"serde\"] }\n";
        if (write_file(dir + "/Cargo.toml", toml.str()))
            std::cout << "[bridge]   " << dir << "/Cargo.toml\n";
    }

    /* src/lib.rs */
    {
        fs::create_directories(dir + "/src");
        std::ostringstream rs;

        rs << "// Manual bridge scaffold for " << pkg << "@" << ver << "\n";
        rs << "// Fill in each #[no_mangle] extern \"C\" fn below with real logic.\n";
        rs << "// Each fn returns *mut c_void → a heap-allocated serde_json::Value.\n\n";
        rs << "use std::ffi::{CStr, CString, c_void};\n";
        rs << "use std::sync::Mutex;\n";
        rs << "use std::collections::HashMap;\n\n";

        /***** LAST ERROR *****/
        rs << "thread_local! {\n";
        rs << "    static LAST_ERROR: std::cell::RefCell<String> = const { std::cell::RefCell::new(String::new()) };\n";
        rs << "}\n";
        rs << "fn set_last_error(s: String) {\n";
        rs << "    LAST_ERROR.with(|e| *e.borrow_mut() = s);\n";
        rs << "}\n\n";

        /***** VALUE HELPERS *****/
        rs << "fn store(v: serde_json::Value) -> *mut c_void {\n";
        rs << "    Box::into_raw(Box::new(v)) as *mut c_void\n";
        rs << "}\n";
        rs << "unsafe fn retrieve<'a>(ptr: *mut c_void) -> &'a mut serde_json::Value {\n";
        rs << "    &mut *(ptr as *mut serde_json::Value)\n";
        rs << "}\n\n";

        /***** _import / _free *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_import() -> *mut c_void {\n";
        rs << "    store(serde_json::Value::Object(serde_json::Map::new()))\n";
        rs << "}\n\n";

        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_free(ptr: *mut c_void) {\n";
        rs << "    if !ptr.is_null() {\n";
        rs << "        unsafe { drop(Box::from_raw(ptr as *mut serde_json::Value)); }\n";
        rs << "    }\n";
        rs << "}\n\n";

        /***** VALUE CONVERTERS *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_str(s: *const std::ffi::c_char) -> *mut c_void {\n";
        rs << "    let s = unsafe { CStr::from_ptr(s) }.to_string_lossy().to_string();\n";
        rs << "    store(serde_json::Value::String(s))\n";
        rs << "}\n\n";

        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_int(v: i64) -> *mut c_void {\n";
        rs << "    store(serde_json::Value::Number(v.into()))\n";
        rs << "}\n\n";

        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_float(v: f64) -> *mut c_void {\n";
        rs << "    store(serde_json::Value::Number(\n";
        rs << "        serde_json::Number::from_f64(v).unwrap_or(serde_json::Number::from(0))))\n";
        rs << "}\n\n";

        /***** _tuple helpers *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_tuple(items: *mut *mut c_void, count: i32) -> *mut c_void {\n";
        rs << "    let mut vec = Vec::with_capacity(count as usize);\n";
        rs << "    for i in 0..count {\n";
        rs << "        let ptr = unsafe { *items.offset(i as isize) };\n";
        rs << "        vec.push(if ptr.is_null() { serde_json::Value::Null } else { unsafe { retrieve(ptr) }.clone() });\n";
        rs << "    }\n";
        rs << "    store(serde_json::Value::Array(vec))\n";
        rs << "}\n\n";

        /***** _dict helpers *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_dict() -> *mut c_void {\n";
        rs << "    store(serde_json::Value::Object(serde_json::Map::new()))\n";
        rs << "}\n\n";

        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_dict_set(d: *mut c_void, key: *const std::ffi::c_char, val: *mut c_void) -> i32 {\n";
        rs << "    if d.is_null() || val.is_null() { return 0; }\n";
        rs << "    let k = match unsafe { CStr::from_ptr(key) }.to_str() { Ok(s) => s.to_string(), Err(_) => return 0 };\n";
        rs << "    let obj = unsafe { retrieve(d) };\n";
        rs << "    let v = unsafe { retrieve(val) }.clone();\n";
        rs << "    obj.as_object_mut().map(|o| { o.insert(k, v); 1 }).unwrap_or(0)\n";
        rs << "}\n\n";

        /***** TEMPLATE: _call — replace with real logic *****/
        rs << "/* ════════════════════════════════════════════════════════════\n";
        rs << "   MANUAL WRAPPERS — Replace these stubs with real FFI calls\n";
        rs << "   ════════════════════════════════════════════════════════════ */\n\n";

        rs << "/// Example: call a function that takes (a: i32, b: String) -> bool\n";
        rs << "/// Delete once you've adapted it to your actual API.\n";
        rs << "pub fn " << pkg << "_add_user(args: Vec<serde_json::Value>) -> Result<serde_json::Value, String> {\n";
        rs << "    // Deserialize positional args\n";
        rs << "    let name: String = serde_json::from_value(args.get(0).ok_or(\"arg0 missing\")?.clone())\n";
        rs << "        .map_err(|e| e.to_string())?;\n";
        rs << "    let age: i32 = serde_json::from_value(args.get(1).ok_or(\"arg1 missing\")?.clone())\n";
        rs << "        .map_err(|e| e.to_string())?;\n";

        rs << "    // TODO: call " << pkg << "::some_function(name, age);\n";
        rs << "    // For now, return a placeholder:\n";
        rs << "    Ok(serde_json::json!({\"id\": 1, \"name\": name, \"age\": age}))\n";
        rs << "}\n\n";

        /***** _call / _call1.._call6 dispatchers *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_call(_mod: *mut c_void, fn_name: *const std::ffi::c_char, args: *mut c_void) -> *mut c_void {\n";
        rs << "    let fname = match unsafe { CStr::from_ptr(fn_name) }.to_str() {\n";
        rs << "        Ok(s) => s, Err(_) => { set_last_error(\"invalid fn_name\".into()); return std::ptr::null_mut(); }\n";
        rs << "    };\n";
        rs << "    let vargs = if args.is_null() { Vec::new() } else { unsafe { retrieve(args) }.as_array().cloned().unwrap_or_default() };\n";
        rs << "    let result: Result<serde_json::Value, String> = match fname {\n";
        rs << "        \"add_user\" => " << pkg << "_add_user(vargs),\n";
        rs << "        // TODO: add more function name -> handler mappings\n";
        rs << "        _ => Err(format!(\"unknown function: {}\", fname)),\n";
        rs << "    };\n";
        rs << "    match result {\n";
        rs << "        Ok(v) => store(v),\n";
        rs << "        Err(e) => { set_last_error(e); std::ptr::null_mut() }\n";
        rs << "    }\n";
        rs << "}\n\n";

        /***** _last_error *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_last_error() -> *mut std::ffi::c_char {\n";
        rs << "    LAST_ERROR.with(|e| {\n";
        rs << "        let s = e.borrow().clone();\n";
        rs << "        if s.is_empty() { std::ptr::null_mut() } else { CString::new(s).unwrap_or_default().into_raw() }\n";
        rs << "    })\n";
        rs << "}\n";

        if (write_file(dir + "/src/lib.rs", rs.str()))
            std::cout << "[bridge]   " << dir << "/src/lib.rs\n";
    }

    /* README */
    {
        std::ostringstream readme;
        readme << "# " << pkg << " — Manual Cargo Bridge\n\n";
        readme << "This bridge was generated with `--manual` because " << pkg
               << " uses generic types, trait bounds, or conditional compilation\n";
        readme << "that the auto-generator cannot handle.\n\n";
        readme << "## To complete\n\n";
        readme << "1. Open `src/lib.rs` and replace the `add_user` example stub with real wrappers.\n";
        readme << "2. Update the `_call()` dispatcher to map function names to your handlers.\n";
        readme << "3. If the crate's types do not implement `serde::Serialize`/`serde::Deserialize`,\n";
        readme << "   add manual conversion helpers (e.g. via `.to_string()` / `.parse()`).\n";
        readme << "4. Run `cargo build --release` in this directory.\n";
        readme << "5. The resulting cdylib will be loaded by Aurora at runtime.\n\n";
        readme << "## Exported C API\n\n";
        readme << "All `#[no_mangle] pub extern \"C\" fn` symbols in `src/lib.rs` are FFI entry points.\n";
        readme << "The Aurora runtime calls:\n";
        readme << "- `" << pkg << "_import()` — initialize\n";
        readme << "- `" << pkg << "_call(mod, \"fn_name\", args)` — invoke a function\n";
        readme << "- `" << pkg << "_free(ptr)` — free a returned value\n";
        readme << "- `" << pkg << "_last_error()` — retrieve last error string\n\n";
        readme << "## Dependencies\n\n";
        readme << "Edit `Cargo.toml` to add any additional crate dependencies your wrappers need.\n";
        readme << "Keep `serde` and `serde_json` — they are required by the bridge runtime.\n";

        if (write_file(dir + "/README.md", readme.str()))
            std::cout << "[bridge]   " << dir << "/README.md\n";
    }

    std::cout << "[bridge] ✅ Manual cargo scaffold created for " << pkg << "@" << ver << "\n";
    std::cout << "[bridge]   Edit src/lib.rs, then run: cd " << dir << " && cargo build --release\n";
}

