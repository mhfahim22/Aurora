#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <mutex>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#include <pthread.h>
#endif

#ifdef _WIN32
#define AURORA_DL_PREFIX "lib"
#define AURORA_DL_SUFFIX ".dll"
#elif __APPLE__
#define AURORA_DL_PREFIX "lib"
#define AURORA_DL_SUFFIX ".dylib"
#else
#define AURORA_DL_PREFIX "lib"
#define AURORA_DL_SUFFIX ".so"
#endif

extern "C" {

/* ── Platform-agnostic mutex ── */
#ifdef _WIN32
struct aurora_mutex {
    CRITICAL_SECTION cs;
    aurora_mutex()  { InitializeCriticalSection(&cs); }
    /* Destructor runs during static destruction — ensure no other thread
       tries to lock g_callback_mutex after this point */
    ~aurora_mutex() { DeleteCriticalSection(&cs); }
    void lock()     { EnterCriticalSection(&cs); }
    void unlock()   { LeaveCriticalSection(&cs); }
};
#else
struct aurora_mutex {
    pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
    void lock()   { pthread_mutex_lock(&mtx); }
    void unlock() { pthread_mutex_unlock(&mtx); }
};
#endif

static aurora_mutex g_callback_mutex;

static std::vector<void*>& get_dl_handles() {
    static std::vector<void*> handles;
    return handles;
}
/* WARNING: g_dl_handle_mtx (std::mutex) has non-trivial destructor.
   Ensure it outlives all dl operations during static destruction.
   g_dl_cleanup (line 830) runs first, before mutex destruction. */
static std::mutex g_dl_handle_mtx;

/* ── Open a shared library ── */
/* Returns opaque handle (pointer to library) or nullptr on failure. */
void* aurora_dl_open(const char* libname) {
    if (!libname || !libname[0]) return nullptr;
#ifdef _WIN32
    /* Primary attempt: standard LoadLibraryA */
    void* lib = (void*)LoadLibraryA(libname);
    if (lib) return lib;
    /* Fallback: use LoadLibraryExW with altered search path to bypass
       SetDllDirectory restrictions (Python init may have removed CWD) */
    /* First, try from the EXE's own directory */
    char exe_dir[1024];
    DWORD len = GetModuleFileNameA(NULL, exe_dir, sizeof(exe_dir));
    if (len > 0 && len < sizeof(exe_dir)) {
        char* last_slash = strrchr(exe_dir, '\\');
        if (last_slash) {
            *(last_slash + 1) = '\0';
            char full_path[2048];
            snprintf(full_path, sizeof(full_path), "%s%s", exe_dir, libname);
            lib = (void*)LoadLibraryExA(full_path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
            if (lib) return lib;
        }
    }
    /* Fallback: traverse up from EXE directory looking for the bridge */
    /* build\Release\flask_pypi\ → build\ → project root → found */
    char up_path[2048];
    snprintf(up_path, sizeof(up_path), "%s", libname);
    /* Try prepending parent dirs relative to EXE dir (up to 3 levels) */
    if (len > 0 && len < sizeof(exe_dir)) {
        char* base = strrchr(exe_dir, '\\');
        if (base) {
            *base = '\0';
            for (int i = 0; i < 4; i++) {
                char try_path[2048];
                snprintf(try_path, sizeof(try_path), "%s\\%s", exe_dir, libname);
                lib = (void*)LoadLibraryExA(try_path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
                if (lib) return lib;
                /* Go up one more directory level */
                char* sep = strrchr(exe_dir, '\\');
                if (!sep) break;
                if (sep == exe_dir) { *sep = '\0'; break; }
                *sep = '\0';
            }
        }
    }
    return nullptr;
#else
    return dlopen(libname, RTLD_NOW | RTLD_LOCAL);
#endif
}

/* ── Resolve a symbol in a loaded library ── */
/* Returns function pointer or nullptr. */
void* aurora_dl_sym(void* lib, const char* name) {
    if (!lib || !name) return nullptr;
#ifdef _WIN32
    return (void*)GetProcAddress((HMODULE)lib, name);
#else
    return dlsym(lib, name);
#endif
}

/* ── Combined open + resolve ── */
/* Forward declaration */
void aurora_dl_close(void* lib);
/* Note: library handle is retained to keep the function pointer valid.
   Use aurora_dl_cleanup_all() to close all tracked handles on shutdown. */
void* aurora_dl_resolve(const char* libname, const char* name) {
    void* lib = aurora_dl_open(libname);
    if (!lib) return nullptr;
    void* sym = aurora_dl_sym(lib, name);
    if (!sym) {
        /* For OpenGL, fall back to wglGetProcAddress (modern GL functions are
           not exported by opengl32.dll — they come from the GPU driver's ICD). */
#ifdef _WIN32
        if (libname && name && strcmp(libname, "opengl32") == 0) {
            HMODULE opengl32 = LoadLibraryA("opengl32.dll");
            if (opengl32) {
                auto wglGetProcAddr = (PROC(WINAPI*)(LPCSTR))
                    GetProcAddress(opengl32, "wglGetProcAddress");
                if (wglGetProcAddr) {
                    sym = (void*)wglGetProcAddr(name);
                }
            }
        }
#endif
        if (!sym) { aurora_dl_close(lib); return nullptr; }
    }
    {
        std::lock_guard<std::mutex> lock(g_dl_handle_mtx);
        get_dl_handles().push_back(lib);
    }
    return sym;
}

/* ── Close all tracked library handles opened via aurora_dl_resolve ── */
void aurora_dl_cleanup_all() {
    std::lock_guard<std::mutex> lock(g_dl_handle_mtx);
    for (void* lib : get_dl_handles())
        aurora_dl_close(lib);
    get_dl_handles().clear();
}

/* ── Close shared library ── */
void aurora_dl_close(void* lib) {
    if (!lib) return;
#ifdef _WIN32
    FreeLibrary((HMODULE)lib);
#else
    dlclose(lib);
#endif
}

/* NOTE: g_dl_cleanup (line 830) runs ~DlCleanup during static destruction.
   It calls aurora_dl_cleanup_all which locks g_dl_handle_mtx. Ensure
   g_dl_handle_mtx is still alive at that point. */

/* ── Last error message ── */
/* Thread-safe: uses thread-local storage for the error buffer. */
const char* aurora_dl_error(void) {
#ifdef _WIN32
    thread_local static char tls_buf[256];
    DWORD err = GetLastError();
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, err, 0, tls_buf, sizeof(tls_buf), nullptr);
    return tls_buf;
#else
    return dlerror();
#endif
}

/* ── Try to load a library by common naming conventions ── */
/* Tries: name, libname.dll, libname.so, libname.dylib */
void* aurora_dl_try_open(const char* name) {
    if (!name || !name[0]) return nullptr;

    /* Try raw name first */
    void* lib = aurora_dl_open(name);
    if (lib) return lib;

    /* Try with platform prefix/suffix */
    char buf[512];
#ifdef _WIN32
    snprintf(buf, sizeof(buf), "%s.dll", name);
    lib = aurora_dl_open(buf);
    if (lib) return lib;
#else
    snprintf(buf, sizeof(buf), "lib%s.so", name);
    lib = aurora_dl_open(buf);
    if (lib) return lib;

    snprintf(buf, sizeof(buf), "lib%s.dylib", name);
    lib = aurora_dl_open(buf);
    if (lib) return lib;

    snprintf(buf, sizeof(buf), "%s.so", name);
    lib = aurora_dl_open(buf);
    if (lib) return lib;
#endif

    return nullptr;
}

/* ── Ecosystem-aware resolve: handles pypi_/npm_/cargo_ prefixes ── */
/* Searches bridge dirs, packages/, and cwd for the bridge DLL. */

/* Validate that a loaded bridge DLL has the expected entry points.
   Prints clear diagnostics on failure. Returns true if valid.
   NOTE: The library handle is owned by the caller (aurora_ecosystem_resolve).
   Handle tracking uses g_dl_handle_mtx/g_dl_cleanup for shutdown. */
static bool aurora_validate_bridge(void* lib, const char* ecs, const char* pkg,
                                     const char* func_name) {
    if (!lib) {
        fprintf(stderr, "[ffi] ⚠ bridge DLL for %s:%s not found\n", ecs, pkg);
        fprintf(stderr, "[ffi]   tried: bridge dir, packages/, cwd\n");
        return false;
    }
    /* Verify the requested function exists */
    void* sym = aurora_dl_sym(lib, func_name);
    if (!sym) {
        fprintf(stderr, "[ffi] ⚠ bridge DLL loaded for %s:%s, but function '%s' not exported\n",
                ecs, pkg, func_name);
        fprintf(stderr, "[ffi]   check the bridge's C wrapper for export declarations\n");
        return false;
    }
    return true;
}

/* Track a loaded library handle for cleanup on shutdown */
static void track_lib(void* lib) {
    std::lock_guard<std::mutex> lock(g_dl_handle_mtx);
    get_dl_handles().push_back(lib);
}

/* Resolve a symbol from an already-opened library and track handle */
static void* resolve_tracked(void* lib, const char* func_name) {
    void* sym = aurora_dl_sym(lib, func_name);
    if (sym) track_lib(lib);
    else     aurora_dl_close(lib);
    return sym;
}

void* aurora_ecosystem_resolve(const char* ecs_name, const char* func_name) {
    if (!ecs_name || !ecs_name[0]) return nullptr;

    char buf[512];
    void* lib = nullptr;
    const char* pkg = nullptr;

    if (strncmp(ecs_name, "pypi_", 5) == 0) {
        pkg = ecs_name + 5;
        /* Try bridge dir (new-style: lodash_pypi/) */
        snprintf(buf, sizeof(buf), "%s_pypi/%s_pypi.dll", pkg, pkg);
        lib = aurora_dl_open(buf);
        if (!lib) {
            snprintf(buf, sizeof(buf), "packages/%s_pypi_bridge/%s_pypi.dll", pkg, pkg);
            lib = aurora_dl_open(buf);
        }
        if (!lib) {
            snprintf(buf, sizeof(buf), "%s_pypi.dll", pkg);
            lib = aurora_dl_open(buf);
        }
        if (lib) {
            if (!aurora_validate_bridge(lib, "pypi", pkg, func_name)) {
                aurora_dl_close(lib); return nullptr;
            }
            return resolve_tracked(lib, func_name);
        }
        /* Fallback: embedded Python */
        lib = aurora_dl_open("python3.dll");
        if (lib) return resolve_tracked(lib, func_name);
        fprintf(stderr, "[ffi] ⚠ could not load bridge for pypi:%s\n", pkg);
        fprintf(stderr, "[ffi]   run: voss bridge pypi \"%s\"\n", pkg);
        return nullptr;
    }

    if (strncmp(ecs_name, "npm_", 4) == 0) {
        pkg = ecs_name + 4;
        /* Try bridge dir (new-style: packages/bridges/npm/lodash_npm/) */
        snprintf(buf, sizeof(buf), "packages/bridges/npm/%s_npm/%s_npm.dll", pkg, pkg);
        lib = aurora_dl_open(buf);
        /* Try bridge dir (old-style: lodash_npm/) */
        if (!lib) {
            snprintf(buf, sizeof(buf), "%s_npm/%s_npm.dll", pkg, pkg);
            lib = aurora_dl_open(buf);
        }
        /* Try bridge dir (old-style: lodash_npm_bridge/) */
        if (!lib) {
            snprintf(buf, sizeof(buf), "%s_npm_bridge/%s_npm.dll", pkg, pkg);
            lib = aurora_dl_open(buf);
        }
        if (!lib) {
            snprintf(buf, sizeof(buf), "packages/%s_npm_bridge/%s_npm.dll", pkg, pkg);
            lib = aurora_dl_open(buf);
        }
        if (!lib) {
            snprintf(buf, sizeof(buf), "%s_npm.dll", pkg);
            lib = aurora_dl_open(buf);
        }
        if (lib) {
            if (!aurora_validate_bridge(lib, "npm", pkg, func_name)) {
                aurora_dl_close(lib); return nullptr;
            }
            return resolve_tracked(lib, func_name);
        }
        lib = aurora_dl_open("node.dll");
        if (lib) return resolve_tracked(lib, func_name);
        fprintf(stderr, "[ffi] ⚠ could not load bridge for npm:%s\n", pkg);
        fprintf(stderr, "[ffi]   run: voss bridge npm \"%s\"\n", pkg);
        return nullptr;
    }

    if (strncmp(ecs_name, "cargo_", 6) == 0) {
        pkg = ecs_name + 6;
        /* Try bridge dir first */
        char bridge_path[512];
#ifdef _WIN32
        snprintf(bridge_path, sizeof(bridge_path), "%s_cargo/%s_cargo.dll", pkg, pkg);
#else
        snprintf(bridge_path, sizeof(bridge_path), "%s_cargo/lib%s_cargo.so", pkg, pkg);
#endif
        lib = aurora_dl_open(bridge_path);
        if (!lib) {
#ifdef _WIN32
            snprintf(bridge_path, sizeof(bridge_path), "%s_cargo_bridge/%s_cargo.dll", pkg, pkg);
#else
            snprintf(bridge_path, sizeof(bridge_path), "%s_cargo_bridge/lib%s_cargo.so", pkg, pkg);
#endif
            lib = aurora_dl_open(bridge_path);
        }
        if (!lib) {
            snprintf(buf, sizeof(buf), "%s_cargo.dll", pkg);
            lib = aurora_dl_try_open(buf);
        }
        if (!lib) {
            snprintf(buf, sizeof(buf), "%s.dll", pkg);
            lib = aurora_dl_try_open(buf);
        }
        if (!lib) {
            fprintf(stderr, "[ffi] ⚠ could not load bridge for cargo:%s\n", pkg);
            fprintf(stderr, "[ffi]   run: voss bridge cargo \"%s\"\n", pkg);
            return nullptr;
        }
        if (!aurora_validate_bridge(lib, "cargo", pkg, func_name)) {
            aurora_dl_close(lib); return nullptr;
        }
        return resolve_tracked(lib, func_name);
    }

    /* Fallback: native_X or plain library name */
    const char* lib_start = ecs_name;
    if (strncmp(ecs_name, "native_", 7) == 0) {
        lib_start = ecs_name + 7;
        /* Try bridge dir first (new-style: user32_native/) */
        snprintf(buf, sizeof(buf), "%s_native/%s_native.dll", lib_start, lib_start);
        lib = aurora_dl_open(buf);
        if (!lib) {
            snprintf(buf, sizeof(buf), "%s_native/lib%s_native.so", lib_start, lib_start);
            lib = aurora_dl_open(buf);
        }
        /* Try bridge dir (old-style: user32_native_bridge/) */
        if (!lib) {
            snprintf(buf, sizeof(buf), "%s_native_bridge/%s_native.dll", lib_start, lib_start);
            lib = aurora_dl_open(buf);
        }
        if (!lib) {
            snprintf(buf, sizeof(buf), "%s_native_bridge/lib%s_native.so", lib_start, lib_start);
            lib = aurora_dl_open(buf);
        }
        if (lib) {
            if (!aurora_validate_bridge(lib, "native", lib_start, func_name)) {
                aurora_dl_close(lib); return nullptr;
            }
            return resolve_tracked(lib, func_name);
        }
        /* Fallback: load the original DLL directly */
        lib = aurora_dl_try_open(lib_start);
        if (lib) return resolve_tracked(lib, func_name);
        fprintf(stderr, "[ffi] ⚠ could not load native library '%s'\n", lib_start);
        return nullptr;
    }
    /* Plain library name (no prefix) — try loading directly */
    lib = aurora_dl_try_open(lib_start);
    if (lib) return resolve_tracked(lib, func_name);
    fprintf(stderr, "[ffi] ⚠ could not load library '%s'\n", lib_start);
    return nullptr;
}

/* ── Aurora String → C String (safe copy) ── */
/* Allocates a fresh, null-terminated C string from an AuroraStr.
   Caller owns the returned memory and must free() it. */
#include "runtime/string.hpp"

/* Free a C string allocated by aurora_str_to_cstr. */
extern "C" void aurora_free_cstr(char* cstr) {
    if (cstr) free(cstr);
}

extern "C" char* aurora_str_to_cstr(const void* aurora_str_ptr) {
    if (!aurora_str_ptr) return nullptr;
    const AuroraStr* s = static_cast<const AuroraStr*>(aurora_str_ptr);
    size_t len = s->len;
    if (len == SIZE_MAX) return nullptr;
    char* out = (char*)malloc(len + 1);
    if (!out) return nullptr;
    if (len > 0 && s->ptr)
        memcpy(out, s->ptr, len);
    out[len] = '\0';
    return out;
}

/* ════════════════════════════════════════════════════════════
   FFI Struct Layout Helpers
   ════════════════════════════════════════════════════════════ */

/* ── Platform ABI alignment helper ── */
/* Infer natural alignment from field size (approximates x64 ABI rules) */
static int64_t align_of(int64_t size) {
    if (size <= 1) return 1;
    if (size <= 2) return 2;
    if (size <= 4) return 4;
    return 8;
}

static int64_t align_up(int64_t offset, int64_t alignment) {
    return (offset + alignment - 1) & ~(alignment - 1);
}

/* Struct layout with platform ABI alignment.
   Returns the total struct size with proper padding between fields
   and trailing padding to satisfy struct alignment. */
int64_t aurora_struct_size(int64_t field_count, const int64_t* field_sizes) {
    if (field_count <= 0) return 0;
    int64_t offset = 0;
    int64_t max_align = 1;
    for (int64_t i = 0; i < field_count; i++) {
        int64_t a = align_of(field_sizes[i]);
        if (a > max_align) max_align = a;
        offset = align_up(offset, a);
        offset += field_sizes[i];
    }
    /* Round up to struct alignment */
    return align_up(offset, max_align);
}

/* Calculate offset of field at given index (0-based) with alignment. */
int64_t aurora_field_offset(int64_t field_count, const int64_t* field_sizes, int64_t index) {
    if (index >= field_count) return 0;
    int64_t offset = 0;
    for (int64_t i = 0; i < index; i++) {
        offset = align_up(offset, align_of(field_sizes[i]));
        offset += field_sizes[i];
    }
    return align_up(offset, align_of(field_sizes[index]));
}

/* Raw memory read/write helpers (for FFI struct access) */
int64_t  aurora_mem_read_i64(const void* addr, int64_t offset) {
    const char* p = (const char*)addr;
    int64_t val;
    memcpy(&val, p + offset, sizeof(val));
    return val;
}

void aurora_mem_write_i64(void* addr, int64_t offset, int64_t val) {
    char* p = (char*)addr;
    memcpy(p + offset, &val, sizeof(val));
}

double aurora_mem_read_f64(const void* addr, int64_t offset) {
    const char* p = (const char*)addr;
    double val;
    memcpy(&val, p + offset, sizeof(val));
    return val;
}

void aurora_mem_write_f64(void* addr, int64_t offset, double val) {
    char* p = (char*)addr;
    memcpy(p + offset, &val, sizeof(val));
}

int32_t aurora_mem_read_i32(const void* addr, int64_t offset) {
    const char* p = (const char*)addr;
    int32_t val;
    memcpy(&val, p + offset, sizeof(val));
    return val;
}

void aurora_mem_write_i32(void* addr, int64_t offset, int32_t val) {
    char* p = (char*)addr;
    memcpy(p + offset, &val, sizeof(val));
}

/* ════════════════════════════════════════════════════════════
   FFI Callback — trampoline-based callback creation
   ════════════════════════════════════════════════════════════
   Each trampoline is a small piece of executable code that
   calls a registered handler function with the original args.
   Supports x64 and ARM64 architectures.
   ════════════════════════════════════════════════════════════ */

/* Dynamic callback slot allocation — no hard limit */
struct CallbackSlot {
    void* trampoline;
    void* handler;     /* The Aurora wrapper function to call */
    bool  in_use;
    void* user_data;
};

#include <vector>
static std::vector<CallbackSlot> callback_slots;

/* Cleanup callback executable memory on shutdown */
namespace {
    struct CallbackCleanup {
        ~CallbackCleanup() {
            for (auto& slot : callback_slots) {
                if (slot.trampoline) {
                    #ifdef _WIN32
                    VirtualFree(slot.trampoline, 0, MEM_RELEASE);
                    #else
                    free(slot.trampoline);
                    #endif
                }
            }
            callback_slots.clear();
        }
    };
    static CallbackCleanup g_callback_cleanup;
}

/* ── Allocates executable memory (platform-agnostic) ── */
static void* alloc_exec_memory(size_t size) {
#ifdef _WIN32
    return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
#else
    void* code = mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (code == MAP_FAILED) ? nullptr : code;
#endif
}

static void free_exec_memory(void* ptr, size_t size) {
    if (!ptr) return;
#ifdef _WIN32
    (void)size;
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    munmap(ptr, size);
#endif
}

static void set_exec_readonly(void* ptr, size_t size) {
#ifdef _WIN32
    DWORD old;
    VirtualProtect(ptr, size, PAGE_EXECUTE_READ, &old);
#else
    mprotect(ptr, size, PROT_READ | PROT_EXEC);
#endif
}

/* ── Platform-specific trampoline generation ── */

#if defined(_M_ARM64) || defined(__aarch64__)

/* ARM64 trampoline:
   adrp x16, <page>   ; load page of handler address
   add  x16, x16, <offset> ; add page offset
   br   x16           ; branch to handler
   (20 bytes) */
static bool create_trampoline(CallbackSlot* slot, void* handler) {
    void* code = alloc_exec_memory(64);
    if (!code) return false;
    unsigned char* p = (unsigned char*)code;
    uint64_t addr = reinterpret_cast<uint64_t>(handler);

    /* ADRP X16, handler_page */
    uint64_t page = addr & ~0xFFFULL;
    uint64_t pc = reinterpret_cast<uint64_t>(code) & ~0xFFFULL;
    int64_t delta = static_cast<int64_t>(page - pc);
    uint32_t imm = (delta >> 12) & 0x3FFFF;
    uint32_t adrp = 0x90000010 | ((imm >> 0) & 3) << 29 | ((imm >> 2) & 0x7FFFF) << 5;
    memcpy(p, &adrp, 4);

    /* ADD X16, X16, low12_page_offset */
    int64_t pg_offset = addr & 0xFFF;
    uint32_t add = 0x91000210 | (pg_offset << 10);
    memcpy(p + 4, &add, 4);

    /* BR X16 */
    uint32_t br = 0xD61F0200 | (16 << 5);
    memcpy(p + 8, &br, 4);

    for (int i = 12; i < 64; i++) p[i] = 0xCC;

    set_exec_readonly(code, 64);
    slot->trampoline = code;
    slot->handler = handler;
    return true;
}

#elif defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)

/* x64 trampoline: mov rax, handler; jmp rax  (12 bytes) */
static bool create_trampoline(CallbackSlot* slot, void* handler) {
    void* code = alloc_exec_memory(64);
    if (!code) return false;

    unsigned char* p = (unsigned char*)code;
    p[0] = 0x48; p[1] = 0xB8;
    memcpy(p + 2, &handler, sizeof(void*));
    p[10] = 0xFF; p[11] = 0xE0;
    for (int i = 12; i < 64; i++) p[i] = 0xCC;

    set_exec_readonly(code, 64);
    slot->trampoline = code;
    slot->handler = handler;
    return true;
}

#else
#error "Unsupported architecture for FFI callback trampolines (only x64 and ARM64 supported)"
#endif

static void destroy_trampoline(CallbackSlot* slot) {
    if (slot->trampoline) {
        free_exec_memory(slot->trampoline, 64);
        slot->trampoline = nullptr;
    }
    slot->handler = nullptr;
    slot->in_use = false;
    slot->user_data = nullptr;
}

/* Create a callback trampoline that calls the given handler.
   Returns the trampoline function pointer (suitable as C callback). */
void* aurora_callback_create(void* handler, void* user_data) {
    std::lock_guard<aurora_mutex> lock(g_callback_mutex);

    /* First try to find an existing free slot */
    for (size_t i = 0; i < callback_slots.size(); i++) {
        if (!callback_slots[i].in_use) {
            if (!create_trampoline(&callback_slots[i], handler)) {
                fprintf(stderr, "[ffi] callback_create: trampoline allocation failed (slot %zu)\n", i);
                return nullptr;
            }
            callback_slots[i].in_use = true;
            callback_slots[i].user_data = user_data;
            return callback_slots[i].trampoline;
        }
    }

    /* No free slot found — grow the vector */
    CallbackSlot new_slot;
    if (!create_trampoline(&new_slot, handler)) {
        fprintf(stderr, "[ffi] callback_create: trampoline allocation failed\n");
        return nullptr;
    }
    new_slot.in_use = true;
    new_slot.user_data = user_data;
    callback_slots.push_back(new_slot);
    return new_slot.trampoline;
}

/* Destroy a previously created callback (free trampoline). */
void aurora_callback_destroy(void* trampoline) {
    if (!trampoline) return;
    std::lock_guard<aurora_mutex> lock(g_callback_mutex);
    for (size_t i = 0; i < callback_slots.size(); i++) {
        if (callback_slots[i].trampoline == trampoline) {
            destroy_trampoline(&callback_slots[i]);
            return;
        }
    }
    fprintf(stderr, "[ffi] callback_destroy: trampoline %p not found\n", trampoline);
}

/* Get the handler associated with a trampoline (for internal routing). */
void* aurora_callback_get_handler(void* trampoline) {
    if (!trampoline) return nullptr;
    std::lock_guard<aurora_mutex> lock(g_callback_mutex);
    for (size_t i = 0; i < callback_slots.size(); i++) {
        if (callback_slots[i].trampoline == trampoline)
            return callback_slots[i].handler;
    }
    return nullptr;
}

/* Destroy all callbacks (call on shutdown). */
void aurora_callback_cleanup(void) {
    std::lock_guard<aurora_mutex> lock(g_callback_mutex);
    for (size_t i = 0; i < callback_slots.size(); i++) {
        if (callback_slots[i].in_use)
            destroy_trampoline(&callback_slots[i]);
    }
}

/* ── Shared Python Runtime (process-wide) ── */
/* All PyPI bridge DLLs share this single Python instance to avoid
   multi-interpreter conflicts. Uses a mutex for thread safety. */

#ifdef _WIN32
#include <windows.h>
static std::atomic<HMODULE> s_python_dll{NULL};
static std::atomic<int> s_python_inited{0};
static CRITICAL_SECTION s_py_lock;  /* initialized via std::once below */
static std::once_flag s_py_lock_init_flag;

static void py_lock_init_impl(void) {
    InitializeCriticalSection(&s_py_lock);
}
static void py_lock_init(void) {
    std::call_once(s_py_lock_init_flag, py_lock_init_impl);
}
#else
#include <dlfcn.h>
#include <pthread.h>
static std::atomic<void*> s_python_dll{NULL};
static std::atomic<int> s_python_inited{0};
/* Use recursive mutex to prevent deadlock if Py_Initialize() re-enters */
#if defined(__linux__)
    static pthread_mutex_t s_py_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
#else
    static pthread_mutex_t s_py_lock = PTHREAD_MUTEX_INITIALIZER;
#endif
static std::once_flag s_py_lock_init_flag;
static void py_lock_init(void) {}
#endif

/* Initialize Python once per process. Returns 1 on success, 0 on failure.
   Thread-safe: subsequent calls return cached status. */
#if defined(_MSC_VER)
extern "C" __declspec(dllexport) int aurora_py_ensure_initialized(void) {
#else
extern "C" int aurora_py_ensure_initialized(void) {
#endif
    py_lock_init();
#ifdef _WIN32
    EnterCriticalSection(&s_py_lock);
#else
    pthread_mutex_lock(&s_py_lock);
#endif
    if (s_python_inited) {
#ifdef _WIN32
        LeaveCriticalSection(&s_py_lock);
#else
        pthread_mutex_unlock(&s_py_lock);
#endif
        return 1;
    }
#ifdef _WIN32
    /* Try versioned DLLs in descending order, then stable forwarder as fallback */
    const char* candidates[] = {
        "python314.dll", "python313.dll", "python312.dll",
        "python311.dll", "python310.dll", "python39.dll", "python38.dll",
        "python3.14.dll", "python3.13.dll", "python3.12.dll",
        "python3.11.dll", "python3.10.dll", "python3.9.dll", "python3.8.dll",
        "python3.dll",
        NULL
    };
    for (int i = 0; candidates[i]; i++) {
        s_python_dll = LoadLibraryA(candidates[i]);
        if (s_python_dll) {
            fprintf(stderr, "[python] loaded %s\n", candidates[i]);
            break;
        }
    }
    if (!s_python_dll) {
        fprintf(stderr, "[python] ERROR: no Python DLL found\n");
#ifdef _WIN32
        LeaveCriticalSection(&s_py_lock);
#else
        pthread_mutex_unlock(&s_py_lock);
#endif
        return 0;
    }
    typedef void (*PyInitFn)(void);
#ifdef _WIN32
    PyInitFn Py_Initialize = (PyInitFn)GetProcAddress(s_python_dll, "Py_Initialize");
#else
    PyInitFn Py_Initialize = (PyInitFn)dlsym(s_python_dll, "Py_Initialize");
#endif
    if (!Py_Initialize) {
        fprintf(stderr, "[python] ERROR: Py_Initialize not found\n");
#ifdef _WIN32
        LeaveCriticalSection(&s_py_lock);
#else
        pthread_mutex_unlock(&s_py_lock);
#endif
        return 0;
    }
    Py_Initialize();
    s_python_inited = 1;
    fprintf(stderr, "[python] shared runtime initialized\n");
#ifdef _WIN32
    LeaveCriticalSection(&s_py_lock);
#else
    pthread_mutex_unlock(&s_py_lock);
#endif
    return 1;
#else
    const char* candidates[] = {
        "libpython3.so", "libpython3.14.so", "libpython3.13.so", "libpython3.12.so",
        "libpython3.11.so", "libpython3.10.so", "libpython3.9.so", "libpython3.8.so",
        "libpython3.14.dylib", "libpython3.13.dylib", "libpython3.12.dylib",
        "libpython3.11.dylib", "libpython3.10.dylib",
        NULL
    };
    for (int i = 0; candidates[i]; i++) {
        s_python_dll = dlopen(candidates[i], RTLD_LAZY | RTLD_GLOBAL);
        if (s_python_dll) {
            fprintf(stderr, "[python] loaded %s\n", candidates[i]);
            break;
        }
    }
    if (!s_python_dll) {
        fprintf(stderr, "[python] ERROR: no Python shared library found\n");
        pthread_mutex_unlock(&s_py_lock);
        return 0;
    }
    PyInitFn Py_Initialize = (PyInitFn)dlsym(s_python_dll, "Py_Initialize");
    if (!Py_Initialize) {
        fprintf(stderr, "[python] ERROR: Py_Initialize not found\n");
        pthread_mutex_unlock(&s_py_lock);
        return 0;
    }
    Py_Initialize();
    s_python_inited = 1;
    fprintf(stderr, "[python] shared runtime initialized\n");
    pthread_mutex_unlock(&s_py_lock);
    return 1;
#endif
}

/* Get Python C API function pointer. Thread-safe. Returns NULL on failure. */
#if defined(_MSC_VER)
extern "C" __declspec(dllexport) void* aurora_py_get_api(const char* name) {
#else
extern "C" void* aurora_py_get_api(const char* name) {
#endif
    if (!s_python_dll) {
        if (!aurora_py_ensure_initialized()) return NULL;
    }
#ifdef _WIN32
    return (void*)GetProcAddress(s_python_dll, name);
#else
    return dlsym(s_python_dll, name);
#endif
}

/* ── Register dl handle cleanup on process exit ── */
namespace { struct DlCleanup { ~DlCleanup() { aurora_dl_cleanup_all(); } }; }
static DlCleanup g_dl_cleanup;

} /* extern "C" */
