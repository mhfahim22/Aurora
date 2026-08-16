#include "std/url.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <algorithm>

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

struct AuroraUrl {
    std::string scheme;
    std::string userinfo;
    std::string host;
    int         port;
    std::string path;
    std::string query;
    std::string fragment;
    std::string full; /* cached full string for c_str() returns */
};

static std::string url_component_decode(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size()) {
            char hex[3] = { s[i+1], s[i+2], 0 };
            out += (char)strtol(hex, nullptr, 16);
            i += 2;
        } else if (s[i] == '+') {
            out += ' ';
        } else {
            out += s[i];
        }
    }
    return out;
}

extern "C" {

AuroraUrl* aurora_url_parse(const char* str) {
    if (!str) return nullptr;
    auto* url = new AuroraUrl();
    std::string s(str);
    size_t pos = 0;

    /* scheme:// */
    size_t scheme_end = s.find("://");
    if (scheme_end != std::string::npos) {
        url->scheme = s.substr(0, scheme_end);
        pos = scheme_end + 3;
    }

    /* userinfo@host:port */
    size_t path_start = s.find('/', pos);
    if (path_start == std::string::npos) path_start = s.size();
    size_t query_start = s.find('?', pos);
    size_t frag_start = s.find('#', pos);
    if (path_start > s.size()) path_start = s.size();

    std::string authority = s.substr(pos, path_start - pos);
    size_t at_pos = authority.find('@');
    if (at_pos != std::string::npos) {
        url->userinfo = url_component_decode(authority.substr(0, at_pos));
        authority = authority.substr(at_pos + 1);
    }
    size_t colon_pos = authority.find(':');
    if (colon_pos != std::string::npos) {
        url->host = authority.substr(0, colon_pos);
        url->port = atoi(authority.substr(colon_pos + 1).c_str());
    } else {
        url->host = authority;
        url->port = 0;
    }

    /* path */
    size_t end_of_path = s.size();
    if (query_start != std::string::npos && query_start < end_of_path) end_of_path = query_start;
    if (frag_start != std::string::npos && frag_start < end_of_path) end_of_path = frag_start;
    if (path_start < end_of_path)
        url->path = s.substr(path_start, end_of_path - path_start);
    if (url->path.empty()) url->path = "/";

    /* query */
    if (query_start != std::string::npos) {
        size_t qend = (frag_start != std::string::npos) ? frag_start : s.size();
        url->query = s.substr(query_start + 1, qend - query_start - 1);
    }

    /* fragment */
    if (frag_start != std::string::npos)
        url->fragment = s.substr(frag_start + 1);

    return url;
}

void aurora_url_free(AuroraUrl* url) { delete url; }

const char* aurora_url_scheme(AuroraUrl* url) { return url ? url->scheme.c_str() : ""; }
const char* aurora_url_host(AuroraUrl* url) { return url ? url->host.c_str() : ""; }
int         aurora_url_port(AuroraUrl* url) { return url ? url->port : 0; }
const char* aurora_url_path(AuroraUrl* url) { return url ? url->path.c_str() : ""; }
const char* aurora_url_query(AuroraUrl* url) { return url ? url->query.c_str() : ""; }
const char* aurora_url_fragment(AuroraUrl* url) { return url ? url->fragment.c_str() : ""; }
const char* aurora_url_userinfo(AuroraUrl* url) { return url ? url->userinfo.c_str() : ""; }

char* aurora_url_build(const char* scheme, const char* host, int port,
                       const char* path, const char* query, const char* fragment) {
    std::string result;
    if (scheme && strlen(scheme) > 0) {
        result += scheme;
        result += "://";
    }
    if (host && strlen(host) > 0) {
        result += host;
        if (port > 0) {
            char port_str[16];
            snprintf(port_str, sizeof(port_str), ":%d", port);
            result += port_str;
        }
    }
    if (path && strlen(path) > 0) {
        if (result.empty() || result.back() != '/') {
            if (path[0] != '/') result += '/';
        }
        result += path;
    } else if (!result.empty()) {
        result += '/';
    }
    if (query && strlen(query) > 0) {
        result += '?';
        result += query;
    }
    if (fragment && strlen(fragment) > 0) {
        result += '#';
        result += fragment;
    }
    return strdup(result.c_str());
}

static bool is_unreserved(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
}

int aurora_url_encode(const char* input, char* out, int out_size) {
    if (!input || !out || out_size <= 0) return 0;
    int written = 0;
    for (const char* p = input; *p && written < out_size - 1; p++) {
        unsigned char c = (unsigned char)*p;
        if (is_unreserved((char)c)) {
            out[written++] = (char)c;
        } else if (c == ' ') {
            out[written++] = '+';
        } else {
            if (written + 3 >= out_size) break;
            snprintf(out + written, 4, "%%%02X", c);
            written += 3;
        }
    }
    out[written] = 0;
    return written;
}

int aurora_url_decode(const char* input, char* out, int out_size) {
    if (!input || !out || out_size <= 0) return 0;
    int written = 0;
    for (const char* p = input; *p && written < out_size - 1; p++) {
        if (*p == '%' && *(p+1) && *(p+2)) {
            char hex[3] = { p[1], p[2], 0 };
            out[written++] = (char)strtol(hex, nullptr, 16);
            p += 2;
        } else if (*p == '+') {
            out[written++] = ' ';
        } else {
            out[written++] = *p;
        }
    }
    out[written] = 0;
    return written;
}

}
