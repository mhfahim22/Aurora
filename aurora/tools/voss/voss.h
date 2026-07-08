#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cctype>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <thread>
#include <mutex>
#include <functional>
#include <chrono>
#include <random>
#include <queue>
#include <iomanip>
#include <ctime>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt")
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/utsname.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

namespace fs = std::filesystem;

static const char* AURA_VERSION = "0.3.0";
static const int LOCK_VERSION = 1;
static const int MAX_THREADS = 8;
static const char* DEFAULT_REGISTRY_URL = "";
extern bool g_offline;
extern std::string g_cross_target; /* e.g. "aarch64-unknown-linux-gnu" */
extern bool g_manual_mode;         /* --manual flag for cargo bridge */

struct RegistryEntry { std::string name, url; };
extern std::vector<RegistryEntry> g_registries;

struct PackageInfo {
    std::string name, version, author, description, entry;
    std::vector<std::string> dependencies, permissions;
};

struct LockEntry {
    std::string name, version, resolved, integrity;
    std::vector<std::string> dependencies;
};

struct LockData {
    int version = LOCK_VERSION;
    std::map<std::string, LockEntry> packages;
};

struct DepNode {
    std::string name, version;
    std::vector<DepNode> children;
    bool conflict = false;
    std::string conflict_msg;
};

struct Vulnerability {
    std::string id, package, version_range, description, severity;
};

struct AuraRecommendEntry {
    std::string name, desc;
    std::vector<std::string> also;
};

struct PermGroup {
    std::string name;
    std::vector<std::string> perms;
    bool denied = false;
};

extern std::vector<Vulnerability> g_vuln_db;
extern std::vector<AuraRecommendEntry> g_rec_db;

/* ── Tier 3.5: AI Compatibility Scanner ── */
struct AIScanResult {
    std::string package, from_version, to_version;
    std::vector<std::string> breaking_changes;
    std::vector<std::string> impacted_files;
    int risk_score; /* 0-100 */
    std::string summary;
};

/* ── Tier 4.2: Dependency Forecast ── */
struct DepForecast {
    std::string package, current_version;
    int abandonment_risk; /* 0-100 */
    int risk_forecast;    /* 0-100 */
    double maintenance_score; /* 0.0-1.0 */
    std::string last_update, next_forecast;
    std::string recommendation;
};

/* ── Additional: LTS Channel ── */
struct LTSChannel {
    std::string name, version, status; /* active | maintenance | eol */
    std::string release_date, eol_date;
};

/* ── Additional: Changelog Entry ── */
struct ChangelogEntry {
    std::string version, date, type; /* added | changed | deprecated | removed | fixed | security */
    std::string description;
};

/* ── Additional: Lifecycle Event ── */
struct LifecycleEvent {
    std::string package, version, event; /* install | update | remove | deprecated | vulnerable */
    int64_t timestamp;
};

/* ── Additional: Telemetry Config ── */
struct TelemetryConfig {
    bool enabled = false;
    std::string endpoint;
    std::vector<std::string> collected; /* usage | errors | perf | deps */
};

/* ── Additional: Fork Info ── */
struct ForkInfo {
    std::string original_name, fork_name;
    std::string reason;
    std::string upstream_url;
    bool is_active = true;
};

/* ── Additional: Package Metrics ── */
struct PackageMetrics {
    std::string name;
    int downloads = 0, stars = 0, forks = 0, open_issues = 0;
    double activity_score = 0.0;
    std::string last_commit;
    int contributor_count = 0;
    int release_count = 0;
    double bus_factor = 0.0; /* 0.0-1.0 (higher = riskier) */
};

/* ── Additional: Insurance Record ── */
struct InsuranceRecord {
    std::string package, provider, policy_id;
    std::string coverage_type;
    int64_t expiry;
    bool active = false;
};

/* ── Additional: Cloud Build Config ── */
struct CloudBuildConfig {
    bool enabled = false;
    std::string provider, project_id;
    std::vector<std::string> agents;
    bool reproducible = true;
};

/* ── Additional: Binary Distribution ── */
struct BinaryDistConfig {
    bool enabled = false;
    std::string platform, arch, format;
    std::vector<std::string> targets;
};

/* ── Additional: Dependency Insurance ── */
struct DepInsurance {
    std::string package, version, status; /* covered | expired | pending */
    std::string provider;
    int64_t expiry_timestamp;
};

