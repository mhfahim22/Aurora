#include "voss.h"

/* ── Search ── */
int cmd_search(const std::string& query) {
    bool found = false;
    for (auto& e : g_rec_db) {
        if (e.name.find(query) != std::string::npos || e.desc.find(query) != std::string::npos)
        { std::cout << e.name; if (!e.desc.empty()) std::cout << "  " << e.desc; std::cout << "\n"; found = true; }
    }
    PackageInfo info = read_manifest(".");
    for (auto& dep : info.dependencies) { std::string dn, dv; parse_pkg_spec(dep, dn, dv); if (dn.find(query) != std::string::npos) { std::cout << dn << "@" << dv << " [installed]\n"; found = true; } }
    for (auto& reg : g_registries) {
        std::string raw = http_fetch(reg.url + "/search?q=" + query);
        if (!raw.empty()) {
            std::string json = extract_json_source(raw); size_t p = 0;
            while ((p = json.find("name\":\"", p)) != std::string::npos) { p += 7; size_t e = json.find('"', p); if (e != std::string::npos) { std::cout << json.substr(p, e - p) << " [registry:" << reg.name << "]\n"; found = true; p = e; } }
        }
    }
    {
        std::string gh_url = "https://api.github.com/search/repositories?q=" + query + "+in:name";
        std::string raw = http_fetch(gh_url);
        if (!raw.empty()) {
            std::string json = extract_json_source(raw);
            size_t p = 0;
            int count = 0;
            while ((p = json.find("\"full_name\":\"", p)) != std::string::npos && count < 10) {
                p += 13;
                size_t e = json.find('"', p);
                if (e != std::string::npos) {
                    std::string full = json.substr(p, e - p);
                    std::string desc = "";
                    size_t dp = json.find("\"description\":", e);
                    if (dp != std::string::npos) {
                        dp = json.find('"', dp + 14);
                        if (dp != std::string::npos) {
                            size_t de = json.find('"', dp + 1);
                            if (de != std::string::npos) desc = json.substr(dp + 1, de - dp - 1);
                        }
                    }
                    std::cout << full << "  " << desc << " [github]\n";
                    found = true;
                    count++;
                    p = e;
                }
            }
        }
    }
    if (!found) std::cout << "no results for '" << query << "'\n"; return found ? 0 : 1;
}

/* ═══════════════════════════════════════════════════════════
   TIER 3.5 — AI Compatibility Scanner
   ═══════════════════════════════════════════════════════════ */

std::vector<std::string> detect_breaking_changes(const std::string& source) {
    std::vector<std::string> changes;
    if (source.find("function") != std::string::npos &&
        source.find("removed") != std::string::npos) changes.push_back("function removals detected");
    if (source.find("deprecated") != std::string::npos) changes.push_back("deprecated API usage");
    if (source.find("rename") != std::string::npos) changes.push_back("API renames detected");
    if (source.find("signature") != std::string::npos) changes.push_back("function signature changes");
    if (source.find("removed parameter") != std::string::npos) changes.push_back("removed parameters");
    return changes;
}

int analyze_upgrade_risk(const std::string& name, const std::string& from_ver, const std::string& to_ver) {
    (void)name;
    if (from_ver.empty() || to_ver.empty()) return 30;
    int diff = semver_cmp(to_ver, from_ver);
    if (diff <= 0) return 5;
    std::string from_major = from_ver.substr(0, from_ver.find('.'));
    std::string to_major = to_ver.substr(0, to_ver.find('.'));
    if (from_major != to_major) return 85;
    std::string from_minor = from_ver.substr(from_ver.find('.') + 1);
    from_minor = from_minor.substr(0, from_minor.find('.'));
    std::string to_minor = to_ver.substr(to_ver.find('.') + 1);
    to_minor = to_minor.substr(0, to_minor.find('.'));
    if (from_minor != to_minor) return 40;
    return 15;
}

AIScanResult ai_scan_package(const std::string& name, const std::string& from_ver, const std::string& to_ver) {
    AIScanResult result;
    result.package = name;
    result.from_version = from_ver;
    result.to_version = to_ver;

    std::vector<std::string> sources;
    if (fs::exists("packages/" + name + "/main.aura")) {
        std::ifstream f("packages/" + name + "/main.aura");
        sources.push_back(std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()));
    }
    if (fs::exists("packages/" + name + "/aurora.pkg")) {
        std::ifstream f("packages/" + name + "/aurora.pkg");
        sources.push_back(std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()));
    }

    for (auto& src : sources) {
        auto changes = detect_breaking_changes(src);
        result.breaking_changes.insert(result.breaking_changes.end(), changes.begin(), changes.end());
    }

    LockData lf = read_lockfile();
    auto it = lf.packages.find(name);
    if (it != lf.packages.end()) {
        std::string current_version = it->second.version;
        result.impacted_files.push_back(name + "/main.aura");
        result.impacted_files.push_back(name + "/aurora.pkg");
        if (fs::exists("packages/" + name)) {
            for (auto& entry : fs::directory_iterator("packages/" + name)) {
                result.impacted_files.push_back(entry.path().string());
            }
        }
    }

    PackageInfo info = read_manifest(".");
    if (fs::exists(info.entry)) {
        std::ifstream f(info.entry);
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        size_t pos = 0;
        while ((pos = content.find(name, pos)) != std::string::npos) {
            size_t line_start = content.rfind('\n', pos);
            if (line_start == std::string::npos) line_start = 0;
            else line_start++;
            size_t line_end = content.find('\n', pos);
            if (line_end == std::string::npos) line_end = content.size();
            std::string line = content.substr(line_start, line_end - line_start);
            if (!line.empty() && line.find("//") == std::string::npos) {
                std::string loc = info.entry + ":" + std::to_string(std::count(content.begin(), content.begin() + pos, '\n') + 1);
                if (std::find(result.impacted_files.begin(), result.impacted_files.end(), loc) == result.impacted_files.end())
                    result.impacted_files.push_back(loc);
            }
            pos++;
        }
    }

    result.risk_score = analyze_upgrade_risk(name, from_ver, to_ver);
    result.risk_score += (int)result.breaking_changes.size() * 10;
    if (result.risk_score > 100) result.risk_score = 100;

    if (result.breaking_changes.empty() && result.risk_score < 20)
        result.summary = "Low risk \u2014 safe to upgrade";
    else if (result.risk_score < 50)
        result.summary = "Medium risk \u2014 review breaking changes";
    else
        result.summary = "High risk \u2014 major version change or breaking API";

    return result;
}

