#include "compiler/lexer.hpp"
#include "compiler/parser.hpp"
#include "compiler/ownership.hpp"
#include "compiler/memory_analyzer.hpp"
#include "compiler/optimized_codegen.hpp"
#include "compiler/benchmark_suite.hpp"
#include "compiler/codegen.hpp"
#include "compiler/aurora_optimizer.hpp"
#include "compiler/package_and_doc.hpp"
#include "compiler/ir/ast_to_ir.hpp"
#include "compiler/ir/ir_lowering.hpp"
#include "compiler/ir/ir_optimizer.hpp"
#include "runtime/crash.h"

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/StandardInstrumentations.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/Transforms/Scalar/IndVarSimplify.h>
#include <llvm/Transforms/Scalar/LoopUnrollPass.h>
#include <llvm/Transforms/Scalar/SimplifyCFG.h>
#include <llvm/Bitcode/BitcodeWriter.h>

#include <fstream>

#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>
#include <filesystem>
#include <cstdlib>
#include <array>
#ifdef _WIN32
#  include <windows.h>
#else
#  include <unistd.h>
#  include <sys/wait.h>
#  include <cerrno>
#endif

namespace fs = std::filesystem;

static bool verbose = false;

/* ── Safe process execution helper (no shell) ── */
#ifdef _WIN32
static std::string windows_quote_arg(const std::string& s) {
    std::string q = "\"";
    for (char c : s) {
        if (c == '"') q += "\\\"";
        else q += c;
    }
    q += "\"";
    return q;
}
#endif

static int run_process(const std::string& exe, const std::vector<std::string>& args) {
#ifdef _WIN32
    std::string cmdline = windows_quote_arg(exe);
    for (auto& a : args)
        cmdline += " " + windows_quote_arg(a);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    BOOL ok = CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, FALSE,
                             CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (!ok) return -1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)exit_code;
#else
    pid_t pid = fork();
    if (pid == 0) {
        std::vector<char*> argv;
        argv.reserve(args.size() + 2);
        argv.push_back(const_cast<char*>(exe.c_str()));
        for (auto& a : args)
            argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        execvp(exe.c_str(), argv.data());
        _exit(127);
    } else if (pid < 0) {
        return -1;
    }
    int status;
    pid_t w;
    do {
        w = waitpid(pid, &status, 0);
    } while (w == -1 && errno == EINTR);
    if (w == -1) return -1;
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return -1;
#endif
}

/* ── Read entire file into string ── */
static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        throw std::runtime_error("aurora: cannot open file: " + path);
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

/* ── Find import file by searching paths ── */
static std::string find_import_file(const std::string& name, const std::string& source_dir, const std::string& exe_dir) {
    /* Handle ecosystem: prefix (libc:math, pypi:lodash, npm:express, cargo:serde) */
    size_t colon = name.find(':');
    if (colon != std::string::npos) {
        std::string prefix = name.substr(0, colon);
        std::string rest = name.substr(colon + 1);
        if (prefix == "libc") {
            /* libc:name — look in libc/ directory */
            auto check_libc = [&](const fs::path& base) -> std::string {
                auto p = base / "libc" / (rest + ".auf");
                return fs::exists(p) ? fs::absolute(p).string() : "";
            };
            auto r = check_libc(fs::current_path());
            if (!r.empty()) return r;
            r = check_libc(fs::path(exe_dir));
            if (!r.empty()) return r;
            r = check_libc(fs::path(exe_dir).parent_path());
            if (!r.empty()) return r;
            r = check_libc(fs::path(exe_dir).parent_path().parent_path());
            if (!r.empty()) return r;
        } else if (prefix == "pypi" || prefix == "npm" || prefix == "cargo" || prefix == "native") {
            /* pypi|npm|cargo|native:name — check for cached bridge */
            auto check_eco = [&](const fs::path& base) -> std::string {
                /* new-style: packages/bridges/npm/name_npm/name.au */
                auto p1 = base / "packages" / "bridges" / prefix / (rest + "_" + prefix) / (rest + ".auf");
                if (fs::exists(p1)) return fs::absolute(p1).string();
                /* new-style: lodash_npm/ */
                auto p2 = base / (rest + "_" + prefix) / (rest + ".auf");
                if (fs::exists(p2)) return fs::absolute(p2).string();
                /* old-style: lodash_npm_bridge/ */
                auto p3 = base / (rest + "_" + prefix + "_bridge") / (rest + ".auf");
                return fs::exists(p3) ? fs::absolute(p3).string() : "";
            };
            auto r = check_eco(fs::current_path());
            if (!r.empty()) return r;
            r = check_eco(fs::path(exe_dir));
            if (!r.empty()) return r;
            r = check_eco(fs::path(exe_dir).parent_path());
            if (!r.empty()) return r;
            r = check_eco(fs::path(exe_dir).parent_path().parent_path());
            if (!r.empty()) return r;
        }
    }
    /* Try with .au extension directly */
    if (fs::exists(name + ".auf")) return fs::absolute(name + ".auf").string();
    /* Try relative to source directory */
    auto src_path_au = fs::path(source_dir) / (name + ".auf");
    if (fs::exists(src_path_au)) return fs::absolute(src_path_au).string();
    /* Try relative to libc/ in various locations */
    auto check_libc = [&](const fs::path& base) -> std::string {
        auto p = base / "libc" / (name + ".auf");
        return fs::exists(p) ? fs::absolute(p).string() : "";
    };
    /* libc/ in cwd */
    auto r = check_libc(fs::current_path());
    if (!r.empty()) return r;
    /* libc/ next to exe */
    r = check_libc(fs::path(exe_dir));
    if (!r.empty()) return r;
    /* libc/ one level above exe */
    r = check_libc(fs::path(exe_dir).parent_path());
    if (!r.empty()) return r;
    /* libc/ two levels above exe (project root when exe is in build/Release/) */
    r = check_libc(fs::path(exe_dir).parent_path().parent_path());
    if (!r.empty()) return r;
    /* Try packages/<name>/src/main.aura (for voss-installed packages) */
    auto check_pkg = [&](const fs::path& base) -> std::string {
        auto p = base / "packages" / name / "src" / "main.aura";
        return fs::exists(p) ? fs::absolute(p).string() : "";
    };
    r = check_pkg(fs::current_path());
    if (!r.empty()) return r;
    r = check_pkg(fs::path(exe_dir));
    if (!r.empty()) return r;
    r = check_pkg(fs::path(exe_dir).parent_path());
    if (!r.empty()) return r;
    r = check_pkg(fs::path(exe_dir).parent_path().parent_path());
    if (!r.empty()) return r;

    /* Check for ecosystem bridge directories (<name>_<eco>/<name>.au or <name>_<eco>_bridge/<name>.auf) */
    auto check_eco_bridge = [&](const fs::path& base) -> std::string {
        const char* ecosystems[] = {"pypi", "npm", "cargo", "native"};
        for (auto eco : ecosystems) {
            /* new-style: packages/bridges/npm/name_npm/name.au */
            auto p1 = base / "packages" / "bridges" / eco / (name + "_" + eco) / (name + ".auf");
            if (fs::exists(p1)) return fs::absolute(p1).string();
            /* new-style: lodash_npm/ */
            auto p2 = base / (name + "_" + eco) / (name + ".auf");
            if (fs::exists(p2)) return fs::absolute(p2).string();
            /* old-style: lodash_npm_bridge/ */
            auto p3 = base / (name + "_" + eco + "_bridge") / (name + ".auf");
            if (fs::exists(p3)) return fs::absolute(p3).string();
        }
        return "";
    };
    r = check_eco_bridge(fs::current_path());
    if (!r.empty()) return r;
    r = check_eco_bridge(fs::path(exe_dir));
    if (!r.empty()) return r;
    r = check_eco_bridge(fs::path(exe_dir).parent_path());
    if (!r.empty()) return r;
    r = check_eco_bridge(fs::path(exe_dir).parent_path().parent_path());
    if (!r.empty()) return r;

    return "";
}

