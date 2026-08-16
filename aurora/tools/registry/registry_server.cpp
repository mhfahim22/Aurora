/* ═══════════════════════════════════════════════════════════════
   Aurora Registry Server (Phase 41.1 — Public Package Registry)
   ═══════════════════════════════════════════════════════════════
   A self-contained HTTP package registry. Stores packages on disk
   under a store directory, serves metadata + archives, accepts
   authenticated publishes, and generates SHA-256 checksums plus
   signed signatures.

   Endpoints:
     GET  /                          → HTML index of all packages
     GET  /packages/<name>/latest    → JSON metadata (latest version)
     GET  /packages/<name>/<ver>     → JSON metadata for a version
     GET  /archive/<name>/<ver>.tgz  → package archive (tgz)
     GET  /checksum/<name>/<ver>     → SHA-256 checksum hex
     POST /publish                   → publish archive (auth header)
     POST /unpublish                 → remove a version (auth header)

   Auth: X-Aurora-Token header must match the server token.
   ───────────────────────────────────────────────────────────── */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <filesystem>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

static std::string g_store_dir = "registry_store";
static std::string g_token = "";
static int g_port = 8090;
static const char* SIG_SALT = ":aurora-pkg-sign:v1";

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

/* ── SHA-256 (matches voss implementation) ── */
static std::string sha256_hex(const std::string& data) {
    uint32_t h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    const uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
        0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
        0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
        0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
        0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };
    auto rotr = [](uint32_t x, uint32_t n) -> uint32_t { return (x >> n) | (x << (32 - n)); };
    uint64_t bit_len = data.size() * 8;
    size_t new_len = ((data.size() + 8 + 64) / 64) * 64;
    std::vector<uint8_t> buf(new_len, 0);
    memcpy(buf.data(), data.data(), data.size());
    buf[data.size()] = 0x80;
    for (size_t i = 0; i < 8; i++) buf[new_len - 8 + i] = (uint8_t)(bit_len >> (56 - i * 8));
    for (size_t block = 0; block < new_len; block += 64) {
        uint32_t w[64];
        for (int t = 0; t < 16; t++)
            w[t] = ((uint32_t)buf[block + t * 4] << 24) | ((uint32_t)buf[block + t * 4 + 1] << 16) | ((uint32_t)buf[block + t * 4 + 2] << 8) | ((uint32_t)buf[block + t * 4 + 3]);
        for (int t = 16; t < 64; t++) {
            uint32_t s0 = rotr(w[t - 15], 7) ^ rotr(w[t - 15], 18) ^ (w[t - 15] >> 3);
            uint32_t s1 = rotr(w[t - 2], 17) ^ rotr(w[t - 2], 19) ^ (w[t - 2] >> 10);
            w[t] = w[t - 16] + s0 + w[t - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int t = 0; t < 64; t++) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t temp1 = hh + S1 + ch + k[t] + w[t];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t temp2 = S0 + maj;
            hh = g; g = f; f = e; e = d + temp1;
            d = c; c = b; b = a; a = temp1 + temp2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    std::string hex;
    const char* hex_chars = "0123456789abcdef";
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 4; j++)
        { uint8_t byte = (uint8_t)(h[i] >> (24 - j * 8)); hex += hex_chars[(byte >> 4) & 0xf]; hex += hex_chars[byte & 0xf]; }
    return hex;
}

/* ── Package path helpers ── */
static std::string pkg_dir(const std::string& name, const std::string& version) {
    return g_store_dir + "/" + name + "/" + version;
}

static std::string archive_path(const std::string& name, const std::string& version) {
    return pkg_dir(name, version) + "/package.tgz";
}

static std::string meta_path(const std::string& name, const std::string& version) {
    return pkg_dir(name, version) + "/meta.json";
}

static std::string checksum_path(const std::string& name, const std::string& version) {
    return pkg_dir(name, version) + "/package.sha256";
}

static std::string sig_path(const std::string& name, const std::string& version) {
    return pkg_dir(name, version) + "/package.sig";
}

