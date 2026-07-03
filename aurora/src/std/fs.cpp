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
#include <io.h>
#include <windows.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#else
#include <unistd.h>
#include <dirent.h>
#include <libgen.h>
#include <vector>
#include <string>
#include <algorithm>
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

char* aurora_fs_listdir(const char* path) {
    if (!path) return nullptr;
#if defined(_WIN32)
    char search[MAX_PATH];
    snprintf(search, sizeof(search), "%s\\*", path);
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(search, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return nullptr;
    size_t cap = 512, len = 0;
    char* buf = (char*)aurora_alloc(cap);
    if (!buf) { FindClose(hFind); return nullptr; }
    buf[0] = '\0';
    do {
        const char* name = ffd.cFileName;
        size_t nlen = strlen(name);
        if (len + nlen + 2 > cap) {
            cap *= 2;
            char* nb = (char*)aurora_alloc(cap);
            if (!nb) { aurora_free(buf); FindClose(hFind); return nullptr; }
            memcpy(nb, buf, len);
            aurora_free(buf);
            buf = nb;
        }
        memcpy(buf + len, name, nlen);
        len += nlen;
        buf[len++] = '\n';
    } while (FindNextFileA(hFind, &ffd) != 0);
    FindClose(hFind);
    buf[len] = '\0';
    return buf;
#else
    DIR* d = opendir(path);
    if (!d) return nullptr;
    size_t cap = 512, len = 0;
    char* buf = (char*)aurora_alloc(cap);
    if (!buf) { closedir(d); return nullptr; }
    buf[0] = '\0';
    struct dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        const char* name = ent->d_name;
        size_t nlen = strlen(name);
        if (len + nlen + 2 > cap) {
            cap *= 2;
            char* nb = (char*)aurora_alloc(cap);
            if (!nb) { aurora_free(buf); closedir(d); return nullptr; }
            memcpy(nb, buf, len);
            aurora_free(buf);
            buf = nb;
        }
        memcpy(buf + len, name, nlen);
        len += nlen;
        buf[len++] = '\n';
    }
    closedir(d);
    buf[len] = '\0';
    return buf;
#endif
}

/* ── Recursive directory walk ── */
static void walk_dir(const char* base, int base_len, char** buf, size_t* cap, size_t* len) {
#ifdef _WIN32
    char search[MAX_PATH];
    snprintf(search, sizeof(search), "%s\\*", base);
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(search, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0) continue;
        size_t nlen = strlen(ffd.cFileName);
        size_t need = *len + base_len + 1 + nlen + 1;
        if (need > *cap) {
            *cap = need * 2;
            char* nb = (char*)aurora_alloc(*cap);
            if (!nb) { FindClose(hFind); return; }
            memcpy(nb, *buf, *len);
            aurora_free(*buf);
            *buf = nb;
        }
        memcpy(*buf + *len, base, base_len);
        *len += base_len;
        if (base_len > 0) { (*buf)[*len] = '/'; *len += 1; }
        memcpy(*buf + *len, ffd.cFileName, nlen);
        *len += nlen;
        (*buf)[*len] = '\n'; *len += 1;
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            char sub[MAX_PATH];
            snprintf(sub, sizeof(sub), "%s\\%s", base, ffd.cFileName);
            walk_dir(sub, (int)(base_len + 1 + nlen), buf, cap, len);
        }
    } while (FindNextFileA(hFind, &ffd) != 0);
    FindClose(hFind);
#else
    DIR* d = opendir(base);
    if (!d) return;
    struct dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        size_t nlen = strlen(ent->d_name);
        size_t need = *len + base_len + 1 + nlen + 1;
        if (need > *cap) {
            *cap = need * 2;
            char* nb = (char*)aurora_alloc(*cap);
            if (!nb) { closedir(d); return; }
            memcpy(nb, *buf, *len);
            aurora_free(*buf);
            *buf = nb;
        }
        memcpy(*buf + *len, base, base_len);
        *len += base_len;
        if (base_len > 0) { (*buf)[*len] = '/'; *len += 1; }
        memcpy(*buf + *len, ent->d_name, nlen);
        *len += nlen;
        (*buf)[*len] = '\n'; *len += 1;

        struct stat st;
        char full[1024];
        snprintf(full, sizeof(full), "%s/%s", base, ent->d_name);
        if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
            walk_dir(full, (int)(base_len + 1 + nlen), buf, cap, len);
        }
    }
    closedir(d);
#endif
}

