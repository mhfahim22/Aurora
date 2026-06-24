#pragma once
#include <string>
#include <vector>
#include <functional>
#include <iostream>
#include <sstream>
#include <cassert>
#include <chrono>
#include <iomanip>

/* ════════════════════════════════════════════════════════════
   Aurora Universal Memory Safety — Test Suite (Phase 10)
   ════════════════════════════════════════════════════════════

   Comprehensive test suite for memory safety system.

   ════════════════════════════════════════════════════════════ */

/* ── Test result ── */
struct TestResult {
    std::string name;
    bool        passed;
    std::string message;
    double      time_ms;
};

/* ── Test suite ── */
class TestSuite {
public:
    TestSuite() = default;

    /* ── Add tests ── */
    void add_test(const std::string& name, std::function<bool()> test) {
        tests_.push_back({name, test});
    }

    /* ── Run all tests ── */
    void run_all() {
        results_.clear();
        total_tests_ = 0;
        passed_tests_ = 0;
        failed_tests_ = 0;

        std::cout << "\n";
        std::cout << "╔══════════════════════════════════════════════════════════╗\n";
        std::cout << "║      Aurora Memory Safety Test Suite                    ║\n";
        std::cout << "╠══════════════════════════════════════════════════════════╣\n";

        for (const auto& test : tests_) {
            total_tests_++;
            auto start = std::chrono::high_resolution_clock::now();

            bool passed = false;
            std::string message;
            try {
                passed = test.func();
                message = passed ? "PASSED" : "FAILED";
            } catch (const std::exception& e) {
                message = "EXCEPTION: " + std::string(e.what());
            } catch (...) {
                message = "UNKNOWN EXCEPTION";
            }

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            double time_ms = duration.count() / 1000.0;

            if (passed) passed_tests_++;
            else failed_tests_++;

            TestResult result{test.name, passed, message, time_ms};
            results_.push_back(result);

            std::cout << "║  " << (passed ? "✓" : "✗") << " " << test.name;
            for (size_t i = test.name.length(); i < 45; i++) std::cout << " ";
            std::cout << std::fixed << std::setprecision(2) << time_ms << " ms  ║\n";
        }

        std::cout << "╠══════════════════════════════════════════════════════════╣\n";
        std::cout << "║  Results: " << passed_tests_ << "/" << total_tests_ << " passed";
        for (size_t i = 0; i < 40; i++) std::cout << " ";
        std::cout << "║\n";

        if (failed_tests_ > 0) {
            std::cout << "║  FAILED TESTS:                                          ║\n";
            for (const auto& r : results_) {
                if (!r.passed) {
                    std::cout << "║    • " << r.name << "\n";
                }
            }
        }

        std::cout << "╚══════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";
    }

    /* ── Get results ── */
    const std::vector<TestResult>& get_results() const { return results_; }
    int get_total() const { return total_tests_; }
    int get_passed() const { return passed_tests_; }
    int get_failed() const { return failed_tests_; }

private:
    struct Test {
        std::string name;
        std::function<bool()> func;
    };

    std::vector<Test> tests_;
    std::vector<TestResult> results_;
    int total_tests_ { 0 };
    int passed_tests_ { 0 };
    int failed_tests_ { 0 };
};

/* ── Test macros ── */
#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  ASSERT FAILED: " << msg << "\n"; \
            return false; \
        } \
    } while(0)

#define TEST_EQ(a, b, msg) \
    do { \
        if ((a) != (b)) { \
            std::cerr << "  ASSERT FAILED: " << msg << "\n"; \
            return false; \
        } \
    } while(0)

#define TEST_NEQ(a, b, msg) \
    do { \
        if ((a) == (b)) { \
            std::cerr << "  ASSERT FAILED: " << msg << "\n"; \
            return false; \
        } \
    } while(0)