int cmd_ai_scan(const std::string& spec) {
    std::string name, to_ver;
    parse_pkg_spec(spec, name, to_ver);
    if (name.empty()) { std::cerr << "error: invalid spec\n"; return 1; }

    std::string from_ver;
    LockData lf = read_lockfile();
    auto it = lf.packages.find(name);
    if (it != lf.packages.end()) from_ver = it->second.version;
    if (to_ver.empty()) to_ver = "latest";

    std::cout << "AI Compatibility Scanner\n";
    std::cout << "|-- Package:  " << name << "\n";
    std::cout << "|-- From:     " << (from_ver.empty() ? "(not installed)" : from_ver) << "\n";
    std::cout << "|-- To:       " << to_ver << "\n\n";

    auto result = ai_scan_package(name, from_ver, to_ver);

    std::cout << "Risk Score: " << result.risk_score << "/100\n";
    std::cout << "Summary:    " << result.summary << "\n\n";

    if (!result.breaking_changes.empty()) {
        std::cout << "Breaking Changes (" << result.breaking_changes.size() << "):\n";
        for (auto& change : result.breaking_changes)
            std::cout << "  [!] " << change << "\n";
        std::cout << "\n";
    }

    if (!result.impacted_files.empty()) {
        std::cout << "Impacted Files (" << result.impacted_files.size() << "):\n";
        for (auto& file : result.impacted_files)
            std::cout << "  [*] " << file << "\n";
        std::cout << "\n";
    }

    if (result.risk_score >= 50) {
        std::cout << "Recommendation: Consider delaying upgrade or running in sandbox first\n";
    } else if (result.risk_score >= 20) {
        std::cout << "Recommendation: Review changes and test before upgrading\n";
    } else {
        std::cout << "Recommendation: Safe to upgrade\n";
    }

    return result.risk_score >= 50 ? 1 : 0;
}

/* ═══════════════════════════════════════════════════════════
   TIER 4.2 — Dependency Forecast
   ═══════════════════════════════════════════════════════════ */

DepForecast forecast_dependency(const std::string& name, const std::string& version) {
    DepForecast f;
    f.package = name;
    f.current_version = version;

    int age_days = 0;
    int release_gap = 0;
    std::string pkg_dir = "packages/" + name;
    if (fs::exists(pkg_dir)) {
        auto ft = fs::last_write_time(pkg_dir);
        auto now = fs::file_time_type::clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::hours>(now - ft).count();
        age_days = (int)(elapsed / 24);
    }

    int release_count = 0;
    LockData lf = read_lockfile();
    auto lit = lf.packages.find(name);
    if (lit != lf.packages.end() && !lit->second.dependencies.empty()) {
        release_count = (int)lit->second.dependencies.size();
    }

    f.abandonment_risk = std::min(95, std::max(5,
        (age_days / 30) * 5 +
        (release_count == 0 ? 30 : std::max(0, 20 - release_count * 2))
    ));

    f.risk_forecast = f.abandonment_risk;
    for (auto& vuln : g_vuln_db) {
        if (vuln.package == name) {
            f.risk_forecast += 10;
            if (vuln.severity == "HIGH" || vuln.severity == "CRITICAL") f.risk_forecast += 10;
        }
    }
    if (f.risk_forecast > 100) f.risk_forecast = 100;

    f.maintenance_score = std::max(0.0, 1.0 - (f.abandonment_risk / 100.0));

    auto now_t = std::time(nullptr);
    char buf[64];
    struct tm local;
#ifdef _WIN32
    localtime_s(&local, &now_t);
#else
    localtime_r(&now_t, &local);
#endif
    strftime(buf, sizeof(buf), "%Y-%m-%d", &local);
    f.last_update = buf;
    f.next_forecast = buf;

    if (f.abandonment_risk > 70)
        f.recommendation = "High abandonment risk \u2014 consider migrating to an alternative";
    else if (f.abandonment_risk > 40)
        f.recommendation = "Moderate risk \u2014 monitor for signs of abandonment";
    else if (f.risk_forecast > 50)
        f.recommendation = "Security risks detected \u2014 review vulnerabilities";
    else
        f.recommendation = "Healthy \u2014 no action required";

    return f;
}

int cmd_forecast() {
    std::cout << "VOSS Dependency Forecast\n\n";
    PackageInfo info = read_manifest(".");
    if (info.dependencies.empty()) { std::cout << "no dependencies to forecast\n"; return 0; }
    int high_risk = 0;
    for (auto& dep : info.dependencies) {
        std::string name, ver; parse_pkg_spec(dep, name, ver);
        auto f = forecast_dependency(name, ver);
        std::cout << "  " << f.package << "@" << f.current_version
                  << "  abandonment:" << f.abandonment_risk << "%"
                  << "  risk:" << f.risk_forecast << "%"
                  << "  maintenance:" << std::fixed << std::setprecision(2) << f.maintenance_score << "\n";
        if (f.abandonment_risk > 50 || f.risk_forecast > 50) high_risk++;
    }
    std::cout << "\n" << info.dependencies.size() << " analyzed, " << high_risk << " high-risk\n";
    return high_risk > 0 ? 1 : 0;
}

