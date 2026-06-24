#include "voss.h"

int cmd_tree() {
    PackageInfo info = read_manifest(".");
    if (info.name.empty()) { std::cerr << "error: no aurora.pkg\n"; return 1; }
    DepNode root; root.name = info.name; root.version = info.version;
    std::set<std::string> visited;
    std::map<std::string, std::string> versions;
    LockData lf = read_lockfile();
    for (auto& dep : info.dependencies) {
        std::string dn, dv; parse_pkg_spec(dep, dn, dv);
        auto it = lf.packages.find(dn);
        if (it != lf.packages.end() && dv.empty()) dv = it->second.version;
        root.children.push_back(build_tree(dn, dv, visited, versions));
    }
    std::cout << root.name << "@" << root.version << "\n";
    for (size_t i = 0; i < root.children.size(); i++) print_tree(root.children[i], "", i == root.children.size() - 1);
    return 0;
}

int cmd_verify() {
    PackageInfo info = read_manifest(".");
    if (info.name.empty()) { std::cerr << "no project\n"; return 1; }
    LockData lf = read_lockfile();
    std::function<bool(const std::string&, std::set<std::string>&)> has_cycle;
    has_cycle = [&](const std::string& pkg, std::set<std::string>& path) -> bool {
        if (path.count(pkg)) return true;
        path.insert(pkg);
        auto it = lf.packages.find(pkg);
        if (it != lf.packages.end())
            for (auto& dep : it->second.dependencies)
            { std::string dn, _; parse_pkg_spec(dep, dn, _); if (has_cycle(dn, path)) return true; }
        path.erase(pkg); return false;
    };
    for (auto& dep : info.dependencies) {
        std::string dn, _; parse_pkg_spec(dep, dn, _);
        std::set<std::string> path;
        if (has_cycle(dn, path)) { std::cerr << "circular dependencies detected\n"; return 1; }
    }
    std::cout << "all clear\n"; return 0;
}

int cmd_audit() {
    LockData lf = read_lockfile();
    if (lf.packages.empty()) { std::cerr << "no lock file\n"; return 1; }
    int vulns = 0;
    for (auto& [name, entry] : lf.packages)
        for (auto& vuln : g_vuln_db)
            if (vuln.package == name && (vuln.version_range.empty() || version_in_range(entry.version, vuln.version_range)))
            { std::cout << "[" << vuln.severity << "] " << vuln.id << ": " << name << " " << entry.version << (!vuln.description.empty() ? " - " + vuln.description : "") << "\n"; vulns++; }
    if (vulns == 0) std::cout << "no vulnerabilities found\n";
    else std::cout << "\n" << vulns << " vulnerability(ies) found\n";
    return vulns > 0 ? 1 : 0;
}

int cmd_health() {
    int score = 100;
    PackageInfo info = read_manifest(".");
    if (info.name.empty()) { std::cerr << "no project\n"; return 1; }
    LockData lf = read_lockfile();
    if (lf.packages.empty()) score -= 20;
    if (info.permissions.empty()) score -= 10;
    for (auto& dep : info.dependencies) { std::string n, v; parse_pkg_spec(dep, n, v); if (v.empty()) score -= 5; }
    for (auto& [name, entry] : lf.packages)
        for (auto& vuln : g_vuln_db)
            if (vuln.package == name && (vuln.version_range.empty() || version_in_range(entry.version, vuln.version_range))) score -= 15;
    std::cout << "health score: " << std::max(0, score) << "/100\n";
    if (score < 50) std::cout << "needs attention\n"; else if (score < 80) std::cout << "fair\n"; else std::cout << "healthy\n";
    return score < 50 ? 1 : 0;
}

int cmd_snapshot(const std::string& label) {
    std::string snap = label.empty() ? "snapshot_" + std::to_string(time(nullptr)) : label;
    std::string dir = ".aura/snapshots/" + snap; fs::create_directories(dir);
    if (fs::exists("aurora.pkg")) fs::copy_file("aurora.pkg", dir + "/aurora.pkg", fs::copy_options::overwrite_existing);
    if (fs::exists("aura.lock")) fs::copy_file("aura.lock", dir + "/aura.lock", fs::copy_options::overwrite_existing);
    std::cout << "snapshot saved: " << snap << "\n"; return 0;
}

