/* ════════════════════════════════════════════════════════════
   Package Manager for Aurora
   ════════════════════════════════════════════════════════════
   Supports: init, install, build, list, clean
   ════════════════════════════════════════════════════════════ */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

/* ── Package manifest structure ── */
struct PackageInfo {
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    std::vector<std::string> dependencies;
    std::string entry; /* main .aura file */
};

static PackageInfo read_manifest(const std::string& dir) {
    PackageInfo info;
    info.name = fs::path(dir).filename().string();
    info.version = "0.1.0";
    info.entry = "main.aura";

    std::string manifest_path = dir + "/aurora.pkg";
    std::ifstream f(manifest_path);
    if (!f.is_open()) return info;

    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("name:", 0) == 0)
            info.name = line.substr(5);
        else if (line.rfind("version:", 0) == 0)
            info.version = line.substr(8);
        else if (line.rfind("author:", 0) == 0)
            info.author = line.substr(7);
        else if (line.rfind("description:", 0) == 0)
            info.description = line.substr(12);
        else if (line.rfind("entry:", 0) == 0)
            info.entry = line.substr(6);
        else if (line.rfind("depends:", 0) == 0) {
            std::string deps = line.substr(8);
            size_t pos = 0;
            while ((pos = deps.find(',')) != std::string::npos) {
                info.dependencies.push_back(deps.substr(0, pos));
                deps.erase(0, pos + 1);
            }
            if (!deps.empty())
                info.dependencies.push_back(deps);
        }
    }
    return info;
}

static bool write_manifest(const std::string& dir, const PackageInfo& info) {
    std::string path = dir + "/aurora.pkg";
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << "name: " << info.name << "\n";
    f << "version: " << info.version << "\n";
    f << "author: " << info.author << "\n";
    f << "description: " << info.description << "\n";
    f << "entry: " << info.entry << "\n";
    if (!info.dependencies.empty()) {
        f << "depends: ";
        for (size_t i = 0; i < info.dependencies.size(); i++) {
            if (i > 0) f << ",";
            f << info.dependencies[i];
        }
        f << "\n";
    }
    return true;
}

/* ── Package init: create new package scaffold ── */
static int cmd_package_init(const std::string& name) {
    std::string dir = name;
    if (fs::exists(dir)) {
        std::cerr << "package: directory '" << dir << "' already exists\n";
        return 1;
    }
    fs::create_directories(dir);
    fs::create_directories(dir + "/src");
    fs::create_directories(dir + "/tests");

    PackageInfo info;
    info.name = name;
    info.version = "0.1.0";
    info.author = "Aurora User";
    info.description = "A new Aurora package";
    info.entry = "src/main.aura";
    write_manifest(dir, info);

    /* Create main.aura stub */
    std::ofstream main_f(dir + "/src/main.aura");
    main_f << "## " << name << " - " << info.description << "\n";
    main_f << "## Version: " << info.version << "\n\n";
    main_f << "function main():\n";
    main_f << "  output(\"Hello from " << name << "!\")\n";
    main_f.close();

    std::cout << "package: created '" << dir << "' with aurora.pkg\n";
    return 0;
}

/* ── Package install: add a dependency ── */
static int cmd_package_install(const std::string& pkg_name) {
    std::string dir = ".";
    PackageInfo info = read_manifest(dir);

    /* Check if already installed */
    for (const auto& dep : info.dependencies) {
        if (dep == pkg_name) {
            std::cout << "package: '" << pkg_name << "' already a dependency\n";
            return 0;
        }
    }

    /* Create packages directory */
    fs::create_directories("packages");

    /* Create stub package */
    std::string pkg_dir = "packages/" + pkg_name;
    if (!fs::exists(pkg_dir)) {
        fs::create_directories(pkg_dir);
        std::ofstream f(pkg_dir + "/main.aura");
        f << "## " << pkg_name << " - installed dependency\n";
        f << "function " << pkg_name << "_init():\n";
        f << "  pass\n";
        f.close();
    }

    /* Add to manifest */
    info.dependencies.push_back(pkg_name);
    write_manifest(dir, info);

    std::cout << "package: installed '" << pkg_name << "'\n";
    return 0;
}

/* ── Package build: compile entry point ── */
static int cmd_package_build() {
    PackageInfo info = read_manifest(".");
    std::string entry_path = info.entry;
    if (!fs::exists(entry_path)) {
        std::cerr << "package: entry '" << entry_path << "' not found\n";
        return 1;
    }

    fs::create_directories("build");
    std::string obj_path = "build/" + info.name + ".obj";
    std::string exe_path = "build/" + info.name + ".exe";

    /* Build using external compiler */
    std::string cmd = "aurorac \"" + entry_path + "\" --emit-obj";
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "package: build failed\n";
        return ret;
    }

    /* Link with runtime */
    cmd = "lld-link \"" + obj_path + "\" aurora_runtime.lib /OUT:\"" + exe_path
        + "\" /NOLOGO /ENTRY:mainCRTStartup /SUBSYSTEM:CONSOLE";
    ret = std::system(cmd.c_str());

    if (ret == 0)
        std::cout << "package: built '" << exe_path << "'\n";
    return ret;
}

/* ── Package list: show dependencies ── */
static int cmd_package_list() {
    PackageInfo info = read_manifest(".");
    std::cout << "Package: " << info.name << " v" << info.version << "\n";
    if (!info.author.empty())
        std::cout << "Author:  " << info.author << "\n";
    if (!info.description.empty())
        std::cout << "About:   " << info.description << "\n";
    std::cout << "Entry:   " << info.entry << "\n";
    if (info.dependencies.empty()) {
        std::cout << "Deps:    (none)\n";
    } else {
        std::cout << "Deps:    ";
        for (size_t i = 0; i < info.dependencies.size(); i++) {
            if (i > 0) std::cout << ", ";
            std::cout << info.dependencies[i];
        }
        std::cout << "\n";
    }
    return 0;
}

/* ── Package clean: remove build artifacts ── */
static int cmd_package_clean() {
    if (fs::exists("build")) {
        fs::remove_all("build");
        std::cout << "package: cleaned build/\n";
    }
    return 0;
}

/* ── Main entry point for package commands ── */
int run_package_command(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "Usage: aurorac --package <init|install|build|list|clean> [args]\n";
        return 1;
    }

    const std::string& cmd = args[0];
    if (cmd == "init") {
        if (args.size() < 2) {
            std::cerr << "Usage: aurorac --package init <name>\n";
            return 1;
        }
        return cmd_package_init(args[1]);
    } else if (cmd == "install") {
        if (args.size() < 2) {
            std::cerr << "Usage: aurorac --package install <package-name>\n";
            return 1;
        }
        return cmd_package_install(args[1]);
    } else if (cmd == "build") {
        return cmd_package_build();
    } else if (cmd == "list") {
        return cmd_package_list();
    } else if (cmd == "clean") {
        return cmd_package_clean();
    } else {
        std::cerr << "package: unknown command '" << cmd << "'\n";
        return 1;
    }
}
