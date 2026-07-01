#include "runtime/websocket.hpp"
#include "runtime/tls.hpp"
#include "runtime/memory.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

static int sha1(const uint8_t* data, size_t len, uint8_t out[20]) {
    BCRYPT_ALG_HANDLE hAlg;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA1_ALGORITHM, NULL, 0);
    if (!BCRYPT_SUCCESS(status)) return -1;
    status = BCryptHash(hAlg, NULL, 0, (PUCHAR)data, (ULONG)len, out, 20);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return BCRYPT_SUCCESS(status) ? 0 : -1;
}

#else
#include <dlfcn.h>

static void* libcrypto_handle = NULL;
static int (*crypto_sha1)(const uint8_t*, size_t, uint8_t*) = NULL;

static int sha1(const uint8_t* data, size_t len, uint8_t out[20]) {
    if (!libcrypto_handle) {
        libcrypto_handle = dlopen("libcrypto.so.3", RTLD_LAZY | RTLD_LOCAL);
        if (!libcrypto_handle)
            libcrypto_handle = dlopen("libcrypto.so.1.1", RTLD_LAZY | RTLD_LOCAL);
        if (!libcrypto_handle)
            libcrypto_handle = dlopen("libcrypto.so", RTLD_LAZY | RTLD_LOCAL);
        if (libcrypto_handle)
            crypto_sha1 = (int (*)(const uint8_t*, size_t, uint8_t*))
                dlsym(libcrypto_handle, "SHA1");
    }
    if (!crypto_sha1) return -1;
    crypto_sha1(data, len, out);
    return 0;
}
#endif

/* ── TLS-aware send helper ── */
static int ws_send(int64_t sock, int64_t tls_handle, const char* data, int len) {
    if (tls_handle > 0)
        return aurora_tls_write(tls_handle, data, len);
#ifdef _WIN32
    return (int)send((SOCKET)(intptr_t)sock, data, len, 0);
#else
    return (int)send((int)sock, data, (size_t)len, MSG_NOSIGNAL);
#endif
}

/* ── TLS-aware recv helper ── */
static int ws_recv(int64_t sock, int64_t tls_handle, char* buf, int size) {
    if (tls_handle > 0)
        return aurora_tls_read(tls_handle, buf, size);
#ifdef _WIN32
    return recv((SOCKET)(intptr_t)sock, buf, size, 0);
#else
    return (int)recv((int)sock, buf, (size_t)size, 0);
#endif
}

/* ── Base64 encoding ── */
static void base64_encode(const uint8_t* in, int in_len, char* out, int out_size) {
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i, j = 0;
    for (i = 0; i < in_len; i += 3) {
        int remaining = in_len - i;
        unsigned int byte0 = in[i];
        unsigned int byte1 = (remaining > 1) ? in[i + 1] : 0;
        unsigned int byte2 = (remaining > 2) ? in[i + 2] : 0;
        unsigned int triple = (byte0 << 16) | (byte1 << 8) | byte2;
        if (j < out_size) out[j++] = b64[(triple >> 18) & 0x3F];
        if (j < out_size) out[j++] = b64[(triple >> 12) & 0x3F];
        if (remaining > 1 && j < out_size) out[j++] = b64[(triple >> 6) & 0x3F];
        else if (j < out_size) out[j++] = '=';
        if (remaining > 2 && j < out_size) out[j++] = b64[triple & 0x3F];
        else if (j < out_size) out[j++] = '=';
    }
    if (j < out_size) out[j] = '\0';
}

static const char* WS_MAGIC = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

int aurora_ws_is_upgrade(const char* raw_request) {
    if (!raw_request) return 0;
    const char* upgrade = strstr(raw_request, "Upgrade:");
    if (!upgrade) upgrade = strstr(raw_request, "upgrade:");
    if (!upgrade) return 0;
    return (strstr(upgrade, "websocket") != NULL) ? 1 : 0;
}

const char* aurora_ws_get_key(const char* raw_request) {
    if (!raw_request) return NULL;
    const char* key_hdr = strstr(raw_request, "Sec-WebSocket-Key:");
    if (!key_hdr) key_hdr = strstr(raw_request, "sec-websocket-key:");
    if (!key_hdr) return NULL;
    key_hdr += 18;
    while (*key_hdr == ' ') key_hdr++;
    const char* end = strstr(key_hdr, "\r\n");
    if (!end) return NULL;
    return key_hdr;
}