int cmd_snapshots() {
    std::string dir = ".aura/snapshots";
    if (!fs::exists(dir)) { std::cout << "no snapshots\n"; return 0; }
    for (auto& entry : fs::directory_iterator(dir))
        if (entry.is_directory()) std::cout << entry.path().filename().string() << "\n";
    return 0;
}

int cmd_restore(const std::string& snapshot) {
    std::string dir = ".aura/snapshots/" + snapshot;
    if (!fs::exists(dir)) { std::cerr << "snapshot not found\n"; return 1; }
    if (fs::exists(dir + "/aurora.pkg")) fs::copy_file(dir + "/aurora.pkg", "aurora.pkg", fs::copy_options::overwrite_existing);
    if (fs::exists(dir + "/aura.lock")) fs::copy_file(dir + "/aura.lock", "aura.lock", fs::copy_options::overwrite_existing);
    std::cout << "restored from snapshot: " << snapshot << "\n"; return 0;
}

int cmd_sandbox(const std::string& pkg, bool verbose) {
    std::string dir = ".aura/sandbox/" + pkg;
    fs::create_directories(dir);
    fs::create_directories(dir + "/packages");

    if (fs::exists("aurora.pkg"))
        fs::copy_file("aurora.pkg", dir + "/aurora.pkg", fs::copy_options::overwrite_existing);
    if (fs::exists("aura.lock"))
        fs::copy_file("aura.lock", dir + "/aura.lock", fs::copy_options::overwrite_existing);
    if (fs::exists("packages/" + pkg))
        fs::copy("packages/" + pkg, dir + "/packages/" + pkg,
                 fs::copy_options::recursive | fs::copy_options::overwrite_existing);

    /* Write sandbox policy file */
    std::ofstream pf(dir + "/sandbox.policy");
    pf << "package: " << pkg << "\n"
       << "network: isolated\n"
       << "filesystem: read-only\n"
       << "process: isolated\n"
       << "memory: 512MB\n"
       << "cpu: 1 core\n";

    std::cout << "sandbox ready at " << dir << "\n";

    if (!verbose) return 0;

    /* Print sandbox info */
    std::cout << "\nsandbox policy:\n";
    std::cout << "  network:     isolated (no external access)\n";
    std::cout << "  filesystem:  read-only (packages/)\n";
    std::cout << "  process:     isolated\n";
    std::cout << "  memory:      512 MB limit\n";
    std::cout << "  cpu:         1 core\n";
    std::cout << "\nto run in sandbox:\n";
    std::cout << "  cd " << dir << " && voss run\n";
    return 0;
}

int cmd_dead_deps() {
    PackageInfo info = read_manifest(".");
    if (info.name.empty()) { std::cerr << "no project\n"; return 1; }
    int dead = 0;
    for (auto& dep : info.dependencies) {
        std::string name, ver; parse_pkg_spec(dep, name, ver);
        bool used = false;
        if (fs::exists(info.entry)) {
            std::ifstream f(info.entry);
            std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            if (content.find(name) != std::string::npos) used = true;
        }
        if (!used) { std::cout << "  " << name << " (unused)\n"; dead++; }
    }
    if (dead == 0) std::cout << "no dead dependencies\n"; else std::cout << "\n" << dead << " dead dependenc(ies)\n";
    return dead > 0 ? 1 : 0;
}

int cmd_detect() {
    std::cout << "Detecting project types:\n";
    if (fs::exists("package.json")) std::cout << "  Node.js/npm\n";
    if (fs::exists("requirements.txt") || fs::exists("setup.py")) std::cout << "  Python\n";
    if (fs::exists("Cargo.toml")) std::cout << "  Rust/Cargo\n";
    if (fs::exists("aurora.pkg")) std::cout << "  Aurora/VOSS\n";
    if (fs::exists("CMakeLists.txt")) std::cout << "  CMake\n"; return 0;
}

