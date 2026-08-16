#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AuroraDecimal AuroraDecimal;

AuroraDecimal* aurora_decimal_from_str(const char* s);
AuroraDecimal* aurora_decimal_from_int(int64_t i);
AuroraDecimal* aurora_decimal_from_float(double f);
char*          aurora_decimal_to_str(AuroraDecimal* d);
void           aurora_decimal_free(AuroraDecimal* d);

AuroraDecimal* aurora_decimal_add(AuroraDecimal* a, AuroraDecimal* b);
AuroraDecimal* aurora_decimal_sub(AuroraDecimal* a, AuroraDecimal* b);
AuroraDecimal* aurora_decimal_mul(AuroraDecimal* a, AuroraDecimal* b);
AuroraDecimal* aurora_decimal_div(AuroraDecimal* a, AuroraDecimal* b);
int            aurora_decimal_compare(AuroraDecimal* a, AuroraDecimal* b);

AuroraDecimal* aurora_decimal_abs(AuroraDecimal* d);
AuroraDecimal* aurora_decimal_negate(AuroraDecimal* d);
AuroraDecimal* aurora_decimal_round(AuroraDecimal* d, int places);

int            aurora_decimal_is_zero(AuroraDecimal* d);
int            aurora_decimal_is_negative(AuroraDecimal* d);

#ifdef __cplusplus
}
#endif
