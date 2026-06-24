// test_memory_safety.cpp — Comprehensive Memory Safety Tests
// Phase 10: Production Release

#include "common/test_suite.hpp"
#include "common/aurora_version.hpp"
#include "compiler/ast.hpp"
#include "compiler/memory_analyzer.hpp"
#include "compiler/escape_analyzer.hpp"
#include "compiler/lifetime_analyzer.hpp"
#include "compiler/ownership_analyzer.hpp"
#include "compiler/allocation_strategy.hpp"
#include "compiler/allocation_profiler.hpp"
#include "runtime/memory.hpp"

#include <iostream>
#include <thread>
#include <vector>
#include <atomic>

/* ════════════════════════════════════════════════════════════
   Test Categories
   ════════════════════════════════════════════════════════════ */

/* ── Version Tests ── */
bool test_version_info() {
    TEST_ASSERT(aurora_version() != nullptr, "Version string should not be null");
    TEST_ASSERT(std::string(aurora_version()) == "1.0.0", "Version should be 1.0.0");
    return true;
}

bool test_version_full() {
    std::string full = aurora_version_full();
    TEST_ASSERT(full.find("1.0.0") != std::string::npos, "Full version should contain 1.0.0");
    return true;
}

/* ── AST Tests ── */
bool test_ast_node_creation() {
    auto node = make_node(NodeType::Assign, "x", 1);
    TEST_ASSERT(node != nullptr, "Node should not be null");
    TEST_ASSERT(node->type == NodeType::Assign, "Node type should be Assign");
    TEST_ASSERT(node->value == "x", "Node value should be x");
    TEST_ASSERT(node->src_line == 1, "Node line should be 1");
    return true;
}

bool test_alloc_strategy_names() {
    TEST_ASSERT(std::string(alloc_strategy_name(AllocStrategy::Stack)) == "Stack", "Stack name");
    TEST_ASSERT(std::string(alloc_strategy_name(AllocStrategy::Arena)) == "Arena", "Arena name");
    TEST_ASSERT(std::string(alloc_strategy_name(AllocStrategy::RAII)) == "RAII", "RAII name");
    TEST_ASSERT(std::string(alloc_strategy_name(AllocStrategy::ARC)) == "ARC", "ARC name");
    TEST_ASSERT(std::string(alloc_strategy_name(AllocStrategy::GC)) == "GC", "GC name");
    return true;
}

bool test_escape_status_names() {
    TEST_ASSERT(std::string(escape_status_name(EscapeStatus::NoEscape)) == "NoEscape", "NoEscape name");
    TEST_ASSERT(std::string(escape_status_name(EscapeStatus::ArgEscape)) == "ArgEscape", "ArgEscape name");
    TEST_ASSERT(std::string(escape_status_name(EscapeStatus::ReturnEscape)) == "ReturnEscape", "ReturnEscape name");
    return true;
}

bool test_lifetime_scope_names() {
    TEST_ASSERT(std::string(lifetime_scope_name(LifetimeScope::Function)) == "Function", "Function name");
    TEST_ASSERT(std::string(lifetime_scope_name(LifetimeScope::Block)) == "Block", "Block name");
    TEST_ASSERT(std::string(lifetime_scope_name(LifetimeScope::Loop)) == "Loop", "Loop name");
    TEST_ASSERT(std::string(lifetime_scope_name(LifetimeScope::Temporary)) == "Temporary", "Temporary name");
    return true;
}

bool test_ownership_type_names() {
    TEST_ASSERT(std::string(ownership_type_name(OwnershipType::Single)) == "Single", "Single name");
    TEST_ASSERT(std::string(ownership_type_name(OwnershipType::Shared)) == "Shared", "Shared name");
    TEST_ASSERT(std::string(ownership_type_name(OwnershipType::Weak)) == "Weak", "Weak name");
    TEST_ASSERT(std::string(ownership_type_name(OwnershipType::Borrowed)) == "Borrowed", "Borrowed name");
    return true;
}

/* ── Escape Analyzer Tests ── */
bool test_escape_analyzer_creation() {
    EscapeAnalyzer analyzer;
    /* Create a simple AST */
    auto root = make_node(NodeType::Function, "test_func", 1);
    root->args = make_node(NodeType::Var, "x", 1);
    root->body = make_node(NodeType::Assign, "", 2);
    root->body->left = make_node(NodeType::Var, "y", 2);
    root->body->right = make_node(NodeType::Var, "x", 2);

    analyzer.analyse(root.get());
    /* Test should not crash */
    return true;
}

/* ── Lifetime Analyzer Tests ── */
bool test_lifetime_analyzer_creation() {
    LifetimeAnalyzer analyzer;
    auto root = make_node(NodeType::Function, "test_func", 1);
    root->args = make_node(NodeType::Var, "x", 1);
    root->body = make_node(NodeType::Assign, "", 2);
    root->body->left = make_node(NodeType::Var, "y", 2);
    root->body->right = make_node(NodeType::Num, "42", 2);

    analyzer.analyse(root.get());
    /* Test should not crash */
    return true;
}

