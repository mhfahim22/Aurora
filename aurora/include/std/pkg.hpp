#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Package Management (6) ── */
int         aurora_pkg_install(const char* name, const char* version);
int         aurora_pkg_remove(const char* name);
int         aurora_pkg_update(const char* name);
int         aurora_pkg_publish(const char* path);
int         aurora_pkg_search(const char* query, char*** out_results, int* out_count);
int         aurora_pkg_list_installed(char*** out_names, int* out_count);

/* ── Registry (2) ── */
void        aurora_pkg_set_registry(const char* url);
const char* aurora_pkg_get_registry(void);

/* ── Authentication (1) ── */
int         aurora_pkg_login(const char* token);

/* ── Lock File (3) ── */
int         aurora_pkg_lock_init(void);
int         aurora_pkg_lock_save(void);
int         aurora_pkg_lock_load(void);

/* ── Dependency Resolution (3) ── */
int         aurora_pkg_dep_resolve(const char* name);
int         aurora_pkg_dep_get_count(void);
const char* aurora_pkg_dep_get_name(int index);

/* ── Offline Cache (3) ── */
int         aurora_pkg_cache_list(char*** out_names, int* out_count);
void        aurora_pkg_cache_clear(void);
const char* aurora_pkg_cache_path(void);

#ifdef __cplusplus
}
#endif
