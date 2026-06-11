#include "voss.h"

PackageInfo read_manifest(const std::string& path) {
    PackageInfo info;
    std::ifstream f(path + "/aurora.pkg");
    if (!f.is_open()) return info;
    std::string line;
    while (std::getline(f, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        size_t colon = t.find(':');
        if (colon == std::string::npos) continue;
        std::string key = trim(t.substr(0, colon));
        std::string val = trim(t.substr(colon + 1));
        if (key == "name") info.name = val;
        else if (key == "version") info.version = val;
        else if (key == "author") info.author = val;
        else if (key == "description") info.description = val;
        else if (key == "entry") info.entry = val;
        else if (key == "dependencies" || key == "deps") {
            if (val.find(',') != std::string::npos) {
                auto parts = split(val, ',');
                info.dependencies.insert(info.dependencies.end(), parts.begin(), parts.end());
            } else if (!val.empty()) info.dependencies.push_back(val);
        } else if (key == "permissions" || key == "perms") {
            if (val.find(',') != std::string::npos) {
                auto parts = split(val, ',');
                info.permissions.insert(info.permissions.end(), parts.begin(), parts.end());
            } else if (!val.empty()) info.permissions.push_back(val);
        }
    }
    return info;
}

LockData read_lockfile() {
    LockData lf;
    std::ifstream f("aura.lock");
    if (!f.is_open()) return lf;
    std::string line;
    LockEntry current;
    bool in_pkg = false;
    std::string pkg_name;
    while (std::getline(f, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        if (t.rfind("version:", 0) == 0) lf.version = std::stoi(trim(t.substr(8)));
        else if (t.rfind("packages:", 0) == 0) continue;
        else if (t[0] == ' ' || t[0] == '\t') {
            size_t colon = t.find(':');
            if (colon == std::string::npos) continue;
            std::string key = trim(t.substr(0, colon));
            std::string val = trim(t.substr(colon + 1));
            if (key == "version") current.version = val;
            else if (key == "resolved") current.resolved = val;
            else if (key == "integrity") current.integrity = val;
            else if (key == "dependencies") {
                if (val.find(',') != std::string::npos) {
                    auto parts = split(val, ',');
                    current.dependencies.insert(current.dependencies.end(), parts.begin(), parts.end());
                } else if (!val.empty()) current.dependencies.push_back(val);
            }
        } else {
            if (in_pkg && !pkg_name.empty()) lf.packages[pkg_name] = current;
            size_t colon = t.find(':');
            if (colon != std::string::npos) {
                pkg_name = trim(t.substr(0, colon));
                in_pkg = true;
                current = LockEntry();
                current.name = pkg_name;
                std::string rest = trim(t.substr(colon + 1));
                if (!rest.empty()) current.version = rest;
            }
        }
    }
    if (in_pkg && !pkg_name.empty()) lf.packages[pkg_name] = current;
    return lf;
}

void write_lockfile(const LockData& lf) {
    std::ofstream f("aura.lock");
    f << "version: " << lf.version << "\npackages:\n";
    for (auto& [name, entry] : lf.packages) {
        f << "  " << name << ":\n    version: " << entry.version << "\n    resolved: " << entry.resolved << "\n    integrity: " << entry.integrity << "\n";
        if (!entry.dependencies.empty()) {
            f << "    dependencies: ";
            for (size_t i = 0; i < entry.dependencies.size(); i++) {
                if (i > 0) f << ", ";
                f << entry.dependencies[i];
            }
            f << "\n";
        }
    }
}

std::string cache_dir() {
#ifdef _WIN32
    const char* local = getenv("LOCALAPPDATA");
    std::string dir = (local ? std::string(local) : std::string(getenv("USERPROFILE") ? getenv("USERPROFILE") : "") + "/.cache") + "/aura/cache";
#else
    const char* home = getenv("HOME");
    std::string dir = (home ? std::string(home) : "/tmp") + "/.aura/cache";
#endif
    fs::create_directories(dir);
    return dir;
}

std::string cache_get(const std::string& key) {
    return cache_get_ttl(key, 0);
}

std::string cache_get_ttl(const std::string& key, int max_age_seconds) {
    std::string path = cache_dir() + "/" + key;
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return "";
    /* Read timestamp (first line) */
    std::string ts_str;
    std::getline(f, ts_str);
    if (max_age_seconds > 0 && !ts_str.empty()) {
        char* end = nullptr;
        long long stored_ts = std::strtoll(ts_str.c_str(), &end, 10);
        if (end != ts_str.c_str()) {
            auto now = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            if (now - stored_ts > max_age_seconds) {
                f.close();
                fs::remove(path);
                return "";
            }
        }
    }
    /* Read rest of file */
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

void cache_put(const std::string& key, const std::string& data) {
    std::string path = cache_dir() + "/" + key;
    std::ofstream f(path, std::ios::binary);
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    f << now << "\n" << data;
}

std::string http_fetch(const std::string& url) {
    std::string cmd;
#ifdef _WIN32
    cmd = "powershell -NoLogo -NoProfile -Command \"try { $r = Invoke-WebRequest -Uri '" + url + "' -UseBasicParsing -TimeoutSec 30; Write-Output $r.Content } catch { }\"" + " 2>nul";
#else
    cmd = "curl -s --max-time 30 '" + url + "' 2>/dev/null || wget -q -O- --timeout=30 '" + url + "' 2>/dev/null";
#endif
    std::string result;
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) return "";
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe) != nullptr) result += buf;
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return result;
}

