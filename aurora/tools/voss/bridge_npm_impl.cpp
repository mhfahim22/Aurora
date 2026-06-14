#include "bridge_npm_impl.hpp"
#include "bridge_shared.h"
#include <string>
#include <cctype>
#include <cstdlib>
#include "bridge_shared.h"

/* ── Detect if an npm package has native addon (N-API / node-gyp) ── */
bool npm_has_native_addon(const JsonValue& json) {
    /* Check top-level native addon indicators */
    if (json.get("gypfile")) {
        auto* gyp = json.get("gypfile");
        if (gyp && gyp->type == JsonValue::Bool && gyp->bool_val) return true;
    }
    if (json.get("binary")) {
        /* Has prebuild binary section = native addon */
        return true;
    }
    if (json.get("napi")) {
        /* Explicitly declares N-API support */
        return true;
    }
    /* Check dependencies for native addon tooling */
    auto* deps = json.get("dependencies");
    if (deps && deps->type == JsonValue::Object) {
        static const char* native_dep_markers[] = {
            "node-addon-api", "node-gyp", "node-gyp-build",
            "node-pre-gyp", "@mapbox/node-pre-gyp",
            "prebuild", "prebuildify", "prebuild-install",
            "nan", "bindings", "node-gyp", "napi-build-utils",
            nullptr
        };
        for (auto& [dname, dver] : deps->obj) {
            for (int i = 0; native_dep_markers[i]; i++) {
                if (dname.find(native_dep_markers[i]) != std::string::npos)
                    return true;
            }
        }
    }
    return false;
}

const char* npm_cjs_fallback(const std::string& pkg) {
    static const struct { const char* name; const char* cjs_ver; } esm_only[] = {
        {"chalk", "4"},
        {"nanocolors", "0"},
        {"got", "11"},
        {"ky", "0"},
        {"execa", "5"},
        {"globby", "11"},
        {"p-map", "4"},
        {"p-limit", "3"},
        {NULL, NULL}
    };
    for (int i = 0; esm_only[i].name; i++) {
        if (pkg == esm_only[i].name) return esm_only[i].cjs_ver;
    }
    return NULL;
}

std::string sanitize_ident(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    for (char c : name) {
        if (isalnum((unsigned char)c) || c == '_') out += c;
        else if (c == '-' || c == '.' || c == '@' || c == '/') out += '_';
    }
    if (out.empty() || !isalpha((unsigned char)out[0])) out = "pkg_" + out;
    return out;
}

/* ── Optional esbuild ESM→CJS transpilation at bridge generation time ── */
/* Runs npx esbuild to convert .mjs to .cjs for better CJS output.
   The generated load_module() will prefer the .cjs at runtime.
   Silently skips if node/esbuild is not available. */
void run_esbuild_transpile(const std::string& entry_path) {
    if (entry_path.size() < 4 ||
        entry_path.compare(entry_path.size() - 4, 4, ".mjs") != 0)
        return;
    std::string outfile = entry_path.substr(0, entry_path.size() - 4) + ".cjs";
#ifdef _WIN32
    std::string cmd = "npx --yes esbuild \"" + entry_path
        + "\" --platform=node --format=cjs --target=es2020 --outfile=\""
        + outfile + "\" 2>nul";
#else
    std::string cmd = "npx --yes esbuild \"" + entry_path
        + "\" --platform=node --format=cjs --target=es2020 --outfile=\""
        + outfile + "\" 2>/dev/null";
#endif
    int ret = std::system(cmd.c_str());
    if (ret == 0)
        std::cout << "[bridge]   esbuild: " << entry_path << " -> " << outfile << "\n";
    /* If esbuild fails (node not found, esbuild not cached, etc.),
       silently fall back to C transpiler at runtime */
}


