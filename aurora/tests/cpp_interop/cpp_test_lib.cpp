#include <cstdint>
#include <cstdio>
#include <cmath>
#include <new>
#include <vector>
#include <string>

#ifdef _WIN32
#define TESTLIB_API __declspec(dllexport)
#else
#define TESTLIB_API __attribute__((visibility("default")))
#endif

extern "C" {

/* ── Simple math ── */
TESTLIB_API double add(double a, double b) { return a + b; }
TESTLIB_API double sub(double a, double b) { return a - b; }
TESTLIB_API double mul(double a, double b) { return a * b; }
TESTLIB_API double divd(double a, double b) { return b != 0 ? a / b : 0; }
TESTLIB_API double power(double base, double exp) { return std::pow(base, exp); }

/* ── Vector operations (C++ std::vector wrapped) ── */
TESTLIB_API void* vec_new() {
    auto* v = new std::vector<double>();
    return static_cast<void*>(v);
}

TESTLIB_API void vec_delete(void* v) {
    if (v) delete static_cast<std::vector<double>*>(v);
}

TESTLIB_API void vec_push(void* v, double val) {
    if (v) static_cast<std::vector<double>*>(v)->push_back(val);
}

TESTLIB_API double vec_get(void* v, int idx) {
    if (!v) return 0;
    auto& vec = *static_cast<std::vector<double>*>(v);
    if (idx >= 0 && idx < (int)vec.size()) return vec[idx];
    return 0;
}

TESTLIB_API int vec_size(void* v) {
    if (!v) return 0;
    return (int)static_cast<std::vector<double>*>(v)->size();
}

TESTLIB_API double vec_sum(void* v) {
    if (!v) return 0;
    auto& vec = *static_cast<std::vector<double>*>(v);
    double s = 0;
    for (auto& x : vec) s += x;
    return s;
}

/* ── String operations ── */
TESTLIB_API void* string_from_cstr(const char* s) {
    auto* str = new std::string(s ? s : "");
    return static_cast<void*>(str);
}

TESTLIB_API void string_delete(void* s) {
    if (s) delete static_cast<std::string*>(s);
}

TESTLIB_API int string_length(void* s) {
    if (!s) return 0;
    return (int)static_cast<std::string*>(s)->length();
}

TESTLIB_API const char* string_cstr(void* s) {
    if (!s) return "";
    return static_cast<std::string*>(s)->c_str();
}

TESTLIB_API void string_print(void* s) {
    if (!s) return;
    std::printf("%s\n", static_cast<std::string*>(s)->c_str());
}

/* ── Counter class (C++ object model demo) ── */
class Counter {
    int64_t count_;
public:
    Counter() : count_(0) {}
    explicit Counter(int64_t start) : count_(start) {}
    int64_t inc() { return ++count_; }
    int64_t dec() { return --count_; }
    int64_t add(int64_t n) { count_ += n; return count_; }
    int64_t get() const { return count_; }
    void reset() { count_ = 0; }
};

TESTLIB_API void* counter_new() {
    auto* c = new Counter();
    return static_cast<void*>(c);
}

TESTLIB_API void* counter_new_start(int64_t start) {
    auto* c = new Counter(start);
    return static_cast<void*>(c);
}

TESTLIB_API void counter_delete(void* c) {
    if (c) delete static_cast<Counter*>(c);
}

TESTLIB_API int64_t counter_inc(void* c) {
    if (!c) return 0;
    return static_cast<Counter*>(c)->inc();
}

TESTLIB_API int64_t counter_dec(void* c) {
    if (!c) return 0;
    return static_cast<Counter*>(c)->dec();
}

TESTLIB_API int64_t counter_add(void* c, int64_t n) {
    if (!c) return 0;
    return static_cast<Counter*>(c)->add(n);
}

TESTLIB_API int64_t counter_get(void* c) {
    if (!c) return 0;
    return static_cast<Counter*>(c)->get();
}

TESTLIB_API void counter_reset(void* c) {
    if (c) static_cast<Counter*>(c)->reset();
}

/* ── Matmul benchmark (native C++ speed test) ── */
TESTLIB_API double bench_matmul(int n, int iterations) {
    std::vector<double> a(n * n), b(n * n), c(n * n);
    for (int i = 0; i < n * n; i++) {
        a[i] = (double)(i % 100) / 10.0;
        b[i] = (double)((i * 7) % 100) / 10.0;
    }
    for (int iter = 0; iter < iterations; iter++) {
        for (int i = 0; i < n; i++) {
            for (int k = 0; k < n; k++) {
                double aik = a[i * n + k];
                for (int j = 0; j < n; j++) {
                    c[i * n + j] += aik * b[k * n + j];
                }
            }
        }
    }
    double sum = 0;
    for (int i = 0; i < n * n; i++) sum += c[i];
    return sum;
}

/* ── Pure compute benchmark (CPU stress test) ── */
TESTLIB_API double bench_compute(int64_t iterations) {
    double x = 1.0;
    for (int64_t i = 0; i < iterations; i++) {
        x = x * 1.0000001 + 0.0000005;
        if (x > 1000.0) x = 1.0;
    }
    return x;
}

/* ── High-precision timer (cross-platform) ── */
#ifdef _WIN32
#include <windows.h>
struct Timer {
    LARGE_INTEGER start, freq;
    Timer() { QueryPerformanceFrequency(&freq); QueryPerformanceCounter(&start); }
    double elapsed_ms() {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return (double)(now.QuadPart - start.QuadPart) * 1000.0 / (double)freq.QuadPart;
    }
};
#else
#include <chrono>
struct Timer {
    std::chrono::high_resolution_clock::time_point start;
    Timer() : start(std::chrono::high_resolution_clock::now()) {}
    double elapsed_ms() {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(now - start).count();
    }
};
#endif

TESTLIB_API void* timer_new() {
    return static_cast<void*>(new Timer());
}

TESTLIB_API void timer_delete(void* t) {
    if (t) delete static_cast<Timer*>(t);
}

TESTLIB_API double timer_elapsed_ms(void* t) {
    if (!t) return 0;
    return static_cast<Timer*>(t)->elapsed_ms();
}

} /* extern "C" */
