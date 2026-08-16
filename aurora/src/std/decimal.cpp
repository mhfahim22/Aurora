#include "std/decimal.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <algorithm>

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

struct AuroraDecimal {
    int64_t value;    /* scaled integer value */
    int32_t scale;    /* number of decimal places (0-18) */
};

static int64_t pow10(int exp) {
    int64_t r = 1;
    for (int i = 0; i < exp; i++) r *= 10;
    return r;
}

static AuroraDecimal* decimal_alloc(int64_t value, int32_t scale) {
    auto* d = (AuroraDecimal*)malloc(sizeof(AuroraDecimal));
    d->value = value;
    d->scale = scale;
    return d;
}

/* align two decimals to the same scale */
static void align(AuroraDecimal* a, AuroraDecimal* b, int64_t& va, int64_t& vb, int32_t& scale) {
    scale = (a->scale > b->scale) ? a->scale : b->scale;
    va = a->value;
    vb = b->value;
    if (a->scale < scale) va *= pow10(scale - a->scale);
    if (b->scale < scale) vb *= pow10(scale - b->scale);
}

extern "C" {

AuroraDecimal* aurora_decimal_from_str(const char* s) {
    if (!s) return nullptr;
    std::string str(s);
    bool neg = false;
    size_t pos = 0;
    if (str[pos] == '-') { neg = true; pos++; }
    else if (str[pos] == '+') pos++;
    std::string int_part, frac_part;
    bool found_dot = false;
    while (pos < str.size()) {
        if (str[pos] == '.') {
            if (found_dot) break;
            found_dot = true;
        } else if (str[pos] >= '0' && str[pos] <= '9') {
            if (!found_dot) int_part += str[pos];
            else frac_part += str[pos];
        } else break;
        pos++;
    }
    if (int_part.empty()) int_part = "0";
    std::string combined = int_part + frac_part;
    int64_t value = 0;
    for (char c : combined) value = value * 10 + (c - '0');
    if (neg) value = -value;
    return decimal_alloc(value, (int32_t)frac_part.size());
}

AuroraDecimal* aurora_decimal_from_int(int64_t i) {
    return decimal_alloc(i, 0);
}

AuroraDecimal* aurora_decimal_from_float(double f) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.17g", f);
    return aurora_decimal_from_str(buf);
}

char* aurora_decimal_to_str(AuroraDecimal* d) {
    if (!d) return nullptr;
    char buf[64];
    if (d->scale == 0) {
        snprintf(buf, sizeof(buf), "%lld", (long long)d->value);
    } else {
        bool neg = d->value < 0;
        int64_t abs_val = neg ? -d->value : d->value;
        char fmt[64];
        /* split into int and frac parts */
        int64_t divisor = pow10(d->scale);
        int64_t int_part = abs_val / divisor;
        int64_t frac_part = abs_val % divisor;
        /* produce frac with leading zeros */
        char frac_str[32];
        snprintf(frac_str, sizeof(frac_str), "%0*lld", (int)d->scale, (long long)frac_part);
        /* trim trailing zeros */
        char* end = frac_str + strlen(frac_str) - 1;
        while (end > frac_str && *end == '0') end--;
        end[1] = 0;
        if (neg)
            snprintf(buf, sizeof(buf), "-%lld.%s", (long long)int_part, frac_str);
        else
            snprintf(buf, sizeof(buf), "%lld.%s", (long long)int_part, frac_str);
    }
    return strdup(buf);
}

void aurora_decimal_free(AuroraDecimal* d) { free(d); }

AuroraDecimal* aurora_decimal_add(AuroraDecimal* a, AuroraDecimal* b) {
    if (!a || !b) return nullptr;
    int64_t va, vb;
    int32_t scale;
    align(a, b, va, vb, scale);
    return decimal_alloc(va + vb, scale);
}

AuroraDecimal* aurora_decimal_sub(AuroraDecimal* a, AuroraDecimal* b) {
    if (!a || !b) return nullptr;
    int64_t va, vb;
    int32_t scale;
    align(a, b, va, vb, scale);
    return decimal_alloc(va - vb, scale);
}

AuroraDecimal* aurora_decimal_mul(AuroraDecimal* a, AuroraDecimal* b) {
    if (!a || !b) return nullptr;
    int64_t result = a->value * b->value;
    int32_t scale = a->scale + b->scale;
    return decimal_alloc(result, scale);
}

AuroraDecimal* aurora_decimal_div(AuroraDecimal* a, AuroraDecimal* b) {
    if (!a || !b || b->value == 0) return nullptr;
    /* scale up dividend for precision */
    int32_t target_scale = (a->scale > b->scale ? a->scale : b->scale) + 10;
    int64_t va = a->value;
    if (target_scale > a->scale) va *= pow10(target_scale - a->scale);
    int64_t result = va / b->value;
    return decimal_alloc(result, target_scale - b->scale);
}

int aurora_decimal_compare(AuroraDecimal* a, AuroraDecimal* b) {
    if (!a || !b) return 0;
    int64_t va, vb;
    int32_t scale;
    align(a, b, va, vb, scale);
    if (va < vb) return -1;
    if (va > vb) return 1;
    return 0;
}

AuroraDecimal* aurora_decimal_abs(AuroraDecimal* d) {
    if (!d) return nullptr;
    return decimal_alloc(d->value < 0 ? -d->value : d->value, d->scale);
}

AuroraDecimal* aurora_decimal_negate(AuroraDecimal* d) {
    if (!d) return nullptr;
    return decimal_alloc(-d->value, d->scale);
}

AuroraDecimal* aurora_decimal_round(AuroraDecimal* d, int places) {
    if (!d || places < 0) return nullptr;
    if (places >= d->scale) return decimal_alloc(d->value, d->scale);
    int64_t divisor = pow10(d->scale - places);
    int64_t rounded = d->value / divisor;
    /* round to nearest */
    int64_t remainder = d->value % divisor;
    if (remainder < 0) remainder = -remainder;
    if (remainder >= divisor / 2) {
        if (d->value >= 0) rounded++;
        else rounded--;
    }
    return decimal_alloc(rounded, places);
}

int aurora_decimal_is_zero(AuroraDecimal* d) { return d ? (d->value == 0 ? 1 : 0) : 0; }
int aurora_decimal_is_negative(AuroraDecimal* d) { return d ? (d->value < 0 ? 1 : 0) : 0; }

}
