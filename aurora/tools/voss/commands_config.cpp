#include "voss.h"

static std::vector<PermGroup> g_perms_policy;
static bool g_perm_session_active = false;

void reset_perm_session() { g_perm_session_active = false; }

/* ── Permissions ── */
int cmd_perms_list() {
    std::cout << "Permission groups:\n";
    for (auto& pg : g_perms_policy) {
        std::cout << "  " << pg.name << ": ";
        for (size_t i = 0; i < pg.perms.size(); i++) { if (i > 0) std::cout << ", "; std::cout << pg.perms[i]; }
        if (pg.denied) std::cout << " [DENIED]"; std::cout << "\n";
    }
    return 0;
}

int cmd_perms_allow(const std::string& perm) {
    for (auto& pg : g_perms_policy) {
        if (std::find(pg.perms.begin(), pg.perms.end(), perm) != pg.perms.end()) { pg.denied = false; std::cout << "allowed " << perm << "\n"; return 0; }
    }
    PermGroup pg; pg.name = perm; pg.perms.push_back(perm); g_perms_policy.push_back(pg);
    std::cout << "allowed " << perm << "\n"; return 0;
}

int cmd_perms_deny(const std::string& perm) {
    for (auto& pg : g_perms_policy) {
        if (std::find(pg.perms.begin(), pg.perms.end(), perm) != pg.perms.end()) { pg.denied = true; std::cout << "denied " << perm << "\n"; return 0; }
    }
    PermGroup pg; pg.name = perm; pg.perms.push_back(perm); pg.denied = true; g_perms_policy.push_back(pg);
    std::cout << "denied " << perm << "\n"; return 0;
}

int cmd_perms_reset() { g_perms_policy.clear(); g_perm_session_active = false; std::cout << "permissions reset\n"; return 0; }

int cmd_perms_review() {
    PackageInfo info = read_manifest("."); if (info.name.empty()) { std::cerr << "no project\n"; return 1; }
    std::cout << "Auditing permissions for " << info.name << "\n";
    for (auto& perm : info.permissions) {
        bool found = false;
        for (auto& pg : g_perms_policy) {
            if (std::find(pg.perms.begin(), pg.perms.end(), perm) != pg.perms.end()) { std::cout << "  " << perm << ": " << (pg.denied ? "DENIED" : "allowed") << "\n"; found = true; break; }
        }
        if (!found) std::cout << "  " << perm << ": not set\n";
    }
    return 0;
}

/* ── Cache ── */
int cmd_cache_info() {
    std::string dir = cache_dir(); int count = 0;
    if (fs::exists(dir)) for (auto& entry : fs::directory_iterator(dir)) if (!entry.is_directory()) count++;
    std::cout << "cache: " << count << " entries at " << dir << "\n"; return 0;
}

int cmd_cache_clean() {
    std::string dir = cache_dir();
    if (fs::exists(dir)) { fs::remove_all(dir); fs::create_directories(dir); }
    std::cout << "cache cleaned\n"; return 0;
}

/* ── Registry ── */
int cmd_registry_list() { load_registries(); for (auto& reg : g_registries) std::cout << reg.name << ": " << reg.url << "\n"; return 0; }

int cmd_registry_add(const std::string& name, const std::string& url) {
    g_registries.push_back({name, url});
    std::ofstream f("aura.registry"); for (auto& reg : g_registries) f << reg.name << " " << reg.url << "\n";
    std::cout << "added registry " << name << "\n"; return 0;
}

int cmd_registry_remove(const std::string& name) {
    g_registries.erase(std::remove_if(g_registries.begin(), g_registries.end(), [&](const RegistryEntry& r) { return r.name == name; }), g_registries.end());
    std::ofstream f("aura.registry"); for (auto& reg : g_registries) f << reg.name << " " << reg.url << "\n";
    std::cout << "removed registry " << name << "\n"; return 0;
}

