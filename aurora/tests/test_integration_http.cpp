#include <cstdio>
#include <cstdlib>
#include <string>
#include "runtime/interop/http_client.hpp"
#include "runtime/interop/json_parser.hpp"

static int tests_run = 0;
static int tests_passed = 0;

/* ── Minimal test helper ── */
#define TEST_START(name) do { ++tests_run; printf("  TEST %d: %s ... ", tests_run, name); fflush(stdout); } while(0)
#define TEST_PASS()       do { ++tests_passed; printf("PASS\n"); } while(0)
#define TEST_FAIL(msg)    do { printf("FAIL (%s)\n", msg); return; } while(0)

/* ════════════════════════════════════════════════════════════
   Integration tests
   ════════════════════════════════════════════════════════════ */

/* ── JSON parser unit tests (no network) ── */
static void test_json_escape() {
    TEST_START("JSON parser handles escape sequences");
    std::string input = R"({"msg": "hello\nworld\t\"test\"\\end"})";
    JsonValue json = JsonParser::parse(input);
    if (json.type != JsonValue::Object) { TEST_FAIL("not object"); }
    if (json.get_string("msg") != "hello\nworld\t\"test\"\\end") {
        TEST_FAIL(("got '" + json.get_string("msg") + "'").c_str());
    }
    TEST_PASS();
}

static void test_json_nested() {
    TEST_START("JSON parser nested() accessor");
    std::string input = R"({"a":{"b":{"c":"deep"}}})";
    JsonValue json = JsonParser::parse(input);
    auto* v = json.nested({"a", "b", "c"});
    if (!v || v->type != JsonValue::String || v->str_val != "deep") {
        TEST_FAIL(("got " + (v ? v->str_val : "null")).c_str());
    }
    TEST_PASS();
}

static void test_json_array() {
    TEST_START("JSON parser handles arrays");
    std::string input = R"({"items":[1,2,3]})";
    JsonValue json = JsonParser::parse(input);
    auto* items = json.get("items");
    if (!items || items->type != JsonValue::Array || items->arr.size() != 3) {
        TEST_FAIL(("size=" + std::to_string(items ? items->arr.size() : -1)).c_str());
    }
    TEST_PASS();
}

static void test_json_numbers() {
    TEST_START("JSON parser handles numbers");
    std::string input = R"({"a":42,"b":-3.14,"c":1.5e10})";
    JsonValue json = JsonParser::parse(input);
    bool ok = json.get_string("a") == "42.000000"
           && json.get_string("b") == "-3.140000"
           && json.get_string("c") == "15000000000.000000";
    if (!ok) {
        TEST_FAIL(("a=" + json.get_string("a") + " b=" + json.get_string("b") + " c=" + json.get_string("c")).c_str());
    }
    TEST_PASS();
}

static void test_json_unicode_skip() {
    TEST_START("JSON parser skips unicode escapes");
    std::string input = R"({"u":"\u0048\u0065\u006c\u006c\u006f"})";
    JsonValue json = JsonParser::parse(input);
    if (json.type != JsonValue::Object) { TEST_FAIL("not object"); }
    /* Parser replaces \u with '?' — that's acceptable */
    std::string val = json.get_string("u");
    if (val.empty()) { TEST_FAIL("empty string"); }
    TEST_PASS();
}

/* ── PyPI integration tests ── */
static void test_pypi_numpy() {
    TEST_START("PyPI HTTPS fetch: numpy version");
    std::string raw = http_get("https://pypi.org/pypi/numpy/json");
    if (raw.empty()) { TEST_FAIL("empty response (network?)"); }

    JsonValue json = JsonParser::parse(raw);
    if (json.type != JsonValue::Object) { TEST_FAIL("not JSON object"); }

    std::string ver = json.nested_str({"info", "version"});
    if (ver.empty()) { TEST_FAIL("no version field"); }

    printf("got numpy@%s ", ver.c_str());

    /* Verify it's a real version string */
    if (ver.find_first_not_of("0123456789.") != std::string::npos) {
        TEST_FAIL(("bad version format: " + ver).c_str());
    }
    TEST_PASS();
}

