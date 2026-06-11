#include "runtime/interop/type_ir.hpp"
#include "runtime/interop/type_mapping.hpp"
#include "runtime/interop/type_serializer.hpp"
#include <cstdio>
#include <cassert>
#include <iostream>

static int g_tests = 0;
static int g_passed = 0;

#define TEST(name) do { \
    g_tests++; \
    printf("  TEST: %s ... ", name); \
    try {

#define END_TEST() \
        printf("PASS\n"); \
        g_passed++; \
    } catch (const std::exception& e) { \
        printf("FAIL: %s\n", e.what()); \
    } catch (...) { \
        printf("FAIL: unknown exception\n"); \
    } \
} while(0)

void test_type_ir_primitives() {
    TEST("primitive type creation") {
        auto t = InteropType::make_primitive(InteropTypeKind::Int32, "i32");
        assert(t.kind == InteropTypeKind::Int32);
        assert(t.name == "i32");
        assert(t.is_signed == true);
    } END_TEST();

    TEST("make_int") {
        auto t = InteropType::make_int(32, true);
        assert(t.kind == InteropTypeKind::Int32);
        assert(t.name == "i32");

        auto u = InteropType::make_int(64, false);
        assert(u.kind == InteropTypeKind::UInt64);
        assert(u.name == "u64");
        assert(u.is_signed == false);
    } END_TEST();

    TEST("make_float") {
        auto t = InteropType::make_float(64);
        assert(t.kind == InteropTypeKind::Float64);
        assert(t.name == "f64");
    } END_TEST();

    TEST("type to_string") {
        assert(InteropType::make_int(32, true).to_string() == "i32");
        assert(InteropType::make_primitive(InteropTypeKind::Bool, "bool").to_string() == "bool");
        assert(InteropType::make_primitive(InteropTypeKind::String, "string").to_string() == "string");
    } END_TEST();
}

void test_type_ir_composite() {
    TEST("struct type") {
        auto t = InteropType::make_struct("Point", {
            {"x", "f64"}, {"y", "f64"}
        });
        assert(t.kind == InteropTypeKind::Struct);
        assert(t.name == "Point");
        assert(t.fields.size() == 2);
        assert(t.fields[0].name == "x");
    } END_TEST();

    TEST("enum type") {
        auto t = InteropType::make_enum("Color", {"Red", "Green", "Blue"});
        assert(t.kind == InteropTypeKind::Enum);
        assert(t.enum_variants.size() == 3);
        std::string s = t.to_string();
        assert(s.find("Color") != std::string::npos);
    } END_TEST();

    TEST("list type") {
        auto t = InteropType::make_list("i32");
        assert(t.kind == InteropTypeKind::List);
        assert(t.element_type == "i32");
        assert(t.to_string() == "list<i32>");
    } END_TEST();

    TEST("map type") {
        auto t = InteropType::make_map("string", "i32");
        assert(t.kind == InteropTypeKind::Map);
        assert(t.key_type == "string");
        assert(t.value_type == "i32");
    } END_TEST();

    TEST("optional type") {
        auto t = InteropType::make_optional("string");
        assert(t.kind == InteropTypeKind::Optional);
        assert(t.inner_type == "string");
        assert(t.to_string() == "string?");
    } END_TEST();

    TEST("result type") {
        auto t = InteropType::make_result("f64", "string");
        assert(t.kind == InteropTypeKind::Result);
        assert(t.inner_type == "f64");
        assert(t.error_type == "string");
    } END_TEST();

    TEST("function type") {
        auto t = InteropType::make_function({{"x", "i32"}, {"y", "i32"}}, "i32");
        assert(t.kind == InteropTypeKind::Function);
        assert(t.fn_params.size() == 2);
        assert(t.fn_return == "i32");
    } END_TEST();

    TEST("pointer type") {
        auto t = InteropType::make_pointer("i32");
        assert(t.kind == InteropTypeKind::Reference);
        assert(t.inner_type == "i32");

        auto u = InteropType::make_pointer("");
        assert(u.kind == InteropTypeKind::Pointer);
    } END_TEST();
}

void test_type_registry() {
    TEST("registry operations") {
        InteropTypeRegistry reg;
        reg.register_type("i32", InteropType::make_int(32, true));
        assert(reg.has_type("i32"));
        auto* t = reg.get_type("i32");
        assert(t != nullptr);
        assert(t->kind == InteropTypeKind::Int32);
    } END_TEST();

    TEST("registry mappings") {
        InteropTypeRegistry reg;
        reg.add_mapping("int", "i32");
        assert(reg.has_type("int"));
        auto* t = reg.get_type("int");
        assert(t != nullptr);
        assert(t->kind == InteropTypeKind::Int32);
        assert(t->name == "i32");
    } END_TEST();
}

