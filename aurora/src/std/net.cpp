#include "std/net.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <atomic>
#include <vector>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winhttp.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#endif

/* ── Thread-safe DNS resolver + TCP connect ── */
static int resolve_and_connect(const char* host, int port) {
    struct addrinfo hints, *res, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);
    int err = getaddrinfo(host, port_str, &hints, &res);
    if (err || !res) return -1;
    int sock = -1;
    for (rp = res; rp; rp = rp->ai_next) {
#ifdef _WIN32
        sock = (int)socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock == INVALID_SOCKET) continue;
#else
        sock = (int)socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock < 0) continue;
#endif
        if (connect(sock, rp->ai_addr, (int)rp->ai_addrlen) == 0) break;
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        sock = -1;
    }
    freeaddrinfo(res);
    return sock;
}

static int udp_resolve(const char* host, struct sockaddr_storage* addr, int* addrlen) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    char port_str[16] = "0";
    int err = getaddrinfo(host, port_str, &hints, &res);
    if (err || !res) return -1;
    memcpy(addr, res->ai_addr, res->ai_addrlen);
    *addrlen = (int)res->ai_addrlen;
    freeaddrinfo(res);
    return 0;
}

int aurora_send_all(int64_t sock, const char* data, int len) {
    int total = 0;
    while (total < len) {
#ifdef _WIN32
        int n = (int)send((SOCKET)(intptr_t)sock, data + total, len - total, 0);
#else
        int n = (int)send((int)sock, data + total, (size_t)(len - total), MSG_NOSIGNAL);
#endif
        if (n <= 0) return total > 0 ? total : -1;
        total += n;
    }
    return total;
}

extern "C" {

static int parse_url(const char* url, char* host, int host_size,
                     int* port, char* path, int path_size);

#ifdef _WIN32
static int winhttp_request(const char* url, LPCWSTR method,
                           const char* headers,
                           const char* body, const char* content_type,
                           char* buffer, int buffer_size);
#endif

#ifdef _WIN32
static std::atomic<bool> net_initialized{false};
static void ensure_winsock() {
    if (!net_initialized.load()) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        net_initialized.store(true);
    }
}
#endif

/* ── HTTP method dispatch helper ── */
static int http_method_request(const char* url, const char* method_str,
                                const char* headers, const char* body,
                                const char* content_type,
                                char* buffer, int buffer_size) {
    if (!url || !buffer || buffer_size <= 0) return -1;
#ifdef _WIN32
    if (strncmp(url, "https://", 8) == 0) {
        wchar_t wmethod[16];
        MultiByteToWideChar(CP_UTF8, 0, method_str, -1, wmethod, 16);
        return winhttp_request(url, wmethod, headers, body, content_type, buffer, buffer_size);
    }
    ensure_winsock();
#endif
    char host[256] = {0};
    char path[1024] = {0};
    int port = 80;
    int is_https = parse_url(url, host, sizeof(host), &port, path, sizeof(path));
    if (is_https) return -1;

    int sock = resolve_and_connect(host, port);
    if (sock < 0) return -1;

    std::string request;
    if (body && strlen(body) > 0) {
        const char* ct = content_type ? content_type : "application/x-www-form-urlencoded";
        request = std::string(method_str) + " " + path + " HTTP/1.0\r\nHost: " + host + "\r\nContent-Type: " + ct + "\r\nContent-Length: " + std::to_string(strlen(body)) + "\r\nConnection: close\r\n";
        if (headers) request += headers;
        request += "\r\n" + std::string(body);
    } else {
        request = std::string(method_str) + " " + path + " HTTP/1.0\r\nHost: " + host + "\r\nConnection: close\r\n";
        if (headers) request += headers;
        request += "\r\n";
    }

    aurora_send_all((int64_t)(intptr_t)sock, request.data(), (int)request.size());

    int total = 0;
    int n;
    while ((n = (int)recv(sock, buffer + total, buffer_size - total - 1, 0)) > 0) {
        total += n;
        if (total >= buffer_size - 1) break;
    }
    buffer[total] = '\0';

    if (strcmp(method_str, "HEAD") == 0) {
        /* Return headers for HEAD */
        char* crlf = strstr(buffer, "\r\n\r\n");
        if (crlf) { *crlf = '\0'; total = (int)(crlf - buffer); }
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        return total;
    }

    char* body_start = strstr(buffer, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        int body_len = (int)(buffer + total - body_start);
        memmove(buffer, body_start, body_len);
        buffer[body_len] = '\0';
        total = body_len;
    }

#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
    return total;
}

