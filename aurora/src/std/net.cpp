#include "std/net.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#endif

/* ── Thread-safe DNS resolver + connect (IPv4/IPv6) ── */
/* Returns connected socket fd, or -1 on failure */
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
        if (connect(sock, rp->ai_addr, (int)rp->ai_addrlen) == 0) {
            break; /* success */
        }
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

/* ── Send all bytes (handles partial writes, prevents SIGPIPE) ── */
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

#ifdef _WIN32
static int net_initialized = 0;
static void ensure_winsock() {
    if (!net_initialized) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        net_initialized = 1;
    }
}
#endif

/* ── HTTP POST helper ── */
int aurora_net_http_post(const char* url, const char* body, const char* content_type,
                          char* buffer, int buffer_size) {
#ifdef _WIN32
    ensure_winsock();
#endif
    char host[256] = {0};
    char path[1024] = {0};

    const char* p = url;
    if (strncmp(p, "http://", 7) == 0) p += 7;
    const char* slash = strchr(p, '/');
    if (slash) {
        size_t host_len = slash - p;
        if (host_len < sizeof(host)) {
            memcpy(host, p, host_len);
            host[host_len] = '\0';
        }
        strncpy(path, slash, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
    } else {
        strncpy(host, p, sizeof(host) - 1);
        host[sizeof(host) - 1] = '\0';
        strcpy(path, "/");
    }

    int sock = resolve_and_connect(host, 80);
    if (sock < 0) return -1;

    char request[4096];
    size_t body_len = body ? strlen(body) : 0;
    const char* ct = content_type ? content_type : "application/x-www-form-urlencoded";
    int req_len = snprintf(request, sizeof(request),
        "POST %s HTTP/1.0\r\nHost: %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n%s",
        path, host, ct, body_len, body ? body : "");

    aurora_send_all((int64_t)(intptr_t)sock, request, req_len);

    int total = 0;
    int n;
    while ((n = (int)recv(sock, buffer + total, buffer_size - total - 1, 0)) > 0) {
        total += n;
        if (total >= buffer_size - 1) break;
    }
    buffer[total] = '\0';

    char* body_start = strstr(buffer, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        int body_len_resp = (int)(buffer + total - body_start);
        memmove(buffer, body_start, body_len_resp);
        buffer[body_len_resp] = '\0';
        total = body_len_resp;
    }

#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
    return total;
}

/* ── DNS lookup: resolve hostname to IP string ── */
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

/* ── TCP Socket functions ── */
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

int aurora_net_resolve_host(const char* hostname, char* buffer, int buffer_size) {
    return aurora_net_dns_lookup(hostname, buffer, buffer_size);
}

int aurora_net_http_get(const char* url, char* buffer, int buffer_size) {
#ifdef _WIN32
    ensure_winsock();
#endif
    /* Simple HTTP GET — basic implementation */
    char host[256] = {0};
    char path[1024] = {0};

    /* Parse URL */
    const char* p = url;
    if (strncmp(p, "http://", 7) == 0) p += 7;
    const char* slash = strchr(p, '/');
    if (slash) {
        size_t host_len = slash - p;
        if (host_len < sizeof(host)) {
            memcpy(host, p, host_len);
            host[host_len] = '\0';
        }
        strncpy(path, slash, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
    } else {
        strncpy(host, p, sizeof(host) - 1);
        host[sizeof(host) - 1] = '\0';
        strcpy(path, "/");
    }

    /* Create socket */
    int sock = resolve_and_connect(host, 80);
    if (sock < 0) return -1;

    char request[2048];
    snprintf(request, sizeof(request),
        "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",
        path, host);

    aurora_send_all((int64_t)(intptr_t)sock, request, (int)strlen(request));

    int total = 0;
    int n;
    while ((n = (int)recv(sock, buffer + total, buffer_size - total - 1, 0)) > 0) {
        total += n;
        if (total >= buffer_size - 1) break;
    }
    buffer[total] = '\0';

    /* Find body after headers */
    char* body = strstr(buffer, "\r\n\r\n");
    if (body) {
        body += 4;
        int body_len = (int)(buffer + total - body);
        memmove(buffer, body, body_len);
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

}
