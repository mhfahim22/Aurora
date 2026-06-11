#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

double aurora_math_sin(double x);
double aurora_math_cos(double x);
double aurora_math_tan(double x);
double aurora_math_sqrt(double x);
double aurora_math_abs(double x);
double aurora_math_floor(double x);
double aurora_math_ceil(double x);
double aurora_math_round(double x);
double aurora_math_pow(double x, double y);
double aurora_math_log(double x);
double aurora_math_log10(double x);
double aurora_math_exp(double x);
double aurora_math_random();
int64_t aurora_math_random_int(int64_t min, int64_t max);
double aurora_math_pi();
double aurora_math_e();

#ifdef __cplusplus
}
#endif