void test_type_mapping_aurora() {
    TEST("Aurora mapper builtins") {
        InteropTypeRegistry reg;
        AuroraMapper mapper;
        mapper.register_builtins(reg);

        assert(reg.has_type("f64"));
        assert(reg.has_type("string"));

        auto mapped = AuroraMapper().map_to_ir("int", reg);
        assert(mapped.ir_type.kind == InteropTypeKind::Int32);

        auto mapped2 = mapper.map_to_ir("f64", reg);
        assert(mapped2.ir_type.kind == InteropTypeKind::Float64);
    } END_TEST();

    TEST("Aurora list type") {
        InteropTypeRegistry reg;
        AuroraMapper mapper;
        mapper.register_builtins(reg);

        auto mapped = mapper.map_to_ir("[]i32", reg);
        assert(mapped.ir_type.kind == InteropTypeKind::List);
        assert(mapped.ir_type.element_type == "i32");
    } END_TEST();

    TEST("Aurora round-trip") {
        InteropTypeRegistry reg;
        AuroraMapper mapper;
        mapper.register_builtins(reg);

        const char* types[] = {"void", "bool", "i32", "i64", "f32", "f64", "string", "cstring", nullptr};
        for (int i = 0; types[i]; i++) {
            auto mapped = mapper.map_to_ir(types[i], reg);
            std::string back = mapper.map_from_ir(mapped.ir_type, reg);
            assert(back == types[i]);
        }
    } END_TEST();
}

void test_type_mapping_c() {
    TEST("C mapper types") {
        InteropTypeRegistry reg;
        CMapper mapper;
        mapper.register_builtins(reg);

        assert(reg.has_type("int"));
        assert(reg.has_type("double"));
        assert(reg.has_type("char*"));

        auto t1 = mapper.map_to_ir("int", reg);
        assert(t1.ir_type.kind == InteropTypeKind::Int32);

        auto t2 = mapper.map_to_ir("double", reg);
        assert(t2.ir_type.kind == InteropTypeKind::Float64);

        auto t3 = mapper.map_to_ir("char*", reg);
        assert(t3.ir_type.kind == InteropTypeKind::CString);

        auto t4 = mapper.map_to_ir("void*", reg);
        assert(t4.ir_type.kind == InteropTypeKind::Pointer);
    } END_TEST();

    TEST("C struct/enum detection") {
        InteropTypeRegistry reg;
        CMapper mapper;
        mapper.register_builtins(reg);

        auto t1 = mapper.map_to_ir("struct Foo", reg);
        assert(t1.ir_type.kind == InteropTypeKind::Struct);
        assert(t1.ir_type.name == "Foo");

        auto t2 = mapper.map_to_ir("enum Color", reg);
        assert(t2.ir_type.kind == InteropTypeKind::Enum);

        auto t3 = mapper.map_to_ir("union Data", reg);
        assert(t3.ir_type.kind == InteropTypeKind::Union);
    } END_TEST();
}

void test_type_mapping_python() {
    TEST("Python mapper types") {
        InteropTypeRegistry reg;
        PythonMapper mapper;
        mapper.register_builtins(reg);

        auto t1 = mapper.map_to_ir("int", reg);
        assert(t1.ir_type.kind == InteropTypeKind::Int64);

        auto t2 = mapper.map_to_ir("str", reg);
        assert(t2.ir_type.kind == InteropTypeKind::String);

        auto t3 = mapper.map_to_ir("list[int]", reg);
        assert(t3.ir_type.kind == InteropTypeKind::List);
        assert(t3.ir_type.element_type == "int");

        auto t4 = mapper.map_to_ir("dict[str, int]", reg);
        assert(t4.ir_type.kind == InteropTypeKind::Map);
        assert(t4.ir_type.key_type == "str");

        auto t5 = mapper.map_to_ir("Optional[str]", reg);
        assert(t5.ir_type.kind == InteropTypeKind::Optional);
        assert(t5.ir_type.inner_type == "str");
    } END_TEST();
}

void test_type_mapping_javascript() {
    TEST("JavaScript mapper types") {
        InteropTypeRegistry reg;
        JavaScriptMapper mapper;
        mapper.register_builtins(reg);

        assert(reg.has_type("number"));
        assert(reg.has_type("string"));
        assert(reg.has_type("boolean"));

        auto t1 = mapper.map_to_ir("Array<string>", reg);
        assert(t1.ir_type.kind == InteropTypeKind::List);
        assert(t1.ir_type.element_type == "string");

        auto t2 = mapper.map_to_ir("Promise<number>", reg);
        assert(t2.ir_type.kind == InteropTypeKind::Future);
        assert(t2.ir_type.inner_type == "number");
    } END_TEST();
}

