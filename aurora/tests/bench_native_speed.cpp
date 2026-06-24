#include <cstdio>
#include <cstdint>
#include <chrono>
#include <string>
#include <functional>

#include "runtime/interop/type_ir.hpp"
#include "runtime/interop/type_mapping.hpp"
#include "runtime/interop/ffi_ownership.hpp"
#include "runtime/interop/ref_count_bridge.hpp"
#include "runtime/interop/eco_type_ir_mapper.hpp"
#include "runtime/interop/universal_binding_gen.hpp"
#include "runtime/interop/universal_resolver.hpp"

/* ════════════════════════════════════════════════════════════
   Native Speed Benchmark — 1 Billion Iterations
   ════════════════════════════════════════════════════════════ */

using Clock = std::chrono::high_resolution_clock;

static int64_t ns_elapsed(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

struct BenchResult {
    const char* name;
    int64_t     iterations;
    double      ns_per_op;
    double      ops_per_sec;
    bool        native_speed;
};

/* Accumulate results to prevent optimization */
static int64_t g_sink = 0;

static BenchResult run_bench(const char* name, int64_t iterations,
                              std::function<void(int64_t)> fn)
{
    auto start = Clock::now();
    fn(iterations);
    auto end = Clock::now();

    int64_t ns = ns_elapsed(start, end);
    double ns_per_op = (double)ns / (double)iterations;
    double ops_per_sec = 1.0e9 / ns_per_op;

    bool native = ops_per_sec > 100'000'000.0;

    BenchResult r = {name, iterations, ns_per_op, ops_per_sec, native};
    return r;
}

static void print_result(const BenchResult& r) {
    const char* verdict = r.native_speed ? "NATIVE" : "SLOW";
    const char* unit = "ops";
    if (r.ns_per_op < 1.0)
        printf("  %-46s %s  |  %8.2f ps/op  |  %10.2f M %s/sec\n",
               r.name, verdict, r.ns_per_op * 1000.0, r.ops_per_sec / 1.0e6, unit);
    else
        printf("  %-46s %s  |  %8.2f ns/op  |  %10.2f M %s/sec\n",
               r.name, verdict, r.ns_per_op, r.ops_per_sec / 1.0e6, unit);
}

/* ════════════════════════════════════════════════════════════ */
/* Phase 1: Type IR Lookup                                    */
/* ════════════════════════════════════════════════════════════ */
static void bench_type_ir_lookup() {
    printf("\n--- Phase 1: Type IR Lookup ---\n");

    InteropTypeRegistry reg;
    reg.register_type("i32", InteropType::make_primitive(InteropTypeKind::Int32, "i32"));
    reg.register_type("f64", InteropType::make_primitive(InteropTypeKind::Float64, "f64"));
    reg.register_type("string", InteropType::make_primitive(InteropTypeKind::String, "string"));
    reg.register_type("bool", InteropType::make_primitive(InteropTypeKind::Bool, "bool"));
    reg.register_type("pointer", InteropType::make_pointer("void"));

    int64_t N = 1'000'000'000;

    auto r = run_bench("unordered_map find (hit)", N, [&](int64_t n) {
        int64_t s = 0;
        for (int64_t i = 0; i < n; i++) {
            auto* t = reg.get_type("i32");
            if (t) s += (int64_t)t->kind;
        }
        g_sink += s;
    });
    print_result(r);

    r = run_bench("unordered_map find (miss)", N, [&](int64_t n) {
        int64_t s = 0;
        for (int64_t i = 0; i < n; i++) {
            auto* t = reg.get_type("void");
            if (t) s += (int64_t)t->kind;
        }
        g_sink += s;
    });
    print_result(r);
}

/* ════════════════════════════════════════════════════════════ */
/* Phase 1: Type Mapping Cost Resolution                      */
/* ════════════════════════════════════════════════════════════ */
static void bench_type_mapping() {
    printf("\n--- Phase 1: Type Mapping Cost Resolution ---\n");

    InteropTypeRegistry reg;
    reg.register_type("i32", InteropType::make_primitive(InteropTypeKind::Int32, "i32"));
    reg.register_type("f64", InteropType::make_primitive(InteropTypeKind::Float64, "f64"));
    reg.register_type("string", InteropType::make_primitive(InteropTypeKind::String, "string"));

    TypeMappingEngine engine;
    AuroraMapper aurora_map;
    CMapper c_map;
    CppMapper cpp_map;
    PythonMapper py_map;
    engine.register_mapper(&aurora_map);
    engine.register_mapper(&c_map);
    engine.register_mapper(&cpp_map);
    engine.register_mapper(&py_map);
    engine.init_builtins();

    int64_t N = 10'000'000;

    auto r = run_bench("Aurora->C i32 (ZeroCost)", N, [&](int64_t n) {
        int64_t s = 0;
        for (int64_t i = 0; i < n; i++) {
            MappedType m = engine.translate(InteropLang::Aurora, "i32", InteropLang::C);
            s += (int64_t)m.cost;
        }
        g_sink += s;
    });
    print_result(r);

    r = run_bench("Aurora->Python string (AllocCost)", N, [&](int64_t n) {
        int64_t s = 0;
        for (int64_t i = 0; i < n; i++) {
            MappedType m = engine.translate(InteropLang::Aurora, "string", InteropLang::Python);
            s += (int64_t)m.cost;
        }
        g_sink += s;
    });
    print_result(r);
}

/* ════════════════════════════════════════════════════════════ */
/* Phase 4: AuroraARC retain/release                          */
/* ════════════════════════════════════════════════════════════ */
static void bench_aurora_arc() {
    printf("\n--- Phase 4: AuroraARC (Atomic Ref-Count) ---\n");

    int64_t N = 1'000'000'000;

    auto r = run_bench("AuroraARC retain()", N, [&](int64_t n) {
        AuroraARC arc(1);
        int64_t s = 0;
        for (int64_t i = 0; i < n; i++) {
            s += arc.retain();
        }
        g_sink += s;
    });
    print_result(r);

    r = run_bench("AuroraARC release()", N, [&](int64_t n) {
        AuroraARC arc(n + 1);
        int64_t s = 0;
        for (int64_t i = 0; i < n; i++) {
            s += arc.release();
        }
        g_sink += s;
    });
    print_result(r);

    r = run_bench("AuroraARC retain+release pair", N, [&](int64_t n) {
        AuroraARC arc(1);
        int64_t s = 0;
        for (int64_t i = 0; i < n; i++) {
            s += arc.retain();
            s += arc.release();
        }
        g_sink += s;
    });
    print_result(r);
}

/* ════════════════════════════════════════════════════════════ */
/* Phase 4: FFI Ownership Resolver                            */
/* ════════════════════════════════════════════════════════════ */
static void bench_ffi_ownership() {
    printf("\n--- Phase 4: FFI Ownership Resolver ---\n");

    FFIOwnershipResolver resolver;
    int64_t N = 1'000'000'000;

    auto r = run_bench("resolve int32 (switch)", N, [&](int64_t n) {
        int64_t s = 0;
        for (int64_t i = 0; i < n; i++) {
            s += (int64_t)resolver.resolve(InteropTypeKind::Int32, false, false);
        }
        g_sink += s;
    });
    print_result(r);

    r = run_bench("resolve pointer (borrow)", N, [&](int64_t n) {
        int64_t s = 0;
        for (int64_t i = 0; i < n; i++) {
            s += (int64_t)resolver.resolve(InteropTypeKind::Pointer, true, false);
        }
        g_sink += s;
    });
    print_result(r);

    r = run_bench("resolve string (move)", N, [&](int64_t n) {
        int64_t s = 0;
        for (int64_t i = 0; i < n; i++) {
            s += (int64_t)resolver.resolve(InteropTypeKind::String, false, true);
        }
        g_sink += s;
    });
    print_result(r);
}

/* ════════════════════════════════════════════════════════════ */
/* Phase 5: Ecosystem Type IR Mapper                          */
/* ════════════════════════════════════════════════════════════ */
static void bench_eco_mapper() {
    printf("\n--- Phase 5: Ecosystem Type IR Mapper ---\n");

    EcosystemTypeIRMapper mapper;
    int64_t N = 1'000'000'000;

    auto r = run_bench("map PyPI int (vector scan)", N, [&](int64_t n) {
        int64_t s = 0;
        for (int64_t i = 0; i < n; i++) {
            MappedType m = mapper.map_to_ir("int", Ecosystem::PyPI);
            s += (int64_t)m.cost;
        }
        g_sink += s;
    });
    print_result(r);

    r = run_bench("to_aurora PyPI str", 10'000'000, [&](int64_t n) {
        int64_t s = 0;
        for (int64_t i = 0; i < n; i++) {
            std::string a = mapper.to_aurora("str", Ecosystem::PyPI);
            s += a.size();
        }
        g_sink += s;
    });
    print_result(r);

    r = run_bench("map Cargo String (AllocCost)", N, [&](int64_t n) {
        int64_t s = 0;
        for (int64_t i = 0; i < n; i++) {
            MappedType m = mapper.map_to_ir("String", Ecosystem::Cargo);
            s += (int64_t)m.cost;
        }
        g_sink += s;
    });
    print_result(r);
}

/* ════════════════════════════════════════════════════════════ */
/* Phase 5: Marshal Code Generation                           */
/* ════════════════════════════════════════════════════════════ */
static void bench_marshal_codegen() {
    printf("\n--- Phase 5: Marshal Code Generation ---\n");

    EcosystemTypeIRMapper mapper;
    MappedType zero_type = mapper.map_to_ir("i32", Ecosystem::Cargo);
    MappedType alloc_type = mapper.map_to_ir("str", Ecosystem::PyPI);

    int64_t N = 1'000'000;

    auto r = run_bench("marshal codegen ZeroCost", N, [&](int64_t n) {
        int64_t s = 0;
        for (int64_t i = 0; i < n; i++) {
            std::string code = mapper.generate_marshal_code(zero_type, "x", true);
            s += code.size();
        }
        g_sink += s;
    });
    print_result(r);

    r = run_bench("marshal codegen AllocCost", N, [&](int64_t n) {
        int64_t s = 0;
        for (int64_t i = 0; i < n; i++) {
            std::string code = mapper.generate_marshal_code(alloc_type, "s", true);
            s += code.size();
        }
        g_sink += s;
    });
    print_result(r);
}

/* ════════════════════════════════════════════════════════════ */
/* Phase 5: Binding Generation Throughput                     */
/* ════════════════════════════════════════════════════════════ */
static void bench_binding_gen() {
    printf("\n--- Phase 5: Universal Binding Generator ---\n");

    EcosystemTypeIRMapper mapper;
    BindingGenOptions opts;
    UniversalBindingGenerator gen(mapper, opts);

    UnifiedPackageInfo pkg;
    pkg.name = "requests";
    pkg.version = "2.31.0";
    pkg.description = "HTTP library for Python";
    pkg.ecosystem = Ecosystem::PyPI;
    pkg.found = true;

    int64_t N = 10'000;

    auto start = Clock::now();
    size_t total_bytes = 0;
    for (int64_t i = 0; i < N; i++) {
        std::string code = gen.generate(pkg);
        total_bytes += code.size();
    }
    auto end = Clock::now();

    int64_t ns = ns_elapsed(start, end);
    double sec = (double)ns / 1.0e9;
    double mb_per_sec = (double)total_bytes / (1024.0 * 1024.0) / sec;

    printf("  %-46s  %8.2f sec  |  %8.2f MB/sec  |  %10.0f gen/sec\n",
           "UniversalBindingGenerator", sec, mb_per_sec, (double)N / sec);
    printf("    → %zu KB/output, %dK generations in %.2fs (extrapolated 1M in %.0fs)\n",
           total_bytes / N / 1024, (int)(N / 1000), sec, sec * 100.0);
}

/* ════════════════════════════════════════════════════════════ */
/* Phase 1: InteropType construction                          */
/* ════════════════════════════════════════════════════════════ */
static void bench_type_construction() {
    printf("\n--- Phase 1: InteropType Construction ---\n");

    int64_t N = 100'000'000;

    auto r = run_bench("make_primitive(int32)", N, [&](int64_t n) {
        int64_t s = 0;
        for (int64_t i = 0; i < n; i++) {
            InteropType t = InteropType::make_primitive(InteropTypeKind::Int32, "i32");
            s += (int64_t)t.kind;
        }
        g_sink += s;
    });
    print_result(r);

    r = run_bench("make_pointer(void)", N, [&](int64_t n) {
        int64_t s = 0;
        for (int64_t i = 0; i < n; i++) {
            InteropType t = InteropType::make_pointer("void");
            s += (int64_t)t.kind;
        }
        g_sink += s;
    });
    print_result(r);
}

/* ════════════════════════════════════════════════════════════ */
/* Phase 4: RefCountAdapter                                   */
/* ════════════════════════════════════════════════════════════ */
static void bench_refcount_adapter() {
    printf("\n--- Phase 4: RefCountAdapter ---\n");

    RefCountAdapter adapter;
    int64_t N = 100'000'000;

    auto r = run_bench("is_registered (bit check)", N, [&](int64_t n) {
        int64_t s = 0;
        for (int64_t i = 0; i < n; i++) {
            s += adapter.is_registered(RefCountProtocol::Python) ? 1 : 0;
        }
        g_sink += s;
    });
    print_result(r);
}

/* ════════════════════════════════════════════════════════════ */
/* Pure C baseline for comparison                             */
/* ════════════════════════════════════════════════════════════ */
static void bench_baseline() {
    printf("\n--- Baseline (pure integer ops) ---\n");

    int64_t N = 1'000'000'000;

    auto r = run_bench("int64 add baseline", N, [&](int64_t n) {
        int64_t s = 0;
        for (int64_t i = 0; i < n; i++) {
            s += i;
        }
        g_sink += s;
    });
    print_result(r);

    r = run_bench("int64 cmp + branch", N, [&](int64_t n) {
        int64_t s = 0;
        for (int64_t i = 0; i < n; i++) {
            if (i & 1) s += 1;
        }
        g_sink += s;
    });
    print_result(r);
}

/* ════════════════════════════════════════════════════════════ */
/* Main                                                       */
/* ════════════════════════════════════════════════════════════ */
int main() {
    setbuf(stdout, NULL); /* unbuffered output for real-time results */
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║     1 BILLION ITERATIONS — NATIVE SPEED BENCHMARK           ║\n");
    printf("║     \"If it's not native speed, it's not Aurora\"             ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\nThreshold: NATIVE > 100M ops/sec  |  SLOW < 100M ops/sec\n");
    printf("ps = picoseconds  |  ns = nanoseconds\n\n");

    bench_baseline();
    bench_type_ir_lookup();
    bench_type_mapping();
    bench_type_construction();
    bench_aurora_arc();
    bench_ffi_ownership();
    bench_refcount_adapter();
    bench_eco_mapper();
    bench_marshal_codegen();
    bench_binding_gen();

    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║     BENCHMARK COMPLETE                                      ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("(g_sink = %lld to prevent over-optimization)\n", (long long)g_sink);
    return 0;
}