std::string extract_json_source(const std::string& raw) {
    size_t start = raw.find('{');
    if (start == std::string::npos) return "";
    size_t end = raw.rfind('}');
    if (end == std::string::npos || end < start) return "";
    return raw.substr(start, end - start + 1);
}

bool resolve_package(const std::string& spec, std::string& name, std::string& version, std::string& source, std::string& integrity) {
    std::string ver;
    if (!parse_pkg_spec(spec, name, ver)) return false;
    LockData lf = read_lockfile();
    auto lit = lf.packages.find(name);
    if (lit != lf.packages.end()) { version = lit->second.version; source = lit->second.resolved; integrity = lit->second.integrity; return true; }
    if (fs::exists("packages/" + name + "/aurora.pkg")) {
        PackageInfo info = read_manifest("packages/" + name);
        version = info.version; source = "file:packages/" + name; integrity = sha256_hex(version); return true;
    }
    std::string cache_key = name + "@" + (ver.empty() ? "latest" : ver);
    std::string cached = cache_get(cache_key);
    if (!cached.empty()) { version = ver.empty() ? "cached" : ver; source = "cache:" + cache_key; integrity = sha256_hex(cached); return true; }
    for (auto& reg : g_registries) {
        std::string url = reg.url + "/packages/" + name + (ver.empty() ? "/latest" : "/" + ver);
        std::string raw = http_fetch(url);
        if (!raw.empty()) {
            std::string json = extract_json_source(raw);
            if (!json.empty()) { version = ver.empty() ? "fetched" : ver; source = "registry:" + reg.name; integrity = sha256_hex(json); cache_put(cache_key, json); return true; }
        }
    }
    /* Try GitHub API registries (github:user/repo with GITHUB_TOKEN auth) */
    {
        std::string token;
        const char* tok = std::getenv("GITHUB_TOKEN");
        if (!tok) tok = std::getenv("GH_TOKEN");
        if (tok) token = tok;
        if (!token.empty()) {
            for (auto& reg : g_registries) {
                std::string gh_spec;
                if (reg.url.find("github:") == 0) gh_spec = reg.url.substr(7);
                else if (reg.url.find("gh:") == 0) gh_spec = reg.url.substr(3);
                else continue;
                size_t slash = gh_spec.find('/');
                if (slash == std::string::npos) continue;
                std::string user = gh_spec.substr(0, slash);
                std::string repo = gh_spec.substr(slash + 1);
                /* Fetch release info via GitHub API */
                std::string api_url = "https://api.github.com/repos/" + user + "/" + repo + "/releases";
                if (!ver.empty() && ver != "latest") api_url += "/tags/v" + ver;
                else api_url += "/latest";
#ifdef _WIN32
                std::string gh_cmd = "powershell -NoLogo -NoProfile -Command \"$h = @{ 'Accept' = 'application/vnd.github.v3+json'; 'Authorization' = 'Bearer " + token + "' }; try { $r = Invoke-WebRequest -Uri '" + api_url + "' -Method GET -Headers $h -UseBasicParsing -TimeoutSec 30; Write-Output $r.Content } catch { }\" 2>nul";
#else
                std::string gh_cmd = "curl -s -H 'Authorization: Bearer " + token + "' -H 'Accept: application/vnd.github.v3+json' '" + api_url + "' 2>/dev/null";
#endif
                std::string gh_result;
#ifdef _WIN32
                FILE* pipe = _popen(gh_cmd.c_str(), "r");
#else
                FILE* pipe = popen(gh_cmd.c_str(), "r");
#endif
                if (pipe) {
                    char buf[4096];
                    while (fgets(buf, sizeof(buf), pipe)) gh_result += buf;
#ifdef _WIN32
                    _pclose(pipe);
#else
                    pclose(pipe);
#endif
                }
                if (!gh_result.empty() && gh_result.find("\"tag_name\"") != std::string::npos) {
                    /* Extract version from tag_name */
                    size_t tn = gh_result.find("\"tag_name\"");
                    size_t vstart = gh_result.find('"', tn + 10);
                    if (vstart != std::string::npos) {
                        size_t vend = gh_result.find('"', vstart + 1);
                        if (vend != std::string::npos) {
                            std::string tag = gh_result.substr(vstart + 1, vend - vstart - 1);
                            if (tag.size() > 1 && tag[0] == 'v') tag = tag.substr(1);
                            version = tag;
                            source = "github:" + user + "/" + repo;
                            integrity = sha256_hex(gh_result);
                            cache_put(cache_key, gh_result);
                            return true;
                        }
                    }
                    /* Fallback: use version from tag */
                    version = ver.empty() ? "latest" : ver;
                    source = "github:" + user + "/" + repo;
                    integrity = sha256_hex(gh_result);
                    cache_put(cache_key, gh_result);
                    return true;
                }
            }
        }
    }
    if (name.find('/') != std::string::npos) {
        size_t slash = name.find('/');
        std::string g_user = name.substr(0, slash);
        std::string g_repo = name.substr(slash + 1);
        VossPackage g_pkg;
        if (resolve_github_package(g_user, g_repo, ver, name, version, source, integrity, g_pkg)) {
            return true;
        }
    }
    return false;
}