/* ── Ownership Analyzer Tests ── */
bool test_ownership_analyzer_creation() {
    OwnershipAnalyzer analyzer;
    auto root = make_node(NodeType::Function, "test_func", 1);
    root->args = make_node(NodeType::Var, "x", 1);
    root->body = make_node(NodeType::Assign, "", 2);
    root->body->left = make_node(NodeType::Var, "y", 2);
    root->body->right = make_node(NodeType::Var, "x", 2);

    analyzer.analyse(root.get());
    /* Test should not crash */
    return true;
}

/* ── Memory Analyzer Tests ── */
bool test_memory_analyzer_creation() {
    MemoryAnalyzer analyzer;
    auto root = make_node(NodeType::Function, "test_func", 1);
    root->args = make_node(NodeType::Var, "x", 1);
    root->body = make_node(NodeType::Assign, "", 2);
    root->body->left = make_node(NodeType::Var, "y", 2);
    root->body->right = make_node(NodeType::Num, "42", 2);

    analyzer.analyse(root.get());
    /* Test should not crash */
    return true;
}

/* ── Runtime Tests ── */
bool test_arena_allocator() {
    aurora_memory_reset_stats();

    void* ptr1 = aurora_arena_alloc(64);
    TEST_ASSERT(ptr1 != nullptr, "Arena allocation should succeed");

    void* ptr2 = aurora_arena_alloc(128);
    TEST_ASSERT(ptr2 != nullptr, "Second arena allocation should succeed");

    TEST_ASSERT(ptr1 != ptr2, "Different allocations should have different addresses");

    aurora_arena_free();
    return true;
}

bool test_shared_box() {
    void* box = aurora_shared_new(nullptr, nullptr);
    TEST_ASSERT(box != nullptr, "Shared box creation should succeed");

    int64_t count = aurora_refcount_get(box);
    TEST_ASSERT(count == 1, "Initial refcount should be 1");

    aurora_refcount_inc(box);
    count = aurora_refcount_get(box);
    TEST_ASSERT(count == 2, "Refcount should be 2 after inc");

    aurora_refcount_dec(box);
    count = aurora_refcount_get(box);
    TEST_ASSERT(count == 1, "Refcount should be 1 after dec");

    return true;
}

bool test_weak_reference() {
    void* box = aurora_shared_new(nullptr, nullptr);
    TEST_ASSERT(box != nullptr, "Shared box creation should succeed");

    void* weak = aurora_weak_new(box);
    TEST_ASSERT(weak != nullptr, "Weak reference creation should succeed");

    void* locked = aurora_weak_lock(weak);
    TEST_ASSERT(locked != nullptr, "Weak lock should succeed");

    aurora_refcount_dec(box);
    aurora_weak_release(weak);

    return true;
}

bool test_drop_glue() {
    void* ptr = aurora_arena_alloc(64);
    TEST_ASSERT(ptr != nullptr, "Allocation should succeed");

    /* Drop should not crash on valid pointer */
    aurora_drop_glue(ptr);

    /* Drop should not crash on null pointer */
    aurora_drop_glue(nullptr);

    return true;
}

