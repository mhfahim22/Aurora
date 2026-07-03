#include "std/pkg.hpp"
#include "std/json.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#define PATH_SEP '\\'
#define popen _popen
#define pclose _pclose
#else
#include <unistd.h>
#include <sys/stat.h>
#include <pwd.h>
#include <dirent.h>
#define PATH_SEP '/'
#endif

/* ── Global State ── */

static char g_registry_url[1024] = "";
static char g_auth_token[4096] = "";
static std::string g_aurora_dir;
static std::string g_packages_dir;
static std::string g_cache_dir;
static std::string g_auth_path;
static std::string g_lock_path;     /* CWD/package-lock.json */
static std::vector<std::string> g_dep_names;
static bool g_paths_inited = false;

/* ── Path Helpers ── */

static const char* get_home_dir() {
#ifdef _WIN32
    static char buf[MAX_PATH];
    DWORD ret = GetEnvironmentVariableA("USERPROFILE", buf, sizeof(buf));
    if (ret > 0 && ret < sizeof(buf)) return buf;
    ret = GetEnvironmentVariableA("HOMEDRIVE", buf, sizeof(buf));
    if (ret > 0 && ret < sizeof(buf)) {
        std::string drive = buf;
        ret = GetEnvironmentVariableA("HOMEPATH", buf, sizeof(buf));
        if (ret > 0 && ret < sizeof(buf))
            drive += buf;
        if (!drive.empty()) {
            strncpy_s(buf, drive.c_str(), sizeof(buf) - 1);
            return buf;
        }
    }
    return "C:\\Users\\Default";
#else
    static char buf[1024];
    const char* home = getenv("HOME");
    if (home) return home;
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir) return pw->pw_dir;
    return "/tmp";
#endif
}

static void ensure_paths() {
    if (g_paths_inited) return;
    g_paths_inited = true;
    std::string home = get_home_dir();
    g_aurora_dir = home + PATH_SEP + ".aurora";
    g_packages_dir = g_aurora_dir + PATH_SEP + "packages";
    g_cache_dir = g_aurora_dir + PATH_SEP + "cache";
    g_auth_path = g_aurora_dir + PATH_SEP + "auth.json";

    if (g_registry_url[0] == '\0')
        snprintf(g_registry_url, sizeof(g_registry_url), "https://aurora-packages.dev/api/v1");
}

static int mkdir_p(const std::string& path) {
#ifdef _WIN32
    return _mkdir(path.c_str());
#else
    return mkdir(path.c_str(), 0755);
#endif
}

static void ensure_dir(const std::string& path) {
    mkdir_p(g_aurora_dir);
    mkdir_p(g_packages_dir);
    mkdir_p(g_cache_dir);
}

static std::string read_file_str(const std::string& path) {
    FILE* f = nullptr;
#ifdef _WIN32
    fopen_s(&f, path.c_str(), "rb");
#else
    f = fopen(path.c_str(), "rb");
#endif
    if (!f) return "";
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    if (len <= 0) { fclose(f); return ""; }
    fseek(f, 0, SEEK_SET);
    std::string out((size_t)len, '\0');
    fread(&out[0], 1, (size_t)len, f);
    fclose(f);
    return out;
}

static int write_file_str(const std::string& path, const std::string& data) {
    FILE* f = nullptr;
#ifdef _WIN32
    fopen_s(&f, path.c_str(), "wb");
#else
    f = fopen(path.c_str(), "wb");
#endif
    if (!f) return 0;
    fwrite(data.data(), 1, data.size(), f);
    fclose(f);
    return 1;
}

static int voss_shell(const char* fmt, ...) {
    char cmd[8192];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);

    FILE* pipe = popen(cmd, "r");
    if (!pipe) return -1;
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) { }
    return pclose(pipe);
}

static std::string voss_capture(const char* fmt, ...) {
    char cmd[8192];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);

    std::string result;
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return "";
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), pipe)) > 0)
        result.append(buf, n);
    pclose(pipe);
    return result;
}

/* ── Package Management (6) ── */

