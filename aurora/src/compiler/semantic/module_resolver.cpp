#include "compiler/lexer.hpp"
#include "compiler/parser.hpp"
#include "compiler/typechecker.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

/* ════════════════════════════════════════════════════════════
   Module Resolver — Phase 4
   ════════════════════════════════════════════════════════════
   Resolves `import "path"` statements by finding, lexing,
   parsing and registering the imported module.
   ════════════════════════════════════════════════════════════ */

struct ResolvedModule {
    std::string          path;
    ASTNode::Ptr         ast;
    std::vector<std::string> exports;
    bool                 is_system { false };
};

class ModuleResolver {
public:
    ModuleResolver(const std::string& search_path)
        : search_path_(search_path) {}

    /* ── Resolve an import path ── */
    ResolvedModule* resolve(const std::string& import_path, int line) {
        /* Check cache */
        auto it = cache_.find(import_path);
        if (it != cache_.end())
            return &it->second;

        /* Search for the file */
        std::string full_path = find_file(import_path);
        if (full_path.empty()) {
            std::cerr << "error at line " << line
                      << ": module not found: '" << import_path << "'\n";
            return nullptr;
        }

        /* Read, lex, parse */
        std::ifstream f(full_path);
        if (!f.is_open()) {
            std::cerr << "error at line " << line
                      << ": cannot open module: " << full_path << "\n";
            return nullptr;
        }
        std::ostringstream ss;
        ss << f.rdbuf();
        std::string source = ss.str();

        Lexer lexer;
        auto lines = lexer.lex(source);

        Parser parser(lines);
        ASTNode::Ptr ast = parser.parse();

        /* Register in cache */
        ResolvedModule mod;
        mod.path = full_path;
        mod.ast = std::move(ast);
        cache_[import_path] = std::move(mod);

        return &cache_[import_path];
    }

    /* ── Get all resolved modules ── */
    const std::unordered_map<std::string, ResolvedModule>& modules() const {
        return cache_;
    }

private:
    std::string search_path_;
    std::unordered_map<std::string, ResolvedModule> cache_;

    std::string find_file(const std::string& name) {
        /* Try as-is */
        if (fs::exists(name))
            return name;

        /* Try with .aura extension */
        std::string with_ext = name;
        if (with_ext.size() < 5 || with_ext.substr(with_ext.size() - 5) != ".aura")
            with_ext += ".aura";
        if (fs::exists(with_ext))
            return with_ext;

        /* Try in search path */
        fs::path search(search_path_);
        fs::path candidate = search / name;
        if (fs::exists(candidate))
            return candidate.string();

        candidate = search / with_ext;
        if (fs::exists(candidate))
            return candidate.string();

        /* Try in aurora/lib/ */
        fs::path lib_path = search / "lib" / name;
        if (fs::exists(lib_path))
            return lib_path.string();

        lib_path = search / "lib" / with_ext;
        if (fs::exists(lib_path))
            return lib_path.string();

        return "";
    }
};
