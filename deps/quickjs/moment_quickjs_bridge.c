/* QuickJS-based npm bridge DLL for moment — native speed, no subprocess */
#define _GNU_SOURCE
#include "quickjs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

static JSRuntime *g_rt = NULL;
static JSContext *g_ctx = NULL;
static int g_inited = 0;

#define MAX_OBJS 65536
static JSValue g_objs[MAX_OBJS];
static int g_next_id = 2;

static void json_escape(const char *in, char *out, size_t outsz) {
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 6 < outsz; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == '\\' || c == '"')  { out[j++] = '\\'; out[j++] = c; }
        else if (c == '\n')         { out[j++] = '\\'; out[j++] = 'n'; }
        else if (c == '\r')         { out[j++] = '\\'; out[j++] = 'r'; }
        else if (c == '\t')         { out[j++] = '\\'; out[j++] = 't'; }
        else if (c < 32)            { j += snprintf(out + j, outsz - j, "\\u%04x", c); }
        else                         out[j++] = c;
    }
    out[j] = '\0';
}

static void *store_json(const char *json) {
    if (!json) return NULL;
    char **pp = (char **)malloc(sizeof(char *));
    *pp = (char *)malloc(strlen(json) + 1);
    strcpy(*pp, json);
    return (void *)pp;
}

static const char *get_json(void *handle) {
    if (!handle) return "null";
    return *((const char **)handle);
}

static char *jsval_to_json(JSContext *ctx, JSValue val) {
    JSValue json = JS_JSONStringify(ctx, val, JS_NULL, JS_NULL);
    if (JS_IsException(json)) return strdup("null");
    const char *s = JS_ToCString(ctx, json);
    char *r = s ? strdup(s) : strdup("null");
    if (s) JS_FreeCString(ctx, s);
    JS_FreeValue(ctx, json);
    return r;
}

static JSValue js_console_log(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv) {
    for (int i = 0; i < argc; i++) {
        if (i > 0) fputc(' ', stderr);
        const char *s = JS_ToCString(ctx, argv[i]);
        if (s) { fputs(s, stderr); JS_FreeCString(ctx, s); }
    }
    fputc('\n', stderr);
    return JS_UNDEFINED;
}

static JSValue js_process_stdout_write(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv) {
    if (argc > 0) {
        const char *s = JS_ToCString(ctx, argv[0]);
        if (s) { printf("%s", s); fflush(stdout); JS_FreeCString(ctx, s); }
    }
    return JS_UNDEFINED;
}

static JSValue js_require(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv);

static JSValue load_commonjs_module(JSContext *ctx, const char *abs_path,
                                     const char *module_name) {
    FILE *f = fopen(abs_path, "rb");
    if (!f) return JS_ThrowReferenceError(ctx, "Cannot find module '%s'", module_name);
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *code = (char *)malloc((size_t)len + 1);
    if (!code) { fclose(f); return JS_ThrowOutOfMemory(ctx); }
    fread(code, 1, len, f);
    code[len] = '\0';
    fclose(f);

    JSValue exports = JS_NewObject(ctx);
    JSValue mod_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, mod_obj, "exports", JS_DupValue(ctx, exports));

    char dir[1024];
    strncpy(dir, abs_path, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';
    char *last = strrchr(dir, '/');
#ifdef _WIN32
    char *last2 = strrchr(dir, '\\');
    if (last2 > last) last = last2;
#endif
    if (last) *last = '\0';

    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "exports", JS_DupValue(ctx, exports));
    JS_SetPropertyStr(ctx, global, "module", JS_DupValue(ctx, mod_obj));
    JS_SetPropertyStr(ctx, global, "__filename", JS_NewString(ctx, abs_path));
    JS_SetPropertyStr(ctx, global, "__dirname", JS_NewString(ctx, dir));
    JS_FreeValue(ctx, global);

    JSValue result = JS_Eval(ctx, code, (size_t)len, abs_path, JS_EVAL_TYPE_GLOBAL);
    free(code);

    if (JS_IsException(result)) {
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, mod_obj);
        JS_FreeValue(ctx, exports);
        return JS_EXCEPTION;
    }
    JS_FreeValue(ctx, result);

    JSValue final_exports = JS_GetPropertyStr(ctx, mod_obj, "exports");
    JS_FreeValue(ctx, mod_obj);
    JS_FreeValue(ctx, exports);
    return final_exports;
}

