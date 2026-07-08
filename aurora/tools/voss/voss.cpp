#include "voss.h"

bool g_offline = false;
std::string g_cross_target;
bool g_manual_mode = false;
std::vector<RegistryEntry> g_registries;
std::vector<ForkInfo> g_forks;
TelemetryConfig g_telemetry;
CloudBuildConfig g_cloud_build;
BinaryDistConfig g_binary_dist;
std::vector<DepInsurance> g_insurance;
std::vector<LTSChannel> g_lts_channels;
std::vector<PackageMetrics> g_metrics_cache;
std::vector<Vulnerability> g_vuln_db = {
    {"CVE-2024-0001", "openssl", "<1.1.1", "Buffer overflow in SSL handling", "HIGH"},
    {"CVE-2024-0002", "libcurl", "<7.80", "Memory leak in HTTP/2", "MEDIUM"},
    {"CVE-2024-0003", "zlib", "<1.2.12", "Compression bomb DoS", "LOW"},
    {"CVE-2024-0004", "libpng", "<1.6.38", "Heap buffer overflow", "HIGH"},
    {"CVE-2024-0005", "sqlite", "<3.40", "SQL injection in FTS5", "MEDIUM"}
};
std::vector<AuraRecommendEntry> g_rec_db = {
    {"json", "JSON parsing library", {"simdjson", "nlohmann-json"}},
    {"simdjson", "High-performance JSON parser", {"json", "nlohmann-json"}},
    {"nlohmann-json", "JSON for Modern C++", {"json", "simdjson"}},
    {"http", "HTTP client library", {"libcurl", "cpp-httplib"}},
    {"libcurl", "Multi-protocol file transfer", {"http", "cpp-httplib"}},
    {"cpp-httplib", "Header-only HTTP library", {"http", "libcurl"}},
    {"crypto", "Cryptographic primitives", {"openssl", "libsodium"}},
    {"openssl", "SSL/TLS toolkit", {"crypto", "libsodium"}},
    {"libsodium", "Modern crypto library", {"crypto", "openssl"}},
    {"log", "Logging framework", {"spdlog", "fmtlog"}},
    {"spdlog", "Fast C++ logging", {"log", "fmtlog"}},
    {"fmtlog", "Format-based logging", {"log", "spdlog"}},
    {"test", "Testing framework", {"doctest", "catch2"}},
    {"doctest", "Header-only test framework", {"test", "catch2"}},
    {"catch2", "C++ test framework", {"test", "doctest"}}
};