int aurora_pkg_install(const char* name, const char* version) {
    if (!name || !*name) return 0;
    ensure_paths();
    ensure_dir("");
    int ret;
    if (version && *version) {
        char spec[2048];
        snprintf(spec, sizeof(spec), "%s@%s", name, version);
        ret = voss_shell("voss install \"%s\" 2>nul", spec);
    } else {
        ret = voss_shell("voss install \"%s\" 2>nul", name);
    }
    return (ret == 0) ? 1 : 0;
}

int aurora_pkg_remove(const char* name) {
    if (!name || !*name) return 0;
    ensure_paths();
    int ret = voss_shell("voss uninstall \"%s\" 2>nul", name);
    return (ret == 0) ? 1 : 0;
}

int aurora_pkg_update(const char* name) {
    ensure_paths();
    int ret;
    if (name && *name)
        ret = voss_shell("voss update \"%s\" 2>nul", name);
    else
        ret = voss_shell("voss update --all 2>nul");
    return (ret == 0) ? 1 : 0;
}

int aurora_pkg_publish(const char* path) {
    if (!path || !*path) return 0;
    ensure_paths();
    int ret = voss_shell("voss publish \"%s\" 2>nul", path);
    return (ret == 0) ? 1 : 0;
}

int aurora_pkg_search(const char* query, char*** out_results, int* out_count) {
    if (!query || !out_results || !out_count) return 0;
    ensure_paths();
    std::string raw = voss_capture("voss search \"%s\" 2>nul", query);
    if (raw.empty()) return 0;

    std::vector<std::string> lines;
    size_t start = 0, end;
    while ((end = raw.find('\n', start)) != std::string::npos) {
        std::string line = raw.substr(start, end - start);
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();
        if (!line.empty()) lines.push_back(line);
        start = end + 1;
    }
    if (start < raw.size()) {
        std::string line = raw.substr(start);
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();
        if (!line.empty()) lines.push_back(line);
    }

    int n = (int)lines.size();
    char** arr = (char**)malloc(sizeof(char*) * (n + 1));
    if (!arr) return 0;
    for (int i = 0; i < n; i++) {
        arr[i] = (char*)malloc(lines[i].size() + 1);
        if (arr[i]) {
            memcpy(arr[i], lines[i].data(), lines[i].size());
            arr[i][lines[i].size()] = '\0';
        }
    }
    arr[n] = nullptr;
    *out_results = arr;
    *out_count = n;
    return 1;
}

int aurora_pkg_list_installed(char*** out_names, int* out_count) {
    if (!out_names || !out_count) return 0;
    ensure_paths();
    ensure_dir("");

    std::vector<std::string> pkgs;
#ifdef _WIN32
    std::string pattern = g_packages_dir + "\\*";
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &ffd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (strcmp(ffd.cFileName, ".") != 0 && strcmp(ffd.cFileName, "..") != 0)
                    pkgs.push_back(ffd.cFileName);
            }
        } while (FindNextFileA(hFind, &ffd) != 0);
        FindClose(hFind);
    }
#else
    DIR* dir = opendir(g_packages_dir.c_str());
    if (dir) {
        struct dirent* ent;
        while ((ent = readdir(dir)) != nullptr) {
            if (ent->d_type == DT_DIR || ent->d_type == DT_UNKNOWN) {
                if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0)
                    pkgs.push_back(ent->d_name);
            }
        }
        closedir(dir);
    }
#endif

    int n = (int)pkgs.size();
    char** arr = (char**)malloc(sizeof(char*) * (n + 1));
    if (!arr) return 0;
    for (int i = 0; i < n; i++) {
        arr[i] = (char*)malloc(pkgs[i].size() + 1);
        if (arr[i]) {
            memcpy(arr[i], pkgs[i].data(), pkgs[i].size());
            arr[i][pkgs[i].size()] = '\0';
        }
    }
    arr[n] = nullptr;
    *out_names = arr;
    *out_count = n;
    return 1;
}

/* ── Registry (2) ── */

void aurora_pkg_set_registry(const char* url) {
    if (url)
        strncpy_s(g_registry_url, url, sizeof(g_registry_url) - 1);
    else
        g_registry_url[0] = '\0';
}

const char* aurora_pkg_get_registry(void) {
    ensure_paths();
    if (g_registry_url[0] == '\0')
        return "https://aurora-packages.dev/api/v1";
    return g_registry_url;
}