/* ── HTTP GET ── */
int aurora_net_http_get(const char* url, char* buffer, int buffer_size) {
    return http_method_request(url, "GET", NULL, NULL, NULL, buffer, buffer_size);
}

/* ── HTTP POST ── */
int aurora_net_http_post(const char* url, const char* body, const char* content_type,
                          char* buffer, int buffer_size) {
    return http_method_request(url, "POST", NULL, body, content_type, buffer, buffer_size);
}

/* ── HTTP PUT ── */
int aurora_net_http_put(const char* url, const char* body, const char* content_type,
                         char* buffer, int buffer_size) {
    return http_method_request(url, "PUT", NULL, body, content_type, buffer, buffer_size);
}

/* ── HTTP DELETE ── */
int aurora_net_http_delete(const char* url, char* buffer, int buffer_size) {
    return http_method_request(url, "DELETE", NULL, NULL, NULL, buffer, buffer_size);
}

/* ── HTTP PATCH ── */
int aurora_net_http_patch(const char* url, const char* body, const char* content_type,
                           char* buffer, int buffer_size) {
    return http_method_request(url, "PATCH", NULL, body, content_type, buffer, buffer_size);
}

/* ── HTTP HEAD ── */
int aurora_net_http_head(const char* url, char* buffer, int buffer_size) {
    return http_method_request(url, "HEAD", NULL, NULL, NULL, buffer, buffer_size);
}

/* ── HTTP GET with custom headers ── */
int aurora_net_http_get_ex(const char* url, const char* headers,
                            char* buffer, int buffer_size) {
    return http_method_request(url, "GET", headers, NULL, NULL, buffer, buffer_size);
}

/* ── HTTP POST with custom headers ── */
int aurora_net_http_post_ex(const char* url, const char* headers,
                             const char* body, const char* content_type,
                             char* buffer, int buffer_size) {
    return http_method_request(url, "POST", headers, body, content_type, buffer, buffer_size);
}

/* ── HTTP status code extraction ── */
int aurora_net_http_status(const char* response) {
    if (!response) return -1;
    int code = 0;
    if (sscanf(response, "HTTP/%*d.%*d %d", &code) == 1) return code;
    return -1;
}

/* ── URL encode ── */
int aurora_net_url_encode(const char* input, char* out, int out_size) {
    if (!input || !out || out_size <= 0) return -1;
    const char* hex = "0123456789ABCDEF";
    int pos = 0;
    for (const char* p = input; *p && pos < out_size - 1; p++) {
        unsigned char c = (unsigned char)*p;
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out[pos++] = c;
        } else if (c == ' ') {
            out[pos++] = '+';
        } else {
            if (pos + 3 > out_size - 1) break;
            out[pos++] = '%';
            out[pos++] = hex[c >> 4];
            out[pos++] = hex[c & 0xf];
        }
    }
    out[pos] = '\0';
    return pos;
}

/* ── URL decode ── */
int aurora_net_url_decode(const char* input, char* out, int out_size) {
    if (!input || !out || out_size <= 0) return -1;
    int pos = 0;
    for (const char* p = input; *p && pos < out_size - 1; p++) {
        if (*p == '%' && *(p+1) && *(p+2)) {
            char hex[3] = { p[1], p[2], '\0' };
            out[pos++] = (char)strtol(hex, NULL, 16);
            p += 2;
        } else if (*p == '+') {
            out[pos++] = ' ';
        } else {
            out[pos++] = *p;
        }
    }
    out[pos] = '\0';
    return pos;
}

/* ── DNS lookup ── */
int aurora_net_dns_lookup(const char* hostname, char* buffer, int buffer_size) {
#ifdef _WIN32
    ensure_winsock();
#endif
    struct addrinfo hints, *res, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int err = getaddrinfo(hostname, nullptr, &hints, &res);
    if (err || !res) return -1;
    for (rp = res; rp; rp = rp->ai_next) {
        if (rp->ai_family == AF_INET) {
            void* addr_ptr = &((struct sockaddr_in*)rp->ai_addr)->sin_addr;
            if (inet_ntop(AF_INET, addr_ptr, buffer, (socklen_t)buffer_size)) {
                freeaddrinfo(res);
                return (int)strlen(buffer);
            }
        } else if (rp->ai_family == AF_INET6) {
            void* addr_ptr = &((struct sockaddr_in6*)rp->ai_addr)->sin6_addr;
            if (inet_ntop(AF_INET6, addr_ptr, buffer, (socklen_t)buffer_size)) {
                freeaddrinfo(res);
                return (int)strlen(buffer);
            }
        }
    }
    freeaddrinfo(res);
    return -1;
}