int main(int argc, char* argv[]) {
    if (argc < 2) return cmd_help();
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) return cmd_help();
    std::string cmd = argv[1];
    load_registries();
    bool verbose = false, auto_yes = false;
    int off = 2;
    g_offline = false;
    while (argc > off && (argv[off][0] == '-')) {
        if (strcmp(argv[off], "-v") == 0 || strcmp(argv[off], "--verbose") == 0) { verbose = true; off++; }
        else if (strcmp(argv[off], "-y") == 0 || strcmp(argv[off], "--yes") == 0) { auto_yes = true; off++; }
        else if (strcmp(argv[off], "--offline") == 0) { g_offline = true; off++; }
        else break;
    }

    /* Core */
    if (cmd == "init") { if (argc < off + 1) { std::cerr << "usage: voss init <name>\n"; return 1; } return cmd_init(argv[off]); }
    if (cmd == "new") { return cmd_new(argc - off, argv + off); }
    if (cmd == "add" || cmd == "a") {
        if (argc < off + 1) { std::cerr << "usage: voss add <package> [version]\n"; return 1; }
        std::string pkg = argv[off];
        std::string ver = (argc > off + 1) ? argv[off + 1] : "";
        return cmd_add(pkg, ver);
    }
    if (cmd == "install" || cmd == "i") {
        if (argc < off + 1) { std::cerr << "usage: voss install <package> [<package> ...]\n"; return 1; }
        if (argc - off >= 2) { std::vector<std::string> pkgs; for (int i = off; i < argc; i++) pkgs.push_back(argv[i]); return cmd_install_parallel(pkgs); }
        return cmd_install(argv[off]);
    }
    if (cmd == "uninstall" || cmd == "remove" || cmd == "rm") { if (argc < off + 1) { std::cerr << "usage: voss uninstall <package>\n"; return 1; } return cmd_uninstall(argv[off]); }
    if (cmd == "lock") return cmd_lock();
    if (cmd == "build" || cmd == "b") return cmd_build(verbose, auto_yes);
    if (cmd == "run" || cmd == "r") return cmd_run(verbose, auto_yes);
    if (cmd == "test" || cmd == "t") return cmd_test(verbose);
    if (cmd == "clean") return cmd_clean();

    /* Analysis */
    if (cmd == "tree") return cmd_tree();
    if (cmd == "verify") return cmd_verify();
    if (cmd == "audit") {
        if (argc > off && (strcmp(argv[off], "--fix") == 0 || strcmp(argv[off], "-f") == 0)) return cmd_audit_fix();
        return cmd_audit();
    }
    if (cmd == "health" || cmd == "score") return cmd_health();
    if (cmd == "snapshot") { std::string label = (argc > off) ? argv[off] : ""; return cmd_snapshot(label); }
    if (cmd == "snapshots") return cmd_snapshots();
    if (cmd == "restore" || cmd == "time-travel" || cmd == "rollback") { if (argc <= off) { std::cerr << "usage: voss restore <snapshot>\n"; return 1; } return cmd_restore(argv[off]); }
    if (cmd == "sandbox") { if (argc <= off) { std::cerr << "usage: voss sandbox <package>\n"; return 1; } return cmd_sandbox(argv[off], verbose); }
    if (cmd == "dead" || cmd == "unused") return cmd_dead_deps();
    if (cmd == "detect") return cmd_detect();
    if (cmd == "trust") { if (argc <= off) { std::cerr << "usage: voss trust <package>\n"; return 1; } return cmd_trust(argv[off]); }
    if (cmd == "migrate") { if (argc <= off) { std::cerr << "usage: voss migrate <pkg>@<version>\n"; return 1; } return cmd_migrate(argv[off]); }
    if (cmd == "why") { if (argc <= off) { std::cerr << "usage: voss why <package>\n"; return 1; } return cmd_why(argv[off]); }
    if (cmd == "export") { if (argc <= off) { std::cerr << "usage: voss export <dir>\n"; return 1; } return cmd_export(argv[off]); }
    if (cmd == "clone" || cmd == "import-project") { if (argc <= off) { std::cerr << "usage: voss clone <dir>\n"; return 1; } return cmd_import_project(argv[off]); }
    if (cmd == "bridge") {
        if (argc < off + 2) { std::cerr << "usage: voss bridge <pypi|npm|cargo|native|--auto> <package> [version] [--target <triple>] [--manual]\n"; return 1; }
        std::string eco = argv[off];
        std::string pkg_arg = argv[off + 1];
        std::string ver = (argc > off + 2) ? argv[off + 2] : "";
        /* Support both "serde@1.0.0" (compact) and "serde 1.0.0" (separate args) */
        std::string pkg;
        std::string ver_from_at;
        parse_pkg_spec(pkg_arg, pkg, ver_from_at);
        if (!ver_from_at.empty()) ver = ver_from_at;
        if (ver.empty() && argc > off + 2) ver = argv[off + 2];
        if (ver.empty()) ver = "latest";
        g_manual_mode = false;
        g_cross_target = "";
        for (int i = off + 3; i < argc; i++) {
            if (strcmp(argv[i], "--target") == 0 && i + 1 < argc) {
                g_cross_target = argv[++i];
            } else if (strcmp(argv[i], "--manual") == 0) {
                g_manual_mode = true;
            }
        }
        if (eco == "--auto") return cmd_bridge_auto(pkg, ver);
        return cmd_bridge(eco, pkg, ver);
    }
    if (cmd == "bind") {
        if (argc <= off) { std::cerr << "usage: voss bind <library> [header.h ...] [options]\n"; return 1; }
        std::string library = argv[off++];
        std::vector<std::string> headers;
        std::string output_dir = "bindings";
        std::string lib_name;
        std::string package;
        std::vector<std::string> inc_dirs;
        std::vector<std::string> defs;
        std::string call_conv;
        bool no_cache = false, bind_verbose = false;
        bool no_macros = false, no_functions = false;
        bool no_structs = false, no_unions = false, no_typedefs = false;
        for (int i = off; i < argc; i++) {
            if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) { if (i + 1 < argc) output_dir = argv[++i]; }
            else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--library-name") == 0) { if (i + 1 < argc) lib_name = argv[++i]; }
            else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--package") == 0) { if (i + 1 < argc) package = argv[++i]; }
            else if (strcmp(argv[i], "-I") == 0) { if (i + 1 < argc) inc_dirs.push_back(argv[++i]); }
            else if (strcmp(argv[i], "-D") == 0) { if (i + 1 < argc) defs.push_back(argv[++i]); }
            else if (strcmp(argv[i], "--cc") == 0) { if (i + 1 < argc) call_conv = argv[++i]; }
            else if (strcmp(argv[i], "--no-cache") == 0) { no_cache = true; }
            else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) { bind_verbose = true; }
            else if (strcmp(argv[i], "--no-macros") == 0) { no_macros = true; }
            else if (strcmp(argv[i], "--no-functions") == 0) { no_functions = true; }
            else if (strcmp(argv[i], "--no-structs") == 0) { no_structs = true; }
            else if (strcmp(argv[i], "--no-unions") == 0) { no_unions = true; }
            else if (strcmp(argv[i], "--no-typedefs") == 0) { no_typedefs = true; }
            else if (argv[i][0] != '-') { headers.push_back(argv[i]); }
        }
        return cmd_bind(library, headers, output_dir, lib_name, package,
                        inc_dirs, defs, call_conv, no_cache, bind_verbose,
                        no_macros, no_functions, no_structs, no_unions, no_typedefs);
    }
    if (cmd == "import") {
        if (argc <= off) { std::cerr << "usage: voss import <npm|pip|cargo> [<fmt> ...]\n"; return 1; }
        std::vector<std::string> fmts; for (int i = off; i < argc; i++) fmts.push_back(argv[i]); return cmd_import_deps(fmts);
    }
    if (cmd == "bench" || cmd == "benchmark") {
        if (argc <= off) { std::cerr << "usage: voss bench <pkg1> [<pkg2> ...]\n"; return 1; }
        std::vector<std::string> pkgs; for (int i = off; i < argc; i++) pkgs.push_back(argv[i]); return cmd_bench(pkgs);
    }
    if (cmd == "recommend" || cmd == "suggest" || cmd == "rec") { if (argc <= off) { std::cerr << "usage: voss recommend <package>\n"; return 1; } return cmd_recommend(argv[off]); }

    /* Security */
    if (cmd == "perms" || cmd == "permissions") {
        reset_perm_session();
        if (argc <= off) return cmd_perms_list();
        std::string sub = argv[off];
        if (sub == "allow" || sub == "a") { if (argc < off + 2) { std::cerr << "usage: voss perms allow <perm>\n"; return 1; } return cmd_perms_allow(argv[off + 1]); }
        if (sub == "deny" || sub == "d") { if (argc < off + 2) { std::cerr << "usage: voss perms deny <perm>\n"; return 1; } return cmd_perms_deny(argv[off + 1]); }
        if (sub == "reset") return cmd_perms_reset();
        if (sub == "review") return cmd_perms_review();
        if (sub == "list" || sub == "ls") return cmd_perms_list();
        std::cerr << "unknown perms command '" << sub << "'\n"; return 1;
    }

    /* Cache */
    if (cmd == "cache") { if (argc > off && strcmp(argv[off], "clean") == 0) return cmd_cache_clean(); return cmd_cache_info(); }

    /* Registry */
    if (cmd == "registry" || cmd == "mirror") {
        load_registries();
        if (argc <= off) return cmd_registry_list();
        std::string sub = argv[off];
        if (sub == "add") { if (argc < off + 3) { std::cerr << "usage: voss registry add <name> <url>\n"; return 1; } return cmd_registry_add(argv[off + 1], argv[off + 2]); }
        if (sub == "remove" || sub == "rm") { if (argc < off + 2) { std::cerr << "usage: voss registry remove <name>\n"; return 1; } return cmd_registry_remove(argv[off + 1]); }
        if (sub == "set" || sub == "primary") { if (argc < off + 2) { std::cerr << "usage: voss registry set <name>\n"; return 1; } return cmd_registry_set(argv[off + 1]); }
        if (sub == "list" || sub == "ls") return cmd_registry_list();
        if (sub == "login" || sub == "auth") {
            std::string reg_spec = (argc > off + 1) ? argv[off + 1] : "";
            return cmd_registry_login(reg_spec);
        }
        if (sub == "register" || sub == "publish" || sub == "push") {
            if (argc < off + 2) { std::cerr << "usage: voss registry register <pkg>@<version> [--registry <spec>]\n"; return 1; }
            std::string pkg_spec = argv[off + 1];
            std::string reg_spec;
            for (int i = off + 2; i < argc; i++) {
                if (strcmp(argv[i], "--registry") == 0 && i + 1 < argc) reg_spec = argv[++i];
            }
            return cmd_registry_register(pkg_spec, reg_spec);
        }
        if (sub == "github-register") {
            if (argc < off + 2) { std::cerr << "usage: voss registry github-register <user/repo> [version]\n"; return 1; }
            std::string ver = (argc > off + 2) ? argv[off + 2] : "";
            return cmd_registry_github_register(argv[off + 1], ver);
        }
        std::cerr << "unknown registry command '" << sub << "'\n"; return 1;
    }

    /* Workspace */
    if (cmd == "workspace" || cmd == "ws") {
        if (argc <= off) { std::cerr << "usage: voss workspace <init|list|build>\n"; return 1; }
        std::string sub = argv[off];
        if (sub == "init") { std::vector<std::string> pkgs; for (int i = off + 1; i < argc; i++) pkgs.push_back(argv[i]); if (pkgs.empty()) { std::cerr << "usage: voss ws init <pkg1> [<pkg2> ...]\n"; return 1; } return cmd_workspace_init(pkgs); }
        if (sub == "list" || sub == "ls") return cmd_workspace_list();
        if (sub == "build" || sub == "b") return cmd_workspace_build(verbose, auto_yes);
        std::cerr << "unknown workspace command '" << sub << "'\n"; return 1;
    }

    /* Roadmap batch 1 */
    if (cmd == "doctor" || cmd == "diagnose") return cmd_doctor();
    if (cmd == "update" || cmd == "upgrade" || cmd == "up") { if (is_frozen()) { std::cerr << "error: dependencies are frozen (run 'voss unfreeze' first)\n"; return 1; } return cmd_update(); }
    if (cmd == "search" || cmd == "find") { if (argc <= off) { std::cerr << "usage: voss search <query>\n"; return 1; } return cmd_search(argv[off]); }
    if (cmd == "publish" || cmd == "push") {
        if (argc > off) {
            std::string tgt = argv[off];
            if (tgt.find("github:") == 0 || tgt.find("gh:") == 0) {
                std::string user, repo, ver;
                bool branch;
                if (parse_github_spec(tgt, user, repo, ver, branch))
                    return cmd_publish_github(user, repo, ver.empty() ? read_manifest(".").version : ver);
                std::cerr << "error: invalid github spec '" << tgt << "'\n";
                return 1;
            }
        }
        return cmd_publish();
    }
    if (cmd == "repair" || cmd == "fix") return cmd_repair();

    /* Package signing */
    if (cmd == "sign") { if (argc <= off) { std::cerr << "usage: voss sign <pkg>[@<version>]\n"; return 1; } return cmd_sign(argv[off]); }
    if (cmd == "verify" || cmd == "verify-pkg") { if (argc <= off) { std::cerr << "usage: voss verify <pkg>[@<version>]\n"; return 1; } return cmd_verify(argv[off]); }

    /* Roadmap batch 2 */
    if (cmd == "graph" || cmd == "depgraph") return cmd_graph();
    if (cmd == "freeze") { if (is_frozen()) { std::cout << "already frozen\n"; return 0; } return cmd_freeze(); }
    if (cmd == "unfreeze") return cmd_unfreeze();
    if (cmd == "outdated" || cmd == "stale") return cmd_outdated();
    if (cmd == "license" || cmd == "licenses") return cmd_license();
    if (cmd == "suggest") { if (argc <= off) { std::cerr << "usage: voss suggest <package>\n"; return 1; } return cmd_suggest(argv[off]); }
    if (cmd == "simulate" || cmd == "dry-run") {
        if (argc <= off) { std::cerr << "usage: voss simulate <pkg1> [<pkg2> ...]\n"; return 1; }
        std::vector<std::string> pkgs; for (int i = off; i < argc; i++) pkgs.push_back(argv[i]); return cmd_simulate(pkgs);
    }
    if (cmd == "mirror") {
        if (argc <= off) { std::cerr << "usage: voss mirror <create|update|status>\n"; return 1; }
        std::string sub = argv[off];
        if (sub == "create" || sub == "init") return cmd_mirror_create();
        if (sub == "update" || sub == "sync") return cmd_mirror_update();
        if (sub == "status" || sub == "info") return cmd_mirror_status();
        std::cerr << "unknown mirror command '" << sub << "'\n"; return 1;
    }
    if (cmd == "dedupe" || cmd == "dedup") return cmd_dedupe();

    /* ── Tier 3.5: AI Compatibility Scanner ── */
    if (cmd == "ai-scan" || cmd == "ai-compat") { if (argc <= off) { std::cerr << "usage: voss ai-scan <pkg>@<version>\n"; return 1; } return cmd_ai_scan(argv[off]); }

    /* ── Tier 4.2: Dependency Forecast ── */
    if (cmd == "forecast" || cmd == "predict") {
        if (argc <= off) return cmd_forecast();
        return cmd_forecast_package(argv[off]);
    }

    /* ── LTS Channels ── */
    if (cmd == "lts") {
        if (argc <= off) return cmd_lts_list();
        std::string sub = argv[off];
        if (sub == "add") { if (argc < off + 4) { std::cerr << "usage: voss lts add <name> <version> <active|maintenance|eol>\n"; return 1; } return cmd_lts_add(argv[off+1], argv[off+2], argv[off+3]); }
        if (sub == "remove" || sub == "rm") { if (argc < off + 2) { std::cerr << "usage: voss lts remove <name>\n"; return 1; } return cmd_lts_remove(argv[off+1]); }
        if (sub == "switch" || sub == "use") { if (argc < off + 2) { std::cerr << "usage: voss lts switch <name>\n"; return 1; } return cmd_lts_switch(argv[off+1]); }
        if (sub == "list" || sub == "ls") return cmd_lts_list();
        std::cerr << "unknown lts command '" << sub << "'\n"; return 1;
    }

    /* ── Reproducible Cloud Builds ── */
    if (cmd == "cloud-build" || cmd == "cloud") {
        if (argc <= off) return cmd_cloud_build_config();
        std::string sub = argv[off];
        if (sub == "init") return cmd_cloud_build_init();
        if (sub == "run" || sub == "build") return cmd_cloud_build_run();
        if (sub == "status" || sub == "info") return cmd_cloud_build_status();
        if (sub == "config") return cmd_cloud_build_config();
        std::cerr << "unknown cloud-build command '" << sub << "'\n"; return 1;
    }

    /* ── Binary Package Distribution ── */
    if (cmd == "binary" || cmd == "bin") {
        if (argc <= off) return cmd_binary_list();
        std::string sub = argv[off];
        if (sub == "package" || sub == "build") return cmd_binary_package();
        if (sub == "install") { if (argc < off + 2) { std::cerr << "usage: voss binary install <pkg>\n"; return 1; } return cmd_binary_install(argv[off+1]); }
        if (sub == "list" || sub == "ls") return cmd_binary_list();
        std::cerr << "unknown binary command '" << sub << "'\n"; return 1;
    }

    /* ── Changelog Analysis ── */
    if (cmd == "changelog" || cmd == "log") { if (argc <= off) { std::cerr << "usage: voss changelog <pkg>\n"; return 1; } return cmd_changelog(argv[off]); }

    /* ── Security Risk Forecast ── */
    if (cmd == "risk-forecast" || cmd == "risk") {
        if (argc <= off) return cmd_risk_forecast();
        return cmd_risk_forecast_package(argv[off]);
    }

    /* ── Package Lifecycle Monitor ── */
    if (cmd == "lifecycle" || cmd == "life") {
        if (argc <= off) return cmd_lifecycle();
        return cmd_lifecycle_package(argv[off]);
    }

    /* ── Package Fork Recovery ── */
    if (cmd == "fork-recover" || cmd == "fork") {
        if (argc <= off) return cmd_fork_list();
        if (argc < off + 3) { std::cerr << "usage: voss fork-recover <original> <fork>\n"; return 1; }
        return cmd_fork_recover(argv[off], argv[off+1]);
    }

    /* ── Dependency Insurance ── */
    if (cmd == "insurance" || cmd == "insure") {
        if (argc <= off) return cmd_dep_insurance();
        std::string sub = argv[off];
        if (sub == "add") { if (argc < off + 3) { std::cerr << "usage: voss insurance add <pkg> <provider>\n"; return 1; } return cmd_dep_insurance_add(argv[off+1], argv[off+2]); }
        if (sub == "check") { if (argc < off + 2) { std::cerr << "usage: voss insurance check <pkg>\n"; return 1; } return cmd_dep_insurance_check(argv[off+1]); }
        if (sub == "list" || sub == "ls") return cmd_dep_insurance();
        std::cerr << "unknown insurance command '" << sub << "'\n"; return 1;
    }

    /* ── Ecosystem Health Dashboard ── */
    if (cmd == "eco-dashboard" || cmd == "eco" || cmd == "dashboard") return cmd_eco_dashboard();

    /* ── Package Telemetry Network ── */
    if (cmd == "telemetry" || cmd == "tel") {
        if (argc <= off) return cmd_telemetry_status();
        std::string sub = argv[off];
        if (sub == "enable") { std::string ep = (argc > off + 1) ? argv[off+1] : "https://telemetry.aurora-pkg.dev/v1"; return cmd_telemetry_enable(ep); }
        if (sub == "disable") return cmd_telemetry_disable();
        if (sub == "submit") return cmd_telemetry_submit();
        if (sub == "status") return cmd_telemetry_status();
        std::cerr << "unknown telemetry command '" << sub << "'\n"; return 1;
    }

    /* ── AI Package Generator ── */
    if (cmd == "ai-gen" || cmd == "generate") {
        if (argc <= off) { std::cerr << "usage: voss ai-gen <description>\n"; return 1; }
        std::string desc; for (int i = off; i < argc; i++) { if (i > off) desc += " "; desc += argv[i]; }
        return cmd_ai_generate(desc);
    }

    /* Info */
    if (cmd == "info") return cmd_info();
    if (cmd == "list" || cmd == "ls") return cmd_list();
    if (cmd == "doc") {
        std::string out_dir;
        bool serve = false;
        for (int i = off; i < argc; i++) {
            if (strcmp(argv[i], "--serve") == 0 || strcmp(argv[i], "-s") == 0) serve = true;
            else if (strcmp(argv[i], "--output") == 0 || strcmp(argv[i], "-o") == 0) { if (i + 1 < argc) out_dir = argv[++i]; }
            else out_dir = argv[i];
        }
        return cmd_doc(out_dir, serve);
    }
    if (cmd == "bundle" || cmd == "bnd") {
        std::string target, format;
        for (int i = off; i < argc; i++) {
            if (strcmp(argv[i], "--target") == 0 || strcmp(argv[i], "-t") == 0) { if (i + 1 < argc) target = argv[++i]; }
            else if (strcmp(argv[i], "--format") == 0 || strcmp(argv[i], "-f") == 0) { if (i + 1 < argc) format = argv[++i]; }
        }
        if (target.empty()) {
            std::cerr << "usage: voss bundle --target <windows|linux|macos|android|ios|all> [--format <format>]\n"
                      << "  Creates platform-specific installer/bundle for distribution\n"
                      << "formats:\n"
                      << "  windows: exe (Inno Setup), msi (WiX)\n"
                      << "  linux: appimage, deb\n"
                      << "  macos: dmg\n"
                      << "  android: apk, aab\n"
                      << "  ios: ipa\n";
            return 1;
        }
        if (format.empty()) {
            if (target == "windows") format = "exe";
            else if (target == "linux") format = "appimage";
            else if (target == "macos") format = "dmg";
            else if (target == "android") format = "apk";
            else if (target == "ios") format = "ipa";
            else { std::cerr << "error: unknown target '" << target << "'\n"; return 1; }
        }
        return cmd_package(target, format);
    }
    if (cmd == "package" || cmd == "pkg") {
        std::string target, format;
        for (int i = off; i < argc; i++) {
            if (strcmp(argv[i], "--target") == 0 || strcmp(argv[i], "-t") == 0) { if (i + 1 < argc) target = argv[++i]; }
            else if (strcmp(argv[i], "--format") == 0 || strcmp(argv[i], "-f") == 0) { if (i + 1 < argc) format = argv[++i]; }
        }
        if (target.empty()) {
            std::cerr << "usage: voss package --target <windows|linux|macos|android|ios|all> --format <format>\n"
                      << "formats:\n"
                      << "  windows: msi, exe\n"
                      << "  linux: appimage, deb\n"
                      << "  macos: dmg\n"
                      << "  android: apk, aab\n"
                      << "  ios: ipa\n";
            return 1;
        }
        return cmd_package(target, format);
    }
    if (cmd == "theme") return cmd_theme(argc - 2, argv + 2);
    if (cmd == "help" || cmd == "--help" || cmd == "-h") return cmd_help();

    std::cerr << "unknown command '" << cmd << "'\n";
    std::cerr << "usage: voss <command> --help for details\n";
    return 1;
}
