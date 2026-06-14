#include "bridge_shared.h"
#include "tool_detection.h"
#include <set>
#include <array>

/* ── Capture stdout of a shell command (≤64KB) ── */
static std::string exec_capture(const std::string& cmd) {
    std::array<char, 4096> buf;
    std::string result;
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) return result;
    while (fgets(buf.data(), (int)buf.size(), pipe))
        result += buf.data();
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return result;
}

void gen_manifest(const std::string& pkg, const std::string& ecosystem,
                   const std::string& ver, const std::string& desc,
                   const std::string& entry, std::ostream& os)
{
    os << "name: " << pkg << "-" << ecosystem << "\n";
    os << "version: " << ver << "\n";
    if (!desc.empty())
        os << "description: \"" << ecosystem << " bridge: " << desc << "\"\n";
    os << "entry: " << entry << "\n";
    os << "dependencies:\n";
    if (ecosystem == "pypi")      os << "  - python3\n";
    else if (ecosystem == "npm")  os << "  - quickjs\n";
    else if (ecosystem == "cargo")os << "  - rust-std\n";
    else if (ecosystem == "native") os << "  - system\n";
}

/* ── Write file helper ── */
bool write_file(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    if (!f) return false;
    f << content;
    return true;
}

/* ── Locate QuickJS source directory ── */
std::string quickjs_dir() {
    /* Try environment variable override first */
    const char* env = std::getenv("QUICKJS_DIR");
    if (env && env[0]) return env;
    /* Compute from __FILE__: this file is at <root>/aurora/tools/voss/bridge_main.cpp */
    /* QuickJS sources are at <root>/quickjs/ */
    std::string self(__FILE__);
    /* Search backward for the known path segment (handle both / and \) */
    std::vector<std::string> markers = {
        "aurora/tools/voss/bridge_main.cpp",
        "aurora\\tools\\voss\\bridge_main.cpp",
    };
    for (auto& marker : markers) {
        auto pos = self.find(marker);
        if (pos != std::string::npos) {
            return self.substr(0, pos) + "quickjs";
        }
    }
    /* Fallback: relative to CWD */
    return "quickjs";
}