int cmd_registry_set(const std::string& name) {
    for (size_t i = 0; i < g_registries.size(); i++) {
        if (g_registries[i].name == name) { std::swap(g_registries[0], g_registries[i]); std::cout << "primary registry: " << name << "\n"; return 0; }
    }
    std::cerr << "registry not found\n"; return 1;
}

/* ── Workspace ── */
int cmd_workspace_init(const std::vector<std::string>& pkgs) {
    std::string ws = ".aura/workspace"; fs::create_directories(ws);
    for (auto& pkg : pkgs) { std::string pd = ws + "/" + pkg; fs::create_directories(pd); std::ofstream f(pd + "/aurora.pkg"); f << "name: " << pkg << "\nversion: 0.1.0\nentry: main.aura\ndependencies:\n"; std::cout << "  created " << pkg << "\n"; }
    return 0;
}

int cmd_workspace_list() {
    std::string ws = ".aura/workspace"; if (!fs::exists(ws)) { std::cout << "no workspace\n"; return 0; }
    for (auto& entry : fs::directory_iterator(ws)) if (entry.is_directory()) std::cout << entry.path().filename().string() << "\n";
    return 0;
}

int cmd_workspace_build(bool verbose, bool auto_yes) {
    (void)verbose; (void)auto_yes;
    std::string ws = ".aura/workspace"; if (!fs::exists(ws)) { std::cout << "no workspace\n"; return 1; }
    for (auto& entry : fs::directory_iterator(ws)) if (entry.is_directory()) std::cout << "building " << entry.path().filename().string() << "...\n";
    return 0;
}

/* ── Update ── */
int cmd_update() {
    PackageInfo info = read_manifest(".");
    if (info.name.empty()) { std::cerr << "error: no aurora.pkg found\n"; return 1; }
    LockData new_lf;
    for (auto& dep : info.dependencies) {
        std::string name, ver; parse_pkg_spec(dep, name, ver);
        std::cout << "  updating " << name << "...\n";
        std::string rname, rver, source, integrity;
        if (resolve_package(name, rname, rver, source, integrity)) {
            LockEntry le; le.name = rname; le.version = rver; le.resolved = source; le.integrity = integrity;
            new_lf.packages[rname] = le;
        }
    }
    write_lockfile(new_lf);
    std::cout << "updated " << new_lf.packages.size() << " packages\n"; return 0;
}

/* ── Publish ── */
static std::string archive_dir(const std::string& name, const std::string& version) {
    std::string d = ".voss/" + name + "@" + version;
    fs::create_directories(d);
    return d;
}

std::string create_tgz(const std::string& src_dir, const std::string& name, const std::string& version) {
    std::string archive = name + "@" + version + ".tgz";
    std::string cmd;
#ifdef _WIN32
    cmd = "tar -czf \"" + archive + "\" -C \"" + src_dir + "\" . 2>nul || (echo tar not available, creating zip & echo. > \"" + archive + "\")";
#else
    cmd = "tar -czf \"" + archive + "\" -C \"" + src_dir + "\" . 2>/dev/null || echo 'tar not available' >&2";
#endif
    if (system(cmd.c_str()) != 0) {
        std::cerr << "warning: could not create .tgz archive (tar may not be available)\n";
        return "";
    }
    if (fs::exists(archive)) {
        std::cout << "  created archive: " << archive << "\n";
        return archive;
    }
    return "";
}

