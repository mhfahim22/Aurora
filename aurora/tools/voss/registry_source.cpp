#include "voss.h"
#include <algorithm>
#include <cstring>
#include <sstream>
#include <iostream>

/* ── RegistrySource trait ── */
/* Base class for registry backends. Each backend knows how to
   resolve packages, publish, and handle authentication. */
struct RegistrySource {
    virtual ~RegistrySource() = default;
    virtual std::string name() const = 0;
    /* Resolve a package: returns JSON metadata or empty string */
    virtual std::string resolve(const std::string& pkg, const std::string& ver) = 0;
    /* Publish a package archive. Returns true on success. */
    virtual bool publish(const std::string& pkg, const std::string& version,
                         const std::string& archive_path,
                         const std::string& voss_json) = 0;
    /* Check if this source is authenticated */
    virtual bool is_authenticated() const { return false; }
    /* Get auth token (for display) */
    virtual std::string auth_info() const { return "no auth"; }
};

/* ── GitHub API Registry Source ── */
/* Uses the GitHub API (via http_client.hpp or http_fetch) to
   publish and resolve packages stored in GitHub Releases.
   Requires GITHUB_TOKEN or GH_TOKEN env var for write operations. */
struct GitHubAPISource : RegistrySource {
    std::string m_user, m_repo;
    std::string m_token;
    bool m_auth_valid;

    GitHubAPISource(const std::string& user, const std::string& repo)
        : m_user(user), m_repo(repo), m_auth_valid(false)
    {
        /* Try GITHUB_TOKEN first, then GH_TOKEN */
        const char* tok = std::getenv("GITHUB_TOKEN");
        if (!tok) tok = std::getenv("GH_TOKEN");
        if (tok && strlen(tok) > 0) {
            m_token = tok;
            m_auth_valid = true;
        }
    }

    std::string name() const override {
        return "github:" + m_user + "/" + m_repo;
    }

    bool is_authenticated() const override { return m_auth_valid; }

    std::string auth_info() const override {
        if (m_auth_valid)
            return "GITHUB_TOKEN (" + m_token.substr(0, 8) + "..." +
                   m_token.substr(m_token.size() - 4) + ")";
        return "no token (set GITHUB_TOKEN or GH_TOKEN)";
    }

    /* Make an authenticated GitHub API request.
       Returns response body, or empty on failure.
       Uses the native http_get from http_client.hpp if available,
       falls back to http_fetch (shell-out). */
    static std::string github_api(const std::string& url, const std::string& method,
                                   const std::string& body, const std::string& token)
    {
        /* We use http_fetch (shell-out) since it's already in voss.
           For POST/PATCH with body, we construct a more elaborate command. */
#ifdef _WIN32
        /* PowerShell: support method + body */
        std::string cmd = "powershell -NoLogo -NoProfile -Command \"";
        cmd += "$h = @{ 'Accept' = 'application/vnd.github.v3+json'";
        if (!token.empty()) {
            cmd += "; 'Authorization' = 'Bearer " + token + "'";
        }
        if (method == "POST" || method == "PATCH") {
            cmd += "; 'Content-Type' = 'application/json'";
        }
        cmd += " }; ";
        if (body.empty()) {
            cmd += "$r = Invoke-WebRequest -Uri '" + url + "' -Method " + method +
                   " -Headers $h -UseBasicParsing -TimeoutSec 30; Write-Output $r.Content";
        } else {
            /* Escape body for PowerShell */
            std::string esc_body;
            for (char c : body) {
                if (c == '\'') esc_body += "''";
                else esc_body += c;
            }
            cmd += "$r = Invoke-WebRequest -Uri '" + url + "' -Method " + method +
                   " -Headers $h -Body '" + esc_body + "' -UseBasicParsing -TimeoutSec 30; Write-Output $r.Content";
        }
        cmd += " } catch { Write-Error $_.Exception.Message }\" 2>nul";
#else
        std::string cmd;
        if (body.empty()) {
            cmd = "curl -s -X " + method + " '" + url + "'";
            if (!token.empty())
                cmd += " -H 'Authorization: Bearer " + token + "'";
            cmd += " -H 'Accept: application/vnd.github.v3+json' 2>/dev/null";
        } else {
            cmd = "curl -s -X " + method + " '" + url + "'";
            if (!token.empty())
                cmd += " -H 'Authorization: Bearer " + token + "'";
            cmd += " -H 'Accept: application/vnd.github.v3+json'";
            std::string esc_body;
            for (char c : body) {
                if (c == '\'') esc_body += "'\\''";
                else esc_body += c;
            }
            cmd += " -d '" + esc_body + "' 2>/dev/null";
        }
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
        if (result.find("Not Found") != std::string::npos ||
            result.find("Bad credentials") != std::string::npos ||
            result.find("Not Found") != std::string::npos)
            return {};
        return result;
    }

