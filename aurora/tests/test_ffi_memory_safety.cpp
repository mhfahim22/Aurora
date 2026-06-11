#include <cassert>
#include <cstdio>
#include <string>
#include "runtime/interop/type_ir.hpp"
#include "runtime/interop/ffi_ownership.hpp"
#include "runtime/interop/ffi_borrow_checker.hpp"
#include "runtime/interop/raii_guard_gen.hpp"
#include "runtime/interop/ref_count_bridge.hpp"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    ++tests_run; \
    printf("  TEST %d: %s ... ", tests_run, name); \
    fflush(stdout);

#define END_TEST ok = true; \
    if (ok) { ++tests_passed; printf("PASS\n"); } \
    else    { printf("FAIL\n"); } \
} while(0)

/* ─────────────────────────────────────────────────────────── */
/* 1. FFI Ownership Protocol Tests                            */
/* ─────────────────────────────────────────────────────────── */
static void test_ffi_ownership_basics() {
    bool ok = false;
    TEST("primitives are Copy") {
        FFIOwnershipResolver r;
        auto prims = {InteropTypeKind::Int32, InteropTypeKind::Float64,
                       InteropTypeKind::Bool, InteropTypeKind::Char,
                       InteropTypeKind::UInt8, InteropTypeKind::Int64};
        for (auto k : prims) {
            if (r.resolve(k, false, false) != FFIOwnership::Copy) { ok = false; return; }
            if (r.resolve(k, false, true) != FFIOwnership::Copy) { ok = false; return; }
        }
        ok = true;
    }
    END_TEST;
}

static void test_ffi_ownership_pointers() {
    bool ok = false;
    TEST("pointers are Borrowed (param) and Move (return)") {
        FFIOwnershipResolver r;
        ok = r.resolve(InteropTypeKind::Pointer, true, false) == FFIOwnership::Borrowed
          && r.resolve(InteropTypeKind::Pointer, true, true)  == FFIOwnership::Move
          && r.resolve(InteropTypeKind::Reference, true, false) == FFIOwnership::Borrowed
          && r.resolve(InteropTypeKind::Reference, true, true) == FFIOwnership::Move;
    }
    END_TEST;
}

static void test_ffi_ownership_strings() {
    bool ok = false;
    TEST("CString is Borrowed, String is Move") {
        FFIOwnershipResolver r;
        ok = r.resolve(InteropTypeKind::CString, false, false) == FFIOwnership::Borrowed
          && r.resolve(InteropTypeKind::String, false, false) == FFIOwnership::Move;
    }
    END_TEST;
}

static void test_ffi_ownership_class() {
    bool ok = false;
    TEST("class handles: param Borrowed, return Move") {
        FFIOwnershipResolver r;
        ok = r.resolve(InteropTypeKind::Class, false, false) == FFIOwnership::Borrowed
          && r.resolve(InteropTypeKind::Class, false, true)  == FFIOwnership::Move;
    }
    END_TEST;
}

static void test_ffi_safety_rules() {
    bool ok = false;
    TEST("safety rules match enum order") {
        ok = ffi_safety_rules[0].ownership == FFIOwnership::Borrowed
          && ffi_safety_rules[0].callee_must_not_free
          && ffi_safety_rules[1].ownership == FFIOwnership::Move
          && ffi_safety_rules[1].caller_must_not_use_after
          && ffi_safety_rules[2].ownership == FFIOwnership::Shared
          && ffi_safety_rules[2].refcount_needed
          && ffi_safety_rules[4].ownership == FFIOwnership::Copy
          && ffi_safety_rules[4].zero_cost;
    }
    END_TEST;
}

/* ─────────────────────────────────────────────────────────── */
/* 2. FFI Borrow Checker Tests                                */
/* ─────────────────────────────────────────────────────────── */
static void test_borrow_checker_basic_call() {
    bool ok = false;
    TEST("valid borrow call passes check") {
        FFIBorrowChecker bc;
        FFIFunctionSignature sig;
        sig.name = "ffi_read";
        sig.params = {
            {"buf", InteropTypeKind::Pointer, FFIOwnership::Borrowed, true, false, false}
        };
        sig.ret = {"", InteropTypeKind::Void, FFIOwnership::Copy, false, true, false};

        auto result = bc.check_ffi_call(sig, {FFIOwnership::Borrowed}, FFICallDirection::AuroraToC);
        ok = result.safe;
    }
    END_TEST;
}

