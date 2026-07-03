#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Plugin ABI version — must match host ── */
#define AURORA_PLUGIN_ABI_VERSION 1

/* ── Plugin management (7) ── */
int         aurora_plugin_load(const char* path);
int         aurora_plugin_unload(const char* name);
int         aurora_plugin_unload_all(void);
int         aurora_plugin_get_count(void);
const char* aurora_plugin_get_name(int index);
int         aurora_plugin_is_loaded(const char* name);
int         aurora_plugin_scan(const char* directory);

/* ── Plugin metadata (3) ── */
const char* aurora_plugin_get_info(const char* name, const char* field);
int         aurora_plugin_get_abi(const char* name);
void*       aurora_plugin_get_function(const char* plugin, const char* func);

/* ── Reflection query (8) ── */
int         aurora_reflection_get_type_count(void);
const char* aurora_reflection_get_type_name(int index);
int         aurora_reflection_get_field_count(const char* type_name);
void        aurora_reflection_get_field_info(const char* type_name, int index,
                char** name, char** type, int* offset, int* size);
int         aurora_reflection_get_method_count(const char* type_name);
void        aurora_reflection_get_method_info(const char* type_name, int index,
                char** name, char** return_type);
void*       aurora_reflection_get_method_pointer(const char* type_name, const char* method_name);

/* ── Version compat (2) ── */
int         aurora_version_abi(void);
const char* aurora_version_string(void);

#ifdef __cplusplus
}
#endif