/* ── Auto-resolve import via voss bridge ── */
static std::string resolve_via_voss(const std::string& name, const std::string& exe_dir) {
    if (verbose) std::cerr << "[voss] auto-resolving: " << name << "\n";

    /* Find voss executable */
    std::string voss_exe = (fs::path(exe_dir) / "voss.exe").string();
    if (!fs::exists(voss_exe)) {
        voss_exe = (fs::path(exe_dir) / "voss").string();
        if (!fs::exists(voss_exe)) {
            voss_exe = "voss.exe";
            if (!fs::exists(voss_exe)) {
                voss_exe = "voss";
                if (!fs::exists(voss_exe)) {
                    std::cerr << "[voss] ERROR: voss executable not found\n";
                    std::cerr << "[voss]   install voss or check PATH\n";
                    return "";
                }
            }
        }
    }
    if (verbose) std::cerr << "[voss] using: " << voss_exe << "\n";

    /* Determine ecosystem from prefix (eco:pkg) or use --auto */
    std::string eco_cmd = "--auto";
    std::string pkg_name = name;
    size_t colon = name.find(':');
    if (colon != std::string::npos) {
        std::string prefix = name.substr(0, colon);
        if (prefix == "pypi" || prefix == "npm" || prefix == "cargo" || prefix == "libc" || prefix == "native") {
            eco_cmd = prefix;
            pkg_name = name.substr(colon + 1);
        }
    }

    /* Sanitize package name: allow only alphanumeric, underscore, hyphen, dot */
    auto safe_pkg = pkg_name;
    for (auto& ch : safe_pkg) {
        if (!std::isalnum((unsigned char)ch) && ch != '_' && ch != '-' && ch != '.')
            ch = '_';
    }

    /* Build command: voss bridge <eco> <pkg> */
    std::string cmd = "\"" + voss_exe + "\" bridge " + eco_cmd + " \"" + safe_pkg + "\"";
    if (verbose) std::cerr << "[voss] running: " << cmd << "\n";

#ifdef _WIN32
    /* Run voss and capture output */
    std::string result;
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE hRead, hWrite;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return "";
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.hStdOutput = hWrite;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.dwFlags |= STARTF_USESTDHANDLES;
    PROCESS_INFORMATION pi = {};
    BOOL ok = CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (!ok) {
        std::cerr << "[voss] ERROR: failed to start voss.exe (is it installed?)\n";
        std::cerr << "[voss]   run: voss bridge " << eco_cmd << " \"" << pkg_name << "\"\n";
        CloseHandle(hRead); CloseHandle(hWrite); return "";
    }
    /* Wait with 120s timeout */
    DWORD wait_rc = WaitForSingleObject(pi.hProcess, 120000);
    if (wait_rc == WAIT_TIMEOUT) {
        std::cerr << "[voss] ERROR: bridge timed out after 120s for " << name << "\n";
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        CloseHandle(hRead); CloseHandle(hWrite);
        return "";
    }
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hWrite);

    /* Read output */
    char buf[4096];
    DWORD read;
    while (ReadFile(hRead, buf, sizeof(buf) - 1, &read, nullptr) && read > 0) {
        buf[read] = 0;
        result += buf;
    }
    CloseHandle(hRead);
#else
    /* POSIX: fork/exec with pipe capture (safe, no shell) */
    std::string result;
    int pipefd[2];
    if (pipe(pipefd) < 0) return "";

    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        char* const argv[] = {
            const_cast<char*>(voss_exe.c_str()),
            const_cast<char*>("bridge"),
            const_cast<char*>(eco_cmd.c_str()),
            const_cast<char*>(safe_pkg.c_str()),
            nullptr
        };
        execvp(voss_exe.c_str(), argv);
        _exit(127);
    } else if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return "";
    }

    close(pipefd[1]);
    char buf[4096];
    ssize_t n;
    do {
        n = read(pipefd[0], buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = 0;
            result += buf;
        }
    } while (n > 0 || (n == -1 && errno == EINTR));
    close(pipefd[0]);

    int status;
    pid_t w;
    do {
        w = waitpid(pid, &status, 0);
    } while (w == -1 && errno == EINTR);
    int exit_code = (w != -1 && WIFEXITED(status)) ? WEXITSTATUS(status) : -1;
#endif

    if (exit_code != 0) {
        std::cerr << "[voss] ERROR: bridge auto-resolve failed for " << name
                  << " (exit " << exit_code << ")\n";
        std::cerr << "[voss]   run manually: voss bridge " << eco_cmd << " \"" << pkg_name << "\"\n";
        return "";
    }

    /* Parse output for BRIDGE_AU=... path */
    std::string au_path, dll_path, bridge_dir, ecosystem;
    std::istringstream ss(result);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.rfind("BRIDGE_AU=", 0) == 0) {
            au_path = line.substr(10);
            if (!au_path.empty() && au_path.back() == '\r') au_path.pop_back();
        } else if (line.rfind("BRIDGE_DLL=", 0) == 0) {
            dll_path = line.substr(11);
            if (!dll_path.empty() && dll_path.back() == '\r') dll_path.pop_back();
        } else if (line.rfind("BRIDGE_DIR=", 0) == 0) {
            bridge_dir = line.substr(11);
            if (!bridge_dir.empty() && bridge_dir.back() == '\r') bridge_dir.pop_back();
        } else if (line.rfind("BRIDGE_ECOSYSTEM=", 0) == 0) {
            ecosystem = line.substr(17);
            if (!ecosystem.empty() && ecosystem.back() == '\r') ecosystem.pop_back();
        }
    }

    if (au_path.empty() || bridge_dir.empty()) {
        std::cerr << "[voss] ERROR: bridge output missing BRIDGE_AU/BRIDGE_DIR\n";
        std::cerr << "[voss]   raw output:\n" << result << "\n";
        return "";
    }

    if (verbose) std::cerr << "[voss] bridge created: " << ecosystem << " → " << name << "\n";
    if (dll_path.empty() || !fs::exists(dll_path)) {
        if (verbose) std::cerr << "[voss] note: bridge DLL not found, runtime resolve may fall back\n";
    } else {
        if (verbose) std::cerr << "[voss] ✅ bridge DLL: " << dll_path << "\n";
    }

    return au_path;
}

