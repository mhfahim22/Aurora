#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <ctime>
#include <thread>
#include <chrono>
#include <vector>
#include <queue>
#include <mutex>
#include <string>
#include "runtime/string.hpp"
#include "runtime/async.hpp"
#include "runtime/memory.hpp"
#include "runtime/reflection.hpp"
#include "common/platform.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#include <shlwapi.h>
#include <psapi.h>
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "psapi.lib")
#else
#include <unistd.h>
#include <sys/utsname.h>
#endif

extern "C" {
    void aurora_panic(const char* msg);
    AuroraStr* aurora_str_upper(const char* str);
    AuroraStr* aurora_str_lower(const char* str);
    int64_t aurora_str_contains(const char* str, const char* sub);
    int64_t aurora_str_index(const char* str, const char* sub);
    int64_t aurora_strlen(const char* str);
    int64_t aurora_time();
    void aurora_sleep(int64_t ms);
    int64_t aurora_random();
    double aurora_math_sqrt(double x);
    double aurora_math_abs(double x);
    double aurora_math_floor(double x);
    double aurora_math_ceil(double x);
    double aurora_math_round(double x);
    double aurora_math_pow(double x, double y);
    double aurora_math_random();
    int64_t aurora_math_random_int(int64_t min, int64_t max);
    char* aurora_fs_read_file(const char* path);
    int aurora_fs_write_file(const char* path, const char* content);
    int aurora_fs_exists(const char* path);
    int aurora_fs_copy(const char* src, const char* dst);
    int aurora_fs_remove(const char* path);
    int aurora_fs_append_file(const char* path, const char* content);
    int aurora_fs_rename(const char* old, const char* newname);
    int aurora_fs_mkdir(const char* path);
    int aurora_fs_is_dir(const char* path);
    char* aurora_fs_dirname(const char* path);
    char* aurora_fs_basename(const char* path);
    int64_t aurora_fs_size(const char* path);
    int64_t aurora_array_len(int64_t arr_ptr);
    int64_t aurora_array_get_int(int64_t arr_ptr, int64_t idx);
    double aurora_array_get_float(int64_t arr_ptr, int64_t idx);
    const char* aurora_array_get_str(int64_t arr_ptr, int64_t idx);
    int64_t aurora_array_get_tag(int64_t arr_ptr, int64_t idx);
    void aurora_array_set_int(int64_t arr_ptr, int64_t idx, int64_t val);
    void aurora_array_set_float(int64_t arr_ptr, int64_t idx, double val);
    void aurora_array_set_str(int64_t arr_ptr, int64_t idx, const char* str);
    int64_t aurora_array_new(int64_t cap);
    void aurora_array_push_int(int64_t arr_ptr, int64_t val);
    void aurora_array_push_float(int64_t arr_ptr, double val);
    void aurora_array_push_str(int64_t arr_ptr, const char* str);
    void aurora_array_push_array(int64_t arr_ptr, int64_t nested_ptr);
    int64_t aurora_array_contains_int(int64_t arr_ptr, int64_t val);
    int64_t aurora_array_copy(int64_t src_ptr);
    char* aurora_json_serialize(void* val);
    void* aurora_json_parse(const char* str);
    int aurora_net_http_get(const char* url, char* buffer, int buffer_size);
    int aurora_net_http_post(const char* url, const char* body, const char* content_type, char* buffer, int buffer_size);
    void aurora_print_str(const char* str);
    void aurora_print_int(int64_t val);
    /* defined in ai_gen.cpp inside extern "C" */
    char* model_generate(struct Model* m, int64_t tokenizer_ptr, const char* text, int64_t max_toks);
    /* these have C linkage (declared extern "C" in ai_common.h) */
    struct Model;
    extern Model* g_active_model;
    extern int64_t g_active_tokenizer;
}

/* g_max_tokens has C++ linkage (not in any extern "C" block) */
extern int64_t g_max_tokens;