static char *resolve_node_module(const char *name, const char *from_dir) {
    static char buf[4096];
    char dir[4096];
    strncpy(dir, from_dir ? from_dir : ".", sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';

    while (1) {
        snprintf(buf, sizeof(buf), "%s/node_modules/%s/package.json", dir, name);
        FILE *f = fopen(buf, "rb");
        if (f) { fclose(f); snprintf(buf, sizeof(buf), "%s/node_modules/%s", dir, name); return buf; }

        snprintf(buf, sizeof(buf), "%s/node_modules/%s.js", dir, name);
        f = fopen(buf, "rb");
        if (f) { fclose(f); snprintf(buf, sizeof(buf), "%s/node_modules/%s.js", dir, name); return buf; }

        char *last = strrchr(dir, '/');
#ifdef _WIN32
        char *last2 = strrchr(dir, '\\');
        if (last2 > last) last = last2;
#endif
        if (!last || last == dir) break;
        *last = '\0';
    }
    return NULL;
}

static JSValue js_require(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "require: missing argument");
    const char *name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_ThrowTypeError(ctx, "require: invalid argument");

    char *module_dir = resolve_node_module(name, ".");
    if (!module_dir) {
        JS_FreeCString(ctx, name);
        return JS_ThrowReferenceError(ctx, "Cannot find module '%s'", name);
    }

    char entry[4096];
    char pkg_json[4096];
    snprintf(pkg_json, sizeof(pkg_json), "%s/package.json", module_dir);
    FILE *f = fopen(pkg_json, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long json_len = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *json_str = (char *)malloc((size_t)json_len + 1);
        fread(json_str, 1, json_len, f);
        json_str[json_len] = '\0';
        fclose(f);

        JSValue pkg_obj = JS_ParseJSON(ctx, json_str, (size_t)json_len, "<package.json>");
        free(json_str);
        if (!JS_IsException(pkg_obj)) {
            JSValue main_val = JS_GetPropertyStr(ctx, pkg_obj, "main");
            if (!JS_IsUndefined(main_val)) {
                const char *main_str = JS_ToCString(ctx, main_val);
                if (main_str) {
                    snprintf(entry, sizeof(entry), "%s/%s", module_dir, main_str);
                    JS_FreeCString(ctx, main_str);
                } else { snprintf(entry, sizeof(entry), "%s/index.js", module_dir); }
                JS_FreeValue(ctx, main_val);
            } else { snprintf(entry, sizeof(entry), "%s/index.js", module_dir); }
            JS_FreeValue(ctx, pkg_obj);
        } else { snprintf(entry, sizeof(entry), "%s/index.js", module_dir); }
    } else {
        if (strstr(module_dir, ".js")) { snprintf(entry, sizeof(entry), "%s", module_dir); }
        else { snprintf(entry, sizeof(entry), "%s/index.js", module_dir); }
    }

    JSValue ret = load_commonjs_module(ctx, entry, name);
    JS_FreeCString(ctx, name);
    return ret;
}