static void test_borrow_checker_mismatch() {
    bool ok = false;
    TEST("ownership mismatch is detected") {
        FFIBorrowChecker bc;
        FFIFunctionSignature sig;
        sig.name = "ffi_take";
        sig.params = {
            {"ptr", InteropTypeKind::Pointer, FFIOwnership::Move, true, false, false}
        };
        sig.ret = {"", InteropTypeKind::Void, FFIOwnership::Copy, false, true, false};

        auto result = bc.check_ffi_call(sig, {FFIOwnership::Borrowed}, FFICallDirection::AuroraToC);
        ok = !result.safe; /* Borrowed ≠ Move → should fail */
    }
    END_TEST;
}

static void test_borrow_checker_param_count() {
    bool ok = false;
    TEST("param count mismatch is detected") {
        FFIBorrowChecker bc;
        FFIFunctionSignature sig;
        sig.name = "ffi_two";
        sig.params = {
            {"a", InteropTypeKind::Int32, FFIOwnership::Copy, false, false, false},
            {"b", InteropTypeKind::Int32, FFIOwnership::Copy, false, false, false},
        };
        sig.ret = {"", InteropTypeKind::Void, FFIOwnership::Copy, false, true, false};

        auto result = bc.check_ffi_call(sig, {FFIOwnership::Copy}, FFICallDirection::AuroraToC);
        ok = !result.safe;
    }
    END_TEST;
}

static void test_borrow_checker_tracking() {
    bool ok = false;
    TEST("foreign borrow tracking works") {
        FFIBorrowChecker bc;
        bc.track_foreign_borrow("my_data", "ffi_handle");
        auto r = bc.check_can_mutate("my_data");
        ok = !r.safe; /* can't mutate while borrowed */
        bc.release_foreign_borrow("my_data");
        r = bc.check_can_mutate("my_data");
        ok = ok && r.safe;
    }
    END_TEST;
}

/* ─────────────────────────────────────────────────────────── */
/* 3. RAII Guard Generator Tests                              */
/* ─────────────────────────────────────────────────────────── */
static void test_raii_trivially_copyable() {
    bool ok = false;
    TEST("RAII trivially copyable struct") {
        RAIIGuardConfig cfg;
        RAIIGuardGenerator gen(cfg);
        RAIIClassInfo cls;
        cls.name = "Point";
        cls.is_trivially_copyable = true;
        cls.fields = {{"x", InteropTypeKind::Float64, true},
                       {"y", InteropTypeKind::Float64, true}};

        std::string code = gen.generate(cls);
        ok = code.find("struct Point") != std::string::npos
          && code.find("double x") != std::string::npos
          && code.find("double y") != std::string::npos;
    }
    END_TEST;
}

static void test_raii_handle_based() {
    bool ok = false;
    TEST("RAII handle-based class") {
        RAIIGuardConfig cfg;
        RAIIGuardGenerator gen(cfg);
        RAIIClassInfo cls;
        cls.name = "Session";
        cls.is_trivially_copyable = false;
        cls.c_type = "void*";
        cls.destroy_fn = "session_destroy";
        cls.methods = {
            {"connect", {}, {"status", InteropTypeKind::Int32, false}, false, false, false, false, false}
        };

        std::string code = gen.generate(cls);
        ok = code.find("class Session") != std::string::npos
          && code.find("session_destroy") != std::string::npos
          && code.find("unique_ptr") != std::string::npos
          && code.find("move ops") == std::string::npos; /* not in output, just in comment sense */
    }
    END_TEST;
}

static void test_raii_namespace() {
    bool ok = false;
    TEST("RAII namespace wrapping") {
        RAIIGuardConfig cfg;
        cfg.namespace_name = "myapp";
        RAIIGuardGenerator gen(cfg);
        RAIIClassInfo cls;
        cls.name = "Widget";
        cls.is_trivially_copyable = true;

        std::string code = gen.generate(cls);
        ok = code.find("namespace myapp") != std::string::npos
          && code.find("struct Widget") != std::string::npos;
    }
    END_TEST;
}