char* aurora_fs_walk(const char* path) {
    if (!path) return nullptr;
    size_t cap = 4096, len = 0;
    char* buf = (char*)aurora_alloc(cap);
    if (!buf) return nullptr;
    buf[0] = '\0';
    walk_dir(path, 0, &buf, &cap, &len);
    buf[len] = '\0';
    return buf;
}

/* ── File metadata (JSON-like format) ── */
char* aurora_fs_metadata(const char* path) {
    if (!path) return nullptr;
    struct stat st;
    if (stat(path, &st) != 0) return nullptr;
    char result[1024];
    int is_dir = (st.st_mode & S_IFDIR) ? 1 : 0;
#ifdef _WIN32
    snprintf(result, sizeof(result),
        "{\"size\":%lld,\"mtime\":%lld,\"mode\":\"%s\",\"is_dir\":%s}",
        (long long)st.st_size, (long long)st.st_mtime,
        is_dir ? "drwxr-xr-x" : "-rw-r--r--", is_dir ? "true" : "false");
#else
    char perm_str[11];
    snprintf(perm_str, sizeof(perm_str), "%c%c%c%c%c%c%c%c%c%c",
        S_ISDIR(st.st_mode) ? 'd' : '-',
        st.st_mode & S_IRUSR ? 'r' : '-',
        st.st_mode & S_IWUSR ? 'w' : '-',
        st.st_mode & S_IXUSR ? 'x' : '-',
        st.st_mode & S_IRGRP ? 'r' : '-',
        st.st_mode & S_IWGRP ? 'w' : '-',
        st.st_mode & S_IXGRP ? 'x' : '-',
        st.st_mode & S_IROTH ? 'r' : '-',
        st.st_mode & S_IWOTH ? 'w' : '-',
        st.st_mode & S_IXOTH ? 'x' : '-');
    snprintf(result, sizeof(result),
        "{\"size\":%lld,\"mtime\":%lld,\"mode\":\"%s\",\"is_dir\":%s}",
        (long long)st.st_size, (long long)st.st_mtime,
        perm_str, is_dir ? "true" : "false");
#endif
    char* out = (char*)aurora_alloc(strlen(result) + 1);
    if (out) strcpy(out, result);
    return out;
}

/* ── File permissions (string like "0644") ── */
char* aurora_fs_permissions(const char* path) {
    if (!path) return nullptr;
    struct stat st;
    if (stat(path, &st) != 0) return nullptr;
    char buf[16];
    snprintf(buf, sizeof(buf), "%o", (unsigned)(st.st_mode & 0777));
    char* out = (char*)aurora_alloc(strlen(buf) + 1);
    if (out) strcpy(out, buf);
    return out;
}

/* ── Set file permissions ── */
int aurora_fs_chmod(const char* path, int mode) {
    if (!path) return 0;
#ifdef _WIN32
    return _chmod(path, mode) == 0 ? 1 : 0;
#else
    return chmod(path, (mode_t)mode) == 0 ? 1 : 0;
#endif
}

/* ── Join path components ── */
char* aurora_fs_join(const char* dir, const char* file) {
    if (!dir && !file) return nullptr;
    if (!dir) return AURORA_STRDUP(file);
    if (!file) return AURORA_STRDUP(dir);
    size_t dlen = strlen(dir), flen = strlen(file);
#ifdef _WIN32
    const char sep = '\\';
#else
    const char sep = '/';
#endif
    int need_sep = (dlen > 0 && dir[dlen - 1] != sep && dir[dlen - 1] != '/'
#ifdef _WIN32
        && dir[dlen - 1] != '\\'
#endif
    ) ? 1 : 0;
    char* result = (char*)aurora_alloc(dlen + need_sep + flen + 1);
    if (!result) return nullptr;
    memcpy(result, dir, dlen);
    if (need_sep) result[dlen] = sep;
    memcpy(result + dlen + need_sep, file, flen);
    result[dlen + need_sep + flen] = '\0';
    return result;
}

/* ── Normalize path (resolve . and .., fix separators) ── */
char* aurora_fs_normalize(const char* path) {
    if (!path) return nullptr;
    char* copy = AURORA_STRDUP(path);
    if (!copy) return nullptr;
#ifdef _WIN32
    char full[MAX_PATH];
    if (_fullpath(full, copy, MAX_PATH)) {
        free(copy);
        char* result = (char*)aurora_alloc(strlen(full) + 1);
        if (result) strcpy(result, full);
        return result;
    }
    free(copy);
    return AURORA_STRDUP(path);
#else
    char* resolved = realpath(path, nullptr);
    if (resolved) {
        free(copy);
        char* result = (char*)aurora_alloc(strlen(resolved) + 1);
        if (result) strcpy(result, resolved);
        free(resolved);
        return result;
    }
    free(copy);
    return AURORA_STRDUP(path);
#endif
}

