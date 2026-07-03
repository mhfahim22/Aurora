#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

char*   aurora_fs_read_file(const char* path);
int     aurora_fs_write_file(const char* path, const char* content);
int     aurora_fs_exists(const char* path);
int     aurora_fs_copy(const char* src, const char* dst);
int     aurora_fs_remove(const char* path);
int64_t aurora_fs_size(const char* path);
int     aurora_fs_append_file(const char* path, const char* content);
int     aurora_fs_mkdir(const char* path);
int     aurora_fs_rmdir(const char* path);
int     aurora_fs_rename(const char* old, const char* newname);
int     aurora_fs_is_dir(const char* path);
char*   aurora_fs_dirname(const char* path);
char*   aurora_fs_basename(const char* path);
char*   aurora_fs_listdir(const char* path);

/* ── File system watching (polling) ── */
void*   aurora_fs_watch_init(const char* path);
int     aurora_fs_watch_poll(void* state);
void    aurora_fs_watch_free(void* state);

/* ── Phase 2: Additional Filesystem APIs ── */
char*   aurora_fs_walk(const char* path);
char*   aurora_fs_metadata(const char* path);
char*   aurora_fs_permissions(const char* path);
int     aurora_fs_chmod(const char* path, int mode);
char*   aurora_fs_join(const char* dir, const char* file);
char*   aurora_fs_normalize(const char* path);
char*   aurora_fs_relative(const char* path, const char* base);
char*   aurora_fs_absolute(const char* path);
char*   aurora_fs_temp_directory();
char*   aurora_fs_extension(const char* path);

#ifdef __cplusplus
}
#endif