/* ── TCP Socket ── */
int64_t aurora_net_tcp_connect(const char* host, int port) {
#ifdef _WIN32
    ensure_winsock();
#endif
    int sock = resolve_and_connect(host, port);
    if (sock < 0) return -1;
#ifdef _WIN32
    return (int64_t)(intptr_t)(SOCKET)sock;
#else
    return (int64_t)(intptr_t)sock;
#endif
}

int aurora_net_tcp_send(int64_t sock, const char* data, int len) {
#ifdef _WIN32
    return send((SOCKET)(intptr_t)sock, data, len, 0);
#else
    return (int)send((int)sock, data, (size_t)len, MSG_NOSIGNAL);
#endif
}

int aurora_net_tcp_recv(int64_t sock, char* buffer, int buffer_size) {
#ifdef _WIN32
    return recv((SOCKET)(intptr_t)sock, buffer, buffer_size, 0);
#else
    return (int)recv((int)sock, buffer, (size_t)buffer_size, 0);
#endif
}

void aurora_net_tcp_close(int64_t sock) {
#ifdef _WIN32
    closesocket((SOCKET)(intptr_t)sock);
#else
    close((int)sock);
#endif
}

/* ── UDP Socket ── */
int64_t aurora_net_udp_socket(void) {
#ifdef _WIN32
    ensure_winsock();
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == INVALID_SOCKET) return -1;
    return (int64_t)(intptr_t)s;
#else
    int s = (int)socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return -1;
    return (int64_t)(intptr_t)s;
#endif
}

int aurora_net_udp_sendto(int64_t sock, const char* data, int len,
                           const char* host, int port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%d", port);
        if (getaddrinfo(host, port_str, &hints, &res) != 0 || !res) return -1;
        memcpy(&addr, res->ai_addr, sizeof(addr));
        freeaddrinfo(res);
    }
#ifdef _WIN32
    return (int)sendto((SOCKET)(intptr_t)sock, data, len, 0,
                        (struct sockaddr*)&addr, sizeof(addr));
#else
    return (int)sendto((int)sock, data, (size_t)len, MSG_NOSIGNAL,
                        (struct sockaddr*)&addr, sizeof(addr));
#endif
}

int aurora_net_udp_recvfrom(int64_t sock, char* buffer, int buffer_size,
                             char* src_host, int src_host_size, int* src_port) {
    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);
#ifdef _WIN32
    int n = recvfrom((SOCKET)(intptr_t)sock, buffer, buffer_size, 0,
                      (struct sockaddr*)&addr, &addrlen);
#else
    ssize_t n = recvfrom((int)sock, buffer, (size_t)buffer_size, 0,
                          (struct sockaddr*)&addr, &addrlen);
#endif
    if (n <= 0) return (int)n;

    if (src_host && src_host_size > 0) {
        if (addr.ss_family == AF_INET) {
            inet_ntop(AF_INET, &((struct sockaddr_in*)&addr)->sin_addr,
                      src_host, (socklen_t)src_host_size);
            if (src_port) *src_port = ntohs(((struct sockaddr_in*)&addr)->sin_port);
        } else if (addr.ss_family == AF_INET6) {
            inet_ntop(AF_INET6, &((struct sockaddr_in6*)&addr)->sin6_addr,
                      src_host, (socklen_t)src_host_size);
            if (src_port) *src_port = ntohs(((struct sockaddr_in6*)&addr)->sin6_port);
        }
    }
    return (int)n;
}

void aurora_net_udp_close(int64_t sock) {
#ifdef _WIN32
    closesocket((SOCKET)(intptr_t)sock);
#else
    close((int)sock);
#endif
}

