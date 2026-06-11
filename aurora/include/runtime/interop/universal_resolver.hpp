#pragma once
#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <cstdint>
#include <sstream>
#include <iostream>

/* ════════════════════════════════════════════════════════════
   Universal Package Resolver — Phase 5
   ════════════════════════════════════════════════════════════
   Meta-registry that searches across PyPI / npm / Cargo to
   resolve any package to a unified Aurora package descriptor.
   ════════════════════════════════════════════════════════════ */

/* ── Supported ecosystems ── */
enum class Ecosystem : uint8_t {
    Unknown,
    PyPI,
    Npm,
    Cargo,
    Native,
};

inline const char* ecosystem_name(Ecosystem e) {
    switch (e) {
        case Ecosystem::PyPI:  return "pypi";
        case Ecosystem::Npm:   return "npm";
        case Ecosystem::Cargo: return "cargo";
        case Ecosystem::Native: return "native";
        default:               return "unknown";
    }
}

inline Ecosystem ecosystem_from_name(const std::string& name) {
    if (name == "pypi")  return Ecosystem::PyPI;
    if (name == "npm")   return Ecosystem::Npm;
    if (name == "cargo") return Ecosystem::Cargo;
    if (name == "native") return Ecosystem::Native;
    return Ecosystem::Unknown;
}

/* ── Unified package descriptor ── */
struct UnifiedPackageInfo {
    std::string  name;
    std::string  version{"latest"};
    std::string  description;
    std::string  homepage;
    std::string  repository;
    std::string  license;
    Ecosystem    ecosystem{Ecosystem::Unknown};
    std::string  registry_url;              /* API URL used to fetch */
    std::string  raw_json;                  /* Raw JSON response for further parsing */
    bool         found{false};

    /* Cross-ecosystem dependencies */
    std::vector<UnifiedPackageInfo> dependencies;
};

/* ── Ecosystem API endpoints ── */
struct EcosystemAPI {
    static std::string pypi_url(const std::string& pkg) {
        return "https://pypi.org/pypi/" + pkg + "/json";
    }
    static std::string npm_url(const std::string& pkg) {
        return "https://registry.npmjs.org/" + pkg;
    }
    static std::string cargo_url(const std::string& pkg) {
        return "https://crates.io/api/v1/crates/" + pkg;
    }
};

/* ── HTTP fetch callback (injected so same code works in voss & tests) ── */
using HttpGetFn = std::function<std::string(const std::string& url)>;

/* ── The universal resolver ── */
class UniversalResolver {
public:
    explicit UniversalResolver(HttpGetFn http_get)
        : http_get_(std::move(http_get)) {}

    /* ── Resolve a package from a specific ecosystem ── */
    UnifiedPackageInfo resolve(const std::string& pkg_name, Ecosystem eco,
                               const std::string& version = "latest") const
    {
        UnifiedPackageInfo info;
        info.name = pkg_name;
        info.ecosystem = eco;
        info.version = version;

        std::string url;
        switch (eco) {
            case Ecosystem::PyPI:  url = EcosystemAPI::pypi_url(pkg_name); break;
            case Ecosystem::Npm:   url = EcosystemAPI::npm_url(pkg_name);  break;
            case Ecosystem::Cargo: url = EcosystemAPI::cargo_url(pkg_name);break;
            default: return info;
        }
        info.registry_url = url;

        std::string json = http_get_(url);
        if (json.empty()) return info;
        info.raw_json = json;
        info.found = true;

        /* Extract version & description */
        switch (eco) {
            case Ecosystem::PyPI:
                info.version = extract_nested_json_str(json, "info", "version");
                info.description = extract_nested_json_str(json, "info", "summary");
                break;
            case Ecosystem::Npm:
                info.version = extract_nested_json_str(json, "dist-tags", "latest");
                if (info.version.empty())
                    info.version = extract_json_str(json, "version");
                info.description = extract_json_str(json, "description");
                break;
            case Ecosystem::Cargo:
                info.version = extract_nested_json_str(json, "crate", "max_stable_version");
                if (info.version.empty())
                    info.version = extract_nested_json_str(json, "crate", "max_version");
                info.description = extract_nested_json_str(json, "crate", "description");
                break;
            default: break;
        }

        return info;
    }