static int init_quickjs(const char *bridge_dir) {
    if (g_inited) return 1;
    g_rt = JS_NewRuntime();
    if (!g_rt) return 0;
    g_ctx = JS_NewContext(g_rt);
    if (!g_ctx) { JS_FreeRuntime(g_rt); g_rt = NULL; return 0; }

    if (bridge_dir) {
#ifdef _WIN32
        SetCurrentDirectoryA(bridge_dir);
#else
        chdir(bridge_dir);
#endif
    }

    JSValue global = JS_GetGlobalObject(g_ctx);

    JSValue console = JS_NewObject(g_ctx);
    JS_SetPropertyStr(g_ctx, console, "log", JS_NewCFunction(g_ctx, js_console_log, "log", 1));
    JS_SetPropertyStr(g_ctx, console, "warn", JS_NewCFunction(g_ctx, js_console_log, "warn", 1));
    JS_SetPropertyStr(g_ctx, console, "error", JS_NewCFunction(g_ctx, js_console_log, "error", 1));
    JS_SetPropertyStr(g_ctx, global, "console", console);

    JSValue process = JS_NewObject(g_ctx);
    JSValue stdout_obj = JS_NewObject(g_ctx);
    JS_SetPropertyStr(g_ctx, stdout_obj, "write", JS_NewCFunction(g_ctx, js_process_stdout_write, "write", 1));
    JS_SetPropertyStr(g_ctx, process, "stdout", stdout_obj);
    JS_SetPropertyStr(g_ctx, global, "process", process);

    JS_SetPropertyStr(g_ctx, global, "require", JS_NewCFunction(g_ctx, js_require, "require", 1));
    JS_FreeValue(g_ctx, global);

    g_inited = 1;
    return 1;
}

static void cleanup_quickjs(void) {
    if (g_ctx) {
        for (int i = 0; i < MAX_OBJS; i++) {
            if (!JS_IsNull(g_objs[i]) && !JS_IsUndefined(g_objs[i]))
                JS_FreeValue(g_ctx, g_objs[i]);
            g_objs[i] = JS_NULL;
        }
        JS_FreeContext(g_ctx);
        g_ctx = NULL;
    }
    if (g_rt) { JS_FreeRuntime(g_rt); g_rt = NULL; }
    g_inited = 0;
    g_next_id = 2;
}

EXPORT void *moment_require(void);
EXPORT void  moment_free(void *handle);
EXPORT void  moment_free_cstr(void *s);
EXPORT void *moment_str(const char *s);
EXPORT void *moment_int(long long v);
EXPORT void *moment_float(double v);
EXPORT char *moment_to_cstr(void *obj);
EXPORT void *moment_tuple2(void *a, void *b);
EXPORT void *moment_tuple3(void *a, void *b, void *c);
EXPORT void *moment_tuple4(void *a, void *b, void *c, void *d);
EXPORT void *moment_tuple(void **items, int count);
EXPORT void *moment_list2(void *a, void *b);
EXPORT void *moment_list3(void *a, void *b, void *c);
EXPORT void *moment_list4(void *a, void *b, void *c, void *d);
EXPORT void *moment_list(void **items, int count);
EXPORT void *moment_dict(void);
EXPORT int   moment_dict_set(void *d, const char *key, void *val);
EXPORT void *moment_call(void *mod, const char *fn, void *args);
EXPORT void *moment_call1(void *mod, const char *fn, void *a);
EXPORT void *moment_call2(void *mod, const char *fn, void *a, void *b);
EXPORT void *moment_call3(void *mod, const char *fn, void *a, void *b, void *c);
EXPORT void *moment_call4(void *mod, const char *fn, void *a, void *b, void *c, void *d);
EXPORT void *moment_call5(void *mod, const char *fn, void *a, void *b, void *c, void *d, void *e);
EXPORT void *moment_call6(void *mod, const char *fn, void *a, void *b, void *c, void *d, void *e, void *f);
EXPORT void *moment_call_kw(void *mod, const char *fn, void *args, void *kwargs);
EXPORT void *moment_getattr(void *obj, const char *name);

static int ensure_moment(void) {
    if (g_inited) return 1;
    if (!init_quickjs("moment_npm_bridge")) return 0;
    for (int i = 0; i < MAX_OBJS; i++) g_objs[i] = JS_NULL;

    JSValue global = JS_GetGlobalObject(g_ctx);
    JSValue req_val = JS_GetPropertyStr(g_ctx, global, "require");
    int ret = 0;
    if (JS_IsFunction(g_ctx, req_val)) {
        JSValue name = JS_NewString(g_ctx, "moment");
        JSValue moment_mod = JS_Call(g_ctx, req_val, JS_UNDEFINED, 1, &name);
        JS_FreeValue(g_ctx, name);
        if (!JS_IsException(moment_mod)) {
            g_objs[1] = JS_DupValue(g_ctx, moment_mod);
            JS_FreeValue(g_ctx, moment_mod);
            ret = 1;
        }
    }
    JS_FreeValue(g_ctx, req_val);
    JS_FreeValue(g_ctx, global);
    g_inited = ret;
    return ret;
}

