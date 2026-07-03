#include "std/plugin.hpp"
#include "runtime/reflection.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#include <dirent.h>
#endif

#ifndef AURORA_VERSION
#define AURORA_VERSION "1.0.0"
#endif

extern "C" {

/* ═══════════════════════════════════════════
 *  Plugin Registry
 * ═══════════════════════════════════════════ */

#define MAX_PLUGINS 64

struct PluginEntry {
    char    name[64];
    void*   handle;
    int     loaded;
    int     abi_version;
};

static PluginEntry g_plugins[MAX_PLUGINS];
static int g_plugin_count = 0;

static int find_plugin(const char* name) {
    if (!name) return -1;
    for (int i = 0; i < g_plugin_count; i++)
        if (strcmp(g_plugins[i].name, name) == 0)
            return i;
    return -1;
}

/* ═══════════════════════════════════════════
 *  Plugin Management
 * ═══════════════════════════════════════════ */

int aurora_plugin_load(const char* path) {
    if (!path || !*path) return 0;
    if (g_plugin_count >= MAX_PLUGINS) return 0;

#ifdef _WIN32
    void* handle = LoadLibraryA(path);
#else
    void* handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
    if (!handle) return 0;

    /* Resolve required plugin symbols */
#ifdef _WIN32
    typedef const char* (*StrFn)();
    typedef int (*IntFn)();
    StrFn name_fn = (StrFn)GetProcAddress((HMODULE)handle, "aurora_plugin_name");
    StrFn ver_fn  = (StrFn)GetProcAddress((HMODULE)handle, "aurora_plugin_version");
    IntFn abi_fn  = (IntFn)GetProcAddress((HMODULE)handle, "aurora_plugin_abi_version");
    IntFn init_fn = (IntFn)GetProcAddress((HMODULE)handle, "aurora_plugin_init");
#else
    typedef const char* (*StrFn)();
    typedef int (*IntFn)();
    StrFn name_fn = (StrFn)dlsym(handle, "aurora_plugin_name");
    StrFn ver_fn  = (StrFn)dlsym(handle, "aurora_plugin_version");
    IntFn abi_fn  = (IntFn)dlsym(handle, "aurora_plugin_abi_version");
    IntFn init_fn = (IntFn)dlsym(handle, "aurora_plugin_init");
#endif

    if (!name_fn || !abi_fn) {
#ifdef _WIN32
        FreeLibrary((HMODULE)handle);
#else
        dlclose(handle);
#endif
        return 0;
    }

    const char* pname = name_fn();
    if (!pname || !*pname) {
#ifdef _WIN32
        FreeLibrary((HMODULE)handle);
#else
        dlclose(handle);
#endif
        return 0;
    }

    /* Check ABI version compatibility */
    int abi = abi_fn();
    if (abi != AURORA_PLUGIN_ABI_VERSION) {
#ifdef _WIN32
        FreeLibrary((HMODULE)handle);
#else
        dlclose(handle);
#endif
        return 0;
    }

    /* Check for duplicates */
    if (find_plugin(pname) >= 0) {
#ifdef _WIN32
        FreeLibrary((HMODULE)handle);
#else
        dlclose(handle);
#endif
        return 0;
    }

    /* Register plugin */
    int idx = g_plugin_count++;
    strncpy(g_plugins[idx].name, pname, sizeof(g_plugins[idx].name) - 1);
    g_plugins[idx].name[sizeof(g_plugins[idx].name) - 1] = '\0';
    g_plugins[idx].handle = handle;
    g_plugins[idx].loaded = 1;
    g_plugins[idx].abi_version = abi;

    /* Call init if available */
    if (init_fn) {
        if (!init_fn()) {
            /* Init failed — unload */
            g_plugin_count--;
#ifdef _WIN32
            FreeLibrary((HMODULE)handle);
#else
            dlclose(handle);
#endif
            return 0;
        }
    }

    return 1;
}

int aurora_plugin_unload(const char* name) {
    int idx = find_plugin(name);
    if (idx < 0) return 0;

#ifdef _WIN32
    typedef void (*VoidFn)();
    VoidFn shutdown_fn = (VoidFn)GetProcAddress((HMODULE)g_plugins[idx].handle, "aurora_plugin_shutdown");
    if (shutdown_fn) shutdown_fn();
    FreeLibrary((HMODULE)g_plugins[idx].handle);
#else
    typedef void (*VoidFn)();
    VoidFn shutdown_fn = (VoidFn)dlsym(g_plugins[idx].handle, "aurora_plugin_shutdown");
    if (shutdown_fn) shutdown_fn();
    dlclose(g_plugins[idx].handle);
#endif

    g_plugin_count--;
    if (idx < g_plugin_count)
        g_plugins[idx] = g_plugins[g_plugin_count];
    memset(&g_plugins[g_plugin_count], 0, sizeof(PluginEntry));
    return 1;
}

int aurora_plugin_unload_all() {
    int count = g_plugin_count;
    while (g_plugin_count > 0)
        aurora_plugin_unload(g_plugins[0].name);
    return count;
}

int aurora_plugin_get_count() {
    return g_plugin_count;
}

const char* aurora_plugin_get_name(int index) {
    if (index < 0 || index >= g_plugin_count) return nullptr;
    return g_plugins[index].name;
}

int aurora_plugin_is_loaded(const char* name) {
    return find_plugin(name) >= 0 ? 1 : 0;
}

int aurora_plugin_scan(const char* directory) {
    if (!directory || !*directory) return 0;
    int loaded = 0;

#ifdef _WIN32
    char pattern[512];
    snprintf(pattern, sizeof(pattern), "%s\\*.dll", directory);
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(pattern, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return 0;
    do {
        char full[512];
        snprintf(full, sizeof(full), "%s\\%s", directory, ffd.cFileName);
        if (aurora_plugin_load(full)) loaded++;
    } while (FindNextFileA(hFind, &ffd) != 0);
    FindClose(hFind);
#else
    DIR* dir = opendir(directory);
    if (!dir) return 0;
    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        const char* ext = strrchr(ent->d_name, '.');
        if (ext && (strcmp(ext, ".so") == 0 || strcmp(ext, ".dylib") == 0)) {
            char full[512];
            snprintf(full, sizeof(full), "%s/%s", directory, ent->d_name);
            if (aurora_plugin_load(full)) loaded++;
        }
    }
    closedir(dir);
#endif
    return loaded;
}

/* ═══════════════════════════════════════════
 *  Plugin Metadata
 * ═══════════════════════════════════════════ */

const char* aurora_plugin_get_info(const char* name, const char* field) {
    int idx = find_plugin(name);
    if (idx < 0) return nullptr;
    if (!field) return nullptr;

#ifdef _WIN32
    typedef const char* (*StrFn)();
    if (strcmp(field, "name") == 0) {
        StrFn fn = (StrFn)GetProcAddress((HMODULE)g_plugins[idx].handle, "aurora_plugin_name");
        return fn ? fn() : nullptr;
    }
    if (strcmp(field, "version") == 0) {
        StrFn fn = (StrFn)GetProcAddress((HMODULE)g_plugins[idx].handle, "aurora_plugin_version");
        return fn ? fn() : nullptr;
    }
    if (strcmp(field, "author") == 0) {
        StrFn fn = (StrFn)GetProcAddress((HMODULE)g_plugins[idx].handle, "aurora_plugin_author");
        return fn ? fn() : nullptr;
    }
    if (strcmp(field, "description") == 0) {
        StrFn fn = (StrFn)GetProcAddress((HMODULE)g_plugins[idx].handle, "aurora_plugin_description");
        return fn ? fn() : nullptr;
    }
#else
    typedef const char* (*StrFn)();
    if (strcmp(field, "name") == 0) {
        StrFn fn = (StrFn)dlsym(g_plugins[idx].handle, "aurora_plugin_name");
        return fn ? fn() : nullptr;
    }
    if (strcmp(field, "version") == 0) {
        StrFn fn = (StrFn)dlsym(g_plugins[idx].handle, "aurora_plugin_version");
        return fn ? fn() : nullptr;
    }
    if (strcmp(field, "author") == 0) {
        StrFn fn = (StrFn)dlsym(g_plugins[idx].handle, "aurora_plugin_author");
        return fn ? fn() : nullptr;
    }
    if (strcmp(field, "description") == 0) {
        StrFn fn = (StrFn)dlsym(g_plugins[idx].handle, "aurora_plugin_description");
        return fn ? fn() : nullptr;
    }
#endif
    return nullptr;
}

int aurora_plugin_get_abi(const char* name) {
    int idx = find_plugin(name);
    return (idx >= 0) ? g_plugins[idx].abi_version : -1;
}

void* aurora_plugin_get_function(const char* plugin, const char* func) {
    int idx = find_plugin(plugin);
    if (idx < 0) return nullptr;
    if (!func) return nullptr;
#ifdef _WIN32
    return (void*)GetProcAddress((HMODULE)g_plugins[idx].handle, func);
#else
    return dlsym(g_plugins[idx].handle, func);
#endif
}

/* ═══════════════════════════════════════════
 *  Reflection Query
 * ═══════════════════════════════════════════ */

int aurora_reflection_get_type_count() {
    auto& reg = ReflectionRegistry::instance();
    auto names = reg.get_type_names();
    return (int)names.size();
}

const char* aurora_reflection_get_type_name(int index) {
    auto& reg = ReflectionRegistry::instance();
    auto names = reg.get_type_names();
    if (index < 0 || index >= (int)names.size()) return nullptr;
    static std::string cached;
    cached = names[index];
    return cached.c_str();
}

int aurora_reflection_get_field_count(const char* type_name) {
    if (!type_name) return 0;
    auto& reg = ReflectionRegistry::instance();
    auto fields = reg.get_fields(type_name);
    return (int)fields.size();
}

void aurora_reflection_get_field_info(const char* type_name, int index,
                                       char** name, char** type, int* offset, int* size) {
    if (!type_name) return;
    auto& reg = ReflectionRegistry::instance();
    auto fields = reg.get_fields(type_name);
    if (index < 0 || index >= (int)fields.size()) return;
    if (name)   *name   = (char*)fields[index].name.c_str();
    if (type)   *type   = (char*)fields[index].type_name.c_str();
    if (offset) *offset = (int)fields[index].offset;
    if (size)   *size   = (int)fields[index].size;
}

int aurora_reflection_get_method_count(const char* type_name) {
    if (!type_name) return 0;
    auto& reg = ReflectionRegistry::instance();
    auto methods = reg.get_methods(type_name);
    return (int)methods.size();
}

void aurora_reflection_get_method_info(const char* type_name, int index,
                                        char** name, char** return_type) {
    if (!type_name) return;
    auto& reg = ReflectionRegistry::instance();
    auto methods = reg.get_methods(type_name);
    if (index < 0 || index >= (int)methods.size()) return;
    if (name)        *name        = (char*)methods[index].name.c_str();
    if (return_type) *return_type = (char*)methods[index].return_type.c_str();
}

void* aurora_reflection_get_method_pointer(const char* type_name, const char* method_name) {
    if (!type_name || !method_name) return nullptr;
    auto& reg = ReflectionRegistry::instance();
    auto methods = reg.get_methods(type_name);
    for (auto& m : methods)
        if (m.name == method_name)
            return m.fn_ptr;
    return nullptr;
}

/* ═══════════════════════════════════════════
 *  Version
 * ═══════════════════════════════════════════ */

int aurora_version_abi() {
    return AURORA_PLUGIN_ABI_VERSION;
}

const char* aurora_version_string() {
    return AURORA_VERSION;
}

} /* extern "C" */