extern "C" {

/* ════════════════════════════════════════════════════════════
   String Built-ins
   ════════════════════════════════════════════════════════════ */

static const char* aurora_str_ptr(const void* s) {
    return s ? ((const AuroraStr*)s)->ptr : nullptr;
}

static int64_t aurora_str_length(const void* s) {
    return s ? (int64_t)((const AuroraStr*)s)->len : 0;
}

AuroraStr* builtin_upper(const void* str) {
    const char* cstr = aurora_str_ptr(str);
    return aurora_str_upper(cstr ? cstr : "");
}

AuroraStr* builtin_lower(const void* str) {
    const char* cstr = aurora_str_ptr(str);
    return aurora_str_lower(cstr ? cstr : "");
}

static int is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

AuroraStr* builtin_trim(const void* str) {
    const char* s = aurora_str_ptr(str);
    if (!s) return aurora_str_new(0);
    int64_t len = (int64_t)strlen(s);
    int64_t start = 0;
    while (start < len && is_ws(s[start])) start++;
    int64_t end = len - 1;
    while (end > start && is_ws(s[end])) end--;
    int64_t new_len = end - start + 1;
    if (new_len <= 0) return aurora_str_new(0);
    size_t cap = (size_t)new_len + 1;
    if (cap < 16) cap = 16;
    AuroraStr* result = aurora_str_new(cap);
    for (int64_t i = 0; i < new_len; i++)
        result->ptr[i] = s[start + i];
    result->ptr[new_len] = '\0';
    result->len = (size_t)new_len;
    return result;
}

AuroraStr* builtin_replace(const void* str, const void* old_a, const void* new_a) {
    const char* s = aurora_str_ptr(str);
    const char* old_str = aurora_str_ptr(old_a);
    const char* new_str = aurora_str_ptr(new_a);
    if (!s || !old_str || !new_str) return aurora_str_new(0);
    int64_t slen = (int64_t)strlen(s);
    int64_t olen = (int64_t)strlen(old_str);
    int64_t nlen = (int64_t)strlen(new_str);
    if (olen == 0) return aurora_str_from_cstr(s);
    int64_t count = 0;
    for (int64_t i = 0; i <= slen - olen; ) {
        bool match = true;
        for (int64_t j = 0; j < olen; j++) {
            if (s[i + j] != old_str[j]) { match = false; break; }
        }
        if (match) { count++; i += olen; }
        else i++;
    }
    int64_t buf_len = slen + count * (nlen - olen) + 1;
    size_t cap = (size_t)buf_len;
    if (cap < 16) cap = 16;
    AuroraStr* result = aurora_str_new(cap);
    int64_t pos = 0;
    for (int64_t i = 0; i < slen; ) {
        bool match = true;
        for (int64_t j = 0; j < olen; j++) {
            if (i + j >= slen || s[i + j] != old_str[j]) { match = false; break; }
        }
        if (match && olen > 0) {
            for (int64_t j = 0; j < nlen; j++)
                result->ptr[pos++] = new_str[j];
            i += olen;
        } else {
            result->ptr[pos++] = s[i++];
        }
    }
    result->ptr[pos] = '\0';
    result->len = (size_t)pos;
    return result;
}

int64_t builtin_split(const void* str, const void* delim_a) {
    const char* s = aurora_str_ptr(str);
    const char* delim = aurora_str_ptr(delim_a);
    if (!s) return 0;
    int64_t slen = (int64_t)strlen(s);
    int64_t dlen = delim ? (int64_t)strlen(delim) : 0;
    if (dlen == 0) {
        int64_t arr = aurora_array_new(slen);
        for (int64_t i = 0; i < slen; i++) {
            char buf[2] = { s[i], '\0' };
            aurora_array_push_str(arr, buf);
        }
        return arr;
    }
    int64_t parts = 1;
    for (int64_t i = 0; i <= slen - dlen; i++) {
        bool match = true;
        for (int64_t j = 0; j < dlen; j++) {
            if (s[i + j] != delim[j]) { match = false; break; }
        }
        if (match) { parts++; i += dlen - 1; }
    }
    int64_t arr = aurora_array_new(parts);
    int64_t start = 0;
    for (int64_t i = 0; i <= slen - dlen; ) {
        bool match = true;
        for (int64_t j = 0; j < dlen; j++) {
            if (s[i + j] != delim[j]) { match = false; break; }
        }
        if (match) {
            int64_t seg_len = i - start;
            char* buf = (char*)malloc((size_t)seg_len + 1);
            if (buf) {
                for (int64_t k = 0; k < seg_len; k++) buf[k] = s[start + k];
                buf[seg_len] = '\0';
                aurora_array_push_str(arr, buf);
                free(buf);
            }
            start = i + dlen;
            i += dlen;
        } else {
            i++;
        }
    }
    int64_t seg_len = slen - start;
    if (seg_len >= 0) {
        char* buf = (char*)malloc((size_t)seg_len + 1);
        if (buf) {
            for (int64_t k = 0; k < seg_len; k++) buf[k] = s[start + k];
            buf[seg_len] = '\0';
            aurora_array_push_str(arr, buf);
            free(buf);
        }
    }
    return arr;
}

AuroraStr* builtin_join(int64_t arr_ptr, const void* sep_a) {
    const char* sep = aurora_str_ptr(sep_a);
    int64_t len = aurora_array_len(arr_ptr);
    if (len <= 0) return aurora_str_new(0);
    int64_t seplen = sep ? (int64_t)strlen(sep) : 0;
    int64_t total = 0;
    for (int64_t i = 0; i < len; i++) {
        int64_t tag = aurora_array_get_tag(arr_ptr, i);
        if (tag == 2 || tag == 4) {
            const char* s = aurora_array_get_str(arr_ptr, i);
            int64_t sl = 0;
            while (s[sl]) sl++;
            total += sl;
        }
        if (i < len - 1) total += seplen;
    }
    size_t cap = (size_t)total + 1;
    if (cap < 16) cap = 16;
    AuroraStr* result = aurora_str_new(cap);
    int64_t pos = 0;
    for (int64_t i = 0; i < len; i++) {
        int64_t tag = aurora_array_get_tag(arr_ptr, i);
        if (tag == 2 || tag == 4) {
            const char* s = aurora_array_get_str(arr_ptr, i);
            int64_t sl = 0;
            while (s[sl]) sl++;
            for (int64_t j = 0; j < sl; j++)
                result->ptr[pos++] = s[j];
        }
        if (i < len - 1) {
            for (int64_t j = 0; j < seplen; j++)
                result->ptr[pos++] = sep[j];
        }
    }
    result->ptr[pos] = '\0';
    result->len = (size_t)pos;
    return result;
}

int64_t builtin_has_str(const void* str, const void* sub) {
    const char* s = aurora_str_ptr(str);
    const char* ss = aurora_str_ptr(sub);
    return aurora_str_contains(s ? s : "", ss ? ss : "");
}

int64_t builtin_starts(const void* str_a, const void* prefix_a) {
    const char* str = aurora_str_ptr(str_a);
    const char* prefix = aurora_str_ptr(prefix_a);
    if (!str || !prefix) return 0;
    int64_t slen = (int64_t)strlen(str), plen = (int64_t)strlen(prefix);
    if (plen > slen) return 0;
    for (int64_t i = 0; i < plen; i++)
        if (str[i] != prefix[i]) return 0;
    return 1;
}

int64_t builtin_ends(const void* str_a, const void* suffix_a) {
    const char* str = aurora_str_ptr(str_a);
    const char* suffix = aurora_str_ptr(suffix_a);
    if (!str || !suffix) return 0;
    int64_t slen = (int64_t)strlen(str), suflen = (int64_t)strlen(suffix);
    if (suflen > slen) return 0;
    int64_t offset = slen - suflen;
    for (int64_t i = 0; i < suflen; i++)
        if (str[offset + i] != suffix[i]) return 0;
    return 1;
}

AuroraStr* builtin_reverse_str(const void* str_a) {
    const char* str = aurora_str_ptr(str_a);
    if (!str) return aurora_str_new(0);
    int64_t len = (int64_t)strlen(str);
    AuroraStr* result = aurora_str_new((size_t)len + 1);
    for (int64_t i = 0; i < len; i++)
        result->ptr[i] = str[len - 1 - i];
    result->ptr[len] = '\0';
    result->len = (size_t)len;
    return result;
}

/* ════════════════════════════════════════════════════════════
   Math Built-ins
   ════════════════════════════════════════════════════════════ */

double builtin_sqrt(double x) { return aurora_math_sqrt(x); }
double builtin_abs_val(double x) { return aurora_math_abs(x); }
double builtin_floor_val(double x) { return aurora_math_floor(x); }
double builtin_ceil_val(double x) { return aurora_math_ceil(x); }
double builtin_round_val(double x) { return aurora_math_round(x); }
double builtin_pow_val(double x, double y) { return aurora_math_pow(x, y); }
int64_t builtin_rand_int(int64_t min, int64_t max) { return aurora_math_random_int(min, max); }
double builtin_rand_float() { return aurora_math_random(); }

double builtin_clamp(double val, double lo, double hi) {
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

double builtin_min2(double a, double b) {
    return a < b ? a : b;
}

double builtin_max2(double a, double b) {
    return a > b ? a : b;
}

int64_t builtin_rand() {
    return aurora_random();
}

/* ════════════════════════════════════════════════════════════
   File Built-ins
   ════════════════════════════════════════════════════════════ */

AuroraStr* builtin_read_file(const void* path_a) {
    const char* path = aurora_str_ptr(path_a);
    char* cstr = aurora_fs_read_file(path ? path : "");
    if (!cstr) return aurora_str_new(0);
    AuroraStr* result = aurora_str_from_cstr(cstr);
    free(cstr);
    return result;
}

int64_t builtin_write_file(const void* path_a, const void* content_a) {
    const char* path = aurora_str_ptr(path_a);
    const char* content = aurora_str_ptr(content_a);
    return (int64_t)aurora_fs_write_file(path ? path : "", content ? content : "");
}

int64_t builtin_append_file(const void* path_a, const void* content_a) {
    const char* path = aurora_str_ptr(path_a);
    const char* content = aurora_str_ptr(content_a);
    return (int64_t)aurora_fs_append_file(path ? path : "", content ? content : "");
}

int64_t builtin_file_exists(const void* path_a) {
    const char* path = aurora_str_ptr(path_a);
    return (int64_t)aurora_fs_exists(path ? path : "");
}

int64_t builtin_delete_file(const void* path_a) {
    const char* path = aurora_str_ptr(path_a);
    return (int64_t)aurora_fs_remove(path ? path : "");
}

int64_t builtin_copy_file(const void* src_a, const void* dst_a) {
    const char* src = aurora_str_ptr(src_a);
    const char* dst = aurora_str_ptr(dst_a);
    return (int64_t)aurora_fs_copy(src ? src : "", dst ? dst : "");
}

int64_t builtin_move_file(const void* old_a, const void* newname_a) {
    const char* old = aurora_str_ptr(old_a);
    const char* newname = aurora_str_ptr(newname_a);
    return (int64_t)aurora_fs_rename(old ? old : "", newname ? newname : "");
}

/* ════════════════════════════════════════════════════════════
   Path Built-ins
   ════════════════════════════════════════════════════════════ */

AuroraStr* builtin_cwd() {
#ifdef _WIN32
    char buf[1024];
    if (_getcwd(buf, 1024)) return aurora_str_from_cstr(buf);
    return aurora_str_new(0);
#else
    char buf[1024];
    if (getcwd(buf, 1024)) return aurora_str_from_cstr(buf);
    return aurora_str_new(0);
#endif
}

int64_t builtin_cd(const void* path_a) {
    const char* path = aurora_str_ptr(path_a);
    if (!path || !*path) return 0;
#ifdef _WIN32
    return _chdir(path) == 0 ? 1 : 0;
#else
    return chdir(path) == 0 ? 1 : 0;
#endif
}

AuroraStr* builtin_dirname(const void* path_a) {
    const char* path = aurora_str_ptr(path_a);
    char* cstr = aurora_fs_dirname(path ? path : "");
    if (!cstr) return aurora_str_new(0);
    AuroraStr* result = aurora_str_from_cstr(cstr);
    free(cstr);
    return result;
}

AuroraStr* builtin_basename(const void* path_a) {
    const char* path = aurora_str_ptr(path_a);
    char* cstr = aurora_fs_basename(path ? path : "");
    if (!cstr) return aurora_str_new(0);
    AuroraStr* result = aurora_str_from_cstr(cstr);
    free(cstr);
    return result;
}

AuroraStr* builtin_ext(const void* path_a) {
    const char* path = aurora_str_ptr(path_a);
    if (!path) return aurora_str_new(0);
    int64_t len = (int64_t)strlen(path);
    int64_t dot = -1;
    for (int64_t i = len - 1; i >= 0; i--) {
        if (path[i] == '.') { dot = i; break; }
        if (path[i] == '/' || path[i] == '\\') break;
    }
    if (dot < 0) return aurora_str_new(0);
    int64_t ext_len = len - dot;
    size_t cap = (size_t)ext_len + 1;
    if (cap < 16) cap = 16;
    AuroraStr* result = aurora_str_new(cap);
    for (int64_t i = 0; i < ext_len; i++)
        result->ptr[i] = path[dot + i];
    result->ptr[ext_len] = '\0';
    result->len = (size_t)ext_len;
    return result;
}

/* ════════════════════════════════════════════════════════════
   Time Built-ins
   ════════════════════════════════════════════════════════════ */

int64_t builtin_now() { return aurora_time(); }
int64_t builtin_stamp() { return aurora_time(); }
void builtin_sleep(int64_t ms) { aurora_sleep(ms); }

/* ════════════════════════════════════════════════════════════
   OS / Environment Built-ins
   ════════════════════════════════════════════════════════════ */

int64_t builtin_os(char* buf, int64_t buf_size) {
    if (!buf || buf_size <= 0) return 0;
#ifdef _WIN32
    const char* os = "windows";
#elif __APPLE__
    const char* os = "macos";
#elif __linux__
    const char* os = "linux";
#else
    const char* os = "unknown";
#endif
    int64_t len = 0;
    while (os[len]) len++;
    if (len < buf_size) {
        for (int64_t i = 0; i < len; i++) buf[i] = os[i];
        buf[len] = '\0';
    }
    return len;
}

int64_t builtin_cpu_count() {
#ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return (int64_t)sysinfo.dwNumberOfProcessors;
#else
    return (int64_t)sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

int64_t builtin_memory_usage() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return (int64_t)pmc.WorkingSetSize;
    return 0;
#else
    return (int64_t)aurora_memory_total_usage();
#endif
}

AuroraStr* builtin_env(const void* name_a) {
    const char* name = aurora_str_ptr(name_a);
    if (!name) return aurora_str_new(0);
#ifdef _WIN32
    char buf[32768];
    DWORD ret = GetEnvironmentVariableA(name, buf, 32768);
    if (ret > 0 && ret < 32768) return aurora_str_from_cstr(buf);
    return aurora_str_new(0);
#else
    char* val = getenv(name);
    if (!val) return aurora_str_new(0);
    return aurora_str_from_cstr(val);
#endif
}

int64_t builtin_run(const void* cmd_a) {
    const char* cmd = aurora_str_ptr(cmd_a);
    if (!cmd) return -1;
    return (int64_t)system(cmd);
}

void builtin_exit(int64_t code) {
    exit((int)code);
}

/* ════════════════════════════════════════════════════════════
   JSON Built-ins
   ════════════════════════════════════════════════════════════ */

AuroraStr* builtin_encode_json(void* val) {
    char* cstr = aurora_json_serialize(val);
    if (!cstr) return aurora_str_new(0);
    AuroraStr* result = aurora_str_from_cstr(cstr);
    free(cstr);
    return result;
}

void* builtin_decode_json(const void* str_a) {
    const char* str = aurora_str_ptr(str_a);
    return aurora_json_parse(str ? str : "");
}

/* ════════════════════════════════════════════════════════════
   HTTP Built-ins
   ════════════════════════════════════════════════════════════ */

int64_t builtin_http_get(const void* url_a, char* buf, int64_t buf_size) {
    const char* url = aurora_str_ptr(url_a);
    return (int64_t)aurora_net_http_get(url ? url : "", buf, (int)buf_size);
}

int64_t builtin_http_post(const void* url_a, const void* body_a, const void* ct_a, char* buf, int64_t buf_size) {
    const char* url = aurora_str_ptr(url_a);
    const char* body = aurora_str_ptr(body_a);
    const char* ct = aurora_str_ptr(ct_a);
    return (int64_t)aurora_net_http_post(url ? url : "", body ? body : "", ct ? ct : "", buf, (int)buf_size);
}

/* ════════════════════════════════════════════════════════════
   Collection Built-ins
   ════════════════════════════════════════════════════════════ */

int64_t builtin_push(int64_t arr_ptr, int64_t val) {
    aurora_array_push_int(arr_ptr, val);
    return arr_ptr;
}

int64_t builtin_push_str(int64_t arr_ptr, const void* str_a) {
    const char* str = aurora_str_ptr(str_a);
    aurora_array_push_str(arr_ptr, str ? str : "");
    return arr_ptr;
}

int64_t builtin_pop(int64_t arr_ptr) {
    int64_t len = aurora_array_len(arr_ptr);
    if (len <= 0) { aurora_panic("pop() on empty array"); return 0; }
    int64_t tag = aurora_array_get_tag(arr_ptr, len - 1);
    int64_t val = 0;
    if (tag == 1) {
        double fv = aurora_array_get_float(arr_ptr, len - 1);
        val = *((int64_t*)&fv);
    } else if (tag == 0) {
        val = aurora_array_get_int(arr_ptr, len - 1);
    }
    aurora_array_set_int(arr_ptr, len - 1, 0);
    /* Directly decrement array length (data=0, len=8, cap=16 in AuroraArray) */
    int64_t* len_field = (int64_t*)((char*)(uintptr_t)arr_ptr + 8);
    *len_field = len - 1;
    return val;
}

void builtin_insert(int64_t arr_ptr, int64_t idx, int64_t val) {
    int64_t len = aurora_array_len(arr_ptr);
    if (idx < 0 || idx > len) { aurora_panic("insert() index out of bounds"); return; }
    aurora_array_push_int(arr_ptr, 0);
    for (int64_t i = len; i > idx; i--) {
        int64_t v = aurora_array_get_int(arr_ptr, i - 1);
        aurora_array_set_int(arr_ptr, i, v);
    }
    aurora_array_set_int(arr_ptr, idx, val);
}

void builtin_remove(int64_t arr_ptr, int64_t idx) {
    int64_t len = aurora_array_len(arr_ptr);
    if (idx < 0 || idx >= len) { aurora_panic("remove() index out of bounds"); return; }
    for (int64_t i = idx; i < len - 1; i++) {
        int64_t v = aurora_array_get_int(arr_ptr, i + 1);
        aurora_array_set_int(arr_ptr, i, v);
    }
    int64_t* len_field = (int64_t*)((char*)(uintptr_t)arr_ptr + 8);
    *len_field = len - 1;
}

void builtin_clear(int64_t arr_ptr) {
    int64_t* len_field = (int64_t*)((char*)(uintptr_t)arr_ptr + 8);
    *len_field = 0;
}

int64_t builtin_has_arr(int64_t arr_ptr, int64_t val) {
    return aurora_array_contains_int(arr_ptr, val);
}

void builtin_sort(int64_t arr_ptr) {
    int64_t len = aurora_array_len(arr_ptr);
    if (len <= 1) return;
    for (int64_t i = 0; i < len - 1; i++) {
        for (int64_t j = 0; j < len - i - 1; j++) {
            int64_t a = aurora_array_get_int(arr_ptr, j);
            int64_t b = aurora_array_get_int(arr_ptr, j + 1);
            if (a > b) {
                aurora_array_set_int(arr_ptr, j, b);
                aurora_array_set_int(arr_ptr, j + 1, a);
            }
        }
    }
}

void builtin_reverse_arr(int64_t arr_ptr) {
    int64_t len = aurora_array_len(arr_ptr);
    for (int64_t i = 0; i < len / 2; i++) {
        int64_t a = aurora_array_get_int(arr_ptr, i);
        int64_t b = aurora_array_get_int(arr_ptr, len - 1 - i);
        aurora_array_set_int(arr_ptr, i, b);
        aurora_array_set_int(arr_ptr, len - 1 - i, a);
    }
}

int64_t builtin_unique(int64_t arr_ptr) {
    int64_t len = aurora_array_len(arr_ptr);
    int64_t result = aurora_array_new(len);
    for (int64_t i = 0; i < len; i++) {
        int64_t v = aurora_array_get_int(arr_ptr, i);
        if (!aurora_array_contains_int(result, v))
            aurora_array_push_int(result, v);
    }
    return result;
}

/* ════════════════════════════════════════════════════════════
   Error / Confirm
   ════════════════════════════════════════════════════════════ */

AuroraStr* builtin_error(const void* msg_a) {
    const char* msg = aurora_str_ptr(msg_a);
    if (!msg) return aurora_str_new(0);
    return aurora_str_from_cstr(msg);
}

int64_t builtin_ask(const void* prompt_a) {
    const char* prompt = aurora_str_ptr(prompt_a);
    if (prompt) {
        aurora_print_str(prompt);
    }
#ifdef _WIN32
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    char buf[16];
    DWORD read = 0;
    if (!ReadFile(hIn, buf, sizeof(buf) - 1, &read, nullptr)) return 0;
    buf[read] = '\0';
#else
    char buf[16];
    if (!fgets(buf, sizeof(buf), stdin)) return 0;
#endif
    return (buf[0] == 'y' || buf[0] == 'Y') ? 1 : 0;
}

/* ════════════════════════════════════════════════════════════
   Type conversion
   ════════════════════════════════════════════════════════════ */

AuroraStr* builtin_char(const void* str_a) {
    const char* s = aurora_str_ptr(str_a);
    if (!s || !*s) return aurora_str_new(0);
    char buf[2] = { s[0], '\0' };
    return aurora_str_from_cstr(buf);
}

/* ════════════════════════════════════════════════════════════
   Channel implementation (blocking bounded queue)
   ════════════════════════════════════════════════════════════ */

int64_t builtin_chan(int64_t capacity) {
    auto* ch = aurora_chan_create((int32_t)capacity);
    return (int64_t)(uintptr_t)ch;
}

void builtin_send(int64_t ch_ptr, int64_t val) {
    aurora_chan_send((AuroraChannel*)(uintptr_t)ch_ptr, (void*)(uintptr_t)val);
}

int64_t builtin_recv(int64_t ch_ptr) {
    void* val = aurora_chan_recv((AuroraChannel*)(uintptr_t)ch_ptr);
    return (int64_t)(uintptr_t)val;
}

/* ════════════════════════════════════════════════════════════
   Fiber builtins
   ════════════════════════════════════════════════════════════ */

int64_t builtin_fiber_create(void* fn_ptr, int64_t arg) {
    auto* func = (void* (*)(void*))fn_ptr;
    auto* fiber = aurora_fiber_create(func, (void*)(uintptr_t)arg);
    return (int64_t)(uintptr_t)fiber;
}

void builtin_fiber_resume(int64_t fiber_ptr) {
    auto* fiber = (AuroraFiber*)(uintptr_t)fiber_ptr;
    aurora_fiber_resume(fiber);
}

void builtin_fiber_yield() {
    aurora_fiber_yield();
}

int64_t builtin_fiber_is_done(int64_t fiber_ptr) {
    auto* fiber = (AuroraFiber*)(uintptr_t)fiber_ptr;
    return (int64_t)aurora_fiber_is_done(fiber);
}

int64_t builtin_fiber_get_result(int64_t fiber_ptr) {
    auto* fiber = (AuroraFiber*)(uintptr_t)fiber_ptr;
    return (int64_t)(uintptr_t)aurora_fiber_get_result(fiber);
}

void builtin_fiber_destroy(int64_t fiber_ptr) {
    auto* fiber = (AuroraFiber*)(uintptr_t)fiber_ptr;
    aurora_fiber_destroy(fiber);
}

/* ════════════════════════════════════════════════════════════
   Event bus builtins
   ════════════════════════════════════════════════════════════ */

void builtin_event_on(void* name_str, void* handler_fn) {
    if (!name_str || !handler_fn) return;
    auto* name = (AuroraStr*)name_str;
    if (!name->ptr) return;
    aurora_event_on(name->ptr, (void(*)(void*))handler_fn, nullptr);
}

void builtin_event_off(void* name_str, void* handler_fn) {
    if (!name_str || !handler_fn) return;
    auto* name = (AuroraStr*)name_str;
    if (!name->ptr) return;
    aurora_event_off(name->ptr, (void(*)(void*))handler_fn);
}

void builtin_event_emit(void* name_str, void* arg) {
    if (!name_str) return;
    auto* name = (AuroraStr*)name_str;
    if (!name->ptr) return;
    aurora_event_emit(name->ptr, arg);
}

/* ════════════════════════════════════════════════════════════
   Async builtins (spawn / await)
   ════════════════════════════════════════════════════════════ */

int64_t builtin_spawn(void* task_ptr) {
    auto* task = (AuroraTask*)task_ptr;
    if (!task) return 0;
    static bool init_once = false;
    if (!init_once) {
        aurora_scheduler_init(aurora_scheduler_default_threads());
        init_once = true;
    }
    aurora_spawn(task);
    return 1;
}

AuroraStr* builtin_await(void* task_ptr) {
    auto* task = (AuroraTask*)task_ptr;
    if (!task) return aurora_str_new(0);
    aurora_wait(task);
    void* result = aurora_task_get_result(task);
    if (!result) return aurora_str_new(0);
    return aurora_str_from_cstr((const char*)result);
}

/* ════════════════════════════════════════════════════════════
   Performance builtins
   ════════════════════════════════════════════════════════════ */

int64_t builtin_measure() {
    auto now = std::chrono::high_resolution_clock::now();
    return (int64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();
}

int64_t builtin_bench(void* fn_ptr, int64_t iterations) {
    if (!fn_ptr) return 0;
    auto start = std::chrono::high_resolution_clock::now();
    for (int64_t i = 0; i < iterations && i < 1000000; i++) {
        /* call the function */
        auto* fn = (void* (*)(void*))fn_ptr;
        fn(nullptr);
    }
    auto end = std::chrono::high_resolution_clock::now();
    return (int64_t)std::chrono::duration_cast<std::chrono::microseconds>(
        end - start).count();
}

int64_t builtin_profile(void* fn_ptr) {
    if (!fn_ptr) return 0;
    return builtin_bench(fn_ptr, 1000);
}

int64_t builtin_trace(void* fn_ptr) {
    if (!fn_ptr) return 0;
    return builtin_bench(fn_ptr, 100);
}

/* ════════════════════════════════════════════════════════════
   Reflection builtins
   ════════════════════════════════════════════════════════════ */

int64_t builtin_fields(void* obj) {
    if (!obj) return aurora_array_new(0);
    auto* box = (SharedBox*)obj;
    if (box->magic != 0x41555230) return aurora_array_new(0);
    if (!box->data) return aurora_array_new(0);
    auto& reg = ReflectionRegistry::instance();
    auto names = reg.get_type_names();
    int64_t arr = aurora_array_new((int64_t)names.size());
    for (const auto& tname : names) {
        auto fields = reg.get_fields(tname);
        for (const auto& f : fields) {
            aurora_array_push_str(arr, f.name.c_str());
        }
    }
    return arr;
}

int64_t builtin_methods(void* obj) {
    if (!obj) return aurora_array_new(0);
    auto* box = (SharedBox*)obj;
    if (box->magic != 0x41555230) return aurora_array_new(0);
    if (!box->data) return aurora_array_new(0);
    auto& reg = ReflectionRegistry::instance();
    auto names = reg.get_type_names();
    int64_t arr = aurora_array_new((int64_t)names.size());
    for (const auto& tname : names) {
        auto methods = reg.get_methods(tname);
        for (const auto& m : methods) {
            aurora_array_push_str(arr, m.name.c_str());
        }
    }
    return arr;
}

/* ════════════════════════════════════════════════════════════
   Package builtins
   ════════════════════════════════════════════════════════════ */

static int run_voss_cmd(const char* cmd, char* buf, int buf_size) {
    if (!cmd || !buf || buf_size <= 0) return -1;
#ifdef _WIN32
    FILE* pipe = _popen(cmd, "r");
#else
    FILE* pipe = popen(cmd, "r");
#endif
    if (!pipe) return -1;
    int total = 0;
    while (total < buf_size - 1) {
        int ch = fgetc(pipe);
        if (ch == EOF) break;
        buf[total++] = (char)ch;
    }
    buf[total] = '\0';
#ifdef _WIN32
    int ret = _pclose(pipe);
#else
    int ret = pclose(pipe);
#endif
    (void)ret;
    return total;
}

AuroraStr* builtin_install(const void* pkg_a) {
    const char* pkg = aurora_str_ptr(pkg_a);
    if (!pkg) return aurora_str_from_cstr("no package specified");
    char cmd[4096];
    char out[4096];
    snprintf(cmd, sizeof(cmd), "voss install \"%s\" 2>&1", pkg);
    int n = run_voss_cmd(cmd, out, sizeof(out));
    if (n <= 0) return aurora_str_from_cstr("package manager (voss) not found on PATH");
    return aurora_str_from_cstr(out);
}

AuroraStr* builtin_update(const void* pkg_a) {
    const char* pkg = aurora_str_ptr(pkg_a);
    if (!pkg) return aurora_str_from_cstr("no package specified");
    char cmd[4096];
    char out[4096];
    snprintf(cmd, sizeof(cmd), "voss update \"%s\" 2>&1", pkg);
    int n = run_voss_cmd(cmd, out, sizeof(out));
    if (n <= 0) return aurora_str_from_cstr("package manager (voss) not found on PATH");
    return aurora_str_from_cstr(out);
}

int64_t builtin_search(const void* query_a) {
    const char* query = aurora_str_ptr(query_a);
    if (!query) return aurora_array_new(0);
    char cmd[4096];
    char out[65536];
    snprintf(cmd, sizeof(cmd), "voss search \"%s\" 2>&1", query);
    int n = run_voss_cmd(cmd, out, sizeof(out));
    if (n <= 0) return aurora_array_new(0);
    int64_t arr = aurora_array_new(32);
    char* line = out;
    while (line && *line) {
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        if (*line) aurora_array_push_str(arr, line);
        if (!nl) break;
        line = nl + 1;
    }
    return arr;
}

/* ════════════════════════════════════════════════════════════
   AI builtins — chat via active model
   ════════════════════════════════════════════════════════════ */

char* chat(const void* msg_a) {
    const char* msg = aurora_str_ptr(msg_a);
    if (!msg) return AURORA_STRDUP("");
    if (!g_active_model) return AURORA_STRDUP("No active model. Train or load one first.");
    char full[16384];
    snprintf(full, sizeof(full), "<|user|>\n%s\n<|end|>\n<|assistant|>\n", msg);
    return model_generate(g_active_model, g_active_tokenizer, full, g_max_tokens);
}

} /* extern "C" */
