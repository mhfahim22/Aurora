#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#pragma comment(lib, "ws2_32.lib")

struct BenchResult {
    int total_requests;
    int success_count;
    int failure_count;
    double elapsed_sec;
    double req_per_sec;
    double avg_latency_ms;
    long long total_bytes;
};

struct ThreadResult {
    int success = 0;
    int failure = 0;
    long long bytes = 0;
    double total_latency_ms = 0;
};

/* Simple HTTP GET request over a new connection */
static bool do_request(const char* host, int port, const char* path, bool use_gzip,
                       long long* out_bytes, double* out_latency_ms) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) return false;

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    auto t0 = std::chrono::steady_clock::now();
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        closesocket(sock);
        return false;
    }

    char req[16384];
    int n = snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "%s"
        "Connection: close\r\n"
        "\r\n",
        path, host, port,
        use_gzip ? "Accept-Encoding: gzip\r\n" : "");

    /* Send with retry */
    int sent = 0;
    while (sent < n) {
        int r = send(sock, req + sent, n - sent, 0);
        if (r <= 0) { closesocket(sock); return false; }
        sent += r;
    }

    /* Read response into vector (response may be large) */
    std::vector<char> response;
    char chunk[65536];
    long long total = 0;
    while (true) {
        int r = recv(sock, chunk, sizeof(chunk), 0);
        if (r <= 0) break;
        response.insert(response.end(), chunk, chunk + r);
        total += r;
    }

    closesocket(sock);
    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    /* Find status code */
    bool ok = false;
    if (total >= 12 && response[9] == '2' && response[10] == '0' && response[11] == '0')
        ok = true;

    if (out_bytes) *out_bytes = total;
    if (out_latency_ms) *out_latency_ms = ms;
    return ok;
}

static void worker_thread(const char* host, int port, const char* path, bool use_gzip,
                          int count, std::atomic<int>& stop_flag, ThreadResult& result) {
    for (int i = 0; i < count && stop_flag.load() == 0; i++) {
        long long bytes = 0;
        double latency = 0;
        if (do_request(host, port, path, use_gzip, &bytes, &latency)) {
            result.success++;
            result.bytes += bytes;
            result.total_latency_ms += latency;
        } else {
            result.failure++;
        }
    }
}

static BenchResult run_benchmark(const char* host, int port, const char* path,
                                 bool use_gzip, int num_threads, int requests_per_thread) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    /* Warmup */
    for (int i = 0; i < 10; i++) {
        do_request(host, port, path, use_gzip, nullptr, nullptr);
    }

    std::atomic<int> stop_flag{0};
    std::vector<std::thread> threads;
    std::vector<ThreadResult> results(num_threads);

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(worker_thread, host, port, path, use_gzip,
                             requests_per_thread, std::ref(stop_flag), std::ref(results[i]));
    }

    for (auto& t : threads) t.join();

    WSACleanup();

    auto end = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();

    BenchResult r;
    r.total_requests = num_threads * requests_per_thread;
    r.success_count = 0;
    r.failure_count = 0;
    r.total_bytes = 0;
    r.avg_latency_ms = 0;
    for (auto& res : results) {
        r.success_count += res.success;
        r.failure_count += res.failure;
        r.total_bytes += res.bytes;
        r.avg_latency_ms += res.total_latency_ms;
    }
    r.elapsed_sec = elapsed;
    r.req_per_sec = r.success_count / elapsed;
    if (r.success_count > 0)
        r.avg_latency_ms /= r.success_count;

    return r;
}

static void print_result(const char* label, const BenchResult& r) {
    printf("  %-40s\n", label);
    printf("    Requests:      %d (%d OK, %d failed)\n", r.total_requests, r.success_count, r.failure_count);
    printf("    Time:          %.2f sec\n", r.elapsed_sec);
    printf("    Throughput:    %.0f req/s\n", r.req_per_sec);
    printf("    Avg latency:   %.2f ms\n", r.avg_latency_ms);
    printf("    Bandwidth:     %.2f MB/s\n", r.total_bytes / (1024.0 * 1024.0) / r.elapsed_sec);
    printf("\n");
}

int main() {
    printf("╔════════════════════════════════════════════════╗\n");
    printf("║        Aurora HTTP Server Benchmark           ║\n");
    printf("╚════════════════════════════════════════════════╝\n\n");

    const char* host = "127.0.0.1";
    int port = 8080;
    const char* path = "/todos";
    int threads = 16;
    int req_per_thread = 200; /* 3200 total requests */

    printf("Target:  http://%s:%d%s\n", host, port, path);
    printf("Config:  %d threads × %d requests = %d total\n\n", threads, req_per_thread, threads * req_per_thread);

    /* Benchmark with gzip */
    auto r1 = run_benchmark(host, port, path, true, threads, req_per_thread);
    print_result("With gzip compression (Accept-Encoding: gzip)", r1);

    /* Benchmark without gzip */
    auto r2 = run_benchmark(host, port, path, false, threads, req_per_thread);
    print_result("Without gzip compression", r2);

    /* Comparison */
    printf("── Comparison ──\n");
    if (r2.req_per_sec > 0) {
        double diff = ((r1.req_per_sec / r2.req_per_sec) - 1.0) * 100.0;
        printf("  gzip vs plain: %+.1f%% throughput\n", diff);
    }
    printf("\n");

    return 0;
}