EXPORT void *moment_require(void) {
    if (!ensure_moment()) return NULL;
    return (void *)(intptr_t)1;
}

EXPORT void moment_free(void *handle) {
    if (!handle || (intptr_t)handle < 2) return;
    int id = (int)(intptr_t)handle;
    if (id < MAX_OBJS) {
        if (!JS_IsNull(g_objs[id]) && !JS_IsUndefined(g_objs[id]))
            JS_FreeValue(g_ctx, g_objs[id]);
        g_objs[id] = JS_NULL;
    }
}

EXPORT void moment_free_cstr(void *s) {
    if (s) free(s);
}

EXPORT void *moment_str(const char *s) {
    if (!s) return store_json("null");
    char escaped[8192];
    json_escape(s, escaped, sizeof(escaped));
    char buf[8194];
    snprintf(buf, sizeof(buf), "\"%s\"", escaped);
    return store_json(buf);
}

EXPORT void *moment_int(long long v) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%lld", v);
    return store_json(buf);
}

EXPORT void *moment_float(double v) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%g", v);
    return store_json(buf);
}

EXPORT char *moment_to_cstr(void *obj) {
    if (!obj) return NULL;
    const char *json = get_json(obj);
    char *out = (char *)malloc(strlen(json) + 1);
    strcpy(out, json);
    return out;
}

EXPORT void *moment_tuple2(void *a, void *b) {
    char buf[16384];
    snprintf(buf, sizeof(buf), "[%s,%s]", get_json(a), get_json(b));
    return store_json(buf);
}

EXPORT void *moment_tuple3(void *a, void *b, void *c) {
    char buf[16384];
    snprintf(buf, sizeof(buf), "[%s,%s,%s]", get_json(a), get_json(b), get_json(c));
    return store_json(buf);
}

EXPORT void *moment_tuple4(void *a, void *b, void *c, void *d) {
    char buf[16384];
    snprintf(buf, sizeof(buf), "[%s,%s,%s,%s]", get_json(a), get_json(b), get_json(c), get_json(d));
    return store_json(buf);
}

EXPORT void *moment_tuple(void **items, int count) {
    if (count <= 0) return store_json("[]");
    char *buf = (char *)malloc(16384);
    if (!buf) return NULL;
    int pos = 0, rem = 16384;
#define TUPLE_APPEND(...) do{int w=snprintf(buf+pos,rem,__VA_ARGS__);if(w>0){pos+=w;if(pos>=16383)pos=16383;rem=16384-pos;}}while(0)
    TUPLE_APPEND("[");
    for (int i = 0; i < count && pos < 16383; i++) {
        if (i > 0) TUPLE_APPEND(",");
        TUPLE_APPEND("%s", get_json(items[i]));
    }
    TUPLE_APPEND("]");
#undef TUPLE_APPEND
    void *h = store_json(buf);
    free(buf);
    return h;
}

EXPORT void *moment_list2(void *a, void *b) { return moment_tuple2(a, b); }
EXPORT void *moment_list3(void *a, void *b, void *c) { return moment_tuple3(a, b, c); }
EXPORT void *moment_list4(void *a, void *b, void *c, void *d) { return moment_tuple4(a, b, c, d); }
EXPORT void *moment_list(void **items, int count) { return moment_tuple(items, count); }
EXPORT void *moment_dict(void) { return store_json("{}"); }

