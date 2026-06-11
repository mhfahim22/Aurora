#include "voss.h"
#include "runtime/interop/http_client.hpp"
#include "runtime/interop/universal_resolver.hpp"
#include "runtime/interop/cross_ecosystem_deps.hpp"

/* ── helpers ── */
bool version_in_range(const std::string& ver, const std::string& range) {
    if (range.empty()) return true;
    if (range[0] == '>') return semver_cmp(ver, range.substr(1)) > 0;
    if (range[0] == '<') return semver_cmp(ver, range.substr(1)) < 0;
    if (range.find("||") != std::string::npos) {
        auto parts = split(range, '|');
        for (auto& p : parts) if (version_in_range(ver, trim(p))) return true;
        return false;
    }
    return semver_cmp(ver, range) == 0;
}

std::vector<std::string> rec_find(const std::string& name) {
    std::vector<std::string> results;
    for (auto& e : g_rec_db) { if (e.name == name) for (auto& alt : e.also) results.push_back(alt); }
    for (auto& e : g_rec_db) {
        if (e.name.find(name) != std::string::npos || name.find(e.name) != std::string::npos)
            if (std::find(results.begin(), results.end(), e.name) == results.end()) results.push_back(e.name);
    }
    return results;
}

void warn_install_perms(const std::string& pkg_name) {
    PackageInfo info = read_manifest("packages/" + pkg_name);
    if (!info.permissions.empty()) {
        std::cout << "warning: " << pkg_name << " requires permissions: ";
        for (size_t i = 0; i < info.permissions.size(); i++) {
            if (i > 0) std::cout << ", ";
            std::cout << info.permissions[i];
        }
        std::cout << "\n";
    }
}

/* ── Cached HTTP GET with filesystem cache (1h TTL for registry data) ── */
static std::string cached_http_get(const std::string& url) {
    std::string cache_key = "http_" + sha256_hex(url);
    std::string cached = cache_get_ttl(cache_key, 3600);
    if (!cached.empty()) {
        return cached;
    }
    std::string result = http_get(url);
    if (!result.empty()) {
        cache_put(cache_key, result);
    }
    return result;
}

/* ── Recursive dependency resolver using CrossEcosystemResolver ── */
static void resolve_recursive_deps(const std::string& pkg_name, const std::string& pkg_ver,
                                   const std::string& ecosystem, LockData& lf) {
    HttpGetFn http_fn = [](const std::string& u) { return cached_http_get(u); };
    UniversalResolver resolver(http_fn);
    CrossEcosystemResolver cross_resolver(resolver);
    cross_resolver.init_builtin_mappings();

    Ecosystem eco_enum = ecosystem_from_name(ecosystem);
    if (eco_enum == Ecosystem::Unknown) return;

    CrossDepResolution result = cross_resolver.resolve(pkg_name, eco_enum, pkg_ver, 5);
    if (!result.success) return;

    auto flat = result.flattened();
    for (auto& [key, node] : flat) {
        if (lf.packages.find(node.name) != lf.packages.end()) continue;
        LockEntry le;
        le.name = node.name;
        le.version = node.version;
        le.resolved = std::string(ecosystem_name(node.ecosystem)) + ":" + node.name + "@" + node.version;
        le.integrity = sha256_hex(node.name + "@" + node.version);
        lf.packages[node.name] = le;
        std::cout << "  resolved " << node.name << "@" << node.version
                  << " (" << ecosystem_name(node.ecosystem) << ")\n";
    }
}

/* ── Core commands ── */
int cmd_init(const std::string& name) {
    if (fs::exists("aurora.pkg")) { std::cerr << "error: aurora.pkg already exists\n"; return 1; }
    std::ofstream f("aurora.pkg");
    f << "name: " << name << "\nversion: 0.1.0\ndescription: " << name << "\nentry: main.aura\ndependencies:\npermissions:\n";
    if (!fs::exists("main.aura")) { std::ofstream mf("main.aura"); mf << "/* " << name << " */\n"; }
    std::cout << "initialized " << name << "\n"; return 0;
}