extern std::vector<ForkInfo> g_forks;
extern TelemetryConfig g_telemetry;
extern CloudBuildConfig g_cloud_build;
extern BinaryDistConfig g_binary_dist;
extern std::vector<DepInsurance> g_insurance;
extern std::vector<LTSChannel> g_lts_channels;
extern std::vector<PackageMetrics> g_metrics_cache;

inline std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

inline std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> parts;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        std::string t = trim(item);
        if (!t.empty()) parts.push_back(t);
    }
    return parts;
}

inline bool parse_pkg_spec(const std::string& spec, std::string& name, std::string& version) {
    size_t at = spec.find('@');
    if (at == std::string::npos) { name = spec; version = ""; }
    else { name = spec.substr(0, at); version = spec.substr(at + 1); }
    name = trim(name);
    return !name.empty();
}

inline std::string sha256_hex(const std::string& data) {
    uint32_t h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    const uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
        0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
        0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
        0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
        0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };
    auto rotr = [](uint32_t x, uint32_t n) -> uint32_t { return (x >> n) | (x << (32 - n)); };
    uint64_t bit_len = data.size() * 8;
    size_t new_len = ((data.size() + 8 + 64) / 64) * 64;
    std::vector<uint8_t> buf(new_len, 0);
    memcpy(buf.data(), data.data(), data.size());
    buf[data.size()] = 0x80;
    for (size_t i = 0; i < 8; i++) buf[new_len - 8 + i] = (uint8_t)(bit_len >> (56 - i * 8));
    for (size_t block = 0; block < new_len; block += 64) {
        uint32_t w[64];
        for (int t = 0; t < 16; t++)
            w[t] = ((uint32_t)buf[block + t * 4] << 24) | ((uint32_t)buf[block + t * 4 + 1] << 16) | ((uint32_t)buf[block + t * 4 + 2] << 8) | ((uint32_t)buf[block + t * 4 + 3]);
        for (int t = 16; t < 64; t++) {
            uint32_t s0 = rotr(w[t - 15], 7) ^ rotr(w[t - 15], 18) ^ (w[t - 15] >> 3);
            uint32_t s1 = rotr(w[t - 2], 17) ^ rotr(w[t - 2], 19) ^ (w[t - 2] >> 10);
            w[t] = w[t - 16] + s0 + w[t - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int t = 0; t < 64; t++) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t temp1 = hh + S1 + ch + k[t] + w[t];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t temp2 = S0 + maj;
            hh = g; g = f; f = e; e = d + temp1;
            d = c; c = b; b = a; a = temp1 + temp2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    std::string hex;
    const char* hex_chars = "0123456789abcdef";
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 4; j++)
        { uint8_t byte = (uint8_t)(h[i] >> (24 - j * 8)); hex += hex_chars[(byte >> 4) & 0xf]; hex += hex_chars[byte & 0xf]; }
    return "sha256-" + hex;
}

inline int semver_cmp(const std::string& a, const std::string& b) {
    auto parse = [](const std::string& s) -> std::vector<int> {
        std::vector<int> parts;
        std::stringstream ss(s); std::string p;
        while (std::getline(ss, p, '.')) {
            try { parts.push_back(std::stoi(p)); } catch (...) { parts.push_back(0); }
        }
        return parts;
    };
    auto va = parse(a), vb = parse(b);
    size_t max_sz = (va.size() > vb.size()) ? va.size() : vb.size();
    for (size_t i = 0; i < max_sz; i++) {
        int na = i < va.size() ? va[i] : 0;
        int nb = i < vb.size() ? vb[i] : 0;
        if (na != nb) return na - nb;
    }
    return 0;
}

inline bool version_satisfies(const std::string& ver, const std::string& range) {
    if (range.empty() || range == "*") return true;
    if (range[0] == '^') {
        std::string min_ver = range.substr(1);
        if (semver_cmp(ver, min_ver) < 0) return false;
        size_t dot = min_ver.find('.');
        std::string major = (dot == std::string::npos) ? min_ver : min_ver.substr(0, dot);
        return ver.rfind(major + ".", 0) == 0;
    }
    if (range[0] == '~') {
        std::string min_ver = range.substr(1);
        if (semver_cmp(ver, min_ver) < 0) return false;
        size_t d1 = min_ver.find('.');
        if (d1 == std::string::npos) return true;
        size_t d2 = min_ver.find('.', d1 + 1);
        std::string minor_prefix = (d2 == std::string::npos) ? min_ver : min_ver.substr(0, d2);
        return ver.rfind(minor_prefix + ".", 0) == 0;
    }
    if (range.size() >= 2 && range[0] == '>' && range[1] == '=') return semver_cmp(ver, trim(range.substr(2))) >= 0;
    if (range[0] == '>') return semver_cmp(ver, trim(range.substr(1))) > 0;
    if (range.size() >= 2 && range[0] == '<' && range[1] == '=') return semver_cmp(ver, trim(range.substr(2))) <= 0;
    if (range[0] == '<') return semver_cmp(ver, trim(range.substr(1))) < 0;
    return ver == range;
}