EXPORT int moment_dict_set(void *d, const char *key, void *val) {
    if (!d || !key) return -1;
    char **pp = (char **)d;
    const char *val_json = get_json(val);
    char escaped_key[512];
    json_escape(key, escaped_key, sizeof(escaped_key));
    char entry[2048];
    int elen = snprintf(entry, sizeof(entry), "\"%s\":%s", escaped_key, val_json);
    if (elen < 0) return -1;
    size_t old_len = strlen(*pp);
    if (old_len <= 2) {
        char *new_s = (char *)malloc((size_t)elen + 3);
        snprintf(new_s, (size_t)elen + 3, "{%s}", entry);
        free(*pp); *pp = new_s;
    } else {
        size_t new_len = old_len + (size_t)elen + 2;
        char *new_s = (char *)malloc(new_len);
        snprintf(new_s, new_len, "%.*s,%s}", (int)(old_len - 1), *pp, entry);
        free(*pp); *pp = new_s;
    }
    return 0;
}

static void *qjs_call_impl(void *mod, const char *fn, const char *args_json) {
    if (!mod || !g_ctx || !g_inited) return NULL;
    int obj_id = (int)(intptr_t)mod;
    if (obj_id < 0 || obj_id >= MAX_OBJS) return NULL;
    if (JS_IsNull(g_objs[obj_id]) || JS_IsUndefined(g_objs[obj_id])) return NULL;

    JSValue obj = g_objs[obj_id];
    JSValue func;

    if (!fn || fn[0] == '\0') {
        func = JS_DupValue(g_ctx, obj);
    } else {
        func = JS_GetPropertyStr(g_ctx, obj, fn);
    }

    if (JS_IsException(func)) {
        JS_FreeValue(g_ctx, func);
        return NULL;
    }

    if (!JS_IsFunction(g_ctx, func)) {
        if (JS_IsObject(func)) {
            int new_id = g_next_id++;
            if (new_id >= MAX_OBJS) { JS_FreeValue(g_ctx, func); return NULL; }
            g_objs[new_id] = func;
            return (void *)(intptr_t)new_id;
        }
        char *json = jsval_to_json(g_ctx, func);
        void *h = store_json(json);
        free(json);
        JS_FreeValue(g_ctx, func);
        return h;
    }

    JSValue args_val = JS_NULL;
    JSValue *argv = NULL;
    int argc = 0;

    if (args_json && args_json[0] && strcmp(args_json, "null") != 0) {
        args_val = JS_ParseJSON(g_ctx, args_json, strlen(args_json), "<args>");
        if (!JS_IsException(args_val) && JS_IsArray(g_ctx, args_val)) {
            JSValue len_val = JS_GetPropertyStr(g_ctx, args_val, "length");
            JS_ToInt32(g_ctx, &argc, len_val);
            JS_FreeValue(g_ctx, len_val);
            if (argc > 0) {
                argv = (JSValue *)malloc((size_t)argc * sizeof(JSValue));
                for (int i = 0; i < argc; i++)
                    argv[i] = JS_GetPropertyUint32(g_ctx, args_val, i);
            }
            JS_FreeValue(g_ctx, args_val);
        } else if (!JS_IsException(args_val)) {
            argc = 1;
            argv = (JSValue *)malloc(sizeof(JSValue));
            argv[0] = args_val;
        }
    }

    JSValue result = JS_Call(g_ctx, func, obj, argc, argv);
    JS_FreeValue(g_ctx, func);
    if (argv) {
        for (int i = 0; i < argc; i++) JS_FreeValue(g_ctx, argv[i]);
        free(argv);
    }

    if (JS_IsException(result)) {
        JS_FreeValue(g_ctx, result);
        return NULL;
    }

    if (JS_IsObject(result) || JS_IsFunction(g_ctx, result)) {
        int new_id = g_next_id++;
        if (new_id >= MAX_OBJS) { JS_FreeValue(g_ctx, result); return NULL; }
        g_objs[new_id] = JS_DupValue(g_ctx, result);
        JS_FreeValue(g_ctx, result);
        return (void *)(intptr_t)new_id;
    }

    char *json = jsval_to_json(g_ctx, result);
    void *h = store_json(json);
    free(json);
    JS_FreeValue(g_ctx, result);
    return h;
}