int cmd_bridge(const std::string& ecosystem, const std::string& pkg, const std::string& version) {
    std::cout << "[bridge] " << ecosystem << " → " << pkg << " @" << version << "\n";

    /* ── Resolve via UniversalResolver ── */
    Ecosystem eco_enum = ecosystem_from_name(ecosystem);
    if (eco_enum == Ecosystem::Unknown) {
        std::cerr << "[bridge] unknown ecosystem '" << ecosystem
                  << "' (use pypi, npm, cargo, or native)\n";
        return 1;
    }

    auto cached_http = [](const std::string& u) -> std::string {
        std::string key = "http_" + sha256_hex(u);
        /* 24h TTL for registry metadata */
        std::string cached = cache_get_ttl(key, 86400);
        if (!cached.empty()) return cached;
        std::string result = http_get(u);
        if (!result.empty()) cache_put(key, result);
        return result;
    };
    UniversalResolver resolver(cached_http);
    JsonValue json;
    std::string ver = version;
    std::string desc;

    if (ecosystem != "native") {
        UnifiedPackageInfo info = resolver.resolve(pkg, eco_enum, version);
        if (!info.found) {
            /* If crates.io API is down (503), try CDN fallback for cargo */
            if (ecosystem == "cargo" && !version.empty() && version != "latest") {
                info.name = pkg;
                info.version = version;
                info.found = true;
                std::cout << "[bridge]   (API unavailable, using CDN directly)\n";
            } else {
                std::cerr << "[bridge] ERROR: could not resolve " << pkg
                          << " @ " << version << " from " << ecosystem << "\n";
                if (ecosystem == "cargo")
                    std::cerr << "[bridge]   crates.io API returned 503. "
                              << "Use a specific version (e.g. serde@1.0.0) to bypass.\n";
                return 1;
            }
        }
        ver = info.version;
        desc = info.description;

        if (!info.raw_json.empty())
            json = JsonParser::parse(info.raw_json);

        std::cout << "[bridge] " << pkg << "@" << ver;
        if (!desc.empty()) std::cout << " - " << desc;
        std::cout << "\n";
    }

    if (ver.empty()) {
        std::cerr << "[bridge] ERROR: could not determine version\n";
        return 1;
    }

    /* ── Create output directory ── */
    std::string dir = pkg + "_" + ecosystem;
    fs::create_directories(dir);
    std::cout << "[bridge]   dir: " << fs::absolute(dir) << "\n";

    /* ── Generate .au binding ── */
    {
        std::ostringstream au;
        if (ecosystem == "pypi")       gen_pypi_au_binding(pkg, json, ver, au);
        else if (ecosystem == "npm")   gen_npm_au_binding(pkg, json, ver, au);
        else if (ecosystem == "native") {
            std::string dll_path = find_native_dll(pkg);
            if (dll_path.empty()) {
                std::cerr << "[bridge] ERROR: could not find native library '" << pkg << "'\n";
                std::cerr << "[bridge]   searched: System32, PATH, current directory\n";
                return 1;
            }
            std::cout << "[bridge]   found: " << dll_path << "\n";
            std::vector<std::string> exports = get_dll_exports(dll_path);
            std::cout << "[bridge]   " << exports.size() << " exports discovered\n";
            gen_native_au_binding(pkg, dll_path, exports, au);
        }
        /* cargo: .au generated after function discovery below */

        if (!au.str().empty()) {
            std::string au_path = dir + "/" + pkg + ".auf";
            if (write_file(au_path, au.str()))
                std::cout << "[bridge]   " << au_path << "\n";
        }
    }

    /* ── Generate universal .au binding with Type IR cost annotations ── */
    if (eco_enum != Ecosystem::Unknown && json.type != JsonValue::Null) {
        UnifiedPackageInfo uinfo;
        uinfo.name = pkg;
        uinfo.version = ver;
        uinfo.description = desc;
        uinfo.ecosystem = eco_enum;

        /* Extract npm dependencies from JSON */
        if (ecosystem == "npm") {
            auto* deps = json.get("dependencies");
            if (deps && deps->type == JsonValue::Object) {
                for (auto& [dname, dver] : deps->obj) {
                    UnifiedPackageInfo dep;
                    dep.name = dname;
                    dep.version = dver.type == JsonValue::String ? dver.str_val : "*";
                    dep.ecosystem = Ecosystem::Npm;
                    uinfo.dependencies.push_back(dep);
                }
            }
        }

        EcosystemTypeIRMapper ir_mapper;
        BindingGenOptions bopts;
        bopts.include_cost_annotations = true;
        bopts.include_marshal_stubs = true;
        bopts.include_dependency_info = true;
        UniversalBindingGenerator uni_gen(ir_mapper, bopts);
        std::string universal_au = uni_gen.generate(uinfo);
        std::string uni_path = dir + "/" + pkg + "_universal.auf";
        if (write_file(uni_path, universal_au))
            std::cout << "[bridge]   " << uni_path << " (universal, with Type IR)\n";
    }

    /* ── Generate C wrapper (PyPI only) ── */
    /* This wrapper compiles WITHOUT Python.h — it loads Python DLL dynamically.
       Compatible with any system that has Python installed (no dev headers needed). */
    if (ecosystem == "pypi") {
        std::ostringstream cw;
        cw << "/* Auto-generated bridge DLL for " << pkg << " */\n";
#ifdef _WIN32
        cw << "#include <windows.h>\n";
        cw << "#include <stdio.h>\n";
        cw << "#include <string.h>\n";
        cw << "#include <stdlib.h>\n";
        cw << "#include <stdint.h>\n";
        cw << "typedef int64_t Py_ssize_t;\n\n";
        cw << "/* GIL (Global Interpreter Lock) helpers */\n";
        cw << "typedef unsigned int PyGILState_STATE;\n";
        cw << "static PyGILState_STATE (*g_PyGILState_Ensure)(void)=NULL;\n";
        cw << "static void (*g_PyGILState_Release)(PyGILState_STATE)=NULL;\n";
        cw << "#define GIL_ENTER PyGILState_STATE __gs__ = g_PyGILState_Ensure()\n";
        cw << "#define GIL_RETURN(x) do { g_PyGILState_Release(__gs__); return x; } while(0)\n";
        cw << "#define GIL_RETURN_VOID do { g_PyGILState_Release(__gs__); return; } while(0)\n\n";
        cw << "/* Use shared Python runtime through aurora_runtime — single Py_Initialize() per process */\n";
        cw << "static int s_py_rt_ok = 0;\n";
        cw << "static void* (*rt_pyapi)(const char*) = NULL;\n";
        cw << "/* Cached Python API function pointers (populated once) */\n";
        cw << "static void* (*g_PyObject_GetAttrString)(void*,const char*)=NULL;\n";
        cw << "static void* (*g_PyObject_CallObject)(void*,void*)=NULL;\n";
        cw << "static void* (*g_PyTuple_New)(int)=NULL;\n";
        cw << "static int   (*g_PyTuple_SetItem)(void*,int,void*)=NULL;\n";
        cw << "static void  (*g_Py_DecRef)(void*)=NULL;\n";
        cw << "static void  (*g_PyErr_Print)(void)=NULL;\n";
        cw << "static void  (*g_PyErr_Clear)(void)=NULL;\n";
        cw << "static void* (*g_PyList_New)(int)=NULL;\n";
        cw << "static int   (*g_PyList_SetItem)(void*,int,void*)=NULL;\n";
        cw << "static void  (*g_Py_IncRef)(void*)=NULL;\n";
        cw << "static void* (*g_PyObject_Str)(void*)=NULL;\n";
        cw << "static void* (*g_PyUnicode_FromString)(const char*)=NULL;\n";
        cw << "/* Extended cached API pointers */\n";
        cw << "static void* (*g_PyLong_FromLongLong)(long long)=NULL;\n";
        cw << "static void* (*g_PyFloat_FromDouble)(double)=NULL;\n";
        cw << "static void* (*g_PyDict_New)(void)=NULL;\n";
        cw << "static int   (*g_PyDict_SetItemString)(void*,const char*,void*)=NULL;\n";
        cw << "static const char* (*g_PyUnicode_AsUTF8AndSize)(void*,Py_ssize_t*)=NULL;\n";
        cw << "static int   (*g_PyObject_IsTrue)(void*)=NULL;\n";
        cw << "static void* (*g_PyBool_FromLong)(int)=NULL;\n";
        cw << "static void* (*g_Py_BuildValue)(const char*)=NULL;\n";
        cw << "static void* (*g_PyBytes_FromStringAndSize)(const char*,int)=NULL;\n";
        cw << "static long long (*g_PyLong_AsLongLong)(void*)=NULL;\n";
        cw << "static double (*g_PyFloat_AsDouble)(void*)=NULL;\n";
        cw << "static long long (*g_PyObject_Size)(void*)=NULL;\n";
        cw << "static void* (*g_PyList_GetItem)(void*,int)=NULL;\n";
        cw << "static void* (*g_PyTuple_GetItem)(void*,int)=NULL;\n";
        cw << "static void* (*g_PyObject_Call)(void*,void*,void*)=NULL;\n";
        cw << "static void* (*g_PyImport_ImportModule)(const char*)=NULL;\n";
        cw << "static int   (*g_py_ver_ok)=0;\n";
        cw << "static char g_last_error[4096]=\"\";\n";
        cw << "static void set_last_error(const char* m){strncpy(g_last_error,m,sizeof(g_last_error)-1);g_last_error[sizeof(g_last_error)-1]=0;}\n";
        cw << "static void capture_py_error(void){\n";
        cw << "  typedef void (*PyEF_t)(void**,void**,void**);\n";
        cw << "  PyEF_t PyErr_Fetch=(PyEF_t)rt_pyapi(\"PyErr_Fetch\");\n";
        cw << "  if(!PyErr_Fetch)return;\n";
        cw << "  void* ptype,*pvalue,*ptb;\n";
        cw << "  PyErr_Fetch(&ptype,&pvalue,&ptb);\n";
        cw << "  if(!pvalue)return;\n";
        cw << "  typedef void (*PyER_t)(void*,void*,void*);\n";
        cw << "  typedef void* (*PyIIM_t)(const char*);\n";
        cw << "  typedef void* (*PyOGA_t)(void*,const char*);\n";
        cw << "  typedef void* (*PyOCO_t)(void*,void*);\n";
        cw << "  typedef const char* (*PyUA8_t)(void*);\n";
        cw << "  PyER_t PyErr_Restore=(PyER_t)rt_pyapi(\"PyErr_Restore\");\n";
        cw << "  PyIIM_t PyImport_ImportModule=(PyIIM_t)rt_pyapi(\"PyImport_ImportModule\");\n";
        cw << "  PyOGA_t PyObject_GetAttrString=(PyOGA_t)rt_pyapi(\"PyObject_GetAttrString\");\n";
        cw << "  PyOCO_t PyObject_CallObject=(PyOCO_t)rt_pyapi(\"PyObject_CallObject\");\n";
        cw << "  PyUA8_t PyUnicode_AsUTF8=(PyUA8_t)rt_pyapi(\"PyUnicode_AsUTF8\");\n";
        cw << "  int have_tb=PyErr_Restore&&PyImport_ImportModule&&PyObject_GetAttrString&&PyObject_CallObject&&PyUnicode_AsUTF8;\n";
        cw << "  if(have_tb){\n";
        cw << "    PyErr_Restore(ptype,pvalue,ptb);\n";
        cw << "    void* tb_mod=PyImport_ImportModule(\"traceback\");\n";
        cw << "    if(tb_mod){\n";
        cw << "      void* fmt_fn=PyObject_GetAttrString(tb_mod,\"format_exc\");\n";
        cw << "      if(fmt_fn){\n";
        cw << "        void* tb_str=PyObject_CallObject(fmt_fn,NULL);\n";
        cw << "        if(tb_str){\n";
        cw << "          const char* s=PyUnicode_AsUTF8(tb_str);\n";
        cw << "          if(s){strncpy(g_last_error,s,sizeof(g_last_error)-1);g_last_error[sizeof(g_last_error)-1]=0;}\n";
        cw << "        }\n";
        cw << "      }\n";
        cw << "    }\n";
        cw << "    typedef void (*PyEC_t)();PyEC_t PyErr_Clear=(PyEC_t)rt_pyapi(\"PyErr_Clear\");\n";
        cw << "    if(PyErr_Clear)PyErr_Clear();\n";
        cw << "  }else{\n";
        cw << "    typedef void* (*PyOS_t)(void*);\n";
        cw << "    PyOS_t PyObject_Str=(PyOS_t)rt_pyapi(\"PyObject_Str\");\n";
        cw << "    if(PyObject_Str&&PyUnicode_AsUTF8){void* s=PyObject_Str(pvalue);if(s){const char* cs=PyUnicode_AsUTF8(s);if(cs){strncpy(g_last_error,cs,sizeof(g_last_error)-1);g_last_error[sizeof(g_last_error)-1]=0;}}}\n";
        cw << "  }\n";
        cw << "}\n\n";
        cw << "static void cache_pyapi(void){\n";
        cw << "  g_PyObject_GetAttrString=(void*)rt_pyapi(\"PyObject_GetAttrString\");\n";
        cw << "  g_PyObject_CallObject=(void*)rt_pyapi(\"PyObject_CallObject\");\n";
        cw << "  g_PyTuple_New=(void*)rt_pyapi(\"PyTuple_New\");\n";
        cw << "  g_PyTuple_SetItem=(int(*)(void*,int,void*))rt_pyapi(\"PyTuple_SetItem\");\n";
        cw << "  g_Py_DecRef=(void(*)(void*))rt_pyapi(\"Py_DecRef\");\n";
        cw << "  g_PyErr_Print=(void(*)(void))rt_pyapi(\"PyErr_Print\");\n";
        cw << "  g_PyErr_Clear=(void(*)(void))rt_pyapi(\"PyErr_Clear\");\n";
        cw << "  g_PyList_New=(void*(*)(int))rt_pyapi(\"PyList_New\");\n";
        cw << "  g_PyList_SetItem=(int(*)(void*,int,void*))rt_pyapi(\"PyList_SetItem\");\n";
        cw << "  g_Py_IncRef=(void(*)(void*))rt_pyapi(\"Py_IncRef\");\n";
        cw << "  g_PyObject_Str=(void*(*)(void*))rt_pyapi(\"PyObject_Str\");\n";
        cw << "  g_PyUnicode_FromString=(void*(*)(const char*))rt_pyapi(\"PyUnicode_FromString\");\n";
        cw << "  /* Extended cached API pointers */\n";
        cw << "  g_PyLong_FromLongLong=(void*(*)(long long))rt_pyapi(\"PyLong_FromLongLong\");\n";
        cw << "  g_PyFloat_FromDouble=(void*(*)(double))rt_pyapi(\"PyFloat_FromDouble\");\n";
        cw << "  g_PyDict_New=(void*(*)(void))rt_pyapi(\"PyDict_New\");\n";
        cw << "  g_PyDict_SetItemString=(int(*)(void*,const char*,void*))rt_pyapi(\"PyDict_SetItemString\");\n";
        cw << "  g_PyUnicode_AsUTF8AndSize=(const char*(*)(void*,Py_ssize_t*))rt_pyapi(\"PyUnicode_AsUTF8AndSize\");\n";
        cw << "  g_PyObject_IsTrue=(int(*)(void*))rt_pyapi(\"PyObject_IsTrue\");\n";
        cw << "  g_PyBool_FromLong=(void*(*)(int))rt_pyapi(\"PyBool_FromLong\");\n";
        cw << "  g_Py_BuildValue=(void*(*)(const char*))(void*)rt_pyapi(\"Py_BuildValue\");\n";
        cw << "  g_PyBytes_FromStringAndSize=(void*(*)(const char*,int))rt_pyapi(\"PyBytes_FromStringAndSize\");\n";
        cw << "  g_PyLong_AsLongLong=(long long(*)(void*))rt_pyapi(\"PyLong_AsLongLong\");\n";
        cw << "  g_PyFloat_AsDouble=(double(*)(void*))rt_pyapi(\"PyFloat_AsDouble\");\n";
        cw << "  g_PyObject_Size=(long long(*)(void*))rt_pyapi(\"PyObject_Size\");\n";
        cw << "  g_PyList_GetItem=(void*(*)(void*,int))rt_pyapi(\"PyList_GetItem\");\n";
        cw << "  g_PyTuple_GetItem=(void*(*)(void*,int))rt_pyapi(\"PyTuple_GetItem\");\n";
        cw << "  g_PyObject_Call=(void*(*)(void*,void*,void*))rt_pyapi(\"PyObject_Call\");\n";
        cw << "  g_PyImport_ImportModule=(void*(*)(const char*))rt_pyapi(\"PyImport_ImportModule\");\n";
        cw << "  g_PyGILState_Ensure=(PyGILState_STATE(*)(void))rt_pyapi(\"PyGILState_Ensure\");\n";
        cw << "  g_PyGILState_Release=(void(*)(PyGILState_STATE))rt_pyapi(\"PyGILState_Release\");\n";
        cw << "}\n\n";
        cw << "static int ensure_python(void) {\n";
        cw << "    if (s_py_rt_ok) return 1;\n";
        cw << "    HMODULE rt = GetModuleHandleA(NULL);\n";
        cw << "    typedef int (*InitFn)(void);\n";
        cw << "    InitFn ensure = (InitFn)GetProcAddress(rt, \"aurora_py_ensure_initialized\");\n";
        cw << "    void* (*api)(const char*) = (void* (*)(const char*))GetProcAddress(rt, \"aurora_py_get_api\");\n";
        cw << "    if (!ensure || !api) {\n";
        cw << "        rt = GetModuleHandleA(\"aurora_runtime.dll\");\n";
        cw << "        if (!rt) rt = LoadLibraryA(\"aurora_runtime.dll\");\n";
        cw << "        if (rt) {\n";
        cw << "            ensure = (InitFn)GetProcAddress(rt, \"aurora_py_ensure_initialized\");\n";
        cw << "            api = (void* (*)(const char*))GetProcAddress(rt, \"aurora_py_get_api\");\n";
        cw << "        }\n";
        cw << "    }\n";
        cw << "    if (!ensure || !api) {\n";
        cw << "        fprintf(stderr, \"[python] ERROR: aurora_runtime python init not found\\n\");\n";
        cw << "        set_last_error(\"ensure_python: runtime init not found\");\n";
        cw << "        return 0;\n";
        cw << "    }\n";
        cw << "    rt_pyapi = api;\n";
        cw << "    cache_pyapi();\n";
        cw << "    if (!ensure()) return 0;\n";
        cw << "    /* Version validation */\n";
        cw << "    typedef const char*(*PyGV_t)(void);\n";
        cw << "    PyGV_t Py_GetVersion=(PyGV_t)rt_pyapi(\"Py_GetVersion\");\n";
        cw << "    if(Py_GetVersion){\n";
        cw << "      const char*pv=Py_GetVersion();\n";
        cw << "      if(pv&&pv[0]!='3'){\n";
        cw << "        fprintf(stderr,\"[python] ERROR: Python 3 required, got: %s\\n\",pv);\n";
        cw << "        return 0;\n";
        cw << "      }\n";
        cw << "    }\n";
        cw << "    s_py_rt_ok = 1;\n";
        cw << "    fprintf(stderr, \"[python] bridge using shared runtime\\n\");\n";
        cw << "    return 1;\n";
        cw << "}\n\n";

        /* PyInit_<pkg> — required by Python module system */
        cw << "__declspec(dllexport) void* PyInit_" << pkg << "(void) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    \n";
        cw << "    typedef void* (*PyModCreate_t)(void);\n";
        cw << "    /* We cannot statically link PyModule_Create, so this\n";
        cw << "       function serves as a placeholder — actual Python\n";
        cw << "       module init happens via python3.dll at runtime.\n";
        cw << "       Return a dummy handle so LoadLibrary succeeds. */\n";
        cw << "    GIL_RETURN((void*)1);\n";
        cw << "}\n\n";

        /* Initialize Python runtime once */
        cw << "static int s_py_init = 0;\n\n";

        /* <pkg>_get_last_error — return last captured Python error string */
        cw << "__declspec(dllexport) char* " << pkg << "_get_last_error(void){return g_last_error;}\n\n";

        /* <pkg>_import — initialize Python (once) and import the package */
        cw << "__declspec(dllexport) void* " << pkg << "_import(void) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    typedef void* (*PyImport_t)(const char*);\n";
        cw << "    PyImport_t PyImport = (PyImport_t)rt_pyapi(\"PyImport_ImportModule\");\n";
        cw << "    if (!PyImport) {\n";
        cw << "        PyImport = (PyImport_t)rt_pyapi(\"PyImport_Import\");\n";
        cw << "    }\n";
        cw << "    if (PyImport) GIL_RETURN(PyImport(\"" << pypi_import_alias(pkg) << "\"));\n";
        cw << "    fprintf(stderr, \"[python] ERROR: PyImport_ImportModule not found\\n\");\n";
        cw << "    GIL_RETURN(NULL);\n";
        cw << "}\n\n";

        /* Get Python C API function by name (uses shared runtime) */
        cw << "static void* pyapi(HMODULE py, const char* name) {\n";
        cw << "    return GetProcAddress(py, name);\n";
        cw << "}\n\n";

        /* <pkg>_call — call a function on a Python object.
           Args: fn = function name on the module, args = Python tuple of args (or NULL for no args).
           Returns: result of calling fn (new reference), or NULL on failure. */
        cw << "__declspec(dllexport) void* " << pkg << "_call(void* mod, const char* fn, void* args) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    if (!mod || !fn) GIL_RETURN(NULL);\n";
        cw << "    typedef void* (*PyGAS_t)(void*, const char*);\n";
        cw << "    typedef void* (*PyCO_t)(void*, void*);\n";
        cw << "    PyGAS_t PyObject_GetAttrString = (PyGAS_t)rt_pyapi( \"PyObject_GetAttrString\");\n";
        cw << "    PyCO_t PyObject_CallObject = (PyCO_t)rt_pyapi( \"PyObject_CallObject\");\n";
        cw << "    if (!g_PyObject_GetAttrString || !g_PyObject_CallObject) GIL_RETURN(NULL);\n";
        cw << "    void* func = PyObject_GetAttrString(mod, fn);\n";
        cw << "    if (!func){capture_py_error();GIL_RETURN(NULL);}\n";
        cw << "    void* result = PyObject_CallObject(func, args);\n";
        cw << "    if(!result)capture_py_error();\n";
        cw << "    typedef void (*PyD_t)(void*);\n";
        cw << "    PyD_t Py_DecRef = (PyD_t)rt_pyapi( \"Py_DecRef\");\n";
        cw << "    if (Py_DecRef) Py_DecRef(func);\n";
        cw << "    GIL_RETURN(result);\n";
        cw << "}\n\n";
        /* <pkg>_call1 — convenience: call a function with a single Python object argument.
           Wraps arg in a 1-element tuple automatically. */
        cw << "__declspec(dllexport) void* " << pkg << "_call1(void* mod, const char* fn, void* arg) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    if (!mod || !fn) GIL_RETURN(NULL);\n";
        cw << "    typedef void* (*PyGAS_t)(void*, const char*);\n";
        cw << "    typedef void* (*PyCO_t)(void*, void*);\n";
        cw << "    typedef void* (*PyTN_t)(int);\n";
        cw << "    typedef int   (*PyTSI_t)(void*, int, void*);\n";
        cw << "    typedef void  (*PyEC_t)(void);\n";
        cw << "    typedef void  (*PyD_t)(void*);\n";
        cw << "    PyGAS_t PyObject_GetAttrString = (PyGAS_t)rt_pyapi( \"PyObject_GetAttrString\");\n";
        cw << "    PyCO_t  PyObject_CallObject   = (PyCO_t)rt_pyapi( \"PyObject_CallObject\");\n";
        cw << "    PyTN_t  PyTuple_New           = (PyTN_t)rt_pyapi( \"PyTuple_New\");\n";
        cw << "    PyTSI_t PyTuple_SetItem       = (PyTSI_t)rt_pyapi( \"PyTuple_SetItem\");\n";
        cw << "    PyEC_t  PyErr_Print           = (PyEC_t)rt_pyapi( \"PyErr_Print\");\n";
        cw << "    PyEC_t  PyErr_Clear           = (PyEC_t)rt_pyapi( \"PyErr_Clear\");\n";
        cw << "    PyD_t   Py_DecRef            = (PyD_t)rt_pyapi( \"Py_DecRef\");\n";
        cw << "    if (!g_PyObject_GetAttrString || !g_PyObject_CallObject) GIL_RETURN(NULL);\n";
        cw << "    void* func = PyObject_GetAttrString(mod, fn);\n";
        cw << "    if (!func) { if (g_PyErr_Print) g_PyErr_Print(); else if (g_PyErr_Clear) g_PyErr_Clear(); GIL_RETURN(NULL); }\n";
        cw << "    void* result = NULL;\n";
        cw << "    if (arg && PyTuple_New && PyTuple_SetItem) {\n";
        cw << "        void* tup = PyTuple_New(1);\n";
        cw << "        if (tup) { PyTuple_SetItem(tup, 0, arg); result = PyObject_CallObject(func, tup); if (Py_DecRef) Py_DecRef(tup); }\n";
        cw << "    } else {\n";
        cw << "        result = PyObject_CallObject(func, NULL);\n";
        cw << "    }\n";
        cw << "    if (!result && PyErr_Print) PyErr_Print(); else if (!result && PyErr_Clear) PyErr_Clear();\n";
    cw << "    if (Py_DecRef) Py_DecRef(func);\n";
    cw << "    GIL_RETURN(result);\n";
    cw << "}\n\n";

    /* <pkg>_call2 — convenience: call with 2 args */
    cw << "__declspec(dllexport) void* " << pkg << "_call2(void* mod, const char* fn, void* arg1, void* arg2) {\n";
    cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
    cw << "    if (!mod || !fn) GIL_RETURN(NULL);\n";
    cw << "    typedef void* (*PyGAS_t)(void*, const char*);\n";
    cw << "    typedef void* (*PyCO_t)(void*, void*);\n";
    cw << "    typedef void* (*PyTN_t)(int);\n";
    cw << "    typedef int   (*PyTSI_t)(void*, int, void*);\n";
    cw << "    typedef void  (*PyD_t)(void*);\n";
    cw << "    PyGAS_t PyObject_GetAttrString = (PyGAS_t)rt_pyapi( \"PyObject_GetAttrString\");\n";
    cw << "    PyCO_t  PyObject_CallObject   = (PyCO_t)rt_pyapi( \"PyObject_CallObject\");\n";
    cw << "    PyTN_t  PyTuple_New           = (PyTN_t)rt_pyapi( \"PyTuple_New\");\n";
    cw << "    PyTSI_t PyTuple_SetItem       = (PyTSI_t)rt_pyapi( \"PyTuple_SetItem\");\n";
    cw << "    PyD_t   Py_DecRef            = (PyD_t)rt_pyapi( \"Py_DecRef\");\n";
    cw << "    if (!g_PyObject_GetAttrString || !g_PyObject_CallObject || !g_PyTuple_New || !g_PyTuple_SetItem) GIL_RETURN(NULL);\n";
    cw << "    void* func = PyObject_GetAttrString(mod, fn);\n";
    cw << "    if (!func) GIL_RETURN(NULL);\n";
    cw << "    void* tup = PyTuple_New(2);\n";
    cw << "    if (!tup) { if (g_Py_DecRef) g_Py_DecRef(func); GIL_RETURN(NULL); }\n";
    cw << "    PyTuple_SetItem(tup, 0, arg1);\n";
    cw << "    PyTuple_SetItem(tup, 1, arg2);\n";
    cw << "    void* result = PyObject_CallObject(func, tup);\n";
    cw << "    if (Py_DecRef) { Py_DecRef(tup); Py_DecRef(func); }\n";
    cw << "    GIL_RETURN(result);\n";
    cw << "}\n\n";

    /* <pkg>_call3 — convenience: call with 3 args */
    cw << "__declspec(dllexport) void* " << pkg << "_call3(void* mod, const char* fn, void* arg1, void* arg2, void* arg3) {\n";
    cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
    cw << "    if (!mod || !fn) GIL_RETURN(NULL);\n";
    cw << "    typedef void* (*PyGAS_t)(void*, const char*);\n";
    cw << "    typedef void* (*PyCO_t)(void*, void*);\n";
    cw << "    typedef void* (*PyTN_t)(int);\n";
    cw << "    typedef int   (*PyTSI_t)(void*, int, void*);\n";
    cw << "    typedef void  (*PyD_t)(void*);\n";
    cw << "    PyGAS_t PyObject_GetAttrString = (PyGAS_t)rt_pyapi( \"PyObject_GetAttrString\");\n";
    cw << "    PyCO_t  PyObject_CallObject   = (PyCO_t)rt_pyapi( \"PyObject_CallObject\");\n";
    cw << "    PyTN_t  PyTuple_New           = (PyTN_t)rt_pyapi( \"PyTuple_New\");\n";
    cw << "    PyTSI_t PyTuple_SetItem       = (PyTSI_t)rt_pyapi( \"PyTuple_SetItem\");\n";
    cw << "    PyD_t   Py_DecRef            = (PyD_t)rt_pyapi( \"Py_DecRef\");\n";
    cw << "    if (!g_PyObject_GetAttrString || !g_PyObject_CallObject || !g_PyTuple_New || !g_PyTuple_SetItem) GIL_RETURN(NULL);\n";
    cw << "    void* func = PyObject_GetAttrString(mod, fn);\n";
    cw << "    if (!func) GIL_RETURN(NULL);\n";
    cw << "    void* tup = PyTuple_New(3);\n";
    cw << "    if (!tup) { if (g_Py_DecRef) g_Py_DecRef(func); GIL_RETURN(NULL); }\n";
    cw << "    PyTuple_SetItem(tup, 0, arg1);\n";
    cw << "    PyTuple_SetItem(tup, 1, arg2);\n";
    cw << "    PyTuple_SetItem(tup, 2, arg3);\n";
    cw << "    void* result = PyObject_CallObject(func, tup);\n";
    cw << "    if (Py_DecRef) { Py_DecRef(tup); Py_DecRef(func); }\n";
    cw << "    GIL_RETURN(result);\n";
    cw << "}\n\n";

    /* <pkg>_free — decref a Python object (release reference) */
        cw << "__declspec(dllexport) void " << pkg << "_free(void* handle) {\n";
        cw << "    if (!ensure_python()) return;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    if (!handle) GIL_RETURN_VOID;\n";
        cw << "    typedef void (*PyD_t)(void*);\n";
        cw << "    PyD_t Py_DecRef = (PyD_t)rt_pyapi( \"Py_DecRef\");\n";
        cw << "    if (Py_DecRef) Py_DecRef(handle);\n";
        cw << "}\n\n";

        /* <pkg>_getattr — get attribute from a Python object (for method chaining).
           Returns the attribute value (a bound method or property), or NULL on failure. */
        cw << "__declspec(dllexport) void* " << pkg << "_getattr(void* obj, const char* name) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    if (!obj || !name) GIL_RETURN(NULL);\n";
        cw << "    typedef void* (*PyGAS_t)(void*, const char*);\n";
        cw << "    PyGAS_t PyObject_GetAttrString = (PyGAS_t)rt_pyapi(\"PyObject_GetAttrString\");\n";
        cw << "    GIL_RETURN(g_PyObject_GetAttrString ? g_PyObject_GetAttrString(obj, name) : NULL);\n";
        cw << "}\n\n";

        /* <pkg>_call_kw — call a function with positional + keyword arguments.
           args = PyTuple of positional args (or NULL for none).
           kwargs = PyDict of keyword args (or NULL for none).
           Returns result, or NULL on failure. */
        cw << "__declspec(dllexport) void* " << pkg << "_call_kw(void* mod, const char* fn, void* args, void* kwargs) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    if (!mod || !fn) GIL_RETURN(NULL);\n";
        cw << "    typedef void* (*PyGAS_t)(void*, const char*);\n";
        cw << "    typedef void* (*PyCOKW_t)(void*, void*, void*);\n";
        cw << "    PyGAS_t  PyObject_GetAttrString = (PyGAS_t)rt_pyapi(\"PyObject_GetAttrString\");\n";
        cw << "    PyCOKW_t PyObject_Call          = (PyCOKW_t)rt_pyapi(\"PyObject_Call\");\n";
    cw << "    if (!g_PyObject_GetAttrString || !PyObject_Call) GIL_RETURN(NULL);\n";
    cw << "    void* func = PyObject_GetAttrString(mod, fn);\n";
    cw << "    if (!func) GIL_RETURN(NULL);\n";
    cw << "    void* result = PyObject_Call(func, args, kwargs);\n";
        cw << "    typedef void (*PyD_t)(void*);\n";
        cw << "    PyD_t Py_DecRef = (PyD_t)rt_pyapi(\"Py_DecRef\");\n";
        cw << "    if (Py_DecRef) Py_DecRef(func);\n";
        cw << "    GIL_RETURN(result);\n";
        cw << "}\n\n";

        /* <pkg>_call4 — convenience: call with 4 args */
        cw << "__declspec(dllexport) void* " << pkg << "_call4(void* mod, const char* fn, void* a1, void* a2, void* a3, void* a4) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    if (!mod || !fn) GIL_RETURN(NULL);\n";
        cw << "    typedef void* (*PyGAS_t)(void*, const char*);\n";
        cw << "    typedef void* (*PyCO_t)(void*, void*);\n";
        cw << "    typedef void* (*PyTN_t)(int);\n";
        cw << "    typedef int   (*PyTSI_t)(void*, int, void*);\n";
        cw << "    PyGAS_t PyObject_GetAttrString = (PyGAS_t)rt_pyapi(\"PyObject_GetAttrString\");\n";
        cw << "    PyCO_t  PyObject_CallObject   = (PyCO_t)rt_pyapi(\"PyObject_CallObject\");\n";
        cw << "    PyTN_t  PyTuple_New           = (PyTN_t)rt_pyapi(\"PyTuple_New\");\n";
        cw << "    PyTSI_t PyTuple_SetItem       = (PyTSI_t)rt_pyapi(\"PyTuple_SetItem\");\n";
        cw << "    if (!g_PyObject_GetAttrString || !g_PyObject_CallObject || !g_PyTuple_New || !g_PyTuple_SetItem) GIL_RETURN(NULL);\n";
        cw << "    void* func = PyObject_GetAttrString(mod, fn);\n";
        cw << "    if (!func) GIL_RETURN(NULL);\n";
        cw << "    typedef void  (*PyD_t)(void*);\n";
        cw << "    PyD_t Py_DecRef = (PyD_t)rt_pyapi(\"Py_DecRef\");\n";
        cw << "    void* tup = PyTuple_New(4);\n";
        cw << "    if (!tup) { if (g_Py_DecRef) g_Py_DecRef(func); GIL_RETURN(NULL); }\n";
        cw << "    PyTuple_SetItem(tup, 0, a1); PyTuple_SetItem(tup, 1, a2);\n";
        cw << "    PyTuple_SetItem(tup, 2, a3); PyTuple_SetItem(tup, 3, a4);\n";
        cw << "    void* result = PyObject_CallObject(func, tup);\n";
        cw << "    if (Py_DecRef) { Py_DecRef(tup); Py_DecRef(func); }\n";
        cw << "    GIL_RETURN(result);\n";
        cw << "}\n\n";

        /* <pkg>_call5 — convenience: call with 5 args */
        cw << "__declspec(dllexport) void* " << pkg << "_call5(void* mod, const char* fn, void* a1, void* a2, void* a3, void* a4, void* a5) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    if (!mod || !fn) GIL_RETURN(NULL);\n";
        cw << "    typedef void* (*PyGAS_t)(void*, const char*);\n";
        cw << "    typedef void* (*PyCO_t)(void*, void*);\n";
        cw << "    typedef void* (*PyTN_t)(int);\n";
        cw << "    typedef int   (*PyTSI_t)(void*, int, void*);\n";
        cw << "    PyGAS_t PyObject_GetAttrString = (PyGAS_t)rt_pyapi(\"PyObject_GetAttrString\");\n";
        cw << "    PyCO_t  PyObject_CallObject   = (PyCO_t)rt_pyapi(\"PyObject_CallObject\");\n";
        cw << "    PyTN_t  PyTuple_New           = (PyTN_t)rt_pyapi(\"PyTuple_New\");\n";
        cw << "    PyTSI_t PyTuple_SetItem       = (PyTSI_t)rt_pyapi(\"PyTuple_SetItem\");\n";
        cw << "    if (!g_PyObject_GetAttrString || !g_PyObject_CallObject || !g_PyTuple_New || !g_PyTuple_SetItem) GIL_RETURN(NULL);\n";
        cw << "    void* func = PyObject_GetAttrString(mod, fn);\n";
        cw << "    if (!func) GIL_RETURN(NULL);\n";
        cw << "    typedef void  (*PyD_t)(void*);\n";
        cw << "    PyD_t Py_DecRef = (PyD_t)rt_pyapi(\"Py_DecRef\");\n";
        cw << "    void* tup = PyTuple_New(5);\n";
        cw << "    if (!tup) { if (g_Py_DecRef) g_Py_DecRef(func); GIL_RETURN(NULL); }\n";
        cw << "    PyTuple_SetItem(tup, 0, a1); PyTuple_SetItem(tup, 1, a2);\n";
        cw << "    PyTuple_SetItem(tup, 2, a3); PyTuple_SetItem(tup, 3, a4);\n";
        cw << "    PyTuple_SetItem(tup, 4, a5);\n";
        cw << "    void* result = PyObject_CallObject(func, tup);\n";
        cw << "    if (Py_DecRef) { Py_DecRef(tup); Py_DecRef(func); }\n";
        cw << "    GIL_RETURN(result);\n";
        cw << "}\n\n";

        /* <pkg>_call6 — convenience: call with 6 args */
        cw << "__declspec(dllexport) void* " << pkg << "_call6(void* mod, const char* fn, void* a1, void* a2, void* a3, void* a4, void* a5, void* a6) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    if (!mod || !fn) GIL_RETURN(NULL);\n";
        cw << "    typedef void* (*PyGAS_t)(void*, const char*);\n";
        cw << "    typedef void* (*PyCO_t)(void*, void*);\n";
        cw << "    typedef void* (*PyTN_t)(int);\n";
        cw << "    typedef int   (*PyTSI_t)(void*, int, void*);\n";
        cw << "    PyGAS_t PyObject_GetAttrString = (PyGAS_t)rt_pyapi(\"PyObject_GetAttrString\");\n";
        cw << "    PyCO_t  PyObject_CallObject   = (PyCO_t)rt_pyapi(\"PyObject_CallObject\");\n";
        cw << "    PyTN_t  PyTuple_New           = (PyTN_t)rt_pyapi(\"PyTuple_New\");\n";
        cw << "    PyTSI_t PyTuple_SetItem       = (PyTSI_t)rt_pyapi(\"PyTuple_SetItem\");\n";
        cw << "    if (!g_PyObject_GetAttrString || !g_PyObject_CallObject || !g_PyTuple_New || !g_PyTuple_SetItem) GIL_RETURN(NULL);\n";
        cw << "    void* func = PyObject_GetAttrString(mod, fn);\n";
        cw << "    if (!func) GIL_RETURN(NULL);\n";
        cw << "    typedef void  (*PyD_t)(void*);\n";
        cw << "    PyD_t Py_DecRef = (PyD_t)rt_pyapi(\"Py_DecRef\");\n";
        cw << "    void* tup = PyTuple_New(6);\n";
        cw << "    if (!tup) { if (g_Py_DecRef) g_Py_DecRef(func); GIL_RETURN(NULL); }\n";
        cw << "    PyTuple_SetItem(tup, 0, a1); PyTuple_SetItem(tup, 1, a2);\n";
        cw << "    PyTuple_SetItem(tup, 2, a3); PyTuple_SetItem(tup, 3, a4);\n";
        cw << "    PyTuple_SetItem(tup, 4, a5); PyTuple_SetItem(tup, 5, a6);\n";
        cw << "    void* result = PyObject_CallObject(func, tup);\n";
        cw << "    if (Py_DecRef) { Py_DecRef(tup); Py_DecRef(func); }\n";
        cw << "    GIL_RETURN(result);\n";
        cw << "}\n\n";

        /* Helper: create a Python string from a C string */
        cw << "__declspec(dllexport) void* " << pkg << "_str(const char* s) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    if (!s) GIL_RETURN(NULL);\n";
        cw << "    typedef void* (*PyUS_t)(const char*);\n";
        cw << "    PyUS_t PyUnicode_FromString = (PyUS_t)rt_pyapi( \"PyUnicode_FromString\");\n";
        cw << "    GIL_RETURN(PyUnicode_FromString ? PyUnicode_FromString(s) : NULL);\n";
        cw << "}\n\n";

        /* Helper: create a Python integer */
        cw << "__declspec(dllexport) void* " << pkg << "_int(long long v) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    \n";
        cw << "    typedef void* (*PyFL_t)(long long);\n";
        cw << "    PyFL_t PyLong_FromLongLong = (PyFL_t)rt_pyapi( \"PyLong_FromLongLong\");\n";
        cw << "    GIL_RETURN(PyLong_FromLongLong ? PyLong_FromLongLong(v) : NULL);\n";
        cw << "}\n\n";

        /* Helper: create a Python float */
        cw << "__declspec(dllexport) void* " << pkg << "_float(double v) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    \n";
        cw << "    typedef void* (*PyFF_t)(double);\n";
        cw << "    PyFF_t PyFloat_FromDouble = (PyFF_t)rt_pyapi( \"PyFloat_FromDouble\");\n";
        cw << "    GIL_RETURN(PyFloat_FromDouble ? PyFloat_FromDouble(v) : NULL);\n";
        cw << "}\n\n";

        /* Helper: create a tuple from an array of objects */
        cw << "__declspec(dllexport) void* " << pkg << "_tuple(void** items, int count) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    \n";
        cw << "    typedef void* (*PyT_t)(int);\n";
        cw << "    typedef int (*PyST_t)(void*, int, void*);\n";
        cw << "    PyT_t PyTuple_New = (PyT_t)rt_pyapi( \"PyTuple_New\");\n";
        cw << "    PyST_t PyTuple_SetItem = (PyST_t)rt_pyapi( \"PyTuple_SetItem\");\n";
        cw << "    if (!g_PyTuple_New || !g_PyTuple_SetItem) GIL_RETURN(NULL);\n";
        cw << "    void* tup = PyTuple_New(count);\n";
        cw << "    if (!tup) GIL_RETURN(NULL);\n";
        cw << "    for (int i = 0; i < count; i++) {\n";
        cw << "        if (items && items[i]) PyTuple_SetItem(tup, i, items[i]);\n";
        cw << "    }\n";
    cw << "    GIL_RETURN(tup);\n";
    cw << "}\n\n";

    /* Helper: create a Python list from an array of objects */
    cw << "__declspec(dllexport) void* " << pkg << "_list(void** items, int count) {\n";
    cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
    cw << "    \n";
    cw << "    typedef void* (*PyLN_t)(int);\n";
    cw << "    typedef int (*PyLSI_t)(void*, int, void*);\n";
    cw << "    PyLN_t PyList_New = (PyLN_t)rt_pyapi(\"PyList_New\");\n";
    cw << "    PyLSI_t PyList_SetItem = (PyLSI_t)rt_pyapi(\"PyList_SetItem\");\n";
    cw << "    if (!g_PyList_New || !g_PyList_SetItem) GIL_RETURN(NULL);\n";
    cw << "    void* lst = PyList_New(count);\n";
    cw << "    if (!lst) GIL_RETURN(NULL);\n";
    cw << "    for (int i = 0; i < count; i++) {\n";
    cw << "        if (items && items[i]) PyList_SetItem(lst, i, items[i]);\n";
    cw << "    }\n";
    cw << "    GIL_RETURN(lst);\n";
    cw << "}\n\n";
    cw << "/* Variadic tuple helpers (ergonomic from Aurora, no pointer array needed) */\n";
    cw << "__declspec(dllexport) void* " << pkg << "_tuple2(void* a, void* b) {\n";
    cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
    cw << "    typedef void* (*PyTN_t)(int);\n";
    cw << "    typedef int (*PyTSI_t)(void*, int, void*);\n";
    cw << "    typedef void (*PyIR_t)(void*);\n";
    cw << "    PyTN_t pn = (PyTN_t)rt_pyapi(\"PyTuple_New\");\n";
    cw << "    PyTSI_t ps = (PyTSI_t)rt_pyapi(\"PyTuple_SetItem\");\n";
    cw << "    PyIR_t pir = (PyIR_t)rt_pyapi(\"Py_IncRef\");\n";
    cw << "    if (!g_PyTuple_New || !g_PyTuple_SetItem || !g_Py_IncRef) GIL_RETURN(NULL);\n";
    cw << "    void* t = pn(2);\n";
    cw << "    if (!t) GIL_RETURN(NULL);\n";
    cw << "    if (a) { pir(a); ps(t, 0, a); }\n";
    cw << "    if (b) { pir(b); ps(t, 1, b); }\n";
    cw << "    GIL_RETURN(t);\n";
    cw << "}\n\n";
    cw << "__declspec(dllexport) void* " << pkg << "_tuple3(void* a, void* b, void* c) {\n";
    cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
    cw << "    typedef void* (*PyTN_t)(int);\n";
    cw << "    typedef int (*PyTSI_t)(void*, int, void*);\n";
    cw << "    typedef void (*PyIR_t)(void*);\n";
    cw << "    PyTN_t pn = (PyTN_t)rt_pyapi(\"PyTuple_New\");\n";
    cw << "    PyTSI_t ps = (PyTSI_t)rt_pyapi(\"PyTuple_SetItem\");\n";
    cw << "    PyIR_t pir = (PyIR_t)rt_pyapi(\"Py_IncRef\");\n";
    cw << "    if (!g_PyTuple_New || !g_PyTuple_SetItem || !g_Py_IncRef) GIL_RETURN(NULL);\n";
    cw << "    void* t = pn(3);\n";
    cw << "    if (!t) GIL_RETURN(NULL);\n";
    cw << "    if (a) { pir(a); ps(t, 0, a); }\n";
    cw << "    if (b) { pir(b); ps(t, 1, b); }\n";
    cw << "    if (c) { pir(c); ps(t, 2, c); }\n";
    cw << "    GIL_RETURN(t);\n";
    cw << "}\n\n";
    cw << "__declspec(dllexport) void* " << pkg << "_tuple4(void* a, void* b, void* c, void* d) {\n";
    cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
    cw << "    typedef void* (*PyTN_t)(int);\n";
    cw << "    typedef int (*PyTSI_t)(void*, int, void*);\n";
    cw << "    typedef void (*PyIR_t)(void*);\n";
    cw << "    PyTN_t pn = (PyTN_t)rt_pyapi(\"PyTuple_New\");\n";
    cw << "    PyTSI_t ps = (PyTSI_t)rt_pyapi(\"PyTuple_SetItem\");\n";
    cw << "    PyIR_t pir = (PyIR_t)rt_pyapi(\"Py_IncRef\");\n";
    cw << "    if (!g_PyTuple_New || !g_PyTuple_SetItem || !g_Py_IncRef) GIL_RETURN(NULL);\n";
    cw << "    void* t = pn(4);\n";
    cw << "    if (!t) GIL_RETURN(NULL);\n";
    cw << "    if (a) { pir(a); ps(t, 0, a); }\n";
    cw << "    if (b) { pir(b); ps(t, 1, b); }\n";
    cw << "    if (c) { pir(c); ps(t, 2, c); }\n";
    cw << "    if (d) { pir(d); ps(t, 3, d); }\n";
    cw << "    GIL_RETURN(t);\n";
    cw << "}\n\n";
    cw << "/* Variadic list helpers */\n";
    cw << "__declspec(dllexport) void* " << pkg << "_list2(void* a, void* b) {\n";
    cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
    cw << "    typedef void* (*PyLN_t)(int);\n";
    cw << "    typedef int (*PyLSI_t)(void*, int, void*);\n";
    cw << "    typedef void (*PyIR_t)(void*);\n";
    cw << "    PyLN_t pn = (PyLN_t)rt_pyapi(\"PyList_New\");\n";
    cw << "    PyLSI_t ps = (PyLSI_t)rt_pyapi(\"PyList_SetItem\");\n";
    cw << "    PyIR_t pir = (PyIR_t)rt_pyapi(\"Py_IncRef\");\n";
    cw << "    if (!g_PyList_New || !g_PyList_SetItem || !g_Py_IncRef) GIL_RETURN(NULL);\n";
    cw << "    void* l = pn(2);\n";
    cw << "    if (!l) GIL_RETURN(NULL);\n";
    cw << "    if (a) { pir(a); ps(l, 0, a); }\n";
    cw << "    if (b) { pir(b); ps(l, 1, b); }\n";
    cw << "    GIL_RETURN(l);\n";
    cw << "}\n\n";
    cw << "__declspec(dllexport) void* " << pkg << "_list3(void* a, void* b, void* c) {\n";
    cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
    cw << "    typedef void* (*PyLN_t)(int);\n";
    cw << "    typedef int (*PyLSI_t)(void*, int, void*);\n";
    cw << "    typedef void (*PyIR_t)(void*);\n";
    cw << "    PyLN_t pn = (PyLN_t)rt_pyapi(\"PyList_New\");\n";
    cw << "    PyLSI_t ps = (PyLSI_t)rt_pyapi(\"PyList_SetItem\");\n";
    cw << "    PyIR_t pir = (PyIR_t)rt_pyapi(\"Py_IncRef\");\n";
    cw << "    if (!g_PyList_New || !g_PyList_SetItem || !g_Py_IncRef) GIL_RETURN(NULL);\n";
    cw << "    void* l = pn(3);\n";
    cw << "    if (!l) GIL_RETURN(NULL);\n";
    cw << "    if (a) { pir(a); ps(l, 0, a); }\n";
    cw << "    if (b) { pir(b); ps(l, 1, b); }\n";
    cw << "    if (c) { pir(c); ps(l, 2, c); }\n";
    cw << "    GIL_RETURN(l);\n";
    cw << "}\n\n";
    cw << "__declspec(dllexport) void* " << pkg << "_list4(void* a, void* b, void* c, void* d) {\n";
    cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
    cw << "    typedef void* (*PyLN_t)(int);\n";
    cw << "    typedef int (*PyLSI_t)(void*, int, void*);\n";
    cw << "    typedef void (*PyIR_t)(void*);\n";
    cw << "    PyLN_t pn = (PyLN_t)rt_pyapi(\"PyList_New\");\n";
    cw << "    PyLSI_t ps = (PyLSI_t)rt_pyapi(\"PyList_SetItem\");\n";
    cw << "    PyIR_t pir = (PyIR_t)rt_pyapi(\"Py_IncRef\");\n";
    cw << "    if (!g_PyList_New || !g_PyList_SetItem || !g_Py_IncRef) GIL_RETURN(NULL);\n";
    cw << "    void* l = pn(4);\n";
    cw << "    if (!l) GIL_RETURN(NULL);\n";
    cw << "    if (a) { pir(a); ps(l, 0, a); }\n";
    cw << "    if (b) { pir(b); ps(l, 1, b); }\n";
    cw << "    if (c) { pir(c); ps(l, 2, c); }\n";
    cw << "    if (d) { pir(d); ps(l, 3, d); }\n";
    cw << "    GIL_RETURN(l);\n";
    cw << "}\n\n";
    cw << "/* Helper: create an empty dict (for keyword args) */\n";
    cw << "__declspec(dllexport) void* " << pkg << "_dict(void) {\n";
    cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
    cw << "    \n";
    cw << "    typedef void* (*PyDN_t)(void);\n";
    cw << "    PyDN_t PyDict_New = (PyDN_t)rt_pyapi(\"PyDict_New\");\n";
    cw << "    GIL_RETURN(PyDict_New ? PyDict_New() : NULL);\n";
    cw << "}\n\n";
    cw << "__declspec(dllexport) int " << pkg << "_dict_set(void* d, const char* key, void* val) {\n";
    cw << "    if (!ensure_python()) return -1;\n";
    cw << "    GIL_ENTER;\n";
    cw << "    if (!d || !key || !val) GIL_RETURN(-1);\n";
    cw << "    typedef int (*PyDSIS_t)(void*, const char*, void*);\n";
    cw << "    PyDSIS_t PyDict_SetItemString = (PyDSIS_t)rt_pyapi(\"PyDict_SetItemString\");\n";
    cw << "    GIL_RETURN(PyDict_SetItemString ? PyDict_SetItemString(d, key, val) : -1);\n";
    cw << "}\n\n";

    /* Helper: convert Python object to heap-allocated C string */
        cw << "__declspec(dllexport) const char* " << pkg << "_to_cstr(void* obj) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    if (!obj) GIL_RETURN(NULL);\n";
        cw << "    typedef void* (*PyStr_t)(void*);\n";
        cw << "    typedef void  (*PyD_t)(void*);\n";
        cw << "    PyStr_t PyStr = (PyStr_t)rt_pyapi(\"PyObject_Str\");\n";
        cw << "    PyD_t Py_DecRef = (PyD_t)rt_pyapi(\"Py_DecRef\");\n";
        cw << "    if (!g_PyObject_Str) GIL_RETURN(NULL);\n";
        cw << "    void* s = PyStr(obj);\n";
        cw << "    if (!s) GIL_RETURN(NULL);\n";
        cw << "    typedef const char* (*PyU8S_t)(void*, Py_ssize_t*);\n";
        cw << "    PyU8S_t PyU8 = (PyU8S_t)rt_pyapi(\"PyUnicode_AsUTF8AndSize\");\n";
        cw << "    if (!g_PyUnicode_FromString) { if (g_Py_DecRef) g_Py_DecRef(s); GIL_RETURN(NULL); }\n";
        cw << "    Py_ssize_t sz = 0;\n";
        cw << "    const char* utf8 = PyU8(s, &sz);\n";
        cw << "    if (!utf8) { if (g_Py_DecRef) g_Py_DecRef(s); GIL_RETURN(NULL); }\n";
        cw << "    char* buf = (char*)malloc((size_t)(sz + 1));\n";
        cw << "    if (!buf) { if (g_Py_DecRef) g_Py_DecRef(s); GIL_RETURN(NULL); }\n";
        cw << "    if (sz > 0) memcpy(buf, utf8, (size_t)sz);\n";
        cw << "    buf[sz] = '\\0';\n";
        cw << "    if (Py_DecRef) Py_DecRef(s);\n";
        cw << "    GIL_RETURN(buf);\n";
        cw << "}\n\n";
        cw << "__declspec(dllexport) void " << pkg << "_free_cstr(const char* s) {\n";
        cw << "    free((void*)s);\n";
        cw << "}\n\n";
        cw << "/* Missing type helpers */\n";
        cw << "__declspec(dllexport) int " << pkg << "_to_bool(void* obj) {\n";
        cw << "    if (!ensure_python()) return 0;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    if (!obj) GIL_RETURN(0);\n";
        cw << "    typedef int (*PyOIT_t)(void*);\n";
        cw << "    PyOIT_t PyObject_IsTrue = (PyOIT_t)rt_pyapi(\"PyObject_IsTrue\");\n";
        cw << "    GIL_RETURN(PyObject_IsTrue ? PyObject_IsTrue(obj) : 0);\n";
        cw << "}\n\n";
        cw << "__declspec(dllexport) void* " << pkg << "_bool(int v) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    typedef void* (*PyBFL_t)(int);\n";
        cw << "    PyBFL_t PyBool_FromLong = (PyBFL_t)rt_pyapi(\"PyBool_FromLong\");\n";
        cw << "    GIL_RETURN(PyBool_FromLong ? PyBool_FromLong(v) : NULL);\n";
        cw << "}\n\n";
        cw << "__declspec(dllexport) void* " << pkg << "_none(void) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    typedef void* (*PyBV_t)(const char*);\n";
        cw << "    PyBV_t Py_BuildValue = (PyBV_t)rt_pyapi(\"Py_BuildValue\");\n";
        cw << "    GIL_RETURN(Py_BuildValue ? Py_BuildValue(\"\") : NULL);\n";
        cw << "}\n\n";
        cw << "__declspec(dllexport) void* " << pkg << "_bytes(const char* s, int len) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    typedef void* (*PyBF_t)(const char*, int);\n";
        cw << "    PyBF_t PyBytes_FromStringAndSize = (PyBF_t)rt_pyapi(\"PyBytes_FromStringAndSize\");\n";
        cw << "    GIL_RETURN(PyBytes_FromStringAndSize ? PyBytes_FromStringAndSize(s, len) : NULL);\n";
        cw << "}\n\n";
        cw << "__declspec(dllexport) long long " << pkg << "_to_int(void* obj) {\n";
        cw << "    if (!ensure_python()) return 0;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    if (!obj) GIL_RETURN(0);\n";
        cw << "    typedef long long (*PyLA_t)(void*);\n";
        cw << "    PyLA_t PyLong_AsLongLong = (PyLA_t)rt_pyapi(\"PyLong_AsLongLong\");\n";
        cw << "    GIL_RETURN(PyLong_AsLongLong ? PyLong_AsLongLong(obj) : 0);\n";
        cw << "}\n\n";
        cw << "__declspec(dllexport) double " << pkg << "_to_float(void* obj) {\n";
        cw << "    if (!ensure_python()) return 0.0;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    if (!obj) GIL_RETURN(0.0);\n";
        cw << "    typedef double (*PyFAD_t)(void*);\n";
        cw << "    PyFAD_t PyFloat_AsDouble = (PyFAD_t)rt_pyapi(\"PyFloat_AsDouble\");\n";
        cw << "    GIL_RETURN(PyFloat_AsDouble ? PyFloat_AsDouble(obj) : 0.0);\n";
        cw << "}\n\n";
        cw << "__declspec(dllexport) long long " << pkg << "_len(void* obj) {\n";
        cw << "    if (!ensure_python()) return -1;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    if (!obj) GIL_RETURN(-1);\n";
        cw << "    typedef long long (*PyOS_t)(void*);\n";
        cw << "    PyOS_t PyObject_Size = (PyOS_t)rt_pyapi(\"PyObject_Size\");\n";
        cw << "    GIL_RETURN(PyObject_Size ? PyObject_Size(obj) : -1);\n";
        cw << "}\n\n";
        cw << "__declspec(dllexport) void* " << pkg << "_getitem(void* obj, int index) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    if (!obj) GIL_RETURN(NULL);\n";
        cw << "    typedef void* (*PyLGI_t)(void*, int);\n";
        cw << "    PyLGI_t PyList_GetItem = (PyLGI_t)rt_pyapi(\"PyList_GetItem\");\n";
        cw << "    if (PyList_GetItem) {\n";
        cw << "        void* item = PyList_GetItem(obj, index);\n";
        cw << "        if (item) { typedef void (*PyIR_t)(void*); PyIR_t pir = (PyIR_t)rt_pyapi(\"Py_IncRef\"); if (pir) pir(item); }\n";
        cw << "        GIL_RETURN(item);\n";
        cw << "    }\n";
        cw << "    typedef void* (*PyTGI_t)(void*, int);\n";
        cw << "    PyTGI_t PyTuple_GetItem = (PyTGI_t)rt_pyapi(\"PyTuple_GetItem\");\n";
        cw << "    if (PyTuple_GetItem) {\n";
        cw << "        void* item = PyTuple_GetItem(obj, index);\n";
        cw << "        if (item) { typedef void (*PyIR_t)(void*); PyIR_t pir = (PyIR_t)rt_pyapi(\"Py_IncRef\"); if (pir) pir(item); }\n";
        cw << "        GIL_RETURN(item);\n";
        cw << "    }\n";
        cw << "    GIL_RETURN(NULL);\n";
        cw << "}\n\n";
#else
        cw << "#include <dlfcn.h>\n";
        cw << "#include <stdio.h>\n";
        cw << "#include <string.h>\n";
        cw << "#include <stdlib.h>\n";
        cw << "#include <stdint.h>\n";
        cw << "typedef int64_t Py_ssize_t;\n\n";
        cw << "/* GIL (Global Interpreter Lock) helpers */\n";
        cw << "typedef unsigned int PyGILState_STATE;\n";
        cw << "static PyGILState_STATE (*g_PyGILState_Ensure)(void)=NULL;\n";
        cw << "static void (*g_PyGILState_Release)(PyGILState_STATE)=NULL;\n";
        cw << "#define GIL_ENTER PyGILState_STATE __gs__ = g_PyGILState_Ensure()\n";
        cw << "#define GIL_RETURN(x) do { g_PyGILState_Release(__gs__); return x; } while(0)\n";
        cw << "#define GIL_RETURN_VOID do { g_PyGILState_Release(__gs__); return; } while(0)\n\n";
        cw << "/* Use shared Python runtime through aurora_runtime */\n";
        cw << "static int s_py_rt_ok = 0;\n";
        cw << "static void* (*rt_pyapi)(const char*) = NULL;\n";
        cw << "/* Cached Python API function pointers (populated once) */\n";
        cw << "static void* (*g_PyObject_GetAttrString)(void*,const char*)=NULL;\n";
        cw << "static void* (*g_PyObject_CallObject)(void*,void*)=NULL;\n";
        cw << "static void* (*g_PyTuple_New)(int)=NULL;\n";
        cw << "static int   (*g_PyTuple_SetItem)(void*,int,void*)=NULL;\n";
        cw << "static void  (*g_Py_DecRef)(void*)=NULL;\n";
        cw << "static void  (*g_PyErr_Print)(void)=NULL;\n";
        cw << "static void  (*g_PyErr_Clear)(void)=NULL;\n";
        cw << "static void* (*g_PyList_New)(int)=NULL;\n";
        cw << "static int   (*g_PyList_SetItem)(void*,int,void*)=NULL;\n";
        cw << "static void  (*g_Py_IncRef)(void*)=NULL;\n";
        cw << "static void* (*g_PyObject_Str)(void*)=NULL;\n";
        cw << "static void* (*g_PyUnicode_FromString)(const char*)=NULL;\n";
        cw << "/* Extended cached API pointers (POSIX) */\n";
        cw << "static void* (*g_PyLong_FromLongLong)(long long)=NULL;\n";
        cw << "static void* (*g_PyFloat_FromDouble)(double)=NULL;\n";
        cw << "static void* (*g_PyDict_New)(void)=NULL;\n";
        cw << "static int   (*g_PyDict_SetItemString)(void*,const char*,void*)=NULL;\n";
        cw << "static const char* (*g_PyUnicode_AsUTF8AndSize)(void*,Py_ssize_t*)=NULL;\n";
        cw << "static int   (*g_PyObject_IsTrue)(void*)=NULL;\n";
        cw << "static void* (*g_PyBool_FromLong)(int)=NULL;\n";
        cw << "static void* (*g_Py_BuildValue)(const char*)=NULL;\n";
        cw << "static void* (*g_PyBytes_FromStringAndSize)(const char*,int)=NULL;\n";
        cw << "static long long (*g_PyLong_AsLongLong)(void*)=NULL;\n";
        cw << "static double (*g_PyFloat_AsDouble)(void*)=NULL;\n";
        cw << "static long long (*g_PyObject_Size)(void*)=NULL;\n";
        cw << "static void* (*g_PyList_GetItem)(void*,int)=NULL;\n";
        cw << "static void* (*g_PyTuple_GetItem)(void*,int)=NULL;\n";
        cw << "static void* (*g_PyObject_Call)(void*,void*,void*)=NULL;\n";
        cw << "static void* (*g_PyImport_ImportModule)(const char*)=NULL;\n";
        cw << "static char g_last_error[4096]=\"\";\n";
        cw << "static void set_last_error(const char* m){strncpy(g_last_error,m,sizeof(g_last_error)-1);g_last_error[sizeof(g_last_error)-1]=0;}\n";
        cw << "static void capture_py_error(void){\n";
        cw << "  typedef void (*PyEF_t)(void**,void**,void**);\n";
        cw << "  PyEF_t PyErr_Fetch=(PyEF_t)rt_pyapi(\"PyErr_Fetch\");\n";
        cw << "  if(!PyErr_Fetch)return;\n";
        cw << "  void* ptype,*pvalue,*ptb;\n";
        cw << "  PyErr_Fetch(&ptype,&pvalue,&ptb);\n";
        cw << "  if(!pvalue)return;\n";
        cw << "  typedef void (*PyER_t)(void*,void*,void*);\n";
        cw << "  typedef void* (*PyIIM_t)(const char*);\n";
        cw << "  typedef void* (*PyOGA_t)(void*,const char*);\n";
        cw << "  typedef void* (*PyOCO_t)(void*,void*);\n";
        cw << "  typedef const char* (*PyUA8_t)(void*);\n";
        cw << "  PyER_t PyErr_Restore=(PyER_t)rt_pyapi(\"PyErr_Restore\");\n";
        cw << "  PyIIM_t PyImport_ImportModule=(PyIIM_t)rt_pyapi(\"PyImport_ImportModule\");\n";
        cw << "  PyOGA_t PyObject_GetAttrString=(PyOGA_t)rt_pyapi(\"PyObject_GetAttrString\");\n";
        cw << "  PyOCO_t PyObject_CallObject=(PyOCO_t)rt_pyapi(\"PyObject_CallObject\");\n";
        cw << "  PyUA8_t PyUnicode_AsUTF8=(PyUA8_t)rt_pyapi(\"PyUnicode_AsUTF8\");\n";
        cw << "  int have_tb=PyErr_Restore&&PyImport_ImportModule&&PyObject_GetAttrString&&PyObject_CallObject&&PyUnicode_AsUTF8;\n";
        cw << "  if(have_tb){\n";
        cw << "    PyErr_Restore(ptype,pvalue,ptb);\n";
        cw << "    void* tb_mod=PyImport_ImportModule(\"traceback\");\n";
        cw << "    if(tb_mod){\n";
        cw << "      void* fmt_fn=PyObject_GetAttrString(tb_mod,\"format_exc\");\n";
        cw << "      if(fmt_fn){\n";
        cw << "        void* tb_str=PyObject_CallObject(fmt_fn,NULL);\n";
        cw << "        if(tb_str){\n";
        cw << "          const char* s=PyUnicode_AsUTF8(tb_str);\n";
        cw << "          if(s){strncpy(g_last_error,s,sizeof(g_last_error)-1);g_last_error[sizeof(g_last_error)-1]=0;}\n";
        cw << "        }\n";
        cw << "      }\n";
        cw << "    }\n";
        cw << "    typedef void (*PyEC_t)();PyEC_t PyErr_Clear=(PyEC_t)rt_pyapi(\"PyErr_Clear\");\n";
        cw << "    if(PyErr_Clear)PyErr_Clear();\n";
        cw << "  }else{\n";
        cw << "    typedef void* (*PyOS_t)(void*);\n";
        cw << "    PyOS_t PyObject_Str=(PyOS_t)rt_pyapi(\"PyObject_Str\");\n";
        cw << "    if(PyObject_Str&&PyUnicode_AsUTF8){void* s=PyObject_Str(pvalue);if(s){const char* cs=PyUnicode_AsUTF8(s);if(cs){strncpy(g_last_error,cs,sizeof(g_last_error)-1);g_last_error[sizeof(g_last_error)-1]=0;}}}\n";
        cw << "  }\n";
        cw << "}\n\n";
        cw << "static void cache_pyapi(void){\n";
        cw << "  g_PyObject_GetAttrString=(void*)rt_pyapi(\"PyObject_GetAttrString\");\n";
        cw << "  g_PyObject_CallObject=(void*)rt_pyapi(\"PyObject_CallObject\");\n";
        cw << "  g_PyTuple_New=(void*)rt_pyapi(\"PyTuple_New\");\n";
        cw << "  g_PyTuple_SetItem=(int(*)(void*,int,void*))rt_pyapi(\"PyTuple_SetItem\");\n";
        cw << "  g_Py_DecRef=(void(*)(void*))rt_pyapi(\"Py_DecRef\");\n";
        cw << "  g_PyErr_Print=(void(*)(void))rt_pyapi(\"PyErr_Print\");\n";
        cw << "  g_PyErr_Clear=(void(*)(void))rt_pyapi(\"PyErr_Clear\");\n";
        cw << "  g_PyList_New=(void*(*)(int))rt_pyapi(\"PyList_New\");\n";
        cw << "  g_PyList_SetItem=(int(*)(void*,int,void*))rt_pyapi(\"PyList_SetItem\");\n";
        cw << "  g_Py_IncRef=(void(*)(void*))rt_pyapi(\"Py_IncRef\");\n";
        cw << "  g_PyObject_Str=(void*(*)(void*))rt_pyapi(\"PyObject_Str\");\n";
        cw << "  g_PyUnicode_FromString=(void*(*)(const char*))rt_pyapi(\"PyUnicode_FromString\");\n";
        cw << "  /* Extended cached API pointers (POSIX) */\n";
        cw << "  g_PyLong_FromLongLong=(void*(*)(long long))rt_pyapi(\"PyLong_FromLongLong\");\n";
        cw << "  g_PyFloat_FromDouble=(void*(*)(double))rt_pyapi(\"PyFloat_FromDouble\");\n";
        cw << "  g_PyDict_New=(void*(*)(void))rt_pyapi(\"PyDict_New\");\n";
        cw << "  g_PyDict_SetItemString=(int(*)(void*,const char*,void*))rt_pyapi(\"PyDict_SetItemString\");\n";
        cw << "  g_PyUnicode_AsUTF8AndSize=(const char*(*)(void*,Py_ssize_t*))rt_pyapi(\"PyUnicode_AsUTF8AndSize\");\n";
        cw << "  g_PyObject_IsTrue=(int(*)(void*))rt_pyapi(\"PyObject_IsTrue\");\n";
        cw << "  g_PyBool_FromLong=(void*(*)(int))rt_pyapi(\"PyBool_FromLong\");\n";
        cw << "  g_Py_BuildValue=(void*(*)(const char*))(void*)rt_pyapi(\"Py_BuildValue\");\n";
        cw << "  g_PyBytes_FromStringAndSize=(void*(*)(const char*,int))rt_pyapi(\"PyBytes_FromStringAndSize\");\n";
        cw << "  g_PyLong_AsLongLong=(long long(*)(void*))rt_pyapi(\"PyLong_AsLongLong\");\n";
        cw << "  g_PyFloat_AsDouble=(double(*)(void*))rt_pyapi(\"PyFloat_AsDouble\");\n";
        cw << "  g_PyObject_Size=(long long(*)(void*))rt_pyapi(\"PyObject_Size\");\n";
        cw << "  g_PyList_GetItem=(void*(*)(void*,int))rt_pyapi(\"PyList_GetItem\");\n";
        cw << "  g_PyTuple_GetItem=(void*(*)(void*,int))rt_pyapi(\"PyTuple_GetItem\");\n";
        cw << "  g_PyObject_Call=(void*(*)(void*,void*,void*))rt_pyapi(\"PyObject_Call\");\n";
        cw << "  g_PyImport_ImportModule=(void*(*)(const char*))rt_pyapi(\"PyImport_ImportModule\");\n";
        cw << "  g_PyGILState_Ensure=(PyGILState_STATE(*)(void))rt_pyapi(\"PyGILState_Ensure\");\n";
        cw << "  g_PyGILState_Release=(void(*)(PyGILState_STATE))rt_pyapi(\"PyGILState_Release\");\n";
        cw << "}\n\n";
        cw << "static int ensure_python(void) {\n";
        cw << "    if (s_py_rt_ok) return 1;\n";
        cw << "    /* Try host process symbols first (static link), then dlopen */\n";
        cw << "    int (*ensure)(void) = (int (*)(void))dlsym(RTLD_DEFAULT, \"aurora_py_ensure_initialized\");\n";
        cw << "    void* (*api)(const char*) = (void* (*)(const char*))dlsym(RTLD_DEFAULT, \"aurora_py_get_api\");\n";
        cw << "    if (!ensure || !api) {\n";
        cw << "        void* rt = dlopen(\"libaurora_runtime.so\", RTLD_NOW);\n";
        cw << "        if (!rt) rt = dlopen(\"aurora_runtime\", RTLD_NOW);\n";
        cw << "        if (!rt) { fprintf(stderr, \"[python] ERROR: aurora_runtime not loaded\\n\"); set_last_error(\"ensure_python: runtime init not found\"); return 0; }\n";
        cw << "        ensure = (int (*)(void))dlsym(rt, \"aurora_py_ensure_initialized\");\n";
        cw << "        api = (void* (*)(const char*))dlsym(rt, \"aurora_py_get_api\");\n";
        cw << "    }\n";
        cw << "    if (!ensure || !api) { fprintf(stderr, \"[python] ERROR: missing shared runtime fns\\n\"); return 0; }\n";
        cw << "    rt_pyapi = api;\n";
        cw << "    cache_pyapi();\n";
        cw << "    if (!ensure()) return 0;\n";
        cw << "    /* Version validation */\n";
        cw << "    typedef const char*(*PyGV_t)(void);\n";
        cw << "    PyGV_t Py_GetVersion=(PyGV_t)rt_pyapi(\"Py_GetVersion\");\n";
        cw << "    if(Py_GetVersion){\n";
        cw << "      const char*pv=Py_GetVersion();\n";
        cw << "      if(pv&&pv[0]!='3'){\n";
        cw << "        fprintf(stderr,\"[python] ERROR: Python 3 required, got: %s\\n\",pv);\n";
        cw << "        return 0;\n";
        cw << "      }\n";
        cw << "    }\n";
        cw << "    s_py_rt_ok = 1;\n";
        cw << "    return 1;\n";
        cw << "}\n\n";
        cw << "void* PyInit_" << pkg << "(void) { GIL_ENTER; GIL_RETURN((void*)1); }\n\n";
        cw << "char* " << pkg << "_get_last_error(void){return g_last_error;}\n\n";
        cw << "void* " << pkg << "_import(void) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    typedef void* (*PyImport_t)(const char*);\n";
        cw << "    PyImport_t fn = (PyImport_t)rt_pyapi(\"PyImport_ImportModule\");\n";
        cw << "    if (!fn) fn = (PyImport_t)rt_pyapi(\"PyImport_Import\");\n";
        cw << "    GIL_RETURN(fn ? fn(\"" << pypi_import_alias(pkg) << "\") : NULL);\n";
        cw << "}\n\n";
        cw << "void* " << pkg << "_call(void* mod, const char* fn, void* args) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    if (!mod || !fn) GIL_RETURN(NULL);\n";
    cw << "    if (!g_PyObject_GetAttrString || !g_PyObject_CallObject) GIL_RETURN(NULL);\n";
    cw << "    void* func = g_PyObject_GetAttrString(mod, fn);\n";
    cw << "    if (!func){capture_py_error();GIL_RETURN(NULL);}\n";
    cw << "    void* result = g_PyObject_CallObject(func, args);\n";
    cw << "    if(!result)capture_py_error();\n";
    cw << "    if (g_Py_DecRef) g_Py_DecRef(func);\n";
    cw << "    GIL_RETURN(result);\n";
    cw << "}\n\n";
    cw << "void* " << pkg << "_call1(void* mod, const char* fn, void* arg) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    if (!mod || !fn) GIL_RETURN(NULL);\n";
        cw << "    typedef void* (*PyGAS_t)(void*, const char*);\n";
        cw << "    typedef void* (*PyCO_t)(void*, void*);\n";
        cw << "    typedef void* (*PyTN_t)(int);\n";
        cw << "    typedef int   (*PyTSI_t)(void*, int, void*);\n";
        cw << "    typedef void  (*PyEC_t)(void);\n";
        cw << "    PyGAS_t PyObject_GetAttrString = (PyGAS_t)rt_pyapi( \"PyObject_GetAttrString\");\n";
        cw << "    PyCO_t  PyObject_CallObject   = (PyCO_t)rt_pyapi( \"PyObject_CallObject\");\n";
        cw << "    PyTN_t  PyTuple_New           = (PyTN_t)rt_pyapi( \"PyTuple_New\");\n";
        cw << "    PyTSI_t PyTuple_SetItem       = (PyTSI_t)rt_pyapi( \"PyTuple_SetItem\");\n";
        cw << "    PyEC_t  PyErr_Print           = (PyEC_t)rt_pyapi( \"PyErr_Print\");\n";
        cw << "    PyEC_t  PyErr_Clear           = (PyEC_t)rt_pyapi( \"PyErr_Clear\");\n";
        cw << "    if (!g_PyObject_GetAttrString || !g_PyObject_CallObject) GIL_RETURN(NULL);\n";
        cw << "    void* func = PyObject_GetAttrString(mod, fn);\n";
        cw << "    typedef void  (*PyD_t)(void*);\n";
        cw << "    PyD_t Py_DecRef = (PyD_t)rt_pyapi( \"Py_DecRef\");\n";
        cw << "    if (!func) { if (g_PyErr_Print) g_PyErr_Print(); else if (g_PyErr_Clear) g_PyErr_Clear(); GIL_RETURN(NULL); }\n";
        cw << "    void* result = NULL;\n";
        cw << "    if (arg && PyTuple_New && PyTuple_SetItem) {\n";
        cw << "        void* tup = PyTuple_New(1);\n";
        cw << "        if (tup) { PyTuple_SetItem(tup, 0, arg); result = PyObject_CallObject(func, tup); if (Py_DecRef) Py_DecRef(tup); }\n";
        cw << "    } else {\n";
        cw << "        result = PyObject_CallObject(func, NULL);\n";
        cw << "    }\n";
        cw << "    if (!result && PyErr_Print) PyErr_Print(); else if (!result && PyErr_Clear) PyErr_Clear();\n";
    cw << "    if (Py_DecRef) Py_DecRef(func);\n";
    cw << "    GIL_RETURN(result);\n";
    cw << "}\n\n";
    cw << "void* " << pkg << "_call2(void* mod, const char* fn, void* arg1, void* arg2) {\n";
    cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
    cw << "    if (!mod || !fn) GIL_RETURN(NULL);\n";
    cw << "    typedef void* (*PyGAS_t)(void*, const char*);\n";
    cw << "    typedef void* (*PyCO_t)(void*, void*);\n";
    cw << "    typedef void* (*PyTN_t)(int);\n";
    cw << "    typedef int   (*PyTSI_t)(void*, int, void*);\n";
    cw << "    typedef void  (*PyD_t)(void*);\n";
    cw << "    PyGAS_t PyObject_GetAttrString = (PyGAS_t)rt_pyapi( \"PyObject_GetAttrString\");\n";
    cw << "    PyCO_t  PyObject_CallObject   = (PyCO_t)rt_pyapi( \"PyObject_CallObject\");\n";
    cw << "    PyTN_t  PyTuple_New           = (PyTN_t)rt_pyapi( \"PyTuple_New\");\n";
    cw << "    PyTSI_t PyTuple_SetItem       = (PyTSI_t)rt_pyapi( \"PyTuple_SetItem\");\n";
    cw << "    PyD_t   Py_DecRef            = (PyD_t)rt_pyapi( \"Py_DecRef\");\n";
    cw << "    if (!g_PyObject_GetAttrString || !g_PyObject_CallObject || !g_PyTuple_New || !g_PyTuple_SetItem) GIL_RETURN(NULL);\n";
    cw << "    void* func = PyObject_GetAttrString(mod, fn);\n";
    cw << "    if (!func) GIL_RETURN(NULL);\n";
    cw << "    void* tup = PyTuple_New(2);\n";
    cw << "    if (!tup) { if (g_Py_DecRef) g_Py_DecRef(func); GIL_RETURN(NULL); }\n";
    cw << "    PyTuple_SetItem(tup, 0, arg1);\n";
    cw << "    PyTuple_SetItem(tup, 1, arg2);\n";
    cw << "    void* result = PyObject_CallObject(func, tup);\n";
    cw << "    if (Py_DecRef) { Py_DecRef(tup); Py_DecRef(func); }\n";
    cw << "    GIL_RETURN(result);\n";
    cw << "}\n\n";
    cw << "void* " << pkg << "_call3(void* mod, const char* fn, void* arg1, void* arg2, void* arg3) {\n";
    cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
    cw << "    if (!mod || !fn) GIL_RETURN(NULL);\n";
    cw << "    typedef void* (*PyGAS_t)(void*, const char*);\n";
    cw << "    typedef void* (*PyCO_t)(void*, void*);\n";
    cw << "    typedef void* (*PyTN_t)(int);\n";
    cw << "    typedef int   (*PyTSI_t)(void*, int, void*);\n";
    cw << "    typedef void  (*PyD_t)(void*);\n";
    cw << "    PyGAS_t PyObject_GetAttrString = (PyGAS_t)rt_pyapi( \"PyObject_GetAttrString\");\n";
    cw << "    PyCO_t  PyObject_CallObject   = (PyCO_t)rt_pyapi( \"PyObject_CallObject\");\n";
    cw << "    PyTN_t  PyTuple_New           = (PyTN_t)rt_pyapi( \"PyTuple_New\");\n";
    cw << "    PyTSI_t PyTuple_SetItem       = (PyTSI_t)rt_pyapi( \"PyTuple_SetItem\");\n";
    cw << "    PyD_t   Py_DecRef            = (PyD_t)rt_pyapi( \"Py_DecRef\");\n";
    cw << "    if (!g_PyObject_GetAttrString || !g_PyObject_CallObject || !g_PyTuple_New || !g_PyTuple_SetItem) GIL_RETURN(NULL);\n";
    cw << "    void* func = PyObject_GetAttrString(mod, fn);\n";
    cw << "    if (!func) GIL_RETURN(NULL);\n";
    cw << "    void* tup = PyTuple_New(3);\n";
    cw << "    if (!tup) { if (g_Py_DecRef) g_Py_DecRef(func); GIL_RETURN(NULL); }\n";
    cw << "    PyTuple_SetItem(tup, 0, arg1);\n";
    cw << "    PyTuple_SetItem(tup, 1, arg2);\n";
    cw << "    PyTuple_SetItem(tup, 2, arg3);\n";
    cw << "    void* result = PyObject_CallObject(func, tup);\n";
    cw << "    if (Py_DecRef) { Py_DecRef(tup); Py_DecRef(func); }\n";
    cw << "    GIL_RETURN(result);\n";
    cw << "}\n\n";
    cw << "void " << pkg << "_free(void* handle) {\n";
        cw << "    if (!ensure_python()) return;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    if (!handle) GIL_RETURN_VOID;\n";
        cw << "    typedef void (*PyD_t)(void*);\n";
        cw << "    PyD_t Py_DecRef = (PyD_t)rt_pyapi( \"Py_DecRef\");\n";
        cw << "    if (Py_DecRef) Py_DecRef(handle);\n";
        cw << "}\n\n";
        cw << "void* " << pkg << "_getattr(void* obj, const char* name) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    if (!obj || !name) GIL_RETURN(NULL);\n";
        cw << "    typedef void* (*PyGAS_t)(void*, const char*);\n";
        cw << "    PyGAS_t fn = (PyGAS_t)rt_pyapi(\"PyObject_GetAttrString\");\n";
        cw << "    GIL_RETURN(fn ? fn(obj, name) : NULL);\n";
        cw << "}\n\n";
        cw << "void* " << pkg << "_call_kw(void* mod, const char* fn, void* args, void* kwargs) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    if (!mod || !fn) GIL_RETURN(NULL);\n";
        cw << "    typedef void* (*PyGAS_t)(void*, const char*);\n";
        cw << "    typedef void* (*PyCOKW_t)(void*, void*, void*);\n";
        cw << "    PyGAS_t  ga = (PyGAS_t)rt_pyapi(\"PyObject_GetAttrString\");\n";
    cw << "    PyCOKW_t pc = (PyCOKW_t)rt_pyapi(\"PyObject_Call\");\n";
    cw << "    if (!ga || !pc) GIL_RETURN(NULL);\n";
        cw << "    void* func = ga(mod, fn);\n";
        cw << "    if (!func) GIL_RETURN(NULL);\n";
        cw << "    void* result = pc(func, args, kwargs);\n";
        cw << "    typedef void (*PyD_t)(void*);\n";
        cw << "    PyD_t pd = (PyD_t)rt_pyapi(\"Py_DecRef\");\n";
        cw << "    if (pd) pd(func);\n";
        cw << "    GIL_RETURN(result);\n";
        cw << "}\n\n";
        cw << "typedef void (*PyD_t)(void*);\n";
        cw << "void* " << pkg << "_call4(void* mod, const char* fn, void* a1, void* a2, void* a3, void* a4) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    if (!mod || !fn) GIL_RETURN(NULL);\n";
        cw << "    typedef void* (*PyGAS_t)(void*, const char*);\n";
        cw << "    typedef void* (*PyCO_t)(void*, void*);\n";
        cw << "    typedef void* (*PyTN_t)(int);\n";
        cw << "    typedef int   (*PyTSI_t)(void*, int, void*);\n";
        cw << "    PyGAS_t ga = (PyGAS_t)rt_pyapi(\"PyObject_GetAttrString\");\n";
        cw << "    PyCO_t  pc = (PyCO_t)rt_pyapi(\"PyObject_CallObject\");\n";
        cw << "    PyTN_t  pn = (PyTN_t)rt_pyapi(\"PyTuple_New\");\n";
        cw << "    PyTSI_t ps = (PyTSI_t)rt_pyapi(\"PyTuple_SetItem\");\n";
        cw << "    PyD_t   pd = (PyD_t)rt_pyapi(\"Py_DecRef\");\n";
        cw << "    if (!ga || !pc || !pn || !ps) GIL_RETURN(NULL);\n";
        cw << "    void* func = ga(mod, fn);\n";
        cw << "    if (!func) GIL_RETURN(NULL);\n";
        cw << "    void* tup = pn(4);\n";
        cw << "    if (!tup) { if (pd) pd(func); GIL_RETURN(NULL); }\n";
        cw << "    ps(tup, 0, a1); ps(tup, 1, a2); ps(tup, 2, a3); ps(tup, 3, a4);\n";
        cw << "    void* result = pc(func, tup);\n";
        cw << "    if (pd) { pd(tup); pd(func); }\n";
        cw << "    GIL_RETURN(result);\n";
        cw << "}\n\n";
        cw << "void* " << pkg << "_call5(void* mod, const char* fn, void* a1, void* a2, void* a3, void* a4, void* a5) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    if (!mod || !fn) GIL_RETURN(NULL);\n";
        cw << "    typedef void* (*PyGAS_t)(void*, const char*);\n";
        cw << "    typedef void* (*PyCO_t)(void*, void*);\n";
        cw << "    typedef void* (*PyTN_t)(int);\n";
        cw << "    typedef int   (*PyTSI_t)(void*, int, void*);\n";
        cw << "    PyGAS_t ga = (PyGAS_t)rt_pyapi(\"PyObject_GetAttrString\");\n";
        cw << "    PyCO_t  pc = (PyCO_t)rt_pyapi(\"PyObject_CallObject\");\n";
        cw << "    PyTN_t  pn = (PyTN_t)rt_pyapi(\"PyTuple_New\");\n";
        cw << "    PyTSI_t ps = (PyTSI_t)rt_pyapi(\"PyTuple_SetItem\");\n";
        cw << "    PyD_t   pd = (PyD_t)rt_pyapi(\"Py_DecRef\");\n";
        cw << "    if (!ga || !pc || !pn || !ps) GIL_RETURN(NULL);\n";
        cw << "    void* func = ga(mod, fn);\n";
        cw << "    if (!func) GIL_RETURN(NULL);\n";
        cw << "    void* tup = pn(5);\n";
        cw << "    if (!tup) { if (pd) pd(func); GIL_RETURN(NULL); }\n";
        cw << "    ps(tup, 0, a1); ps(tup, 1, a2); ps(tup, 2, a3); ps(tup, 3, a4); ps(tup, 4, a5);\n";
        cw << "    void* result = pc(func, tup);\n";
        cw << "    if (pd) { pd(tup); pd(func); }\n";
        cw << "    GIL_RETURN(result);\n";
        cw << "}\n\n";
        cw << "void* " << pkg << "_call6(void* mod, const char* fn, void* a1, void* a2, void* a3, void* a4, void* a5, void* a6) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    if (!mod || !fn) GIL_RETURN(NULL);\n";
        cw << "    typedef void* (*PyGAS_t)(void*, const char*);\n";
        cw << "    typedef void* (*PyCO_t)(void*, void*);\n";
        cw << "    typedef void* (*PyTN_t)(int);\n";
        cw << "    typedef int   (*PyTSI_t)(void*, int, void*);\n";
        cw << "    PyGAS_t ga = (PyGAS_t)rt_pyapi(\"PyObject_GetAttrString\");\n";
        cw << "    PyCO_t  pc = (PyCO_t)rt_pyapi(\"PyObject_CallObject\");\n";
        cw << "    PyTN_t  pn = (PyTN_t)rt_pyapi(\"PyTuple_New\");\n";
        cw << "    PyTSI_t ps = (PyTSI_t)rt_pyapi(\"PyTuple_SetItem\");\n";
        cw << "    PyD_t   pd = (PyD_t)rt_pyapi(\"Py_DecRef\");\n";
        cw << "    if (!ga || !pc || !pn || !ps) GIL_RETURN(NULL);\n";
        cw << "    void* func = ga(mod, fn);\n";
        cw << "    if (!func) GIL_RETURN(NULL);\n";
        cw << "    void* tup = pn(6);\n";
        cw << "    if (!tup) { if (pd) pd(func); GIL_RETURN(NULL); }\n";
        cw << "    ps(tup, 0, a1); ps(tup, 1, a2); ps(tup, 2, a3); ps(tup, 3, a4); ps(tup, 4, a5); ps(tup, 5, a6);\n";
        cw << "    void* result = pc(func, tup);\n";
        cw << "    if (pd) { pd(tup); pd(func); }\n";
        cw << "    GIL_RETURN(result);\n";
        cw << "}\n\n";
        cw << "void* " << pkg << "_str(const char* s) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    if (!s) GIL_RETURN(NULL);\n";
        cw << "    typedef void* (*PyUS_t)(const char*);\n";
        cw << "    PyUS_t PyUnicode_FromString = (PyUS_t)rt_pyapi( \"PyUnicode_FromString\");\n";
        cw << "    GIL_RETURN(PyUnicode_FromString ? PyUnicode_FromString(s) : NULL);\n";
        cw << "}\n\n";
        cw << "void* " << pkg << "_int(long long v) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    \n";
        cw << "    typedef void* (*PyFL_t)(long long);\n";
        cw << "    PyFL_t PyLong_FromLongLong = (PyFL_t)rt_pyapi( \"PyLong_FromLongLong\");\n";
        cw << "    GIL_RETURN(PyLong_FromLongLong ? PyLong_FromLongLong(v) : NULL);\n";
        cw << "}\n\n";
        cw << "void* " << pkg << "_float(double v) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    \n";
        cw << "    typedef void* (*PyFF_t)(double);\n";
        cw << "    PyFF_t PyFloat_FromDouble = (PyFF_t)rt_pyapi( \"PyFloat_FromDouble\");\n";
        cw << "    GIL_RETURN(PyFloat_FromDouble ? PyFloat_FromDouble(v) : NULL);\n";
        cw << "}\n\n";
        cw << "void* " << pkg << "_tuple(void** items, int count) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    \n";
        cw << "    typedef void* (*PyT_t)(int);\n";
        cw << "    typedef int (*PyST_t)(void*, int, void*);\n";
        cw << "    PyT_t PyTuple_New = (PyT_t)rt_pyapi( \"PyTuple_New\");\n";
        cw << "    PyST_t PyTuple_SetItem = (PyST_t)rt_pyapi( \"PyTuple_SetItem\");\n";
        cw << "    if (!g_PyTuple_New || !g_PyTuple_SetItem) GIL_RETURN(NULL);\n";
        cw << "    void* tup = PyTuple_New(count);\n";
        cw << "    if (!tup) GIL_RETURN(NULL);\n";
        cw << "    for (int i = 0; i < count; i++) {\n";
        cw << "        if (items && items[i]) PyTuple_SetItem(tup, i, items[i]);\n";
        cw << "    }\n";
    cw << "    GIL_RETURN(tup);\n";
    cw << "}\n\n";
    cw << "void* " << pkg << "_list(void** items, int count) {\n";
    cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
    cw << "    \n";
    cw << "    typedef void* (*PyLN_t)(int);\n";
    cw << "    typedef int (*PyLSI_t)(void*, int, void*);\n";
    cw << "    PyLN_t PyList_New = (PyLN_t)rt_pyapi( \"PyList_New\");\n";
    cw << "    PyLSI_t PyList_SetItem = (PyLSI_t)rt_pyapi( \"PyList_SetItem\");\n";
    cw << "    if (!g_PyList_New || !g_PyList_SetItem) GIL_RETURN(NULL);\n";
    cw << "    void* lst = PyList_New(count);\n";
    cw << "    if (!lst) GIL_RETURN(NULL);\n";
    cw << "    for (int i = 0; i < count; i++) {\n";
    cw << "        if (items && items[i]) PyList_SetItem(lst, i, items[i]);\n";
    cw << "    }\n";
    cw << "    GIL_RETURN(lst);\n";
    cw << "}\n\n";
    cw << "/* Variadic tuple helpers (ergonomic from Aurora, no pointer array needed) */\n";
    cw << "void* " << pkg << "_tuple2(void* a, void* b) {\n";
    cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
    cw << "    typedef void* (*PyTN_t)(int);\n";
    cw << "    typedef int (*PyTSI_t)(void*, int, void*);\n";
    cw << "    typedef void (*PyIR_t)(void*);\n";
    cw << "    PyTN_t pn = (PyTN_t)rt_pyapi(\"PyTuple_New\");\n";
    cw << "    PyTSI_t ps = (PyTSI_t)rt_pyapi(\"PyTuple_SetItem\");\n";
    cw << "    PyIR_t pir = (PyIR_t)rt_pyapi(\"Py_IncRef\");\n";
    cw << "    if (!g_PyTuple_New || !g_PyTuple_SetItem || !g_Py_IncRef) GIL_RETURN(NULL);\n";
    cw << "    void* t = pn(2);\n";
    cw << "    if (!t) GIL_RETURN(NULL);\n";
    cw << "    if (a) { pir(a); ps(t, 0, a); }\n";
    cw << "    if (b) { pir(b); ps(t, 1, b); }\n";
    cw << "    GIL_RETURN(t);\n";
    cw << "}\n\n";
    cw << "void* " << pkg << "_tuple3(void* a, void* b, void* c) {\n";
    cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
    cw << "    typedef void* (*PyTN_t)(int);\n";
    cw << "    typedef int (*PyTSI_t)(void*, int, void*);\n";
    cw << "    typedef void (*PyIR_t)(void*);\n";
    cw << "    PyTN_t pn = (PyTN_t)rt_pyapi(\"PyTuple_New\");\n";
    cw << "    PyTSI_t ps = (PyTSI_t)rt_pyapi(\"PyTuple_SetItem\");\n";
    cw << "    PyIR_t pir = (PyIR_t)rt_pyapi(\"Py_IncRef\");\n";
    cw << "    if (!g_PyTuple_New || !g_PyTuple_SetItem || !g_Py_IncRef) GIL_RETURN(NULL);\n";
    cw << "    void* t = pn(3);\n";
    cw << "    if (!t) GIL_RETURN(NULL);\n";
    cw << "    if (a) { pir(a); ps(t, 0, a); }\n";
    cw << "    if (b) { pir(b); ps(t, 1, b); }\n";
    cw << "    if (c) { pir(c); ps(t, 2, c); }\n";
    cw << "    GIL_RETURN(t);\n";
    cw << "}\n\n";
    cw << "void* " << pkg << "_tuple4(void* a, void* b, void* c, void* d) {\n";
    cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
    cw << "    typedef void* (*PyTN_t)(int);\n";
    cw << "    typedef int (*PyTSI_t)(void*, int, void*);\n";
    cw << "    typedef void (*PyIR_t)(void*);\n";
    cw << "    PyTN_t pn = (PyTN_t)rt_pyapi(\"PyTuple_New\");\n";
    cw << "    PyTSI_t ps = (PyTSI_t)rt_pyapi(\"PyTuple_SetItem\");\n";
    cw << "    PyIR_t pir = (PyIR_t)rt_pyapi(\"Py_IncRef\");\n";
    cw << "    if (!g_PyTuple_New || !g_PyTuple_SetItem || !g_Py_IncRef) GIL_RETURN(NULL);\n";
    cw << "    void* t = pn(4);\n";
    cw << "    if (!t) GIL_RETURN(NULL);\n";
    cw << "    if (a) { pir(a); ps(t, 0, a); }\n";
    cw << "    if (b) { pir(b); ps(t, 1, b); }\n";
    cw << "    if (c) { pir(c); ps(t, 2, c); }\n";
    cw << "    if (d) { pir(d); ps(t, 3, d); }\n";
    cw << "    GIL_RETURN(t);\n";
    cw << "}\n\n";
    cw << "/* Variadic list helpers */\n";
    cw << "void* " << pkg << "_list2(void* a, void* b) {\n";
    cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
    cw << "    typedef void* (*PyLN_t)(int);\n";
    cw << "    typedef int (*PyLSI_t)(void*, int, void*);\n";
    cw << "    typedef void (*PyIR_t)(void*);\n";
    cw << "    PyLN_t pn = (PyLN_t)rt_pyapi(\"PyList_New\");\n";
    cw << "    PyLSI_t ps = (PyLSI_t)rt_pyapi(\"PyList_SetItem\");\n";
    cw << "    PyIR_t pir = (PyIR_t)rt_pyapi(\"Py_IncRef\");\n";
    cw << "    if (!g_PyList_New || !g_PyList_SetItem || !g_Py_IncRef) GIL_RETURN(NULL);\n";
    cw << "    void* l = pn(2);\n";
    cw << "    if (!l) GIL_RETURN(NULL);\n";
    cw << "    if (a) { pir(a); ps(l, 0, a); }\n";
    cw << "    if (b) { pir(b); ps(l, 1, b); }\n";
    cw << "    GIL_RETURN(l);\n";
    cw << "}\n\n";
    cw << "void* " << pkg << "_list3(void* a, void* b, void* c) {\n";
    cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
    cw << "    typedef void* (*PyLN_t)(int);\n";
    cw << "    typedef int (*PyLSI_t)(void*, int, void*);\n";
    cw << "    typedef void (*PyIR_t)(void*);\n";
    cw << "    PyLN_t pn = (PyLN_t)rt_pyapi(\"PyList_New\");\n";
    cw << "    PyLSI_t ps = (PyLSI_t)rt_pyapi(\"PyList_SetItem\");\n";
    cw << "    PyIR_t pir = (PyIR_t)rt_pyapi(\"Py_IncRef\");\n";
    cw << "    if (!g_PyList_New || !g_PyList_SetItem || !g_Py_IncRef) GIL_RETURN(NULL);\n";
    cw << "    void* l = pn(3);\n";
    cw << "    if (!l) GIL_RETURN(NULL);\n";
    cw << "    if (a) { pir(a); ps(l, 0, a); }\n";
    cw << "    if (b) { pir(b); ps(l, 1, b); }\n";
    cw << "    if (c) { pir(c); ps(l, 2, c); }\n";
    cw << "    GIL_RETURN(l);\n";
    cw << "}\n\n";
    cw << "void* " << pkg << "_list4(void* a, void* b, void* c, void* d) {\n";
    cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
    cw << "    typedef void* (*PyLN_t)(int);\n";
    cw << "    typedef int (*PyLSI_t)(void*, int, void*);\n";
    cw << "    typedef void (*PyIR_t)(void*);\n";
    cw << "    PyLN_t pn = (PyLN_t)rt_pyapi(\"PyList_New\");\n";
    cw << "    PyLSI_t ps = (PyLSI_t)rt_pyapi(\"PyList_SetItem\");\n";
    cw << "    PyIR_t pir = (PyIR_t)rt_pyapi(\"Py_IncRef\");\n";
    cw << "    if (!g_PyList_New || !g_PyList_SetItem || !g_Py_IncRef) GIL_RETURN(NULL);\n";
    cw << "    void* l = pn(4);\n";
    cw << "    if (!l) GIL_RETURN(NULL);\n";
    cw << "    if (a) { pir(a); ps(l, 0, a); }\n";
    cw << "    if (b) { pir(b); ps(l, 1, b); }\n";
    cw << "    if (c) { pir(c); ps(l, 2, c); }\n";
    cw << "    if (d) { pir(d); ps(l, 3, d); }\n";
    cw << "    GIL_RETURN(l);\n";
    cw << "}\n\n";
    cw << "/* Helper: create an empty dict (for keyword args) */\n";
    cw << "void* " << pkg << "_dict(void) {\n";
    cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
    cw << "    \n";
    cw << "    typedef void* (*PyDN_t)(void);\n";
    cw << "    PyDN_t PyDict_New = (PyDN_t)rt_pyapi(\"PyDict_New\");\n";
    cw << "    GIL_RETURN(PyDict_New ? PyDict_New() : NULL);\n";
    cw << "}\n\n";
    cw << "int " << pkg << "_dict_set(void* d, const char* key, void* val) {\n";
    cw << "    if (!ensure_python()) return -1;\n";
    cw << "    GIL_ENTER;\n";
    cw << "    if (!d || !key || !val) GIL_RETURN(-1);\n";
    cw << "    typedef int (*PyDSIS_t)(void*, const char*, void*);\n";
    cw << "    PyDSIS_t PyDict_SetItemString = (PyDSIS_t)rt_pyapi(\"PyDict_SetItemString\");\n";
    cw << "    GIL_RETURN(PyDict_SetItemString ? PyDict_SetItemString(d, key, val) : -1);\n";
    cw << "}\n\n";
    cw << "const char* " << pkg << "_to_cstr(void* obj) {\n";
    cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
    cw << "    if (!obj) GIL_RETURN(NULL);\n";
    cw << "    typedef void* (*PyStr_t)(void*);\n";
    cw << "    typedef void  (*PyD_t)(void*);\n";
    cw << "    PyStr_t PyStr = (PyStr_t)rt_pyapi(\"PyObject_Str\");\n";
    cw << "    PyD_t Py_DecRef = (PyD_t)rt_pyapi(\"Py_DecRef\");\n";
    cw << "    if (!g_PyObject_Str) GIL_RETURN(NULL);\n";
    cw << "    void* s = PyStr(obj);\n";
    cw << "    if (!s) GIL_RETURN(NULL);\n";
    cw << "    typedef const char* (*PyU8S_t)(void*, Py_ssize_t*);\n";
    cw << "    PyU8S_t PyU8 = (PyU8S_t)rt_pyapi(\"PyUnicode_AsUTF8AndSize\");\n";
    cw << "    if (!g_PyUnicode_FromString) { if (g_Py_DecRef) g_Py_DecRef(s); GIL_RETURN(NULL); }\n";
    cw << "    Py_ssize_t sz = 0;\n";
    cw << "    const char* utf8 = PyU8(s, &sz);\n";
    cw << "    if (!utf8) { if (g_Py_DecRef) g_Py_DecRef(s); GIL_RETURN(NULL); }\n";
    cw << "    char* buf = (char*)malloc((size_t)(sz + 1));\n";
    cw << "    if (!buf) { if (g_Py_DecRef) g_Py_DecRef(s); GIL_RETURN(NULL); }\n";
    cw << "    if (sz > 0) memcpy(buf, utf8, (size_t)sz);\n";
    cw << "    buf[sz] = '\\0';\n";
    cw << "    if (Py_DecRef) Py_DecRef(s);\n";
    cw << "    GIL_RETURN(buf);\n";
    cw << "}\n\n";
        cw << "void " << pkg << "_free_cstr(const char* s) {\n";
        cw << "    free((void*)s);\n";
        cw << "}\n\n";
        cw << "/* Missing type helpers */\n";
        cw << "int " << pkg << "_to_bool(void* obj) {\n";
        cw << "    if (!ensure_python()) return 0;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    if (!obj) GIL_RETURN(0);\n";
        cw << "    typedef int (*PyOIT_t)(void*);\n";
        cw << "    PyOIT_t PyObject_IsTrue = (PyOIT_t)rt_pyapi(\"PyObject_IsTrue\");\n";
        cw << "    GIL_RETURN(PyObject_IsTrue ? PyObject_IsTrue(obj) : 0);\n";
        cw << "}\n\n";
        cw << "void* " << pkg << "_bool(int v) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    typedef void* (*PyBFL_t)(int);\n";
        cw << "    PyBFL_t PyBool_FromLong = (PyBFL_t)rt_pyapi(\"PyBool_FromLong\");\n";
        cw << "    GIL_RETURN(PyBool_FromLong ? PyBool_FromLong(v) : NULL);\n";
        cw << "}\n\n";
        cw << "void* " << pkg << "_none(void) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    typedef void* (*PyBV_t)(const char*);\n";
        cw << "    PyBV_t Py_BuildValue = (PyBV_t)rt_pyapi(\"Py_BuildValue\");\n";
        cw << "    GIL_RETURN(Py_BuildValue ? Py_BuildValue(\"\") : NULL);\n";
        cw << "}\n\n";
        cw << "void* " << pkg << "_bytes(const char* s, int len) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    typedef void* (*PyBF_t)(const char*, int);\n";
        cw << "    PyBF_t PyBytes_FromStringAndSize = (PyBF_t)rt_pyapi(\"PyBytes_FromStringAndSize\");\n";
        cw << "    GIL_RETURN(PyBytes_FromStringAndSize ? PyBytes_FromStringAndSize(s, len) : NULL);\n";
        cw << "}\n\n";
        cw << "long long " << pkg << "_to_int(void* obj) {\n";
        cw << "    if (!ensure_python()) return 0;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    if (!obj) GIL_RETURN(0);\n";
        cw << "    typedef long long (*PyLA_t)(void*);\n";
        cw << "    PyLA_t PyLong_AsLongLong = (PyLA_t)rt_pyapi(\"PyLong_AsLongLong\");\n";
        cw << "    GIL_RETURN(PyLong_AsLongLong ? PyLong_AsLongLong(obj) : 0);\n";
        cw << "}\n\n";
        cw << "double " << pkg << "_to_float(void* obj) {\n";
        cw << "    if (!ensure_python()) return 0.0;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    if (!obj) GIL_RETURN(0.0);\n";
        cw << "    typedef double (*PyFAD_t)(void*);\n";
        cw << "    PyFAD_t PyFloat_AsDouble = (PyFAD_t)rt_pyapi(\"PyFloat_AsDouble\");\n";
        cw << "    GIL_RETURN(PyFloat_AsDouble ? PyFloat_AsDouble(obj) : 0.0);\n";
        cw << "}\n\n";
        cw << "long long " << pkg << "_len(void* obj) {\n";
        cw << "    if (!ensure_python()) return -1;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    if (!obj) GIL_RETURN(-1);\n";
        cw << "    typedef long long (*PyOS_t)(void*);\n";
        cw << "    PyOS_t PyObject_Size = (PyOS_t)rt_pyapi(\"PyObject_Size\");\n";
        cw << "    GIL_RETURN(PyObject_Size ? PyObject_Size(obj) : -1);\n";
        cw << "}\n\n";
        cw << "void* " << pkg << "_getitem(void* obj, int index) {\n";
        cw << "    if (!ensure_python()) return NULL;\n";
        cw << "    GIL_ENTER;\n";
        cw << "    if (!obj) GIL_RETURN(NULL);\n";
        cw << "    typedef void* (*PyLGI_t)(void*, int);\n";
        cw << "    PyLGI_t PyList_GetItem = (PyLGI_t)rt_pyapi(\"PyList_GetItem\");\n";
        cw << "    if (PyList_GetItem) {\n";
        cw << "        void* item = PyList_GetItem(obj, index);\n";
        cw << "        if (item) { typedef void (*PyIR_t)(void*); PyIR_t pir = (PyIR_t)rt_pyapi(\"Py_IncRef\"); if (pir) pir(item); }\n";
        cw << "        GIL_RETURN(item);\n";
        cw << "    }\n";
        cw << "    typedef void* (*PyTGI_t)(void*, int);\n";
        cw << "    PyTGI_t PyTuple_GetItem = (PyTGI_t)rt_pyapi(\"PyTuple_GetItem\");\n";
        cw << "    if (PyTuple_GetItem) {\n";
        cw << "        void* item = PyTuple_GetItem(obj, index);\n";
        cw << "        if (item) { typedef void (*PyIR_t)(void*); PyIR_t pir = (PyIR_t)rt_pyapi(\"Py_IncRef\"); if (pir) pir(item); }\n";
        cw << "        GIL_RETURN(item);\n";
        cw << "    }\n";
        cw << "    GIL_RETURN(NULL);\n";
        cw << "}\n\n";
#endif

        /* Post-process: replace remaining rt_pyapi lookups with cached globals */
        std::string c_code = cw.str();
        {
            /* Replace per-call lookups with cached globals */
            auto gsub = [&](const std::string& f, const std::string& t) {
                for (size_t p = 0; (p = c_code.find(f, p)) != std::string::npos; p += t.size())
                    c_code.replace(p, f.size(), t);
            };
            gsub("(PyGAS_t)rt_pyapi(\"PyObject_GetAttrString\")", "g_PyObject_GetAttrString");
            gsub("(PyGAS_t)rt_pyapi( \"PyObject_GetAttrString\")", "g_PyObject_GetAttrString");
            gsub("(PyGAS_t)rt_pyapi(\"PyObject_GetAttrString\")", "g_PyObject_GetAttrString");
            gsub("(PyGAS_t)rt_pyapi( \"PyObject_GetAttrString\")", "g_PyObject_GetAttrString");
            gsub("(PyCO_t)rt_pyapi(\"PyObject_CallObject\")", "g_PyObject_CallObject");
            gsub("(PyCO_t)rt_pyapi( \"PyObject_CallObject\")", "g_PyObject_CallObject");
            gsub("(PyCO_t)rt_pyapi(\"PyObject_CallObject\")", "g_PyObject_CallObject");
            gsub("(PyCO_t)rt_pyapi( \"PyObject_CallObject\")", "g_PyObject_CallObject");
            gsub("(PyTN_t)rt_pyapi(\"PyTuple_New\")", "g_PyTuple_New");
            gsub("(PyTN_t)rt_pyapi( \"PyTuple_New\")", "g_PyTuple_New");
            gsub("(PyTSI_t)rt_pyapi(\"PyTuple_SetItem\")", "g_PyTuple_SetItem");
            gsub("(PyTSI_t)rt_pyapi( \"PyTuple_SetItem\")", "g_PyTuple_SetItem");
            gsub("(PyD_t)rt_pyapi(\"Py_DecRef\")", "g_Py_DecRef");
            gsub("(PyD_t)rt_pyapi( \"Py_DecRef\")", "g_Py_DecRef");
            gsub("(PyEC_t)rt_pyapi(\"PyErr_Print\")", "g_PyErr_Print");
            gsub("(PyEC_t)rt_pyapi( \"PyErr_Print\")", "g_PyErr_Print");
            gsub("(PyEC_t)rt_pyapi(\"PyErr_Clear\")", "g_PyErr_Clear");
            gsub("(PyEC_t)rt_pyapi( \"PyErr_Clear\")", "g_PyErr_Clear");

            /* Replace local variable usages with cached globals directly.
               Use space/prefix patterns to avoid double-prefixing g_ names. */
            gsub(" PyObject_GetAttrString(", " g_PyObject_GetAttrString(");
            gsub(")PyObject_GetAttrString(", ")g_PyObject_GetAttrString(");
            gsub(" PyObject_CallObject(", " g_PyObject_CallObject(");
            gsub(")PyObject_CallObject(", ")g_PyObject_CallObject(");
            gsub(" PyTuple_New(", " g_PyTuple_New(");
            gsub(")PyTuple_New(", ")g_PyTuple_New(");
            gsub(" PyTuple_SetItem(", " g_PyTuple_SetItem(");
            gsub(")PyTuple_SetItem(", ")g_PyTuple_SetItem(");
            gsub(" Py_DecRef(", " g_Py_DecRef(");
            gsub(")Py_DecRef(", ")g_Py_DecRef(");
            gsub(" PyErr_Print(", " g_PyErr_Print(");
            gsub(")PyErr_Print(", ")g_PyErr_Print(");
            gsub(" PyErr_Clear(", " g_PyErr_Clear(");
            gsub(")PyErr_Clear(", ")g_PyErr_Clear(");
            /* Replace null checks on local variables */
            gsub(" PyTuple_New &&", " g_PyTuple_New &&");
            gsub(" PyTuple_SetItem)", " g_PyTuple_SetItem)");
            gsub(" PyErr_Print)", " g_PyErr_Print)");
            gsub(" PyErr_Clear)", " g_PyErr_Clear)");
            gsub(" PyErr_Print;", " g_PyErr_Print;");
            gsub(" PyErr_Clear;", " g_PyErr_Clear;");

            /* Remove leftover (PyEC_t) cast patterns in inline lookups */
            gsub("(PyEC_t)rt_pyapi(\"PyErr_Print\");\n", "/* c */\n");
            gsub("(PyEC_t)rt_pyapi(\"PyErr_Clear\");\n", "/* c */\n");
            gsub("PyEC_t pe = (PyEC_t)rt_pyapi(\"PyErr_Print\");\n", "/* c */\n");
            gsub("PyEC_t ec = (PyEC_t)rt_pyapi(\"PyErr_Clear\");\n", "/* c */\n");

            /* Extended: replace all 16 new inline lookups with cached globals */
            gsub("(PyFL_t)rt_pyapi(\"PyLong_FromLongLong\")", "g_PyLong_FromLongLong");
            gsub("(PyFL_t)rt_pyapi( \"PyLong_FromLongLong\")", "g_PyLong_FromLongLong");
            gsub("(PyFF_t)rt_pyapi(\"PyFloat_FromDouble\")", "g_PyFloat_FromDouble");
            gsub("(PyFF_t)rt_pyapi( \"PyFloat_FromDouble\")", "g_PyFloat_FromDouble");
            gsub("(PyDN_t)rt_pyapi(\"PyDict_New\")", "g_PyDict_New");
            gsub("(PyDN_t)rt_pyapi( \"PyDict_New\")", "g_PyDict_New");
            gsub("(PyDSIS_t)rt_pyapi(\"PyDict_SetItemString\")", "g_PyDict_SetItemString");
            gsub("(PyDSIS_t)rt_pyapi( \"PyDict_SetItemString\")", "g_PyDict_SetItemString");
            gsub("(PyU8S_t)rt_pyapi(\"PyUnicode_AsUTF8AndSize\")", "g_PyUnicode_AsUTF8AndSize");
            gsub("(PyU8S_t)rt_pyapi( \"PyUnicode_AsUTF8AndSize\")", "g_PyUnicode_AsUTF8AndSize");
            gsub("(PyOIT_t)rt_pyapi(\"PyObject_IsTrue\")", "g_PyObject_IsTrue");
            gsub("(PyOIT_t)rt_pyapi( \"PyObject_IsTrue\")", "g_PyObject_IsTrue");
            gsub("(PyBFL_t)rt_pyapi(\"PyBool_FromLong\")", "g_PyBool_FromLong");
            gsub("(PyBFL_t)rt_pyapi( \"PyBool_FromLong\")", "g_PyBool_FromLong");
            gsub("(PyBV_t)rt_pyapi(\"Py_BuildValue\")", "g_Py_BuildValue");
            gsub("(PyBV_t)rt_pyapi( \"Py_BuildValue\")", "g_Py_BuildValue");
            gsub("(PyBF_t)rt_pyapi(\"PyBytes_FromStringAndSize\")", "g_PyBytes_FromStringAndSize");
            gsub("(PyBF_t)rt_pyapi( \"PyBytes_FromStringAndSize\")", "g_PyBytes_FromStringAndSize");
            gsub("(PyLA_t)rt_pyapi(\"PyLong_AsLongLong\")", "g_PyLong_AsLongLong");
            gsub("(PyLA_t)rt_pyapi( \"PyLong_AsLongLong\")", "g_PyLong_AsLongLong");
            gsub("(PyFAD_t)rt_pyapi(\"PyFloat_AsDouble\")", "g_PyFloat_AsDouble");
            gsub("(PyFAD_t)rt_pyapi( \"PyFloat_AsDouble\")", "g_PyFloat_AsDouble");
            gsub("(PyOS_t)rt_pyapi(\"PyObject_Size\")", "g_PyObject_Size");
            gsub("(PyOS_t)rt_pyapi( \"PyObject_Size\")", "g_PyObject_Size");
            gsub("(PyLGI_t)rt_pyapi(\"PyList_GetItem\")", "g_PyList_GetItem");
            gsub("(PyLGI_t)rt_pyapi( \"PyList_GetItem\")", "g_PyList_GetItem");
            gsub("(PyTGI_t)rt_pyapi(\"PyTuple_GetItem\")", "g_PyTuple_GetItem");
            gsub("(PyTGI_t)rt_pyapi( \"PyTuple_GetItem\")", "g_PyTuple_GetItem");
            gsub("(PyCOKW_t)rt_pyapi(\"PyObject_Call\")", "g_PyObject_Call");
            gsub("(PyCOKW_t)rt_pyapi( \"PyObject_Call\")", "g_PyObject_Call");
            gsub("(PyIIM_t)rt_pyapi(\"PyImport_ImportModule\")", "g_PyImport_ImportModule");
            gsub("(PyIIM_t)rt_pyapi( \"PyImport_ImportModule\")", "g_PyImport_ImportModule");

            /* Replace local variable usages with cached globals */
            gsub(" PyLong_FromLongLong(", " g_PyLong_FromLongLong(");
            gsub(")PyLong_FromLongLong(", ")g_PyLong_FromLongLong(");
            gsub(" PyFloat_FromDouble(", " g_PyFloat_FromDouble(");
            gsub(")PyFloat_FromDouble(", ")g_PyFloat_FromDouble(");
            gsub(" PyDict_New(", " g_PyDict_New(");
            gsub(")PyDict_New(", ")g_PyDict_New(");
            gsub(" PyDict_SetItemString(", " g_PyDict_SetItemString(");
            gsub(")PyDict_SetItemString(", ")g_PyDict_SetItemString(");
            gsub(" PyU8(", " g_PyUnicode_AsUTF8AndSize(");
            gsub(")PyU8(", ")g_PyUnicode_AsUTF8AndSize(");
            gsub(" PyUnicode_AsUTF8AndSize(", " g_PyUnicode_AsUTF8AndSize(");
            gsub(")PyUnicode_AsUTF8AndSize(", ")g_PyUnicode_AsUTF8AndSize(");
            gsub(" PyObject_IsTrue(", " g_PyObject_IsTrue(");
            gsub(")PyObject_IsTrue(", ")g_PyObject_IsTrue(");
            gsub(" PyBool_FromLong(", " g_PyBool_FromLong(");
            gsub(")PyBool_FromLong(", ")g_PyBool_FromLong(");
            gsub(" Py_BuildValue(", " g_Py_BuildValue(");
            gsub(")Py_BuildValue(", ")g_Py_BuildValue(");
            gsub(" PyBytes_FromStringAndSize(", " g_PyBytes_FromStringAndSize(");
            gsub(")PyBytes_FromStringAndSize(", ")g_PyBytes_FromStringAndSize(");
            gsub(" PyLong_AsLongLong(", " g_PyLong_AsLongLong(");
            gsub(")PyLong_AsLongLong(", ")g_PyLong_AsLongLong(");
            gsub(" PyFloat_AsDouble(", " g_PyFloat_AsDouble(");
            gsub(")PyFloat_AsDouble(", ")g_PyFloat_AsDouble(");
            gsub(" PyObject_Size(", " g_PyObject_Size(");
            gsub(")PyObject_Size(", ")g_PyObject_Size(");
            gsub(" PyList_GetItem(", " g_PyList_GetItem(");
            gsub(")PyList_GetItem(", ")g_PyList_GetItem(");
            gsub(" PyTuple_GetItem(", " g_PyTuple_GetItem(");
            gsub(")PyTuple_GetItem(", ")g_PyTuple_GetItem(");
            gsub(" PyObject_Call(", " g_PyObject_Call(");
            gsub(")PyObject_Call(", ")g_PyObject_Call(");
            gsub(" PyImport_ImportModule(", " g_PyImport_ImportModule(");
            gsub(")PyImport_ImportModule(", ")g_PyImport_ImportModule(");
            /* Extended null-check patterns */
            gsub(" PyLong_FromLongLong ?", " g_PyLong_FromLongLong ?");
            gsub(" PyFloat_FromDouble ?", " g_PyFloat_FromDouble ?");
            gsub(" PyDict_New ?", " g_PyDict_New ?");
            gsub(" PyDict_SetItemString)", " g_PyDict_SetItemString)");
            gsub(" PyUnicode_AsUTF8AndSize ?", " g_PyUnicode_AsUTF8AndSize ?");
            gsub(" PyObject_IsTrue ?", " g_PyObject_IsTrue ?");
            gsub(" PyBool_FromLong ?", " g_PyBool_FromLong ?");
            gsub(" Py_BuildValue ?", " g_Py_BuildValue ?");
            gsub(" PyBytes_FromStringAndSize ?", " g_PyBytes_FromStringAndSize ?");
            gsub(" PyLong_AsLongLong ?", " g_PyLong_AsLongLong ?");
            gsub(" PyFloat_AsDouble ?", " g_PyFloat_AsDouble ?");
            gsub(" PyObject_Size ?", " g_PyObject_Size ?");
            gsub(" PyList_GetItem ?", " g_PyList_GetItem ?");
            gsub(" PyList_GetItem)", " g_PyList_GetItem)");
            gsub(" PyTuple_GetItem ?", " g_PyTuple_GetItem ?");
            gsub(" PyTuple_GetItem)", " g_PyTuple_GetItem)");
            gsub(" PyObject_Call ?", " g_PyObject_Call ?");
            gsub(" PyObject_Call)", " g_PyObject_Call)");
            gsub(" PyImport_ImportModule ?", " g_PyImport_ImportModule ?");
        }
        /* Sanitize pkg name for C identifiers (hyphens → underscores) */
        {
            std::string pkg_safe = pkg;
            for (auto& c : pkg_safe) if (c == '-') c = '_';
            if (pkg_safe != pkg) {
                std::string old_pat = pkg + "_";
                std::string new_pat = pkg_safe + "_";
                size_t pos = 0;
                while ((pos = c_code.find(old_pat, pos)) != std::string::npos) {
                    c_code.replace(pos, old_pat.length(), new_pat);
                    pos += new_pat.length();
                }
                /* Also fix comments: replace pkg name in the header comment */
                old_pat = pkg;
                new_pat = pkg_safe;
                pos = 0;
                while ((pos = c_code.find(old_pat, pos)) != std::string::npos) {
                    /* Only replace inside comments (starts with /* or //) */
                    size_t comment_start = c_code.rfind("/*", pos);
                    size_t comment_line = c_code.rfind("//", pos);
                    size_t line_start = c_code.rfind('\n', pos);
                    bool in_comment = (comment_start != std::string::npos && comment_start > line_start) ||
                                      (comment_line != std::string::npos && comment_line > line_start);
                    if (in_comment || pos == 0 || !std::isalnum((unsigned char)c_code[pos - 1])) {
                        c_code.replace(pos, old_pat.length(), new_pat);
                        pos += new_pat.length();
                    } else {
                        pos += 1;
                    }
                }
            }
        }
        std::string wrapper_path = dir + "/" + pkg + "_" + ecosystem + ".c";
        if (write_file(wrapper_path, c_code))
            std::cout << "[bridge]   " << wrapper_path << "\n";

        /* Ensure Python is available */
        ToolInfo py = ensure_python();
        if (!py.found) {
            std::cerr << "[bridge] WARNING: Python not found — runtime import may fail\n";
            std::cerr << "[bridge]   install Python manually or use 'voss bridge pypi' with system Python\n";
        }

        /* Install the actual Python package via pip */
        std::cout << "[bridge] installing Python package: " << pkg << "\n";
        std::string pip_python = py.found ? py.path : "python";
        int pip_rc = -1;
        /* Use batch file to avoid cmd.exe quoting issues with paths containing spaces/msys chars */
        {
            std::string bat_path = (fs::temp_directory_path() / (pkg + "_pip_install.cmd")).string();
            {
                std::ofstream bat(bat_path);
                bat << "@echo off\n";
                bat << "\"" << pip_python << "\" -m pip install \"" << pkg << "\"\n";
            }
            pip_rc = std::system(("\"" + bat_path + "\"").c_str());
            std::remove(bat_path.c_str());
        }
        if (pip_rc != 0) {
            std::string bat_path = (fs::temp_directory_path() / (pkg + "_pip_fallback.cmd")).string();
            {
                std::ofstream bat(bat_path);
                bat << "@echo off\n";
                bat << "pip install \"" << pkg << "\"\n";
            }
            pip_rc = std::system(("\"" + bat_path + "\"").c_str());
            std::remove(bat_path.c_str());
        }
        if (pip_rc != 0) {
            std::cout << "[bridge] note: pip install failed — runtime import may return NULL\n";
            std::cout << "[bridge]       manually run: pip install " << pkg << "\n";
        } else {
            std::cout << "[bridge]   ✅ pip installed: " << pkg << "@" << ver << "\n";
        }
    } else if (ecosystem == "cargo") {
        /* ── --manual mode: skip auto-discovery, generate scaffold ── */
        if (g_manual_mode) {
            gen_cargo_manual_scaffold(pkg, ver, dir);
            std::cout << "[bridge]   (skipping auto-build — edit src/lib.rs then run cargo build --release)\n";
            /* Write minimal .au binding */
            {
                std::ostringstream au;
                gen_cargo_au_binding(pkg, json, ver, au, "");
                std::string au_path = dir + "/" + pkg + ".auf";
                if (write_file(au_path, au.str()))
                    std::cout << "[bridge]   " << au_path << "\n";
            }
            goto after_build;
        }

        /* Ensure Cargo is available */
        ToolInfo cargo_tool = detect_cargo();
        if (!cargo_tool.found) {
            std::cerr << "[bridge] WARNING: Cargo/Rust not found in PATH — "
                      << "install Rust from https://rustup.rs\n";
        } else {
            std::cout << "[tools] found Cargo: " << cargo_tool.path << "\n";
        }

        /* Cargo: auto-discover public functions, generate Rust project, compile */
        CargoDiscovery disc = discover_cargo_functions(pkg, ver, dir);

        /* Auto-manual fallback: if 0 methods discovered, generate scaffold instead */
        if (disc.fn_count + disc.method_count == 0) {
            std::cout << "[bridge]   auto-gen found 0 bridgeable functions — generating manual scaffold\n";
            gen_cargo_manual_scaffold(pkg, ver, dir);
            /* Write minimal .au binding */
            {
                std::ostringstream au;
                gen_cargo_au_binding(pkg, json, ver, au, "");
                std::string au_path = dir + "/" + pkg + ".auf";
                if (write_file(au_path, au.str()))
                    std::cout << "[bridge]   " << au_path << "\n";
            }
            std::cout << "[bridge]   edit " << dir << "/src/lib.rs then run: cargo build --release\n";
            goto after_build;
        }

        /* Generate .au binding with method/constructor exports */
        {
            std::ostringstream au;
            gen_cargo_au_binding(pkg, json, ver, au, disc.method_au_entries);
            std::string au_path = dir + "/" + pkg + ".auf";
            if (write_file(au_path, au.str()))
                std::cout << "[bridge]   " << au_path << "\n";
        }

        gen_cargo_rust_wrapper(pkg, ver, dir, disc);
        std::string target_flag;
        std::string target_dir = "release";
        if (!g_cross_target.empty()) {
            target_flag = " --target " + g_cross_target;
            target_dir = std::string(g_cross_target) + "/release";
            std::cout << "[bridge]   cross-compiling for " << g_cross_target << "\n";
        }
        std::cout << "[bridge] cargo build: " << pkg << "\n";
#ifdef _WIN32
        std::string cargo_cmd = "cd /D \"" + fs::absolute(dir).string() + "\" && cargo build --release" + target_flag + " 2>&1";
#else
        std::string cargo_cmd = "cd \"" + fs::absolute(dir).string() + "\" && cargo build --release" + target_flag + " 2>&1";
#endif
        int cargo_rc = std::system(cargo_cmd.c_str());
        if (cargo_rc != 0) {
            std::cout << "[bridge] note: cargo build had issues (check Rust toolchain";
            if (!g_cross_target.empty())
                std::cout << " and target '" << g_cross_target << "'";
            std::cout << ")\n";
        } else {
            std::cout << "[bridge]   ✅ cargo build succeeded\n";
#ifdef _WIN32
            std::string dll_src = fs::absolute(dir).string() + "\\target\\" + target_dir + "\\" + pkg + "_bridge.dll";
            std::string dll_dst = fs::absolute(dir).string() + "\\" + pkg + "_cargo.dll";
#else
            std::string dll_src = fs::absolute(dir).string() + "/target/" + target_dir + "/lib" + pkg + "_bridge.so";
            std::string dll_dst = fs::absolute(dir).string() + "/lib" + pkg + "_cargo.so";
#endif
            if (fs::exists(dll_src)) {
                fs::copy_file(dll_src, dll_dst, fs::copy_options::overwrite_existing);
                std::cout << "[bridge]   ✅ DLL: " << dll_dst << "\n";
            } else {
                std::cout << "[bridge] note: cargo output not found at " << dll_src << "\n";
            }
        }
    } else if (ecosystem == "npm") {
        /* npm: auto-detect native addon → choose bridge strategy */
        if (json.type != JsonValue::Null && npm_has_native_addon(json)) {
            /* Native addon: need Node.js subprocess */
            ToolInfo node = ensure_node();
            if (!node.found) {
                std::cerr << "[bridge] WARNING: native addon requires Node.js — "
                          << "auto-download failed, install Node.js manually\n";
            }
            std::cout << "[bridge]   detected native addon (N-API/node-gyp), "
                      << "using Node.js subprocess bridge\n";
            gen_npm_cpp_wrapper(pkg, dir, node.path);
        } else {
            /* Pure JS: use QuickJS bridge (no Node.js needed) */
            gen_quickjs_npm_wrapper(pkg, dir);
        }
    }

after_build:

    /* ── Generate manifest ── */
    {
        std::ostringstream mf;
        gen_manifest(pkg, ecosystem, ver, desc, pkg + ".auf", mf);

        std::string mf_path = dir + "/aurora.pkg";
        if (write_file(mf_path, mf.str()))
            std::cout << "[bridge]   " << mf_path << "\n";
    }

    /* ── Generate type IR annotations ── */
    {
        std::ostringstream tir;
        tir << "// Type IR cost annotations for " << pkg << "\n";
        tir << "// Auto-generated — used by Aurora compiler for zero-cost interop\n";
        tir << "{\n";
        if (ecosystem == "pypi") {
            tir << "  \"types\": {\n";
            tir << "    \"Py_int\":    {\"ir\": \"i64\",   \"cost\": \"zero\"},\n";
            tir << "    \"Py_float\":  {\"ir\": \"f64\",   \"cost\": \"zero\"},\n";
            tir << "    \"Py_str\":    {\"ir\": \"String\", \"cost\": \"alloc\"},\n";
            tir << "    \"Py_bool\":   {\"ir\": \"bool\",   \"cost\": \"zero\"},\n";
            tir << "    \"Py_list\":   {\"ir\": \"List\",   \"cost\": \"alloc\"}\n";
            tir << "  }\n";
        } else if (ecosystem == "npm") {
            tir << "  \"types\": {\n";
            tir << "    \"JS_number\": {\"ir\": \"f64\",   \"cost\": \"zero\"},\n";
            tir << "    \"JS_string\": {\"ir\": \"String\", \"cost\": \"alloc\"},\n";
            tir << "    \"JS_object\": {\"ir\": \"pointer\",\"cost\": \"indirection\"}\n";
            tir << "  }\n";
        } else if (ecosystem == "cargo") {
            tir << "  \"types\": {\n";
            tir << "    \"Rs_i32\":    {\"ir\": \"i32\",   \"cost\": \"zero\"},\n";
            tir << "    \"Rs_f64\":    {\"ir\": \"f64\",   \"cost\": \"zero\"},\n";
            tir << "    \"Rs_String\": {\"ir\": \"String\", \"cost\": \"alloc\"}\n";
            tir << "  }\n";
        } else if (ecosystem == "native") {
            tir << "  \"types\": {\n";
            tir << "    \"C_int\":  {\"ir\": \"i32\",   \"cost\": \"zero\"},\n";
            tir << "    \"C_long\": {\"ir\": \"i64\",   \"cost\": \"zero\"},\n";
            tir << "    \"C_float\":{\"ir\": \"f32\",   \"cost\": \"zero\"},\n";
            tir << "    \"C_double\":{\"ir\": \"f64\",  \"cost\": \"zero\"},\n";
            tir << "    \"C_str\":  {\"ir\": \"cstring\",\"cost\": \"zero\"}\n";
            tir << "  }\n";
        }
        tir << "}\n";

        std::string tir_path = dir + "/types.json";
        if (write_file(tir_path, tir.str()))
            std::cout << "[bridge]   " << tir_path << "\n";
    }

    std::cout << "[bridge] ✅ " << ecosystem << " bridge created for " << pkg << "@" << ver << "\n";

    /* ── Dependency auto-chain (PyPI only — pip install ensures queryability) ── */
    if (ecosystem == "pypi") {
        static std::set<std::string> g_auto_bridged;
        if (g_auto_bridged.insert(pkg).second) {
            /* Query deps via importlib.metadata. Packages were installed by pip
               (which may use a different Python than ensure_python found). */
            std::string py_snippet = std::string("import importlib.metadata; r=importlib.metadata.requires('") + pkg + "'); [print(d) for d in (r or [])]";
            std::string show_out = exec_capture("python -c \"" + py_snippet + "\"");
            std::istringstream ss(show_out);
            std::string line;
            while (std::getline(ss, line)) {
                size_t semi = line.find(';');
                if (semi != std::string::npos) line = line.substr(0, semi);
                /* Strip version constraints — find earliest operator */
                size_t first_op = std::string::npos;
                for (const char* op : {"~=", ">=", "<=", "!=", "==", ">", "<"}) {
                    size_t pos = line.find(op);
                    if (pos != std::string::npos && (first_op == std::string::npos || pos < first_op))
                        first_op = pos;
                }
                if (first_op != std::string::npos)
                    line = line.substr(0, first_op);
                /* Trim whitespace and trailing non-alphanumeric chars (keep _, -, .) */
                line.erase(0, line.find_first_not_of(" \t\r\n"));
                while (!line.empty()) {
                    char c = line.back();
                    if (std::isalnum((unsigned char)c) || c == '_' || c == '-' || c == '.')
                        break;
                    line.pop_back();
                }
                line.erase(0, line.find_first_not_of(" \t\r\n"));
                line.erase(line.find_last_not_of(" \t\r\n") + 1);
                if (!line.empty() && line != pkg) {
                    std::string dep_dir = line + "_" + ecosystem;
                    if (!fs::exists(dep_dir + "/aurora.pkg")) {
                        std::cout << "[bridge]   auto-chaining dependency: " << line << "\n";
                        cmd_bridge(ecosystem, line, "latest");
                    }
                }
            }
        }
    }

    /* ════════════════════════════════════════════════════════════
       Auto-compile C wrapper to shared library (DLL/SO)
       ════════════════════════════════════════════════════════════ */
    /* Skip compilation for native — the original DLL is used directly */
    if (ecosystem == "native") {
        std::cout << "[bridge]   (native bridge uses original DLL directly — no wrapper needed)\n";
    } else {
    std::string wrapper_ext = (ecosystem == "cargo") ? ".rs" : ".c";
    std::string wrapper_file = dir + "/" + pkg + "_" + ecosystem + wrapper_ext;
    if (fs::exists(wrapper_file)) {
        /* Auto-detect C compiler — download Zig cc if none found */
        ToolInfo cc = ensure_c_compiler();
        if (!cc.found) {
            std::cerr << "[bridge] ERROR: no C compiler available — "
                      << "install gcc, clang, MSVC, or Zig\n";
            return 1;
        }
        std::cout << "[bridge] compiling " << wrapper_file
                  << " using " << (cc.is_portable ? "portable " : "") << cc.path << "\n";
        std::string qjs_abs = fs::absolute(fs::path(quickjs_dir())).string();
#ifdef _WIN32
        std::string dll_out = fs::absolute(dir).string() + "\\" + pkg + "_" + ecosystem + ".dll";
        std::string abs_wrapper = fs::absolute(wrapper_file).string();

        /* Build compile command based on compiler */
        std::string cc_path = cc.path;
        std::string cc_exe = fs::path(cc_path).filename().string();

        bool use_zig = cc_exe == "zig" || cc_exe == "zig.exe";
        bool use_msvc = cc_exe == "cl.exe" || cc_exe == "cl";

        if (use_zig || !use_msvc) {
            /* GCC-compatible compiler (gcc, clang, cc, zig cc) — use GCC-style flags */
            std::string bat_path = (fs::temp_directory_path() / (pkg + "_" + ecosystem + "_gcc.cmd")).string();
            {
                std::ofstream bat(bat_path);
                bat << "@echo off\n";
                if (ecosystem == "npm") {
                    bat << "\"" << cc_path << "\" -shared -o \"" << dll_out << "\" \""
                        << abs_wrapper << "\" \""
                        << qjs_abs << "\\quickjs.c\" \""
                        << qjs_abs << "\\cutils.c\" \""
                        << qjs_abs << "\\libregexp.c\" \""
                        << qjs_abs << "\\libunicode.c\" \""
                        << qjs_abs << "\\dtoa.c\""
                        << " -I\"" << qjs_abs << "\""
                        << " -include \"" << qjs_abs << "\\quickjs_config.h\""
                        << " -lbcrypt\n";
                } else {
                    bat << "\"" << cc_path << "\" -shared -o \"" << dll_out << "\" \""
                        << abs_wrapper << "\"\n";
                }
            }
            int gcc_rc = std::system(("\"" + bat_path + "\"").c_str());
            std::remove(bat_path.c_str());
            if (gcc_rc != 0) {
                std::cerr << "[bridge] ERROR: compilation failed (exit " << gcc_rc << ")\n";
                return 1;
            }
        } else {
            /* MSVC (cl.exe) — use MSVC-style flags */
            std::string bat_path = (fs::temp_directory_path() / (pkg + "_" + ecosystem + "_msvc.cmd")).string();
            {
                std::ofstream bat(bat_path);
                bat << "@echo off\n";
                if (ecosystem == "npm") {
                    bat << "\"" << cc_path << "\" /LD /Fe\"" << dll_out << "\" \""
                        << abs_wrapper << "\" \""
                        << qjs_abs << "\\quickjs.c\" \""
                        << qjs_abs << "\\cutils.c\" \""
                        << qjs_abs << "\\libregexp.c\" \""
                        << qjs_abs << "\\libunicode.c\" \""
                        << qjs_abs << "\\dtoa.c\""
                        << " /I\"" << qjs_abs << "\""
                        << " /FI\"" << qjs_abs << "\\quickjs_config.h\"\n";
                } else {
                    bat << "\"" << cc_path << "\" /LD /Fe\"" << dll_out << "\" \""
                        << abs_wrapper << "\"\n";
                }
            }
            int msvc_rc = std::system(("\"" + bat_path + "\"").c_str());
            std::remove(bat_path.c_str());
            if (msvc_rc != 0) {
                std::cerr << "[bridge] ERROR: MSVC compilation failed (exit " << msvc_rc << ")\n";
                return 1;
            }
        }

        std::string dll_check = dll_out;
#else
        /* POSIX: use detected C compiler */
        std::string so_path = dir + "/lib" + pkg + "_" + ecosystem + ".so";
        std::string cc_cmd;
        if (ecosystem == "npm") {
            cc_cmd = "\"" + cc.path + "\" -shared -fPIC -o \"" + so_path + "\" \""
                   + wrapper_file + "\" \""
                   + qjs_abs + "/quickjs.c\" \""
                   + qjs_abs + "/cutils.c\" \""
                   + qjs_abs + "/libregexp.c\" \""
                   + qjs_abs + "/libunicode.c\" \""
                   + qjs_abs + "/dtoa.c\""
                   + " -I\"" + qjs_abs + "\""
                   + " -include \"" + qjs_abs + "/quickjs_config.h\""
                   + " 2>&1";
        } else {
            cc_cmd = "\"" + cc.path + "\" -shared -fPIC -o \"" + so_path + "\" \""
                   + wrapper_file + "\" 2>&1";
        }
        int compile_rc = std::system(cc_cmd.c_str());
        if (compile_rc != 0) {
            std::cerr << "[bridge] ERROR: shared library compilation failed (exit " << compile_rc << ")\n";
            return 1;
        }

        std::string dll_check = so_path;
#endif
        if (!fs::exists(dll_check)) {
            std::cerr << "[bridge] ERROR: bridge DLL was not created: " << dll_check << "\n";
            std::cerr << "[bridge]   compilation ran but output file is missing\n";
            return 1;
        }
        std::cout << "[bridge]   ✅ DLL: " << dll_check << "\n";
    }
    } /* end else (not native) */

    /* Print ecosystem name for --auto consumers (e.g. compiler) */
    std::cout << "BRIDGE_ECOSYSTEM=" << ecosystem << "\n";
    std::cout << "BRIDGE_DIR=" << dir << "\n";
    std::cout << "BRIDGE_AU=" << dir << "/" << pkg << ".au\n";
    if (ecosystem == "native") {
        /* Native bridge uses original DLL directly — no wrapper DLL generated */
        std::cout << "BRIDGE_DLL=none\n";
    } else {
#ifdef _WIN32
        std::cout << "BRIDGE_DLL=" << dir << "/" << pkg << "_" << ecosystem << ".dll\n";
#else
        std::cout << "BRIDGE_DLL=" << dir << "/lib" << pkg << "_" << ecosystem << ".so\n";
#endif
    }
    return 0;
}

/* ── Auto-resolve: try pypi → npm → cargo, use first success ── */
int cmd_bridge_auto(const std::string& pkg, const std::string& version) {
    if (pkg.empty()) {
        std::cerr << "[bridge] ERROR: empty package name\n";
        return 1;
    }
    const char* ecosystems[] = {"pypi", "npm", "cargo"};
    for (auto eco : ecosystems) {
        std::cout << "[bridge] trying " << eco << " for " << pkg << "\n";
        int rc = cmd_bridge(eco, pkg, version);
        if (rc == 0) return 0;
    }
    std::cerr << "[bridge] ERROR: could not resolve " << pkg
              << " from any ecosystem (tried pypi, npm, cargo)\n";
    return 1;
}
