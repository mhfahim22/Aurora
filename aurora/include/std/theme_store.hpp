#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Theme Store — search, install, apply, publish ── */

int         aurora_theme_store_search(const char* query, char*** out_results, int* out_count);
int         aurora_theme_store_install(const char* name);
int         aurora_theme_store_uninstall(const char* name);
int         aurora_theme_store_list_installed(char*** out_names, int* out_count);
int         aurora_theme_store_apply(const char* name);
int         aurora_theme_store_publish(const char* path);
const char* aurora_theme_store_get_current(void);
const char* aurora_theme_store_export_json(const char* name);
int         aurora_theme_store_import_json(const char* json);
int         aurora_theme_store_validate(const char* path);

#ifdef __cplusplus
}
#endif