/* ── Authentication (1) ── */

int aurora_pkg_login(const char* token) {
    if (!token || !*token) return 0;
    ensure_paths();
    ensure_dir("");
    if (!g_aurora_dir.empty() && g_aurora_dir.back() != PATH_SEP) {
        JsonValue* root = aurora_json_new_object();
        aurora_json_set_str(root, "token", token);
        char* json = aurora_json_serialize(root);
        int ok = write_file_str(g_auth_path, json) ? 1 : 0;
        free(json);
        aurora_json_free(root);
        if (ok) {
            strncpy_s(g_auth_token, token, sizeof(g_auth_token) - 1);
            return 1;
        }
    }
    return 0;
}

/* ── Lock File (3) ── */

int aurora_pkg_lock_init(void) {
    ensure_paths();
    std::string cwd;
    char buf[4096];
#ifdef _WIN32
    if (_getcwd(buf, sizeof(buf))) cwd = buf;
#else
    if (getcwd(buf, sizeof(buf))) cwd = buf;
#endif
    if (cwd.empty()) return 0;
    g_lock_path = cwd + PATH_SEP + "package-lock.json";

    JsonValue* root = aurora_json_new_object();
    aurora_json_set(root, "version", 1);
    aurora_json_set_str(root, "registry", aurora_pkg_get_registry());
    aurora_json_set_obj(root, "packages", aurora_json_new_object());
    char* json = aurora_json_serialize(root);
    int ok = write_file_str(g_lock_path, json) ? 1 : 0;
    free(json);
    aurora_json_free(root);
    return ok;
}

int aurora_pkg_lock_save(void) {
    ensure_paths();
    if (g_lock_path.empty()) {
        char buf[4096];
#ifdef _WIN32
        if (!_getcwd(buf, sizeof(buf))) return 0;
#else
        if (!getcwd(buf, sizeof(buf))) return 0;
#endif
        g_lock_path = std::string(buf) + PATH_SEP + "package-lock.json";
    }

    JsonValue* root = aurora_json_new_object();
    aurora_json_set(root, "version", 1);
    aurora_json_set_str(root, "registry", aurora_pkg_get_registry());
    JsonValue* pkgs = aurora_json_new_object();

    char** names; int count;
    if (aurora_pkg_list_installed(&names, &count)) {
        for (int i = 0; i < count; i++) {
            if (names[i]) {
                JsonValue* info = aurora_json_new_object();
                std::string pkg_path = g_packages_dir + PATH_SEP + names[i];

                std::string meta = read_file_str(pkg_path + PATH_SEP + "metadata.json");
                if (!meta.empty()) {
                    JsonValue* mv = aurora_json_parse(meta.c_str());
                    if (mv) {
                        JsonValue* v = aurora_json_get_obj(mv, "version");
                        if (v && aurora_json_type(v) == JSON_STR)
                            aurora_json_set_str(info, "version", v->str_val);
                        v = aurora_json_get_obj(mv, "dependencies");
                        if (v && aurora_json_type(v) == JSON_ARRAY) {
                            JsonValue* deps_arr = aurora_json_new_array();
                            for (int j = 0; j < v->count; j++) {
                                JsonValue* item = aurora_json_array_get(v, j);
                                if (item && aurora_json_type(item) == JSON_STR)
                                    aurora_json_array_push(deps_arr, aurora_json_new_str(item->str_val));
                            }
                            aurora_json_set_obj(info, "dependencies", deps_arr);
                        }
                        aurora_json_free(mv);
                    }
                }
                aurora_json_set_obj(pkgs, names[i], info);
                free(names[i]);
            }
        }
        free(names);
    }

    aurora_json_set_obj(root, "packages", pkgs);
    char* json = aurora_json_serialize(root);
    int ok = write_file_str(g_lock_path, json) ? 1 : 0;
    free(json);
    aurora_json_free(root);
    return ok;
}

