#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <cassert>
#include <atomic>
#include <mutex>

#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "bcrypt.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <dlfcn.h>
#endif

#include "runtime/backend.hpp"
#include "runtime/tls.hpp"
#include "runtime/websocket.hpp"

#define TEST_PORT 19876
#define TEST_MSG "Hello Aurora Server!"

static std::atomic<int> g_ws_echo_count{0};
static std::mutex g_print_mutex;

static void test_log(const char* msg) {
    std::lock_guard<std::mutex> lock(g_print_mutex);
    printf("  %s\n", msg);
    fflush(stdout);
}

#ifdef _WIN32
static void ensure_winsock() {
    static int init = 0;
    if (!init) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        init = 1;
    }
}

static int sha1_digest(const uint8_t* data, size_t len, uint8_t out[20]) {
    BCRYPT_ALG_HANDLE hAlg;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA1_ALGORITHM, NULL, 0);
    if (!BCRYPT_SUCCESS(status)) return -1;
    status = BCryptHash(hAlg, NULL, 0, (PUCHAR)data, (ULONG)len, out, 20);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return BCRYPT_SUCCESS(status) ? 0 : -1;
}
#else
static void* libcrypto_handle = NULL;
static int (*crypto_sha1)(const uint8_t*, size_t, uint8_t*) = NULL;

static int sha1_digest(const uint8_t* data, size_t len, uint8_t out[20]) {
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

static int tcp_send(int sock, const char* data, int len) {
#ifdef _WIN32
    return (int)send((SOCKET)(intptr_t)sock, data, len, 0);
#else
    return (int)send(sock, data, (size_t)len, MSG_NOSIGNAL);
#endif
}

static int tcp_recv(int sock, char* buf, int size) {
#ifdef _WIN32
    return recv((SOCKET)(intptr_t)sock, buf, size, 0);
#else
    return (int)recv(sock, buf, (size_t)size, 0);
#endif
}

static void tcp_close(int sock) {
#ifdef _WIN32
    closesocket((SOCKET)(intptr_t)sock);
#else
    close(sock);
#endif
}

static int tcp_connect(const char* host, int port) {
#ifdef _WIN32
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) return -1;
#else
    int s = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;
#endif
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((short)port);
    addr.sin_addr.s_addr = inet_addr(host);
#ifdef _WIN32
    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        closesocket(s);
        return -1;
    }
#else
    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(s);
        return -1;
    }
#endif
    return (int)(intptr_t)s;
}

static void base64_encode_ws(const uint8_t* in, int in_len, char* out, int out_size) {
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

static int ws_client_handshake(int sock) {
    uint8_t key_bytes[16];
#ifdef _WIN32
    HCRYPTPROV hProv;
    if (CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        CryptGenRandom(hProv, 16, key_bytes);
        CryptReleaseContext(hProv, 0);
    } else {
        for (int i = 0; i < 16; i++)
            key_bytes[i] = (uint8_t)(rand() & 0xFF);
    }
#else
    for (int i = 0; i < 16; i++)
        key_bytes[i] = (uint8_t)(rand() & 0xFF);
#endif

    char key_b64[32];
    base64_encode_ws(key_bytes, 16, key_b64, (int)sizeof(key_b64));

    char request[1024];
    int rlen = snprintf(request, sizeof(request),
        "GET / HTTP/1.1\r\n"
        "Host: localhost:%d\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n", TEST_PORT, key_b64);

    const char* p = request;
    int remaining = rlen;
    while (remaining > 0) {
        int n = tcp_send(sock, p, remaining);
        if (n <= 0) return -1;
        p += n;
        remaining -= n;
    }

    char response[4096];
    int n = tcp_recv(sock, response, (int)sizeof(response) - 1);
    if (n <= 0) return -1;
    response[n] = '\0';

    if (!strstr(response, "101 Switching Protocols"))
        return -1;
    if (!strstr(response, "Sec-WebSocket-Accept:"))
        return -1;

    return 0;
}

static int ws_send_frame(int sock, int opcode, const uint8_t* data, int len) {
    uint8_t header[14];
    int hdr_len;
    uint8_t mask[4];

#ifdef _WIN32
    HCRYPTPROV hProv;
    if (CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        CryptGenRandom(hProv, 4, mask);
        CryptReleaseContext(hProv, 0);
    } else {
        for (int i = 0; i < 4; i++)
            mask[i] = (uint8_t)(rand() & 0xFF);
    }
#else
    for (int i = 0; i < 4; i++)
        mask[i] = (uint8_t)(rand() & 0xFF);
#endif

    header[0] = 0x80 | (opcode & 0x0F);
    header[1] = 0x80;
    if (len < 126) {
        header[1] |= (uint8_t)len;
        hdr_len = 2;
    } else if (len < 65536) {
        header[1] |= 126;
        header[2] = (uint8_t)((len >> 8) & 0xFF);
        header[3] = (uint8_t)(len & 0xFF);
        hdr_len = 4;
    } else {
        header[1] |= 127;
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
            int n = tcp_send(sock, p, remaining);
            if (n <= 0) return -1;
            p += n;
            remaining -= n;
        }
    }

    if (len > 0) {
        uint8_t* masked = (uint8_t*)malloc((size_t)len);
        if (!masked) return -1;
        for (int i = 0; i < len; i++)
            masked[i] = data[i] ^ mask[i % 4];
        {
            const char* p = (const char*)mask;
            int remaining = 4;
            while (remaining > 0) {
                int n = tcp_send(sock, p, remaining);
                if (n <= 0) { free(masked); return -1; }
                p += n;
                remaining -= n;
            }
        }
        {
            const char* p = (const char*)masked;
            int remaining = len;
            while (remaining > 0) {
                int n = tcp_send(sock, p, remaining);
                if (n <= 0) { free(masked); return -1; }
                p += n;
                remaining -= n;
            }
        }
        free(masked);
    }
    return len;
}

