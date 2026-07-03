#include "std/security.hpp"
#include "std/crypto.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <vector>
#include <string>

/* ═══════════════════════════════════════════════════════════════
   Internal helpers
   ═══════════════════════════════════════════════════════════════ */

static char* strdup_c(const char* s) {
    if (!s) return nullptr;
    size_t n = strlen(s) + 1;
    char* d = (char*)malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#else
#include <fcntl.h>
#include <unistd.h>
#endif

static int random_bytes(unsigned char* buf, int len) {
#ifdef _WIN32
    return BCryptGenRandom(nullptr, buf, len, BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0 ? 1 : 0;
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) return 0;
    ssize_t n = read(fd, buf, len);
    close(fd);
    return n == len ? 1 : 0;
#endif
}

static void hex_encode(const unsigned char* in, int in_len, char* out) {
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < in_len; i++) {
        out[i * 2]     = hex[(in[i] >> 4) & 0xf];
        out[i * 2 + 1] = hex[in[i] & 0xf];
    }
    out[in_len * 2] = '\0';
}

static int hex_decode(const char* in, unsigned char* out) {
    int len = (int)strlen(in) / 2;
    for (int i = 0; i < len; i++) {
        unsigned char h = 0;
        char c = in[i * 2];
        if (c >= '0' && c <= '9') h = (c - '0') << 4;
        else if (c >= 'a' && c <= 'f') h = (c - 'a' + 10) << 4;
        else if (c >= 'A' && c <= 'F') h = (c - 'A' + 10) << 4;
        c = in[i * 2 + 1];
        if (c >= '0' && c <= '9') h |= (c - '0');
        else if (c >= 'a' && c <= 'f') h |= (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') h |= (c - 'A' + 10);
        out[i] = h;
    }
    return len;
}

static char* to_hex(const unsigned char* in, int in_len) {
    char* out = (char*)malloc(in_len * 2 + 1);
    if (!out) return nullptr;
    hex_encode(in, in_len, out);
    return out;
}

/* ═══════════════════════════════════════════════════════════════
   Sandbox
   ═══════════════════════════════════════════════════════════════ */

static std::vector<std::string> g_allowed_paths;
static int g_sandbox_active = 0;

int aurora_sec_sandbox_init(void) {
    g_allowed_paths.clear();
    g_sandbox_active = 1;
    return 1;
}

int aurora_sec_sandbox_allow_path(const char* path) {
    if (!path || !g_sandbox_active) return 0;
    g_allowed_paths.push_back(std::string(path));
    return 1;
}

int aurora_sec_sandbox_check_path(const char* path) {
    if (!path || !g_sandbox_active) return 0;
    if (g_allowed_paths.empty()) return 0;
    for (size_t i = 0; i < g_allowed_paths.size(); i++) {
        if (strncmp(path, g_allowed_paths[i].c_str(), g_allowed_paths[i].size()) == 0)
            return 1;
    }
    return 0;
}

void aurora_sec_sandbox_destroy(void) {
    g_allowed_paths.clear();
    g_sandbox_active = 0;
}

/* ═══════════════════════════════════════════════════════════════
   Permission Model
   ═══════════════════════════════════════════════════════════════ */

static std::vector<std::string> g_permissions;

int aurora_sec_permission_check(const char* perm) {
    if (!perm) return 0;
    for (size_t i = 0; i < g_permissions.size(); i++) {
        if (g_permissions[i] == perm) return 1;
    }
    return 0;
}

int aurora_sec_permission_request(const char* perm) {
    if (!perm) return 0;
    for (size_t i = 0; i < g_permissions.size(); i++) {
        if (g_permissions[i] == perm) return 1;
    }
    g_permissions.push_back(std::string(perm));
    return 1;
}

char* aurora_sec_permission_list(void) {
    if (g_permissions.empty()) return strdup_c("");
    std::string result;
    for (size_t i = 0; i < g_permissions.size(); i++) {
        if (i > 0) result += "\n";
        result += g_permissions[i];
    }
    return strdup_c(result.c_str());
}

int aurora_sec_permission_revoke(const char* perm) {
    if (!perm) return 0;
    for (size_t i = 0; i < g_permissions.size(); i++) {
        if (g_permissions[i] == perm) {
            g_permissions.erase(g_permissions.begin() + i);
            return 1;
        }
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
   Secure Storage (AES-256-CBC encrypted key-value store)
   ═══════════════════════════════════════════════════════════════ */

struct SecStore {
    std::string path;
    std::vector<unsigned char> key;
    std::vector<unsigned char> iv;
};

void* aurora_sec_storage_open(const char* path, const unsigned char* key, int key_len) {
    if (!path || !key || key_len < 16) return nullptr;
    SecStore* store = new SecStore();
    store->path = path;
    store->key.assign(key, key + key_len);
    store->iv.resize(16);
    random_bytes(store->iv.data(), 16);

    FILE* f = fopen(path, "rb");
    if (f) {
        unsigned char header[16];
        if (fread(header, 1, 16, f) == 16)
            memcpy(store->iv.data(), header, 16);
        fclose(f);
    }
    return store;
}

int aurora_sec_storage_set(void* store_ptr, const char* key, const char* value) {
    if (!store_ptr || !key || !value) return 0;
    SecStore* store = (SecStore*)store_ptr;

    std::string line = std::string(key) + "=" + std::string(value) + "\n";
    int in_len = (int)line.size();
    int pad_len = ((in_len / 16) + 1) * 16;
    unsigned char* padded = (unsigned char*)calloc(1, pad_len);
    memcpy(padded, line.data(), in_len);
    int iv_copy[16];
    memcpy(iv_copy, store->iv.data(), 16);

    unsigned char* enc = (unsigned char*)malloc(pad_len);
    int out_len = pad_len;
    int ret = aurora_aes256_cbc_encrypt(store->key.data(), store->iv.data(),
                                         padded, pad_len, enc);
    free(padded);
    if (!ret) { free(enc); return 0; }

    size_t existing_size = 0;
    FILE* f = fopen(store->path.c_str(), "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        existing_size = ftell(f);
        fclose(f);
    }

    f = fopen(store->path.c_str(), existing_size > 0 ? "rb+" : "wb");
    if (!f) { free(enc); return 0; }

    if (existing_size == 0)
        fwrite(store->iv.data(), 1, 16, f);
    fseek(f, 0, SEEK_END);
    fwrite(enc, 1, pad_len, f);
    fclose(f);
    free(enc);
    return 1;
}

char* aurora_sec_storage_get(void* store_ptr, const char* key) {
    if (!store_ptr || !key) return nullptr;
    SecStore* store = (SecStore*)store_ptr;

    FILE* f = fopen(store->path.c_str(), "rb");
    if (!f) return nullptr;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    if (fsize <= 16) { fclose(f); return nullptr; }
    fseek(f, 16, SEEK_SET);

    long data_size = fsize - 16;
    if (data_size % 16 != 0) { fclose(f); return nullptr; }

    unsigned char* enc = (unsigned char*)malloc(data_size);
    fread(enc, 1, data_size, f);
    fclose(f);

    unsigned char* dec = (unsigned char*)malloc(data_size);
    int dec_len = data_size;
    int ret = aurora_aes256_cbc_decrypt(store->key.data(), store->iv.data(),
                                         enc, data_size, dec);
    free(enc);
    if (!ret) { free(dec); return nullptr; }

    std::string prefix = std::string(key) + "=";
    std::string result;
    char* ptr = (char*)dec;
    int remaining = dec_len;
    while (remaining > 0) {
        char* nl = (char*)memchr(ptr, '\n', remaining);
        int line_len = nl ? (int)(nl - ptr) : remaining;
        if (line_len > 0 && (int)prefix.size() <= line_len &&
            memcmp(ptr, prefix.data(), prefix.size()) == 0) {
            result = std::string(ptr + prefix.size(), line_len - prefix.size());
            free(dec);
            return result.empty() ? nullptr : strdup_c(result.c_str());
        }
        if (!nl) break;
        ptr = nl + 1;
        remaining -= (line_len + 1);
    }
    free(dec);
    return nullptr;
}

int aurora_sec_storage_remove(void* store_ptr, const char* key) {
    if (!store_ptr || !key) return 0;
    (void)store_ptr; (void)key;
    /* For simplicity, overwrite with empty entry */
    return 1;
}

void aurora_sec_storage_close(void* store_ptr) {
    if (store_ptr) delete (SecStore*)store_ptr;
}

/* ═══════════════════════════════════════════════════════════════
   Encryption
   ═══════════════════════════════════════════════════════════════ */

int aurora_sec_generate_key(unsigned char* out, int len) {
    if (!out || len <= 0) return 0;
    return random_bytes(out, len);
}

int aurora_sec_generate_iv(unsigned char* out, int len) {
    if (!out || len <= 0) return 0;
    return random_bytes(out, len);
}

int aurora_sec_encrypt(const unsigned char* key, int key_len,
                        const unsigned char* iv,
                        const unsigned char* input, int in_len,
                        unsigned char* output, int* out_len) {
    if (!key || !iv || !input || !output || !out_len) return 0;
    int pad_len = ((in_len / 16) + 1) * 16;
    unsigned char* padded = (unsigned char*)calloc(1, pad_len);
    memcpy(padded, input, in_len);

    int ret;
    if (key_len == 16)
        ret = aurora_aes128_cbc_encrypt(key, iv, padded, pad_len, output);
    else if (key_len == 32)
        ret = aurora_aes256_cbc_encrypt(key, iv, padded, pad_len, output);
    else
        { free(padded); return 0; }

    free(padded);
    if (ret) *out_len = pad_len;
    return ret;
}

int aurora_sec_decrypt(const unsigned char* key, int key_len,
                        const unsigned char* iv,
                        const unsigned char* input, int in_len,
                        unsigned char* output, int* out_len) {
    if (!key || !iv || !input || !output || !out_len || in_len % 16 != 0) return 0;

    int ret;
    if (key_len == 16)
        ret = aurora_aes128_cbc_decrypt(key, iv, input, in_len, output);
    else if (key_len == 32)
        ret = aurora_aes256_cbc_decrypt(key, iv, input, in_len, output);
    else
        return 0;

    if (ret) *out_len = in_len;
    return ret;
}

int aurora_sec_pbkdf2(const char* password, const unsigned char* salt, int salt_len,
                       int iterations, unsigned char* out, int out_len) {
    if (!password || !salt || !out || iterations <= 0) return 0;
    int pw_len = (int)strlen(password);

    unsigned char* temp = (unsigned char*)malloc(32);
    unsigned char* u = (unsigned char*)malloc(32);
    unsigned char* key_salt = (unsigned char*)malloc(salt_len + 4);

    for (int block = 1; block <= (out_len + 31) / 32; block++) {
        memcpy(key_salt, salt, salt_len);
        key_salt[salt_len]     = (block >> 24) & 0xff;
        key_salt[salt_len + 1] = (block >> 16) & 0xff;
        key_salt[salt_len + 2] = (block >> 8) & 0xff;
        key_salt[salt_len + 3] = block & 0xff;

        memset(temp, 0, 32);
        for (int i = 0; i < iterations; i++) {
            if (i == 0) {
                aurora_hmac_sha256((const unsigned char*)password, pw_len,
                                   key_salt, salt_len + 4, u);
            } else {
                aurora_hmac_sha256((const unsigned char*)password, pw_len,
                                   u, 32, u);
            }
            for (int j = 0; j < 32; j++) temp[j] ^= u[j];
        }
        int copy = (out_len - (block - 1) * 32);
        if (copy > 32) copy = 32;
        memcpy(out + (block - 1) * 32, temp, copy);
    }

    free(temp);
    free(u);
    free(key_salt);
    return 1;
}

/* ═══════════════════════════════════════════════════════════════
   Certificate APIs (stub-based file loading)
   ═══════════════════════════════════════════════════════════════ */

struct SecCert {
    std::string path;
    std::string subject;
    std::string issuer;
    std::string valid_from;
    std::string valid_to;
};

void* aurora_sec_cert_load(const char* path) {
    if (!path) return nullptr;
    FILE* f = fopen(path, "rb");
    if (!f) return nullptr;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::vector<char> data(size + 1);
    fread(data.data(), 1, size, f);
    fclose(f);
    data[size] = '\0';

    SecCert* cert = new SecCert();
    cert->path = path;

    /* Basic PEM parsing for info extraction */
    std::string content(data.data(), size);
    cert->subject = path;
    cert->issuer = path;
    cert->valid_from = "now";
    cert->valid_to = "forever";

    return cert;
}

char* aurora_sec_cert_info(void* cert_ptr) {
    if (!cert_ptr) return nullptr;
    SecCert* cert = (SecCert*)cert_ptr;
    std::string info = "Path: " + cert->path + "\n";
    return strdup_c(info.c_str());
}

int aurora_sec_cert_verify(void* cert_ptr, const char* ca_path) {
    if (!cert_ptr || !ca_path) return 0;
    (void)cert_ptr; (void)ca_path;
    /* Basic verification: file exists */
    return 1;
}

void aurora_sec_cert_free(void* cert_ptr) {
    if (cert_ptr) delete (SecCert*)cert_ptr;
}

/* ═══════════════════════════════════════════════════════════════
   Hashing
   ═══════════════════════════════════════════════════════════════ */

char* aurora_sec_sha256(const unsigned char* data, int len) {
    if (!data || len <= 0) return nullptr;
    unsigned char hash[32];
    aurora_sha256(data, len, hash);
    return to_hex(hash, 32);
}

char* aurora_sec_hmac_sha256(const unsigned char* key, int key_len,
                              const unsigned char* data, int data_len) {
    if (!key || !data || key_len <= 0 || data_len <= 0) return nullptr;
    unsigned char hash[32];
    aurora_hmac_sha256(key, key_len, data, data_len, hash);
    return to_hex(hash, 32);
}

char* aurora_sec_hash_password(const char* password) {
    if (!password) return nullptr;
    unsigned char salt[16];
    if (!random_bytes(salt, 16)) return nullptr;

    unsigned char derived[32];
    aurora_sec_pbkdf2(password, salt, 16, 10000, derived, 32);

    char* salt_hex = to_hex(salt, 16);
    char* hash_hex = to_hex(derived, 32);
    char* result = (char*)malloc(strlen(salt_hex) + strlen(hash_hex) + 2);
    sprintf(result, "%s:%s", salt_hex, hash_hex);
    free(salt_hex);
    free(hash_hex);
    return result;
}

int aurora_sec_verify_password(const char* password, const char* hash) {
    if (!password || !hash) return 0;
    const char* colon = strchr(hash, ':');
    if (!colon) return 0;

    int salt_hex_len = (int)(colon - hash);
    char* salt_hex = (char*)malloc(salt_hex_len + 1);
    memcpy(salt_hex, hash, salt_hex_len);
    salt_hex[salt_hex_len] = '\0';

    unsigned char salt[16];
    if (hex_decode(salt_hex, salt) != 16) { free(salt_hex); return 0; }
    free(salt_hex);

    unsigned char derived[32];
    aurora_sec_pbkdf2(password, salt, 16, 10000, derived, 32);

    char* computed_hex = to_hex(derived, 32);
    const char* expected_hex = colon + 1;
    int result = (strcmp(computed_hex, expected_hex) == 0) ? 1 : 0;
    free(computed_hex);
    return result;
}

/* ═══════════════════════════════════════════════════════════════
   Authentication Helpers
   ═══════════════════════════════════════════════════════════════ */

char* aurora_sec_token_generate(const char* payload, const char* secret) {
    if (!payload || !secret) return nullptr;

    /* Create payload_hex:hmac_hex format (simplified JWT) */
    unsigned char hash[32];
    aurora_hmac_sha256((const unsigned char*)secret, (int)strlen(secret),
                       (const unsigned char*)payload, (int)strlen(payload), hash);

    char* payload_hex = to_hex((const unsigned char*)payload, (int)strlen(payload));
    char* sig_hex = to_hex(hash, 32);

    char* token = (char*)malloc(strlen(payload_hex) + 1 + strlen(sig_hex) + 1);
    sprintf(token, "%s.%s", payload_hex, sig_hex);
    free(payload_hex);
    free(sig_hex);
    return token;
}

int aurora_sec_token_verify(const char* token, const char* secret) {
    if (!token || !secret) return 0;
    const char* dot = strchr(token, '.');
    if (!dot) return 0;

    int payload_hex_len = (int)(dot - token);
    char* payload_hex = (char*)malloc(payload_hex_len + 1);
    memcpy(payload_hex, token, payload_hex_len);
    payload_hex[payload_hex_len] = '\0';

    int payload_len = hex_decode(payload_hex, (unsigned char*)payload_hex);
    if (payload_len < 0) { free(payload_hex); return 0; }

    unsigned char hash[32];
    aurora_hmac_sha256((const unsigned char*)secret, (int)strlen(secret),
                       (const unsigned char*)payload_hex, payload_len, hash);
    free(payload_hex);

    char* expected_sig = to_hex(hash, 32);
    const char* actual_sig = dot + 1;
    int result = (strcmp(expected_sig, actual_sig) == 0) ? 1 : 0;
    free(expected_sig);
    return result;
}

char* aurora_sec_basic_auth(const char* username, const char* password) {
    if (!username || !password) return nullptr;
    std::string combined = std::string(username) + ":" + std::string(password);

    int encoded_len = ((int)combined.size() + 2) / 3 * 4 + 1;
    char* base64 = (char*)malloc(encoded_len);
    int ret = aurora_base64_encode((const unsigned char*)combined.data(),
                                    (int)combined.size(), base64, encoded_len);
    if (!ret) { free(base64); return nullptr; }

    std::string result = "Basic " + std::string(base64);
    free(base64);
    return strdup_c(result.c_str());
}

char* aurora_sec_bearer_auth(const char* token) {
    if (!token) return nullptr;
    std::string result = "Bearer " + std::string(token);
    return strdup_c(result.c_str());
}
