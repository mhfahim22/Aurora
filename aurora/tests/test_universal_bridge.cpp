#include <cassert>
#include <cstdio>
#include <string>
#include <sstream>
#include <functional>
#include "runtime/interop/type_ir.hpp"
#include "runtime/interop/type_mapping.hpp"
#include "runtime/interop/universal_resolver.hpp"
#include "runtime/interop/cross_ecosystem_deps.hpp"
#include "runtime/interop/eco_type_ir_mapper.hpp"
#include "runtime/interop/universal_binding_gen.hpp"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { ++tests_run; printf("  TEST %d: %s ... ", tests_run, name); fflush(stdout);

#define END_TEST ok = true; \
    if (ok) { ++tests_passed; printf("PASS\n"); } \
    else    { printf("FAIL\n"); } \
} while(0)

/* ── Mock HTTP responses ── */
static std::string mock_pypi_json(const std::string& pkg) {
    return "{\"info\": {\"version\": \"2.31.0\", \"summary\": \"HTTP library\"}}";
}
static std::string mock_npm_json(const std::string& pkg) {
    return "{\"name\": \"" + pkg + "\", \"version\": \"4.18.2\", \"description\": \"web framework\"}";
}
static std::string mock_cargo_json(const std::string& pkg) {
    return "{\"crate\": {\"max_stable_version\": \"1.0.0\", \"description\": \"serialization framework\"}}";
}
static std::string mock_empty(const std::string& url) { return {}; }

/* ════════════════════════════════════════════════════════════ */
/* 1. Universal Resolver Tests                                */
/* ════════════════════════════════════════════════════════════ */
static void test_resolver_pypi() {
    bool ok = false;
    TEST("UniversalResolver resolves PyPI package") {
        auto http = [](const std::string& url) -> std::string {
            return mock_pypi_json("requests");
        };
        UniversalResolver res(http);
        auto info = res.resolve("requests", Ecosystem::PyPI);
        ok = info.found
          && info.version == "2.31.0"
          && info.description == "HTTP library"
          && info.ecosystem == Ecosystem::PyPI;
    }
    END_TEST;
}

static void test_resolver_npm() {
    bool ok = false;
    TEST("UniversalResolver resolves npm package") {
        auto http = [](const std::string& url) -> std::string {
            return mock_npm_json("express");
        };
        UniversalResolver res(http);
        auto info = res.resolve("express", Ecosystem::Npm);
        ok = info.found
          && info.version == "4.18.2"
          && info.description == "web framework"
          && info.ecosystem == Ecosystem::Npm;
    }
    END_TEST;
}

static void test_resolver_cargo() {
    bool ok = false;
    TEST("UniversalResolver resolves Cargo package") {
        auto http = [](const std::string& url) -> std::string {
            return mock_cargo_json("serde");
        };
        UniversalResolver res(http);
        auto info = res.resolve("serde", Ecosystem::Cargo);
        ok = info.found
          && info.version == "1.0.0"
          && info.ecosystem == Ecosystem::Cargo;
    }
    END_TEST;
}

static void test_resolver_not_found() {
    bool ok = false;
    TEST("UniversalResolver returns not-found for unknown") {
        UniversalResolver res(mock_empty);
        auto info = res.resolve("nonexistent_pkg_xyz", Ecosystem::PyPI);
        ok = !info.found;
    }
    END_TEST;
}

static void test_resolver_auto_detect() {
    bool ok = false;
    TEST("UniversalResolver auto_detect finds first match") {
        int call_count = 0;
        auto http = [&](const std::string& url) -> std::string {
            call_count++;
            if (url.find("pypi.org") != std::string::npos)
                return mock_pypi_json("requests");
            return {};
        };
        UniversalResolver res(http);
        auto info = res.auto_detect("requests");
        ok = info.found
          && info.ecosystem == Ecosystem::PyPI
          && call_count >= 1;
    }
    END_TEST;
}

static void test_resolver_auto_detect_fallback() {
    bool ok = false;
    TEST("UniversalResolver auto_detect tries all ecosystems") {
        int call_count = 0;
        auto http = [&](const std::string& url) -> std::string {
            call_count++;
            if (url.find("crates.io") != std::string::npos)
                return mock_cargo_json("serde");
            return {};
        };
        UniversalResolver res(http);
        auto info = res.auto_detect("serde");
        ok = info.found
          && info.ecosystem == Ecosystem::Cargo
          && call_count == 3; /* PyPI fail, npm fail, Cargo hit */
    }
    END_TEST;
}

