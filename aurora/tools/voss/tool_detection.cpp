#include "tool_detection.h"
#include "bridge_shared.h"
#include <cstdio>
#include <cstdlib>

/* ── Run a command and return trimmed stdout ── */
static std::string cmd_output(const std::string& cmd) {
    std::string result;
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) return {};
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) result += buf;
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    return result;
}

/* ── Check if executable exists in PATH ── */
static std::string find_in_path(const std::string& name) {
#ifdef _WIN32
    /* Check common locations first */
    const char* known_dirs[] = {
        "C:\\Windows\\System32\\", "C:\\Windows\\SysWOW64\\",
        "C:\\Program Files\\", "C:\\Program Files (x86)\\",
        nullptr
    };
    for (int i = 0; known_dirs[i]; i++) {
        std::string fp = std::string(known_dirs[i]) + name;
        if (fs::exists(fp)) return fp;
        if (fs::exists(fp + ".exe")) return fp + ".exe";
    }
    std::string out = cmd_output("where " + name + " 2>nul");
    if (!out.empty()) {
        size_t nl = out.find('\n');
        if (nl != std::string::npos) out = out.substr(0, nl);
        nl = out.find('\r');
        if (nl != std::string::npos) out = out.substr(0, nl);
    }
    return out;
#else
    return cmd_output("which " + name + " 2>/dev/null");
#endif
}

/* ── Check if a path/file exists and executable ── */
static bool exists_exe(const std::string& path) {
    if (path.empty()) return false;
#ifdef _WIN32
    return fs::exists(path) || fs::exists(path + ".exe");
#else
    return fs::exists(path);
#endif
}

/* ════════════════════════════════════════════════════════════
   Tool Detection
   ════════════════════════════════════════════════════════════ */

ToolInfo detect_c_compiler() {
    ToolInfo info;
    info.name = "C compiler";

    struct { const char* label; const char* exe; } candidates[] = {
        {"zig cc",     "zig"},
        {"gcc",        "gcc"},
        {"clang",      "clang"},
        {"cc",         "cc"},
#ifdef _WIN32
        {"MSVC cl",    "cl.exe"},
#endif
        {nullptr, nullptr}
    };

    for (int i = 0; candidates[i].label; i++) {
        std::string p = find_in_path(candidates[i].exe);
        if (!p.empty()) {
            info.found = true;
            info.path = p;
            info.version = cmd_output(std::string(candidates[i].exe) + " --version 2>&1 | head -1");
            return info;
        }
    }
    return info;
}

ToolInfo detect_python() {
    ToolInfo info;
    info.name = "Python";

    const char* candidates[] = {"python3", "python", "python.exe", nullptr};
    for (int i = 0; candidates[i]; i++) {
        std::string p = find_in_path(candidates[i]);
        if (!p.empty()) {
            info.found = true;
            info.path = p;
            info.version = cmd_output(std::string(candidates[i]) + " --version 2>&1");
            return info;
        }
    }
    return info;
}

ToolInfo detect_node() {
    ToolInfo info;
    info.name = "Node.js";

    const char* candidates[] = {"node", "node.exe", nullptr};
    for (int i = 0; candidates[i]; i++) {
        std::string p = find_in_path(candidates[i]);
        if (!p.empty()) {
            info.found = true;
            info.path = p;
            info.version = cmd_output(std::string(candidates[i]) + " --version 2>&1");
            return info;
        }
    }
    return info;
}

ToolInfo detect_cargo() {
    ToolInfo info;
    info.name = "Cargo";

    const char* candidates[] = {"cargo", "cargo.exe", nullptr};
    for (int i = 0; candidates[i]; i++) {
        std::string p = find_in_path(candidates[i]);
        if (!p.empty()) {
            info.found = true;
            info.path = p;
            return info;
        }
    }
    return info;
}

/* ════════════════════════════════════════════════════════════
   Tools Cache Directory
   ════════════════════════════════════════════════════════════ */

std::string tools_cache_dir() {
    std::string dir = cache_dir() + "/tools";
    fs::create_directories(dir);
    return dir;
}

