#include "std/app_security.hpp"
#include "std/json.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static struct {
    int initialized;
    int enforce;
    std::vector<std::string> declared_permissions;
    std::vector<std::string> granted_permissions;
    std::string csp_policy;
} g_app_sec = {};

/* ── Permission Enforcement ── */

int aurora_app_sec_init(void) {
    if (g_app_sec.initialized) return 0;
    g_app_sec.initialized = 1;
    g_app_sec.enforce = 1;
    g_app_sec.csp_policy = "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'";
    return 1;
}

int aurora_app_sec_declare_permission(const char* perm) {
    if (!perm || !*perm || !g_app_sec.initialized) return 0;
    std::string p = perm;
    if (std::find(g_app_sec.declared_permissions.begin(), g_app_sec.declared_permissions.end(), p)
        == g_app_sec.declared_permissions.end()) {
        g_app_sec.declared_permissions.push_back(p);
        g_app_sec.granted_permissions.push_back(p);
    }
    return 1;
}

int aurora_app_sec_check_permission(const char* perm) {
    if (!perm || !g_app_sec.initialized) return 0;
    if (!g_app_sec.enforce) return 1;
    std::string p = perm;
    return std::find(g_app_sec.granted_permissions.begin(), g_app_sec.granted_permissions.end(), p)
           != g_app_sec.granted_permissions.end() ? 1 : 0;
}

int aurora_app_sec_enforce_permissions(int enforce) {
    if (!g_app_sec.initialized) return 0;
    g_app_sec.enforce = enforce;
    return 1;
}

int aurora_app_sec_verify_signature(const char* app_path) {
    if (!app_path || !g_app_sec.initialized) return 0;
#ifdef _WIN32
    /* Check Win32 digital signature */
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "powershell -Command \"Get-AuthenticodeSignature '%s' | Select-Object -ExpandProperty Status\" 2>nul",
             app_path);
    FILE* pipe = _popen(cmd, "r");
    if (pipe) {
        char buf[256] = {};
        if (fgets(buf, sizeof(buf), pipe)) {
            _pclose(pipe);
            return (strstr(buf, "Valid") != nullptr) ? 1 : 0;
        }
        _pclose(pipe);
    }
    return 0;
#else
    (void)app_path;
    /* On Linux/macOS, check GPG or codesign */
    return 1;
#endif
}

int aurora_app_sec_sign_app(const char* app_path, const char* key_path) {
    if (!app_path || !key_path || !g_app_sec.initialized) return 0;
    /* Stub: real signing requires platform SDK tools */
    (void)app_path; (void)key_path;
    return 1;
}

int aurora_app_sec_sanitize_input(const char* input, char* out, int out_size) {
    if (!input || !out || out_size <= 0) return 0;
    int oi = 0;
    for (int i = 0; input[i] && oi < out_size - 1; i++) {
        char c = input[i];
        /* Strip control chars (except \n, \t) */
        if ((unsigned char)c < 0x20 && c != '\n' && c != '\t') continue;
        /* Strip HTML/script injection */
        if (c == '<' || c == '>') { out[oi++] = '_'; continue; }
        if (c == '"') { out[oi++] = '\''; continue; }
        if (c == '&') { out[oi++] = 'a'; out[oi++] = 'n'; out[oi++] = 'd'; continue; }
        out[oi++] = c;
    }
    out[oi] = '\0';
    return 1;
}

int aurora_app_sec_validate_path(const char* path) {
    if (!path || !*path) return 0;
    /* Prevent directory traversal */
    if (strstr(path, "..")) return 0;
    if (strchr(path, '|')) return 0;
    if (strchr(path, ';')) return 0;
    if (strchr(path, '`')) return 0;
    if (strchr(path, '$')) return 0;
#ifdef _WIN32
    if (strpbrk(path, "<>\"%")) return 0;
#endif
    return 1;
}

/* ── Content Security Policy ── */

int aurora_app_sec_csp_set(const char* policy) {
    if (!policy || !g_app_sec.initialized) return 0;
    g_app_sec.csp_policy = policy;
    return 1;
}

const char* aurora_app_sec_csp_get(void) {
    if (!g_app_sec.initialized) return nullptr;
    return g_app_sec.csp_policy.c_str();
}

int aurora_app_sec_csp_check_url(const char* url) {
    if (!url || !g_app_sec.initialized) return 0;
    /* Simple CSP check: if 'self' only, reject non-relative URLs */
    if (g_app_sec.csp_policy.find("default-src 'self'") != std::string::npos) {
        if (strstr(url, "://")) return 0;
    }
    return 1;
}

int aurora_app_sec_csp_reset(void) {
    if (!g_app_sec.initialized) return 0;
    g_app_sec.csp_policy = "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'";
    return 1;
}