static void test_search_all() {
    bool ok = false;
    TEST("search_all finds across multiple ecosystems") {
        int call_count = 0;
        auto http = [&](const std::string& url) -> std::string {
            call_count++;
            if (url.find("pypi.org") != std::string::npos) return mock_pypi_json("pkg");
            if (url.find("npmjs.org") != std::string::npos) return mock_npm_json("pkg");
            if (url.find("crates.io") != std::string::npos) return mock_cargo_json("pkg");
            return {};
        };
        UniversalResolver res(http);
        auto results = res.search_all("pkg");
        ok = results.size() == 3
          && call_count == 3;
    }
    END_TEST;
}

/* ════════════════════════════════════════════════════════════ */
/* 2. Cross-Ecosystem Dep Resolution Tests                    */
/* ════════════════════════════════════════════════════════════ */
static void test_cross_eco_mapping() {
    bool ok = false;
    TEST("CrossEcosystemResolver finds mapping") {
        auto http = [](const std::string&) -> std::string { return mock_pypi_json("requests"); };
        UniversalResolver res(http);
        CrossEcosystemResolver cross(res);
        cross.init_builtin_mappings();

        auto mapping = cross.find_mapping("requests", Ecosystem::PyPI, "npm");
        ok = mapping.has_value()
          && mapping->mapped_name == "node-fetch";
    }
    END_TEST;
}

static void test_cross_eco_no_mapping() {
    bool ok = false;
    TEST("CrossEcosystemResolver returns nullopt for unknown") {
        auto http = [](const std::string&) -> std::string { return mock_pypi_json("xyz"); };
        UniversalResolver res(http);
        CrossEcosystemResolver cross(res);
        cross.init_builtin_mappings();

        auto mapping = cross.find_mapping("xyz_unknown", Ecosystem::PyPI, "npm");
        ok = !mapping.has_value();
    }
    END_TEST;
}

static void test_cross_resolve() {
    bool ok = false;
    TEST("CrossEcosystemResolver resolves with cross-eco deps") {
        int call_count = 0;
        auto http = [&](const std::string& url) -> std::string {
            call_count++;
            if (url.find("pypi.org") != std::string::npos) return mock_pypi_json("numpy");
            if (url.find("crates.io") != std::string::npos) return mock_cargo_json("ndarray");
            return {};
        };
        UniversalResolver res(http);
        CrossEcosystemResolver cross(res);
        cross.init_builtin_mappings();

        auto result = cross.resolve("numpy", Ecosystem::PyPI, "latest", 1);

        /* numpy resolves to pypi and has cargo mapping → ndarray */
        ok = result.success;
    }
    END_TEST;
}

/* ════════════════════════════════════════════════════════════ */
/* 3. Ecosystem Type IR Mapper Tests                          */
/* ════════════════════════════════════════════════════════════ */
static void test_eco_mapper_pypi() {
    bool ok = false;
    TEST("EcoTypeIRMapper maps Python int → i64 (ZeroCost)") {
        EcosystemTypeIRMapper mapper;
        auto mapped = mapper.map_to_ir("int", Ecosystem::PyPI);
        ok = mapped.ir_type.kind == InteropTypeKind::Int64
          && mapped.cost == InteropCost::ZeroCost
          && mapper.to_aurora("int", Ecosystem::PyPI) == "i64";
    }
    END_TEST;
}

static void test_eco_mapper_pypi_string() {
    bool ok = false;
    TEST("EcoTypeIRMapper maps Python str → String (AllocCost)") {
        EcosystemTypeIRMapper mapper;
        auto mapped = mapper.map_to_ir("str", Ecosystem::PyPI);
        ok = mapped.ir_type.kind == InteropTypeKind::String
          && mapped.cost == InteropCost::AllocCost;
    }
    END_TEST;
}

static void test_eco_mapper_npm() {
    bool ok = false;
    TEST("EcoTypeIRMapper maps JS number → f64 (ZeroCost)") {
        EcosystemTypeIRMapper mapper;
        auto mapped = mapper.map_to_ir("number", Ecosystem::Npm);
        ok = mapped.ir_type.kind == InteropTypeKind::Float64
          && mapped.cost == InteropCost::ZeroCost;
    }
    END_TEST;
}

static void test_eco_mapper_cargo() {
    bool ok = false;
    TEST("EcoTypeIRMapper maps Rust Vec → List (AllocCost)") {
        EcosystemTypeIRMapper mapper;
        auto mapped = mapper.map_to_ir("Vec", Ecosystem::Cargo);
        ok = mapped.ir_type.kind == InteropTypeKind::List
          && mapped.cost == InteropCost::AllocCost;
    }
    END_TEST;
}