static std::string read_file_str(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static bool write_file_bin(const std::string& path, const std::string& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    f.write(data.data(), (std::streamsize)data.size());
    return true;
}

/* ── Metadata: build JSON for a package version ── */
static std::string build_meta_json(const std::string& name, const std::string& version,
                                   const std::string& author, const std::string& desc,
                                   const std::string& entry, const std::vector<std::string>& deps) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"name\": \"" << name << "\",\n";
    ss << "  \"version\": \"" << version << "\",\n";
    ss << "  \"author\": \"" << author << "\",\n";
    ss << "  \"description\": \"" << desc << "\",\n";
    ss << "  \"entry\": \"" << entry << "\",\n";
    ss << "  \"dependencies\": [";
    for (size_t i = 0; i < deps.size(); i++) {
        if (i > 0) ss << ", ";
        ss << "\"" << deps[i] << "\"";
    }
    ss << "],\n";
    std::string checksum = read_file_str(checksum_path(name, version));
    ss << "  \"integrity\": \"sha256-" << checksum << "\",\n";
    std::string sig = read_file_str(sig_path(name, version));
    ss << "  \"signature\": \"" << sig << "\"\n";
    ss << "}\n";
    return ss.str();
}

/* ── List latest version of a package ── */
static bool find_latest_version(const std::string& name, std::string& out_ver) {
    std::string dir = g_store_dir + "/" + name;
    if (!fs::exists(dir)) return false;
    std::vector<std::string> vers;
    for (auto& e : fs::directory_iterator(dir))
        if (e.is_directory()) vers.push_back(e.path().filename().string());
    if (vers.empty()) return false;
    std::sort(vers.begin(), vers.end(), [](const std::string& a, const std::string& b) {
        auto num = [](const std::string& s) {
            std::vector<int> parts;
            std::stringstream ss(s); std::string p;
            while (std::getline(ss, p, '.')) parts.push_back(atoi(p.c_str()));
            return parts;
        };
        auto va = num(a), vb = num(b);
        size_t mx = std::max(va.size(), vb.size());
        for (size_t i = 0; i < mx; i++) {
            int na = i < va.size() ? va[i] : 0;
            int nb = i < vb.size() ? vb[i] : 0;
            if (na != nb) return na > nb;
        }
        return false;
    });
    out_ver = vers[0];
    return true;
}

/* ── HTTP helpers ── */
static void send_response(int cs, int code, const std::string& mime,
                          const std::string& body, const std::string& extra_hdr = "") {
    std::ostringstream res;
    std::string status = (code == 200) ? "200 OK" :
                         (code == 201) ? "201 Created" :
                         (code == 404) ? "404 Not Found" :
                         (code == 401) ? "401 Unauthorized" :
                         (code == 400) ? "400 Bad Request" : "500 Internal Server Error";
    res << "HTTP/1.1 " << status << "\r\n";
    res << "Content-Type: " << mime << "\r\n";
    res << "Content-Length: " << body.size() << "\r\n";
    if (!extra_hdr.empty()) res << extra_hdr;
    res << "Connection: close\r\n\r\n";
    res << body;
    std::string rs = res.str();
    send(cs, rs.c_str(), (int)rs.size(), 0);
}

static void send_html(int cs, const std::string& body) { send_response(cs, 200, "text/html", body); }
static void send_json(int cs, int code, const std::string& body) { send_response(cs, code, "application/json", body); }
static void send_error(int cs, int code, const std::string& msg) {
    send_json(cs, code, "{\"error\": \"" + msg + "\"}\n");
}

/* ── Parse request line: GET /path HTTP/1.1 ── */
static std::string parse_path(const std::string& req) {
    size_t g = req.find(' ');
    if (g == std::string::npos) return "/";
    size_t e = req.find(' ', g + 1);
    if (e == std::string::npos) return req.substr(g + 1);
    return req.substr(g + 1, e - g - 1);
}

/* ── Extract a request header value (case-insensitive) ── */
static std::string get_header(const std::string& req, const std::string& name) {
    size_t pos = 0;
    while (true) {
        size_t nl = req.find("\r\n", pos);
        if (nl == std::string::npos) break;
        std::string line = req.substr(pos, nl - pos);
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = trim(line.substr(0, colon));
            std::string val = trim(line.substr(colon + 1));
            std::string lk = key;
            std::transform(lk.begin(), lk.end(), lk.begin(), ::tolower);
            std::string lname = name;
            std::transform(lname.begin(), lname.end(), lname.begin(), ::tolower);
            if (lk == lname) return val;
        }
        pos = nl + 2;
        if (pos + 1 < req.size() && req[pos] == '\r' && req[pos + 1] == '\n') break;
    }
    return "";
}

static int get_content_length(const std::string& req) {
    std::string cl = get_header(req, "Content-Length");
    return cl.empty() ? 0 : atoi(cl.c_str());
}

static bool check_auth(const std::string& req) {
    if (g_token.empty()) return true;
    std::string tok = get_header(req, "X-Aurora-Token");
    return tok == g_token;
}