/* ── Find header file (for auto-bindgen) ── */
static std::string find_header_file(const std::string& name, const std::string& source_dir, const std::string& exe_dir) {
    if (fs::exists(name)) return fs::absolute(name).string();
    if (fs::exists(name + ".h")) return fs::absolute(name + ".h").string();
    auto src_path = fs::path(source_dir) / name;
    if (fs::exists(src_path)) return fs::absolute(src_path).string();
    auto src_path_h = fs::path(source_dir) / (name + ".h");
    if (fs::exists(src_path_h)) return fs::absolute(src_path_h).string();
    /* Check libc/ in various locations */
    auto check_libc = [&](const fs::path& base) -> std::string {
        auto p = base / "libc" / (name + ".h");
        return fs::exists(p) ? fs::absolute(p).string() : "";
    };
    auto r = check_libc(fs::current_path());
    if (!r.empty()) return r;
    r = check_libc(fs::path(exe_dir));
    if (!r.empty()) return r;
    r = check_libc(fs::path(exe_dir).parent_path());
    if (!r.empty()) return r;
    return "";
}

/* ── Auto-generate FFI bindings via aurora-bindgen ── */
static std::string auto_bindgen(const std::string& header_path, const std::string& exe_dir) {
    fs::path hdr(header_path);
    fs::path out_path = hdr.parent_path() / (hdr.stem().string() + ".auf");

    std::string bindgen_exe = (fs::path(exe_dir) / "aurora_bindgen.exe").string();
    if (!fs::exists(bindgen_exe)) {
        bindgen_exe = (fs::path(exe_dir) / "aurora-bindgen.exe").string();
        if (!fs::exists(bindgen_exe)) {
            bindgen_exe = "aurora_bindgen.exe";
        }
    }

    if (verbose) std::cerr << "[auto-bindgen] " << hdr.filename().string() << " → " << out_path.filename().string() << "\n" << std::flush;

    /* Sanitize: ensure header_path doesn't contain shell metacharacters */
    auto sanitized_path = header_path;
    for (auto& ch : sanitized_path) {
        if (ch == '|' || ch == '&' || ch == ';' || ch == '$' || ch == '`' ||
            ch == '(' || ch == ')' || ch == '<' || ch == '>' || ch == '\'' || ch == '"')
            ch = '_';
    }

#ifdef _WIN32
    /* Build command line args */
    std::string args = std::string("\"") + bindgen_exe + "\" \"" + sanitized_path
                     + "\" -o \"" + out_path.string() + "\"";

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    BOOL ok = CreateProcessA(nullptr, args.data(), nullptr, nullptr, FALSE,
                             CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (!ok) {
        std::cerr << "aurora: bindgen failed for " << header_path << "\n";
        return "";
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#else
    std::vector<std::string> bindgen_args = { sanitized_path, "-o", out_path.string() };
    int exit_code = run_process(bindgen_exe, bindgen_args);
#endif

    if (exit_code != 0) {
        std::cerr << "aurora: bindgen failed for " << header_path << " (exit " << exit_code << ")\n";
        return "";
    }
    if (!fs::exists(out_path)) return "";
    return fs::absolute(out_path).string();
}

/* ── Resolve imports in AST (internal, with cycle detection) ── */
static ASTNode::Ptr resolve_imports_impl(ASTNode::Ptr root, const std::string& source_dir,
                                         const std::string& exe_dir,
                                         std::vector<std::string>& resolving,
                                         std::unordered_set<std::string>& resolving_fast) {
    if (!root) return nullptr;

    std::vector<ASTNode::Ptr> nodes;
    while (root) {
        auto next = std::move(root->next);
        nodes.push_back(std::move(root));
        root = std::move(next);
    }

    ASTNode::Ptr head = nullptr;
    ASTNode* tail = nullptr;

    for (auto& node : nodes) {
        if (node->type == NodeType::Import) {
            std::string path = node->value;
            if (verbose) std::cerr << "[import] " << path << "\n" << std::flush;
            std::string found = find_import_file(path, source_dir, exe_dir);
            if (found.empty()) {
                /* Auto-bindgen: quoted header import or convention-based .h lookup */
                std::string header_path = find_header_file(path, source_dir, exe_dir);
                if (!header_path.empty()) {
                    found = auto_bindgen(header_path, exe_dir);
                }
            }
            if (found.empty()) {
                /* Ecosystem bridge: try voss bridge --auto to resolve */
                found = resolve_via_voss(path, exe_dir);
            }
            if (found.empty()) {
                throw std::runtime_error("aurora: cannot find import: " + path);
            }

            /* Normalize path for cycle detection */
            std::string norm_path = fs::absolute(found).string();

            if (!resolving_fast.insert(norm_path).second) {
                /* Build the resolution chain for the error message */
                std::string chain;
                for (size_t i = 0; i < resolving.size(); ++i) {
                    if (i > 0) chain += " → ";
                    chain += resolving[i];
                }
                chain += " → " + norm_path;
                throw std::runtime_error("aurora: circular import detected:\n  " + chain);
            }
            resolving.push_back(norm_path);

            /* TODO: support namespace/module scoping for imports —
               e.g., import com.example.foo should resolve within a
               module com.example { ... } scope, not the global scope. */
            std::string src = read_file(found);
            Lexer lexer;
            auto lines = lexer.lex(src);
            Parser parser(lines);
            auto imported = parser.parse();
            imported = resolve_imports_impl(std::move(imported), source_dir, exe_dir, resolving, resolving_fast);

            resolving.pop_back();
            resolving_fast.erase(norm_path);

            if (imported) {
                if (!head) {
                    head = std::move(imported);
                    tail = head.get();
                    while (tail->next) tail = tail->next.get();
                } else {
                    tail->next = std::move(imported);
                    while (tail->next) tail = tail->next.get();
                }
            }
        } else {
            node->next = nullptr;
            if (!head) {
                head = std::move(node);
                tail = head.get();
            } else {
                tail->next = std::move(node);
                tail = tail->next.get();
            }
        }
    }

    return head;
}

/* ── Resolve imports in AST (public, with cycle detection) ── */
static ASTNode::Ptr resolve_imports(ASTNode::Ptr root, const std::string& source_dir, const std::string& exe_dir) {
    std::vector<std::string> resolving;
    std::unordered_set<std::string> resolving_fast;
    return resolve_imports_impl(std::move(root), source_dir, exe_dir, resolving, resolving_fast);
}

/* ── Emit object file using LLVM TargetMachine ── */
static bool emit_object_file(llvm::Module* module, const std::string& obj_path,
                             bool fast_math = false, bool use_lto = false) {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    std::string target_triple = llvm::sys::getProcessTriple();
    module->setTargetTriple(target_triple);

    std::string error;
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(target_triple, error);
    if (!target) {
        std::cerr << "aurora: " << error << "\n";
        return false;
    }

    llvm::TargetOptions options;
    options.AllowFPOpFusion = llvm::FPOpFusion::Standard;
    if (fast_math) {
        options.AllowFPOpFusion = llvm::FPOpFusion::Fast;
        options.UnsafeFPMath = true;
        options.NoInfsFPMath = true;
        options.NoNaNsFPMath = true;
    }

    if (use_lto) {
        /* Emit LLVM bitcode for LTO instead of native object */
        std::error_code ec;
        llvm::raw_fd_ostream bc_file(obj_path, ec, llvm::sys::fs::OF_None);
        if (ec) {
            std::cerr << "aurora: cannot write bitcode file: " << ec.message() << "\n";
            return false;
        }
        llvm::WriteBitcodeToFile(*module, bc_file);
        bc_file.close();
        return true;
    }

    /* Detect native CPU and features */
    std::string cpu = llvm::sys::getHostCPUName().str();
    std::string features_str;
#if LLVM_VERSION_MAJOR >= 19
    auto host_features = llvm::sys::getHostCPUFeatures();
    if (!host_features.empty()) {
        for (const auto& f : host_features) {
            if (!features_str.empty()) features_str += ",";
            features_str += f.second ? "+" : "-";
            features_str += f.first();
        }
    }
#else
    llvm::StringMap<bool, llvm::MallocAllocator> host_features;
    if (llvm::sys::getHostCPUFeatures(host_features)) {
        for (const auto& f : host_features) {
            if (!features_str.empty()) features_str += ",";
            features_str += f.second ? "+" : "-";
            features_str += f.first();
        }
    }
#endif

    auto* target_machine = target->createTargetMachine(
        target_triple, cpu, features_str, options, llvm::Reloc::PIC_);

    if (!target_machine) {
        std::cerr << "aurora: could not create target machine\n";
        return false;
    }

    module->setDataLayout(target_machine->createDataLayout());

    std::error_code ec;
    llvm::raw_fd_ostream obj_file(obj_path, ec, llvm::sys::fs::OF_None);
    if (ec) {
        std::cerr << "aurora: cannot write object file: " << ec.message() << "\n";
        delete target_machine;
        return false;
    }

    llvm::legacy::PassManager pass_manager;
    if (target_machine->addPassesToEmitFile(pass_manager, obj_file, nullptr,
            llvm::CodeGenFileType::ObjectFile)) {
        std::cerr << "aurora: target machine cannot emit object file\n";
        delete target_machine;
        return false;
    }

    pass_manager.run(*module);
    delete target_machine;
    return true;
}

#ifdef _WIN32
/* ── Auto-detect MSVC & Windows SDK library paths (Windows only) ── */
static std::vector<std::string> detect_msvc_lib_paths() {
    std::vector<std::string> paths;
    char buf[512];

    /* Try vswhere via PATH first, then via ProgramFiles env vars */
    std::string vswhere_cmd = "vswhere -latest -property installationPath -format value 2>nul";
    FILE* pipe = _popen(vswhere_cmd.c_str(), "r");
    if (!pipe) {
        const char* pf[] = {
            std::getenv("ProgramFiles(x86)"),
            std::getenv("ProgramFiles"),
            std::getenv("ProgramW6432"),
        };
        for (auto p : pf) {
            if (!p) continue;
            std::string cmd = std::string("\"") + p +
                "\\Microsoft Visual Studio\\Installer\\vswhere.exe\" "
                "-latest -property installationPath -format value 2>nul";
            pipe = _popen(cmd.c_str(), "r");
            if (pipe) break;
        }
    }
    std::string vs_install;
    if (pipe) {
        if (fgets(buf, sizeof(buf), pipe)) {
            vs_install = buf;
            while (!vs_install.empty() && (vs_install.back() == '\n' || vs_install.back() == '\r'))
                vs_install.pop_back();
        }
        _pclose(pipe);
    }

    if (vs_install.empty()) {
        const char* pf[] = {
            std::getenv("ProgramFiles"),
            std::getenv("ProgramFiles(x86)"),
            std::getenv("ProgramW6432"),
        };
        for (auto p : pf) {
            if (!p) continue;
            std::string base = std::string(p) + "\\Microsoft Visual Studio";
            const char* editions[] = { "2022\\Community", "2022\\BuildTools", "2022\\Professional", "2022\\Enterprise",
                                       "2019\\Community", "2019\\BuildTools", "2019\\Professional", "2019\\Enterprise" };
            for (auto e : editions) {
                std::string candidate = base + "\\" + e;
                if (fs::exists(candidate)) { vs_install = candidate; break; }
            }
            if (!vs_install.empty()) break;
        }
    }

    if (vs_install.empty()) return paths;

    std::string msvc_root = vs_install + "\\VC\\Tools\\MSVC";
    if (fs::exists(msvc_root)) {
        std::string msvc_ver;
        for (auto& entry : fs::directory_iterator(msvc_root)) {
            if (entry.is_directory()) {
                msvc_ver = entry.path().filename().string();
                break;
            }
        }
        if (!msvc_ver.empty()) {
            std::string lib64 = msvc_root + "\\" + msvc_ver + "\\lib\\x64";
            if (fs::exists(lib64)) paths.push_back(lib64);
        }
    }

    std::string kits_root;
    const char* kit_pf[] = {
        std::getenv("ProgramFiles(x86)"),
        std::getenv("ProgramFiles"),
        std::getenv("ProgramW6432"),
    };
    for (auto p : kit_pf) {
        if (!p) continue;
        kits_root = std::string(p) + "\\Windows Kits\\10\\Lib";
        if (fs::exists(kits_root)) break;
    }
    if (fs::exists(kits_root)) {
        std::string sdk_ver;
        for (auto& entry : fs::directory_iterator(kits_root)) {
            if (entry.is_directory()) {
                sdk_ver = entry.path().filename().string();
                break;
            }
        }
        if (!sdk_ver.empty()) {
            std::string ucrt = kits_root + "\\" + sdk_ver + "\\ucrt\\x64";
            std::string um   = kits_root + "\\" + sdk_ver + "\\um\\x64";
            if (fs::exists(ucrt)) paths.push_back(ucrt);
            if (fs::exists(um))   paths.push_back(um);
        }
    }

    return paths;
}
#endif

/* ── Locate lld-link on Windows by searching common install paths ── */
#ifdef _WIN32
static std::string find_lld_link() {
    /* 1) LLVM_HOME env var (most specific user override) */
    const char* llvm_home = std::getenv("LLVM_HOME");
    if (llvm_home) {
        std::string p = std::string(llvm_home) + "\\bin\\lld-link.exe";
        if (fs::exists(p)) return p;
    }

    /* 2) Search PATH via where.exe */
    {
        FILE* pipe = _popen("where lld-link 2>nul", "r");
        if (pipe) {
            char buf[512];
            if (fgets(buf, sizeof(buf), pipe)) {
                std::string path = buf;
                while (!path.empty() && (path.back() == '\n' || path.back() == '\r'))
                    path.pop_back();
                if (fs::exists(path)) { _pclose(pipe); return path; }
            }
            _pclose(pipe);
        }
    }

    /* 3) ProgramFiles env var based paths */
    const char* pf[] = {
        std::getenv("ProgramFiles"),
        std::getenv("ProgramFiles(x86)"),
        std::getenv("ProgramW6432"),
    };
    for (auto p : pf) {
        if (!p) continue;
        std::string c = std::string(p) + "\\LLVM\\bin\\lld-link.exe";
        if (fs::exists(c)) return c;
    }

    /* 4) Hardcoded fallback paths (last resort) */
    std::vector<std::string> fallbacks = {
        "C:\\LLVM\\bin\\lld-link.exe",
        "C:\\Program Files\\LLVM\\bin\\lld-link.exe",
        "C:\\Program Files (x86)\\LLVM\\bin\\lld-link.exe",
    };
    for (auto& c : fallbacks) {
        if (fs::exists(c)) return c;
    }

    /* 5) Bare name — let the OS resolve via PATH in CreateProcess */
    return "lld-link";
}
#endif

/* ── Link object file into executable (cross-platform) ── */
static bool link_exe(const std::string& obj_path, const std::string& exe_path,
                     const std::string& exe_dir,
                     const std::vector<std::string>& link_libs,
                     const std::vector<std::string>& lib_paths,
                     bool use_lto = false) {
    std::string linker;
    std::vector<std::string> args;

#ifdef _WIN32
    /* ── Windows: lld-link (COFF) ── */
    std::string rt_lib = exe_dir + "/aurora_runtime.lib";
    if (!fs::exists(rt_lib))
        rt_lib = exe_dir + "/lib/aurora_runtime.lib";
    if (!fs::exists(rt_lib))
        rt_lib = exe_dir + "/../build/Release/aurora_runtime.lib";
    if (!fs::exists(rt_lib))
        rt_lib = "aurora_runtime.lib";

    linker = find_lld_link();
    args = { obj_path, rt_lib,
             "/OUT:" + exe_path, "/NOLOGO",
             "/ENTRY:mainCRTStartup", "/SUBSYSTEM:CONSOLE" };
    if (use_lto) args.push_back("/LTCG");

    for (auto& lp : lib_paths)
        args.push_back("/LIBPATH:" + lp);

    if (lib_paths.empty()) {
        auto msvc_paths = detect_msvc_lib_paths();
        for (auto& p : msvc_paths)
            args.push_back("/LIBPATH:" + p);
    }

    args.push_back("msvcrt.lib");
    for (auto& lib : link_libs) {
        if (lib.size() > 4 && lib.substr(lib.size() - 4) == ".lib")
            args.push_back(lib);
        else
            args.push_back(lib + ".lib");
    }

#elif defined(__APPLE__)
    /* ── macOS: ld64.lld ── */
    std::string rt_lib = exe_dir + "/libaurora_runtime.a";
    if (!fs::exists(rt_lib))
        rt_lib = exe_dir + "/../build/libaurora_runtime.a";
    if (!fs::exists(rt_lib))
        rt_lib = "libaurora_runtime.a";

    linker = "ld64.lld";
    args = { "-o", exe_path, obj_path, rt_lib };
    if (use_lto) args.push_back("--lto-O3");
    for (auto& lp : lib_paths)
        args.push_back("-L" + lp);
    for (auto& lib : link_libs)
        args.push_back("-l" + lib);
    args.push_back("-lc");
    args.push_back("-lc++");

#else
    /* ── Linux/Unix: ld.lld (ELF) ── */
    std::string rt_lib = exe_dir + "/libaurora_runtime.a";
    if (!fs::exists(rt_lib))
        rt_lib = exe_dir + "/../build/libaurora_runtime.a";
    if (!fs::exists(rt_lib))
        rt_lib = "libaurora_runtime.a";

    linker = "ld.lld";
    args = { "-o", exe_path, obj_path, rt_lib };
    if (use_lto) args.push_back("--lto-O3");
    for (auto& lp : lib_paths)
        args.push_back("-L" + lp);
    for (auto& lib : link_libs)
        args.push_back("-l" + lib);
    args.push_back("-lc");
    args.push_back("-lc++");
    args.push_back("-lm");
#endif

    std::cout << "aurora: linking " << exe_path << "\n";
    int ret = run_process(linker, args);
    if (ret != 0) {
        std::cerr << "aurora: link failed (exit " << ret << ")\n";
        return false;
    }
    return true;
}

int main(int argc, char** argv) {
    /* Install crash handler (SEH on Windows, signal on Unix).
       Catches crashes and writes dump files to output/crash/. */
    aurora_install_crash_handler();

    /* Determine executable directory for finding libc/ bindings */
    std::string exe_dir;
    if (argc > 0 && argv[0]) {
        exe_dir = fs::path(argv[0]).parent_path().string();
    }

    bool emit_ir = false;
    bool emit_obj = false;
    std::string output_path;                /* -o <file> */
    std::vector<std::string> link_libs;     /* -l <lib> */
    std::vector<std::string> lib_paths;     /* -L <path> */
    bool run_jit = false;
    bool show_memory_report = false;
    bool show_lifetime_report = false;
    bool show_ownership_report = false;
    bool show_alias_graph = false;
    bool show_allocation_report = false;
    bool show_profiler_report = false;
    bool show_detailed_report = false;
    bool show_performance_report = false;
    bool show_percentage_report = false;
    bool export_json = false;
    bool export_csv = false;
    bool use_aurora_ir = false;
    bool use_optimized_codegen = false;
    bool run_benchmarks = false;
    bool repl_mode = false;
    bool doc_mode = false;
    std::string doc_output;
    bool package_mode = false;
    std::vector<std::string> package_args;
    std::string source_path;
    int opt_level = 2;             /* -O flag: 0-3, default 2 */
    bool opt_size = false;         /* -Os: optimize for size */
    bool opt_size_aggressive = false; /* -Oz: aggressively optimize for size */
    bool fast_math = false;        /* -ffast-math: enable unsafe FP optimizations */
    bool use_lto = false;          /* -flto: enable link-time optimization */
    bool enable_coverage = false;  /* --coverage: enable code coverage tracing */

    /* Check for --repl, --doc, --package first */
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--repl") repl_mode = true;
        if (arg == "--doc") doc_mode = true;
        if (arg == "--package") {
            package_mode = true;
            /* Collect remaining args for package command */
            for (int j = i + 1; j < argc; j++)
                package_args.push_back(argv[j]);
            break;
        }
    }

    /* For REPL/doc/package mode, source file is optional */
    if (!repl_mode && !doc_mode && !package_mode && argc < 2) {
        std::cerr << "Usage: aurorac <source.aura> [--emit-obj] [-o output] [-l lib] [-L path]\n";
        std::cerr << "       aurorac --repl\n";
        std::cerr << "       aurorac --doc <source.aura> [output.html]\n";
        std::cerr << "       aurorac --package <init|install|build|list|clean>\n";
        std::cerr << "\n";
        std::cerr << "Options:\n";
        std::cerr << "  --emit-ir       Print LLVM IR to stdout\n";
        std::cerr << "  --emit-obj      Emit object file (and link if -l used)\n";
        std::cerr << "  -o <file>       Output path (.obj or .exe)\n";
        std::cerr << "  -l <lib>        Library to link against (e.g. -l user32)\n";
        std::cerr << "  -L <path>       Library search path\n";
        std::cerr << "  -O0/-O1/-O2/-O3  Optimization level (default: -O3)\n";
        std::cerr << "  -Os             Optimize for size\n";
        std::cerr << "  -Oz             Aggressively optimize for size\n";
        std::cerr << "  -ffast-math     Enable unsafe floating-point optimizations\n";
        std::cerr << "  -flto           Enable link-time optimization (ThinLTO)\n";
        std::cerr << "  --verbose, -v   Show verbose stage/trace output for debugging\n";
        return 1;
    }

    /* Get source file path (first non-flag argument).
       For --doc: first arg is source, second (optional) is output */
    int doc_state = 0; /* 0=normal, 1=expecting source, 2=expecting output */
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--repl" || arg == "--package") { doc_state = 0; continue; }
        if (arg == "--doc") { doc_state = 1; continue; }
        if (doc_state == 1) { source_path = arg; doc_state = 2; continue; }
        if (doc_state == 2) { doc_output = arg; doc_state = 0; continue; }
        if (arg[0] == '-') {
            doc_state = 0;
            /* Skip next arg for flags that take a value */
            if ((arg == "-o" || arg == "-l" || arg == "-L") && i + 1 < argc) i++;
            continue;
        }
        source_path = arg;
        break;
    }

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--emit-ir") emit_ir = true;
        else if (arg == "--emit-obj") emit_obj = true;
        else if (arg == "-o" && i + 1 < argc) output_path = argv[++i];
        else if (arg == "-l" && i + 1 < argc) link_libs.push_back(argv[++i]);
        else if (arg == "-L" && i + 1 < argc) lib_paths.push_back(argv[++i]);
        else if (arg == "--memory-report") show_memory_report = true;
        else if (arg == "--lifetime-report") show_lifetime_report = true;
        else if (arg == "--ownership-report") show_ownership_report = true;
        else if (arg == "--alias-graph") show_alias_graph = true;
        else if (arg == "--allocation-report") show_allocation_report = true;
        else if (arg == "--profiler-report") show_profiler_report = true;
        else if (arg == "--detailed-report") show_detailed_report = true;
        else if (arg == "--performance-report") show_performance_report = true;
        else if (arg == "--percentage-report") show_percentage_report = true;
        else if (arg == "--export-json") export_json = true;
        else if (arg == "--export-csv") export_csv = true;
        else if (arg == "--aurora-ir") use_aurora_ir = true;
        else if (arg == "--optimized") use_optimized_codegen = true;
        else if (arg == "--benchmark") run_benchmarks = true;
        else if (arg == "-O0") { opt_level = 0; opt_size = false; opt_size_aggressive = false; }
        else if (arg == "-O1") { opt_level = 1; opt_size = false; opt_size_aggressive = false; }
        else if (arg == "-O2") { opt_level = 2; opt_size = false; opt_size_aggressive = false; }
        else if (arg == "-O3") { opt_level = 3; opt_size = false; opt_size_aggressive = false; }
        else if (arg == "-Os") { opt_level = 2; opt_size = true; opt_size_aggressive = false; }
        else if (arg == "-Oz") { opt_level = 2; opt_size = true; opt_size_aggressive = true; }
        else if (arg == "-ffast-math") fast_math = true;
        else if (arg == "-flto") use_lto = true;
        else if (arg == "--coverage") enable_coverage = true;
        else if (arg == "--verbose" || arg == "-v") verbose = true;
        else if (arg == "--run")      run_jit = true;
        else if (arg == "--repl") {} /* already handled */
        else if (arg == "--doc") {} /* already handled */
        else if (arg == "--package") {} /* already handled */
    }

    /* ── Package Mode ── */
    if (package_mode) {
        return run_package_command(package_args);
    }

    /* ── Doc Mode ── */
    if (doc_mode) {
        if (source_path.empty()) {
            std::cerr << "Usage: aurorac --doc <source.aura> [output.html]\n";
            return 1;
        }
        return run_doc_generator(source_path, doc_output);
    }

    /* ── REPL Mode ── */
    if (repl_mode) {
        std::cout << "╔══════════════════════════════════════════╗\n";
        std::cout << "║     Aurora Language REPL v1.0           ║\n";
        std::cout << "║     Type code, press Enter to run.      ║\n";
        std::cout << "║     Type 'exit' to quit.                ║\n";
        std::cout << "╚══════════════════════════════════════════╝\n";

        std::vector<std::string> history;
        std::string line;
        while (true) {
            std::cout << "aura> " << std::flush;
            if (!std::getline(std::cin, line)) {
                std::cout << "\n";
                break;
            }
            if (line == "exit" || line == ":q") break;
            if (line.empty()) continue;

            history.push_back(line);

            /* Build full source buffer from history */
            std::string source;
            for (const auto& h : history)
                source += h + "\n";

            /* Wrap in function main() — no return type, indented body */
            std::string wrapped =
                "function main():\n"
                "  " + line + "\n"
                "  return 0\n";

            try {
                /* Lex */
                Lexer lexer;
                auto lines = lexer.lex(wrapped);

                /* Parse */
                Parser parser(lines);
                ASTNode::Ptr ast = parser.parse();

                /* Memory analysis */
                MemoryAnalyzer memory_analyzer;
                memory_analyzer.analyse(ast.get());
                memory_analyzer.apply_to_ast(ast.get());

                if (memory_analyzer.has_errors()) {
                    std::cerr << "REPL: compilation errors\n";
                    continue;
                }

                /* Codegen */
                auto ctx = std::make_unique<llvm::LLVMContext>();
                auto module = std::make_unique<llvm::Module>("aurora_repl", *ctx);
                auto builder = std::make_unique<llvm::IRBuilder<>>(*ctx);
                Codegen codegen(*ctx, module, builder);
                codegen.set_source_file(source_path);
                codegen.set_coverage_enabled(enable_coverage);
                codegen.generate(ast.get());

                /* Run JIT */
                int exit_code = jit_execute_main(std::move(ctx), std::move(module));
                if (exit_code != 0 && exit_code != -1)
                    std::cout << "exit: " << exit_code << "\n";

            } catch (const std::exception& e) {
                std::cerr << "REPL error: " << e.what() << "\n";
            }
        }
        return 0;
    }

    if (source_path.empty() && !repl_mode) {
        std::cerr << "Usage: aurorac <source.aura> [options]\n";
        return 1;
    }

    try {
        /* ── Stage 1: Lex ── */
        if (verbose) std::cerr << "STAGE1: Lex\n" << std::flush;
        std::string source = read_file(source_path);
        Lexer lexer;
        auto lines = lexer.lex(source);

        /* ── Stage 2: Parse ── */
        if (verbose) std::cerr << "STAGE2: Parse\n" << std::flush;
        Parser parser(lines);
        ASTNode::Ptr ast = parser.parse();

        /* ── Stage 2b: Resolve imports ── */
        {
            auto sep = source_path.find_last_of("/\\");
            std::string source_dir = (sep == std::string::npos) ? "." : source_path.substr(0, sep);
            ast = resolve_imports(std::move(ast), source_dir, exe_dir);
        }

        /* ── Stage 3: Memory Analysis (Phase 1-8) ── */
        if (verbose) std::cerr << "STAGE3: MemoryAnalysis\n" << std::flush;
        MemoryAnalyzer memory_analyzer;
        memory_analyzer.analyse(ast.get());
        memory_analyzer.apply_to_ast(ast.get());

        /* Show reports if requested */
        if (show_memory_report) {
            memory_analyzer.print_escape_report();
        }
        if (show_lifetime_report) {
            memory_analyzer.print_lifetime_report();
        }
        if (show_ownership_report) {
            memory_analyzer.print_ownership_report();
        }
        if (show_alias_graph) {
            memory_analyzer.print_alias_graph();
        }
        if (show_allocation_report) {
            memory_analyzer.print_allocation_report();
        }
        if (show_profiler_report) {
            memory_analyzer.print_profiler_report();
        }
        if (show_detailed_report) {
            memory_analyzer.print_detailed_report();
        }
        if (show_performance_report) {
            memory_analyzer.print_performance_report();
        }
        if (show_percentage_report) {
            memory_analyzer.print_percentage_report();
        }
        if (export_json) {
            std::cout << memory_analyzer.export_profiler_json() << "\n";
        }
        if (export_csv) {
            std::cout << memory_analyzer.export_profiler_csv();
        }

        /* Run benchmarks if requested */
        if (run_benchmarks) {
            BenchmarkSuite benchmark;
            benchmark.run_all_benchmarks(memory_analyzer);
            benchmark.print_summary();
            benchmark.print_detailed();
            benchmark.print_optimization_report();
        }

        /* Check for errors before codegen */
        if (memory_analyzer.has_errors()) {
            if (verbose) std::cerr << "STAGE4: Skipped (compilation errors)\n" << std::flush;
            return 1;
        }

        /* ── Stage 4: Code Generation ── */
        if (verbose) std::cerr << "STAGE4: CodeGen\n" << std::flush;
        auto ctx = std::make_unique<llvm::LLVMContext>();
        std::unique_ptr<llvm::Module> module;
        auto builder = std::make_unique<llvm::IRBuilder<>>(*ctx);

        if (fast_math) {
            llvm::FastMathFlags fmf;
            fmf.setFast();
            builder->setFastMathFlags(fmf);
        }

        if (use_aurora_ir) {
            /* ── Aurora IR pipeline: AST → Aurora IR → Optimize → Lower to LLVM IR ── */
            AstToIr ast_to_ir;
            IrModule ir_mod = ast_to_ir.translate(ast.get());
            if (verbose) std::cerr << "STAGE4a: Aurora IR generated (" << ir_mod.functions.size()
                                   << " functions, " << ir_mod.type_pool.size() << " types)\n" << std::flush;

            /* Optimize Aurora IR */
            try {
                ir_optimize(ir_mod);
            } catch (const std::exception& e) {
                std::cerr << "\n\033[1;33m[Warning]\033[0m Aurora IR optimization failed: " << e.what() << "\n" << std::flush;
            } catch (...) {
                std::cerr << "\n\033[1;33m[Warning]\033[0m Aurora IR optimization failed (unknown exception)\n" << std::flush;
            }
            if (verbose) std::cerr << "STAGE4b: Aurora IR optimized\n" << std::flush;

            /* Lower to LLVM IR */
            module.reset(lower_ir_to_llvm(ir_mod, *ctx));
            if (!module)
                module = std::make_unique<llvm::Module>("aurora_module", *ctx);
            module->setSourceFileName(source_path);
            if (verbose) std::cerr << "STAGE4c: Lowered to LLVM IR\n" << std::flush;

        } else if (use_optimized_codegen) {
            /* Use optimized codegen with memory analysis */
            module = std::make_unique<llvm::Module>("aurora_module", *ctx);
            OptimizedCodegen opt_codegen(*ctx, module, builder);
            opt_codegen.generate(ast.get(), memory_analyzer);
        } else {
            /* Use standard codegen */
            module = std::make_unique<llvm::Module>("aurora_module", *ctx);
            Codegen codegen(*ctx, module, builder);
            codegen.set_source_file(source_path);
            codegen.set_coverage_enabled(enable_coverage);
            codegen.generate(ast.get());
        }
        if (verbose) std::cerr << "STAGE4: Done\n" << std::flush;

        /* ── Stage 5: Aurora custom optimizations (LLVM IR level) ── */
        if (!use_aurora_ir) {
            if (verbose) std::cerr << "STAGE5: BeforeOpt: " << module->size() << " functions\n" << std::flush;
            try {
                run_aurora_optimizer(module.get());
            } catch (const std::exception& e) {
                std::cerr << "\n\033[1;33m[Warning]\033[0m Aurora LLVM optimizer failed: " << e.what() << "\n" << std::flush;
            } catch (...) {
                std::cerr << "\n\033[1;33m[Warning]\033[0m Aurora LLVM optimizer failed (unknown exception)\n" << std::flush;
            }
            if (verbose) std::cerr << "STAGE5: AfterOpt: " << module->size() << " functions\n" << std::flush;
        }

        /* ── Stage 6: LLVM optimization + native CPU ── */
        if (verbose) std::cerr << "STAGE6: init\n" << std::flush;
        std::string cpu = llvm::sys::getHostCPUName().str();
        if (verbose) std::cerr << "STAGE6: cpu=" << cpu << "\n" << std::flush;
        {
            std::string features_str;
#if LLVM_VERSION_MAJOR >= 19
            auto host_features = llvm::sys::getHostCPUFeatures();
            if (!host_features.empty()) {
                for (const auto& f : host_features) {
                    if (!features_str.empty()) features_str += ",";
                    features_str += f.second ? "+" : "-";
                    features_str += f.first();
                }
            }
#else
            llvm::StringMap<bool, llvm::MallocAllocator> host_features;
            if (llvm::sys::getHostCPUFeatures(host_features)) {
                for (const auto& f : host_features) {
                    if (!features_str.empty()) features_str += ",";
                    features_str += f.second ? "+" : "-";
                    features_str += f.first();
                }
            }
#endif
            if (verbose) std::cerr << "STAGE6: features=" << features_str << "\n" << std::flush;
            module->setTargetTriple(llvm::sys::getProcessTriple());
            if (verbose) std::cerr << "STAGE6: triple=" << module->getTargetTriple() << "\n" << std::flush;

            if (verbose) std::cerr << "STAGE6: creating managers\n" << std::flush;
            llvm::LoopAnalysisManager LAM;
            llvm::FunctionAnalysisManager FAM;
            llvm::CGSCCAnalysisManager CGAM;
            llvm::ModuleAnalysisManager MAM;
            if (verbose) std::cerr << "STAGE6: creating passbuilder\n" << std::flush;
            llvm::PassBuilder PB;

            PB.registerModuleAnalyses(MAM);
            PB.registerCGSCCAnalyses(CGAM);
            PB.registerFunctionAnalyses(FAM);
            PB.registerLoopAnalyses(LAM);
            PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

            /* Select optimization level */
            llvm::OptimizationLevel ol;
            if (opt_level == 0) ol = llvm::OptimizationLevel::O0;
            else if (opt_level == 1) ol = llvm::OptimizationLevel::O1;
            else if (opt_level == 2) ol = opt_size
                ? (opt_size_aggressive ? llvm::OptimizationLevel::Oz : llvm::OptimizationLevel::Os)
                : llvm::OptimizationLevel::O2;
            else ol = llvm::OptimizationLevel::O3;

            if (verbose) std::cerr << "STAGE6: opt_level=" << opt_level << " ol_type=" << (opt_level == 0 ? "O0" : "O1+") << "\n" << std::flush;
            if (opt_level > 0) {
                if (verbose) std::cerr << "STAGE6: building pipeline\n" << std::flush;
                llvm::ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(ol);
                MPM.run(*module, MAM);

                /* Post-optimization cleanup passes */
                {
                    llvm::LoopPassManager LPM;
                    LPM.addPass(llvm::IndVarSimplifyPass());

                    llvm::FunctionPassManager FPM;
                    FPM.addPass(llvm::createFunctionToLoopPassAdaptor(std::move(LPM)));
                    FPM.addPass(llvm::SimplifyCFGPass());

                    llvm::ModulePassManager MPM2;
                    MPM2.addPass(llvm::createModuleToFunctionPassAdaptor(std::move(FPM)));
                    MPM2.run(*module, MAM);
                }
            }
        }
        if (verbose) std::cerr << "STAGE6: opt" << opt_level << " done (cpu=" << cpu << ")\n" << std::flush;

        if (run_jit) {
            /* JIT-execute main function */
            if (verbose) std::cerr << "JIT: Starting execution\n" << std::flush;
            int exit_code = jit_execute_main(std::move(ctx), std::move(module));
            if (exit_code != 0 && exit_code != -1)
                if (verbose) std::cerr << "JIT exit code: " << exit_code << "\n" << std::flush;
            return exit_code == -1 ? 1 : 0;
        }

        if (emit_ir) {
            /* Print LLVM IR to stdout */
            module->print(llvm::outs(), nullptr);
        } else if (emit_obj) {
            /* Emit object file */
            std::string obj_path = output_path.empty()
                ? source_path.substr(0, source_path.rfind('.')) + ".obj"
                : output_path;
            /* If -o ends with .exe, derive obj path */
            std::string exe_path;
            if (obj_path.size() > 4 && obj_path.substr(obj_path.size() - 4) == ".exe") {
                exe_path = obj_path;
                obj_path = obj_path.substr(0, obj_path.size() - 4) + ".obj";
            }
            if (emit_object_file(module.get(), obj_path, fast_math, use_lto)) {
                std::cout << "aurora: object written to " << obj_path << "\n";
            } else {
                return 1;
            }

            /* Link if libraries were specified or exe output was requested */
            if (!link_libs.empty() || !exe_path.empty()) {
                if (exe_path.empty())
                    exe_path = source_path.substr(0, source_path.rfind('.')) + ".exe";

                if (!link_exe(obj_path, exe_path, exe_dir, link_libs, lib_paths, use_lto))
                    return 1;
                std::cout << "aurora: executable written to " << exe_path << "\n";
            }
        } else {
            /* Write IR to .ll file */
            std::string ir_str;
            llvm::raw_string_ostream ir_os(ir_str);
            module->print(ir_os, nullptr);
            ir_os.flush();

            if (verbose) std::cerr << "IR length: " << ir_str.size() << "\n" << std::flush;

            auto sep = source_path.find_last_of("/\\");
            std::string fname = (sep == std::string::npos) ? source_path : source_path.substr(sep + 1);
            fname = fname.substr(0, fname.rfind('.')) + ".ll";
            std::string out_dir = "output/ir/";
            fs::create_directories(out_dir);
            std::string out_path = out_dir + fname;
            std::ofstream ofs(out_path);
            ofs << ir_str;
            ofs.close();

            if (verbose) std::cerr << "File written: " << out_path << "\n" << std::flush;
        }

    } catch (const OwnershipError& e) {
        std::cerr << "\n\033[1;31m[Ownership Error]\033[0m\n  " << e.what() << "\n";
        return 1;
    } catch (const TypeError& e) {
        std::cerr << "\n\033[1;31m[Error]\033[0m Line " << e.line << ": " << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "\n\033[1;31m[Error]\033[0m " << e.what() << "\n";
        return 1;
    }

    return 0;
}
