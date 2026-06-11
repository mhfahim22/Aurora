#pragma once
#include "runtime/interop/universal_resolver.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <sstream>

/* ════════════════════════════════════════════════════════════
   Cross-Ecosystem Dependency Resolution — Phase 5.2
   ════════════════════════════════════════════════════════════
   Resolves transitive dependencies across PyPI/npm/Cargo
   boundaries. Builds a unified dependency graph.
   ════════════════════════════════════════════════════════════ */

/* ── A node in the cross-ecosystem dependency graph ── */
struct CrossDepNode {
    std::string name;
    std::string version;
    Ecosystem ecosystem{Ecosystem::Unknown};

    /* Resolved sub-dependencies */
    std::vector<CrossDepNode> deps;

    /* Depth in resolution tree (cycle detection) */
    int depth{0};
};

/* ── Resolved dependency tree ── */
struct CrossDepResolution {
    bool success{false};
    std::string root_name;
    std::vector<CrossDepNode> tree;
    std::vector<std::string> errors;

    /* Flattened set of all unique resolved packages */
    std::unordered_map<std::string, CrossDepNode> flattened() const {
        std::unordered_map<std::string, CrossDepNode> flat;
        std::function<void(const CrossDepNode&)> walk = [&](const CrossDepNode& node) {
            std::string key = ecosystem_name(node.ecosystem) + std::string(":") + node.name;
            flat[key] = node;
            for (auto& child : node.deps) walk(child);
        };
        for (auto& root : tree) walk(root);
        return flat;
    }
};

/* ── Known cross-ecosystem dependency maps ── */
/* Some packages have known equivalents in other ecosystems */
struct KnownMapping {
    std::string name;
    std::string ecosystem_from;
    std::string ecosystem_to;
    std::string mapped_name;
};

class CrossEcosystemResolver {
public:
    explicit CrossEcosystemResolver(const UniversalResolver& resolver)
        : resolver_(resolver) {}

    /* ── Add a known cross-ecosystem mapping ── */
    void add_mapping(const KnownMapping& m) {
        std::string key = m.ecosystem_from + ":" + m.name;
        cross_map_[key] = m;
    }