void load_registries() {
    if (!g_registries.empty()) return;
    std::ifstream f("aura.registry");
    if (f.is_open()) {
        std::string line;
        while (std::getline(f, line)) {
            std::string t = trim(line);
            if (t.empty() || t[0] == '#') continue;
            auto parts = split(t, ' ');
            if (parts.size() >= 2) g_registries.push_back({parts[0], parts[1]});
        }
    }
    if (g_registries.empty()) {
        g_registries.push_back({"aurora", DEFAULT_REGISTRY_URL});
        std::cout << "info: no registries configured, using default: " << DEFAULT_REGISTRY_URL << "\n";
        std::cout << "info: configure registries via 'voss registry add <name> <url>' or edit aura.registry\n";
    }
}

int run_cmd(const std::vector<std::string>& args) {
    if (args.empty()) return -1;
    std::string cmd;
    for (size_t i = 0; i < args.size(); i++) {
        if (i > 0) cmd += " ";
        if (args[i].find(' ') != std::string::npos) cmd += "\"" + args[i] + "\"";
        else cmd += args[i];
    }
    return system(cmd.c_str());
}

bool parse_github_spec(const std::string& spec, std::string& user, std::string& repo, std::string& ref, bool& is_branch) {
    std::string s = spec;
    size_t prefix_len = 0;
    if (s.find("github:") == 0) prefix_len = 7;
    else if (s.find("gh:") == 0) prefix_len = 3;
    else return false;
    s = s.substr(prefix_len);
    is_branch = false;
    ref = "";
    size_t hash = s.find('#');
    if (hash != std::string::npos) {
        ref = s.substr(hash + 1);
        s = s.substr(0, hash);
        is_branch = true;
    }
    size_t at = s.find('@');
    if (at != std::string::npos) {
        ref = s.substr(at + 1);
        s = s.substr(0, at);
        is_branch = false;
    }
    size_t slash = s.find('/');
    if (slash == std::string::npos) return false;
    user = s.substr(0, slash);
    repo = s.substr(slash + 1);
    return !user.empty() && !repo.empty();
}

static std::string json_extract_string(const std::string& json, const std::string& field) {
    size_t pos = json.find("\"" + field + "\"");
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + field.size() + 2);
    if (pos == std::string::npos) return "";
    pos = json.find('"', pos);
    if (pos == std::string::npos) return "";
    size_t end = json.find('"', pos + 1);
    if (end == std::string::npos) return "";
    return json.substr(pos + 1, end - pos - 1);
}

static std::vector<std::string> json_extract_array(const std::string& json, const std::string& field) {
    std::vector<std::string> result;
    size_t pos = json.find("\"" + field + "\"");
    if (pos == std::string::npos) return result;
    pos = json.find('[', pos);
    if (pos == std::string::npos) return result;
    size_t end = json.find(']', pos);
    if (end == std::string::npos) return result;
    std::string arr = json.substr(pos + 1, end - pos - 1);
    size_t s = 0;
    while ((s = arr.find('"', s)) != std::string::npos) {
        s++;
        size_t e = arr.find('"', s);
        if (e == std::string::npos) break;
        result.push_back(arr.substr(s, e - s));
        s = e + 1;
    }
    return result;
}