/* ── WebSocket Client ── */
int64_t aurora_net_ws_connect(const char* url, char* response, int response_size) {
    if (!url) return -1;
#ifdef _WIN32
    ensure_winsock();
#endif
    char host[256] = {0};
    char path[1024] = {0};
    int port = 80;
    int is_https = 0;

    if (strncmp(url, "wss://", 6) == 0) {
        is_https = 1;
        const char* p = url + 6;
        const char* slash = strchr(p, '/');
        const char* colon = strchr(p, ':');
        if (slash && colon && colon < slash) {
            size_t hlen = colon - p;
            if (hlen < sizeof(host)) { memcpy(host, p, hlen); host[hlen] = '\0'; }
            port = atoi(colon + 1);
            strncpy(path, slash, sizeof(path)-1);
        } else if (slash) {
            size_t hlen = slash - p;
            if (hlen < sizeof(host)) { memcpy(host, p, hlen); host[hlen] = '\0'; }
            port = 443;
            strncpy(path, slash, sizeof(path)-1);
        } else {
            strncpy(host, p, sizeof(host)-1);
            port = 443;
            strcpy(path, "/");
        }
    } else if (strncmp(url, "ws://", 5) == 0) {
        const char* p = url + 5;
        const char* slash = strchr(p, '/');
        const char* colon = strchr(p, ':');
        if (slash && colon && colon < slash) {
            size_t hlen = colon - p;
            if (hlen < sizeof(host)) { memcpy(host, p, hlen); host[hlen] = '\0'; }
            port = atoi(colon + 1);
            strncpy(path, slash, sizeof(path)-1);
        } else if (slash) {
            size_t hlen = slash - p;
            if (hlen < sizeof(host)) { memcpy(host, p, hlen); host[hlen] = '\0'; }
            port = 80;
            strncpy(path, slash, sizeof(path)-1);
        } else {
            strncpy(host, p, sizeof(host)-1);
            port = 80;
            strcpy(path, "/");
        }
    } else {
        return -1;
    }

#ifdef _WIN32
    if (is_https) {
        /* Use WinHTTP for WSS: we open a TCP tunnel via CONNECT or use direct schannel
           For MVP, fall back to WinHTTP WebSocket */
        HINTERNET hSession = WinHttpOpen(L"Aurora-WS/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
        if (!hSession) return -1;
        wchar_t whost[256];
        MultiByteToWideChar(CP_UTF8, 0, host, -1, whost, 256);
        HINTERNET hConnect = WinHttpConnect(hSession, whost, (INTERNET_PORT)port, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession); return -1; }
        wchar_t wpath[1024];
        MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, 1024);
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wpath, NULL,
            NULL, NULL, WINHTTP_FLAG_SECURE);
        if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return -1; }

        char ws_key[20];
        for (int i = 0; i < 16; i++) ws_key[i] = (char)(rand() % 256);
        char ws_key_b64[32];
        aurora_net_url_encode(ws_key, ws_key_b64, 32); /* Simplified */

        wchar_t wheaders[1024];
        char extra_headers[512];
        snprintf(extra_headers, sizeof(extra_headers),
            "Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n",
            ws_key_b64);
        MultiByteToWideChar(CP_UTF8, 0, extra_headers, -1, wheaders, 1024);

        if (!WinHttpSendRequest(hRequest, wheaders, (DWORD)wcslen(wheaders),
                                NULL, 0, 0, 0)) {
            WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession); return -1;
        }
        if (!WinHttpReceiveResponse(hRequest, NULL)) {
            WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession); return -1;
        }
        if (response && response_size > 0) {
            DWORD read = 0;
            WinHttpReadData(hRequest, response, (DWORD)(response_size - 1), &read);
            if (read > 0) response[read] = '\0';
        }
        /* For MVP: return hRequest as ws handle; actual full WS frame support
           would need raw socket access. We store it and the session/connect handles. */
        /* Return a pseudo-handle; real WS over WinHTTP requires WinHttpWebSocket */
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return -1; /* WSS not fully supported in MVP */
    }
#endif

    int sock = resolve_and_connect(host, port);
    if (sock < 0) return -1;

    char ws_key[20];
    for (int i = 0; i < 16; i++) ws_key[i] = (char)(32 + (rand() % 95));
    ws_key[16] = '\0';

    char request[4096];
    int req_len = snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n",
        path, host, ws_key);

    aurora_send_all((int64_t)(intptr_t)sock, request, req_len);

    int total = 0;
    int n;
    while ((n = (int)recv(sock, response + total, response_size - total - 1, 0)) > 0) {
        total += n;
        if (strstr(response, "\r\n\r\n")) break;
        if (total >= response_size - 1) break;
    }
    if (response) response[total] = '\0';

    if (!strstr(response, "101") || !strstr(response, "101 Switching Protocols")) {
        closesocket(sock);
        return -1;
    }