EXPORT void *moment_call(void *mod, const char *fn, void *args) {
    if (!args) return qjs_call_impl(mod, fn, "[]");
    return qjs_call_impl(mod, fn, get_json(args));
}

EXPORT void *moment_call1(void *mod, const char *fn, void *a) {
    char buf[16384];
    snprintf(buf, sizeof(buf), "[%s]", get_json(a));
    return qjs_call_impl(mod, fn, buf);
}

EXPORT void *moment_call2(void *mod, const char *fn, void *a, void *b) {
    char buf[16384];
    snprintf(buf, sizeof(buf), "[%s,%s]", get_json(a), get_json(b));
    return qjs_call_impl(mod, fn, buf);
}

EXPORT void *moment_call3(void *mod, const char *fn, void *a, void *b, void *c) {
    char buf[16384];
    snprintf(buf, sizeof(buf), "[%s,%s,%s]", get_json(a), get_json(b), get_json(c));
    return qjs_call_impl(mod, fn, buf);
}

EXPORT void *moment_call4(void *mod, const char *fn, void *a, void *b, void *c, void *d) {
    char buf[16384];
    snprintf(buf, sizeof(buf), "[%s,%s,%s,%s]", get_json(a), get_json(b), get_json(c), get_json(d));
    return qjs_call_impl(mod, fn, buf);
}

EXPORT void *moment_call5(void *mod, const char *fn, void *a, void *b, void *c, void *d, void *e) {
    char buf[16384];
    snprintf(buf, sizeof(buf), "[%s,%s,%s,%s,%s]", get_json(a), get_json(b), get_json(c), get_json(d), get_json(e));
    return qjs_call_impl(mod, fn, buf);
}

EXPORT void *moment_call6(void *mod, const char *fn, void *a, void *b, void *c, void *d, void *e, void *f) {
    char buf[16384];
    snprintf(buf, sizeof(buf), "[%s,%s,%s,%s,%s,%s]", get_json(a), get_json(b), get_json(c), get_json(d), get_json(e), get_json(f));
    return qjs_call_impl(mod, fn, buf);
}

EXPORT void *moment_call_kw(void *mod, const char *fn, void *args, void *kwargs) {
    const char *args_str = get_json(args);
    const char *kwargs_str = get_json(kwargs);
    size_t alen = strlen(args_str);
    char buf[65536];
    if (alen >= 2 && alen <= 32768) {
        snprintf(buf, sizeof(buf), "%.*s,%s]", (int)(alen - 1), args_str, kwargs_str);
    } else {
        snprintf(buf, sizeof(buf), "[%s]", kwargs_str);
    }
    return qjs_call_impl(mod, fn, buf);
}

EXPORT void *moment_getattr(void *obj, const char *name) {
    if (!obj || !name || !g_ctx || !g_inited) return NULL;
    int obj_id = (int)(intptr_t)obj;
    if (obj_id < 0 || obj_id >= MAX_OBJS) return NULL;
    if (JS_IsNull(g_objs[obj_id]) || JS_IsUndefined(g_objs[obj_id])) return NULL;

    JSValue val = JS_GetPropertyStr(g_ctx, g_objs[obj_id], name);
    if (JS_IsException(val)) { JS_FreeValue(g_ctx, val); return NULL; }

    if (JS_IsObject(val) || JS_IsFunction(g_ctx, val)) {
        int new_id = g_next_id++;
        if (new_id >= MAX_OBJS) { JS_FreeValue(g_ctx, val); return NULL; }
        g_objs[new_id] = JS_DupValue(g_ctx, val);
        JS_FreeValue(g_ctx, val);
        return (void *)(intptr_t)new_id;
    }

    char *json = jsval_to_json(g_ctx, val);
    void *h = store_json(json);
    free(json);
    JS_FreeValue(g_ctx, val);
    return h;
}

#ifdef _WIN32
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_DETACH) cleanup_quickjs();
    return TRUE;
}
#endif