int aurora_ws_upgrade(const char* key, int64_t sock, int64_t tls_handle) {
    if (!key) return 0;
    size_t key_len = strcspn(key, "\r\n");
    char combined[256];
    int combined_len = 0;
    for (int i = 0; i < (int)key_len && i < 255; i++)
        combined[combined_len++] = key[i];
    for (const char* m = WS_MAGIC; *m && combined_len < 255; combined_len++)
        combined[combined_len] = *m++;

    uint8_t hash[20];
    if (sha1((const uint8_t*)combined, (size_t)combined_len, hash) != 0)
        return 0;

    char accept_b64[64];
    base64_encode(hash, 20, accept_b64, (int)sizeof(accept_b64));

    char response[1024];
    int rlen = snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n", accept_b64);

    const char* p = response;
    int remaining = rlen;
    while (remaining > 0) {
        int n = ws_send(sock, tls_handle, p, remaining);
        if (n <= 0) return 0;
        p += n;
        remaining -= n;
    }
    return 1;
}

int aurora_ws_read_frame(int64_t sock, int64_t tls_handle, uint8_t** out_payload, int* opcode) {
    if (!out_payload || !opcode) return -1;
    *out_payload = NULL;
    *opcode = 0;

    uint8_t header[14];
    int n = ws_recv(sock, tls_handle, (char*)header, 2);
    if (n != 2) return -1;

    int opcode_val = header[0] & 0x0F;
    int masked = (header[1] >> 7) & 1;
    int64_t payload_len = header[1] & 0x7F;

    if (payload_len == 126) {
        uint8_t ext[2];
        n = ws_recv(sock, tls_handle, (char*)ext, 2);
        if (n != 2) return -1;
        payload_len = ((int64_t)ext[0] << 8) | (int64_t)ext[1];
    } else if (payload_len == 127) {
        uint8_t ext[8];
        n = ws_recv(sock, tls_handle, (char*)ext, 8);
        if (n != 8) return -1;
        payload_len = 0;
        for (int i = 0; i < 8; i++)
            payload_len = (payload_len << 8) | (int64_t)ext[i];
    }

    uint8_t mask[4] = {0};
    if (masked) {
        n = ws_recv(sock, tls_handle, (char*)mask, 4);
        if (n != 4) return -1;
    }

    if (payload_len > 10 * 1024 * 1024) return -1;
    uint8_t* payload = (uint8_t*)aurora_alloc((size_t)payload_len + 1);
    if (!payload) return -1;

    if (payload_len > 0) {
        int64_t total = 0;
        while (total < payload_len) {
            int r = ws_recv(sock, tls_handle, (char*)payload + total,
                            (int)(payload_len - total));
            if (r <= 0) { aurora_free(payload); return -1; }
            total += r;
        }
    }
    payload[payload_len] = '\0';

    if (masked) {
        for (int64_t i = 0; i < payload_len; i++)
            payload[i] ^= mask[i % 4];
    }

    *out_payload = payload;
    *opcode = opcode_val;
    return (int)payload_len;
}

int aurora_ws_write_frame(int64_t sock, int64_t tls_handle, int opcode, const uint8_t* data, int len) {
    uint8_t header[14];
    int hdr_len;

    header[0] = 0x80 | (opcode & 0x0F);
    if (len < 126) {
        header[1] = (uint8_t)len;
        hdr_len = 2;
    } else if (len < 65536) {
        header[1] = 126;
        header[2] = (uint8_t)((len >> 8) & 0xFF);
        header[3] = (uint8_t)(len & 0xFF);
        hdr_len = 4;
    } else {
        header[1] = 127;
        int64_t llen = (int64_t)len;
        for (int i = 7; i >= 0; i--) {
            header[2 + i] = (uint8_t)(llen & 0xFF);
            llen >>= 8;
        }
        hdr_len = 10;
    }

    {
        const char* p = (const char*)header;
        int remaining = hdr_len;
        while (remaining > 0) {
            int n = ws_send(sock, tls_handle, p, remaining);
            if (n <= 0) return -1;
            p += n;
            remaining -= n;
        }
    }

    if (len > 0) {
        const char* p = (const char*)data;
        int remaining = len;
        while (remaining > 0) {
            int n = ws_send(sock, tls_handle, p, remaining);
            if (n <= 0) return -1;
            p += n;
            remaining -= n;
        }
    }
    return len;
}

void aurora_ws_close(int64_t sock, int64_t tls_handle) {
    uint8_t close_payload[2] = {0x03, 0xE8};
    aurora_ws_write_frame(sock, tls_handle, WS_OPCODE_CLOSE, close_payload, 2);
}