#ifdef _WIN32
    return (int64_t)(intptr_t)(SOCKET)sock;
#else
    return (int64_t)(intptr_t)sock;
#endif
}

int aurora_net_ws_send(int64_t ws, const char* data, int len, int is_text) {
    /* Build WebSocket frame */
    std::vector<unsigned char> frame;
    frame.push_back(is_text ? 0x81 : 0x82); /* FIN + opcode */
    if (len < 126) {
        frame.push_back((unsigned char)len | 0x80); /* masked */
    } else if (len < 65536) {
        frame.push_back(126 | 0x80);
        frame.push_back((unsigned char)(len >> 8));
        frame.push_back((unsigned char)(len & 0xff));
    } else {
        frame.push_back(127 | 0x80);
        for (int i = 7; i >= 0; i--) frame.push_back((unsigned char)((len >> (i * 8)) & 0xff));
    }
    unsigned char mask[4];
    mask[0] = (unsigned char)(rand() % 256);
    mask[1] = (unsigned char)(rand() % 256);
    mask[2] = (unsigned char)(rand() % 256);
    mask[3] = (unsigned char)(rand() % 256);
    for (int i = 0; i < 4; i++) frame.push_back(mask[i]);
    for (int i = 0; i < len; i++) frame.push_back((unsigned char)(data[i]) ^ mask[i % 4]);

#ifdef _WIN32
    return (int)send((SOCKET)(intptr_t)ws, (const char*)frame.data(), (int)frame.size(), 0);
#else
    return (int)send((int)ws, frame.data(), frame.size(), MSG_NOSIGNAL);
#endif
}

int aurora_net_ws_recv(int64_t ws, char* buffer, int buffer_size, int* is_text) {
    unsigned char header[2];
#ifdef _WIN32
    if (recv((SOCKET)(intptr_t)ws, (char*)header, 2, 0) != 2) return -1;
#else
    if (recv((int)ws, (char*)header, 2, 0) != 2) return -1;
#endif
    int opcode = header[0] & 0x0f;
    int masked = (header[1] & 0x80) ? 1 : 0;
    int64_t payload_len = header[1] & 0x7f;
    if (payload_len == 126) {
        unsigned char ext[2];
#ifdef _WIN32
        if (recv((SOCKET)(intptr_t)ws, (char*)ext, 2, 0) != 2) return -1;
#else
        if (recv((int)ws, (char*)ext, 2, 0) != 2) return -1;
#endif
        payload_len = ((int64_t)ext[0] << 8) | ext[1];
    } else if (payload_len == 127) {
        unsigned char ext[8];
#ifdef _WIN32
        if (recv((SOCKET)(intptr_t)ws, (char*)ext, 8, 0) != 8) return -1;
#else
        if (recv((int)ws, (char*)ext, 8, 0) != 8) return -1;
#endif
        payload_len = 0;
        for (int i = 0; i < 8; i++) payload_len = (payload_len << 8) | ext[i];
    }
    unsigned char mask_key[4] = {0,0,0,0};
    if (masked) {
#ifdef _WIN32
        if (recv((SOCKET)(intptr_t)ws, (char*)mask_key, 4, 0) != 4) return -1;
#else
        if (recv((int)ws, (char*)mask_key, 4, 0) != 4) return -1;
#endif
    }
    if (opcode == 0x08) { /* Close frame */
        unsigned char close_frame[2] = {0x88, 0x00};
#ifdef _WIN32
        send((SOCKET)(intptr_t)ws, (const char*)close_frame, 2, 0);
#else
        send((int)ws, (const char*)close_frame, 2, MSG_NOSIGNAL);
#endif
        return -1;
    }
    if (opcode == 0x09) { /* Ping */
        unsigned char pong[2] = {0x8A, 0x00};
#ifdef _WIN32
        send((SOCKET)(intptr_t)ws, (const char*)pong, 2, 0);
#else
        send((int)ws, (const char*)pong, 2, MSG_NOSIGNAL);
#endif
        return 0;
    }
    int to_read = (int)(payload_len < buffer_size ? payload_len : buffer_size);
    int received = 0;
    while (received < to_read) {
#ifdef _WIN32
        int n = (int)recv((SOCKET)(intptr_t)ws, buffer + received, to_read - received, 0);
#else
        int n = (int)recv((int)ws, buffer + received, (size_t)(to_read - received), 0);
#endif
        if (n <= 0) break;
        received += n;
    }
    if (masked) {
        for (int i = 0; i < received; i++)
            buffer[i] = (char)((unsigned char)buffer[i] ^ mask_key[i % 4]);
    }
    if (is_text) *is_text = (opcode == 0x01) ? 1 : 0;
    buffer[received] = '\0';
    return received;
}

