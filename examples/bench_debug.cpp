#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#pragma comment(lib, "ws2_32.lib")

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        printf("connect failed: %d\n", WSAGetLastError());
        return 1;
    }

    const char* req = "GET /todos HTTP/1.1\r\nHost: 127.0.0.1:8080\r\nConnection: close\r\n\r\n";
    send(sock, req, (int)strlen(req), 0);

    char buf[65536];
    int total = 0;
    while (true) {
        int r = recv(sock, buf + total, sizeof(buf) - total, 0);
        if (r <= 0) break;
        total += r;
    }
    buf[total] = 0;

    printf("total=%d bytes\n", total);
    printf("First 200 chars:\n%.200s\n", buf);
    printf("\n\nHex of first 16:\n");
    for (int i = 0; i < 16 && i < total; i++)
        printf("%02X ", (unsigned char)buf[i]);
    printf("\n");

    printf("Status check: total>=12? %s, buf[9..11]=%c%c%c\n",
           total >= 12 ? "yes" : "no",
           total >= 12 ? buf[9] : '?',
           total >= 12 ? buf[10] : '?',
           total >= 12 ? buf[11] : '?');

    closesocket(sock);
    WSACleanup();
    return 0;
}