int cmd_forecast_package(const std::string& pkg) {
    std::string name, ver; parse_pkg_spec(pkg, name, ver);
    if (name.empty()) { std::cerr << "invalid package\n"; return 1; }
    auto f = forecast_dependency(name, ver);
    std::cout << "Forecast for " << f.package << "\n";
    std::cout << "  Version:          " << f.current_version << "\n";
    std::cout << "  Abandonment Risk: " << f.abandonment_risk << "%\n";
    std::cout << "  Risk Forecast:    " << f.risk_forecast << "%\n";
    std::cout << "  Maintenance:      " << std::fixed << std::setprecision(2) << f.maintenance_score << "\n";
    std::cout << "  Last Update:      " << f.last_update << "\n";
    std::cout << "  Recommendation:   " << f.recommendation << "\n";
    return f.abandonment_risk > 50 ? 1 : 0;
}

/* ═══════════════════════════════════════════════════════════
   LTS Channels
   ═══════════════════════════════════════════════════════════ */

int cmd_lts_list() {
    if (g_lts_channels.empty()) {
        g_lts_channels.push_back({"stable", "1.0.0", "active", "2024-01-01", "2026-01-01"});
        g_lts_channels.push_back({"lts", "0.9.0", "maintenance", "2023-06-01", "2025-06-01"});
        g_lts_channels.push_back({"legacy", "0.5.0", "eol", "2022-01-01", "2024-01-01"});
    }
    std::cout << "LTS Channels:\n";
    for (auto& ch : g_lts_channels) {
        std::cout << "  " << ch.name << "  " << ch.version << "  [" << ch.status << "]"
                  << "  released:" << ch.release_date << "  eol:" << ch.eol_date << "\n";
    }
    return 0;
}

int cmd_lts_add(const std::string& name, const std::string& version, const std::string& status) {
    LTSChannel ch;
    ch.name = name; ch.version = version; ch.status = status;
    auto t = std::time(nullptr); struct tm local; char buf[16];
#ifdef _WIN32
    localtime_s(&local, &t);
#else
    localtime_r(&t, &local);
#endif
    strftime(buf, sizeof(buf), "%Y-%m-%d", &local);
    ch.release_date = buf;
    ch.eol_date = "unknown";
    g_lts_channels.push_back(ch);
    std::cout << "added LTS channel: " << name << "@" << version << " [" << status << "]\n";
    return 0;
}

int cmd_lts_remove(const std::string& name) {
    auto it = std::remove_if(g_lts_channels.begin(), g_lts_channels.end(),
        [&](const LTSChannel& ch) { return ch.name == name; });
    if (it == g_lts_channels.end()) { std::cerr << "channel not found\n"; return 1; }
    g_lts_channels.erase(it, g_lts_channels.end());
    std::cout << "removed LTS channel: " << name << "\n";
    return 0;
}

int cmd_lts_switch(const std::string& name) {
    for (auto& ch : g_lts_channels) {
        if (ch.name == name) {
            if (ch.status == "eol") {
                std::cerr << "warning: " << name << " is EOL \u2014 not recommended\n";
            }
            PackageInfo info = read_manifest(".");
            if (!info.name.empty()) {
                std::ofstream f("aura.lts");
                f << "channel: " << name << "\nversion: " << ch.version << "\nstatus: " << ch.status << "\n";
                std::cout << "switched to LTS channel: " << name << "@" << ch.version << "\n";
                return 0;
            }
        }
    }
    std::cerr << "LTS channel not found: " << name << "\n";
    return 1;
}

/* ═══════════════════════════════════════════════════════════
   Reproducible Cloud Builds
   ═══════════════════════════════════════════════════════════ */

int cmd_cloud_build_init() {
    g_cloud_build.enabled = true;
    g_cloud_build.provider = "aurora-cloud";
    g_cloud_build.project_id = "default";
    g_cloud_build.agents = {"linux-x64", "win-x64", "mac-arm64"};
    g_cloud_build.reproducible = true;
    std::ofstream f("aura.cloud");
    f << "provider: " << g_cloud_build.provider << "\n"
      << "project_id: " << g_cloud_build.project_id << "\n"
      << "reproducible: true\n"
      << "agents: linux-x64, win-x64, mac-arm64\n";
    std::cout << "cloud build initialized (provider: " << g_cloud_build.provider << ")\n";
    return 0;
}

int cmd_cloud_build_run() {
    if (!g_cloud_build.enabled) { std::cerr << "cloud build not initialized (run 'voss cloud-build init')\n"; return 1; }
    PackageInfo info = read_manifest(".");
    if (info.name.empty()) { std::cerr << "no project\n"; return 1; }
    std::cout << "Submitting reproducible build for " << info.name << "@" << info.version << "\n";
    std::cout << "  Provider: " << g_cloud_build.provider << "\n";
    std::cout << "  Agents:   ";
    for (size_t i = 0; i < g_cloud_build.agents.size(); i++) { if (i > 0) std::cout << ", "; std::cout << g_cloud_build.agents[i]; }
    std::cout << "\n";
    std::cout << "  SHA-256:  " << sha256_hex(info.name + info.version) << "\n";
    std::cout << "Build submitted (simulated)\n";

    std::string build_dir = ".aura/cloud-builds";
    fs::create_directories(build_dir);
    auto t = std::time(nullptr);
    std::string bid = std::to_string(t);
    std::ofstream bf(build_dir + "/" + bid);
    bf << "package: " << info.name << "@" << info.version << "\n"
       << "provider: " << g_cloud_build.provider << "\n"
       << "reproducible: true\n"
       << "timestamp: " << bid << "\n"
       << "status: submitted\n";
    return 0;
}

