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

#ifdef __cplusplus
}
#endif