int cmd_lock() {
    PackageInfo info = read_manifest(".");
    if (info.name.empty()) { std::cerr << "error: no aurora.pkg found\n"; return 1; }
    LockData lf = read_lockfile();
    for (auto& dep : info.dependencies) {
        std::string name, version; parse_pkg_spec(dep, name, version);
        if (lf.packages.find(name) != lf.packages.end()) continue;
        std::string rname, rver, source, integrity;
        if (resolve_package(dep, rname, rver, source, integrity)) {
            LockEntry le; le.name = rname; le.version = rver; le.resolved = source; le.integrity = integrity;
            lf.packages[rname] = le;
        }
        /* Recursively resolve transitive deps across ecosystems */
        for (auto& eco_name : {"pypi", "npm", "cargo"}) {
            resolve_recursive_deps(name, version, eco_name, lf);
        }
    }
    write_lockfile(lf);
    std::cout << "locked " << lf.packages.size() << " packages\n"; return 0;
}

int cmd_install_github(const std::string& spec) {
    std::string user, repo, ref;
    bool is_branch = false;
    if (!parse_github_spec(spec, user, repo, ref, is_branch)) {
        std::cerr << "error: invalid github spec '" << spec << "'\n";
        std::cerr << "usage: voss install github:user/repo[@tag][#branch]\n";
        return 1;
    }
    std::string clone_url = "https://github.com/" + user + "/" + repo + ".git";
    std::string temp_dir = ".__voss_clone_" + repo;
    std::string target_dir = "voss_packages/" + repo;

    // Resolve version via GitHub API if no explicit ref given
    std::string resolved_ref = ref;
    if (resolved_ref.empty() && !is_branch) {
        std::vector<std::string> tags = github_list_tags(user, repo);
        if (!tags.empty()) {
            std::string latest = tags[0];
            for (auto& t : tags) {
                std::string tv = (t.rfind("v", 0) == 0) ? t.substr(1) : t;
                if (semver_cmp(tv, (latest.rfind("v", 0) == 0) ? latest.substr(1) : latest) > 0)
                    latest = t;
            }
            resolved_ref = latest;
        }
    }

    // Check voss_index.json for existing install
    std::string index_path = "voss_index.json";
    std::map<std::string, std::string> idx_versions;
    std::ifstream idx_r(index_path);
    if (idx_r.is_open()) {
        std::stringstream ss;
        ss << idx_r.rdbuf();
        std::string idxc = ss.str();
        size_t pkg_start = idxc.find('"', 2);
        while (pkg_start != std::string::npos) {
            size_t pkg_end = idxc.find('"', pkg_start + 1);
            if (pkg_end == std::string::npos) break;
            std::string idx_name = idxc.substr(pkg_start + 1, pkg_end - pkg_start - 1);
            size_t ver_f = idxc.find("\"version\":", pkg_end);
            if (ver_f != std::string::npos) {
                ver_f = idxc.find('"', ver_f + 10);
                if (ver_f != std::string::npos) {
                    size_t ver_e = idxc.find('"', ver_f + 1);
                    idx_versions[idx_name] = idxc.substr(ver_f + 1, ver_e - ver_f - 1);
                }
            }
            pkg_start = idxc.find('"', pkg_end + 1);
        }
    }

    std::string install_key = user + "/" + repo;
    if (idx_versions.count(install_key)) {
        std::cout << repo << " already installed (" << idx_versions[install_key] << ")\n";
        return 0;
    }
    LockData lf = read_lockfile();
    auto lk = lf.packages.find(repo);
    if (lk != lf.packages.end() && lk->second.resolved.rfind("github:", 0) == 0) {
        std::cout << repo << " already installed (" << lk->second.version << ")\n";
        return 0;
    }

    std::cout << "warning: installing from untrusted repository '" << user << "/" << repo << "'\n";
    if (fs::exists(temp_dir)) fs::remove_all(temp_dir);
    std::string git_cmd = "git clone";
    if (!resolved_ref.empty()) git_cmd += " -b \"" + resolved_ref + "\"";
    git_cmd += " \"" + clone_url + "\" \"" + temp_dir + "\" 2>&1";
    std::cout << "cloning " << user << "/" << repo;
    if (!resolved_ref.empty()) std::cout << " (" << (is_branch ? "#" : "@") << resolved_ref << ")";
    std::cout << "...\n";
    if (system(git_cmd.c_str()) != 0) {
        std::cerr << "error: failed to clone " << clone_url << "\n";
        if (fs::exists(temp_dir)) fs::remove_all(temp_dir);
        return 1;
    }
    if (!fs::exists(temp_dir + "/voss.json")) {
        std::cerr << "error: Invalid VOSS package - missing voss.json\n";
        fs::remove_all(temp_dir);
        return 1;
    }
    VossPackage pkg = read_voss_json(temp_dir);
    if (pkg.name.empty() || pkg.version.empty() || pkg.entry.empty()) {
        std::cerr << "error: Invalid VOSS package - voss.json must contain name, version, and entry fields\n";
        fs::remove_all(temp_dir);
        return 1;
    }
    if (!fs::exists(temp_dir + "/" + pkg.entry)) {
        std::cerr << "error: entry file '" << pkg.entry << "' not found in package\n";
        fs::remove_all(temp_dir);
        return 1;
    }
    fs::create_directories("voss_packages");
    if (fs::exists(target_dir)) fs::remove_all(target_dir);
    fs::rename(temp_dir, target_dir);

    // Update voss_index.json
    std::string idx_content;
    std::ifstream idx_r2(index_path);
    if (idx_r2.is_open()) {
        std::stringstream ss;
        ss << idx_r2.rdbuf();
        idx_content = ss.str();
    }
    if (idx_content.empty()) idx_content = "{\n";
    size_t close_brace = idx_content.rfind('}');
    if (close_brace == std::string::npos) {
        idx_content = "{\n";
        close_brace = idx_content.size();
    }
    std::string comma = "";
    std::string before_close = idx_content.substr(0, close_brace);
    size_t last_brace = before_close.rfind('{');
    size_t last_content = before_close.find_last_not_of(" \t\n\r");
    if (last_content != std::string::npos && last_content > last_brace) comma = ",\n";
    std::string idx_entry = comma + "  \"" + install_key + "\": {\n    \"version\": \"" + pkg.version + "\",\n    \"source\": \"github:" + user + "/" + repo + "\",\n    \"entry\": \"" + pkg.entry + "\",\n    \"path\": \"" + target_dir + "\",\n    \"ref\": \"" + (resolved_ref.empty() ? "main" : resolved_ref) + "\"\n  }\n";
    idx_content.insert(close_brace, idx_entry);
    if (idx_content.back() != '}') idx_content += "\n}\n";
    std::ofstream idx_w(index_path);
    idx_w << idx_content;

    // Register in aura.lock for integration with existing commands
    std::string source_str = "github:" + user + "/" + repo + (resolved_ref.empty() ? "" : "@" + resolved_ref);
    if (!lf.packages.count(pkg.name)) {
        LockEntry le;
        le.name = pkg.name;
        le.version = pkg.version;
        le.resolved = source_str;
        le.integrity = sha256_hex(pkg.name + "@" + pkg.version);
        le.dependencies = pkg.dependencies;
        lf.packages[pkg.name] = le;
        write_lockfile(lf);
    }

    std::cout << "successfully installed " << pkg.name << "@" << pkg.version;
    if (!resolved_ref.empty()) std::cout << " (from " << (is_branch ? "#" : "@") << resolved_ref << ")";
    std::cout << "\n";

    // Resolve transitive dependencies from voss.json
    if (!pkg.dependencies.empty()) {
        std::cout << "resolving dependencies for " << pkg.name << "...\n";
        for (auto& dep : pkg.dependencies) {
            std::string dep_name, dep_ver;
            parse_pkg_spec(dep, dep_name, dep_ver);
            auto dit = lf.packages.find(dep_name);
            if (dit != lf.packages.end()) continue;
            VossPackage dep_pkg;
            std::string rn, rv, rs, ri;
            if (resolve_package(dep, rn, rv, rs, ri)) {
                LockEntry de;
                de.name = rn;
                de.version = rv;
                de.resolved = rs;
                de.integrity = ri;
                lf.packages[rn] = de;
                std::cout << "  resolved " << rn << "@" << rv << " (" << rs << ")\n";
            } else {
                std::cout << "  warning: could not resolve dependency " << dep << "\n";
            }
        }
        write_lockfile(lf);
    }
    return 0;
}

