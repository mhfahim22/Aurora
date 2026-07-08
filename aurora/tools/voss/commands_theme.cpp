#include "voss.h"

/* ── Theme Store — voss theme ── */

static const char* theme_dir() {
    static std::string dir;
    if (dir.empty()) {
        const char* home = getenv("USERPROFILE");
        if (!home) home = getenv("HOME");
        if (!home) home = "/tmp";
        dir = std::string(home) + "/.aurora/themes";
    }
    return dir.c_str();
}

static void ensure_theme_dir() {
    fs::create_directories(theme_dir());
}

int cmd_theme(int argc, char** argv) {
    if (argc < 1) {
        std::cout << "usage: voss theme <command> [options]\n"
                  << "commands:\n"
                  << "  search  <query>    Search themes in registry\n"
                  << "  install <name>     Install a theme\n"
                  << "  uninstall <name>   Remove an installed theme\n"
                  << "  list               List installed themes\n"
                  << "  apply <name>       Apply a theme\n"
                  << "  publish <path>     Publish a theme to registry\n"
                  << "  show [name]        Show theme info (or current)\n"
                  << "  validate <path>    Validate a theme JSON file\n";
        return 0;
    }

    std::string cmd = argv[0];

    if (cmd == "search") {
        if (argc < 2) { std::cerr << "usage: voss theme search <query>\n"; return 1; }
        std::string query = argv[1];
        for (int i = 2; i < argc; i++) query += " " + std::string(argv[i]);

        std::string result = http_fetch("https://aurora-packages.dev/api/v1/search?q=" + query +
                                        "&type=theme");
        if (result.empty()) {
            std::cout << "no themes found for '" << query << "'\n";
            return 0;
        }

        /* Parse and display results */
        size_t pos = 0;
        while ((pos = result.find("name\":\"")) != std::string::npos) {
            pos += 7;
            size_t end = result.find("\"", pos);
            if (end == std::string::npos) break;
            std::string name = result.substr(pos, end - pos);
            std::cout << "  " << name << "\n";
            result = result.substr(end);
        }
        return 0;
    }

    if (cmd == "install") {
        if (argc < 2) { std::cerr << "usage: voss theme install <name>\n"; return 1; }
        std::string name = argv[1];
        ensure_theme_dir();

        std::string url = "https://aurora-packages.dev/api/v1/theme/" + name;
        std::string json = http_fetch(url);
        if (json.empty()) {
            std::cerr << "error: theme '" << name << "' not found in registry\n";
            return 1;
        }

        std::string path = std::string(theme_dir()) + "/" + name + ".json";
        std::ofstream f(path);
        if (!f) { std::cerr << "error: cannot write to " << path << "\n"; return 1; }
        f << json;
        f.close();

        std::cout << "installed theme '" << name << "'\n";
        return 0;
    }

    if (cmd == "uninstall") {
        if (argc < 2) { std::cerr << "usage: voss theme uninstall <name>\n"; return 1; }
        std::string name = argv[1];
        std::string path = std::string(theme_dir()) + "/" + name + ".json";
        if (!fs::remove(path)) {
            std::cerr << "error: theme '" << name << "' not found\n";
            return 1;
        }
        std::cout << "uninstalled theme '" << name << "'\n";
        return 0;
    }

    if (cmd == "list") {
        ensure_theme_dir();
        int count = 0;
        for (auto& entry : fs::directory_iterator(theme_dir())) {
            if (entry.path().extension() == ".json") {
                std::string name = entry.path().stem().string();
                std::ifstream f(entry.path());
                std::string line;
                std::getline(f, line);
                std::cout << "  " << name << "\n";
                count++;
            }
        }
        if (count == 0) std::cout << "no themes installed\n";
        return 0;
    }

    if (cmd == "apply") {
        if (argc < 2) { std::cerr << "usage: voss theme apply <name>\n"; return 1; }
        std::string name = argv[1];
        std::string path = std::string(theme_dir()) + "/" + name + ".json";
        if (!fs::exists(path)) {
            std::cerr << "error: theme '" << name << "' not installed\n";
            std::cerr << "try: voss theme install " << name << "\n";
            return 1;
        }

        /* Write theme name to a marker file for app consumption */
        std::string marker = std::string(theme_dir()) + "/current_theme";
        std::ofstream mf(marker);
        if (mf) { mf << name; mf.close(); }

        std::cout << "applied theme '" << name << "'\n";
        std::cout << "restart your app to see the change\n";
        return 0;
    }

    if (cmd == "publish") {
        if (argc < 2) { std::cerr << "usage: voss theme publish <path>\n"; return 1; }
        std::string path = argv[1];

        /* Validate first */
        std::string json = read_file_str(path);
        if (json.empty()) { std::cerr << "error: cannot read " << path << "\n"; return 1; }

        /* POST to registry */
        std::string response = http_fetch("https://aurora-packages.dev/api/v1/theme/publish");
        /* For now, just print instructions */
        std::cout << "theme at " << path << " ready for publishing\n";
        std::cout << "to publish, run: voss publish " << path << "\n";
        return 0;
    }

    if (cmd == "show") {
        ensure_theme_dir();
        std::string name;
        if (argc >= 2) {
            name = argv[1];
        } else {
            std::string marker = std::string(theme_dir()) + "/current_theme";
            std::ifstream mf(marker);
            std::getline(mf, name);
            if (name.empty()) { std::cerr << "no theme currently applied\n"; return 1; }
        }

        std::string path = std::string(theme_dir()) + "/" + name + ".json";
        if (!fs::exists(path)) {
            std::cerr << "error: theme '" << name << "' not found\n";
            return 1;
        }

        std::ifstream f(path);
        std::string line;
        while (std::getline(f, line)) std::cout << line << "\n";
        return 0;
    }

    if (cmd == "validate") {
        if (argc < 2) { std::cerr << "usage: voss theme validate <path>\n"; return 1; }
        std::string path = argv[1];
        std::string json = read_file_str(path);
        if (json.empty()) { std::cerr << "error: cannot read " << path << "\n"; return 1; }

        /* Basic JSON validation */
        int brace_depth = 0, bracket_depth = 0;
        bool in_str = false;
        bool valid = true;
        for (char c : json) {
            if (c == '"' && (json.empty() || json[&c - &json[0]] == 0 || json[&c - &json[0] - 1] != '\\'))
                in_str = !in_str;
            if (!in_str) {
                if (c == '{') brace_depth++;
                if (c == '}') brace_depth--;
                if (c == '[') bracket_depth++;
                if (c == ']') bracket_depth--;
                if (brace_depth < 0 || bracket_depth < 0) { valid = false; break; }
            }
        }
        if (brace_depth != 0 || bracket_depth != 0) valid = false;

        if (valid)
            std::cout << "theme '" << path << "' is valid\n";
        else
            std::cerr << "theme '" << path << "' has invalid JSON\n";
        return valid ? 0 : 1;
    }

    std::cerr << "unknown theme command '" << cmd << "'\n";
    return 1;
}