VossPackage read_voss_json(const std::string& dir) {
    VossPackage pkg;
    std::ifstream f(dir + "/voss.json");
    if (!f.is_open()) return pkg;
    std::stringstream ss;
    ss << f.rdbuf();
    std::string content = ss.str();
    return parse_voss_json_string(content);
}

VossPackage parse_voss_json_string(const std::string& json) {
    VossPackage pkg;
    pkg.name = json_extract_string(json, "name");
    pkg.version = json_extract_string(json, "version");
    pkg.entry = json_extract_string(json, "entry");
    pkg.dependencies = json_extract_array(json, "dependencies");
    return pkg;
}

bool resolve_github_package(const std::string& user, const std::string& repo, const std::string& ver, std::string& out_name, std::string& out_version, std::string& out_source, std::string& out_integrity, VossPackage& out_pkg) {
    std::string ref = ver.empty() ? "main" : ver;
    std::string raw_url = "https://raw.githubusercontent.com/" + user + "/" + repo + "/" + ref + "/voss.json";
    std::string raw = http_fetch(raw_url);
    if (raw.empty() && ref == "main") {
        raw_url = "https://raw.githubusercontent.com/" + user + "/" + repo + "/master/voss.json";
        raw = http_fetch(raw_url);
    }
    if (raw.empty()) {
        if (!ver.empty()) {
            // Try common tag prefixes
            std::vector<std::string> alt_refs = {"v" + ver, "v" + ver};
            for (auto& ar : alt_refs) {
                std::string alt_url = "https://raw.githubusercontent.com/" + user + "/" + repo + "/" + ar + "/voss.json";
                raw = http_fetch(alt_url);
                if (!raw.empty()) { ref = ar; break; }
            }
        }
    }
    if (raw.empty()) return false;
    std::string json = extract_json_source(raw);
    if (json.empty()) return false;
    out_pkg = parse_voss_json_string(json);
    if (out_pkg.name.empty()) return false;
    out_name = out_pkg.name;
    out_version = ver.empty() ? out_pkg.version : ver;
    out_source = "github:" + user + "/" + repo + (ver.empty() ? "" : "@" + ver);
    out_integrity = sha256_hex(json);
    std::string cache_key = user + "/" + repo + "@" + (ver.empty() ? "latest" : ver);
    cache_put(cache_key, json);
    return true;
}

std::vector<std::string> github_list_tags(const std::string& user, const std::string& repo) {
    std::vector<std::string> tags;
#ifdef _WIN32
    std::string cmd = "git ls-remote --tags \"https://github.com/" + user + "/" + repo + ".git\" 2>nul";
#else
    std::string cmd = "git ls-remote --tags \"https://github.com/" + user + "/" + repo + ".git\" 2>/dev/null";
#endif
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) return tags;
    char buf[4096];
    std::string output;
    while (fgets(buf, sizeof(buf), pipe) != nullptr) output += buf;
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    std::stringstream ss(output);
    std::string line;
    std::set<std::string> seen;
    while (std::getline(ss, line)) {
        size_t refs = line.find("refs/tags/");
        if (refs == std::string::npos) continue;
        std::string tag = line.substr(refs + 10);
        if (tag.back() == '^') tag.pop_back();
        if (tag.rfind("^{}") != std::string::npos) tag = tag.substr(0, tag.size() - 3);
        if (seen.insert(tag).second) tags.push_back(tag);
    }
    return tags;
}

DepNode build_tree(const std::string& name, const std::string& version,
                   std::set<std::string>& visited, std::map<std::string, std::string>& versions) {
    DepNode node;
    node.name = name;
    node.version = version;
    if (visited.count(name)) { node.conflict = true; node.conflict_msg = "circular dependency"; return node; }
    visited.insert(name);
    if (versions.count(name) && versions[name] != version) { node.conflict = true; node.conflict_msg = "version conflict: " + versions[name] + " vs " + version; }
    else versions[name] = version;
    LockData lf = read_lockfile();
    auto it = lf.packages.find(name);
    if (it != lf.packages.end())
        for (auto& dep : it->second.dependencies)
        { std::string dn, dv; parse_pkg_spec(dep, dn, dv); node.children.push_back(build_tree(dn, dv, visited, versions)); }
    visited.erase(name);
    return node;
}

void print_tree(const DepNode& node, const std::string& prefix, bool is_last) {
    std::cout << prefix << (is_last ? "└── " : "├── ") << node.name;
    if (!node.version.empty()) std::cout << "@" << node.version;
    if (node.conflict) std::cout << " [CONFLICT]";
    std::cout << "\n";
    std::string child_prefix = prefix + (is_last ? "    " : "│   ");
    for (size_t i = 0; i < node.children.size(); i++)
        print_tree(node.children[i], child_prefix, i == node.children.size() - 1);
}