int cmd_trust(const std::string& pkg) {
    int score = 50;
    LockData lf = read_lockfile();
    auto it = lf.packages.find(pkg);
    if (it != lf.packages.end()) score += 10;
    if (fs::exists("packages/" + pkg)) score += 10;
    for (auto& vuln : g_vuln_db) if (vuln.package == pkg) score -= 20;
    std::cout << pkg << " trust score: " << std::max(0, std::min(100, score)) << "/100\n";
    return score < 50 ? 1 : 0;
}

int cmd_migrate(const std::string& spec) {
    PackageInfo info = read_manifest(".");
    std::string name, new_ver; parse_pkg_spec(spec, name, new_ver);
    for (auto& dep : info.dependencies) {
        std::string dn, dv; parse_pkg_spec(dep, dn, dv);
        if (dn == name) {
            dep = name + "@" + new_ver;
            std::ofstream mf("aurora.pkg");
            mf << "name: " << info.name << "\nversion: " << info.version << "\ndescription: " << info.description << "\nentry: " << info.entry << "\ndependencies: ";
            for (size_t i = 0; i < info.dependencies.size(); i++) { if (i > 0) mf << ", "; mf << info.dependencies[i]; }
            mf << "\n"; std::cout << "migrated " << name << " to " << new_ver << "\n"; return 0;
        }
    }
    std::cerr << name << " not found in dependencies\n"; return 1;
}

int cmd_why(const std::string& pkg) {
    PackageInfo info = read_manifest("."); LockData lf = read_lockfile();
    for (auto& dep : info.dependencies) { std::string dn, _; parse_pkg_spec(dep, dn, _); if (dn == pkg) { std::cout << pkg << " is a direct dependency\n"; return 0; } }
    std::function<bool(const std::string&, std::set<std::string>&)> find_chain;
    find_chain = [&](const std::string& target, std::set<std::string>& visited) -> bool {
        for (auto& [n, entry] : lf.packages) {
            if (visited.count(n)) continue;
            for (auto& dep : entry.dependencies) { std::string dn, _; parse_pkg_spec(dep, dn, _); if (dn == target) { std::cout << pkg << " is required by " << n << "\n"; return true; } }
            visited.insert(n); if (find_chain(target, visited)) return true;
        }
        return false;
    };
    std::set<std::string> visited;
    if (!find_chain(pkg, visited)) std::cout << pkg << " not found in dependency tree\n";
    return 0;
}

int cmd_export(const std::string& dir) {
    fs::create_directories(dir);
    if (fs::exists("aurora.pkg")) fs::copy_file("aurora.pkg", dir + "/aurora.pkg", fs::copy_options::overwrite_existing);
    if (fs::exists("aura.lock")) fs::copy_file("aura.lock", dir + "/aura.lock", fs::copy_options::overwrite_existing);
    std::cout << "exported to " << dir << "\n"; return 0;
}

int cmd_import_project(const std::string& dir) {
    if (!fs::exists(dir + "/aurora.pkg")) { std::cerr << "no project in " << dir << "\n"; return 1; }
    fs::copy_file(dir + "/aurora.pkg", "aurora.pkg", fs::copy_options::overwrite_existing);
    if (fs::exists(dir + "/aura.lock")) fs::copy_file(dir + "/aura.lock", "aura.lock", fs::copy_options::overwrite_existing);
    std::cout << "imported from " << dir << "\n"; return 0;
}