    /* ── Auto-detect: try each ecosystem in order, return first hit ── */
    UnifiedPackageInfo auto_detect(const std::string& pkg_name,
                                   const std::string& version = "latest") const
    {
        static const Ecosystem order[] = {Ecosystem::PyPI, Ecosystem::Npm, Ecosystem::Cargo};

        for (auto eco : order) {
            UnifiedPackageInfo info = resolve(pkg_name, eco, version);
            if (info.found) {
                std::cerr << "[resolve] auto-detected " << pkg_name
                          << " → " << ecosystem_name(eco) << "@" << info.version << "\n";
                return info;
            }
        }

        UnifiedPackageInfo not_found;
        not_found.name = pkg_name;
        return not_found;
    }

    /* ── Search all ecosystems simultaneously for a package ── */
    std::vector<UnifiedPackageInfo> search_all(const std::string& pkg_name,
                                                const std::string& version = "latest") const
    {
        std::vector<UnifiedPackageInfo> results;
        static const Ecosystem all[] = {Ecosystem::PyPI, Ecosystem::Npm, Ecosystem::Cargo};

        for (auto eco : all) {
            UnifiedPackageInfo info = resolve(pkg_name, eco, version);
            if (info.found)
                results.push_back(std::move(info));
        }

        return results;
    }

private:
    HttpGetFn http_get_;

    /* ── Simple JSON field extraction ── */
    static std::string extract_json_str(const std::string& json, const std::string& key) {
        size_t kp = json.find("\"" + key + "\"");
        if (kp == std::string::npos) return {};
        size_t vp = json.find(':', kp + key.size() + 2);
        if (vp == std::string::npos) return {};
        vp = json.find_first_not_of(" \t\r\n", vp + 1);
        if (vp == std::string::npos) return {};
        /* Handle string value */
        if (json[vp] == '"') {
            size_t ve = vp + 1;
            while (ve < json.size()) {
                if (json[ve] == '\\') { ve += 2; continue; }
                if (json[ve] == '"') break;
                ve++;
            }
            if (ve >= json.size()) return {};
            return json.substr(vp + 1, ve - vp - 1);
        }
        /* Handle number/boolean/null — return as string */
        size_t ve = json.find_first_of(",\n\r}]", vp + 1);
        if (ve == std::string::npos) return {};
        std::string val = json.substr(vp, ve - vp);
        /* Trim whitespace */
        size_t s = val.find_first_not_of(" \t\r\n");
        size_t e = val.find_last_not_of(" \t\r\n");
        if (s == std::string::npos) return {};
        return val.substr(s, e - s + 1);
    }

    static std::string extract_nested_json_str(const std::string& json,
                                                const std::string& outer_key,
                                                const std::string& inner_key)
    {
        size_t ok = json.find("\"" + outer_key + "\"");
        if (ok == std::string::npos) return {};
        size_t val_start = json.find(':', ok + outer_key.size() + 2);
        if (val_start == std::string::npos) return {};
        /* Skip whitespace and find the opening { or value */
        val_start = json.find_first_not_of(" \t\r\n", val_start + 1);
        if (val_start == std::string::npos || json[val_start] != '{') return {};

        int depth = 1;
        size_t pos = val_start + 1;
        bool in_str = false;
        while (depth > 0 && pos < json.size()) {
            char c = json[pos];
            if (in_str) {
                if (c == '\\') { pos += 2; continue; }
                if (c == '"') in_str = false;
            } else {
                if (c == '"') in_str = true;
                else if (c == '{') depth++;
                else if (c == '}') depth--;
            }
            pos++;
        }
        std::string section = json.substr(val_start, pos - val_start);
        return extract_json_str(section, inner_key);
    }
};
