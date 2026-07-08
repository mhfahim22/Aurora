#include "std/theme_store.hpp"
#include "std/app.hpp"
#include "std/json.hpp"
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>

#ifdef _WIN32
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

/* ── Global state ── */

static std::string g_themes_dir;
static std::string g_current_theme;
static bool g_paths_inited = false;

/* ── Path helpers ── */

static const char* get_home_dir() {
#ifdef _WIN32
    static char buf[MAX_PATH];
    DWORD ret = GetEnvironmentVariableA("USERPROFILE", buf, sizeof(buf));
    if (ret > 0 && ret < sizeof(buf)) return buf;
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
    g_themes_dir = home + PATH_SEP + ".aurora" + PATH_SEP + "themes";
#ifdef _WIN32
    _mkdir(g_themes_dir.c_str());
#else
    mkdir(g_themes_dir.c_str(), 0755);
#endif
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

/* ── Theme Store Functions ── */

int aurora_theme_store_search(const char* query, char*** out_results, int* out_count) {
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

int aurora_theme_store_install(const char* name) {
    if (!name || !*name) return 0;
    ensure_paths();

    /* Download theme via voss */
    std::string spec = std::string("theme@") + name;
    std::string raw = voss_capture("voss install \"%s\" 2>nul", spec.c_str());
    if (raw.empty()) return 0;

    /* Check if theme was installed into packages dir */
    std::string home = get_home_dir();
    std::string pkg_dir = home + PATH_SEP + ".aurora" + PATH_SEP + "packages" + PATH_SEP + "theme-" + name;
    std::string theme_json = pkg_dir + PATH_SEP + "theme.json";

    std::string existing = read_file_str(theme_json);
    if (existing.empty()) {
        /* Try direct path */
        theme_json = pkg_dir + PATH_SEP + name + ".json";
        existing = read_file_str(theme_json);
    }
    if (existing.empty()) return 0;

    /* Copy to themes directory */
    std::string dest = g_themes_dir + PATH_SEP + name + ".json";
    if (!write_file_str(dest, existing)) return 0;

    return 1;
}

int aurora_theme_store_uninstall(const char* name) {
    if (!name || !*name) return 0;
    ensure_paths();
    std::string path = g_themes_dir + PATH_SEP + name + ".json";
#ifdef _WIN32
    return DeleteFileA(path.c_str()) ? 1 : 0;
#else
    return (unlink(path.c_str()) == 0) ? 1 : 0;
#endif
}

int aurora_theme_store_list_installed(char*** out_names, int* out_count) {
    if (!out_names || !out_count) return 0;
    ensure_paths();

    std::vector<std::string> items;
#ifdef _WIN32
    std::string pattern = g_themes_dir + "\\*.json";
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &ffd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            const char* name = ffd.cFileName;
            size_t len = strlen(name);
            if (len > 5 && strcmp(name + len - 5, ".json") == 0) {
                std::string s(name, len - 5);
                if (!s.empty()) items.push_back(s);
            }
        } while (FindNextFileA(hFind, &ffd) != 0);
        FindClose(hFind);
    }
#else
    DIR* dir = opendir(g_themes_dir.c_str());
    if (dir) {
        struct dirent* ent;
        while ((ent = readdir(dir)) != nullptr) {
            const char* name = ent->d_name;
            size_t len = strlen(name);
            if (len > 5 && strcmp(name + len - 5, ".json") == 0) {
                std::string s(name, len - 5);
                if (!s.empty()) items.push_back(s);
            }
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

int aurora_theme_store_apply(const char* name) {
    if (!name || !*name) return 0;
    ensure_paths();
    std::string path = g_themes_dir + PATH_SEP + name + ".json";
    std::string json = read_file_str(path);
    if (json.empty()) return 0;

    JsonValue* root = aurora_json_parse(json.c_str());
    if (!root) return 0;

    void* theme = aurora_app_theme_init();
    if (!theme) { aurora_json_free(root); return 0; }

    /* Apply mode */
    JsonValue* mode = aurora_json_get_obj(root, "mode");
    if (mode && aurora_json_type(mode) == JSON_NUM)
        aurora_app_theme_set_mode(theme, (int)mode->num_val);

    /* Apply colors */
    JsonValue* colors = aurora_json_get_obj(root, "colors");
    if (colors && aurora_json_type(colors) == JSON_ARRAY) {
        for (int i = 0; i < colors->count; i++) {
            JsonValue* entry = aurora_json_array_get(colors, i);
            if (entry && aurora_json_type(entry) == JSON_OBJECT) {
                JsonValue* key = aurora_json_get_obj(entry, "key");
                JsonValue* val = aurora_json_get_obj(entry, "value");
                if (key && aurora_json_type(key) == JSON_NUM &&
                    val && aurora_json_type(val) == JSON_NUM) {
                    aurora_app_theme_set_color(theme, (int)key->num_val,
                                               (unsigned int)(int)val->num_val);
                }
            }
        }
    }

    /* Apply fonts */
    JsonValue* fonts = aurora_json_get_obj(root, "fonts");
    if (fonts && aurora_json_type(fonts) == JSON_ARRAY) {
        for (int i = 0; i < fonts->count; i++) {
            JsonValue* entry = aurora_json_array_get(fonts, i);
            if (entry && aurora_json_type(entry) == JSON_OBJECT) {
                JsonValue* key = aurora_json_get_obj(entry, "key");
                JsonValue* val = aurora_json_get_obj(entry, "size");
                if (key && aurora_json_type(key) == JSON_NUM &&
                    val && aurora_json_type(val) == JSON_NUM) {
                    aurora_app_theme_set_font(theme, (int)key->num_val, (int)val->num_val);
                }
            }
        }
    }

    aurora_json_free(root);
    g_current_theme = name;
    return 1;
}

int aurora_theme_store_publish(const char* path) {
    if (!path || !*path) return 0;
    ensure_paths();
    std::string raw = voss_capture("voss publish \"%s\" 2>nul", path);
    return raw.empty() ? 0 : 1;
}

const char* aurora_theme_store_get_current() {
    if (g_current_theme.empty()) return nullptr;
    return g_current_theme.c_str();
}

const char* aurora_theme_store_export_json(const char* name) {
    if (!name || !*name) return nullptr;
    ensure_paths();
    std::string path = g_themes_dir + PATH_SEP + name + ".json";
    std::string json = read_file_str(path);
    if (json.empty()) return nullptr;
    static std::string cached;
    cached = json;
    return cached.c_str();
}

int aurora_theme_store_import_json(const char* json) {
    if (!json || !*json) return 0;
    ensure_paths();
    JsonValue* root = aurora_json_parse(json);
    if (!root) return 0;
    JsonValue* name_val = aurora_json_get_obj(root, "name");
    if (!name_val || aurora_json_type(name_val) != JSON_STR || !name_val->str_val) {
        aurora_json_free(root);
        return 0;
    }
    std::string theme_name = name_val->str_val;
    std::string path = g_themes_dir + PATH_SEP + theme_name + ".json";
    int ok = write_file_str(path, json) ? 1 : 0;
    aurora_json_free(root);
    if (ok) g_current_theme = theme_name;
    return ok;
}

int aurora_theme_store_validate(const char* path) {
    if (!path || !*path) return 0;
    std::string json = read_file_str(path);
    if (json.empty()) return 0;
    JsonValue* root = aurora_json_parse(json.c_str());
    if (!root) return 0;

    int valid = 0;
    JsonValue* name = aurora_json_get_obj(root, "name");
    JsonValue* colors = aurora_json_get_obj(root, "colors");
    if (name && aurora_json_type(name) == JSON_STR && name->str_val &&
        colors && aurora_json_type(colors) == JSON_ARRAY) {
        valid = 1;
    }
    aurora_json_free(root);
    return valid;
}