void aurora_net_ws_close(int64_t ws) {
    unsigned char close_frame[2] = {0x88, 0x00};
#ifdef _WIN32
    send((SOCKET)(intptr_t)ws, (const char*)close_frame, 2, 0);
    closesocket((SOCKET)(intptr_t)ws);
#else
    send((int)ws, (const char*)close_frame, 2, MSG_NOSIGNAL);
    close((int)ws);
#endif
}

/* ── Multipart/form-data builder ── */
char* aurora_net_multipart_begin(const char* boundary) {
    return strdup(""); /* Start with empty string */
}

char* aurora_net_multipart_add_field(char* form, const char* boundary,
                                      const char* name, const char* value) {
    if (!form || !boundary || !name || !value) return form;
    std::string part = std::string("--") + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"" + name + "\"\r\n\r\n"
        + value + "\r\n";
    size_t old_len = strlen(form);
    size_t new_len = old_len + part.size() + 8;
    char* new_form = (char*)realloc(form, new_len);
    if (!new_form) return form;
    memcpy(new_form + old_len, part.data(), part.size());
    new_form[old_len + part.size()] = '\0';
    return new_form;
}

char* aurora_net_multipart_add_file(char* form, const char* boundary,
                                     const char* name, const char* filename,
                                     const char* data, int data_len) {
    if (!form || !boundary || !name || !filename || !data) return form;
    std::string part = std::string("--") + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"" + name + "\"; filename=\"" + filename + "\"\r\n"
        "Content-Type: application/octet-stream\r\n\r\n";
    size_t old_len = strlen(form);
    size_t part_hdr_len = part.size();
    size_t new_len = old_len + part_hdr_len + (size_t)data_len + 4;
    char* new_form = (char*)realloc(form, new_len + 8);
    if (!new_form) return form;
    memcpy(new_form + old_len, part.data(), part_hdr_len);
    memcpy(new_form + old_len + part_hdr_len, data, (size_t)data_len);
    size_t pos = old_len + part_hdr_len + (size_t)data_len;
    new_form[pos] = '\r'; new_form[pos+1] = '\n';
    new_form[pos+2] = '\0';
    return new_form;
}

char* aurora_net_multipart_end(char* form, const char* boundary, int* out_len) {
    if (!form || !boundary) return form;
    std::string closing = std::string("--") + boundary + "--\r\n";
    size_t old_len = strlen(form);
    size_t new_len = old_len + closing.size() + 2;
    char* new_form = (char*)realloc(form, new_len + 2);
    if (!new_form) return form;
    memcpy(new_form + old_len, closing.data(), closing.size());
    new_form[old_len + closing.size()] = '\0';
    *out_len = (int)(old_len + closing.size());
    return new_form;
}

/* ── Download helper ── */
int aurora_net_download(const char* url, const char* filepath) {
    if (!url || !filepath) return -1;
    char buffer[16384];
    int result = aurora_net_http_get(url, buffer, sizeof(buffer));
    if (result < 0) return -1;
    FILE* f = fopen(filepath, "wb");
    if (!f) return -1;
    fwrite(buffer, 1, (size_t)result, f);
    fclose(f);
    return result;
}

/* ── Auth helpers ── */
void aurora_net_auth_basic(const char* username, const char* password,
                            char* out, int out_size) {
    if (!username || !password || !out || out_size <= 0) return;
    /* Simple Base64 encoding for "username:password" */
    std::string raw = std::string(username) + ":" + password;
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    size_t i = 0;
    while (i < raw.size()) {
        unsigned char c1 = (unsigned char)raw[i++];
        unsigned char c2 = (i < raw.size()) ? (unsigned char)raw[i++] : 0;
        unsigned char c3 = (i < raw.size()) ? (unsigned char)raw[i++] : 0;
        encoded += b64[c1 >> 2];
        encoded += b64[((c1 & 3) << 4) | (c2 >> 4)];
        encoded += (i > raw.size() - 2) ? '=' : b64[((c2 & 0x0f) << 2) | (c3 >> 6)];
        encoded += (i > raw.size() - 1) ? '=' : b64[c3 & 0x3f];
    }
    std::string hdr = "Authorization: Basic " + encoded + "\r\n";
    strncpy(out, hdr.c_str(), (size_t)out_size - 1);
    out[out_size - 1] = '\0';
}