int cmd_install(const std::string& pkg) {
    if (pkg.find("github:") == 0 || pkg.find("gh:") == 0) {
        return cmd_install_github(pkg);
    }
    std::string name, version;
    if (!parse_pkg_spec(pkg, name, version)) { std::cerr << "error: invalid spec\n"; return 1; }
    if (name.find('/') != std::string::npos && name.find("://") == std::string::npos && name.find('\\') == std::string::npos) {
        return cmd_install_github("github:" + name + (version.empty() ? "" : "@" + version));
    }
    PackageInfo info = read_manifest(".");
    if (info.name.empty()) { std::cerr << "error: no aurora.pkg found\n"; return 1; }
    for (auto& dep : info.dependencies) {
        std::string dn, dv; parse_pkg_spec(dep, dn, dv);
        if (dn == name) { std::cout << name << " already installed\n"; return 0; }
    }
    std::string rname, rver, source, integrity;
    if (!resolve_package(pkg, rname, rver, source, integrity)) {
        fs::create_directories("packages/" + name);
        std::ofstream sf("packages/" + name + "/aurora.pkg");
        sf << "name: " << name << "\nversion: " << (version.empty() ? "1.0.0" : version) << "\ndependencies:\npermissions:\n";
        std::ofstream mf("packages/" + name + "/main.aura"); mf << "/* " << name << " stub */\n";
        rver = version.empty() ? "1.0.0" : version; source = "stub"; integrity = sha256_hex(rver);
    }
    info.dependencies.push_back(name + "@" + rver);
    LockData lf = read_lockfile();
    LockEntry le; le.name = name; le.version = rver; le.resolved = source; le.integrity = integrity;
    lf.packages[name] = le;

    /* Recursively resolve transitive deps across ecosystems */
    for (auto& eco_name : {"pypi", "npm", "cargo"}) {
        resolve_recursive_deps(name, rver, eco_name, lf);
    }

    write_lockfile(lf);
    std::ofstream mf2("aurora.pkg");
    mf2 << "name: " << info.name << "\nversion: " << info.version << "\ndescription: " << info.description << "\nentry: " << info.entry << "\ndependencies: ";
    for (size_t i = 0; i < info.dependencies.size(); i++) { if (i > 0) mf2 << ", "; mf2 << info.dependencies[i]; }
    mf2 << "\n";
    if (!info.permissions.empty()) { mf2 << "permissions: "; for (size_t i = 0; i < info.permissions.size(); i++) { if (i > 0) mf2 << ", "; mf2 << info.permissions[i]; } mf2 << "\n"; }
    std::cout << "installed " << name << "@" << rver << "\n"; return 0;
}

