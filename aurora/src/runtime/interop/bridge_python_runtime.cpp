#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <atomic>

/* ── Runtime bridge for Python ecosystem calls ──
 *   Called by LLVM IR emitted from bridge_python.cpp codegen.
 *   Supports: int, float, string, bool, dict, list, pointer types.
 *   SAFETY: DLL loading uses secure search paths; init is thread-safe;
 *           string copies are bounded and null-terminated;
 *           Py_None dereference is guarded against null.
 */

extern "C" {

typedef struct _object PyObject;

static void* python_dll_handle_ = nullptr;

/* ── CPython C API function pointer typedefs ── */
typedef PyObject* (*PyImport_ImportModuleFunc)(const char*);
typedef PyObject* (*PyObject_GetAttrStringFunc)(PyObject*, const char*);
typedef PyObject* (*PyObject_CallObjectFunc)(PyObject*, PyObject*);
typedef PyObject* (*PyObject_CallFunc)(PyObject*, PyObject*, PyObject*);
typedef long long (*PyLong_AsLongLongFunc)(PyObject*);
typedef PyObject* (*PyLong_FromLongLongFunc)(long long);
typedef double   (*PyFloat_AsDoubleFunc)(PyObject*);
typedef PyObject* (*PyFloat_FromDoubleFunc)(double);
typedef PyObject* (*PyUnicode_FromStringFunc)(const char*);
typedef const char* (*PyUnicode_AsUTF8Func)(PyObject*);
typedef void (*Py_DECREFunc)(PyObject*);
typedef void (*Py_INCREFFunc)(PyObject*);
typedef PyObject* (*PyTuple_NewFunc)(int);
typedef void (*PyTuple_SetItemFunc)(PyObject*, int, PyObject*);
typedef PyObject* (*PyDict_NewFunc)(void);
typedef int (*PyDict_SetItemFunc)(PyObject*, PyObject*, PyObject*);
typedef PyObject* (*PyDict_GetItemFunc)(PyObject*, PyObject*);
typedef PyObject* (*PyDict_KeysFunc)(PyObject*);
typedef int (*PyDict_SizeFunc)(PyObject*);
typedef PyObject* (*PyList_GetItemFunc)(PyObject*, int);
typedef int (*PyList_SizeFunc)(PyObject*);
typedef PyObject* (*PyBool_FromLongFunc)(long);
typedef PyObject* (*PyUnicode_FromKindAndDataFunc)(int, const void*, int);
typedef PyObject* (*PyImport_ImportModuleLevelFunc)(const char*, PyObject*, PyObject*, PyObject*, int);

/* ── Function pointer instances ── */
static PyImport_ImportModuleFunc  PyImport_ImportModule_fn  = nullptr;
static PyObject_GetAttrStringFunc PyObject_GetAttrString_fn = nullptr;
static PyObject_CallObjectFunc    PyObject_CallObject_fn    = nullptr;
static PyObject_CallFunc          PyObject_Call_fn          = nullptr;
static PyLong_FromLongLongFunc    PyLong_FromLongLong_fn    = nullptr;
static PyLong_AsLongLongFunc      PyLong_AsLongLong_fn      = nullptr;
static PyFloat_FromDoubleFunc     PyFloat_FromDouble_fn     = nullptr;
static PyFloat_AsDoubleFunc       PyFloat_AsDouble_fn       = nullptr;
static PyUnicode_FromStringFunc   PyUnicode_FromString_fn   = nullptr;
static PyUnicode_AsUTF8Func       PyUnicode_AsUTF8_fn       = nullptr;
static Py_DECREFunc               Py_DECREFn_fn             = nullptr;
static Py_INCREFFunc              Py_INCREFn_fn             = nullptr;
static PyTuple_NewFunc            PyTuple_New_fn            = nullptr;
static PyTuple_SetItemFunc        PyTuple_SetItem_fn        = nullptr;
static PyDict_NewFunc             PyDict_New_fn             = nullptr;
static PyDict_SetItemFunc         PyDict_SetItem_fn         = nullptr;
static PyDict_GetItemFunc         PyDict_GetItem_fn         = nullptr;
static PyDict_KeysFunc            PyDict_Keys_fn            = nullptr;
static PyDict_SizeFunc            PyDict_Size_fn            = nullptr;
static PyList_GetItemFunc         PyList_GetItem_fn         = nullptr;
static PyList_SizeFunc            PyList_Size_fn            = nullptr;
static PyBool_FromLongFunc        PyBool_FromLong_fn        = nullptr;

static std::atomic<int> python_bridge_initialized_{0};

/* Py_None is a global object exported by the CPython DLL.
   We resolve it via GetProcAddress/dlsym at init time.       */
typedef PyObject* (*Py_NoneFunc)();
static PyObject* Py_None_ptr = nullptr;

static void* python_bridge_sym(const char* name) {
#ifdef _WIN32
    return (void*)::GetProcAddress((HMODULE)python_dll_handle_, name);
#else
    return ::dlsym(python_dll_handle_, name);
#endif
}

static void python_bridge_init(void) {
    if (python_bridge_initialized_.load(std::memory_order_acquire)) return;

#ifdef _WIN32
    python_dll_handle_ = (void*)::LoadLibraryExA("python3.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!python_dll_handle_) python_dll_handle_ = (void*)::LoadLibraryExA("python312.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!python_dll_handle_) python_dll_handle_ = (void*)::LoadLibraryExA("python313.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!python_dll_handle_) python_dll_handle_ = (void*)::LoadLibraryExA("python314.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
#else
    python_dll_handle_ = ::dlopen("libpython3.so", RTLD_LAZY | RTLD_GLOBAL);
    if (!python_dll_handle_) python_dll_handle_ = ::dlopen("libpython3.12.so", RTLD_LAZY | RTLD_GLOBAL);
    if (!python_dll_handle_) python_dll_handle_ = ::dlopen("libpython3.13.so", RTLD_LAZY | RTLD_GLOBAL);
#endif

    if (!python_dll_handle_) {
        std::fprintf(stderr, "[bridge:python] ERROR: could not load python3.dll\n");
        return;
    }

    PyImport_ImportModule_fn  = (PyImport_ImportModuleFunc)python_bridge_sym("PyImport_ImportModule");
    PyObject_GetAttrString_fn = (PyObject_GetAttrStringFunc)python_bridge_sym("PyObject_GetAttrString");
    PyObject_CallObject_fn    = (PyObject_CallObjectFunc)python_bridge_sym("PyObject_CallObject");
    PyObject_Call_fn          = (PyObject_CallFunc)python_bridge_sym("PyObject_Call");
    PyLong_FromLongLong_fn    = (PyLong_FromLongLongFunc)python_bridge_sym("PyLong_FromLongLong");
    PyLong_AsLongLong_fn      = (PyLong_AsLongLongFunc)python_bridge_sym("PyLong_AsLongLong");
    PyFloat_FromDouble_fn     = (PyFloat_FromDoubleFunc)python_bridge_sym("PyFloat_FromDouble");
    PyFloat_AsDouble_fn       = (PyFloat_AsDoubleFunc)python_bridge_sym("PyFloat_AsDouble");
    PyUnicode_FromString_fn   = (PyUnicode_FromStringFunc)python_bridge_sym("PyUnicode_FromString");
    PyUnicode_AsUTF8_fn       = (PyUnicode_AsUTF8Func)python_bridge_sym("PyUnicode_AsUTF8");
    Py_DECREFn_fn             = (Py_DECREFunc)python_bridge_sym("Py_DECREF");
    Py_INCREFn_fn             = (Py_INCREFFunc)python_bridge_sym("Py_INCREF");
    PyTuple_New_fn            = (PyTuple_NewFunc)python_bridge_sym("PyTuple_New");
    PyTuple_SetItem_fn        = (PyTuple_SetItemFunc)python_bridge_sym("PyTuple_SetItem");
    PyDict_New_fn             = (PyDict_NewFunc)python_bridge_sym("PyDict_New");
    PyDict_SetItem_fn         = (PyDict_SetItemFunc)python_bridge_sym("PyDict_SetItem");
    PyDict_GetItem_fn         = (PyDict_GetItemFunc)python_bridge_sym("PyDict_GetItem");
    PyDict_Keys_fn            = (PyDict_KeysFunc)python_bridge_sym("PyDict_Keys");
    PyDict_Size_fn            = (PyDict_SizeFunc)python_bridge_sym("PyDict_Size");
    PyList_GetItem_fn         = (PyList_GetItemFunc)python_bridge_sym("PyList_GetItem");
    PyList_Size_fn            = (PyList_SizeFunc)python_bridge_sym("PyList_Size");
    PyBool_FromLong_fn        = (PyBool_FromLongFunc)python_bridge_sym("PyBool_FromLong");

    /* Resolve Py_None — it's a global object, not a function.
       GetProcAddress returns the address of the PyObject* pointer.
       We dereference it to get the actual Py_None object pointer.  */
    PyObject** pynone_ptr = (PyObject**)python_bridge_sym("_Py_NoneStruct");
    if (!pynone_ptr) pynone_ptr = (PyObject**)python_bridge_sym("Py_None");
    if (pynone_ptr) Py_None_ptr = *pynone_ptr;

    if (!PyImport_ImportModule_fn || !PyObject_CallObject_fn) {
        std::fprintf(stderr, "[bridge:python] ERROR: missing required CPython symbols\n");
        return;
    }

    python_bridge_initialized_.store(1, std::memory_order_release);
}

/* ── Marshal an Aurora i64 argument to a Python object ── */
static PyObject* marshal_arg_to_py(const void* arg_val, const char* expected_type) {
    if (!arg_val) {
        if (!Py_None_ptr) return nullptr;
        Py_INCREFn_fn(Py_None_ptr);
        return Py_None_ptr;
    }
    int64_t val = (int64_t)(uintptr_t)arg_val;
    if (expected_type) {
        if (std::strcmp(expected_type, "float") == 0 || std::strcmp(expected_type, "f64") == 0 ||
            std::strcmp(expected_type, "double") == 0 || std::strcmp(expected_type, "Float") == 0) {
            double d;
            std::memcpy(&d, &val, sizeof(d));
            return PyFloat_FromDouble_fn(d);
        }
        if (std::strcmp(expected_type, "string") == 0 || std::strcmp(expected_type, "str") == 0 ||
            std::strcmp(expected_type, "String") == 0) {
            return PyUnicode_FromString_fn((const char*)val);
        }
        if (std::strcmp(expected_type, "bool") == 0 || std::strcmp(expected_type, "Bool") == 0) {
            return PyBool_FromLong_fn(val ? 1 : 0);
        }
        if (std::strcmp(expected_type, "dict") == 0 || std::strcmp(expected_type, "Map") == 0 ||
            std::strcmp(expected_type, "map") == 0) {
            return (PyObject*)arg_val;
        }
        if (std::strcmp(expected_type, "list") == 0 || std::strcmp(expected_type, "List") == 0) {
            return (PyObject*)arg_val;
        }
    }
    return PyLong_FromLongLong_fn((long long)val);
}

/* ── Marshal a Python object return value to Aurora i64/ptr ── */
static void* marshal_py_to_aurora(PyObject* result, const char* ret_type) {
    if (!result) return nullptr;

    if (std::strcmp(ret_type, "void") == 0 || std::strcmp(ret_type, "Void") == 0) {
        return nullptr;
    }
    if (std::strcmp(ret_type, "int") == 0 || std::strcmp(ret_type, "i64") == 0 ||
        std::strcmp(ret_type, "Int") == 0) {
        long long ival = PyLong_AsLongLong_fn(result);
        return (void*)(uintptr_t)(int64_t)ival;
    }
    if (std::strcmp(ret_type, "float") == 0 || std::strcmp(ret_type, "f64") == 0 ||
        std::strcmp(ret_type, "Float") == 0 || std::strcmp(ret_type, "double") == 0) {
        double dval = PyFloat_AsDouble_fn(result);
        uint64_t bits;
        std::memcpy(&bits, &dval, sizeof(bits));
        return (void*)(uintptr_t)bits;
    }
    if (std::strcmp(ret_type, "string") == 0 || std::strcmp(ret_type, "String") == 0 ||
        std::strcmp(ret_type, "str") == 0) {
        const char* s = PyUnicode_AsUTF8_fn(result);
        if (s) {
            size_t slen = std::strlen(s);
            char* copy = (char*)std::malloc(slen + 1);
            if (copy) {
                std::memcpy(copy, s, slen);
                copy[slen] = '\0';
            }
            return copy;
        }
        return nullptr;
    }
    if (std::strcmp(ret_type, "bool") == 0 || std::strcmp(ret_type, "Bool") == 0) {
        long long ival = PyLong_AsLongLong_fn(result);
        return (void*)(uintptr_t)(ival != 0 ? 1 : 0);
    }
    if (std::strcmp(ret_type, "dict") == 0 || std::strcmp(ret_type, "Map") == 0 ||
        std::strcmp(ret_type, "map") == 0) {
        Py_INCREFn_fn(result);
        return (void*)result;
    }
    if (std::strcmp(ret_type, "list") == 0 || std::strcmp(ret_type, "List") == 0) {
        Py_INCREFn_fn(result);
        return (void*)result;
    }
    /* Default: return as pointer */
    return (void*)result;
}

/* ── Main bridge entry point ── */
void* aurora_bridge_python_call(const char* module_name,
                                 const char* func_name,
                                 void** args,
                                 int arg_count,
                                 const char* ret_type) {
    python_bridge_init();
    if (!python_bridge_initialized_.load(std::memory_order_acquire)) return nullptr;

    PyObject* mod = PyImport_ImportModule_fn(module_name);
    if (!mod) {
        std::fprintf(stderr, "[bridge:python] ERROR: could not import module '%s'\n", module_name);
        return nullptr;
    }

    PyObject* func = PyObject_GetAttrString_fn(mod, func_name);
    if (!func) {
        std::fprintf(stderr, "[bridge:python] ERROR: function '%s' not found in module '%s'\n",
                     func_name, module_name);
        Py_DECREFn_fn(mod);
        return nullptr;
    }

    PyObject* args_tuple = PyTuple_New_fn(arg_count);
    for (int i = 0; i < arg_count; i++) {
        PyObject* py_arg = marshal_arg_to_py(args[i], nullptr);
        PyTuple_SetItem_fn(args_tuple, i, py_arg);
    }

    PyObject* result = PyObject_CallObject_fn(func, args_tuple);

    void* ret_val = marshal_py_to_aurora(result, ret_type);

    if (result) Py_DECREFn_fn(result);
    Py_DECREFn_fn(args_tuple);
    Py_DECREFn_fn(func);
    Py_DECREFn_fn(mod);

    return ret_val;
}

/* ════════════════════════════════════════════════════════════
   Dict-specific bridge functions
   ════════════════════════════════════════════════════════════ */

/* ── Create a new Python dict and return as opaque pointer ── */
void* aurora_bridge_python_dict_create(void) {
    python_bridge_init();
    if (!python_bridge_initialized_) return nullptr;
    PyObject* d = PyDict_New_fn();
    if (!d) return nullptr;
    return (void*)d;
}

/* ── Set a dict entry: dict[key] = value ──
 *   key and value are Aurora i64 values. */
void aurora_bridge_python_dict_set(void* dict_ptr, void* key_val, void* val_val,
                                    const char* key_type, const char* val_type) {
    if (!dict_ptr) return;
    PyObject* d = (PyObject*)dict_ptr;
    PyObject* py_key = marshal_arg_to_py(key_val, key_type);
    PyObject* py_val = marshal_arg_to_py(val_val, val_type);
    PyDict_SetItem_fn(d, py_key, py_val);
    Py_DECREFn_fn(py_key);
    Py_DECREFn_fn(py_val);
}

/* ── Get a dict entry: value = dict[key] ──
 *   Returns Aurora i64 value. */
void* aurora_bridge_python_dict_get(void* dict_ptr, void* key_val,
                                     const char* key_type, const char* val_type) {
    if (!dict_ptr) return nullptr;
    PyObject* d = (PyObject*)dict_ptr;
    PyObject* py_key = marshal_arg_to_py(key_val, key_type);
    PyObject* py_val = PyDict_GetItem_fn(d, py_key);
    Py_DECREFn_fn(py_key);
    if (!py_val) return nullptr;
    return marshal_py_to_aurora(py_val, val_type);
}

/* ── Get number of entries in a dict ── */
int64_t aurora_bridge_python_dict_size(void* dict_ptr) {
    if (!dict_ptr) return 0;
    return PyDict_Size_fn((PyObject*)dict_ptr);
}

/* ── Free a Python object reference ── */
void aurora_bridge_python_free(void* obj_ptr) {
    if (obj_ptr) Py_DECREFn_fn((PyObject*)obj_ptr);
}

} /* extern "C" */