int cmd_import_deps(const std::vector<std::string>& fmts) {
    for (auto& fmt : fmts) {
        if (fmt == "npm") {
            std::ifstream f("package.json"); if (!f.is_open()) { std::cerr << "no package.json\n"; continue; }
            std::string json((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            size_t ds = json.find("\"dependencies\"");
            if (ds != std::string::npos) {
                size_t obj_s = json.find('{', json.find(':', ds));
                size_t obj_e = json.find('}', obj_s);
                if (obj_s != std::string::npos && obj_e != std::string::npos) {
                    std::string sec = json.substr(obj_s, obj_e - obj_s + 1); size_t p = 1;
                    while (p < sec.size()) {
                        size_t q1 = sec.find('"', p); if (q1 == std::string::npos) break;
                        size_t q2 = sec.find('"', q1 + 1); if (q2 == std::string::npos) break;
                        std::string dn = sec.substr(q1 + 1, q2 - q1 - 1);
                        size_t q3 = sec.find('"', sec.find(':', q2));
                        size_t q4 = sec.find('"', q3 + 1);
                        std::string dv = (q3 != std::string::npos && q4 != std::string::npos) ? sec.substr(q3 + 1, q4 - q3 - 1) : "*";
                        cmd_install(dn + "@" + dv); p = (q4 != std::string::npos) ? q4 + 1 : q2 + 1;
                    }
                }
            }
        } else if (fmt == "pip") {
            std::ifstream f("requirements.txt"); if (!f.is_open()) { std::cerr << "no requirements.txt\n"; continue; }
            std::string line;
            while (std::getline(f, line)) {
                std::string t = trim(line); if (t.empty() || t[0] == '#') continue;
                size_t eq = t.find("=="); std::string dn = (eq != std::string::npos) ? trim(t.substr(0, eq)) : t;
                std::string dv = (eq != std::string::npos) ? trim(t.substr(eq + 2)) : "";
                cmd_install(dv.empty() ? dn : dn + "@" + dv);
            }
        } else if (fmt == "cargo") {
            std::ifstream f("Cargo.toml"); if (!f.is_open()) { std::cerr << "no Cargo.toml\n"; continue; }
            std::string line; bool in_deps = false;
            while (std::getline(f, line)) {
                std::string t = trim(line);
                if (t.rfind("[dependencies]", 0) == 0) { in_deps = true; continue; }
                if (t.rfind("[", 0) == 0 && t != "[dependencies]") in_deps = false;
                if (in_deps && !t.empty()) {
                    size_t eq = t.find('=');
                    if (eq != std::string::npos) {
                        std::string dn = trim(t.substr(0, eq));
                        std::string dv = trim(t.substr(eq + 1));
                        if (dv.size() >= 2 && dv[0] == '"') dv = dv.substr(1, dv.size() - 2);
                        cmd_install(trim(dv).empty() ? dn : dn + "@" + trim(dv));
                    }
                }
            }
        }
    }
    std::cout << "import complete\n"; return 0;
}

int cmd_bench(const std::vector<std::string>& pkgs) {
    PackageInfo info = read_manifest(".");
    if (pkgs.empty() && info.name.empty()) {
        std::cerr << "error: no target specified\n";
        return 1;
    }

    auto run_single = [](const std::string& label, const std::string& entry, const std::string& out) -> int {
        std::vector<std::string> c = {"aurorac", entry, "-o", out};
        int rc = run_cmd(c);
        if (rc != 0) {
            std::cout << label << ": COMPILE FAIL\n";
            return 1;
        }

        const int ITERATIONS = 10;
        long long total_ns = 0;
        for (int i = 0; i < ITERATIONS; i++) {
            auto start = std::chrono::high_resolution_clock::now();
            run_cmd({out});
            auto end = std::chrono::high_resolution_clock::now();
            total_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        }

        long long avg_ns = total_ns / ITERATIONS;
        double avg_ms = avg_ns / 1000000.0;
        std::cout << label << ": " << avg_ms << " ms avg (" << ITERATIONS << " runs)\n";
        return 0;
    };

    std::cout << "=== voss bench ===\n\n";
    int failed = 0;

    if (!pkgs.empty()) {
        for (auto& pkg : pkgs) {
            std::string name, ver;
            parse_pkg_spec(pkg, name, ver);
            std::string pkg_dir = "packages/" + name;
            std::string entry = pkg_dir + "/main.aura";
            if (fs::exists(entry)) {
                if (run_single(name, entry, "build/bench_" + name) != 0)
                    failed++;
            } else {
                std::cout << name << ": not installed\n";
                failed++;
            }
        }
    } else if (fs::exists(info.entry)) {
        if (run_single(info.name, info.entry, "build/bench_" + info.name) != 0)
            failed++;
    } else {
        std::cerr << "error: entry file not found\n";
        return 1;
    }

    return failed > 0 ? 1 : 0;
}

int cmd_recommend(const std::string& pkg) {
    auto recs = rec_find(pkg);
    if (recs.empty()) { std::cout << "no recommendations for " << pkg << "\n"; return 1; }
    std::cout << "Recommendations for " << pkg << ":\n";
    for (auto& r : recs) {
        std::string desc;
        for (auto& e : g_rec_db) if (e.name == r) desc = e.desc;
        std::cout << "  " << r; if (!desc.empty()) std::cout << "  " << desc; std::cout << "\n";
    }
    return 0;
}

/* ── Graph ── */
void graph_print(const DepNode& node, const std::string& prefix, bool is_last) {
    std::cout << prefix << (is_last ? "└── " : "├── ") << node.name;
    if (!node.version.empty()) std::cout << "@" << node.version;
    if (node.conflict) std::cout << " [CONFLICT: " << node.conflict_msg << "]";
    std::cout << "\n";
    std::string cp = prefix + (is_last ? "    " : "│   ");
    for (size_t i = 0; i < node.children.size(); i++) graph_print(node.children[i], cp, i == node.children.size() - 1);
}

int cmd_graph() {
    PackageInfo info = read_manifest(".");
    if (info.name.empty()) { std::cerr << "error: no aurora.pkg found\n"; return 1; }
    DepNode root; root.name = info.name; root.version = info.version;
    std::set<std::string> visited; std::map<std::string, std::string> versions;
    LockData lf = read_lockfile();
    for (auto& dep : info.dependencies) {
        std::string dn, dv; parse_pkg_spec(dep, dn, dv);
        auto it = lf.packages.find(dn); if (it != lf.packages.end() && dv.empty()) dv = it->second.version;
        root.children.push_back(build_tree(dn, dv, visited, versions));
    }
    std::cout << root.name << "@" << root.version << "\n";
    for (size_t i = 0; i < root.children.size(); i++) graph_print(root.children[i], "", i == root.children.size() - 1);
    return 0;
}

/* ── Outdated ── */
int cmd_outdated() {
    PackageInfo info = read_manifest(".");
    if (info.name.empty()) { std::cerr << "error: no aurora.pkg found\n"; return 1; }
    if (info.dependencies.empty()) { std::cout << "no dependencies\n"; return 0; }
    int outdated = 0;
    for (auto& dep : info.dependencies) {
        std::string name, cur_ver; parse_pkg_spec(dep, name, cur_ver);
        if (cur_ver.empty()) { std::cout << "  " << name << "  (unpinned)\n"; continue; }
        LockData lf = read_lockfile();
        auto lit = lf.packages.find(name);
        std::string locked_ver = (lit != lf.packages.end()) ? lit->second.version : "";
        std::string latest = locked_ver;
        for (auto& reg : g_registries) {
            std::string raw = http_fetch(reg.url + "/packages/" + name + "/latest");
            if (!raw.empty()) { std::string json = extract_json_source(raw); if (!json.empty()) { size_t vq = json.find('"'); if (vq != std::string::npos) { size_t vq2 = json.find('"', vq + 1); if (vq2 != std::string::npos) latest = json.substr(vq + 1, vq2 - vq - 1); } } break; }
        }
        if (!latest.empty() && latest != cur_ver) {
            std::cout << "  " << name << "  " << cur_ver << " -> " << latest; if (!locked_ver.empty() && locked_ver != cur_ver) std::cout << " (locked: " << locked_ver << ")"; std::cout << "\n"; outdated++;
        } else std::cout << "  " << name << "  " << cur_ver << " (latest)\n";
    }
    if (outdated == 0) std::cout << "all dependencies up-to-date\n"; else std::cout << "\n" << outdated << " outdated (run 'voss update')\n";
    return outdated > 0 ? 1 : 0;
}

/* ── License ── */
static const std::vector<std::string> PERMISSIVE_LICENSES = {"MIT", "Apache-2.0", "BSD-2-Clause", "BSD-3-Clause", "ISC", "Unlicense", "CC0-1.0"};
static const std::vector<std::string> WEAK_COPYLEFT = {"LGPL-2.1", "LGPL-3.0", "MPL-2.0"};
static const std::vector<std::string> STRONG_COPYLEFT = {"GPL-2.0", "GPL-3.0", "AGPL-3.0"};

static std::string license_category(const std::string& lic) {
    for (auto& l : PERMISSIVE_LICENSES) if (l == lic) return "permissive";
    for (auto& l : WEAK_COPYLEFT) if (l == lic) return "weak-copyleft";
    for (auto& l : STRONG_COPYLEFT) if (l == lic) return "strong-copyleft";
    return "unknown";
}

static bool license_compatible(const std::string& project_license, const std::string& dep_license) {
    if (project_license.empty() || dep_license.empty()) return true;
    std::string pc = license_category(project_license), dc = license_category(dep_license);
    if (pc == "strong-copyleft") return true;
    if (pc == "weak-copyleft" && dc == "strong-copyleft") return false;
    if (pc == "permissive" && dc != "permissive") return false;
    return true;
}

int cmd_license() {
    PackageInfo info = read_manifest(".");
    if (info.name.empty()) { std::cerr << "error: no aurora.pkg found\n"; return 1; }
    std::string project_license;
    { std::ifstream f("aurora.pkg"); std::string line; while (std::getline(f, line)) { std::string t = trim(line); if (t.rfind("license:", 0) == 0) project_license = trim(t.substr(8)); } }
    if (project_license.empty()) { std::cout << "no license declared in aurora.pkg (add 'license: MIT')\n"; project_license = "unknown"; }
    else std::cout << "Project license: " << project_license << " (" << license_category(project_license) << ")\n";
    int issues = 0;
    for (auto& dep : info.dependencies) {
        std::string dn, dv; parse_pkg_spec(dep, dn, dv);
        std::string dep_license;
        { std::ifstream f("packages/" + dn + "/aurora.pkg"); std::string line; while (std::getline(f, line)) { std::string t = trim(line); if (t.rfind("license:", 0) == 0) dep_license = trim(t.substr(8)); } }
        std::string dc = dep_license.empty() ? "unknown" : dep_license;
        std::string status;
        if (dep_license.empty()) { status = " [UNKNOWN]"; issues++; }
        else if (!license_compatible(project_license, dep_license)) { status = " [INCOMPATIBLE]"; issues++; }
        else status = " [" + license_category(dep_license) + "]";
        std::cout << "  " << dn << " " << dv << " - " << dc << status << "\n";
    }
    if (issues == 0) std::cout << "\nall licenses compatible\n"; else std::cerr << "\n" << issues << " license issue(s) found\n";
    return issues > 0 ? 1 : 0;
}

/* ── Suggest ── */
int cmd_suggest(const std::string& pkg_name) {
    bool found = false;
    for (auto& e : g_rec_db) {
        if (e.name == pkg_name) {
            found = true;
            if (!e.also.empty()) { std::cout << "Alternatives to '" << pkg_name << "':\n"; for (auto& alt : e.also) { std::string d; for (auto& e2 : g_rec_db) if (e2.name == alt) d = e2.desc; std::cout << "  " << alt; if (!d.empty()) std::cout << "  " << d; std::cout << "\n"; } }
            else std::cout << "No alternatives known for '" << pkg_name << "'\n";
            break;
        }
    }
    if (!found) {
        for (auto& vuln : g_vuln_db) {
            if (vuln.package == pkg_name) {
                std::cout << "Warning: '" << pkg_name << "' has known vulnerabilities:\n  " << vuln.id << ": " << vuln.description << " (" << vuln.severity << ")\n";
                auto recs = rec_find(pkg_name); if (!recs.empty()) { std::cout << "Consider: "; for (size_t i = 0; i < recs.size() && i < 3; i++) { if (i > 0) std::cout << ", "; std::cout << recs[i]; } std::cout << "\n"; }
                found = true; break;
            }
        }
    }
    if (!found) {
        auto similar = rec_find(pkg_name);
        if (!similar.empty()) { std::cout << "Did you mean: "; for (size_t i = 0; i < similar.size() && i < 5; i++) { if (i > 0) std::cout << ", "; std::cout << similar[i]; } std::cout << "?\n"; found = true; }
    }
    if (!found) std::cout << "no suggestions for '" << pkg_name << "'\n";
    return found ? 0 : 1;
}

/* ── Simulate ── */
int cmd_simulate(const std::vector<std::string>& packages) {
    PackageInfo info = read_manifest(".");
    if (packages.empty()) { std::cerr << "usage: voss simulate <pkg1> [<pkg2> ...]\n"; return 1; }
    std::cout << "Simulating install for " << packages.size() << " package(s):\n\n";
    int resolved = 0, failed = 0;
    for (auto& spec : packages) {
        std::string name, version; parse_pkg_spec(spec, name, version);
        bool exists = false;
        for (auto& dep : info.dependencies) { std::string dn, dv; parse_pkg_spec(dep, dn, dv); if (dn == name) { exists = true; break; } }
        if (exists) { std::cout << "  [SKIP] " << spec << " (already installed)\n"; continue; }
        std::string rname, rver, source, integ;
        if (!resolve_package(spec, rname, rver, source, integ)) { std::cout << "  [FAIL] " << spec << " (could not resolve)\n"; failed++; continue; }
        std::cout << "  [OK]   " << rname << "@" << rver;
        if (source.rfind("registry:", 0) == 0) std::cout << " (from " << source << ")"; else if (source.rfind("file:", 0) == 0) std::cout << " (local)"; else if (source.rfind("cache:", 0) == 0) std::cout << " (cached)"; else std::cout << " (" << source << ")";
        std::cout << "\n";
        if (fs::exists("packages/" + rname + "/aurora.pkg")) { PackageInfo sub = read_manifest("packages/" + rname); if (!sub.dependencies.empty()) std::cout << "       transitive: " << sub.dependencies.size() << " dep(s)\n"; }
        resolved++;
    }
    std::cout << "\n" << resolved << " would be installed, " << failed << " failed\n(dry run - no changes made)\n";
    return failed > 0 ? 1 : 0;
}

/* ── Mirror ── */
std::string mirror_dir() { std::string m = cache_dir() + "/mirror"; fs::create_directories(m); return m; }

int cmd_mirror_create() {
    std::string mdir = mirror_dir(); std::cout << "mirror directory: " << mdir << "\n"; int mirrored = 0;
    for (auto& reg : g_registries) {
        std::string raw = http_fetch(reg.url + "/packages");
        if (raw.empty()) { std::cerr << "warning: could not fetch from '" << reg.name << "'\n"; continue; }
        std::string json = extract_json_source(raw); if (json.empty()) continue;
        size_t pos = 0;
        while (pos < json.size()) {
            size_t q1 = json.find('"', pos); if (q1 == std::string::npos) break;
            size_t q2 = json.find('"', q1 + 1); if (q2 == std::string::npos) break;
            std::string pn = json.substr(q1 + 1, q2 - q1 - 1); pos = q2 + 1;
            std::string pr = http_fetch(reg.url + "/packages/" + pn + "/latest");
            if (!pr.empty()) { std::string pc = extract_json_source(pr); if (!pc.empty()) { std::string pd = mdir + "/" + pn; fs::create_directories(pd); std::ofstream of(pd + "/main.aura"); of << pc; mirrored++; } }
        }
    }
    if (mirrored > 0) std::cout << "mirrored " << mirrored << " package(s) to '" << mdir << "'\n"; else std::cerr << "no packages mirrored\n";
    return mirrored > 0 ? 0 : 1;
}

int cmd_mirror_update() { return cmd_mirror_create(); }

int cmd_mirror_status() {
    std::string mdir = mirror_dir(); if (!fs::exists(mdir + "/INDEX")) { std::cout << "no mirror found (run 'voss mirror create')\n"; return 0; }
    int count = 0; std::ifstream idx(mdir + "/INDEX"); std::string line; while (std::getline(idx, line)) if (!line.empty()) count++;
    std::cout << "mirror: " << count << " packages at " << mdir << "\n"; return 0;
}

/* ── Audit --fix ── */
int cmd_audit_fix() {
    LockData lf = read_lockfile();
    if (lf.packages.empty()) { std::cerr << "no lock file (run 'voss lock' first)\n"; return 1; }
    int fixed = 0;
    for (auto& [name, entry] : lf.packages) {
        bool needs_fix = false;
        if (entry.resolved.rfind("file:", 0) == 0 && !fs::exists(entry.resolved.substr(5))) needs_fix = true;
        if (!entry.integrity.empty() && cache_get(entry.integrity).empty()) needs_fix = true;
        if (needs_fix) {
            std::string rn, rv, src, integ;
            if (resolve_package(name, rn, rv, src, integ)) { entry.version = rv; entry.resolved = src; entry.integrity = integ; fixed++; std::cout << "  [FIX] " << name << ": re-resolved\n"; }
        }
    }
    write_lockfile(lf);
    if (fixed > 0) std::cout << "fixed " << fixed << " entries\n"; else std::cout << "all entries valid\n";
    return 0;
}

/* ── Dedupe ── */
int cmd_dedupe() {
    PackageInfo info = read_manifest("."); LockData lf = read_lockfile();
    if (lf.packages.empty()) { std::cerr << "no lock file\n"; return 1; }
    std::map<std::string, std::vector<std::string>> dep_versions;
    for (auto& [name, entry] : lf.packages) dep_versions[name].push_back(entry.version);
    int duplicates = 0;
    for (auto& [name, versions] : dep_versions) {
        std::sort(versions.begin(), versions.end());
        versions.erase(std::unique(versions.begin(), versions.end()), versions.end());
        if (versions.size() > 1) { std::cout << "  " << name << ": "; for (size_t i = 0; i < versions.size(); i++) { if (i > 0) std::cout << ", "; std::cout << versions[i]; } std::cout << "\n"; duplicates++; }
    }
    if (duplicates == 0) std::cout << "no duplicate dependencies\n"; else { std::cout << "\n" << duplicates << " duplicate(s) found\n"; std::cout << "run 'voss update' to unify versions\n"; }
    return duplicates > 0 ? 1 : 0;
}

/* ── Doctor ── */
int cmd_doctor() {
    int issues = 0;
    std::cout << "VOSS Doctor - Project Diagnostics\n\n";
    PackageInfo info = read_manifest(".");
    if (info.name.empty()) { std::cout << "[ERR] no aurora.pkg found\n"; issues++; }
    else {
        std::cout << "[OK]  aurora.pkg: " << info.name << "@" << info.version << "\n";
        if (info.entry.empty()) { std::cout << "[ERR] no entry point defined\n"; issues++; }
        else if (!fs::exists(info.entry)) { std::cout << "[WARN] entry point '" << info.entry << "' not found\n"; issues++; }
        else std::cout << "[OK]  entry: " << info.entry << "\n";
    }
    LockData lf = read_lockfile();
    if (lf.packages.empty()) std::cout << "[WARN] no lock file (run 'voss lock')\n";
    else std::cout << "[OK]  lock file: " << lf.packages.size() << " packages\n";
    for (auto& [name, entry] : lf.packages) {
        if (entry.resolved == "stub") std::cout << "[WARN] " << name << " is a stub (may be incomplete)\n";
    }
    for (auto& vuln : g_vuln_db) {
        auto lit = lf.packages.find(vuln.package);
        if (lit != lf.packages.end() && (vuln.version_range.empty() || version_in_range(lit->second.version, vuln.version_range)))
        { std::cout << "[VULN] " << vuln.package << ": " << vuln.id << " (" << vuln.severity << ")\n"; issues++; }
    }
    if (is_frozen()) std::cout << "[INFO] dependencies are frozen (aura.freeze)\n";
    if (issues == 0) std::cout << "\nall clear! (" << info.dependencies.size() << " deps, " << lf.packages.size() << " locked)\n";
    else std::cout << "\n" << issues << " issue(s) found\n";
    return issues > 0 ? 1 : 0;
}