int cmd_install_parallel(const std::vector<std::string>& pkgs) {
    int success = 0, fail = 0;
    std::vector<std::thread> threads;
    std::mutex result_mutex;
    for (const auto& pkg : pkgs) {
        threads.emplace_back([pkg, &success, &fail, &result_mutex]() {
            int rc = cmd_install(pkg);
            std::lock_guard<std::mutex> lock(result_mutex);
            if (rc == 0) success++; else fail++;
        });
    }
    for (auto& t : threads) t.join();
    std::cout << "installed " << success << ", failed " << fail << " (parallel)\n";
    return fail > 0 ? 1 : 0;
}

int cmd_uninstall(const std::string& pkg) {
    PackageInfo info = read_manifest(".");
    std::string name, _v; parse_pkg_spec(pkg, name, _v);
    auto it = std::find_if(info.dependencies.begin(), info.dependencies.end(),
        [&name](const std::string& d) { std::string dn, _; parse_pkg_spec(d, dn, _); return dn == name; });
    if (it == info.dependencies.end()) { std::cerr << "error: " << name << " not installed\n"; return 1; }
    info.dependencies.erase(it);
    LockData lf = read_lockfile(); lf.packages.erase(name); write_lockfile(lf);
    if (fs::exists("packages/" + name)) fs::remove_all("packages/" + name);
    std::ofstream mf("aurora.pkg");
    mf << "name: " << info.name << "\nversion: " << info.version << "\ndescription: " << info.description << "\nentry: " << info.entry << "\ndependencies: ";
    for (size_t i = 0; i < info.dependencies.size(); i++) { if (i > 0) mf << ", "; mf << info.dependencies[i]; }
    mf << "\n"; std::cout << "removed " << name << "\n"; return 0;
}