/* ── Percent-decode path component ── */
static std::string url_decode(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int v;
            char hex[3] = { s[i + 1], s[i + 2], 0 };
            v = (int)strtol(hex, nullptr, 16);
            out += (char)v;
            i += 2;
        } else if (s[i] == '+') {
            out += ' ';
        } else {
            out += s[i];
        }
    }
    return out;
}

/* ── Handle a single HTTP request ── */
static void handle_request(int cs, const std::string& req) {
    std::string path = parse_path(req);

    /* Root index */
    if (path == "/" || path == "/index.html") {
        std::ostringstream html;
        html << "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\"><title>Aurora Package Registry</title></head>\n"
             << "<body style=\"font-family: sans-serif; max-width: 800px; margin: 0 auto; padding: 2em;\">\n"
             << "<h1>Aurora Package Registry</h1>\n"
             << "<p>Self-hosted package registry for the Aurora language.</p>\n"
             << "<h2>Publishing</h2>\n"
             << "<pre>voss publish --registry http://localhost:" << g_port << "</pre>\n"
             << "<h2>Installing</h2>\n"
             << "<pre>voss install &lt;pkg&gt;@&lt;version&gt; --registry http://localhost:" << g_port << "</pre>\n";
        html << "<h2>Available packages</h2>\n<ul>\n";
        if (fs::exists(g_store_dir)) {
            for (auto& e : fs::directory_iterator(g_store_dir)) {
                if (!e.is_directory()) continue;
                std::string name = e.path().filename().string();
                std::string latest;
                if (find_latest_version(name, latest))
                    html << "<li><a href=\"/packages/" << name << "/latest\">" << name << "</a> @" << latest << "</li>\n";
            }
        }
        html << "</ul>\n</body></html>\n";
        send_html(cs, html.str());
        return;
    }

    /* GET /packages/<name>/latest|<ver> */
    if (path.rfind("/packages/", 0) == 0) {
        std::string rest = path.substr(10);
        size_t slash = rest.find('/');
        std::string name = url_decode(slash == std::string::npos ? rest : rest.substr(0, slash));
        std::string ver = slash == std::string::npos ? "latest" : url_decode(rest.substr(slash + 1));
        if (ver == "latest") {
            if (!find_latest_version(name, ver)) {
                send_error(cs, 404, "package not found: " + name);
                return;
            }
        }
        if (!fs::exists(meta_path(name, ver))) {
            send_error(cs, 404, "version not found: " + name + "@" + ver);
            return;
        }
        send_json(cs, 200, read_file_str(meta_path(name, ver)));
        return;
    }

    /* GET /archive/<name>/<ver>.tgz */
    if (path.rfind("/archive/", 0) == 0) {
        std::string rest = path.substr(9);
        size_t slash = rest.find('/');
        std::string name = url_decode(rest.substr(0, slash));
        std::string file = rest.substr(slash + 1);
        std::string ver = file;
        size_t dot = ver.rfind(".tgz");
        if (dot != std::string::npos) ver = ver.substr(0, dot);
        if (ver == "latest") {
            if (!find_latest_version(name, ver)) {
                send_error(cs, 404, "package not found: " + name);
                return;
            }
        }
        std::string ap = archive_path(name, ver);
        if (!fs::exists(ap)) {
            send_error(cs, 404, "archive not found");
            return;
        }
        std::string body = read_file_str(ap);
        send_response(cs, 200, "application/gzip", body);
        return;
    }

    /* GET /checksum/<name>/<ver> */
    if (path.rfind("/checksum/", 0) == 0) {
        std::string rest = path.substr(10);
        size_t slash = rest.find('/');
        std::string name = url_decode(rest.substr(0, slash));
        std::string ver = url_decode(rest.substr(slash + 1));
        if (ver == "latest") {
            if (!find_latest_version(name, ver)) {
                send_error(cs, 404, "package not found: " + name);
                return;
            }
        }
        if (!fs::exists(checksum_path(name, ver))) {
            send_error(cs, 404, "checksum not found");
            return;
        }
        send_response(cs, 200, "text/plain", read_file_str(checksum_path(name, ver)) + "\n");
        return;
    }

    /* POST /publish — body is the tgz archive; query/headers carry metadata */
    if (path == "/publish") {
        if (!check_auth(req)) {
            send_error(cs, 401, "invalid or missing X-Aurora-Token");
            return;
        }
        std::string name = url_decode(get_header(req, "X-Package-Name"));
        std::string version = url_decode(get_header(req, "X-Package-Version"));
        std::string author = url_decode(get_header(req, "X-Package-Author"));
        std::string desc = url_decode(get_header(req, "X-Package-Description"));
        std::string entry = url_decode(get_header(req, "X-Package-Entry"));
        if (name.empty() || version.empty()) {
            send_error(cs, 400, "missing X-Package-Name / X-Package-Version headers");
            return;
        }
        int clen = get_content_length(req);
        if (clen <= 0) {
            send_error(cs, 400, "missing archive body (Content-Length 0)");
            return;
        }
        /* Read body after header terminator */
        size_t hdr_end = req.find("\r\n\r\n");
        if (hdr_end == std::string::npos) {
            send_error(cs, 400, "malformed request");
            return;
        }
        std::string body = req.substr(hdr_end + 4);
        if ((int)body.size() < clen) {
            /* Read remaining bytes from socket */
            char buf[8192];
            while ((int)body.size() < clen) {
                int n = (int)recv(cs, buf, sizeof(buf), 0);
                if (n <= 0) break;
                body.append(buf, n);
            }
        }
        if ((int)body.size() < clen) {
            send_error(cs, 400, "incomplete archive body");
            return;
        }
        body.resize(clen);

        std::string dir = pkg_dir(name, version);
        fs::create_directories(dir);
        if (!write_file_bin(archive_path(name, version), body)) {
            send_error(cs, 500, "could not store archive");
            return;
        }
        /* Checksum */
        std::string checksum = sha256_hex(body);
        write_file_bin(checksum_path(name, version), checksum);
        /* Signature: sha256(name@version:salt) */
        std::string sig = sha256_hex(name + "@" + version + SIG_SALT);
        write_file_bin(sig_path(name, version), sig);
        /* Metadata */
        std::vector<std::string> deps;
        write_file_bin(meta_path(name, version), build_meta_json(name, version, author, desc, entry, deps));

        std::ostringstream resp;
        resp << "{\"ok\": true, \"name\": \"" << name << "\", \"version\": \"" << version
             << "\", \"integrity\": \"sha256-" << checksum << "\", \"signature\": \"" << sig << "\"}\n";
        send_json(cs, 201, resp.str());
        std::cout << "  published " << name << "@" << version
                  << " (sha256-" << checksum.substr(0, 12) << "...)\n";
        return;
    }

    /* POST /unpublish — remove a version */
    if (path == "/unpublish") {
        if (!check_auth(req)) {
            send_error(cs, 401, "invalid or missing X-Aurora-Token");
            return;
        }
        std::string name = url_decode(get_header(req, "X-Package-Name"));
        std::string version = url_decode(get_header(req, "X-Package-Version"));
        if (name.empty() || version.empty()) {
            send_error(cs, 400, "missing X-Package-Name / X-Package-Version headers");
            return;
        }
        std::string dir = pkg_dir(name, version);
        if (fs::exists(dir)) fs::remove_all(dir);
        send_json(cs, 200, "{\"ok\": true, \"removed\": \"" + name + "@" + version + "\"}\n");
        std::cout << "  unpublished " << name << "@" << version << "\n";
        return;
    }

    send_error(cs, 404, "not found: " + path);
}