PackageInfo read_manifest(const std::string& path);
LockData read_lockfile();
void write_lockfile(const LockData& lf);
std::string cache_dir();
std::string cache_get(const std::string& key);
std::string cache_get_ttl(const std::string& key, int max_age_seconds);
void cache_put(const std::string& key, const std::string& data);
std::string http_fetch(const std::string& url);
std::string extract_json_source(const std::string& raw);
bool resolve_package(const std::string& spec, std::string& name, std::string& version, std::string& source, std::string& integrity);
void load_registries();
int run_cmd(const std::vector<std::string>& args);
DepNode build_tree(const std::string& name, const std::string& version, std::set<std::string>& visited, std::map<std::string, std::string>& versions);
void print_tree(const DepNode& node, const std::string& prefix, bool is_last);
bool version_in_range(const std::string& ver, const std::string& range);
std::vector<std::string> rec_find(const std::string& name);
void warn_install_perms(const std::string& pkg_name);
void reset_perm_session();
int cmd_help();

int cmd_init(const std::string& name);
int cmd_sign(const std::string& spec);
int cmd_add(const std::string& pkg, const std::string& version = "");
int cmd_verify();
int cmd_verify(const std::string& spec);
bool verify_package_signature(const std::string& name, const std::string& version);
int cmd_install(const std::string& pkg, const std::string& ecosystem_hint = "");
int cmd_install_parallel(const std::vector<std::string>& pkgs);
int cmd_uninstall(const std::string& pkg);
int cmd_lock();
int cmd_build(bool verbose, bool auto_yes = false);
int cmd_run(bool verbose, bool auto_yes);
int cmd_test(bool verbose);
int cmd_clean();
int cmd_info();
int cmd_list();
int cmd_tree();
int cmd_audit();
int cmd_health();
int cmd_snapshot(const std::string& label);
int cmd_snapshots();
int cmd_restore(const std::string& snapshot);
int cmd_sandbox(const std::string& pkg, bool verbose);
int cmd_dead_deps();
int cmd_detect();
int cmd_trust(const std::string& pkg);
int cmd_migrate(const std::string& spec);
int cmd_why(const std::string& pkg);
int cmd_export(const std::string& dir);
int cmd_import_project(const std::string& dir);
int cmd_import_deps(const std::vector<std::string>& fmts);
int cmd_bench(const std::vector<std::string>& pkgs);
int cmd_recommend(const std::string& pkg);
int cmd_perms_list();
int cmd_perms_allow(const std::string& perm);
int cmd_perms_deny(const std::string& perm);
int cmd_perms_reset();
int cmd_perms_review();
int cmd_cache_clean();
int cmd_cache_info();
int cmd_registry_list();
int cmd_registry_add(const std::string& name, const std::string& url);
int cmd_registry_remove(const std::string& name);
int cmd_registry_set(const std::string& name);
int cmd_registry_register(const std::string& spec, const std::string& registry_spec);
int cmd_registry_login(const std::string& registry_spec);
int cmd_registry_github_register(const std::string& user_repo, const std::string& version);
std::string create_tgz(const std::string& src_dir, const std::string& name, const std::string& version);
int cmd_workspace_init(const std::vector<std::string>& pkgs);
int cmd_workspace_list();
int cmd_workspace_build(bool verbose, bool auto_yes);
int cmd_doctor();
int cmd_update();
int cmd_search(const std::string& query);
int cmd_publish();
int cmd_repair();
int cmd_graph();
int cmd_freeze();
int cmd_unfreeze();
int cmd_outdated();
int cmd_license();
int cmd_suggest(const std::string& pkg_name);
int cmd_simulate(const std::vector<std::string>& packages);
int cmd_mirror_create();
int cmd_mirror_update();
int cmd_mirror_status();
int cmd_audit_fix();
int cmd_dedupe();
bool is_frozen();
std::string freeze_file();
std::string mirror_dir();
void graph_print(const DepNode& node, const std::string& prefix, bool is_last);

