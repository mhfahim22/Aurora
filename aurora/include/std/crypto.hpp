#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

void aurora_sha256(const unsigned char* data, size_t len, unsigned char* out);

void aurora_hmac_sha256(const unsigned char* key, size_t key_len,
                        const unsigned char* data, size_t data_len,
                        unsigned char* out);

int aurora_aes128_ecb_encrypt(const unsigned char* key, const unsigned char* input,
                              size_t len, unsigned char* output);

int aurora_aes128_ecb_decrypt(const unsigned char* key, const unsigned char* input,
                              size_t len, unsigned char* output);

int aurora_aes256_ecb_encrypt(const unsigned char* key, const unsigned char* input,
                              size_t len, unsigned char* output);

int aurora_aes256_ecb_decrypt(const unsigned char* key, const unsigned char* input,
                              size_t len, unsigned char* output);

int aurora_aes128_cbc_encrypt(const unsigned char* key, const unsigned char* iv,
                              const unsigned char* input, size_t len,
                              unsigned char* output);

int aurora_aes128_cbc_decrypt(const unsigned char* key, const unsigned char* iv,
                              const unsigned char* input, size_t len,
                              unsigned char* output);

int aurora_aes256_cbc_encrypt(const unsigned char* key, const unsigned char* iv,
                              const unsigned char* input, size_t len,
                              unsigned char* output);

int aurora_aes256_cbc_decrypt(const unsigned char* key, const unsigned char* iv,
                              const unsigned char* input, size_t len,
                              unsigned char* output);

int aurora_base64_encode(const unsigned char* data, size_t len,
                         char* out, size_t out_size);

int aurora_base64_decode(const char* in, unsigned char* out, size_t out_size);

char* aurora_hex_encode(const unsigned char* data, size_t len);

#ifdef __cplusplus
}
#endif
