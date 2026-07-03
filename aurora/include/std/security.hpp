#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Sandbox (4) ── */
int         aurora_sec_sandbox_init(void);
int         aurora_sec_sandbox_allow_path(const char* path);
int         aurora_sec_sandbox_check_path(const char* path);
void        aurora_sec_sandbox_destroy(void);

/* ── Permission Model (4) ── */
int         aurora_sec_permission_check(const char* perm);
int         aurora_sec_permission_request(const char* perm);
char*       aurora_sec_permission_list(void);
int         aurora_sec_permission_revoke(const char* perm);

/* ── Secure Storage (5) ── */
void*       aurora_sec_storage_open(const char* path, const unsigned char* key, int key_len);
int         aurora_sec_storage_set(void* store, const char* key, const char* value);
char*       aurora_sec_storage_get(void* store, const char* key);
int         aurora_sec_storage_remove(void* store, const char* key);
void        aurora_sec_storage_close(void* store);

/* ── Encryption (5) ── */
int         aurora_sec_generate_key(unsigned char* out, int len);
int         aurora_sec_generate_iv(unsigned char* out, int len);
int         aurora_sec_encrypt(const unsigned char* key, int key_len,
                               const unsigned char* iv,
                               const unsigned char* input, int in_len,
                               unsigned char* output, int* out_len);
int         aurora_sec_decrypt(const unsigned char* key, int key_len,
                               const unsigned char* iv,
                               const unsigned char* input, int in_len,
                               unsigned char* output, int* out_len);
int         aurora_sec_pbkdf2(const char* password, const unsigned char* salt, int salt_len,
                              int iterations, unsigned char* out, int out_len);

/* ── Certificate APIs (4) ── */
void*       aurora_sec_cert_load(const char* path);
char*       aurora_sec_cert_info(void* cert);
int         aurora_sec_cert_verify(void* cert, const char* ca_path);
void        aurora_sec_cert_free(void* cert);

/* ── Hashing (4) ── */
char*       aurora_sec_sha256(const unsigned char* data, int len);
char*       aurora_sec_hmac_sha256(const unsigned char* key, int key_len,
                                    const unsigned char* data, int data_len);
char*       aurora_sec_hash_password(const char* password);
int         aurora_sec_verify_password(const char* password, const char* hash);

/* ── Authentication Helpers (4) ── */
char*       aurora_sec_token_generate(const char* payload, const char* secret);
int         aurora_sec_token_verify(const char* token, const char* secret);
char*       aurora_sec_basic_auth(const char* username, const char* password);
char*       aurora_sec_bearer_auth(const char* token);

#ifdef __cplusplus
}
#endif