void test_type_mapping_rust() {
    TEST("Rust mapper types") {
        InteropTypeRegistry reg;
        RustMapper mapper;
        mapper.register_builtins(reg);

        auto t1 = mapper.map_to_ir("Vec<i32>", reg);
        assert(t1.ir_type.kind == InteropTypeKind::List);
        assert(t1.ir_type.element_type == "i32");

        auto t2 = mapper.map_to_ir("Option<String>", reg);
        assert(t2.ir_type.kind == InteropTypeKind::Optional);
        assert(t2.ir_type.inner_type == "String");

        auto t3 = mapper.map_to_ir("Result<f64, String>", reg);
        assert(t3.ir_type.kind == InteropTypeKind::Result);
        assert(t3.ir_type.inner_type == "f64");
        assert(t3.ir_type.error_type == "String");

        auto t4 = mapper.map_to_ir("Box<dyn Fn>", reg);
        assert(t4.ir_type.kind == InteropTypeKind::Reference);

        auto t5 = mapper.map_to_ir("HashMap<String, i32>", reg);
        assert(t5.ir_type.kind == InteropTypeKind::Map);
    } END_TEST();
}

void test_cross_language_translation() {
    TEST("C ↔ Aurora translation") {
        TypeMappingEngine engine;
        engine.init_builtins();
        CMapper c_mapper;
        AuroraMapper au_mapper;
        engine.register_mapper(&c_mapper);
        engine.register_mapper(&au_mapper);

        auto result = engine.translate(InteropLang::C, "int", InteropLang::Aurora);
        assert(result.ir_type.kind == InteropTypeKind::Int32);
        assert(result.native_type == "i32");

        auto result2 = engine.translate(InteropLang::C, "double", InteropLang::Aurora);
        assert(result2.ir_type.kind == InteropTypeKind::Float64);
    } END_TEST();

    TEST("Zero-cost translation detection") {
        TypeMappingEngine engine;
        engine.init_builtins();
        CMapper c_mapper;
        AuroraMapper au_mapper;
        engine.register_mapper(&c_mapper);
        engine.register_mapper(&au_mapper);

        auto r = engine.translate(InteropLang::C, "int", InteropLang::Aurora);
        assert(r.cost == InteropCost::ZeroCost);
        assert(r.is_trivially_castable);

        auto r2 = engine.translate(InteropLang::Aurora, "pointer", InteropLang::C);
        assert(r2.cost == InteropCost::ZeroCost);

        auto r3 = engine.translate(InteropLang::Aurora, "i32", InteropLang::Aurora);
        assert(r3.cost == InteropCost::ZeroCost);
    } END_TEST();

    TEST("Python ↔ Aurora translation") {
        TypeMappingEngine engine;
        engine.init_builtins();
        PythonMapper py_mapper;
        AuroraMapper au_mapper;
        engine.register_mapper(&py_mapper);
        engine.register_mapper(&au_mapper);

        auto result = engine.translate(InteropLang::Python, "int", InteropLang::Aurora);
        assert(result.ir_type.kind == InteropTypeKind::Int64);

        auto result2 = engine.translate(InteropLang::Python, "str", InteropLang::Aurora);
        assert(result2.ir_type.kind == InteropTypeKind::String);
    } END_TEST();

    TEST("Rust ↔ Python translation") {
        TypeMappingEngine engine;
        engine.init_builtins();
        RustMapper rs_mapper;
        PythonMapper py_mapper;
        engine.register_mapper(&rs_mapper);
        engine.register_mapper(&py_mapper);

        auto result = engine.translate(InteropLang::Rust, "Vec<i32>", InteropLang::Python);
        assert(result.ir_type.kind == InteropTypeKind::List);

        auto result2 = engine.translate(InteropLang::Rust, "Option<String>", InteropLang::Python);
        assert(result2.ir_type.kind == InteropTypeKind::Optional);
        assert(result2.native_type.find("Optional") != std::string::npos);
    } END_TEST();

    TEST("JavaScript ↔ Rust translation") {
        TypeMappingEngine engine;
        engine.init_builtins();
        JavaScriptMapper js_mapper;
        RustMapper rs_mapper;
        engine.register_mapper(&js_mapper);
        engine.register_mapper(&rs_mapper);

        auto result = engine.translate(InteropLang::JavaScript, "Array<number>", InteropLang::Rust);
        assert(result.ir_type.kind == InteropTypeKind::List);

        auto result2 = engine.translate(InteropLang::Rust, "HashMap<String, i32>", InteropLang::JavaScript);
        assert(result2.ir_type.kind == InteropTypeKind::Map);
    } END_TEST();
}

