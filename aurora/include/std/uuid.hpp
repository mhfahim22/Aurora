#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AuroraUuid AuroraUuid;

AuroraUuid* aurora_uuid_v4(void);
AuroraUuid* aurora_uuid_parse(const char* str);
char*       aurora_uuid_to_string(AuroraUuid* uuid);
AuroraUuid* aurora_uuid_nil(void);
int         aurora_uuid_equal(AuroraUuid* a, AuroraUuid* b);
void        aurora_uuid_free(AuroraUuid* uuid);

#ifdef __cplusplus
}
#endif
