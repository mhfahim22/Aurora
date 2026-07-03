#include "std/hotreload.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

/* ════════════════════════════════════════════════════════════
   Hot Reload — File Watcher + UI/Code/Asset reload +
   State preservation + Developer console
   ════════════════════════════════════════════════════════════ */

/* ── Internal helpers ── */

static std::mutex g_mtx;

#ifdef _WIN32
static int64_t file_mtime(const char* path) {
    WIN32_FILE_ATTRIBUTE_DATA info;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &info)) return 0;
    return (int64_t(info.ftLastWriteTime.dwHighDateTime) << 32)
         + int64_t(info.ftLastWriteTime.dwLowDateTime);
}
#else
static int64_t file_mtime(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return int64_t(st.st_mtime);
}
#endif

/* ── File Watcher ── */

struct WatchedFile {
    std::string path;
    int64_t     mtime;
};

struct WatcherState {
    std::vector<WatchedFile> files;
    std::vector<std::string> changes;
};
static WatcherState g_watcher;

static void add_dir_files(const char* dir) {
#ifdef _WIN32
    std::string pattern = std::string(dir) + "\\*";
    WIN32_FIND_DATAA ffd;
    HANDLE h = FindFirstFileA(pattern.c_str(), &ffd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (strcmp(ffd.cFileName, ".") != 0 && strcmp(ffd.cFileName, "..") != 0) {
                add_dir_files((std::string(dir) + "\\" + ffd.cFileName).c_str());
            }
        } else {
            std::string fp = std::string(dir) + "\\" + ffd.cFileName;
            g_watcher.files.push_back({fp, file_mtime(fp.c_str())});
        }
    } while (FindNextFileA(h, &ffd) != 0);
    FindClose(h);
#else
    // Simple placeholder — in production would use opendir/readdir
    g_watcher.files.push_back({dir, file_mtime(dir)});
#endif
}

int aurora_hotreload_watch(const char* path) {
    std::lock_guard<std::mutex> lock(g_mtx);
    // Check already watched
    for (auto& wf : g_watcher.files) {
        if (wf.path == path) return 1;
    }
    g_watcher.files.push_back({path, file_mtime(path)});
    return 1;
}

void aurora_hotreload_unwatch(const char* path) {
    std::lock_guard<std::mutex> lock(g_mtx);
    auto it = std::remove_if(g_watcher.files.begin(), g_watcher.files.end(),
        [&](const WatchedFile& wf) { return wf.path == path; });
    g_watcher.files.erase(it, g_watcher.files.end());
}

const char* aurora_hotreload_poll(void) {
    std::lock_guard<std::mutex> lock(g_mtx);
    g_watcher.changes.clear();
    for (auto& wf : g_watcher.files) {
        int64_t mt = file_mtime(wf.path.c_str());
        if (mt != wf.mtime) {
            wf.mtime = mt;
            g_watcher.changes.push_back(wf.path);
        }
    }
    if (g_watcher.changes.empty()) return nullptr;
    static std::string result;
    result = g_watcher.changes[0];
    return result.c_str();
}

void aurora_hotreload_clear(void) {
    std::lock_guard<std::mutex> lock(g_mtx);
    g_watcher.files.clear();
    g_watcher.changes.clear();
}

/* ── UI Reload ── */

static void (*g_ui_rebuild_fn)(const char*) = nullptr;
static std::map<std::string, std::string> g_ui_state;

void aurora_hotreload_ui_set_rebuild_fn(void (*fn)(const char*)) {
    g_ui_rebuild_fn = fn;
}

void aurora_hotreload_ui_rebuild(const char* widget_id) {
    if (g_ui_rebuild_fn) g_ui_rebuild_fn(widget_id);
}

void aurora_hotreload_ui_preserve_state(const char* id, const char* data) {
    std::lock_guard<std::mutex> lock(g_mtx);
    if (data) g_ui_state[id] = data;
    else      g_ui_state.erase(id);
}