    /* ── Register built-in mappings ── */
    void init_builtin_mappings() {
        /* Python → npm equivalents */
        add_mapping({"requests", "pypi", "npm", "node-fetch"});
        add_mapping({"numpy", "pypi", "npm", "numjs"});
        add_mapping({"pandas", "pypi", "npm", "danfojs"});
        add_mapping({"pillow", "pypi", "npm", "sharp"});
        add_mapping({"flask", "pypi", "npm", "express"});
        add_mapping({"fastapi", "pypi", "npm", "fastify"});
        add_mapping({"click", "pypi", "npm", "commander"});
        add_mapping({"jinja2", "pypi", "npm", "nunjucks"});
        add_mapping({"scipy", "pypi", "npm", "science"});
        add_mapping({"matplotlib", "pypi", "npm", "plotly.js"});
        add_mapping({"beautifulsoup4", "pypi", "npm", "cheerio"});
        add_mapping({"selenium", "pypi", "npm", "puppeteer"});
        add_mapping({"redis", "pypi", "npm", "ioredis"});
        add_mapping({"sqlalchemy", "pypi", "npm", "knex"});
        add_mapping({"aiohttp", "pypi", "npm", "undici"});
        add_mapping({"pytest", "pypi", "npm", "jest"});
        add_mapping({"black", "pypi", "npm", "prettier"});
        add_mapping({"isort", "pypi", "npm", "eslint"});
        add_mapping({"mypy", "pypi", "npm", "typescript"});
        add_mapping({"uvicorn", "pypi", "npm", "node-http"});

        /* Python → Cargo equivalents */
        add_mapping({"numpy", "pypi", "cargo", "ndarray"});
        add_mapping({"regex", "pypi", "cargo", "regex"});
        add_mapping({"pandas", "pypi", "cargo", "polars"});
        add_mapping({"scipy", "pypi", "cargo", "scirs"});
        add_mapping({"flask", "pypi", "cargo", "axum"});
        add_mapping({"django", "pypi", "cargo", "dioxus"});
        add_mapping({"sqlalchemy", "pypi", "cargo", "diesel"});
        add_mapping({"httpx", "pypi", "cargo", "reqwest"});
        add_mapping({"aiohttp", "pypi", "cargo", "hyper"});
        add_mapping({"pydantic", "pypi", "cargo", "serde"});
        add_mapping({"loguru", "pypi", "cargo", "log"});
        add_mapping({"click", "pypi", "cargo", "clap"});
        add_mapping({"toml", "pypi", "cargo", "toml"});
        add_mapping({"yaml", "pypi", "cargo", "serde_yaml"});
        add_mapping({"pillow", "pypi", "cargo", "image"});
        add_mapping({"beautifulsoup4", "pypi", "cargo", "scraper"});
        add_mapping({"lxml", "pypi", "cargo", "quick-xml"});

        /* npm → Python equivalents */
        add_mapping({"express", "npm", "pypi", "flask"});
        add_mapping({"lodash", "npm", "pypi", "toolz"});
        add_mapping({"moment", "npm", "pypi", "pendulum"});
        add_mapping({"axios", "npm", "pypi", "httpx"});
        add_mapping({"react", "npm", "pypi", "reactpy"});
        add_mapping({"vue", "npm", "pypi", "vuepy"});
        add_mapping({"next", "npm", "pypi", "nextpy"});
        add_mapping({"chalk", "npm", "pypi", "rich"});
        add_mapping({"cheerio", "npm", "pypi", "beautifulsoup4"});
        add_mapping({"puppeteer", "npm", "pypi", "selenium"});
        add_mapping({"jest", "npm", "pypi", "pytest"});
        add_mapping({"typescript", "npm", "pypi", "mypy"});
        add_mapping({"prettier", "npm", "pypi", "black"});
        add_mapping({"eslint", "npm", "pypi", "flake8"});
        add_mapping({"ws", "npm", "pypi", "websockets"});
        add_mapping({"socket.io", "npm", "pypi", "python-socketio"});
        add_mapping({"sharp", "npm", "pypi", "pillow"});
        add_mapping({"fastify", "npm", "pypi", "fastapi"});
        add_mapping({"ioredis", "npm", "pypi", "redis"});

        /* npm → Cargo equivalents */
        add_mapping({"lodash", "npm", "cargo", "itertools"});
        add_mapping({"chalk", "npm", "cargo", "colored"});
        add_mapping({"express", "npm", "cargo", "axum"});
        add_mapping({"axios", "npm", "cargo", "reqwest"});
        add_mapping({"moment", "npm", "cargo", "chrono"});
        add_mapping({"yargs", "npm", "cargo", "clap"});
        add_mapping({"winston", "npm", "cargo", "log"});
        add_mapping({"sharp", "npm", "cargo", "image"});
        add_mapping({"cheerio", "npm", "cargo", "scraper"});
        add_mapping({"uuid", "npm", "cargo", "uuid"});
        add_mapping({"js-yaml", "npm", "cargo", "serde_yaml"});
        add_mapping({"commander", "npm", "cargo", "clap"});

        /* Cargo → Python equivalents */
        add_mapping({"serde", "cargo", "pypi", "pydantic"});
        add_mapping({"regex", "cargo", "pypi", "regex"});
        add_mapping({"tokio", "cargo", "pypi", "anyio"});
        add_mapping({"axum", "cargo", "pypi", "fastapi"});
        add_mapping({"reqwest", "cargo", "pypi", "httpx"});
        add_mapping({"clap", "cargo", "pypi", "click"});
        add_mapping({"serde_json", "cargo", "pypi", "orjson"});
        add_mapping({"chrono", "cargo", "pypi", "pendulum"});
        add_mapping({"diesel", "cargo", "pypi", "sqlalchemy"});
        add_mapping({"tower", "cargo", "pypi", "starlette"});
        add_mapping({"tracing", "cargo", "pypi", "loguru"});
        add_mapping({"image", "cargo", "pypi", "pillow"});
        add_mapping({"scraper", "cargo", "pypi", "beautifulsoup4"});
        add_mapping({"polars", "cargo", "pypi", "pandas"});
        add_mapping({"ndarray", "cargo", "pypi", "numpy"});
        add_mapping({"quick-xml", "cargo", "pypi", "lxml"});
        add_mapping({"uuid", "cargo", "pypi", "uuid"});
        add_mapping({"toml", "cargo", "pypi", "toml"});
        add_mapping({"itertools", "cargo", "pypi", "more-itertools"});
        add_mapping({"colored", "cargo", "pypi", "rich"});

        /* Cargo → npm equivalents */
        add_mapping({"serde", "cargo", "npm", "zod"});
        add_mapping({"tokio", "cargo", "npm", "async"});
        add_mapping({"axum", "cargo", "npm", "express"});
        add_mapping({"reqwest", "cargo", "npm", "axios"});
        add_mapping({"clap", "cargo", "npm", "commander"});
        add_mapping({"chrono", "cargo", "npm", "moment"});
        add_mapping({"log", "cargo", "npm", "winston"});
        add_mapping({"image", "cargo", "npm", "sharp"});
        add_mapping({"uuid", "cargo", "npm", "uuid"});
        add_mapping({"serde_yaml", "cargo", "npm", "js-yaml"});
        add_mapping({"itertools", "cargo", "npm", "lodash"});
        add_mapping({"polars", "cargo", "npm", "danfojs"});
    }