int cmd_build(bool verbose, bool auto_yes) {
    (void)auto_yes;
    PackageInfo info = read_manifest(".");
    if (info.name.empty()) { std::cerr << "error: no aurora.pkg\n"; return 1; }
    if (!fs::exists("build")) fs::create_directories("build");
    std::vector<std::string> c = {"aurorac", info.entry, "-o", "build/" + info.name};
    if (verbose) c.push_back("-v");
    return run_cmd(c);
}

int cmd_run(bool verbose, bool auto_yes) {
    int rc = cmd_build(verbose, auto_yes);
    if (rc != 0) return rc;
    PackageInfo info = read_manifest(".");
    return run_cmd({"build/" + info.name});
}

int cmd_test(bool verbose) { return cmd_build(verbose, false); }

int cmd_clean() { if (fs::exists("build")) fs::remove_all("build"); std::cout << "cleaned\n"; return 0; }

int cmd_info() {
    PackageInfo info = read_manifest(".");
    if (info.name.empty()) { std::cerr << "no project\n"; return 1; }
    std::cout << "name: " << info.name << "\nversion: " << info.version << "\n";
    if (!info.description.empty()) std::cout << "description: " << info.description << "\n";
    if (!info.author.empty()) std::cout << "author: " << info.author << "\n";
    if (!info.dependencies.empty()) { std::cout << "dependencies (" << info.dependencies.size() << "):\n"; for (auto& d : info.dependencies) std::cout << "  " << d << "\n"; }
    return 0;
}

int cmd_list() {
    PackageInfo info = read_manifest(".");
    if (info.name.empty()) { std::cerr << "no project\n"; return 1; }
    std::cout << info.name << "@" << info.version << "\n";
    LockData lf = read_lockfile();
    for (auto& d : info.dependencies) {
        std::string name, ver; parse_pkg_spec(d, name, ver);
        auto it = lf.packages.find(name);
        std::cout << "  " << name << "@" << ((it != lf.packages.end()) ? it->second.version : ver) << "\n";
    }
    return 0;
}