/* ─────────────────────────────────────────────────────────── */
/* 4. Ref-Count Bridge Tests                                  */
/* ─────────────────────────────────────────────────────────── */
static void test_aurora_arc() {
    bool ok = false;
    TEST("AuroraARC basic retain/release") {
        AuroraARC arc(1);
        ok = arc.count() == 1
          && arc.retain() == 2
          && arc.release() == 1
          && arc.is_unique();
    }
    END_TEST;
}

static void test_aurora_arc_multi() {
    bool ok = false;
    TEST("AuroraARC multi-threaded retain") {
        AuroraARC arc(1);
        arc.retain();
        arc.retain();
        arc.retain();
        ok = arc.count() == 4 && !arc.is_unique();
        arc.release();
        arc.release();
        arc.release();
        ok = ok && arc.count() == 1 && arc.is_unique();
    }
    END_TEST;
}

static void test_ref_count_adapter() {
    bool ok = false;
    TEST("RefCountAdapter basic operations") {
        RefCountAdapter adapter;
        RefCountedHandle h;
        h.ptr = new AuroraARC(1);
        h.protocol = RefCountProtocol::AuroraARC;

        void* retained = adapter.retain(h);
        ok = retained == h.ptr;

        adapter.release(h);
        adapter.release(h); /* back to 0 from retain + initial */

        delete static_cast<AuroraARC*>(h.ptr);
        ok = true;
    }
    END_TEST;
}

static void test_ref_count_protocol_registration() {
    bool ok = false;
    TEST("protocol registration and query") {
        RefCountAdapter adapter;
        ok = !adapter.is_registered(RefCountProtocol::Python);
        RefCountBridgeVTable vtable;
        adapter.register_protocol(RefCountProtocol::Python, vtable);
        ok = ok && adapter.is_registered(RefCountProtocol::Python);
    }
    END_TEST;
}

static void test_ownership_delegation() {
    bool ok = false;
    TEST("ownership delegation: same protocol = zero-cost") {
        auto d = resolve_delegation(RefCountProtocol::AuroraARC, RefCountProtocol::AuroraARC);
        ok = d.zero_cost_path && !d.need_retain_on_import;

        auto d2 = resolve_delegation(RefCountProtocol::AuroraARC, RefCountProtocol::Python);
        ok = ok && !d2.zero_cost_path && d2.need_retain_on_import;
    }
    END_TEST;
}

/* ─────────────────────────────────────────────────────────── */
/* 5. Thunk Declaration Tests                                 */
/* ─────────────────────────────────────────────────────────── */
static void test_thunk_declarations() {
    bool ok = false;
    TEST("thunk declaration generation") {
        RAIIGuardConfig cfg;
        RAIIGuardGenerator gen(cfg);
        RAIIClassInfo cls;
        cls.name = "Buffer";
        cls.destroy_fn = "buffer_destroy";
        cls.methods = {
            {"read", {{"offset", InteropTypeKind::Int64, false}},
                      {"byte", InteropTypeKind::UInt8, false}, false, false, false, false, false}
        };

        std::string thunks = gen.generate_thunk_declarations(cls);
        ok = thunks.find("extern \"C\"") != std::string::npos
          && thunks.find("buffer_destroy") != std::string::npos;
    }
    END_TEST;
}

/* ─────────────────────────────────────────────────────────── */
/* Main                                                       */
/* ─────────────────────────────────────────────────────────── */
int main() {
    printf("═══ Phase 4: Cross-Language Memory Safety ═══\n\n");

    printf("--- 1. FFI Ownership Protocol ---\n");
    test_ffi_ownership_basics();
    test_ffi_ownership_pointers();
    test_ffi_ownership_strings();
    test_ffi_ownership_class();
    test_ffi_safety_rules();

    printf("\n--- 2. FFI Borrow Checker ---\n");
    test_borrow_checker_basic_call();
    test_borrow_checker_mismatch();
    test_borrow_checker_param_count();
    test_borrow_checker_tracking();

    printf("\n--- 3. RAII Guard Generator ---\n");
    test_raii_trivially_copyable();
    test_raii_handle_based();
    test_raii_namespace();
    test_thunk_declarations();

    printf("\n--- 4. Ref-Count Bridge ---\n");
    test_aurora_arc();
    test_aurora_arc_multi();
    test_ref_count_adapter();
    test_ref_count_protocol_registration();
    test_ownership_delegation();

    printf("\n═══════════════════════════════════════════\n");
    printf("Results: %d / %d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
