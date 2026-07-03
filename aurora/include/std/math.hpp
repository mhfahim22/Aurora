#pragma once
#include <cstdint>
#include "common/platform.hpp"

#ifdef __cplusplus
extern "C" {
#endif

AURORA_EXPORT double aurora_math_sin(double x);
AURORA_EXPORT double aurora_math_cos(double x);
AURORA_EXPORT double aurora_math_tan(double x);
AURORA_EXPORT double aurora_math_sqrt(double x);
AURORA_EXPORT double aurora_math_abs(double x);
AURORA_EXPORT double aurora_math_floor(double x);
AURORA_EXPORT double aurora_math_ceil(double x);
AURORA_EXPORT double aurora_math_round(double x);
AURORA_EXPORT double aurora_math_pow(double x, double y);
AURORA_EXPORT double aurora_math_log(double x);
AURORA_EXPORT double aurora_math_log10(double x);
AURORA_EXPORT double aurora_math_exp(double x);
AURORA_EXPORT double aurora_math_random();
AURORA_EXPORT int64_t aurora_math_random_int(int64_t min, int64_t max);
AURORA_EXPORT double aurora_math_pi();
AURORA_EXPORT double aurora_math_e();
AURORA_EXPORT double aurora_math_asin(double x);
AURORA_EXPORT double aurora_math_acos(double x);
AURORA_EXPORT double aurora_math_atan(double x);
AURORA_EXPORT double aurora_math_atan2(double y, double x);
AURORA_EXPORT double aurora_math_sinh(double x);
AURORA_EXPORT double aurora_math_cosh(double x);
AURORA_EXPORT double aurora_math_tanh(double x);
AURORA_EXPORT double aurora_math_log2(double x);
AURORA_EXPORT double aurora_math_cbrt(double x);
AURORA_EXPORT double aurora_math_hypot(double x, double y);
AURORA_EXPORT double aurora_math_erf(double x);
AURORA_EXPORT double aurora_math_tgamma(double x);
AURORA_EXPORT double aurora_math_fmod(double x, double y);
AURORA_EXPORT double aurora_math_remainder(double x, double y);
AURORA_EXPORT double aurora_math_copysign(double x, double y);
AURORA_EXPORT double aurora_math_nextafter(double x, double y);

#ifdef __cplusplus
}
#endif
