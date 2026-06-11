#include "std/math.hpp"
#include <cmath>
#include <cstdlib>
#include <ctime>

extern "C" {

double aurora_math_sin(double x) { return sin(x); }
double aurora_math_cos(double x) { return cos(x); }
double aurora_math_tan(double x) { return tan(x); }
double aurora_math_sqrt(double x) { return sqrt(x); }
double aurora_math_abs(double x) { return fabs(x); }
double aurora_math_floor(double x) { return floor(x); }
double aurora_math_ceil(double x) { return ceil(x); }
double aurora_math_round(double x) { return round(x); }
double aurora_math_pow(double x, double y) { return pow(x, y); }
double aurora_math_log(double x) { return log(x); }
double aurora_math_log10(double x) { return log10(x); }
double aurora_math_exp(double x) { return exp(x); }
double aurora_math_random() {
    static bool seeded = false;
    if (!seeded) { srand((unsigned)time(nullptr)); seeded = true; }
    return (double)rand() / (double)RAND_MAX;
}
int64_t aurora_math_random_int(int64_t min, int64_t max) {
    if (min >= max) return min;
    return min + (int64_t)((double)rand() / ((double)RAND_MAX + 1) * (max - min + 1));
}
double aurora_math_pi() { return 3.14159265358979323846; }
double aurora_math_e() { return 2.71828182845904523536; }

}