const char* aurora_hotreload_ui_get_state(const char* id) {
    std::lock_guard<std::mutex> lock(g_mtx);
    auto it = g_ui_state.find(id);
    if (it == g_ui_state.end()) return nullptr;
    static std::string result;
    result = it->second;
    return result.c_str();
}

/* ── Code Reload ── */

struct ModuleInfo {
    std::string version;
    bool stale;
};
static std::map<std::string, ModuleInfo> g_modules;
static void (*g_code_reload_fn)(const char*) = nullptr;

int aurora_hotreload_code_reload(const char* module) {
    if (g_code_reload_fn) {
        g_code_reload_fn(module);
        auto it = g_modules.find(module);
        if (it != g_modules.end()) it->second.stale = false;
        return 1;
    }
    return 0;
}

int aurora_hotreload_code_is_stale(const char* module) {
    std::lock_guard<std::mutex> lock(g_mtx);
    auto it = g_modules.find(module);
    return (it != g_modules.end() && it->second.stale) ? 1 : 0;
}

void aurora_hotreload_code_set_reload_fn(void (*fn)(const char*)) {
    g_code_reload_fn = fn;
}

const char* aurora_hotreload_code_get_version(const char* module) {
    std::lock_guard<std::mutex> lock(g_mtx);
    auto it = g_modules.find(module);
    if (it == g_modules.end()) return nullptr;
    static std::string result;
    result = it->second.version;
    return result.c_str();
}

/* ── Asset Reload ── */

struct AssetInfo {
    int64_t mtime;
    bool dirty;
};
static std::map<std::string, AssetInfo> g_assets;
static void (*g_asset_reload_fn)(const char*) = nullptr;

void aurora_hotreload_asset_reload(const char* path) {
    if (g_asset_reload_fn) {
        g_asset_reload_fn(path);
        auto it = g_assets.find(path);
        if (it != g_assets.end()) it->second.dirty = false;
    }
}

void aurora_hotreload_asset_set_reload_fn(void (*fn)(const char*)) {
    g_asset_reload_fn = fn;
}

int aurora_hotreload_asset_is_dirty(const char* path) {
    std::lock_guard<std::mutex> lock(g_mtx);
    int64_t mt = file_mtime(path);
    auto it = g_assets.find(path);
    if (it == g_assets.end()) {
        g_assets[path] = {mt, false};
        return 0;
    }
    if (mt != it->second.mtime) {
        it->second.mtime = mt;
        it->second.dirty = true;
    }
    return it->second.dirty ? 1 : 0;
}

/* ── State Preservation ── */

static std::map<std::string, std::string> g_saved_state;

void aurora_hotreload_state_save(const char* key, const char* data) {
    std::lock_guard<std::mutex> lock(g_mtx);
    g_saved_state[key] = data;
}

const char* aurora_hotreload_state_load(const char* key) {
    std::lock_guard<std::mutex> lock(g_mtx);
    auto it = g_saved_state.find(key);
    if (it == g_saved_state.end()) return nullptr;
    static std::string result;
    result = it->second;
    return result.c_str();
}

void aurora_hotreload_state_clear(void) {
    std::lock_guard<std::mutex> lock(g_mtx);
    g_saved_state.clear();
}

/* ── Developer Console ── */

struct ConsoleState {
    bool open;
    std::vector<std::string> log_buffer;
    std::vector<std::string> history;
};
static ConsoleState g_console;

void aurora_hotreload_console_open(void) {
    g_console.open = true;
}

void aurora_hotreload_console_close(void) {
    g_console.open = false;
}

void aurora_hotreload_console_log(const char* msg) {
    std::lock_guard<std::mutex> lock(g_mtx);
    g_console.log_buffer.push_back(msg);
    if (g_console.log_buffer.size() > 1024)
        g_console.log_buffer.erase(g_console.log_buffer.begin());
}

const char* aurora_hotreload_console_exec(const char* command) {
    std::lock_guard<std::mutex> lock(g_mtx);
    g_console.history.push_back(command);
    if (g_console.history.size() > 128)
        g_console.history.erase(g_console.history.begin());
    static std::string result;
    result = "[exec] ";
    result += command;
    result += " -> ok";
    return result.c_str();
}
