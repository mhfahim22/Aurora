// test_smart_ptr.cpp — Tests for AuroraSharedPtr, AuroraWeakPtr, CppSharedPtrBridge
#include "runtime/smart_ptr.hpp"
#include <cstdio>
#include <cassert>
#include <string>
#include <thread>
#include <vector>

static int g_tests = 0, g_passed = 0, g_failed = 0;

#define TEST(name) do { \
    printf("  %s ... ", name); \
    g_tests++; \
} while(0)

#define PASS() do { g_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { g_failed++; printf("FAIL: %s\n", msg); } while(0)

/* ── Test helpers ── */
static int g_destructor_calls = 0;
struct Tracked {
    int id;
    Tracked(int i = 0) : id(i) {}
    ~Tracked() { g_destructor_calls++; }
};

static void reset_tracking() { g_destructor_calls = 0; }

/* ═══════════════════════════════════════════════════════════════
   AuroraSharedPtr Tests
   ═══════════════════════════════════════════════════════════════ */

static void test_shared_null() {
    printf("\n=== SharedPtr: Null ===\n");
    TEST("default constructor is null");
    AuroraSharedPtr<Tracked> p;
    assert(!p);
    assert(p.get() == nullptr);
    assert(p.use_count() == 0);
    PASS();

    TEST("nullptr comparison");
    assert(p == nullptr);
    assert(nullptr == p);
    PASS();
}

static void test_shared_basic() {
    printf("\n=== SharedPtr: Basic ===\n");
    reset_tracking();
    TEST("construct from raw pointer");
    {
        AuroraSharedPtr<Tracked> p(new Tracked(42));
        assert(p);
        assert(p.get() != nullptr);
        assert(p->id == 42);
        assert((*p).id == 42);
        assert(p.use_count() == 1);
        assert(p.unique());
        PASS();
    }
    assert(g_destructor_calls == 1);
}

static void test_shared_copy() {
    printf("\n=== SharedPtr: Copy ===\n");
    reset_tracking();
    TEST("copy increases refcount");
    AuroraSharedPtr<Tracked> p1(new Tracked(1));
    assert(p1.use_count() == 1);

    AuroraSharedPtr<Tracked> p2(p1);
    assert(p2.use_count() == 2);
    assert(p1.use_count() == 2);
    assert(p1.get() == p2.get());
    assert(!p1.unique());
    PASS();

    TEST("copy assignment");
    AuroraSharedPtr<Tracked> p3;
    p3 = p1;
    assert(p3.use_count() == 3);
    PASS();

    TEST("destructor called once when last ref drops");
    { AuroraSharedPtr<Tracked> p4(p1); assert(p4.use_count() == 4); }
    assert(p1.use_count() == 3);
    PASS();

    TEST("all destructors called");
    p1.reset();
    assert(p1.use_count() == 0);
    assert(g_destructor_calls == 0); /* p2 and p3 still alive */
    p2.reset();
    assert(g_destructor_calls == 0); /* p3 still alive */
    p3.reset();
    assert(g_destructor_calls == 1);
    PASS();
}

static void test_shared_move() {
    printf("\n=== SharedPtr: Move ===\n");
    reset_tracking();
    TEST("move transfers ownership");
    AuroraSharedPtr<Tracked> p1(new Tracked(2));
    assert(p1.use_count() == 1);
    AuroraSharedPtr<Tracked> p2(std::move(p1));
    assert(!p1);
    assert(p2);
    assert(p2.use_count() == 1);
    PASS();

    TEST("move assignment");
    AuroraSharedPtr<Tracked> p3;
    p3 = std::move(p2);
    assert(!p2);
    assert(p3);
    PASS();
    /* p1,p2,p3 all out of scope — destructor called once */
}

static void test_shared_reset() {
    printf("\n=== SharedPtr: Reset ===\n");
    reset_tracking();
    TEST("reset to null drops ref");
    AuroraSharedPtr<Tracked> p(new Tracked(3));
    assert(p);
    p.reset();
    assert(!p);
    assert(g_destructor_calls == 1);
    PASS();

    TEST("reset to new pointer replaces");
    AuroraSharedPtr<Tracked> p2(new Tracked(4));
    p2.reset(new Tracked(5));
    assert(p2->id == 5);
    assert(g_destructor_calls == 2); /* old Tracked(4) destroyed */
    PASS();
}