static int ws_recv_frame(int sock, uint8_t* buf, int buf_size, int* out_opcode) {
    uint8_t header[14];
    int n = tcp_recv(sock, (char*)header, 2);
    if (n != 2) return -1;

    int opcode = header[0] & 0x0F;
    int64_t payload_len = header[1] & 0x7F;

    if (payload_len == 126) {
        uint8_t ext[2];
        n = tcp_recv(sock, (char*)ext, 2);
        if (n != 2) return -1;
        payload_len = ((int64_t)ext[0] << 8) | (int64_t)ext[1];
    } else if (payload_len == 127) {
        uint8_t ext[8];
        n = tcp_recv(sock, (char*)ext, 8);
        if (n != 8) return -1;
        payload_len = 0;
        for (int i = 0; i < 8; i++)
            payload_len = (payload_len << 8) | (int64_t)ext[i];
    }

    if (payload_len > (int64_t)buf_size - 1)
        payload_len = (int64_t)buf_size - 1;

    int64_t total = 0;
    while (total < payload_len) {
        n = tcp_recv(sock, (char*)buf + total, (int)(payload_len - total));
        if (n <= 0) return -1;
        total += n;
    }
    buf[total] = '\0';
    *out_opcode = opcode;
    return (int)total;
}

/* ── Test 1: TLS context creation (self-signed cert on Windows) ── */
static void test_tls_context() {
    test_log("test_tls_context...");
#ifdef _WIN32
    /* On Windows, Schannel + self-signed cert should work */
    AuroraTLSContext* ctx = aurora_tls_server_ctx_new(nullptr, nullptr);
    assert(ctx != nullptr && "TLS context should be created with self-signed cert");
    aurora_tls_ctx_free(ctx);
    test_log("  PASS test_tls_context (self-signed cert created and freed)");
#else
    /* On POSIX, OpenSSL may not be available — skip is acceptable */
    test_log("  SKIP test_tls_context (requires OpenSSL on POSIX)");
#endif
}

/* ── Test 2: HTTP basic request/response ── */
extern "C" void http_test_handler(AuroraHttpRequest* req, AuroraHttpResponse* res) {
    (void)req;
    aurora_http_response_set_body(res, TEST_MSG);
    aurora_http_response_set_content_type(res, "text/plain");
}