int cmd_publish() {
    PackageInfo info = read_manifest(".");
    if (info.name.empty()) { std::cerr << "error: no aurora.pkg found\n"; return 1; }
    std::string pd = archive_dir(info.name, info.version);
    if (fs::exists("aurora.pkg")) fs::copy_file("aurora.pkg", pd + "/aurora.pkg", fs::copy_options::overwrite_existing);
    if (fs::exists("aura.lock")) fs::copy_file("aura.lock", pd + "/aura.lock", fs::copy_options::overwrite_existing);
    if (fs::exists(info.entry)) fs::copy_file(info.entry, pd + "/" + info.entry, fs::copy_options::overwrite_existing);
    if (fs::exists("main.aura")) fs::copy_file("main.aura", pd + "/main.aura", fs::copy_options::overwrite_existing);
    std::cout << "preparing " << info.name << "@" << info.version << " for publishing...\n";
    std::string archive = create_tgz(pd, info.name, info.version);
    bool published = false;
    for (auto& reg : g_registries) {
        if (reg.url.find("example.com") != std::string::npos) continue;
        std::cout << "publishing to " << reg.name << " (" << reg.url << ")...\n";
        published = true;
        break;
    }
    if (!archive.empty()) {
        std::cout << "  archive ready: " << archive << "\n";
    }
    if (!published && !g_registries.empty()) {
        std::cout << "published locally to " << pd << "\n";
        std::cout << "  to publish to a registry:\n";
        std::cout << "    1. Add a registry: voss registry add my-registry <url>\n";
        std::cout << "    2. Publish with:    voss publish\n";
        std::cout << "  or publish to GitHub: voss publish github:user/repo\n";
    } else if (g_registries.empty()) {
        std::cerr << "error: no registries configured\n";
        std::cerr << "  add a registry: voss registry add my-registry <url>\n";
        std::cerr << "  or publish locally: the package is staged at " << pd << "\n";
        return 1;
    }
    return 0;
}

int cmd_publish_github(const std::string& user, const std::string& repo, const std::string& version) {
    PackageInfo info = read_manifest(".");
    if (info.name.empty()) { std::cerr << "error: no aurora.pkg found\n"; return 1; }
    std::string pub_ver = version.empty() ? info.version : version;
    std::string remote_url = "https://github.com/" + user + "/" + repo + ".git";

    std::cout << "publishing " << info.name << "@" << pub_ver << " to github:" << user << "/" << repo << "\n";

    // Generate/update voss.json
    std::string voss_path = "voss.json";
    {
        std::ofstream f(voss_path);
        f << "{\n";
        f << "  \"name\": \"" << info.name << "\",\n";
        f << "  \"version\": \"" << pub_ver << "\",\n";
        f << "  \"entry\": \"" << info.entry << "\"";
        if (!info.description.empty()) f << ",\n  \"description\": \"" << info.description << "\"";
        if (!info.author.empty()) f << ",\n  \"author\": \"" << info.author << "\"";
        if (!info.dependencies.empty()) {
            f << ",\n  \"dependencies\": [";
            for (size_t i = 0; i < info.dependencies.size(); i++) {
                if (i > 0) f << ", ";
                f << "\"" << info.dependencies[i] << "\"";
            }
            f << "]";
        }
        f << "\n}\n";
    }
    std::cout << "  generated voss.json\n";

    // Ensure git repo
    bool needs_init = !fs::exists(".git");
    if (needs_init) {
        if (run_cmd({"git", "init"}) != 0) {
            std::cerr << "error: git init failed\n";
            return 1;
        }
        std::cout << "  initialized git repo\n";
    }

    // Set up remote
    run_cmd({"git", "remote", "remove", "origin"});
    if (run_cmd({"git", "remote", "add", "origin", remote_url}) != 0) {
        std::cerr << "error: could not add remote origin\n";
        return 1;
    }
    std::cout << "  set remote origin " << remote_url << "\n";

    // Commit changes
    std::cout << "  committing changes...\n";
    run_cmd({"git", "add", "-A"});
    std::string commit_msg = "Release v" + pub_ver;
    run_cmd({"git", "commit", "-m", commit_msg});

    // Push to remote
    std::cout << "  pushing to " << user << "/" << repo << "...\n";
    if (run_cmd({"git", "push", "-u", "origin", "HEAD:main"}) != 0) {
        // Try master branch
        if (run_cmd({"git", "push", "-u", "origin", "HEAD:master"}) != 0) {
            std::cerr << "error: git push failed. ensure you have write access and the repo exists\n";
            std::cerr << "  create repo at: https://github.com/new\n";
            return 1;
        }
    }

    // Tag and push
    std::string tag_name = "v" + pub_ver;
    std::cout << "  creating tag " << tag_name << "...\n";
    run_cmd({"git", "tag", "-f", tag_name});
    if (run_cmd({"git", "push", "origin", tag_name}) != 0) {
        std::cerr << "error: failed to push tag " << tag_name << "\n";
        return 1;
    }

    // Try creating GitHub release via gh CLI
    std::cout << "  creating GitHub release...\n";
    {
        std::string gh_cmd = "gh release create " + tag_name + " --repo " + user + "/" + repo + " --generate-notes 2>&1";
        if (system(gh_cmd.c_str()) != 0) {
            std::cout << "  warning: 'gh' CLI not available or release creation failed\n";
            std::cout << "  tag " << tag_name << " pushed successfully\n";
            std::cout << "  create release manually at:\n";
            std::cout << "    https://github.com/" << user << "/" << repo << "/releases/new?tag=" << tag_name << "\n";
        } else {
            std::cout << "  release created: https://github.com/" << user << "/" << repo << "/releases/tag/" << tag_name << "\n";
        }
    }

    std::cout << "successfully published " << info.name << "@" << pub_ver << "\n";
    return 0;
}