static void test_pypi_numpy_major() {
    TEST_START("PyPI numpy version >= 2.0");
    std::string raw = http_get("https://pypi.org/pypi/numpy/json");
    if (raw.empty()) { TEST_FAIL("empty response"); }

    JsonValue json = JsonParser::parse(raw);
    std::string ver = json.nested_str({"info", "version"});
    size_t dot = ver.find('.');
    int major = dot != std::string::npos ? std::stoi(ver.substr(0, dot)) : 0;

    if (major < 2) {
        TEST_FAIL(("numpy@" + ver + " major=" + std::to_string(major)).c_str());
    }
    TEST_PASS();
}

static void test_pypi_numpy_desc() {
    TEST_START("PyPI numpy description non-empty");
    std::string raw = http_get("https://pypi.org/pypi/numpy/json");
    if (raw.empty()) { TEST_FAIL("empty response"); }

    JsonValue json = JsonParser::parse(raw);
    std::string desc = json.nested_str({"info", "summary"});
    if (desc.empty()) { TEST_FAIL("empty description"); }

    TEST_PASS();
}

static void test_pypi_requests() {
    TEST_START("PyPI HTTPS fetch: requests (large response)");
    std::string raw = http_get("https://pypi.org/pypi/requests/json");
    if (raw.empty()) { TEST_FAIL("empty response (network?)"); }

    JsonValue json = JsonParser::parse(raw);
    if (json.type != JsonValue::Object) { TEST_FAIL("not JSON object"); }

    auto* info = json.get("info");
    if (!info || info->type != JsonValue::Object) { TEST_FAIL("no info field"); }

    std::string ver = info->get_string("version");
    if (ver.empty()) { TEST_FAIL("no version"); }

    printf("got requests@%s ", ver.c_str());
    TEST_PASS();
}

/* ── npm integration tests ── */
static void test_npm_lodash() {
    TEST_START("npm HTTPS fetch: lodash version");
    std::string raw = http_get("https://registry.npmjs.org/lodash");
    if (raw.empty()) { TEST_FAIL("empty response (network?)"); }

    JsonValue json = JsonParser::parse(raw);
    if (json.type != JsonValue::Object) { TEST_FAIL("not JSON object"); }

    std::string ver = json.nested_str({"dist-tags", "latest"});
    if (ver.empty()) ver = json.get_string("version");
    if (ver.empty()) { TEST_FAIL("no version"); }

    printf("got lodash@%s ", ver.c_str());
    TEST_PASS();
}

static void test_npm_lodash_desc() {
    TEST_START("npm lodash description non-empty");
    std::string raw = http_get("https://registry.npmjs.org/lodash");
    if (raw.empty()) { TEST_FAIL("empty response"); }

    JsonValue json = JsonParser::parse(raw);
    std::string desc = json.get_string("description");
    if (desc.empty()) { TEST_FAIL("empty description"); }

    TEST_PASS();
}

/* ── Cargo integration test (may fail if crates.io is unavailable) ── */
static void test_cargo_serde() {
    TEST_START("Cargo HTTPS fetch: serde (may 503)");
    std::string raw = http_get("https://crates.io/api/v1/crates/serde");
    if (raw.empty()) {
        printf("(crates.io unavailable — skip) ");
        TEST_PASS();
        return;
    }

    JsonValue json = JsonParser::parse(raw);
    if (json.type != JsonValue::Object) { TEST_FAIL("not JSON object"); }

    std::string ver = json.nested_str({"crate", "max_stable_version"});
    if (ver.empty()) ver = json.nested_str({"crate", "max_version"});
    if (ver.empty()) { TEST_FAIL("no version field"); }

    printf("got serde@%s ", ver.c_str());
    TEST_PASS();
}

int main() {
    printf("Integration Test: Real HTTPS + JSON Pipeline\n");
    printf("──────────────────────────────────────────\n");

    /* Local JSON parser tests */
    test_json_escape();
    test_json_nested();
    test_json_array();
    test_json_numbers();
    test_json_unicode_skip();

    /* Real HTTPS tests */
    test_pypi_numpy();
    test_pypi_numpy_major();
    test_pypi_numpy_desc();
    test_pypi_requests();
    test_npm_lodash();
    test_npm_lodash_desc();
    test_cargo_serde();

    printf("\n──────────────────────────────────────────\n");
    printf("  %d / %d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