/* ════════════════════════════════════════════════════════════
   Download + Extract Portable Tools
   ════════════════════════════════════════════════════════════ */

bool download_and_extract(const std::string& url, const std::string& dest_dir,
                          const std::string& expected_exe)
{
    fs::create_directories(dest_dir);
    std::string ext = ".zip";
    size_t dot_pos = url.rfind('.');
    if (dot_pos != std::string::npos)
        ext = url.substr(dot_pos);
    std::string archive_path = dest_dir + "/dl" + ext;

    std::cout << "[tools] downloading " << url << "\n";

    /* Download via system shell */
    std::string dl_cmd;
#ifdef _WIN32
    dl_cmd = "powershell -NoLogo -NoProfile -Command \""
             "$wc=New-Object System.Net.WebClient; "
             "try{ $wc.DownloadFile('" + url + "','" + archive_path + "'); "
             "Write-Host 'ok' } catch{ Write-Host 'fail' }\" 2>nul";
#else
    dl_cmd = "curl -sLo \"" + archive_path + "\" \"" + url
           + "\" 2>/dev/null || wget -qO \"" + archive_path + "\" \"" + url + "\" 2>/dev/null";
#endif
    if (std::system(dl_cmd.c_str()) != 0) {
        std::cerr << "[tools] download failed\n";
        return false;
    }
    if (!fs::exists(archive_path) || fs::file_size(archive_path) == 0) {
        std::cerr << "[tools] downloaded file is empty\n";
        return false;
    }

    std::cout << "[tools] extracting...\n";

    /* Extract */
    if (ext == ".zip") {
#ifdef _WIN32
        std::string ext_cmd = "powershell -NoLogo -NoProfile -Command \""
                              "Expand-Archive -Path '" + archive_path
                              + "' -DestinationPath '" + dest_dir + "' -Force\" 2>nul";
#else
        std::string ext_cmd = "unzip -o \"" + archive_path + "\" -d \"" + dest_dir + "\" 2>/dev/null";
#endif
        if (std::system(ext_cmd.c_str()) != 0) {
            std::cerr << "[tools] extraction failed\n";
            std::remove(archive_path.c_str());
            return false;
        }
    } else {
        /* tar.gz / tar.xz */
        std::string ext_cmd = "tar xf \"" + archive_path + "\" -C \"" + dest_dir + "\" 2>&1";
        if (std::system(ext_cmd.c_str()) != 0) {
            std::cerr << "[tools] extraction failed\n";
            std::remove(archive_path.c_str());
            return false;
        }
    }

    std::remove(archive_path.c_str());

    /* Check expected executable exists somewhere in dest_dir */
    bool found = false;
    if (fs::exists(dest_dir + "/" + expected_exe))
        found = true;
    if (!found)
        for (auto& entry : fs::recursive_directory_iterator(dest_dir))
            if (entry.path().filename() == expected_exe) {
                found = true;
                break;
            }

    if (!found) {
        std::cerr << "[tools] warning: '" << expected_exe << "' not found after extraction\n";
        return false;
    }

    std::cout << "[tools] ready\n";
    return true;
}

/* ════════════════════════════════════════════════════════════
   Ensure Tools (Detect → Auto-Download)
   ════════════════════════════════════════════════════════════ */

