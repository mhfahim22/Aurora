#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AuroraUrl AuroraUrl;

AuroraUrl* aurora_url_parse(const char* str);
void       aurora_url_free(AuroraUrl* url);

const char* aurora_url_scheme(AuroraUrl* url);
const char* aurora_url_host(AuroraUrl* url);
int         aurora_url_port(AuroraUrl* url);
const char* aurora_url_path(AuroraUrl* url);
const char* aurora_url_query(AuroraUrl* url);
const char* aurora_url_fragment(AuroraUrl* url);
const char* aurora_url_userinfo(AuroraUrl* url);

char* aurora_url_build(const char* scheme, const char* host, int port,
                       const char* path, const char* query, const char* fragment);

int  aurora_url_encode(const char* input, char* out, int out_size);
int  aurora_url_decode(const char* input, char* out, int out_size);

#ifdef __cplusplus
}
#endif