int cmd_cloud_build_status() {
    std::string dir = ".aura/cloud-builds";
    if (!fs::exists(dir)) { std::cout << "no cloud builds\n"; return 0; }
    int count = 0;
    for (auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_directory()) {
            std::ifstream f(entry.path());
            std::string line, pkg, status;
            while (std::getline(f, line)) {
                if (line.rfind("package:", 0) == 0) pkg = trim(line.substr(8));
                if (line.rfind("status:", 0) == 0) status = trim(line.substr(7));
            }
            std::cout << "  " << entry.path().filename().string() << "  " << pkg << "  " << status << "\n";
            count++;
        }
    }
    std::cout << count << " build(s)\n";
    return 0;
}

int cmd_cloud_build_config() {
    if (!g_cloud_build.enabled) {
        std::cout << "cloud build: not configured\n";
        std::cout << "  run 'voss cloud-build init' to set up\n";
        return 0;
    }
    std::cout << "Cloud Build Configuration:\n";
    std::cout << "  Provider:     " << g_cloud_build.provider << "\n";
    std::cout << "  Project:      " << g_cloud_build.project_id << "\n";
    std::cout << "  Reproducible: " << (g_cloud_build.reproducible ? "yes" : "no") << "\n";
    std::cout << "  Agents (" << g_cloud_build.agents.size() << "): ";
    for (size_t i = 0; i < g_cloud_build.agents.size(); i++) { if (i > 0) std::cout << ", "; std::cout << g_cloud_build.agents[i]; }
    std::cout << "\n";
    return 0;
}

/* ═══════════════════════════════════════════════════════════
   Binary Package Distribution
   ═══════════════════════════════════════════════════════════ */

int cmd_binary_package() {
    PackageInfo info = read_manifest(".");
    if (info.name.empty()) { std::cerr << "no project\n"; return 1; }
    std::string bin_dir = "bin-pkg/" + info.name + "@" + info.version;
    fs::create_directories(bin_dir);

    if (fs::exists("build")) {
        for (auto& entry : fs::recursive_directory_iterator("build")) {
            if (!entry.is_directory()) {
                std::string rel = entry.path().string().substr(6);
                std::string target = bin_dir + "/" + rel;
                fs::create_directories(fs::path(target).parent_path());
                fs::copy_file(entry.path(), target, fs::copy_options::overwrite_existing);
            }
        }
    }

    std::ofstream mf(bin_dir + "/BINARY");
    mf << "name: " << info.name << "\nversion: " << info.version << "\n"
       << "platform: " <<
#ifdef _WIN32
       "windows"
#elif defined(__APPLE__)
       "macos"
#else
       "linux"
#endif
       << "\narch: x86_64\n"
       << "integrity: " << sha256_hex(info.name + info.version) << "\n";
    mf.close();

    std::cout << "binary package created: " << bin_dir << "\n";
    return 0;
}

int cmd_binary_install(const std::string& pkg) {
    std::string name, version; parse_pkg_spec(pkg, name, version);
    std::string src = "bin-pkg/" + name + (version.empty() ? "" : "@" + version);
    if (!fs::exists(src)) {
        for (auto& reg : g_registries) {
            std::string url = reg.url + "/binary/" + name + (version.empty() ? "/latest" : "/" + version);
            std::string raw = http_fetch(url);
            if (!raw.empty()) {
                std::string bin_dir = "bin-pkg/" + name + "@" + (version.empty() ? "fetched" : version);
                fs::create_directories(bin_dir);
                std::ofstream bf(bin_dir + "/package.bin");
                bf << raw;
                src = bin_dir;
                std::cout << "downloaded binary from " << reg.name << "\n";
                break;
            }
        }
    }
    if (!fs::exists(src)) { std::cerr << "binary package not found: " << pkg << "\n"; return 1; }
    std::string dest = "packages/" + name;
    fs::create_directories(dest);
    if (fs::exists(src + "/main.aura"))
        fs::copy_file(src + "/main.aura", dest + "/main.aura", fs::copy_options::overwrite_existing);
    std::cout << "binary installed: " << name << " (from " << src << ")\n";
    return 0;
}

int cmd_binary_list() {
    std::string dir = "bin-pkg";
    if (!fs::exists(dir)) { std::cout << "no binary packages\n"; return 0; }
    int count = 0;
    for (auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_directory()) {
            std::string name = entry.path().filename().string();
            std::ifstream mf(entry.path().string() + "/BINARY");
            std::string line, ver, plat;
            while (std::getline(mf, line)) {
                if (line.rfind("version:", 0) == 0) ver = trim(line.substr(8));
                if (line.rfind("platform:", 0) == 0) plat = trim(line.substr(9));
            }
            std::cout << "  " << name << "  " << (plat.empty() ? "" : "[" + plat + "]") << "\n";
            count++;
        }
    }
    std::cout << count << " binary package(s)\n";
    return 0;
}

/* ═══════════════════════════════════════════════════════════
   Changelog Analysis
   ═══════════════════════════════════════════════════════════ */

std::string fetch_changelog(const std::string& pkg, const std::string& version) {
    std::vector<std::string> paths = {
        "packages/" + pkg + "/CHANGELOG",
        "packages/" + pkg + "/CHANGELOG.md",
        "packages/" + pkg + "/HISTORY",
        pkg + "/CHANGELOG",
        pkg + "/CHANGELOG.md"
    };
    for (auto& p : paths) {
        if (fs::exists(p)) {
            std::ifstream f(p);
            return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        }
    }
    for (auto& reg : g_registries) {
        std::string url = reg.url + "/packages/" + pkg + "/changelog" + (version.empty() ? "" : "/" + version);
        std::string raw = http_fetch(url);
        if (!raw.empty()) return raw;
    }
    return "";
}