    /* Resolve from GitHub Releases or git tags */
    std::string resolve(const std::string& pkg, const std::string& ver) override {
        /* Try GitHub releases API first */
        std::string url = "https://api.github.com/repos/" + m_user + "/" + m_repo +
                          "/releases";
        if (!ver.empty() && ver != "latest") {
            url += "/tags/v" + ver;
        } else {
            url += "/latest";
        }
        std::string result = github_api(url, "GET", "", m_token);
        if (!result.empty()) return result;

        /* Fall back: raw voss.json from git tags */
        std::string tag = ver.empty() ? "latest" : "v" + ver;
        std::string raw_url = "https://raw.githubusercontent.com/" + m_user +
                              "/" + m_repo + "/" + tag + "/voss.json";
        /* Use regular http_fetch without auth for raw content */
        std::string raw = http_fetch(raw_url);
        if (!raw.empty()) return raw;

        return {};
    }

    /* Publish to GitHub Releases via API */
    bool publish(const std::string& pkg, const std::string& version,
                 const std::string& archive_path,
                 const std::string& voss_json) override
    {
        if (!m_auth_valid) {
            std::cerr << "error: GITHUB_TOKEN not set. publish requires authentication.\n";
            std::cerr << "  set GITHUB_TOKEN or GH_TOKEN environment variable\n";
            return false;
        }

        /* 1. Create release via API */
        std::string tag_name = "v" + version;
        std::string release_body = "{\"tag_name\":\"" + tag_name + "\",\"name\":\"" +
                                   pkg + " v" + version + "\",\"body\":\"" +
                                   "Release of " + pkg + " v" + version + "\"}";

        std::string create_url = "https://api.github.com/repos/" + m_user +
                                  "/" + m_repo + "/releases";

        std::cout << "  creating GitHub release " << tag_name << "...\n";
        std::string result = github_api(create_url, "POST", release_body, m_token);

        /* Extract upload_url from response */
        std::string upload_url;
        std::string release_id;
        {
            /* Find "id": number */
            size_t id_pos = result.find("\"id\":");
            if (id_pos != std::string::npos) {
                id_pos += 5;
                while (id_pos < result.size() && (result[id_pos] == ' ' || result[id_pos] == ':')) id_pos++;
                size_t id_end = result.find_first_not_of("0123456789", id_pos);
                if (id_end != std::string::npos)
                    release_id = result.substr(id_pos, id_end - id_pos);
            }
        }

        if (release_id.empty()) {
            std::cerr << "error: could not create release\n";
            return false;
        }

        /* 2. Upload archive asset */
        if (!archive_path.empty() && fs::exists(archive_path)) {
            std::string asset_url = "https://uploads.github.com/repos/" + m_user +
                                     "/" + m_repo + "/releases/" + release_id +
                                     "/assets?name=" + pkg + "@" + version + ".tgz";

            std::cout << "  uploading archive...\n";
            /* For uploading binary assets, we need a different approach:
               use the raw file upload via PowerShell/curl */
#ifdef _WIN32
            std::string cmd = "powershell -NoLogo -NoProfile -Command \"";
            cmd += "$h = @{ 'Accept' = 'application/vnd.github.v3+json'; 'Authorization' = 'Bearer " + m_token + "'; 'Content-Type' = 'application/octet-stream' }; ";
            cmd += "$r = Invoke-WebRequest -Uri '" + asset_url + "' -Method POST -Headers $h -InFile '" + archive_path + "' -UseBasicParsing -TimeoutSec 120; Write-Output $r.Content";
            cmd += " } catch { Write-Error $_.Exception.Message }\" 2>nul";
#else
            std::string cmd = "curl -s -X POST '" + asset_url + "'";
            cmd += " -H 'Authorization: Bearer " + m_token + "'";
            cmd += " -H 'Accept: application/vnd.github.v3+json'";
            cmd += " -H 'Content-Type: application/octet-stream'";
            cmd += " --data-binary '@" + archive_path + "' 2>/dev/null";
#endif
            std::string upload_result;
#ifdef _WIN32
            FILE* pipe = _popen(cmd.c_str(), "r");
#else
            FILE* pipe = popen(cmd.c_str(), "r");
#endif
            if (pipe) {
                char buf[4096];
                while (fgets(buf, sizeof(buf), pipe) != nullptr) upload_result += buf;
#ifdef _WIN32
                _pclose(pipe);
#else
                pclose(pipe);
#endif
            }
            if (!upload_result.empty() && upload_result.find("\"state\":\"uploaded\"") != std::string::npos) {
                std::cout << "  asset uploaded\n";
            } else {
                std::cout << "  warning: asset upload may have failed (try uploading manually)\n";
                std::cout << "    " << create_url << "/releases/tag/" << tag_name << "\n";
            }
        }

        std::cout << "  published to https://github.com/" << m_user << "/" << m_repo
                  << "/releases/tag/" << tag_name << "\n";
        return true;
    }
};