void aurora_net_auth_bearer(const char* token, char* out, int out_size) {
    if (!token || !out || out_size <= 0) return;
    std::string hdr = "Authorization: Bearer " + std::string(token) + "\r\n";
    strncpy(out, hdr.c_str(), (size_t)out_size - 1);
    out[out_size - 1] = '\0';
}

int aurora_net_resolve_host(const char* hostname, char* buffer, int buffer_size) {
    return aurora_net_dns_lookup(hostname, buffer, buffer_size);
}

/* ── Parse URL ── */
static int parse_url(const char* url, char* host, int host_size,
                     int* port, char* path, int path_size) {
    const char* p = url;
    int is_https = 0;
    if (strncmp(p, "https://", 8) == 0) {
        is_https = 1;
        p += 8;
    } else if (strncmp(p, "http://", 7) == 0) {
        p += 7;
    }
    const char* slash = strchr(p, '/');
    const char* colon = strchr(p, ':');
    if (slash && colon && colon < slash) {
        size_t host_len = colon - p;
        if (host_len < (size_t)host_size) {
            memcpy(host, p, host_len);
            host[host_len] = '\0';
        }
        *port = atoi(colon + 1);
        strncpy(path, slash, (size_t)path_size - 1);
        path[path_size - 1] = '\0';
    } else if (slash) {
        size_t host_len = slash - p;
        if (host_len < (size_t)host_size) {
            memcpy(host, p, host_len);
            host[host_len] = '\0';
        }
        *port = is_https ? 443 : 80;
        strncpy(path, slash, (size_t)path_size - 1);
        path[path_size - 1] = '\0';
    } else {
        strncpy(host, p, (size_t)host_size - 1);
        host[host_size - 1] = '\0';
        *port = is_https ? 443 : 80;
        strcpy(path, "/");
    }
    return is_https;
}

#ifdef _WIN32
static int winhttp_request(const char* url, LPCWSTR method,
                           const char* headers,
                           const char* body, const char* content_type,
                           char* buffer, int buffer_size) {
    char host[256] = {0};
    char path[2048] = {0};
    int port = 80;
    parse_url(url, host, sizeof(host), &port, path, sizeof(path));

    HINTERNET hSession = WinHttpOpen(L"Aurora/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) return -1;

    wchar_t whost[256];
    MultiByteToWideChar(CP_UTF8, 0, host, -1, whost, 256);

    HINTERNET hConnect = WinHttpConnect(hSession, whost, (INTERNET_PORT)port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return -1; }

    wchar_t wpath[2048];
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, 2048);
    LPCWSTR types[] = { L"*/*", NULL };

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, method, wpath, NULL,
        NULL, types,
        (port == 443) ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return -1; }

    /* Build WinHTTP headers string */
    std::wstring wheaders;
    if (content_type) {
        char ct_hdr[512];
        snprintf(ct_hdr, sizeof(ct_hdr), "Content-Type: %s", content_type);
        wchar_t wct[512];
        MultiByteToWideChar(CP_UTF8, 0, ct_hdr, -1, wct, 512);
        wheaders = wct;
    }
    if (headers) {
        wchar_t whdrs[1024];
        MultiByteToWideChar(CP_UTF8, 0, headers, -1, whdrs, 1024);
        if (!wheaders.empty()) wheaders += L"\r\n";
        wheaders += whdrs;
    }

    DWORD body_len = body ? (DWORD)strlen(body) : 0;
    BOOL sent = WinHttpSendRequest(hRequest,
        wheaders.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : wheaders.c_str(),
        wheaders.empty() ? 0 : (DWORD)wheaders.size(),
        body_len > 0 ? (LPVOID)body : WINHTTP_NO_REQUEST_DATA,
        body_len, body_len, 0);

    if (!sent) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return -1;
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return -1;
    }

    int total = 0;
    DWORD read = 0;
    while (WinHttpReadData(hRequest, buffer + total,
                           (DWORD)(buffer_size - total - 1), &read) && read > 0) {
        total += (int)read;
        if (total >= buffer_size - 1) break;
    }
    buffer[total] = '\0';

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return total;
}
#endif

}
