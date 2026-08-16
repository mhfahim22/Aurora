#include "std/uuid.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <random>

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

struct AuroraUuid {
    uint8_t bytes[16];
};

static std::mt19937_64& rng() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    return gen;
}

extern "C" {

AuroraUuid* aurora_uuid_v4(void) {
    auto* uuid = (AuroraUuid*)malloc(sizeof(AuroraUuid));
    auto& gen = rng();
    for (int i = 0; i < 2; i++) {
        uint64_t r = gen();
        for (int j = 0; j < 8 && i * 8 + j < 16; j++)
            uuid->bytes[i * 8 + j] = (uint8_t)(r >> (j * 8));
    }
    uuid->bytes[6] = (uuid->bytes[6] & 0x0f) | 0x40;
    uuid->bytes[8] = (uuid->bytes[8] & 0x3f) | 0x80;
    return uuid;
}

AuroraUuid* aurora_uuid_parse(const char* str) {
    if (!str || strlen(str) < 36) return nullptr;
    auto* uuid = (AuroraUuid*)malloc(sizeof(AuroraUuid));
    unsigned int b[16];
    if (sscanf(str, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
               &b[0], &b[1], &b[2], &b[3],
               &b[4], &b[5], &b[6], &b[7],
               &b[8], &b[9], &b[10], &b[11],
               &b[12], &b[13], &b[14], &b[15]) != 16) {
        free(uuid);
        return nullptr;
    }
    for (int i = 0; i < 16; i++) uuid->bytes[i] = (uint8_t)b[i];
    return uuid;
}

char* aurora_uuid_to_string(AuroraUuid* uuid) {
    if (!uuid) return nullptr;
    char buf[37];
    snprintf(buf, sizeof(buf),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuid->bytes[0], uuid->bytes[1], uuid->bytes[2], uuid->bytes[3],
        uuid->bytes[4], uuid->bytes[5], uuid->bytes[6], uuid->bytes[7],
        uuid->bytes[8], uuid->bytes[9], uuid->bytes[10], uuid->bytes[11],
        uuid->bytes[12], uuid->bytes[13], uuid->bytes[14], uuid->bytes[15]);
    return strdup(buf);
}

AuroraUuid* aurora_uuid_nil(void) {
    auto* uuid = (AuroraUuid*)malloc(sizeof(AuroraUuid));
    memset(uuid->bytes, 0, 16);
    return uuid;
}

int aurora_uuid_equal(AuroraUuid* a, AuroraUuid* b) {
    if (!a || !b) return 0;
    return memcmp(a->bytes, b->bytes, 16) == 0 ? 1 : 0;
}

void aurora_uuid_free(AuroraUuid* uuid) { free(uuid); }

}
