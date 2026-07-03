#pragma once

#include <algorithm>

#define AURORA_UA "Aurora-Voss/0.4 (github.com/anomalyco/aurora)"

#ifdef _WIN32
#include <string>
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

/* ════════════════════════════════════════════════════════════
   WinHTTP-based HTTPS client (Windows native)
   ════════════════════════════════════════════════════════════
   Supports:
   - HTTPS (TLS 1.2+)
   - HTTP redirect following
   - Keep-alive
   - Last HTTP status code tracking (for 503 detection)
   ════════════════════════════════════════════════════════════ */

/* Last HTTP status code from http_get_impl (thread-safe) */
thread_local static int g_last_http_status = 200;

static std::wstring utf8_to_wide(const std::string& s) {
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
    if (len <= 0) return {};
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], len);
    return w;
}

static std::string http_get_impl(const std::string& url, int timeout_ms) {
    g_last_http_status = 0;

    /* ── Parse URL: [https://]host[:port][/path] ── */
    bool use_tls = true;
    size_t start = 0;
    size_t proto = url.find("://");
    if (proto != std::string::npos) {
        use_tls = (url.substr(0, proto) == "https");
        start = proto + 3;
    }
    size_t path_start = url.find('/', start);
    size_t port_start = url.find(':', start);

    std::string host;
    std::string path = "/";
    int port = use_tls ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;

    if (path_start != std::string::npos) {
        host = url.substr(start, path_start - start);
        path = url.substr(path_start);
    } else {
        host = url.substr(start);
    }

    /* Check for explicit port */
    if (port_start != std::string::npos && port_start < path_start) {
        host = url.substr(start, port_start - start);
        std::string port_str = url.substr(port_start + 1, path_start - port_start - 1);
        port = std::stoi(port_str);
    }

    /* Open WinHTTP session with proper User-Agent */
    std::wstring wua = utf8_to_wide(AURORA_UA);
    HINTERNET hSession = WinHttpOpen(wua.c_str(),
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      NULL, NULL, 0);
    if (!hSession) return {};

    std::wstring whost = utf8_to_wide(host);
    std::wstring wpath = utf8_to_wide(path);

    /* Connect */
    HINTERNET hConnect = WinHttpConnect(hSession, whost.c_str(), (INTERNET_PORT)port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return {};
    }

    /* Open request */
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wpath.c_str(), NULL,
                                            NULL, NULL,
                                            use_tls ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return {};
    }

    /* Set options: redirect follow, timeout */
    DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY,
                     &redirect_policy, sizeof(redirect_policy));

    DWORD total_timeout = (DWORD)timeout_ms;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_CONNECT_TIMEOUT,
                     &total_timeout, sizeof(total_timeout));
    WinHttpSetOption(hRequest, WINHTTP_OPTION_SEND_TIMEOUT,
                     &total_timeout, sizeof(total_timeout));
    WinHttpSetOption(hRequest, WINHTTP_OPTION_RECEIVE_TIMEOUT,
                     &total_timeout, sizeof(total_timeout));

    /* Compatibility: TLS 1.2+ */
    DWORD tls_protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 |
                           WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURE_PROTOCOLS,
                     &tls_protocols, sizeof(tls_protocols));

    /* Send request */
    LPCWSTR headers = L"Accept: application/json\r\n";
    if (!WinHttpSendRequest(hRequest, headers, wcslen(headers),
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return {};
    }

    /* Receive response */
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return {};
    }

    /* Check HTTP status */
    DWORD status = 0;
    DWORD status_size = sizeof(status);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        NULL, &status, &status_size, NULL);
    g_last_http_status = (int)status;
    if (status < 200 || status >= 300) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return {};
    }

    /* Read all data */
    std::string result;
    char buf[4096];
    DWORD bytes_read = 0;
    while (WinHttpReadData(hRequest, buf, sizeof(buf) - 1, &bytes_read) && bytes_read > 0) {
        buf[bytes_read] = '\0';
        result += buf;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return result;
}

#else
/* POSIX fallback: use libcurl */
#include <string>
#include <unistd.h>
#include <curl/curl.h>

static size_t write_cb(void* contents, size_t size, size_t nmemb, std::string* out) {
    size_t total = size * nmemb;
    out->append((char*)contents, total);
    return total;
}

/* Last HTTP status code from http_get_impl (thread-safe) */
static thread_local int g_last_http_status = 200;

static std::string http_get_impl(const std::string& url, int timeout_ms) {
    g_last_http_status = 0;
    CURL* curl = curl_easy_init();
    if (!curl) return {};

    std::string result;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)timeout_ms);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, AURORA_UA);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

    CURLcode res = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    g_last_http_status = (int)http_code;

    curl_easy_cleanup(curl);

    if (res != CURLE_OK || (http_code >= 400)) return {};
    return result;
}
#endif

/* ── Cross-platform sleep in milliseconds ── */
#include <thread>
static void sleep_ms(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

/* ── Rate limiter: minimum interval between requests to the same host ── */
/* Prevents 503 from rapid-fire requests to rate-limited APIs */
#include <chrono>
#include <unordered_map>
#include <mutex>

static void rate_limit_host(const std::string& url) {
    static std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_req;
    static std::mutex rl_mutex;

    /* Extract host from URL */
    size_t proto = url.find("://");
    size_t start = (proto != std::string::npos) ? proto + 3 : 0;
    size_t end = url.find('/', start);
    std::string host = url.substr(start, end - start);

    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(rl_mutex);

    auto it = last_req.find(host);
    if (it != last_req.end()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second);
        int min_interval = 1500; /* 1.5s minimum between requests to same host */
        if (elapsed.count() < min_interval) {
            sleep_ms(min_interval - (int)elapsed.count());
        }
    }
    last_req[host] = std::chrono::steady_clock::now();
}

/* ── Exponential backoff: wait duration for attempt N (0-indexed) ── */
static int backoff_duration(int attempt, bool is_rate_limit) {
    if (is_rate_limit) {
        /* 2s, 4s, 8s, 16s for rate limiting */
        return (std::min)(2000 * (1 << attempt), 16000);
    }
    /* 500ms, 1000ms for transient errors */
    return 500 * (1 << attempt);
}

/* ── Public API: http_get with retry + exponential backoff ── */
/* For 503/429 (rate limit): up to 4 attempts with 2s/4s/8s/16s backoff
   For other failures: up to 3 attempts with 500ms/1s backoff */
static std::string http_get(const std::string& url, int timeout_ms = 15000) {
    rate_limit_host(url);

    for (int attempt = 0; attempt < 4; attempt++) {
        std::string result = http_get_impl(url, timeout_ms);
        if (!result.empty()) return result;

        bool is_rate_limit = (g_last_http_status == 503 || g_last_http_status == 429);
        int max_attempts = is_rate_limit ? 4 : 3;

        if (attempt >= max_attempts - 1) break;
        sleep_ms(backoff_duration(attempt, is_rate_limit));
    }
    return {};
}