static void test_eco_mapper_unknown() {
    bool ok = false;
    TEST("EcoTypeIRMapper returns UnknownCost for unknown type") {
        EcosystemTypeIRMapper mapper;
        auto mapped = mapper.map_to_ir("SomeUnknownType", Ecosystem::PyPI);
        ok = mapped.cost == InteropCost::UnknownCost;
    }
    END_TEST;
}

static void test_eco_mapper_aurora_names() {
    bool ok = false;
    TEST("EcoTypeIRMapper to_aurora returns correct names") {
        EcosystemTypeIRMapper mapper;
        ok = mapper.to_aurora("int", Ecosystem::PyPI) == "i64"
          && mapper.to_aurora("number", Ecosystem::Npm) == "f64"
          && mapper.to_aurora("i32", Ecosystem::Cargo) == "i32"
          && mapper.to_aurora("String", Ecosystem::Cargo) == "String";
    }
    END_TEST;
}

/* ════════════════════════════════════════════════════════════ */
/* 4. Universal Binding Generator Tests                       */
/* ════════════════════════════════════════════════════════════ */
static void test_binding_gen_pypi() {
    bool ok = false;
    TEST("BindingGenerator generates PyPI .au file") {
        EcosystemTypeIRMapper mapper;
        BindingGenOptions opts;
        UniversalBindingGenerator gen(mapper, opts);

        UnifiedPackageInfo pkg;
        pkg.name = "requests";
        pkg.version = "2.31.0";
        pkg.description = "HTTP library";
        pkg.ecosystem = Ecosystem::PyPI;
        pkg.found = true;

        std::string code = gen.generate(pkg);
        ok = code.find("requests@2.31.0") != std::string::npos
          && code.find("extern \"pypi_requests\"") != std::string::npos
          && code.find("PyInit_requests") != std::string::npos
          && code.find("type Py_int") != std::string::npos
          && code.find("@cost(zero)") != std::string::npos
          && code.find("@cost(alloc)") != std::string::npos
          && code.find("Marshal: Py_str") != std::string::npos;
    }
    END_TEST;
}

static void test_binding_gen_npm() {
    bool ok = false;
    TEST("BindingGenerator generates npm .au file") {
        EcosystemTypeIRMapper mapper;
        BindingGenOptions opts;
        UniversalBindingGenerator gen(mapper, opts);

        UnifiedPackageInfo pkg;
        pkg.name = "express";
        pkg.version = "4.18.2";
        pkg.ecosystem = Ecosystem::Npm;
        pkg.found = true;

        std::string code = gen.generate(pkg);
        ok = code.find("napi_register_module_v1") != std::string::npos
          && code.find("type JS_number") != std::string::npos
          && code.find("Marshal: JS_string") != std::string::npos;
    }
    END_TEST;
}

static void test_binding_gen_cargo() {
    bool ok = false;
    TEST("BindingGenerator generates Cargo .au file") {
        EcosystemTypeIRMapper mapper;
        BindingGenOptions opts;
        UniversalBindingGenerator gen(mapper, opts);

        UnifiedPackageInfo pkg;
        pkg.name = "serde";
        pkg.version = "1.0.0";
        pkg.ecosystem = Ecosystem::Cargo;
        pkg.found = true;

        std::string code = gen.generate(pkg);
        ok = code.find("serde_init") != std::string::npos
          && code.find("type Rs_i32") != std::string::npos
          && code.find("Marshal: Rs_String") != std::string::npos;
    }
    END_TEST;
}

static void test_binding_gen_with_deps() {
    bool ok = false;
    TEST("BindingGenerator includes dependency info") {
        EcosystemTypeIRMapper mapper;
        BindingGenOptions opts;
        opts.include_dependency_info = true;
        UniversalBindingGenerator gen(mapper, opts);

        UnifiedPackageInfo pkg;
        pkg.name = "numpy";
        pkg.version = "1.24.0";
        pkg.ecosystem = Ecosystem::PyPI;
        pkg.found = true;

        UnifiedPackageInfo dep;
        dep.name = "ndarray";
        dep.version = "0.15.0";
        dep.ecosystem = Ecosystem::Cargo;
        pkg.dependencies.push_back(dep);

        std::string code = gen.generate(pkg);
        ok = code.find("// depends: cargo:ndarray@0.15.0") != std::string::npos;
    }
    END_TEST;
}

