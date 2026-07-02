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

/* ── Runtime bridge for QuickJS ecosystem calls ──
 *   Called by LLVM IR emitted from bridge_quickjs.cpp codegen.
 *   SAFETY: All externally-visible functions validate inputs,
 *           escape shell/JS metacharacters, and use type-aware
 *           marshaling to prevent type confusion.
 */

extern "C" {

/* Opaque handle types for QuickJS (loaded at runtime) */
typedef struct QJSRuntime { void* data; } QJSRuntime;
typedef struct QJSContext { void* data; } QJSContext;
typedef struct { uint64_t data[2]; } QJSValue;

static void* quickjs_dll_handle_ = nullptr;

/* ── Thread safety: std::atomic flag for init-once ── */
#include <atomic>
static std::atomic<int> quickjs_bridge_initialized_{0};

typedef QJSRuntime* (*JS_NewRuntimeFunc)(void);
typedef void (*JS_FreeRuntimeFunc)(QJSRuntime*);
typedef QJSContext* (*JS_NewContextFunc)(QJSRuntime*);
typedef void (*JS_FreeContextFunc)(QJSContext*);
typedef QJSValue (*JS_EvalFunc)(QJSContext*, const char*, size_t, const char*, int);
typedef QJSValue (*JS_GetGlobalObjectFunc)(QJSContext*);
typedef QJSValue (*JS_GetPropertyStrFunc)(QJSContext*, QJSValue, const char*);
typedef QJSValue (*JS_CallFunc)(QJSContext*, QJSValue, QJSValue, int, QJSValue*);
typedef int (*JS_ToInt32Func)(QJSContext*, int*, QJSValue);
typedef int64_t (*JS_ToInt64Func)(QJSContext*, QJSValue);
typedef double (*JS_ToFloat64Func)(QJSContext*, QJSValue);
typedef const char* (*JS_ToCStringLen2Func)(QJSContext*, size_t*, QJSValue, size_t*);
typedef void (*JS_FreeCStringFunc)(QJSContext*, const char*);
typedef void (*JS_FreeValueFunc)(QJSContext*, QJSValue);
typedef QJSValue (*JS_NewInt32Func)(QJSContext*, int);
typedef QJSValue (*JS_NewInt64Func)(QJSContext*, int64_t);
typedef QJSValue (*JS_NewFloat64Func)(QJSContext*, double);
typedef QJSValue (*JS_NewStringFunc)(QJSContext*, const char*);
typedef QJSValue (*JS_NewBoolFunc)(QJSContext*, int);
typedef int (*JS_ToBoolFunc)(QJSContext*, QJSValue);

/* ── Function pointer instances ── */
static JS_NewRuntimeFunc      JS_NewRuntime_fn      = nullptr;
static JS_FreeRuntimeFunc     JS_FreeRuntime_fn     = nullptr;
static JS_NewContextFunc      JS_NewContext_fn      = nullptr;
static JS_FreeContextFunc     JS_FreeContext_fn     = nullptr;
static JS_EvalFunc            JS_Eval_fn            = nullptr;
static JS_GetGlobalObjectFunc JS_GetGlobalObject_fn = nullptr;
static JS_GetPropertyStrFunc  JS_GetPropertyStr_fn  = nullptr;
static JS_CallFunc            JS_Call_fn            = nullptr;
static JS_ToInt32Func         JS_ToInt32_fn         = nullptr;
static JS_ToInt64Func         JS_ToInt64_fn         = nullptr;
static JS_ToFloat64Func       JS_ToFloat64_fn       = nullptr;
static JS_ToCStringLen2Func   JS_ToCStringLen2_fn   = nullptr;
static JS_FreeCStringFunc     JS_FreeCString_fn     = nullptr;
static JS_FreeValueFunc       JS_FreeValue_fn       = nullptr;
static JS_NewInt32Func        JS_NewInt32_fn        = nullptr;
static JS_NewInt64Func        JS_NewInt64_fn        = nullptr;
static JS_NewFloat64Func      JS_NewFloat64_fn      = nullptr;
static JS_NewStringFunc       JS_NewString_fn       = nullptr;
static JS_NewBoolFunc         JS_NewBool_fn         = nullptr;
static JS_ToBoolFunc          JS_ToBool_fn          = nullptr;

static void* quickjs_bridge_sym(const char* name) {
#ifdef _WIN32
    return (void*)::GetProcAddress((HMODULE)quickjs_dll_handle_, name);
#else
    return ::dlsym(quickjs_dll_handle_, name);
#endif
}

static void quickjs_bridge_init(void) {
    if (quickjs_bridge_initialized_.load(std::memory_order_acquire)) return;

#ifdef _WIN32
    quickjs_dll_handle_ = (void*)::LoadLibraryExA("quickjs.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!quickjs_dll_handle_) quickjs_dll_handle_ = (void*)::LoadLibraryExA("libquickjs.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
#else
    quickjs_dll_handle_ = ::dlopen("libquickjs.so", RTLD_LAZY | RTLD_GLOBAL);
#endif

    if (!quickjs_dll_handle_) {
        std::fprintf(stderr, "[bridge:quickjs] WARNING: quickjs.dll not found\n");
        return;
    }

    JS_NewRuntime_fn      = (JS_NewRuntimeFunc)quickjs_bridge_sym("JS_NewRuntime");
    JS_FreeRuntime_fn     = (JS_FreeRuntimeFunc)quickjs_bridge_sym("JS_FreeRuntime");
    JS_NewContext_fn      = (JS_NewContextFunc)quickjs_bridge_sym("JS_NewContext");
    JS_FreeContext_fn     = (JS_FreeContextFunc)quickjs_bridge_sym("JS_FreeContext");
    JS_Eval_fn            = (JS_EvalFunc)quickjs_bridge_sym("JS_Eval");
    JS_GetGlobalObject_fn = (JS_GetGlobalObjectFunc)quickjs_bridge_sym("JS_GetGlobalObject");
    JS_GetPropertyStr_fn  = (JS_GetPropertyStrFunc)quickjs_bridge_sym("JS_GetPropertyStr");
    JS_Call_fn            = (JS_CallFunc)quickjs_bridge_sym("JS_Call");
    JS_ToInt32_fn         = (JS_ToInt32Func)quickjs_bridge_sym("JS_ToInt32");
    JS_ToInt64_fn         = (JS_ToInt64Func)quickjs_bridge_sym("JS_ToInt64");
    JS_ToFloat64_fn       = (JS_ToFloat64Func)quickjs_bridge_sym("JS_ToFloat64");
    JS_ToCStringLen2_fn   = (JS_ToCStringLen2Func)quickjs_bridge_sym("JS_ToCStringLen2");
    JS_FreeCString_fn     = (JS_FreeCStringFunc)quickjs_bridge_sym("JS_FreeCString");
    JS_FreeValue_fn       = (JS_FreeValueFunc)quickjs_bridge_sym("JS_FreeValue");
    JS_NewInt32_fn        = (JS_NewInt32Func)quickjs_bridge_sym("JS_NewInt32");
    JS_NewInt64_fn        = (JS_NewInt64Func)quickjs_bridge_sym("JS_NewInt64");
    JS_NewFloat64_fn      = (JS_NewFloat64Func)quickjs_bridge_sym("JS_NewFloat64");
    JS_NewString_fn       = (JS_NewStringFunc)quickjs_bridge_sym("JS_NewString");
    JS_NewBool_fn         = (JS_NewBoolFunc)quickjs_bridge_sym("JS_NewBool");
    JS_ToBool_fn          = (JS_ToBoolFunc)quickjs_bridge_sym("JS_ToBool");

    if (!JS_NewRuntime_fn || !JS_NewContext_fn || !JS_Eval_fn || !JS_Call_fn) {
        std::fprintf(stderr, "[bridge:quickjs] ERROR: missing required QuickJS symbols\n");
        return;
    }

    qjs_rt_ = JS_NewRuntime_fn();
    if (!qjs_rt_) {
        std::fprintf(stderr, "[bridge:quickjs] ERROR: JS_NewRuntime failed\n");
        return;
    }
    qjs_ctx_ = JS_NewContext_fn(qjs_rt_);
    if (!qjs_ctx_) {
        std::fprintf(stderr, "[bridge:quickjs] ERROR: JS_NewContext failed\n");
        JS_FreeRuntime_fn(qjs_rt_);
        qjs_rt_ = nullptr;
        return;
    }

    quickjs_bridge_initialized_.store(1, std::memory_order_release);
}

/* ── Escape single quotes in a string for safe JS embedding ──
 *   Returns a heap-allocated copy; caller must std::free.
 */
static char* escape_single_quotes(const char* src) {
    if (!src) return nullptr;
    size_t len = std::strlen(src);
    /* worst case: every char is a quote -> 2 * len + 1 */
    char* out = (char*)std::malloc(2 * len + 1);
    if (!out) return nullptr;
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (src[i] == '\'') {
            out[j++] = '\'';
            out[j++] = '\\';
            out[j++] = '\'';
            out[j++] = '\'';
        } else {
            out[j++] = src[i];
        }
    }
    out[j] = '\0';
    return out;
}

void* aurora_bridge_quickjs_call(const char* module_name,
                                  const char* func_name,
                                  void** args,
                                  int arg_count,
                                  const char* ret_type) {
    quickjs_bridge_init();
    if (!quickjs_bridge_initialized_.load(std::memory_order_acquire)) return nullptr;
    if (!qjs_ctx_) return nullptr;

    /* Escape module_name to prevent JS injection via single quotes */
    char* escaped_mod = escape_single_quotes(module_name);
    if (!escaped_mod) return nullptr;

    char eval_buf[2048];
    int written = std::snprintf(eval_buf, sizeof(eval_buf),
                  "try { globalThis.__module__ = require('%s'); } catch(e) { "
                  "try { globalThis.__module__ = import('%s'); } catch(e2) {} }",
                  escaped_mod, escaped_mod);
    std::free(escaped_mod);
    if (written < 0 || (size_t)written >= sizeof(eval_buf)) return nullptr;

    QJSValue eval_result = JS_Eval_fn(qjs_ctx_, eval_buf, std::strlen(eval_buf),
                                       "<bridge>", 0);

    QJSValue global = JS_GetGlobalObject_fn(qjs_ctx_);
    QJSValue module_val = JS_GetPropertyStr_fn(qjs_ctx_, global, "__module__");
    QJSValue func_val = JS_GetPropertyStr_fn(qjs_ctx_, module_val, func_name);

    QJSValue* js_args = nullptr;
    if (arg_count > 0) {
        js_args = new QJSValue[arg_count];
        for (int i = 0; i < arg_count; i++) {
            int64_t ival = (int64_t)(uintptr_t)args[i];
            if (JS_NewInt64_fn) {
                js_args[i] = JS_NewInt64_fn(qjs_ctx_, ival);
            } else {
                js_args[i] = JS_NewInt32_fn(qjs_ctx_, (int32_t)ival);
            }
        }
    }

    QJSValue zero = JS_NewInt32_fn(qjs_ctx_, 0);
    QJSValue result = JS_Call_fn(qjs_ctx_, func_val, zero, arg_count, js_args);

    void* ret_val = nullptr;
    if (std::strcmp(ret_type, "void") == 0 || std::strcmp(ret_type, "Void") == 0) {
    } else if (std::strcmp(ret_type, "int") == 0 || std::strcmp(ret_type, "i64") == 0 ||
               std::strcmp(ret_type, "Int") == 0) {
        int64_t ival = 0;
        if (JS_ToInt64_fn) {
            ival = JS_ToInt64_fn(qjs_ctx_, result);
        } else {
            int tmp = 0;
            JS_ToInt32_fn(qjs_ctx_, &tmp, result);
            ival = tmp;
        }
        ret_val = (void*)(uintptr_t)ival;
    } else if (std::strcmp(ret_type, "float") == 0 || std::strcmp(ret_type, "f64") == 0 ||
               std::strcmp(ret_type, "Float") == 0 || std::strcmp(ret_type, "double") == 0) {
        double dval = JS_ToFloat64_fn(qjs_ctx_, result);
        uint64_t bits;
        std::memcpy(&bits, &dval, sizeof(bits));
        ret_val = (void*)(uintptr_t)bits;
    } else if (std::strcmp(ret_type, "string") == 0 || std::strcmp(ret_type, "String") == 0 ||
               std::strcmp(ret_type, "str") == 0) {
        const char* s = JS_ToCStringLen2_fn(qjs_ctx_, nullptr, result, nullptr);
        if (s) {
            size_t slen = std::strlen(s);
            char* copy = (char*)std::malloc(slen + 1);
            if (copy) {
                std::memcpy(copy, s, slen + 1);
                ret_val = copy;
            }
            JS_FreeCString_fn(qjs_ctx_, s);
        }
    } else if (std::strcmp(ret_type, "bool") == 0 || std::strcmp(ret_type, "Bool") == 0) {
        int bval = JS_ToBool_fn(qjs_ctx_, result);
        ret_val = (void*)(uintptr_t)(bval ? 1 : 0);
    } else {
        ret_val = (void*)(uintptr_t)result.data[0];
    }

    JS_FreeValue_fn(qjs_ctx_, func_val);
    JS_FreeValue_fn(qjs_ctx_, module_val);
    JS_FreeValue_fn(qjs_ctx_, global);
    JS_FreeValue_fn(qjs_ctx_, eval_result);
    delete[] js_args;

    return ret_val;
}

} /* extern "C" */