/* ── Help ── */
int cmd_help() {
    std::cout << "voss v" << AURA_VERSION << " - Aurora Package Manager\n";
    std::cout << "Usage: voss <command> [options]\n";
    std::cout << "\nCore:\n  init <name>         Create a new package\n  install <pkg>       Add dependency (@version, or multiple for parallel)\n  install user/repo   Install from GitHub (shorthand, @tag or #branch)\n  uninstall <pkg>     Remove dependency\n  lock                Resolve + lock all deps (incl. transitive)\n  build               Compile + link (enforces permissions)\n  run                 Build + execute\n  test                Build + run tests/\n";
    std::cout << "\nAnalysis:\n  tree                Show dep tree with version conflict detection\n  verify              Check for circular deps\n  audit               Scan locked packages for known vulnerabilities\n  recommend <pkg>     AI advisor: suggest similar/related packages\n  health              Dependency health score (0-100)\n  snapshot [label]    Save project snapshot\n  snapshots           List available snapshots\n  restore <snap>      Time-travel: restore from snapshot\n  sandbox <pkg>       Run a package in isolated sandbox\n  bench <pkg>...      Benchmark: compare build/run performance\n  dead                Detect unused dependencies\n  import <npm|pip|cargo>  Import deps from other package managers\n  detect              Detect project types in current directory\n  why <pkg>           Show why a package is installed (dep chain)\n  migrate <pkg>@<ver> Apply migration rules for version upgrade\n  trust <pkg>         Show trust score for a package\n  export <dir>        Export project snapshot for sharing\n  clone <dir>         Import/restore project from export\n";
    std::cout << "\nSecurity:\n  perms               List permissions (groups, policy)\n  perms allow <perm>  Permanently allow a permission\n  perms deny <perm>   Permanently deny a permission\n  perms reset         Clear permission policy\n  perms review        Audit perms across all deps\n";
    std::cout << "\nCache:\n  cache               Show cache info (SHA-256 content-addressed)\n  cache clean         Clear cache\n";
    std::cout << "\nWorkspace:\n  ws init <pkg>...    Init workspace packages\n  ws list             List workspace packages\n  ws build            Build all workspace packages\n";
    std::cout << "\nRoadmap:\n  doctor              Project diagnostics + auto-fix\n  update              Update all dependencies to latest\n  search <query>      Search packages (DB + installed + registries)\n  publish             Publish package locally\n  publish github:user/repo  Publish to GitHub (voss.json, push, release)\n  repair              Self-healing: fix cache, lockfile, deps\n  graph               Visualize dependency graph\n  freeze              Pin all dependency versions\n  unfreeze            Unpin dependencies\n  outdated            Check for outdated packages\n  license             Scan license compatibility\n  suggest <pkg>       Smart package suggestions\n  simulate <pkg>...   Dry-run install (no changes)\n  mirror <create|update|status>  Offline registry mirror\n  audit --fix         Auto-resolve lock file conflicts\n  dedupe              Find duplicate dependency versions\n";
    std::cout << "\nInfo:\n  info | list | clean | help\n  registry            Manage package registry mirrors\n";
    std::cout << "\nNext-Gen (Tier 3-4):\n  ai-scan <pkg>@<v>   AI Compatibility Scanner: detect breaking changes\n  forecast [pkg]      Dependency Forecast: predict abandonment & risk\n  lts list|add|rm|switch  LTS channel management\n  cloud-build         Reproducible cloud builds (init/run/status/config)\n  binary              Binary package distribution (package/install/list)\n  changelog <pkg>     Built-in changelog analysis\n  risk-forecast [pkg] Security risk forecasting\n  lifecycle [pkg]     Package lifecycle monitor\n  fork-recover <o> <f>  Package fork recovery\n  insurance           Dependency insurance (add/check/list)\n  eco-dashboard       Ecosystem health dashboard\n  telemetry           Opt-in telemetry network (enable/disable/submit)\n  ai-gen <desc>       AI Package Generator: generate packages from description\n";
    std::cout << "\nOptions:\n  -v, --verbose       Show build commands\n  -y, --yes           Auto-approve permissions\n  --offline           Only use cache + lock file (no stubs)\n";
    return 0;
}