/* ── Simple event loop ── */
static void run_server() {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    int sock = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { std::cerr << "error: socket\n"; return; }
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)g_port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "error: bind port " << g_port << "\n";
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }
    listen(sock, 16);
    std::cout << "Aurora registry server listening on http://localhost:" << g_port << "\n";
    std::cout << "  store: " << g_store_dir << "\n";

    while (true) {
        struct sockaddr_in cli;
#ifdef _WIN32
        int cli_len = sizeof(cli);
#else
        socklen_t cli_len = sizeof(cli);
#endif
        int cs = (int)accept(sock, (struct sockaddr*)&cli, &cli_len);
        if (cs < 0) continue;
        char buf[16384];
        int n = (int)recv(cs, buf, sizeof(buf) - 1, 0);
        if (n > 0) {
            buf[n] = 0;
            std::string req(buf, n);
            handle_request(cs, req);
        }
#ifdef _WIN32
        closesocket(cs);
#else
        close(cs);
#endif
    }
#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif
}

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) g_port = atoi(argv[++i]);
        else if (strcmp(argv[i], "--store") == 0 && i + 1 < argc) g_store_dir = argv[++i];
        else if (strcmp(argv[i], "--token") == 0 && i + 1 < argc) g_token = argv[++i];
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            std::cout << "Aurora Package Registry Server\n\n"
                      << "Usage: aurora_registry [--port <port>] [--store <dir>] [--token <token>]\n\n"
                      << "  --port   HTTP port (default 8090)\n"
                      << "  --store  package store directory (default ./registry_store)\n"
                      << "  --token  auth token required for publish/unpublish (default: none)\n";
            return 0;
        }
    }
    fs::create_directories(g_store_dir);
    run_server();
    return 0;
}