ToolInfo ensure_c_compiler() {
    ToolInfo info = detect_c_compiler();
    if (info.found) {
        std::cout << "[tools] found C compiler: " << info.path
                  << (info.version.empty() ? "" : " (" + info.version + ")") << "\n";
        return info;
    }

    /* Auto-download Zig cc as portable C compiler */
    std::cout << "[tools] no C compiler found — downloading Zig cc (portable)\n";
    std::string cache = tools_cache_dir() + "/zig";

#ifdef _WIN32
    std::string url = "https://ziglang.org/download/0.13.0/zig-windows-x86_64.zip";
    std::string exe_name = "zig.exe";
#elif defined(__APPLE__)
    #ifdef __aarch64__
        std::string url = "https://ziglang.org/download/0.13.0/zig-macos-aarch64.tar.xz";
    #else
        std::string url = "https://ziglang.org/download/0.13.0/zig-macos-x86_64.tar.xz";
    #endif
    std::string exe_name = "zig";
#else
    std::string url = "https://ziglang.org/download/0.13.0/zig-linux-x86_64.tar.xz";
    std::string exe_name = "zig";
#endif

    if (!download_and_extract(url, cache, exe_name)) {
        std::cerr << "[tools] WARNING: could not download Zig cc — "
                  << "install a C compiler (gcc/clang/MSVC) manually\n";
        return info;
    }

    /* Find zig executable */
    for (auto& entry : fs::recursive_directory_iterator(cache)) {
        if (entry.path().filename() == exe_name) {
            info.found = true;
            info.path = fs::absolute(entry.path()).string();
            info.is_portable = true;
            info.version = cmd_output("\"" + info.path + "\" version 2>&1");
            std::cout << "[tools] downloaded Zig cc: " << info.path
                      << (info.version.empty() ? "" : " (" + info.version + ")") << "\n";
            return info;
        }
    }

    std::cerr << "[tools] WARNING: Zig downloaded but executable not found\n";
    return info;
}

ToolInfo ensure_python() {
    ToolInfo info = detect_python();
    if (info.found) {
        std::cout << "[tools] found Python: " << info.path
                  << (info.version.empty() ? "" : " (" + info.version + ")") << "\n";
        return info;
    }

#ifdef _WIN32
    /* Download embeddable Python for Windows */
    std::cout << "[tools] no Python found — downloading embeddable Python\n";
    std::string cache = tools_cache_dir() + "/python";
    std::string url = "https://www.python.org/ftp/python/3.12.0/python-3.12.0-embed-amd64.zip";
    std::string exe_name = "python.exe";

    if (!download_and_extract(url, cache, exe_name)) {
        std::cerr << "[tools] WARNING: could not download Python — "
                  << "install Python manually\n";
        return info;
    }

    /* Find python executable */
    for (auto& entry : fs::recursive_directory_iterator(cache)) {
        if (entry.path().filename() == exe_name) {
            info.found = true;
            info.path = fs::absolute(entry.path()).string();
            info.is_portable = true;
            info.version = cmd_output("\"" + info.path + "\" --version 2>&1");
            std::cout << "[tools] downloaded Python: " << info.path
                      << (info.version.empty() ? "" : " (" + info.version + ")") << "\n";
            return info;
        }
    }
#else
    std::cerr << "[tools] WARNING: Python not found — install python3 with your package manager\n";
#endif

    return info;
}

ToolInfo ensure_node() {
    ToolInfo info = detect_node();
    if (info.found) {
        std::cout << "[tools] found Node.js: " << info.path
                  << (info.version.empty() ? "" : " (" + info.version + ")") << "\n";
        return info;
    }

    std::cout << "[tools] no Node.js found — downloading portable Node.js\n";
    std::string cache = tools_cache_dir() + "/node";
    std::string exe_name = "node.exe";

#ifdef _WIN32
    std::string url = "https://nodejs.org/dist/v20.11.0/node-v20.11.0-win-x64.zip";
    exe_name = "node.exe";
#elif defined(__APPLE__)
    std::string url = "https://nodejs.org/dist/v20.11.0/node-v20.11.0-darwin-x64.tar.gz";
    exe_name = "node";
#else
    std::string url = "https://nodejs.org/dist/v20.11.0/node-v20.11.0-linux-x64.tar.xz";
    exe_name = "node";
#endif

    if (!download_and_extract(url, cache, exe_name)) {
        std::cerr << "[tools] WARNING: could not download Node.js — "
                  << "install Node.js manually\n";
        return info;
    }

    for (auto& entry : fs::recursive_directory_iterator(cache)) {
        if (entry.path().filename() == exe_name) {
            info.found = true;
            info.path = fs::absolute(entry.path()).string();
            info.is_portable = true;
            info.version = cmd_output("\"" + info.path + "\" --version 2>&1");
            std::cout << "[tools] downloaded Node.js: " << info.path
                      << (info.version.empty() ? "" : " (" + info.version + ")") << "\n";
            return info;
        }
    }

    return info;
}