/* ── Thread Safety Tests ── */
bool test_thread_safe_refcount() {
    void* box = aurora_shared_new(nullptr, nullptr);
    TEST_ASSERT(box != nullptr, "Shared box creation should succeed");

    std::atomic<int64_t> counter{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < 4; i++) {
        threads.emplace_back([&box, &counter]() {
            for (int j = 0; j < 1000; j++) {
                aurora_refcount_inc_atomic(box);
                counter++;
                aurora_refcount_dec_atomic(box);
                counter--;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    int64_t final_count = aurora_refcount_get(box);
    TEST_ASSERT(final_count == 1, "Final refcount should be 1");

    return true;
}

/* ── GC Tests ── */
bool test_gc_alloc_collect() {
    aurora_gc_init();

    size_t live_before, collected_before;
    aurora_gc_get_stats(&live_before, &collected_before);

    void* p1 = aurora_gc_alloc(64);
    void* p2 = aurora_gc_alloc(128);
    TEST_ASSERT(p1 != nullptr, "GC alloc 1 should succeed");
    TEST_ASSERT(p2 != nullptr, "GC alloc 2 should succeed");
    TEST_ASSERT(p1 != p2, "GC allocs should return different addresses");

    size_t live_after;
    aurora_gc_get_stats(&live_after, &collected_before);
    TEST_ASSERT(live_after == live_before + 2, "Live objects should increase by 2 after allocs");

    /* Root-sweep: register then unregister root — object should be freed on collect */
    aurora_gc_register_root(p1);
    aurora_gc_unregister_root(p1);

    /* p2 was never rooted — should be freed */
    size_t live_mid, collected_mid;
    aurora_gc_collect();
    aurora_gc_get_stats(&live_mid, &collected_mid);
    TEST_ASSERT(collected_mid > collected_before, "GC collect should free at least p2");
    TEST_ASSERT(live_mid < live_after, "Live objects should decrease after collect");

    return true;
}

bool test_gc_register_root() {
    aurora_gc_init();

    void* p = aurora_gc_alloc(32);
    aurora_gc_register_root(p);
    aurora_gc_register_root(p);
    aurora_gc_collect();

    size_t live, collected_before, collected_after;
    aurora_gc_get_stats(&live, &collected_before);
    TEST_ASSERT(live >= 1, "Rooted object should survive collection");
    TEST_ASSERT(p != nullptr, "Pointer should still be valid");

    aurora_gc_unregister_root(p);
    aurora_gc_unregister_root(p);
    aurora_gc_collect();

    aurora_gc_get_stats(&live, &collected_after);
    TEST_ASSERT(live == 0, "All GC objects freed after root removal");
    TEST_ASSERT(collected_after > collected_before, "GC collected after root removal");
    return true;
}

bool test_gc_auto_collect_threshold() {
    aurora_gc_init();

    size_t live, collected;
    aurora_gc_get_stats(&live, &collected);
    size_t collected_before = collected;

    aurora_gc_set_auto_collect_threshold(25);

    /* Allocate 50 objects at 8 bytes each — should trigger auto-collect at least once */
    for (int i = 0; i < 50; i++) aurora_gc_alloc(8);

    aurora_gc_get_stats(&live, &collected);
    TEST_ASSERT(collected > collected_before, "Auto-collect should have run at least once");

    aurora_gc_set_auto_collect_threshold(5000);
    return true;
}

bool test_gc_trigger_check() {
    aurora_gc_init();

    aurora_gc_set_auto_collect_threshold(10);
    for (int i = 0; i < 12; i++) aurora_gc_alloc(8);

    /* Should not crash */
    aurora_gc_trigger_check();

    aurora_gc_set_auto_collect_threshold(5000);
    return true;
}

/* ── Memory Diagnostics Tests ── */
bool test_memory_diagnostics() {
    aurora_memory_reset_stats();
    aurora_memory_set_diagnostics(1);

    for (int i = 0; i < 10; i++) {
        aurora_arena_alloc(64);
    }

    AuroraMemoryStats stats;
    aurora_memory_get_stats(&stats);

    TEST_ASSERT(stats.arena_allocated > 0, "Arena allocated should be > 0");
    TEST_ASSERT(stats.arena_blocks > 0, "Arena blocks should be > 0");

    aurora_memory_set_diagnostics(0);
    return true;
}

/* ════════════════════════════════════════════════════════════
   Main Test Runner
   ════════════════════════════════════════════════════════════ */

int main() {
    TestSuite suite;

    /* Version Tests */
    suite.add_test("Version Info", test_version_info);
    suite.add_test("Version Full", test_version_full);

    /* AST Tests */
    suite.add_test("AST Node Creation", test_ast_node_creation);
    suite.add_test("Alloc Strategy Names", test_alloc_strategy_names);
    suite.add_test("Escape Status Names", test_escape_status_names);
    suite.add_test("Lifetime Scope Names", test_lifetime_scope_names);
    suite.add_test("Ownership Type Names", test_ownership_type_names);

    /* Analyzer Tests */
    suite.add_test("Escape Analyzer Creation", test_escape_analyzer_creation);
    suite.add_test("Lifetime Analyzer Creation", test_lifetime_analyzer_creation);
    suite.add_test("Ownership Analyzer Creation", test_ownership_analyzer_creation);
    suite.add_test("Memory Analyzer Creation", test_memory_analyzer_creation);

    /* Runtime Tests */
    suite.add_test("Arena Allocator", test_arena_allocator);
    suite.add_test("Shared Box", test_shared_box);
    suite.add_test("Weak Reference", test_weak_reference);
    suite.add_test("Drop Glue", test_drop_glue);

    /* Thread Safety Tests */
    suite.add_test("Thread Safe Refcount", test_thread_safe_refcount);

    /* GC Tests */
    suite.add_test("GC Alloc/Collect", test_gc_alloc_collect);
    suite.add_test("GC Register Root", test_gc_register_root);
    suite.add_test("GC Auto Collect Threshold", test_gc_auto_collect_threshold);
    suite.add_test("GC Trigger Check", test_gc_trigger_check);

    /* Memory Diagnostics Tests */
    suite.add_test("Memory Diagnostics", test_memory_diagnostics);

    /* Run all tests */
    suite.run_all();

    /* Print summary */
    std::cout << "Aurora Universal Memory Safety v" << aurora_version() << "\n";
    std::cout << "Test Results: " << suite.get_passed() << "/" << suite.get_total() << " passed\n";

    return (suite.get_failed() > 0) ? 1 : 0;
}