/* ── Factory: create appropriate RegistrySource for a given spec ── */
static RegistrySource* create_registry_source(const std::string& spec) {
    /* github:user/repo */
    if (spec.find("github:") == 0 || spec.find("gh:") == 0) {
        std::string gh_spec = spec;
        if (gh_spec.find("github:") == 0)
            gh_spec = gh_spec.substr(7);
        else
            gh_spec = gh_spec.substr(3);

        size_t slash = gh_spec.find('/');
        if (slash != std::string::npos) {
            std::string user = gh_spec.substr(0, slash);
            std::string repo = gh_spec.substr(slash + 1);
            return new GitHubAPISource(user, repo);
        }
    }
    /* file:///path or just a URL (future: plain registry server) */
    return nullptr;
}

/* ── Command: voss registry register <pkg>@<version> [--registry <spec>] ── */
int cmd_registry_register(const std::string& spec, const std::string& registry_spec) {
    std::string pkg, ver;
    if (!parse_pkg_spec(spec, pkg, ver)) {
        std::cerr << "error: invalid package spec '" << spec << "'\n";
        return 1;
    }

    /* Read current manifest */
    PackageInfo info = read_manifest(".");
    if (info.name.empty()) {
        std::cerr << "error: no aurora.pkg found in current directory\n";
        return 1;
    }
    if (ver.empty()) ver = info.version;

    /* Determine which registry to use */
    RegistrySource* source = nullptr;
    if (!registry_spec.empty()) {
        source = create_registry_source(registry_spec);
        if (!source) {
            std::cerr << "error: unsupported registry spec '" << registry_spec << "'\n";
            std::cerr << "  format: github:user/repo\n";
            return 1;
        }
    } else {
        /* Check configured registries for GitHub ones */
        load_registries();
        for (auto& reg : g_registries) {
            if (reg.url.find("github:") == 0 || reg.url.find("gh:") == 0) {
                source = create_registry_source(reg.url);
                if (source) break;
            }
        }
        if (!source) {
            std::cerr << "error: no GitHub registry configured\n";
            std::cerr << "  add one: voss registry add my-gh-registry github:youruser/repo\n";
            return 1;
        }
    }

    std::cout << "Registering " << pkg << "@" << ver
              << " with " << source->name() << "\n";
    std::cout << "  auth: " << source->auth_info() << "\n";

    /* Create archive */
    std::string archive = create_tgz(".", pkg, ver);

    /* Generate voss.json */
    std::string voss_json;
    {
        std::ostringstream ss;
        ss << "{\n";
        ss << "  \"name\": \"" << info.name << "\",\n";
        ss << "  \"version\": \"" << ver << "\",\n";
        ss << "  \"entry\": \"" << info.entry << "\"";
        if (!info.description.empty())
            ss << ",\n  \"description\": \"" << info.description << "\"";
        if (!info.author.empty())
            ss << ",\n  \"author\": \"" << info.author << "\"";
        if (!info.dependencies.empty()) {
            ss << ",\n  \"dependencies\": [";
            for (size_t i = 0; i < info.dependencies.size(); i++) {
                if (i > 0) ss << ", ";
                ss << "\"" << info.dependencies[i] << "\"";
            }
            ss << "]";
        }
        ss << "\n}\n";
        voss_json = ss.str();
    }

    /* Publish */
    bool ok = source->publish(pkg, ver, archive, voss_json);

    /* Clean up archive */
    if (!archive.empty()) {
        fs::remove(archive);
    }

    delete source;
    return ok ? 0 : 1;
}