void test_serialization() {
    TEST("JSON serialization round-trip") {
        auto t = InteropType::make_struct("Person", {
            {"name", "string"}, {"age", "i32"}, {"active", "bool"}
        });
        std::string json = TypeSerializer::to_json(t);
        assert(!json.empty());
        assert(json.find("Person") != std::string::npos);
        assert(json.find("name") != std::string::npos);
    } END_TEST();

    TEST("JSON registry serialization") {
        InteropTypeRegistry reg;
        reg.register_type("Point", InteropType::make_struct("Point", {{"x", "f64"}, {"y", "f64"}}));
        reg.register_type("i32", InteropType::make_int(32, true));
        std::string json = TypeSerializer::to_json_registry(reg);
        assert(!json.empty());
    } END_TEST();

    TEST("JSON schema generation") {
        std::string schema = TypeSerializer::to_schema();
        assert(!schema.empty());
        assert(schema.find("$schema") != std::string::npos);
        assert(schema.find("Interop Type IR") != std::string::npos);
    } END_TEST();
}

void test_marshal_codegen() {
    TEST("marshal code generation — zero-cost path") {
        TypeMappingEngine engine;
        engine.init_builtins();

        MappedType from;
        from.ir_type = InteropType::make_int(32, true);
        from.native_type = "i32";
        from.cost = InteropCost::ZeroCost;
        from.is_trivially_castable = true;

        MappedType to;
        to.ir_type = InteropType::make_int(32, true);
        to.native_type = "int32_t";
        to.cost = InteropCost::ZeroCost;
        to.is_trivially_castable = true;

        std::string code = engine.generate_marshal_code(from, to, "x");
        assert(code.find("ZERO-COST") != std::string::npos);
    } END_TEST();

    TEST("marshal code generation — alloc-cost path") {
        TypeMappingEngine engine;
        engine.init_builtins();

        MappedType from;
        from.ir_type = InteropType::make_primitive(InteropTypeKind::String, "string");
        from.native_type = "std::string";
        from.cost = InteropCost::MarshalCost;

        MappedType to;
        to.ir_type = InteropType::make_primitive(InteropTypeKind::CString, "cstring");
        to.native_type = "const char*";
        to.cost = InteropCost::AllocCost;

        std::string code = engine.generate_marshal_code(from, to, "my_var");
        assert(code.find("c_str") != std::string::npos);
    } END_TEST();

    TEST("marshal code generation — unknown cost = error") {
        TypeMappingEngine engine;
        engine.init_builtins();

        MappedType from;
        from.ir_type.kind = InteropTypeKind::Unknown;
        from.native_type = "custom_t";
        from.cost = InteropCost::UnknownCost;

        MappedType to;
        to.ir_type.kind = InteropTypeKind::Unknown;
        to.native_type = "other_t";
        to.cost = InteropCost::UnknownCost;

        std::string code = engine.generate_marshal_code(from, to, "x");
        assert(code.find("COMPILE ERROR") != std::string::npos);
    } END_TEST();
}

int main() {
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║  Phase 1: Unified Type System — Test Suite      ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    printf("── Type IR Primitives ──\n");
    test_type_ir_primitives();

    printf("\n── Type IR Composite ──\n");
    test_type_ir_composite();

    printf("\n── Type Registry ──\n");
    test_type_registry();

    printf("\n── Aurora Type Mapping ──\n");
    test_type_mapping_aurora();

    printf("\n── C Type Mapping ──\n");
    test_type_mapping_c();

    printf("\n── Python Type Mapping ──\n");
    test_type_mapping_python();

    printf("\n── JavaScript Type Mapping ──\n");
    test_type_mapping_javascript();

    printf("\n── Rust Type Mapping ──\n");
    test_type_mapping_rust();

    printf("\n── Cross-Language Translation ──\n");
    test_cross_language_translation();

    printf("\n── Serialization ──\n");
    test_serialization();

    printf("\n── Marshal Codegen ──\n");
    test_marshal_codegen();

    printf("\n╔══════════════════════════════════════════════════╗\n");
    printf("║  Results: %d/%d passed                         ║\n", g_passed, g_tests);
    printf("╚══════════════════════════════════════════════════╝\n");

    return g_passed == g_tests ? 0 : 1;
}
