#include "std/fs.hpp"
#include "runtime/memory.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>

#ifdef _WIN32
#define AURORA_STRDUP _strdup
#else
#define AURORA_STRDUP strdup
#endif

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#else
#include <unistd.h>
#include <dirent.h>
#include <libgen.h>
#endif

extern "C" {

char* aurora_fs_read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return nullptr;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)aurora_alloc(size + 1);
    if (!buf) { fclose(f); return nullptr; }
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    return buf;
}

int aurora_fs_write_file(const char* path, const char* content) {
    FILE* f = fopen(path, "wb");
    if (!f) return 0;
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, f);
    fclose(f);
    return (int)(written == len);
}

int aurora_fs_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 ? 1 : 0;
}

int aurora_fs_copy(const char* src, const char* dst) {
    char* content = aurora_fs_read_file(src);
    if (!content) return 0;
    int result = aurora_fs_write_file(dst, content);
    aurora_free(content);
    return result;
}

int aurora_fs_remove(const char* path) {
    return remove(path) == 0 ? 1 : 0;
}

int64_t aurora_fs_size(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (int64_t)st.st_size;
}

int aurora_fs_append_file(const char* path, const char* content) {
    FILE* f = fopen(path, "ab");
    if (!f) return 0;
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, f);
    fclose(f);
    return (int)(written == len);
}

int aurora_fs_mkdir(const char* path) {
#ifdef _WIN32
    return _mkdir(path) == 0 ? 1 : 0;
#else
    return mkdir(path, 0755) == 0 ? 1 : 0;
#endif
}

int aurora_fs_rmdir(const char* path) {
#ifdef _WIN32
    return _rmdir(path) == 0 ? 1 : 0;
#else
    return rmdir(path) == 0 ? 1 : 0;
#endif
}

int aurora_fs_rename(const char* old, const char* newname) {
    return rename(old, newname) == 0 ? 1 : 0;
}

int aurora_fs_is_dir(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (st.st_mode & S_IFDIR) ? 1 : 0;
}

char* aurora_fs_dirname(const char* path) {
    char* copy = AURORA_STRDUP(path);
    if (!copy) return nullptr;
#ifdef _WIN32
    char drive[_MAX_DRIVE], dir[_MAX_DIR];
    _splitpath(copy, drive, dir, nullptr, nullptr);
    char* result = (char*)aurora_alloc(strlen(drive) + strlen(dir) + 1);
    if (result) {
        strcpy(result, drive);
        strcat(result, dir);
        size_t len = strlen(result);
        if (len > 0 && result[len - 1] == '\\')
            result[len - 1] = '\0';
    }
#else
    char* result = strdup(dirname(copy));
#endif
    free(copy);
    return result;
}

char* aurora_fs_basename(const char* path) {
    char* copy = AURORA_STRDUP(path);
    if (!copy) return nullptr;
#ifdef _WIN32
    char fname[_MAX_FNAME], ext[_MAX_EXT];
    _splitpath(copy, nullptr, nullptr, fname, ext);
    char* result = (char*)aurora_alloc(strlen(fname) + strlen(ext) + 1);
    if (result) {
        strcpy(result, fname);
        strcat(result, ext);
    }
#else
    char* result = strdup(basename(copy));
#endif
    free(copy);
    return result;
}

}
