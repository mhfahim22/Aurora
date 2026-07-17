// C JSON Parsing Benchmark
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

// Simple JSON parser (handles basic objects with string/number/bool/null values)
// This is a minimal parser to avoid needing a JSON library dependency
typedef struct {
    const char* start;
    const char* pos;
} json_ctx;

static void json_skip_ws(json_ctx* ctx) {
    while (*ctx->pos == ' ' || *ctx->pos == '\t' || *ctx->pos == '\n' || *ctx->pos == '\r')
        ctx->pos++;
}

static int json_parse_value(json_ctx* ctx) {
    json_skip_ws(ctx);
    if (*ctx->pos == '{') {
        ctx->pos++; // skip {
        json_skip_ws(ctx);
        if (*ctx->pos != '}') {
            while (1) {
                json_skip_ws(ctx);
                if (*ctx->pos == '"') {
                    ctx->pos++; // skip opening quote
                    while (*ctx->pos && *ctx->pos != '"') {
                        if (*ctx->pos == '\\') ctx->pos++;
                        ctx->pos++;
                    }
                    if (*ctx->pos == '"') ctx->pos++;
                }
                json_skip_ws(ctx);
                if (*ctx->pos == ':') ctx->pos++;
                json_parse_value(ctx);
                json_skip_ws(ctx);
                if (*ctx->pos == ',') ctx->pos++;
                else if (*ctx->pos == '}') break;
            }
        }
        if (*ctx->pos == '}') ctx->pos++;
        return 1;
    }
    if (*ctx->pos == '[') {
        ctx->pos++;
        json_skip_ws(ctx);
        if (*ctx->pos != ']') {
            while (1) {
                json_parse_value(ctx);
                json_skip_ws(ctx);
                if (*ctx->pos == ',') ctx->pos++;
                else if (*ctx->pos == ']') break;
            }
        }
        if (*ctx->pos == ']') ctx->pos++;
        return 1;
    }
    if (*ctx->pos == '"') {
        ctx->pos++;
        while (*ctx->pos && *ctx->pos != '"') {
            if (*ctx->pos == '\\') ctx->pos++;
            ctx->pos++;
        }
        if (*ctx->pos == '"') ctx->pos++;
        return 1;
    }
    if (*ctx->pos == 't' || *ctx->pos == 'f' || *ctx->pos == 'n') {
        while (*ctx->pos && ((*ctx->pos >= 'a' && *ctx->pos <= 'z') || (*ctx->pos >= 'A' && *ctx->pos <= 'Z')))
            ctx->pos++;
        return 1;
    }
    if (*ctx->pos == '-' || (*ctx->pos >= '0' && *ctx->pos <= '9')) {
        if (*ctx->pos == '-') ctx->pos++;
        while (*ctx->pos >= '0' && *ctx->pos <= '9') ctx->pos++;
        if (*ctx->pos == '.') {
            ctx->pos++;
            while (*ctx->pos >= '0' && *ctx->pos <= '9') ctx->pos++;
        }
        return 1;
    }
    return 0;
}

int main() {
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);

    const char* json_str = "{\"name\":\"Alice\",\"age\":30,\"city\":\"New York\",\"score\":95.5,\"active\":true,\"data\":[1,2,3,4,5],\"nested\":{\"a\":1,\"b\":2}}";
    int count = 100000;

    QueryPerformanceCounter(&start);
    for (int i = 0; i < count; i++) {
        json_ctx ctx;
        ctx.start = json_str;
        ctx.pos = json_str;
        json_parse_value(&ctx);
    }
    QueryPerformanceCounter(&end);

    double ms = (double)(end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart;
    printf("C JSON: %.0f ms (%d iterations, %.0f ns/op)\n", ms, count, ms * 1e6 / count);

    return 0;
}