static void test_http_basic() {
    test_log("test_http_basic...");
    AuroraServer* srv = aurora_server_init(TEST_PORT);
    assert(srv != nullptr);

    AuroraRouter* router = aurora_router_new();
    assert(router != nullptr);
    aurora_route_add(router, "GET", "/test", (void*)http_test_handler);

    aurora_server_start(srv);
    assert(srv->running);

    std::thread server_thread([srv, router]() {
        aurora_server_accept_and_handle(srv, router);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    int sock = tcp_connect("127.0.0.1", TEST_PORT);
    assert(sock >= 0);

    const char* req = "GET /test HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
    tcp_send(sock, req, (int)strlen(req));

    char resp[4096];
    int n = tcp_recv(sock, resp, (int)sizeof(resp) - 1);
    assert(n > 0);
    resp[n] = '\0';
    tcp_close(sock);

    assert(strstr(resp, "200 OK") != nullptr);
    assert(strstr(resp, TEST_MSG) != nullptr);

    aurora_server_stop(srv);
    server_thread.join();

    test_log("  PASS test_http_basic");
}

/* ── Test 3: WebSocket handshake and frame echo ── */
extern "C" void ws_test_handler(AuroraHttpRequest* req, AuroraHttpResponse* res) {
    (void)req;
    aurora_http_response_set_body(res, "WebSocket endpoint");
}

static void test_websocket_echo() {
    test_log("test_websocket_echo...");
    AuroraServer* srv = aurora_server_init(TEST_PORT + 1);
    assert(srv != nullptr);

    AuroraRouter* router = aurora_router_new();
    assert(router != nullptr);
    aurora_route_add(router, "GET", "/ws", (void*)ws_test_handler);

    aurora_server_start(srv);
    assert(srv->running);

    std::thread server_thread([srv, router]() {
        /* Single accept — the WS handler in handle_client() echoes frames */
        aurora_server_accept_and_handle(srv, router);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    int sock = tcp_connect("127.0.0.1", TEST_PORT + 1);
    assert(sock >= 0);

    int ret = ws_client_handshake(sock);
    assert(ret == 0 && "WebSocket handshake should succeed");

    const char* test_payload = "Hello WebSocket!";
    int payload_len = (int)strlen(test_payload);

    ret = ws_send_frame(sock, 0x1, (const uint8_t*)test_payload, payload_len);
    assert(ret == payload_len);

    uint8_t echo_buf[256];
    int echo_opcode = 0;
    int echo_len = ws_recv_frame(sock, echo_buf, (int)sizeof(echo_buf), &echo_opcode);
    assert(echo_len == payload_len);
    assert(echo_opcode == 0x1);
    assert(memcmp(echo_buf, test_payload, (size_t)payload_len) == 0);

    uint8_t close_frame[2] = {0x03, 0xE8};
    ws_send_frame(sock, 0x8, close_frame, 2);
    tcp_close(sock);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    aurora_server_stop(srv);
    server_thread.join();

    test_log("  PASS test_websocket_echo");
}

/* ── Test 4: Concurrent HTTP connections (10+ clients) ── */
extern "C" void concurrent_test_handler(AuroraHttpRequest* req, AuroraHttpResponse* res) {
    (void)req;
    aurora_http_response_set_body(res, "concurrent-ok");
}

static void run_concurrent_client(int client_id, int port, std::atomic<int>* success_count) {
    int sock = tcp_connect("127.0.0.1", port);
    if (sock < 0) {
        printf("  [client %d] connect failed\n", client_id);
        return;
    }

    char req[256];
    int rlen = snprintf(req, sizeof(req),
        "GET /concurrent HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n"
        "X-Client-Id: %d\r\n"
        "\r\n", client_id);

    tcp_send(sock, req, rlen);

    char resp[4096];
    char *p = resp;
    int remaining = (int)sizeof(resp) - 1;
    int n, total = 0, header_end = -1, content_length = -1;
    while (remaining > 0) {
        n = tcp_recv(sock, p, remaining);
        if (n <= 0) break;
        p += n;
        total += n;
        remaining -= n;
        /* Check for complete body after Content-Length */
        if (header_end < 0) {
            char* hdr_end = strstr(resp, "\r\n\r\n");
            if (hdr_end) {
                header_end = (int)(hdr_end - resp) + 4;
                char* cl = strstr(resp, "Content-Length:");
                if (!cl) cl = strstr(resp, "content-length:");
                if (cl) content_length = atoi(cl + 15);
            }
        }
        if (header_end >= 0 && content_length >= 0 &&
            total >= header_end + content_length)
            break;
    }
    tcp_close(sock);

    if (total > 0) {
        resp[total] = '\0';
        if (strstr(resp, "200 OK") && strstr(resp, "concurrent-ok"))
            (*success_count)++;
        else
            printf("  [client %d] unexpected response (total=%d cl=%d): %.*s\n",
                   client_id, total, content_length, total > 120 ? 120 : total, resp);
    } else {
        printf("  [client %d] recv failed (total=%d)\n", client_id, total);
    }
}

static void test_concurrent_connections() {
    test_log("test_concurrent_connections (10 clients)...");
    int port = TEST_PORT + 2;

    AuroraServer* srv = aurora_server_init(port);
    assert(srv != nullptr);

    AuroraRouter* router = aurora_router_new();
    assert(router != nullptr);
    aurora_route_add(router, "GET", "/concurrent", (void*)concurrent_test_handler);

    aurora_server_start(srv);
    assert(srv->running);

    std::thread server_thread([srv, router]() {
        aurora_server_accept_loop(srv, router);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    const int NUM_CLIENTS = 10;
    std::atomic<int> success_count{0};
    std::vector<std::thread> clients;

    for (int i = 0; i < NUM_CLIENTS; i++) {
        clients.emplace_back(run_concurrent_client, i, port, &success_count);
    }

    for (auto& t : clients) {
        t.join();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    aurora_server_stop(srv);
    server_thread.join();

    char buf[128];
    snprintf(buf, sizeof(buf), "  PASS test_concurrent_connections (%d/%d succeeded)",
             success_count.load(), NUM_CLIENTS);
    test_log(buf);
    assert(success_count.load() == NUM_CLIENTS);
}

/* ── Test 5: Server static file serving ── */
static void test_static_file() {
    test_log("test_static_file...");
    AuroraServer* srv = aurora_server_init(TEST_PORT + 3);
    assert(srv != nullptr);

    aurora_server_static(srv, "/", ".");
    aurora_server_start(srv);
    assert(srv->running);

    std::thread server_thread([srv]() {
        AuroraRouter* r = aurora_router_new();
        aurora_server_accept_and_handle(srv, r);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    int sock = tcp_connect("127.0.0.1", TEST_PORT + 3);
    assert(sock >= 0);

    /* Request CMakeLists.txt (exists at root) */
    const char* req = "GET /CMakeLists.txt HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
    tcp_send(sock, req, (int)strlen(req));

    char resp[8192];
    int n = tcp_recv(sock, resp, (int)sizeof(resp) - 1);
    assert(n > 0);
    resp[n] = '\0';
    tcp_close(sock);

    assert(strstr(resp, "200 OK") != nullptr);
    assert(strstr(resp, "cmake_minimum_required") != nullptr);
    assert(strstr(resp, "Content-Type:") != nullptr);

    aurora_server_stop(srv);
    server_thread.join();

    test_log("  PASS test_static_file");
}

/* ── Test 6: HTTP 404 fallback ── */
extern "C" void fallback_handler(AuroraHttpRequest* req, AuroraHttpResponse* res) {
    (void)req;
    aurora_http_response_set_body(res, "exact-match");
}

static void test_http_404() {
    test_log("test_http_404...");
    int port = TEST_PORT + 4;

    AuroraServer* srv = aurora_server_init(port);
    assert(srv != nullptr);

    AuroraRouter* router = aurora_router_new();
    assert(router != nullptr);
    aurora_route_add(router, "GET", "/exists", (void*)fallback_handler);
    aurora_server_start(srv);
    assert(srv->running);

    std::thread server_thread([srv, router]() {
        aurora_server_accept_and_handle(srv, router);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    int sock = tcp_connect("127.0.0.1", port);
    assert(sock >= 0);

    const char* req = "GET /nonexistent HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
    tcp_send(sock, req, (int)strlen(req));

    char resp[4096];
    int n = tcp_recv(sock, resp, (int)sizeof(resp) - 1);
    assert(n > 0);
    resp[n] = '\0';
    tcp_close(sock);

    assert(strstr(resp, "404 Not Found") != nullptr);

    aurora_server_stop(srv);
    server_thread.join();

    test_log("  PASS test_http_404");
}

int main() {
#ifdef _WIN32
    ensure_winsock();
#endif

    printf("=== Server Integration Tests ===\n");

    test_tls_context();
    test_http_basic();
    test_websocket_echo();
    test_concurrent_connections();
    test_static_file();
    test_http_404();

    printf("=== ALL SERVER TESTS PASSED ===\n");
    return 0;
}
