#pragma once
#include "voss.h"
#include "runtime/interop/json_parser.hpp"
#include "runtime/interop/http_client.hpp"
#include "runtime/interop/universal_resolver.hpp"
#include "runtime/interop/universal_binding_gen.hpp"
#include "runtime/interop/cross_ecosystem_deps.hpp"
#include <sstream>
#include <fstream>
#include <regex>
#include <filesystem>
#include <cstdio>
#include <cctype>
#include <algorithm>
#include <vector>
#include <string>
namespace fs = std::filesystem;

/* ── Shared helpers ── */
bool write_file(const std::string& path, const std::string& content);
std::string quickjs_dir();
void gen_manifest(const std::string& pkg, const std::string& ecosystem,
                  const std::string& ver, const std::string& desc,
                  const std::string& entry, std::ostream& os);

/* ── PyPI bridge ── */
const char* pypi_import_alias(const std::string& pkg);
void gen_pypi_au_binding(const std::string& pkg, const JsonValue& json,
                          const std::string& ver, std::ostream& os);

/* ── npm bridge ── */
void gen_npm_au_binding(const std::string& pkg, const JsonValue& json,
                         const std::string& ver, std::ostream& os);
bool npm_has_native_addon(const JsonValue& json);
void gen_npm_cpp_wrapper(const std::string& pkg, const std::string& dir,
                         const std::string& node_path = "");
void gen_quickjs_npm_wrapper(const std::string& pkg, const std::string& dir);

/* ── Cargo bridge ── */
struct CargoDiscovery {
    std::string registry_entries;       // m.insert(...) calls for free fns
    std::string method_registry_init;   // type_map.fill() code (for type_registry)
    std::string ctor_registry_init;     // ctor_map.fill() code (for ctor_registry)
    std::string drop_registry_init;     // drop_registry fill code
    std::string method_au_entries;      // Lines for .au binding
    std::vector<std::string> required_features; // cfg(feature = "...") deps
    int fn_count;
    int method_count;
};

void gen_cargo_au_binding(const std::string& pkg, const JsonValue& json,
                           const std::string& ver, std::ostream& os,
                           const std::string& extra_au = "");
CargoDiscovery discover_cargo_functions(const std::string& pkg,
                                         const std::string& ver,
                                         const std::string& dir);
void gen_cargo_rust_wrapper(const std::string& pkg, const std::string& ver,
                              const std::string& dir,
                              const CargoDiscovery& disc);
void gen_cargo_manual_scaffold(const std::string& pkg, const std::string& ver,
                                const std::string& dir);

/* ── Native DLL bridge ── */
std::string find_native_dll(const std::string& name);
std::vector<std::string> get_dll_exports(const std::string& dll_path);
void gen_native_au_binding(const std::string& pkg, const std::string& dll_path,
                            const std::vector<std::string>& exports,
                            std::ostream& os);

/* ── JVM bridge ── */
void gen_jvm_au_binding(const std::string& pkg, const JsonValue& json,
                         const std::string& ver, std::ostream& os);
void gen_jvm_c_wrapper(const std::string& pkg, const std::string& dir);

/* ── Go bridge ── */
void gen_go_au_binding(const std::string& pkg, const JsonValue& json,
                        const std::string& ver, std::ostream& os);
void gen_go_c_wrapper(const std::string& pkg, const std::string& dir);
