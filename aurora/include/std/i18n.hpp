#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char language[8];
    char region[8];
    char script[16];
} AuroraLocale;

int         aurora_i18n_locale(AuroraLocale* out);
int         aurora_i18n_set_locale(const char* lang);
const char* aurora_i18n_get_locale(void);
const char* aurora_i18n_get_system_locale(void);
const char* aurora_i18n_translate(const char* key, const char* domain);
int         aurora_i18n_load(const char* domain, const char* filepath);
int         aurora_i18n_load_json(const char* domain, const char* json_data);
int         aurora_i18n_add(const char* domain, const char* lang, const char* key, const char* value);
const char* aurora_i18n_format(const char* pattern, int argc, const char** args);
int         aurora_i18n_rtl(const char* locale);
int         aurora_i18n_language_name(const char* lang, char* buf, int bufsize);
int         aurora_i18n_available_locales(char* buf, int bufsize);

#ifdef __cplusplus
}
#endif