/* ── Compute relative path ── */
char* aurora_fs_relative(const char* path, const char* base) {
    if (!path || !base) return nullptr;
    char* abs_path = aurora_fs_absolute(path);
    char* abs_base = aurora_fs_absolute(base);
    if (!abs_path || !abs_base) {
        if (abs_path) aurora_free(abs_path);
        if (abs_base) aurora_free(abs_base);
        return nullptr;
    }
    char* result = nullptr;
#ifdef _WIN32
    char rel[MAX_PATH];
    if (PathRelativePathToA(rel, abs_base, FILE_ATTRIBUTE_DIRECTORY, abs_path, FILE_ATTRIBUTE_NORMAL)) {
        result = (char*)aurora_alloc(strlen(rel) + 1);
        if (result) strcpy(result, rel);
    }
#else
    std::string p(abs_path), b(abs_base);
    if (p == b) {
        result = (char*)aurora_alloc(2);
        if (result) strcpy(result, ".");
    } else {
        std::vector<std::string> pp, bp;
        auto split = [](const std::string& s, char d, std::vector<std::string>& out) {
            size_t start = 0, end;
            while ((end = s.find(d, start)) != std::string::npos) {
                if (end > start) out.push_back(s.substr(start, end - start));
                start = end + 1;
            }
            if (start < s.size()) out.push_back(s.substr(start));
        };
        split(p, '/', pp);
        split(b, '/', bp);
        size_t i = 0;
        while (i < pp.size() && i < bp.size() && pp[i] == bp[i]) i++;
        std::string rel;
        for (size_t j = i; j < bp.size(); j++) rel += "../";
        for (size_t j = i; j < pp.size(); j++) {
            if (j > i) rel += "/";
            rel += pp[j];
        }
        result = (char*)aurora_alloc(rel.size() + 1);
        if (result) memcpy(result, rel.c_str(), rel.size() + 1);
    }
#endif
    aurora_free(abs_path);
    aurora_free(abs_base);
    return result;
}

/* ── Convert to absolute path ── */
char* aurora_fs_absolute(const char* path) {
    if (!path) return nullptr;
#ifdef _WIN32
    char full[MAX_PATH];
    if (_fullpath(full, path, MAX_PATH)) {
        char* result = (char*)aurora_alloc(strlen(full) + 1);
        if (result) strcpy(result, full);
        return result;
    }
    return AURORA_STRDUP(path);
#else
    char* resolved = realpath(path, nullptr);
    if (resolved) {
        char* result = (char*)aurora_alloc(strlen(resolved) + 1);
        if (result) strcpy(result, resolved);
        free(resolved);
        return result;
    }
    return AURORA_STRDUP(path);
#endif
}

/* ── Get temp directory ── */
char* aurora_fs_temp_directory() {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD len = GetTempPathA(MAX_PATH, buf);
    if (len > 0 && len < MAX_PATH) {
        if (len > 0 && buf[len - 1] == '\\') buf[len - 1] = '\0';
        char* result = (char*)aurora_alloc(strlen(buf) + 1);
        if (result) strcpy(result, buf);
        return result;
    }
    return AURORA_STRDUP("C:\\Windows\\Temp");
#else
    const char* tmp = getenv("TMPDIR");
    if (!tmp) tmp = getenv("TMP");
    if (!tmp) tmp = getenv("TEMP");
    if (!tmp) tmp = "/tmp";
    char* result = (char*)aurora_alloc(strlen(tmp) + 1);
    if (result) strcpy(result, tmp);
    return result;
#endif
}

/* ── Get file extension ── */
char* aurora_fs_extension(const char* path) {
    if (!path) return nullptr;
    const char* dot = strrchr(path, '.');
    if (!dot || dot == path) {
        char* empty = (char*)aurora_alloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    const char* sep = strrchr(path, '/');
#ifdef _WIN32
    const char* bsep = strrchr(path, '\\');
    if (!sep || (bsep && bsep > sep)) sep = bsep;
#endif
    if (sep && sep > dot) {
        char* empty = (char*)aurora_alloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    char* result = (char*)aurora_alloc(strlen(dot) + 1);
    if (result) strcpy(result, dot);
    return result;
}

}