    /* ── Try to find a cross-ecosystem equivalent ── */
    std::optional<KnownMapping> find_mapping(const std::string& name,
                                              Ecosystem from_eco,
                                              const std::string& target_eco) const
    {
        std::string key = ecosystem_name(from_eco) + std::string(":") + name;
        auto it = cross_map_.find(key);
        if (it != cross_map_.end() && it->second.ecosystem_to == target_eco)
            return it->second;
        /* Fallback: if same package name exists in target ecosystem, use it */
        Ecosystem te = ecosystem_from_name(target_eco);
        if (te != Ecosystem::Unknown && te != from_eco) {
            UnifiedPackageInfo info = resolver_.resolve(name, te, "latest");
            if (info.found) {
                KnownMapping auto_map;
                auto_map.name = name;
                auto_map.ecosystem_from = ecosystem_name(from_eco);
                auto_map.ecosystem_to = target_eco;
                auto_map.mapped_name = name;
                return auto_map;
            }
        }
        return std::nullopt;
    }

    /* ── Resolve a package and its cross-ecosystem deps ── */
    CrossDepResolution resolve(const std::string& pkg_name, Ecosystem eco,
                                const std::string& version = "latest",
                                int max_depth = 3) const
    {
        CrossDepResolution result;
        result.root_name = pkg_name;

        /* Native ecosystem has no cross-ecosystem dependency resolution */
        if (eco == Ecosystem::Native) {
            result.success = true;
            CrossDepNode node;
            node.name = pkg_name;
            node.ecosystem = eco;
            node.version = version;
            node.depth = 0;
            result.tree.push_back(node);
            return result;
        }

        std::unordered_set<std::string> visited;
        std::queue<CrossDepNode> queue;

        CrossDepNode root;
        root.name = pkg_name;
        root.ecosystem = eco;
        root.version = version;
        root.depth = 0;
        queue.push(root);

        std::vector<CrossDepNode> tree;

        while (!queue.empty()) {
            CrossDepNode node = queue.front();
            queue.pop();

            std::string visit_key = std::string(ecosystem_name(node.ecosystem)) + ":" + node.name;
            if (visited.find(visit_key) != visited.end()) continue;
            visited.insert(visit_key);

            /* Resolve the package in its ecosystem */
            UnifiedPackageInfo pkg_info = resolver_.resolve(node.name, node.ecosystem, node.version);
            if (!pkg_info.found) {
                result.errors.push_back("not found: " + visit_key);
                continue;
            }

            node.version = pkg_info.version;
            tree.push_back(node);
            result.success = true;

            if (node.depth >= max_depth) continue;

            /* Try to find cross-ecosystem equivalents */
            static const std::pair<const char*, const char*> eco_pairs[] = {
                {"cargo", "pypi"}, {"cargo", "npm"},
                {"pypi", "cargo"}, {"pypi", "npm"},
                {"npm", "cargo"}, {"npm", "pypi"},
            };

            for (auto& [from, to] : eco_pairs) {
                Ecosystem to_eco = ecosystem_from_name(to);
                if (to_eco == Ecosystem::Unknown) continue;

                auto mapping = find_mapping(node.name, node.ecosystem, to);
                if (mapping.has_value()) {
                    CrossDepNode dep;
                    dep.name = mapping->mapped_name;
                    dep.ecosystem = to_eco;
                    dep.version = "latest";
                    dep.depth = node.depth + 1;
                    queue.push(dep);
                }
            }
        }

        result.tree = tree;
        return result;
    }

private:
    const UniversalResolver& resolver_;
    std::unordered_map<std::string, KnownMapping> cross_map_;
};