int aurora_pkg_lock_load(void) {
    ensure_paths();
    if (g_lock_path.empty()) {
        char buf[4096];
#ifdef _WIN32
        if (!_getcwd(buf, sizeof(buf))) return 0;
#else
        if (!getcwd(buf, sizeof(buf))) return 0;
#endif
        g_lock_path = std::string(buf) + PATH_SEP + "package-lock.json";
    }

    std::string data = read_file_str(g_lock_path);
    if (data.empty()) return 0;

    JsonValue* root = aurora_json_parse(data.c_str());
    if (!root) return 0;

    JsonValue* ver = aurora_json_get_obj(root, "version");
    if (!ver || aurora_json_type(ver) != JSON_NUM) {
        aurora_json_free(root);
        return 0;
    }

    int version_locked = (int)ver->num_val;
    if (version_locked < 1) { aurora_json_free(root); return 0; }
    (void)version_locked;

    JsonValue* reg = aurora_json_get_obj(root, "registry");
    if (reg && aurora_json_type(reg) == JSON_STR && reg->str_val)
        aurora_pkg_set_registry(reg->str_val);

    aurora_json_free(root);
    return 1;
}

/* ── Dependency Resolution (3) ── */

int aurora_pkg_dep_resolve(const char* name) {
    if (!name || !*name) return 0;
    ensure_paths();
    g_dep_names.clear();

    std::string meta_path = g_packages_dir + PATH_SEP + name + PATH_SEP + "metadata.json";
    std::string meta = read_file_str(meta_path);
    if (meta.empty()) return 0;

    JsonValue* root = aurora_json_parse(meta.c_str());
    if (!root) return 0;

    JsonValue* deps = aurora_json_get_obj(root, "dependencies");
    if (deps && aurora_json_type(deps) == JSON_ARRAY) {
        for (int i = 0; i < deps->count; i++) {
            JsonValue* item = aurora_json_array_get(deps, i);
            if (item && aurora_json_type(item) == JSON_STR && item->str_val)
                g_dep_names.push_back(item->str_val);
        }
    }

    aurora_json_free(root);
    return (int)g_dep_names.size();
}

int aurora_pkg_dep_get_count(void) {
    return (int)g_dep_names.size();
}

const char* aurora_pkg_dep_get_name(int index) {
    if (index < 0 || index >= (int)g_dep_names.size()) return nullptr;
    return g_dep_names[index].c_str();
}

/* ── Offline Cache (3) ── */

int aurora_pkg_cache_list(char*** out_names, int* out_count) {
    if (!out_names || !out_count) return 0;
    ensure_paths();
    ensure_dir("");

    std::vector<std::string> items;
#ifdef _WIN32
    std::string pattern = g_cache_dir + "\\*";
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &ffd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(ffd.cFileName, ".") != 0 && strcmp(ffd.cFileName, "..") != 0)
                items.push_back(ffd.cFileName);
        } while (FindNextFileA(hFind, &ffd) != 0);
        FindClose(hFind);
    }
#else
    DIR* dir = opendir(g_cache_dir.c_str());
    if (dir) {
        struct dirent* ent;
        while ((ent = readdir(dir)) != nullptr) {
            if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0)
                items.push_back(ent->d_name);
        }
        closedir(dir);
    }
#endif

    int n = (int)items.size();
    char** arr = (char**)malloc(sizeof(char*) * (n + 1));
    if (!arr) return 0;
    for (int i = 0; i < n; i++) {
        arr[i] = (char*)malloc(items[i].size() + 1);
        if (arr[i]) {
            memcpy(arr[i], items[i].data(), items[i].size());
            arr[i][items[i].size()] = '\0';
        }
    }
    arr[n] = nullptr;
    *out_names = arr;
    *out_count = n;
    return 1;
}

void aurora_pkg_cache_clear(void) {
    ensure_paths();
#ifdef _WIN32
    std::string cmd = "rmdir /s /q \"" + g_cache_dir + "\" 2>nul";
    (void)system(cmd.c_str());
    mkdir_p(g_cache_dir);
#else
    std::string cmd = "rm -rf \"" + g_cache_dir + "\" 2>/dev/null";
    (void)system(cmd.c_str());
    mkdir_p(g_cache_dir);
#endif
}

const char* aurora_pkg_cache_path(void) {
    ensure_paths();
    ensure_dir("");
    return g_cache_dir.c_str();
}