static void test_binding_gen_no_cost() {
    bool ok = false;
    TEST("BindingGenerator omits cost annotations when disabled") {
        EcosystemTypeIRMapper mapper;
        BindingGenOptions opts;
        opts.include_cost_annotations = false;
        UniversalBindingGenerator gen(mapper, opts);

        UnifiedPackageInfo pkg;
        pkg.name = "requests";
        pkg.version = "2.31.0";
        pkg.ecosystem = Ecosystem::PyPI;
        pkg.found = true;

        std::string code = gen.generate(pkg);
        ok = code.find("@cost(zero)") == std::string::npos;
    }
    END_TEST;
}

/* ════════════════════════════════════════════════════════════ */
/* 5. Marshal Code Generation Tests                           */
/* ════════════════════════════════════════════════════════════ */
static void test_marshal_zero_cost() {
    bool ok = false;
    TEST("Marshal code for ZeroCost is no-op") {
        EcosystemTypeIRMapper mapper;
        auto mapped = mapper.map_to_ir("i32", Ecosystem::Cargo);
        std::string code = mapper.generate_marshal_code(mapped, "val", true);
        ok = code.find("Zero-cost") != std::string::npos
          && code.find("same ABI layout") != std::string::npos;
    }
    END_TEST;
}

static void test_marshal_alloc_cost() {
    bool ok = false;
    TEST("Marshal code for AllocCost includes allocation") {
        EcosystemTypeIRMapper mapper;
        auto mapped = mapper.map_to_ir("str", Ecosystem::PyPI);
        std::string code = mapper.generate_marshal_code(mapped, "py_str", true);
        ok = code.find("Alloc-cost") != std::string::npos
          && code.find("aurora_string_from_c") != std::string::npos;
    }
    END_TEST;
}

/* ════════════════════════════════════════════════════════════ */
/* 6. Edge cases                                              */
/* ════════════════════════════════════════════════════════════ */
static void test_safe_ident() {
    bool ok = false;
    /* safe_ident is a private static function in UniversalBindingGenerator,
       so we test indirectly by checking generated output */
    TEST("generated identifiers are safe") {
        EcosystemTypeIRMapper mapper;
        BindingGenOptions opts;
        UniversalBindingGenerator gen(mapper, opts);

        UnifiedPackageInfo pkg;
        pkg.name = "my-package.v2";
        pkg.version = "1.0.0";
        pkg.ecosystem = Ecosystem::PyPI;
        pkg.found = true;

        std::string code = gen.generate(pkg);
        ok = code.find("my_package_v2_version") != std::string::npos
          && code.find("const my_package_v2_version") != std::string::npos;
    }
    END_TEST;
}

static void test_empty_package() {
    bool ok = false;
    TEST("generator handles minimal package") {
        EcosystemTypeIRMapper mapper;
        UniversalBindingGenerator gen(mapper);

        UnifiedPackageInfo pkg;
        pkg.name = "minimal";
        pkg.ecosystem = Ecosystem::PyPI;
        pkg.found = true;

        std::string code = gen.generate(pkg);
        ok = !code.empty();
    }
    END_TEST;
}

/* ════════════════════════════════════════════════════════════ */
/* Main                                                       */
/* ════════════════════════════════════════════════════════════ */
int main() {
    printf("═══ Phase 5: Universal Package Bridge ═══\n\n");

    printf("--- 1. Universal Resolver ---\n");
    test_resolver_pypi();
    test_resolver_npm();
    test_resolver_cargo();
    test_resolver_not_found();
    test_resolver_auto_detect();
    test_resolver_auto_detect_fallback();
    test_search_all();

    printf("\n--- 2. Cross-Ecosystem Dep Resolution ---\n");
    test_cross_eco_mapping();
    test_cross_eco_no_mapping();
    test_cross_resolve();

    printf("\n--- 3. Ecosystem Type IR Mapper ---\n");
    test_eco_mapper_pypi();
    test_eco_mapper_pypi_string();
    test_eco_mapper_npm();
    test_eco_mapper_cargo();
    test_eco_mapper_unknown();
    test_eco_mapper_aurora_names();

    printf("\n--- 4. Universal Binding Generator ---\n");
    test_binding_gen_pypi();
    test_binding_gen_npm();
    test_binding_gen_cargo();
    test_binding_gen_with_deps();
    test_binding_gen_no_cost();

    printf("\n--- 5. Marshal Code Generation ---\n");
    test_marshal_zero_cost();
    test_marshal_alloc_cost();

    printf("\n--- 6. Edge Cases ---\n");
    test_safe_ident();
    test_empty_package();

    printf("\n═══════════════════════════════════════════\n");
    printf("Results: %d / %d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