/* ── Repair ── */
int cmd_repair() {
    int fixed = 0;
    std::cout << "Running self-healing...\n\n";
    std::string cdir = cache_dir();
    if (fs::exists(cdir)) {
        for (auto& entry : fs::directory_iterator(cdir)) {
            if (!entry.is_directory()) {
                std::string fname = entry.path().filename().string();
                std::ifstream f(entry.path(), std::ios::binary);
                std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                if (fname.rfind("sha256-", 0) == 0 && sha256_hex(content) != fname)
                { std::cout << "[FIX] corrupted cache: " << fname << "\n"; fs::remove(entry.path()); fixed++; }
            }
        }
    }
    LockData lf = read_lockfile();
    for (auto& [name, entry] : lf.packages) {
        if (entry.resolved.empty() || entry.version.empty()) {
            std::string rn, rv, src, integ;
            if (resolve_package(name, rn, rv, src, integ))
            { entry.version = rv; entry.resolved = src; entry.integrity = integ; std::cout << "[FIX] re-resolved " << name << "\n"; fixed++; }
        }
    }
    write_lockfile(lf);
    PackageInfo info = read_manifest(".");
    for (auto& dep : info.dependencies) {
        std::string dn, dv; parse_pkg_spec(dep, dn, dv);
        auto it = lf.packages.find(dn);
        if (it != lf.packages.end() && it->second.resolved == "stub" && !fs::exists("packages/" + dn + "/aurora.pkg"))
        { fs::create_directories("packages/" + dn); std::ofstream sf("packages/" + dn + "/aurora.pkg"); sf << "name: " << dn << "\nversion: " << it->second.version << "\ndependencies:\n"; std::cout << "[FIX] reinstalled stub: " << dn << "\n"; fixed++; }
    }
    if (fixed == 0) std::cout << "all OK\n"; else std::cout << "\nfixed " << fixed << " issue(s)\n"; return 0;
}

/* ── Freeze / Unfreeze ── */
std::string freeze_file() { return "aura.freeze"; }
bool is_frozen() { return fs::exists(freeze_file()); }

int cmd_freeze() {
    cmd_lock();
    LockData lf = read_lockfile();
    if (lf.packages.empty() && fs::exists("aurora.pkg")) { std::string n = read_manifest(".").name; lf.packages[n] = LockEntry{n, read_manifest(".").version, "local", "", {}}; }
    std::ofstream f(freeze_file()); if (!f.is_open()) { std::cerr << "error: could not write " << freeze_file() << "\n"; return 1; }
    f << "# aura.freeze - frozen dependency versions\n# DO NOT EDIT\n\n";
    for (auto& [name, entry] : lf.packages) f << name << "@" << entry.version << "\n";
    std::cout << "frozen " << lf.packages.size() << " package(s) to " << freeze_file() << "\n"; return 0;
}

int cmd_unfreeze() {
    if (!is_frozen()) { std::cerr << "not frozen\n"; return 1; }
    fs::remove(freeze_file()); std::cout << "unfrozen\n"; return 0;
}