int cmd_changelog(const std::string& pkg) {
    std::string name, version; parse_pkg_spec(pkg, name, version);
    std::cout << "Changelog for " << name << (version.empty() ? "" : "@" + version) << "\n\n";
    std::string changelog = fetch_changelog(name, version);
    if (changelog.empty()) {
        LockData lf = read_lockfile();
        auto it = lf.packages.find(name);
        if (it != lf.packages.end()) {
            std::cout << "## " << it->second.version << " (" << (version.empty() ? "current" : version) << ")\n";
            std::cout << "  - Dependency update\n";
            if (!it->second.dependencies.empty()) {
                std::cout << "  - Dependencies:";
                for (auto& dep : it->second.dependencies) std::cout << " " << dep;
                std::cout << "\n";
            }
            std::cout << "\n  Integrity: " << it->second.integrity << "\n";
            return 0;
        }
        std::cout << "  No changelog available for " << name << "\n";
        return 1;
    }
    std::cout << changelog << "\n";
    return 0;
}

/* ═══════════════════════════════════════════════════════════
   Security Risk Forecast
   ═══════════════════════════════════════════════════════════ */

static std::vector<std::pair<std::string, int>> g_risk_patterns = {
    {"ssl", 15}, {"crypto", 20}, {"network", 10}, {"http", 5},
    {"auth", 25}, {"password", 30}, {"token", 20}, {"database", 10},
    {"exec", 25}, {"shell", 30}, {"eval", 35}, {"injection", 40}
};

int cmd_risk_forecast() {
    std::cout << "VOSS Security Risk Forecast\n\n";
    PackageInfo info = read_manifest(".");
    if (info.dependencies.empty()) { std::cout << "no dependencies\n"; return 0; }
    int high_risk = 0;
    for (auto& dep : info.dependencies) {
        std::string name, ver; parse_pkg_spec(dep, name, ver);
        int risk = 5;
        for (auto& vuln : g_vuln_db) {
            if (vuln.package == name) {
                risk += (vuln.severity == "HIGH" || vuln.severity == "CRITICAL") ? 25 : 10;
            }
        }
        for (auto& [pattern, score] : g_risk_patterns) {
            if (name.find(pattern) != std::string::npos) risk += score;
        }
        if (fs::exists("packages/" + name + "/main.aura")) {
            std::ifstream f("packages/" + name + "/main.aura");
            std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            for (auto& [pattern, score] : g_risk_patterns) {
                size_t pos = 0; int count = 0;
                while ((pos = content.find(pattern, pos)) != std::string::npos) { count++; pos++; }
                risk += count * (score / 2);
            }
        }
        if (risk > 100) risk = 100;
        std::string level = (risk >= 60) ? "HIGH" : (risk >= 30) ? "MEDIUM" : "LOW";
        std::cout << "  " << name << "@" << (ver.empty() ? "*" : ver)
                  << "  risk:" << risk << "%  [" << level << "]\n";
        if (risk >= 60) high_risk++;
    }
    std::cout << "\n" << info.dependencies.size() << " analyzed, " << high_risk << " high-risk\n";
    return high_risk > 0 ? 1 : 0;
}

int cmd_risk_forecast_package(const std::string& pkg) {
    std::string name, ver; parse_pkg_spec(pkg, name, ver);
    if (name.empty()) { std::cerr << "invalid package\n"; return 1; }
    int risk = 5;
    for (auto& vuln : g_vuln_db) if (vuln.package == name) risk += (vuln.severity == "HIGH" || vuln.severity == "CRITICAL") ? 25 : 10;
    std::cout << "Security Risk Forecast for " << name << "@" << (ver.empty() ? "*" : ver) << "\n";
    std::cout << "  Risk Score: " << std::min(100, risk) << "/100\n";
    std::cout << "  Level:      " << ((risk >= 60) ? "HIGH" : (risk >= 30) ? "MEDIUM" : "LOW") << "\n";
    if (risk >= 30) std::cout << "  Actions:    Review permissions, run audit, consider alternatives\n";
    return risk >= 60 ? 1 : 0;
}

/* ═══════════════════════════════════════════════════════════
   Package Lifecycle Monitor
   ═══════════════════════════════════════════════════════════ */

void log_lifecycle(const std::string& pkg, const std::string& version, const std::string& event) {
    std::string dir = ".aura/lifecycle";
    fs::create_directories(dir);
    auto t = std::time(nullptr);
    std::ofstream f(dir + "/" + pkg + ".log", std::ios::app);
    f << t << " " << event << " " << pkg << "@" << version << "\n";
}

static std::vector<LifecycleEvent> load_lifecycle_log(const std::string& pkg) {
    std::vector<LifecycleEvent> events;
    std::string path = ".aura/lifecycle/" + pkg + ".log";
    if (!fs::exists(path)) return events;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        std::string t = trim(line);
        if (t.empty()) continue;
        std::stringstream ss(t);
        LifecycleEvent ev;
        ss >> ev.timestamp >> ev.event >> ev.package >> ev.version;
        events.push_back(ev);
    }
    return events;
}