/* ── Command: voss registry login [github:user/repo] ── */
int cmd_registry_login(const std::string& registry_spec) {
    if (registry_spec.empty()) {
        /* Show status of all configured registries */
        load_registries();
        std::cout << "Registry authentication status:\n";
        for (auto& reg : g_registries) {
            std::unique_ptr<RegistrySource> src(create_registry_source(reg.url));
            if (src) {
                std::cout << "  " << reg.name << " (" << reg.url << "): "
                          << src->auth_info() << "\n";
            } else {
                std::cout << "  " << reg.name << " (" << reg.url << "): "
                          << "unsupported type (use github:user/repo)\n";
            }
        }
        std::cout << "\nTo authenticate for GitHub:\n";
        std::cout << "  set GITHUB_TOKEN=<your-token>\n";
        std::cout << "  or set GH_TOKEN=<your-token>\n";
        return 0;
    }

    RegistrySource* source = create_registry_source(registry_spec);
    if (!source) {
        std::cerr << "error: unsupported registry spec '" << registry_spec << "'\n";
        return 1;
    }

    std::cout << "Registry: " << source->name() << "\n";
    std::cout << "Auth: " << source->auth_info() << "\n";

    if (source->is_authenticated()) {
        std::cout << "Status: authenticated\n";
        /* Verify by trying to fetch the user */
        std::string user_check = GitHubAPISource::github_api(
            "https://api.github.com/user", "GET", "",
            std::getenv("GITHUB_TOKEN") ? std::getenv("GITHUB_TOKEN") :
            (std::getenv("GH_TOKEN") ? std::getenv("GH_TOKEN") : ""));
        if (!user_check.empty() && user_check.find("\"login\"") != std::string::npos) {
            std::cout << "Token valid (GitHub API authentication confirmed)\n";
        } else {
            std::cout << "Warning: token set but GitHub API verification failed\n";
            std::cout << "  Check that your token has 'repo' scope\n";
        }
    } else {
        std::cout << "Status: not authenticated\n";
    }

    delete source;
    return 0;
}

/* ── Command: voss registry github-register <user/repo> [version] ── */
/* Convenience wrapper: registers current package to a GitHub repo as registry */
int cmd_registry_github_register(const std::string& user_repo, const std::string& version) {
    std::string spec = "github:" + user_repo;
    std::string pkg, ver;
    PackageInfo info = read_manifest(".");
    if (!info.name.empty() && !version.empty()) {
        pkg = info.name;
        ver = version;
    } else if (!info.name.empty()) {
        pkg = info.name;
        ver = info.version;
    } else {
        pkg = "(unknown)";
        ver = version.empty() ? "0.1.0" : version;
    }
    return cmd_registry_register(pkg + "@" + ver, spec);
}
