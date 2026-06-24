#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#pragma comment(lib, "ws2_32.lib")

bool do_request(bool use_gzip) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) { printf("socket failed\n"); return false; }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        printf("connect failed: %d\n", WSAGetLastError());
        closesocket(sock);
        return false;
    }

    char req[4096];
    int n = snprintf(req, sizeof(req),
        "GET /todos HTTP/1.1\r\n"
        "Host: 127.0.0.1:8080\r\n"
        "%s"
        "Connection: close\r\n"
        "\r\n",
        use_gzip ? "Accept-Encoding: gzip\r\n" : "");

    int sent = send(sock, req, n, 0);
    if (sent != n) { printf("send %d != %d\n", sent, n); closesocket(sock); return false; }

    char buf[65536];
    int total = 0;
    while (true) {
        int r = recv(sock, buf + total, sizeof(buf) - total, 0);
        if (r <= 0) break;
        total += r;
    }
    closesocket(sock);

    if (total >= 12 && buf[9] == '2' && buf[10] == '0' && buf[11] == '0')
        return true;
    printf("  FAIL: total=%d, status=%.12s\n", total, total >= 12 ? buf : "?");
    return false;
}

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);

    for (int i = 0; i < 5; i++) {
        bool ok = do_request(false);
        printf("Request %d: %s\n", i+1, ok ? "OK" : "FAILED");
    }

    printf("\n--- threaded test ---\n");
    std::atomic<int> ok_count{0};
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; i++) {
        threads.emplace_back([&ok_count]() {
            WSADATA wsa2;
            WSAStartup(MAKEWORD(2,2), &wsa2);
            for (int j = 0; j < 100; j++) {
                if (do_request(false)) ok_count++;
            }
            WSACleanup();
        });
    }
    for (auto& t : threads) t.join();
    printf("Threaded: %d/800 OK\n", ok_count.load());

    WSACleanup();
    return 0;
}