/* ── AI Compatibility Scanner (Tier 3.5) ── */
int cmd_ai_scan(const std::string& spec);
AIScanResult ai_scan_package(const std::string& name, const std::string& from_ver, const std::string& to_ver);
std::vector<std::string> detect_breaking_changes(const std::string& source);
int analyze_upgrade_risk(const std::string& name, const std::string& from, const std::string& to);

/* ── Dependency Forecast (Tier 4.2) ── */
int cmd_forecast();
int cmd_forecast_package(const std::string& pkg);
DepForecast forecast_dependency(const std::string& name, const std::string& version);

/* ── LTS Channels ── */
int cmd_lts_list();
int cmd_lts_add(const std::string& name, const std::string& version, const std::string& status);
int cmd_lts_remove(const std::string& name);
int cmd_lts_switch(const std::string& name);

/* ── Reproducible Cloud Builds ── */
int cmd_cloud_build_init();
int cmd_cloud_build_run();
int cmd_cloud_build_status();
int cmd_cloud_build_config();

/* ── Binary Package Distribution ── */
int cmd_binary_package();
int cmd_binary_install(const std::string& pkg);
int cmd_binary_list();

/* ── Built-in Changelog Analysis ── */
int cmd_changelog(const std::string& pkg);
std::string fetch_changelog(const std::string& pkg, const std::string& version);

/* ── Security Risk Forecast ── */
int cmd_risk_forecast();
int cmd_risk_forecast_package(const std::string& pkg);

/* ── Package Lifecycle Monitor ── */
int cmd_lifecycle();
int cmd_lifecycle_package(const std::string& pkg);
void log_lifecycle(const std::string& pkg, const std::string& version, const std::string& event);

/* ── Package Fork Recovery ── */
int cmd_fork_recover(const std::string& original, const std::string& fork);
int cmd_fork_list();

/* ── Dependency Insurance ── */
int cmd_dep_insurance();
int cmd_dep_insurance_add(const std::string& pkg, const std::string& provider);
int cmd_dep_insurance_check(const std::string& pkg);

/* ── Ecosystem Health Dashboard ── */
int cmd_eco_dashboard();
void print_eco_health();

/* ── Package Telemetry Network (Opt-In) ── */
int cmd_telemetry_status();
int cmd_telemetry_enable(const std::string& endpoint);
int cmd_telemetry_disable();
int cmd_telemetry_submit();

/* ── Package Templates (voss new) ── */
int cmd_new(int argc, char** argv);

/* ── Packaging (voss package) ── */
int cmd_package(const std::string& target, const std::string& format);

/* ── Documentation Generator (voss doc) ── */
int cmd_doc(const std::string& output_dir, bool serve);

/* ── Theme Store ── */
int cmd_theme(int argc, char** argv);

/* ── File helpers (used by theme and other commands) ── */
std::string read_file_str(const std::string& path);

/* ── AI Package Generator ── */
int cmd_ai_generate(const std::string& description);
int cmd_ai_generate_file(const std::string& desc, const std::string& output);
std::string ai_generate_package(const std::string& desc);

/* ── Ecosystem Bridges (Phase 3) ── */
int cmd_bridge(const std::string& ecosystem, const std::string& pkg, const std::string& version);
int cmd_bridge_auto(const std::string& pkg, const std::string& version, const std::string& hint_eco = "");

/* ── Automatic Binding Generator (Phase 2) ── */
int cmd_bind(const std::string& library, const std::vector<std::string>& headers,
             const std::string& output_dir, const std::string& lib_name,
             const std::string& package, const std::vector<std::string>& inc_dirs,
             const std::vector<std::string>& defs, const std::string& call_conv,
             bool no_cache, bool verbose, bool no_macros, bool no_functions,
             bool no_structs, bool no_unions, bool no_typedefs);

/* ── GitHub Registry Integration ── */
struct VossPackage {
    std::string name, version, entry;
    std::vector<std::string> dependencies;
};
bool parse_github_spec(const std::string& spec, std::string& user, std::string& repo, std::string& ref, bool& is_branch);
VossPackage read_voss_json(const std::string& dir);
VossPackage parse_voss_json_string(const std::string& json);
bool resolve_github_package(const std::string& user, const std::string& repo, const std::string& ver, std::string& out_name, std::string& out_version, std::string& out_source, std::string& out_integrity, VossPackage& out_pkg);
std::vector<std::string> github_list_tags(const std::string& user, const std::string& repo);
int cmd_install_github(const std::string& spec);
int cmd_publish_github(const std::string& user, const std::string& repo, const std::string& version);