static void test_shared_swap() {
    printf("\n=== SharedPtr: Swap ===\n");
    reset_tracking();
    AuroraSharedPtr<Tracked> a(new Tracked(10));
    AuroraSharedPtr<Tracked> b(new Tracked(20));
    TEST("swap exchanges pointers");
    assert(a->id == 10);
    assert(b->id == 20);
    swap(a, b);
    assert(a->id == 20);
    assert(b->id == 10);
    PASS();
}

static void test_shared_make_shared() {
    printf("\n=== SharedPtr: make_shared ===\n");
    reset_tracking();
    TEST("aurora_make_shared with args");
    auto p = aurora_make_shared<Tracked>(99);
    assert(p);
    assert(p->id == 99);
    assert(p.use_count() == 1);
    PASS();
}

static void test_shared_thread_safety() {
    printf("\n=== SharedPtr: Thread Safety ===\n");
    reset_tracking();
    TEST("concurrent copy/destroy does not crash");

    auto* raw = new Tracked(100);
    AuroraSharedPtr<Tracked> shared(raw);

    std::vector<std::thread> threads;
    for (int i = 0; i < 8; i++) {
        threads.emplace_back([&shared]() {
            for (int j = 0; j < 1000; j++) {
                auto copy = shared;
                (void)copy.get();
            }
        });
    }
    for (auto& t : threads) t.join();
    assert(shared.use_count() == 1);
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
   AuroraWeakPtr Tests
   ═══════════════════════════════════════════════════════════════ */

static void test_weak_null() {
    printf("\n=== WeakPtr: Null ===\n");
    TEST("default constructor is null");
    AuroraWeakPtr<Tracked> w;
    assert(w.expired());
    auto sp = w.lock();
    assert(!sp);
    PASS();
}

static void test_weak_basic() {
    printf("\n=== WeakPtr: Basic ===\n");
    reset_tracking();
    TEST("lock from shared");
    AuroraSharedPtr<Tracked> sp(new Tracked(7));
    AuroraWeakPtr<Tracked> wp(sp);
    assert(!wp.expired());
    assert(wp.use_count() == 1);
    PASS();

    TEST("lock returns valid shared");
    auto locked = wp.lock();
    assert(locked);
    assert(locked->id == 7);
    assert(locked.use_count() == 2);
    PASS();

    TEST("weak expires after shared reset");
    sp.reset();
    assert(wp.expired());
    auto locked2 = wp.lock();
    assert(!locked2);
    PASS();
}

static void test_weak_copy() {
    printf("\n=== WeakPtr: Copy ===\n");
    reset_tracking();
    AuroraSharedPtr<Tracked> sp(new Tracked(8));
    AuroraWeakPtr<Tracked> wp1(sp);
    TEST("copy weak");
    AuroraWeakPtr<Tracked> wp2(wp1);
    assert(!wp2.expired());
    auto locked = wp2.lock();
    assert(locked);
    assert(locked->id == 8);
    PASS();
}

static void test_weak_assign_from_shared() {
    printf("\n=== WeakPtr: Assign from Shared ===\n");
    reset_tracking();
    AuroraWeakPtr<Tracked> wp;
    TEST("weak initialized from shared assignment");
    {
        AuroraSharedPtr<Tracked> sp(new Tracked(9));
        wp = AuroraWeakPtr<Tracked>(sp);
        assert(!wp.expired());
    }
    assert(wp.expired());
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
   CppSharedPtrBridge Tests
   ═══════════════════════════════════════════════════════════════ */

static void test_cpp_bridge_control_block() {
    printf("\n=== CppSharedPtr: Control Block ===\n");
    TEST("create and release");
    int dtor_count = 0;
    auto* obj = new int(42);
    auto* cb = cpp_shared_ptr_create(obj, [&dtor_count](void* p) {
        delete static_cast<int*>(p);
        dtor_count++;
    });
    assert(cb != nullptr);
    assert(cb->shared_count.load() == 1);
    assert(cpp_shared_ptr_is_valid(cb));

    cpp_shared_ptr_release(cb);
    assert(dtor_count == 1);
    PASS();
}

static void test_cpp_bridge_retain_release() {
    printf("\n=== CppSharedPtr: Retain/Release ===\n");
    TEST("retain/release cycle");
    int dtor = 0;
    auto* obj = new int(99);
    auto* cb = cpp_shared_ptr_create(obj, [&dtor](void* p) {
        delete static_cast<int*>(p);
        dtor++;
    });
    cpp_shared_ptr_retain(cb); /* 2 */
    assert(cb->shared_count.load() == 2);

    cpp_shared_ptr_release(cb); /* 1 */
    assert(dtor == 0);

    cpp_shared_ptr_release(cb); /* 0 → delete */
    assert(dtor == 1);
    PASS();
}

static void test_cpp_bridge_vtable() {
    printf("\n=== CppSharedPtr: VTable ===\n");
    TEST("vtable functions work");

    auto vt = cpp_shared_ptr_vtable();
    assert(vt.retain   == cpp_shared_ptr_retain);
    assert(vt.release  == cpp_shared_ptr_release);
    assert(vt.is_valid == cpp_shared_ptr_is_valid);
    assert(vt.copy     == cpp_shared_ptr_copy);

    int dtor = 0;
    auto* obj = new double(3.14);
    auto* cb = cpp_shared_ptr_create(obj, [&dtor](void* p) {
        delete static_cast<double*>(p);
        dtor++;
    });

    assert(vt.is_valid(cb));
    void* retained = vt.retain(cb);
    assert(retained == obj);
    vt.release(cb);
    assert(dtor == 0);
    vt.release(cb);
    assert(dtor == 1);
    PASS();
}

static void test_cpp_bridge_register() {
    printf("\n=== CppSharedPtr: Bridge Registration ===\n");
    TEST("register CppSharedPtr vtable with adapter");

    RefCountAdapter adapter;
    adapter.register_protocol(RefCountProtocol::CppSharedPtr, cpp_shared_ptr_vtable());
    assert(adapter.is_registered(RefCountProtocol::CppSharedPtr));

    int dtor = 0;
    auto* obj = new char('X');
    auto* cb = cpp_shared_ptr_create(obj, [&dtor](void* p) {
        delete static_cast<char*>(p);
        dtor++;
    });

    RefCountedHandle h;
    h.ptr = cb;
    h.protocol = RefCountProtocol::CppSharedPtr;

    /* Retain via adapter */
    void* r = adapter.retain(h);
    assert(r == cb);
    assert(cb->shared_count.load() == 2);

    /* Release via adapter (first release) */
    adapter.release(h);
    assert(dtor == 0);
    assert(cb->shared_count.load() == 1);

    /* Release via adapter (second release → delete) */
    adapter.release(h);
    assert(dtor == 1);
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
   to_std_shared / from_std_shared Tests
   ═══════════════════════════════════════════════════════════════ */

static void test_std_interop() {
    printf("\n=== std::shared_ptr Interop ===\n");
    reset_tracking();

    TEST("AuroraSharedPtr → std::shared_ptr via to_std_shared");
    {
        AuroraSharedPtr<Tracked> ap(new Tracked(200));
        assert(ap.use_count() == 1);

        auto sp = to_std_shared(ap);
        assert(sp.get() == ap.get());
        assert(ap.use_count() == 1); /* std::shared_ptr is a non-owning alias */
        PASS();
    }
    assert(g_destructor_calls == 1);
}

/* ═══════════════════════════════════════════════════════════════
   RefCountAdapter integration tests
   ═══════════════════════════════════════════════════════════════ */

static void test_delegation() {
    printf("\n=== Ownership Delegation ===\n");
    TEST("same protocol → zero cost");
    auto d = resolve_delegation(
        RefCountProtocol::AuroraARC,
        RefCountProtocol::AuroraARC);
    assert(d.zero_cost_path);
    assert(!d.need_retain_on_import);
    assert(!d.need_release_on_export);
    PASS();

    TEST("cross protocol → adapter needed");
    auto d2 = resolve_delegation(
        RefCountProtocol::AuroraARC,
        RefCountProtocol::CppSharedPtr);
    assert(!d2.zero_cost_path);
    assert(d2.need_retain_on_import);
    assert(d2.need_release_on_export);
    PASS();
}

int main() {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    printf("========================================\n");
    printf("  Smart Pointer Test Suite\n");
    printf("========================================\n");

    /* AuroraSharedPtr */
    test_shared_null();
    test_shared_basic();
    test_shared_copy();
    test_shared_move();
    test_shared_reset();
    test_shared_swap();
    test_shared_make_shared();
    test_shared_thread_safety();

    /* AuroraWeakPtr */
    test_weak_null();
    test_weak_basic();
    test_weak_copy();
    test_weak_assign_from_shared();

    /* CppSharedPtrBridge */
    test_cpp_bridge_control_block();
    test_cpp_bridge_retain_release();
    test_cpp_bridge_vtable();
    test_cpp_bridge_register();

    /* std::shared_ptr interop */
    test_std_interop();

    /* Ownership delegation */
    test_delegation();

    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed, %d total\n",
           g_passed, g_failed, g_tests);
    printf("========================================\n");
    return g_failed > 0 ? 1 : 0;
}
