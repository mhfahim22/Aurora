#include <chrono>
#include <cstdint>
#include <iostream>

/* Mininal benchmark runner for cross-language interop hot paths */

static uint64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

/* InteropType prebuilt access (inline in header) */
#include "runtime/interop/type_ir.hpp"

static void bench_prebuilt() {
    constexpr int64_t ITERS = 10000000;
    uint64_t start = now_ns();
    volatile const void* sink = nullptr;
    for (int64_t i = 0; i < ITERS; i++) {
        sink = &InteropType::prebuilt(InteropTypeKind::Int64);
    }
    uint64_t elapsed = now_ns() - start;
    std::cout << "prebuilt  : " << ITERS << " iters in "
              << (elapsed / 1000000) << " ms, "
              << (ITERS * 1000 / (elapsed ? elapsed : 1)) << " M/s\n";
}

int main() {
    std::cout << "bench_fast — microbenchmarks\n";
    bench_prebuilt();
    return 0;
}
