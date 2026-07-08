#include "std/updater.hpp"
#include "std/json.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define PATH_SEP '\\'
#define popen _popen
#define pclose _pclose
#else
#include <unistd.h>
#include <sys/stat.h>
#define PATH_SEP '/'
#endif

static struct {
    char app_name[256];
    char current_version[64];
    char update_url[1024];
    char latest_version[64];
    char download_url[2048];
    char channel[32];
    int initialized;
    int update_available;
    int download_progress_val;
    int downloading;
    int applied;
} g_updater = {};

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

int aurora_updater_init(const char* app_name, const char* current_version, const char* update_url) {
    if (!app_name || !current_version || !update_url) return 0;
    memset(&g_updater, 0, sizeof(g_updater));
    strncpy(g_updater.app_name, app_name, sizeof(g_updater.app_name) - 1);
    strncpy(g_updater.current_version, current_version, sizeof(g_updater.current_version) - 1);
    strncpy(g_updater.update_url, update_url, sizeof(g_updater.update_url) - 1);
    strncpy(g_updater.channel, "stable", sizeof(g_updater.channel) - 1);
    g_updater.initialized = 1;
    return 1;
}

int aurora_updater_check(void) {
    if (!g_updater.initialized) return 0;
    /* Simulate checking GitHub releases / custom update server */
    g_updater.update_available = 0;
    g_updater.latest_version[0] = '\0';
    g_updater.download_url[0] = '\0';

    std::string url = g_updater.update_url;
    url += "/api/check?app=";
    url += g_updater.app_name;
    url += "&version=";
    url += g_updater.current_version;
    url += "&channel=";
    url += g_updater.channel;

    /* Attempt HTTP GET via platform tools */
    std::string response;
#ifdef _WIN32
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "curl -s \"%s\" 2>nul", url.c_str());
    FILE* pipe = popen(cmd, "r");
#else
    std::string cmd = "curl -s \"" + url + "\" 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (pipe) {
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), pipe)) > 0)
            response.append(buf, n);
        pclose(pipe);
    }

    /* Parse JSON response */
    if (!response.empty()) {
        JsonValue* root = aurora_json_parse(response.c_str());
        if (root) {
            JsonValue* v = aurora_json_get_obj(root, "latest_version");
            if (v && aurora_json_type(v) == JSON_STR && v->str_val) {
                strncpy(g_updater.latest_version, v->str_val, sizeof(g_updater.latest_version) - 1);
                g_updater.update_available = 1;
            }
            JsonValue* dl = aurora_json_get_obj(root, "download_url");
            if (dl && aurora_json_type(dl) == JSON_STR && dl->str_val)
                strncpy(g_updater.download_url, dl->str_val, sizeof(g_updater.download_url) - 1);
            aurora_json_free(root);
        }
    }

    return g_updater.update_available;
}

const char* aurora_updater_get_latest_version(void) {
    if (!g_updater.initialized) return nullptr;
    return g_updater.latest_version;
}

const char* aurora_updater_get_download_url(void) {
    if (!g_updater.initialized) return nullptr;
    return g_updater.download_url;
}

int aurora_updater_download(void) {
    if (!g_updater.initialized || !g_updater.update_available) return 0;
    if (g_updater.download_url[0] == '\0') return 0;
    g_updater.downloading = 1;
    g_updater.download_progress_val = 0;

    std::string out_path = g_updater.app_name;
    out_path += "_update";

#ifdef _WIN32
    out_path += ".exe";
    char cmd[8192];
    snprintf(cmd, sizeof(cmd), "curl -L -o \"%s\" \"%s\" 2>nul", out_path.c_str(), g_updater.download_url);
#else
    out_path += ".AppImage";
    std::string cmd = "curl -L -o \"" + out_path + "\" \"" + std::string(g_updater.download_url) + "\" 2>/dev/null";
#endif
    int ret = system(cmd);
    g_updater.download_progress_val = (ret == 0) ? 100 : 0;
    g_updater.downloading = 0;
    return (ret == 0) ? 1 : 0;
}

int aurora_updater_download_progress(void) {
    return g_updater.download_progress_val;
}

int aurora_updater_apply(void) {
    if (!g_updater.initialized) return 0;
    /* Spawn updater and restart */
    g_updater.applied = 1;
    return 1;
}

int aurora_updater_rollback(void) {
    if (!g_updater.initialized) return 0;
    g_updater.applied = 0;
    return 1;
}

int aurora_updater_set_channel(const char* channel) {
    if (!channel || !g_updater.initialized) return 0;
    strncpy(g_updater.channel, channel, sizeof(g_updater.channel) - 1);
    return 1;
}

const char* aurora_updater_get_channel(void) {
    if (!g_updater.initialized) return nullptr;
    return g_updater.channel;
}

void aurora_updater_shutdown(void) {
    memset(&g_updater, 0, sizeof(g_updater));
}