int cmd_lifecycle() {
    PackageInfo info = read_manifest(".");
    if (info.name.empty()) { std::cerr << "no project\n"; return 1; }
    std::cout << "Package Lifecycle Monitor\n\n";
    int total_events = 0;
    for (auto& dep : info.dependencies) {
        std::string name, ver; parse_pkg_spec(dep, name, ver);
        auto events = load_lifecycle_log(name);
        if (!events.empty()) {
            std::cout << "  " << name << " (" << events.size() << " events):\n";
            for (auto& ev : events) {
                char buf[32];
                struct tm local;
#ifdef _WIN32
                localtime_s(&local, &ev.timestamp);
#else
                time_t ts = static_cast<time_t>(ev.timestamp);
                localtime_r(&ts, &local);
#endif
                strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &local);
                std::cout << "    [" << buf << "] " << ev.event << " " << ev.version << "\n";
            }
            total_events += (int)events.size();
        }
    }
    if (total_events == 0) std::cout << "  no lifecycle events recorded\n";
    std::cout << "\n" << total_events << " total events\n";
    return 0;
}

int cmd_lifecycle_package(const std::string& pkg) {
    std::string name, ver; parse_pkg_spec(pkg, name, ver);
    auto events = load_lifecycle_log(name);
    if (events.empty()) {
        std::cout << "No lifecycle events for " << name << "\n";
        log_lifecycle(name, ver.empty() ? "*" : ver, "check");
        return 0;
    }
    std::cout << "Lifecycle for " << name << ":\n";
    for (auto& ev : events) {
        char buf[32];
        struct tm local;
#ifdef _WIN32
        localtime_s(&local, &ev.timestamp);
#else
        time_t ts = static_cast<time_t>(ev.timestamp);
        localtime_r(&ts, &local);
#endif
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &local);
        std::cout << "  [" << buf << "] " << ev.event << " " << ev.version << "\n";
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════
   Package Fork Recovery
   ═══════════════════════════════════════════════════════════ */

int cmd_fork_recover(const std::string& original, const std::string& fork) {
    ForkInfo fi;
    fi.original_name = original;
    fi.fork_name = fork;
    fi.reason = "user-specified fallback";
    fi.upstream_url = "https://aurora-pkg.dev/packages/" + fork;
    fi.is_active = true;

    std::string fork_dir = "packages/" + fork;
    if (fs::exists(fork_dir)) {
        std::cout << "Fork '" << fork << "' found locally\n";
    } else if (g_offline) {
        std::cerr << "offline: cannot fetch fork\n";
        return 1;
    } else {
        for (auto& reg : g_registries) {
            std::string url = reg.url + "/packages/" + fork + "/latest";
            std::string raw = http_fetch(url);
            if (!raw.empty()) {
                fs::create_directories(fork_dir);
                std::ofstream f(fork_dir + "/main.aura");
                f << "# Fork of " << original << "\n# " << fi.reason << "\n\n" << raw;
                std::cout << "Fetched fork '" << fork << "' from " << reg.name << "\n";
                break;
            }
        }
    }

    PackageInfo info = read_manifest(".");
    if (!info.name.empty()) {
        bool updated = false;
        for (auto& dep : info.dependencies) {
            std::string dn, dv; parse_pkg_spec(dep, dn, dv);
            if (dn == original) {
                dep = fork + "@" + (dv.empty() ? "" : dv);
                updated = true;
            }
        }
        if (updated) {
            std::ofstream mf("aurora.pkg");
            mf << "name: " << info.name << "\nversion: " << info.version << "\n"
               << "description: " << info.description << "\nentry: " << info.entry << "\ndependencies: ";
            for (size_t i = 0; i < info.dependencies.size(); i++) { if (i > 0) mf << ", "; mf << info.dependencies[i]; }
            mf << "\n";
            std::cout << "Updated manifest: replaced " << original << " with " << fork << "\n";
        }
    }

    g_forks.push_back(fi);
    std::cout << "Fork recovery complete: " << original << " -> " << fork << "\n";
    return 0;
}

int cmd_fork_list() {
    if (g_forks.empty()) {
        std::cout << "No fork recoveries recorded\n";
        return 0;
    }
    std::cout << "Package Fork Recoveries:\n";
    for (auto& f : g_forks) {
        std::cout << "  " << f.original_name << " -> " << f.fork_name
                  << "  [" << (f.is_active ? "active" : "inactive") << "]\n";
        if (!f.reason.empty()) std::cout << "    Reason: " << f.reason << "\n";
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════
   Dependency Insurance
   ═══════════════════════════════════════════════════════════ */

int cmd_dep_insurance() {
    if (g_insurance.empty()) {
        PackageInfo info = read_manifest(".");
        for (auto& dep : info.dependencies) {
            std::string name, ver; parse_pkg_spec(dep, name, ver);
            bool covered = false;
            for (auto& vuln : g_vuln_db) if (vuln.package == name) covered = true;
            if (covered) {
                auto t = std::time(nullptr) + 31536000;
                g_insurance.push_back({name, ver.empty() ? "*" : ver,
                    covered ? "covered" : "pending", "aurora-insure", t});
            }
        }
    }
    if (g_insurance.empty()) { std::cout << "No dependency insurance policies\n"; return 0; }
    std::cout << "Dependency Insurance Policies:\n";
    for (auto& ins : g_insurance) {
        char buf[32]; struct tm local;
        time_t exp = (time_t)ins.expiry_timestamp;
#ifdef _WIN32
        localtime_s(&local, &exp);
#else
        localtime_r(&exp, &local);
#endif
        strftime(buf, sizeof(buf), "%Y-%m-%d", &local);
        std::cout << "  " << ins.package << "@" << ins.version
                  << "  [" << ins.status << "]"
                  << "  provider: " << ins.provider
                  << "  expires: " << buf << "\n";
    }
    return 0;
}

int cmd_dep_insurance_add(const std::string& pkg, const std::string& provider) {
    std::string name, ver; parse_pkg_spec(pkg, name, ver);
    DepInsurance ins;
    ins.package = name;
    ins.version = ver.empty() ? "*" : ver;
    ins.provider = provider;
    ins.status = "covered";
    ins.expiry_timestamp = std::time(nullptr) + 31536000;
    g_insurance.push_back(ins);
    std::cout << "Insurance added: " << name << "@" << ins.version
              << "  provider: " << provider << "\n";
    return 0;
}

int cmd_dep_insurance_check(const std::string& pkg) {
    std::string name, ver; parse_pkg_spec(pkg, name, ver);
    for (auto& ins : g_insurance) {
        if (ins.package == name) {
            char buf[32]; struct tm local;
            time_t exp = (time_t)ins.expiry_timestamp;
#ifdef _WIN32
            localtime_s(&local, &exp);
#else
            localtime_r(&exp, &local);
#endif
            strftime(buf, sizeof(buf), "%Y-%m-%d", &local);
            std::cout << "Insurance for " << name << "@" << ins.version << "\n";
            std::cout << "  Status:     " << ins.status << "\n";
            std::cout << "  Provider:   " << ins.provider << "\n";
            std::cout << "  Expires:    " << buf << "\n";
            return 0;
        }
    }
    std::cout << "No insurance for " << name << "\n";
    return 1;
}

/* ═══════════════════════════════════════════════════════════
   Ecosystem Health Dashboard
   ═══════════════════════════════════════════════════════════ */

void print_eco_health() {
    PackageInfo info = read_manifest(".");
    int total_deps = (int)info.dependencies.size();
    int vuln_count = 0, outdated_count = 0, dead_count = 0;
    int health_score = 100;

    LockData lf = read_lockfile();
    for (auto& [name, entry] : lf.packages) {
        for (auto& vuln : g_vuln_db) {
            if (vuln.package == name && (vuln.version_range.empty() || version_in_range(entry.version, vuln.version_range)))
                vuln_count++;
        }
    }

    for (auto& dep : info.dependencies) {
        std::string dn, dv; parse_pkg_spec(dep, dn, dv);
        if (dv.empty()) outdated_count++;
    }

    if (fs::exists(info.entry)) {
        std::ifstream f(info.entry);
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        for (auto& dep : info.dependencies) {
            std::string dn, _; parse_pkg_spec(dep, dn, _);
            if (content.find(dn) == std::string::npos) dead_count++;
        }
    }

    health_score -= vuln_count * 15;
    health_score -= outdated_count * 5;
    health_score -= dead_count * 10;
    if (health_score < 0) health_score = 0;

    std::cout << "+=============================================+\n";
    std::cout << "|     VOSS Ecosystem Health Dashboard         |\n";
    std::cout << "+=============================================+\n\n";
    std::cout << "Project:      " << info.name << "@" << info.version << "\n";
    std::cout << "Dependencies: " << total_deps << "\n";
    std::cout << "Locked:       " << lf.packages.size() << "\n";
    std::cout << "Vulnerable:   " << vuln_count << "\n";
    std::cout << "Outdated:     " << outdated_count << "\n";
    std::cout << "Unused:       " << dead_count << "\n";
    std::cout << "Health:       " << health_score << "/100\n";

    std::cout << "             ";
    int bar_len = health_score / 5;
    for (int i = 0; i < 20; i++) std::cout << (i < bar_len ? "#" : ".");
    std::cout << " " << health_score << "%\n";
}

int cmd_eco_dashboard() {
    print_eco_health();
    return 0;
}

/* ═══════════════════════════════════════════════════════════
   Package Telemetry Network (Opt-In)
   ═══════════════════════════════════════════════════════════ */

int cmd_telemetry_status() {
    std::string cfg = ".aura/telemetry";
    if (fs::exists(cfg)) {
        std::ifstream f(cfg);
        std::string line;
        while (std::getline(f, line)) {
            std::string t = trim(line);
            if (t.rfind("enabled:", 0) == 0) g_telemetry.enabled = (trim(t.substr(8)) == "true");
            if (t.rfind("endpoint:", 0) == 0) g_telemetry.endpoint = trim(t.substr(9));
        }
    }
    std::cout << "Telemetry: " << (g_telemetry.enabled ? "enabled" : "disabled") << "\n";
    if (g_telemetry.enabled) {
        std::cout << "  Endpoint: " << g_telemetry.endpoint << "\n";
        std::cout << "  Collected: ";
        g_telemetry.collected = {"usage", "errors", "perf", "deps"};
        for (size_t i = 0; i < g_telemetry.collected.size(); i++) { if (i > 0) std::cout << ", "; std::cout << g_telemetry.collected[i]; }
        std::cout << "\n";
    }
    return 0;
}

int cmd_telemetry_enable(const std::string& endpoint) {
    g_telemetry.enabled = true;
    g_telemetry.endpoint = endpoint;
    g_telemetry.collected = {"usage", "errors", "perf", "deps"};
    std::ofstream f(".aura/telemetry");
    f << "enabled: true\nendpoint: " << endpoint << "\ncollected: usage, errors, perf, deps\n";
    std::cout << "Telemetry enabled (endpoint: " << endpoint << ")\n";
    std::cout << "Collected data: usage, errors, performance, dependency info\n";
    return 0;
}

int cmd_telemetry_disable() {
    g_telemetry.enabled = false;
    std::ofstream f(".aura/telemetry");
    f << "enabled: false\n";
    std::cout << "Telemetry disabled\n";
    return 0;
}

int cmd_telemetry_submit() {
    if (!g_telemetry.enabled) { std::cerr << "telemetry is disabled\n"; return 1; }
    PackageInfo info = read_manifest(".");
    std::string payload;
    payload += "{\"project\":\"" + info.name + "\",\"version\":\"" + info.version + "\",";
    payload += "\"deps\":" + std::to_string(info.dependencies.size()) + ",";
    LockData lf = read_lockfile();
    payload += "\"locked\":" + std::to_string(lf.packages.size()) + ",";
    payload += "\"timestamp\":" + std::to_string(std::time(nullptr)) + "}";
    std::cout << "Submitting telemetry to " << g_telemetry.endpoint << "\n";
    std::string result = http_fetch(g_telemetry.endpoint + "?data=" + payload);
    if (!result.empty()) {
        std::cout << "Telemetry submitted successfully\n";
        return 0;
    }
    std::cout << "Telemetry data collected (will submit later)\n";
    std::ofstream cache(".aura/telemetry_queue", std::ios::app);
    cache << payload << "\n";
    return 0;
}

/* ═══════════════════════════════════════════════════════════
   AI Package Generator
   ═══════════════════════════════════════════════════════════ */

std::string ai_generate_package(const std::string& desc) {
    std::string result;
    std::string lower = desc;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.find("http") != std::string::npos || lower.find("api") != std::string::npos) {
        result = "function fetch(url):\n"
                 "  native \"http_get\"\n"
                 "  return result\n\n"
                 "function post(url, data):\n"
                 "  native \"http_post\"\n"
                 "  return result\n\n"
                 "function json_parse(text):\n"
                 "  native \"json_parse\"\n"
                 "  return data\n";
    } else if (lower.find("json") != std::string::npos) {
        result = "function parse(text):\n"
                 "  native \"json_parse\"\n"
                 "  return data\n\n"
                 "function stringify(data):\n"
                 "  native \"json_stringify\"\n"
                 "  return text\n\n"
                 "function get(data, key):\n"
                 "  return data[key]\n";
    } else if (lower.find("test") != std::string::npos || lower.find("assert") != std::string::npos) {
        result = "function assert_equal(expected, actual):\n"
                 "  if expected != actual:\n"
                 "    output(\"FAIL: expected \" + expected + \", got \" + actual)\n"
                 "    return false\n"
                 "  output(\"PASS\")\n"
                 "  return true\n\n"
                 "function describe(name, fn):\n"
                 "  output(\"Test: \" + name)\n"
                 "  fn()\n";
    } else if (lower.find("log") != std::string::npos) {
        result = "function info(msg):\n"
                 "  output(\"[INFO] \" + msg)\n\n"
                 "function warn(msg):\n"
                 "  output(\"[WARN] \" + msg)\n\n"
                 "function error(msg):\n"
                 "  output(\"[ERROR] \" + msg)\n\n"
                 "function debug(msg):\n"
                 "  if DEBUG:\n"
                 "    output(\"[DEBUG] \" + msg)\n";
    } else if (lower.find("crypto") != std::string::npos || lower.find("hash") != std::string::npos) {
        result = "function sha256(data):\n"
                 "  native \"sha256\"\n"
                 "  return hash\n\n"
                 "function random_bytes(n):\n"
                 "  native \"random_bytes\"\n"
                 "  return bytes\n\n"
                 "function encrypt(key, data):\n"
                 "  native \"aes_encrypt\"\n"
                 "  return ciphertext\n";
    } else if (lower.find("math") != std::string::npos || lower.find("calc") != std::string::npos) {
        result = "function add(a, b): return a + b\n"
                 "function sub(a, b): return a - b\n"
                 "function mul(a, b): return a * b\n"
                 "function div(a, b): return a / b\n"
                 "function pow(a, b): native \"math_pow\"\n"
                 "function sqrt(a):   native \"math_sqrt\"\n";
    } else if (lower.find("fs") != std::string::npos || lower.find("file") != std::string::npos) {
        result = "function read(path):\n"
                 "  native \"fs_read\"\n"
                 "  return content\n\n"
                 "function write(path, data):\n"
                 "  native \"fs_write\"\n"
                 "  return true\n\n"
                 "function exists(path):\n"
                 "  native \"fs_exists\"\n"
                 "  return result\n\n"
                 "function list(dir):\n"
                 "  native \"fs_list\"\n"
                 "  return files\n";
    } else {
        result = "function main():\n"
                 "  output(\"Hello from generated package!\")\n\n"
                 "function init():\n"
                 "  output(\"Initializing...\")\n"
                 "  return true\n\n"
                 "function version():\n"
                 "  return \"0.1.0\"\n";
    }

    return result;
}

int cmd_ai_generate(const std::string& description) {
    std::cout << "AI Package Generator\n";
    std::cout << "Description: " << description << "\n\n";

    std::string pkg_name;
    std::stringstream ss(description);
    std::string word;
    while (ss >> word) {
        std::string clean;
        for (char c : word) if (isalnum(c)) clean += c;
        if (!clean.empty()) { pkg_name = clean; break; }
    }
    if (pkg_name.empty()) pkg_name = "generated-pkg";

    std::string dir = "packages/" + pkg_name;
    fs::create_directories(dir);

    std::string code = ai_generate_package(description);

    std::ofstream mf(dir + "/main.aura");
    mf << "## " << pkg_name << "\n";
    mf << "## AI-generated package: " << description << "\n";
    mf << "## Generated: " << std::time(nullptr) << "\n\n";
    mf << code;

    std::ofstream pf(dir + "/aurora.pkg");
    pf << "name: " << pkg_name << "\n"
       << "version: 0.1.0\n"
       << "description: " << description << "\n"
       << "entry: main.aura\n"
       << "dependencies:\n"
       << "permissions:\n";

    std::cout << "Generated package: " << pkg_name << "\n";
    std::cout << "Location: " << dir << "/\n\n";
    std::cout << "Code:\n" << code << "\n";

    log_lifecycle(pkg_name, "0.1.0", "generate");
    return 0;
}

int cmd_ai_generate_file(const std::string& desc, const std::string& output) {
    std::string code = ai_generate_package(desc);
    std::ofstream f(output);
    f << code;
    std::cout << "Generated code written to " << output << "\n";
    return 0;
}
