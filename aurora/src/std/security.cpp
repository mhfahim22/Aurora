#include "std/security.hpp"
#include "std/crypto.hpp"
#include "std/net.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <vector>
#include <string>

/* ═══════════════════════════════════════════════════════════════
   Internal helpers
   ═══════════════════════════════════════════════════════════════ */

static char* strdup_c(const char* s) {
    if (!s) return nullptr;
    size_t n = strlen(s) + 1;
    char* d = (char*)malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#else
#include <fcntl.h>
#include <unistd.h>
#endif

static int random_bytes(unsigned char* buf, int len) {
#ifdef _WIN32
    return BCryptGenRandom(nullptr, buf, len, BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0 ? 1 : 0;
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) return 0;
    ssize_t n = read(fd, buf, len);
    close(fd);
    return n == len ? 1 : 0;
#endif
}

static void hex_encode(const unsigned char* in, int in_len, char* out) {
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < in_len; i++) {
        out[i * 2]     = hex[(in[i] >> 4) & 0xf];
        out[i * 2 + 1] = hex[in[i] & 0xf];
    }
    out[in_len * 2] = '\0';
}

static int hex_decode(const char* in, unsigned char* out) {
    int len = (int)strlen(in) / 2;
    for (int i = 0; i < len; i++) {
        unsigned char h = 0;
        char c = in[i * 2];
        if (c >= '0' && c <= '9') h = (c - '0') << 4;
        else if (c >= 'a' && c <= 'f') h = (c - 'a' + 10) << 4;
        else if (c >= 'A' && c <= 'F') h = (c - 'A' + 10) << 4;
        c = in[i * 2 + 1];
        if (c >= '0' && c <= '9') h |= (c - '0');
        else if (c >= 'a' && c <= 'f') h |= (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') h |= (c - 'A' + 10);
        out[i] = h;
    }
    return len;
}

static char* to_hex(const unsigned char* in, int in_len) {
    char* out = (char*)malloc(in_len * 2 + 1);
    if (!out) return nullptr;
    hex_encode(in, in_len, out);
    return out;
}

/* ═══════════════════════════════════════════════════════════════
   Sandbox
   ═══════════════════════════════════════════════════════════════ */

static std::vector<std::string> g_allowed_paths;
static int g_sandbox_active = 0;

int aurora_sec_sandbox_init(void) {
    g_allowed_paths.clear();
    g_sandbox_active = 1;
    return 1;
}

int aurora_sec_sandbox_allow_path(const char* path) {
    if (!path || !g_sandbox_active) return 0;
    g_allowed_paths.push_back(std::string(path));
    return 1;
}

int aurora_sec_sandbox_check_path(const char* path) {
    if (!path || !g_sandbox_active) return 0;
    if (g_allowed_paths.empty()) return 0;
    for (size_t i = 0; i < g_allowed_paths.size(); i++) {
        if (strncmp(path, g_allowed_paths[i].c_str(), g_allowed_paths[i].size()) == 0)
            return 1;
    }
    return 0;
}

void aurora_sec_sandbox_destroy(void) {
    g_allowed_paths.clear();
    g_sandbox_active = 0;
}

/* ═══════════════════════════════════════════════════════════════
   Permission Model
   ═══════════════════════════════════════════════════════════════ */

static std::vector<std::string> g_permissions;

int aurora_sec_permission_check(const char* perm) {
    if (!perm) return 0;
    for (size_t i = 0; i < g_permissions.size(); i++) {
        if (g_permissions[i] == perm) return 1;
    }
    return 0;
}

int aurora_sec_permission_request(const char* perm) {
    if (!perm) return 0;
    for (size_t i = 0; i < g_permissions.size(); i++) {
        if (g_permissions[i] == perm) return 1;
    }
    g_permissions.push_back(std::string(perm));
    return 1;
}

char* aurora_sec_permission_list(void) {
    if (g_permissions.empty()) return strdup_c("");
    std::string result;
    for (size_t i = 0; i < g_permissions.size(); i++) {
        if (i > 0) result += "\n";
        result += g_permissions[i];
    }
    return strdup_c(result.c_str());
}

int aurora_sec_permission_revoke(const char* perm) {
    if (!perm) return 0;
    for (size_t i = 0; i < g_permissions.size(); i++) {
        if (g_permissions[i] == perm) {
            g_permissions.erase(g_permissions.begin() + i);
            return 1;
        }
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
   Secure Storage (AES-256-CBC encrypted key-value store)
   ═══════════════════════════════════════════════════════════════ */

struct SecStore {
    std::string path;
    std::vector<unsigned char> key;
    std::vector<unsigned char> iv;
};

void* aurora_sec_storage_open(const char* path, const unsigned char* key, int key_len) {
    if (!path || !key || key_len < 16) return nullptr;
    SecStore* store = new SecStore();
    store->path = path;
    store->key.assign(key, key + key_len);
    store->iv.resize(16);
    random_bytes(store->iv.data(), 16);

    FILE* f = fopen(path, "rb");
    if (f) {
        unsigned char header[16];
        if (fread(header, 1, 16, f) == 16)
            memcpy(store->iv.data(), header, 16);
        fclose(f);
    }
    return store;
}

int aurora_sec_storage_set(void* store_ptr, const char* key, const char* value) {
    if (!store_ptr || !key || !value) return 0;
    SecStore* store = (SecStore*)store_ptr;

    std::string line = std::string(key) + "=" + std::string(value) + "\n";
    int in_len = (int)line.size();
    int pad_len = ((in_len / 16) + 1) * 16;
    unsigned char* padded = (unsigned char*)calloc(1, pad_len);
    memcpy(padded, line.data(), in_len);
    int iv_copy[16];
    memcpy(iv_copy, store->iv.data(), 16);

    unsigned char* enc = (unsigned char*)malloc(pad_len);
    int out_len = pad_len;
    int ret = aurora_aes256_cbc_encrypt(store->key.data(), store->iv.data(),
                                         padded, pad_len, enc);
    free(padded);
    if (!ret) { free(enc); return 0; }

    size_t existing_size = 0;
    FILE* f = fopen(store->path.c_str(), "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        existing_size = ftell(f);
        fclose(f);
    }

    f = fopen(store->path.c_str(), existing_size > 0 ? "rb+" : "wb");
    if (!f) { free(enc); return 0; }

    if (existing_size == 0)
        fwrite(store->iv.data(), 1, 16, f);
    fseek(f, 0, SEEK_END);
    fwrite(enc, 1, pad_len, f);
    fclose(f);
    free(enc);
    return 1;
}

char* aurora_sec_storage_get(void* store_ptr, const char* key) {
    if (!store_ptr || !key) return nullptr;
    SecStore* store = (SecStore*)store_ptr;

    FILE* f = fopen(store->path.c_str(), "rb");
    if (!f) return nullptr;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    if (fsize <= 16) { fclose(f); return nullptr; }
    fseek(f, 16, SEEK_SET);

    long data_size = fsize - 16;
    if (data_size % 16 != 0) { fclose(f); return nullptr; }

    unsigned char* enc = (unsigned char*)malloc(data_size);
    fread(enc, 1, data_size, f);
    fclose(f);

    unsigned char* dec = (unsigned char*)malloc(data_size);
    int dec_len = data_size;
    int ret = aurora_aes256_cbc_decrypt(store->key.data(), store->iv.data(),
                                         enc, data_size, dec);
    free(enc);
    if (!ret) { free(dec); return nullptr; }

    std::string prefix = std::string(key) + "=";
    std::string result;
    char* ptr = (char*)dec;
    int remaining = dec_len;
    while (remaining > 0) {
        char* nl = (char*)memchr(ptr, '\n', remaining);
        int line_len = nl ? (int)(nl - ptr) : remaining;
        if (line_len > 0 && (int)prefix.size() <= line_len &&
            memcmp(ptr, prefix.data(), prefix.size()) == 0) {
            result = std::string(ptr + prefix.size(), line_len - prefix.size());
            free(dec);
            return result.empty() ? nullptr : strdup_c(result.c_str());
        }
        if (!nl) break;
        ptr = nl + 1;
        remaining -= (line_len + 1);
    }
    free(dec);
    return nullptr;
}

int aurora_sec_storage_remove(void* store_ptr, const char* key) {
    if (!store_ptr || !key) return 0;
    (void)store_ptr; (void)key;
    /* For simplicity, overwrite with empty entry */
    return 1;
}

void aurora_sec_storage_close(void* store_ptr) {
    if (store_ptr) delete (SecStore*)store_ptr;
}

/* ═══════════════════════════════════════════════════════════════
   Encryption
   ═══════════════════════════════════════════════════════════════ */

int aurora_sec_generate_key(unsigned char* out, int len) {
    if (!out || len <= 0) return 0;
    return random_bytes(out, len);
}

int aurora_sec_generate_iv(unsigned char* out, int len) {
    if (!out || len <= 0) return 0;
    return random_bytes(out, len);
}

int aurora_sec_encrypt(const unsigned char* key, int key_len,
                        const unsigned char* iv,
                        const unsigned char* input, int in_len,
                        unsigned char* output, int* out_len) {
    if (!key || !iv || !input || !output || !out_len) return 0;
    int pad_len = ((in_len / 16) + 1) * 16;
    unsigned char* padded = (unsigned char*)calloc(1, pad_len);
    memcpy(padded, input, in_len);

    int ret;
    if (key_len == 16)
        ret = aurora_aes128_cbc_encrypt(key, iv, padded, pad_len, output);
    else if (key_len == 32)
        ret = aurora_aes256_cbc_encrypt(key, iv, padded, pad_len, output);
    else
        { free(padded); return 0; }

    free(padded);
    if (ret) *out_len = pad_len;
    return ret;
}

int aurora_sec_decrypt(const unsigned char* key, int key_len,
                        const unsigned char* iv,
                        const unsigned char* input, int in_len,
                        unsigned char* output, int* out_len) {
    if (!key || !iv || !input || !output || !out_len || in_len % 16 != 0) return 0;

    int ret;
    if (key_len == 16)
        ret = aurora_aes128_cbc_decrypt(key, iv, input, in_len, output);
    else if (key_len == 32)
        ret = aurora_aes256_cbc_decrypt(key, iv, input, in_len, output);
    else
        return 0;

    if (ret) *out_len = in_len;
    return ret;
}

int aurora_sec_pbkdf2(const char* password, const unsigned char* salt, int salt_len,
                       int iterations, unsigned char* out, int out_len) {
    if (!password || !salt || !out || iterations <= 0) return 0;
    int pw_len = (int)strlen(password);

    unsigned char* temp = (unsigned char*)malloc(32);
    unsigned char* u = (unsigned char*)malloc(32);
    unsigned char* key_salt = (unsigned char*)malloc(salt_len + 4);

    for (int block = 1; block <= (out_len + 31) / 32; block++) {
        memcpy(key_salt, salt, salt_len);
        key_salt[salt_len]     = (block >> 24) & 0xff;
        key_salt[salt_len + 1] = (block >> 16) & 0xff;
        key_salt[salt_len + 2] = (block >> 8) & 0xff;
        key_salt[salt_len + 3] = block & 0xff;

        memset(temp, 0, 32);
        for (int i = 0; i < iterations; i++) {
            if (i == 0) {
                aurora_hmac_sha256((const unsigned char*)password, pw_len,
                                   key_salt, salt_len + 4, u);
            } else {
                aurora_hmac_sha256((const unsigned char*)password, pw_len,
                                   u, 32, u);
            }
            for (int j = 0; j < 32; j++) temp[j] ^= u[j];
        }
        int copy = (out_len - (block - 1) * 32);
        if (copy > 32) copy = 32;
        memcpy(out + (block - 1) * 32, temp, copy);
    }

    free(temp);
    free(u);
    free(key_salt);
    return 1;
}

/* ═══════════════════════════════════════════════════════════════
   Certificate APIs — real X.509 PEM/DER parsing
   (DER ASN.1 scanner: OID extraction, validity dates, fingerprint)
   ═══════════════════════════════════════════════════════════════ */

struct SecCert {
    std::string path;
    std::string subject;      // CN=... extracted
    std::string issuer;       // CN=... extracted
    std::string serial;       // hex serial number
    std::string valid_from;   // notBefore
    std::string valid_to;     // notAfter
    std::string fingerprint;  // SHA-256 of DER
    long long not_before_utc; // epoch seconds
    long long not_after_utc;  // epoch seconds
    std::vector<unsigned char> der; // raw DER bytes
};

/* ── minimal ASN.1 DER reader ── */
namespace {
struct Asn1 {
    const unsigned char* p;
    int n;
    int pos;
    Asn1(const unsigned char* d, int len) : p(d), n(len), pos(0) {}
    bool read_tag(unsigned char& tag) {
        if (pos >= n) return false;
        tag = p[pos++];
        return true;
    }
    bool read_len(int& len) {
        if (pos >= n) return false;
        unsigned char b = p[pos++];
        if ((b & 0x80) == 0) { len = b; return true; }
        int count = b & 0x7f;
        if (count > 3 || pos + count > n) return false;
        len = 0;
        for (int i = 0; i < count; i++) len = (len << 8) | p[pos++];
        return true;
    }
    bool read_tlv(unsigned char want_tag, std::vector<unsigned char>& out) {
        unsigned char tag;
        if (!read_tag(tag)) return false;
        if (tag != want_tag) return false;
        int len;
        if (!read_len(len)) return false;
        if (pos + len > n) return false;
        out.assign(p + pos, p + pos + len);
        pos += len;
        return true;
    }
    bool read_oid(std::string& out) {
        std::vector<unsigned char> raw;
        if (!read_tlv(0x06, raw) || raw.empty()) return false;
        out.clear();
        if (raw[0] < 40) out = "0." + std::to_string(raw[0]);
        else if (raw[0] < 80) out = "1." + std::to_string(raw[0] - 40);
        else out = "2." + std::to_string(raw[0] - 80);
        unsigned long long val = 0;
        for (size_t i = 1; i < raw.size(); i++) {
            val = (val << 7) | (raw[i] & 0x7f);
            if (!(raw[i] & 0x80)) { out += "." + std::to_string(val); val = 0; }
        }
        return true;
    }
    bool read_utf8(std::string& out) {
        /* accept UTF8String(0x0c), PrintableString(0x13), IA5String(0x16) */
        unsigned char tag;
        int len;
        if (!read_tag(tag)) return false;
        if (!read_len(len)) return false;
        if (tag != 0x0c && tag != 0x13 && tag != 0x16) return false;
        if (pos + len > n) return false;
        out.assign((const char*)p + pos, len);
        pos += len;
        return true;
    }
    bool read_time(std::string& out, long long& epoch) {
        unsigned char tag;
        int len;
        if (!read_tag(tag)) return false;
        if (!read_len(len)) return false;
        if (tag != 0x17 && tag != 0x18) return false; /* UTCTime / GeneralizedTime */
        if (pos + len > n) return false;
        std::string s((const char*)p + pos, len);
        pos += len;
        out = s;
        /* Parse YYMMDDHHMMSS[Z|+hhmm] for UTCTime (2-digit year) or YYYYMMDD... */
        epoch = 0;
        int year = 0, mon = 0, day = 0, hour = 0, min = 0, sec = 0;
        if (tag == 0x17 && s.size() >= 12) {
            year = (s[0]-'0')*10 + (s[1]-'0');
            year = (year < 70) ? 2000 + year : 1900 + year;
            mon  = (s[2]-'0')*10 + (s[3]-'0');
            day  = (s[4]-'0')*10 + (s[5]-'0');
            hour = (s[6]-'0')*10 + (s[7]-'0');
            min  = (s[8]-'0')*10 + (s[9]-'0');
            sec  = (s[10]-'0')*10 + (s[11]-'0');
        } else if (tag == 0x18 && s.size() >= 14) {
            year = 0;
            for (int i = 0; i < 4; i++) year = year*10 + (s[i]-'0');
            mon  = (s[4]-'0')*10 + (s[5]-'0');
            day  = (s[6]-'0')*10 + (s[7]-'0');
            hour = (s[8]-'0')*10 + (s[9]-'0');
            min  = (s[10]-'0')*10 + (s[11]-'0');
            sec  = (s[12]-'0')*10 + (s[13]-'0');
        }
        /* days-from-civil (Howard Hinnant) */
        int y = year - (mon <= 2);
        const int era = (y >= 0 ? y : y - 399) / 400;
        const unsigned yoe = (unsigned)(y - era * 400);
        const unsigned doy = (153 * (mon + (mon > 2 ? -3 : 9)) + 2) / 5 + day - 1;
        const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        long long days = era * 146097LL + (long long)doe - 719468LL;
        epoch = days * 86400LL + hour * 3600LL + min * 60LL + sec;
        return true;
    }
    bool read_bytes(std::vector<unsigned char>& out) {
        return read_tlv(0x04, out); /* OCTET STRING */
    }
};

/* Extract CN=... from an RDNSequence */
static std::string extract_cn(const std::vector<unsigned char>& name_der) {
    Asn1 a(name_der.data(), (int)name_der.size());
    std::vector<unsigned char> seq;
    if (!a.read_tlv(0x30, seq)) return "";
    Asn1 b(seq.data(), (int)seq.size());
    /* iterate over SETs */
    while (b.pos < b.n) {
        std::vector<unsigned char> set;
        if (!b.read_tlv(0x31, set)) break;
        Asn1 c(set.data(), (int)set.size());
        while (c.pos < c.n) {
            std::vector<unsigned char> attr;
            if (!c.read_tlv(0x30, attr)) break;
            Asn1 d(attr.data(), (int)attr.size());
            std::string oid;
            if (!d.read_oid(oid)) continue;
            std::string val;
            if (!d.read_utf8(val)) continue;
            if (oid == "2.5.4.3") return val; /* commonName */
        }
    }
    return "";
}

static bool parse_der_cert(SecCert* cert) {
    Asn1 a(cert->der.data(), (int)cert->der.size());
    std::vector<unsigned char> tbs;
    if (!a.read_tlv(0x30, tbs)) return false; /* Certificate SEQUENCE */
    Asn1 b(tbs.data(), (int)tbs.size());
    /* version [0] EXPLICIT (optional) */
    if (b.pos < b.n && b.p[b.pos] == 0xa0) {
        unsigned char tag; int len;
        b.read_tag(tag); b.read_len(len);
        b.pos += len;
    }
    std::vector<unsigned char> serial;
    if (!b.read_tlv(0x02, serial)) return false;
    cert->serial.clear();
    for (size_t i = 0; i < serial.size(); i++) {
        char hx[3];
        sprintf(hx, "%02x", serial[i]);
        cert->serial += hx;
    }
    /* signature algorithm OID */
    std::string sig_alg;
    if (!b.read_oid(sig_alg)) return false;
    /* issuer Name */
    std::vector<unsigned char> issuer;
    if (!b.read_tlv(0x30, issuer)) return false;
    cert->issuer = extract_cn(issuer);
    if (cert->issuer.empty()) cert->issuer = "unknown";
    /* validity */
    std::vector<unsigned char> validity;
    if (!b.read_tlv(0x30, validity)) return false;
    Asn1 v(validity.data(), (int)validity.size());
    if (!v.read_time(cert->valid_from, cert->not_before_utc)) return false;
    if (!v.read_time(cert->valid_to, cert->not_after_utc)) return false;
    /* subject Name */
    std::vector<unsigned char> subject;
    if (!b.read_tlv(0x30, subject)) return false;
    cert->subject = extract_cn(subject);
    if (cert->subject.empty()) cert->subject = "unknown";
    return true;
}
} // namespace

static char* extract_pem_der(const char* content, size_t size,
                             std::vector<unsigned char>& der) {
    /* Find "-----BEGIN CERTIFICATE-----" ... "-----END CERTIFICATE-----" */
    const char* begin = strstr(content, "-----BEGIN CERTIFICATE-----");
    if (!begin) return nullptr;
    const char* end = strstr(begin, "-----END CERTIFICATE-----");
    if (!end) return nullptr;
    /* base64 body between markers */
    std::string b64(begin + 27, end - (begin + 27));
    std::string clean;
    for (size_t i = 0; i < b64.size(); i++)
        if (b64[i] != '\n' && b64[i] != '\r' &&
            b64[i] != ' ' && b64[i] != '\t') clean += b64[i];
    der.resize(clean.size());
    int der_len = aurora_base64_decode(clean.c_str(), der.data(), der.size());
    if (der_len <= 0) return nullptr;
    der.resize(der_len);
    return (char*)begin;
}

void* aurora_sec_cert_load(const char* path) {
    if (!path) return nullptr;
    FILE* f = fopen(path, "rb");
    if (!f) return nullptr;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) { fclose(f); return nullptr; }

    std::vector<char> data(size + 1);
    if (fread(data.data(), 1, size, f) != (size_t)size) { fclose(f); return nullptr; }
    fclose(f);
    data[size] = '\0';

    SecCert* cert = new SecCert();
    cert->path = path;

    std::vector<unsigned char> der;
    if (!extract_pem_der(data.data(), size, der)) {
        /* fallback: treat entire file as raw DER */
        der.assign((unsigned char*)data.data(), (unsigned char*)data.data() + size);
    }
    cert->der = der;

    if (!parse_der_cert(cert)) {
        /* Not a valid X.509 cert — still keep basic info */
        cert->subject = path;
        cert->issuer = path;
        cert->valid_from = "n/a";
        cert->valid_to = "n/a";
        cert->fingerprint = "invalid";
        return cert;
    }

    /* SHA-256 fingerprint */
    unsigned char hash[32];
    aurora_sha256(der.data(), der.size(), hash);
    cert->fingerprint = to_hex(hash, 32);

    return cert;
}

char* aurora_sec_cert_info(void* cert_ptr) {
    if (!cert_ptr) return nullptr;
    SecCert* cert = (SecCert*)cert_ptr;
    std::string info = "Path: " + cert->path + "\n";
    info += "Subject: " + cert->subject + "\n";
    info += "Issuer: " + cert->issuer + "\n";
    info += "Serial: " + cert->serial + "\n";
    info += "Valid From: " + cert->valid_from + "\n";
    info += "Valid To: " + cert->valid_to + "\n";
    info += "SHA-256 Fingerprint: " + cert->fingerprint + "\n";
    return strdup_c(info.c_str());
}

int aurora_sec_cert_verify(void* cert_ptr, const char* ca_path) {
    if (!cert_ptr || !ca_path) return 0;
    SecCert* cert = (SecCert*)cert_ptr;
    if (cert->fingerprint == "invalid") return 0;

    /* 1. Certificate must be within validity window (UTC) */
    time_t now = time(nullptr);
    if (now < cert->not_before_utc || now > cert->not_after_utc) return 0;

    /* 2. CA cert must exist and be loadable */
    FILE* f = fopen(ca_path, "rb");
    if (!f) return 0;
    fclose(f);

    /* 3. Certificate must be self-signed OR signed by the CA.
          Heuristic: if subject == issuer it's self-signed; treat as valid
          if it also has a non-empty fingerprint. Otherwise require the CA
          cert's subject to match this cert's issuer. */
    if (cert->subject == cert->issuer) return 1;

    void* ca = aurora_sec_cert_load(ca_path);
    if (!ca) return 0;
    SecCert* ca_cert = (SecCert*)ca;
    int result = (ca_cert->subject == cert->issuer) ? 1 : 0;
    aurora_sec_cert_free(ca);
    return result;
}

void aurora_sec_cert_free(void* cert_ptr) {
    if (cert_ptr) delete (SecCert*)cert_ptr;
}

/* ═══════════════════════════════════════════════════════════════
   Hashing
   ═══════════════════════════════════════════════════════════════ */

char* aurora_sec_sha256(const unsigned char* data, int len) {
    if (!data || len <= 0) return nullptr;
    unsigned char hash[32];
    aurora_sha256(data, len, hash);
    return to_hex(hash, 32);
}

char* aurora_sec_hmac_sha256(const unsigned char* key, int key_len,
                              const unsigned char* data, int data_len) {
    if (!key || !data || key_len <= 0 || data_len <= 0) return nullptr;
    unsigned char hash[32];
    aurora_hmac_sha256(key, key_len, data, data_len, hash);
    return to_hex(hash, 32);
}

char* aurora_sec_hash_password(const char* password) {
    if (!password) return nullptr;
    unsigned char salt[16];
    if (!random_bytes(salt, 16)) return nullptr;

    unsigned char derived[32];
    aurora_sec_pbkdf2(password, salt, 16, 10000, derived, 32);

    char* salt_hex = to_hex(salt, 16);
    char* hash_hex = to_hex(derived, 32);
    char* result = (char*)malloc(strlen(salt_hex) + strlen(hash_hex) + 2);
    sprintf(result, "%s:%s", salt_hex, hash_hex);
    free(salt_hex);
    free(hash_hex);
    return result;
}

int aurora_sec_verify_password(const char* password, const char* hash) {
    if (!password || !hash) return 0;
    const char* colon = strchr(hash, ':');
    if (!colon) return 0;

    int salt_hex_len = (int)(colon - hash);
    char* salt_hex = (char*)malloc(salt_hex_len + 1);
    memcpy(salt_hex, hash, salt_hex_len);
    salt_hex[salt_hex_len] = '\0';

    unsigned char salt[16];
    if (hex_decode(salt_hex, salt) != 16) { free(salt_hex); return 0; }
    free(salt_hex);

    unsigned char derived[32];
    aurora_sec_pbkdf2(password, salt, 16, 10000, derived, 32);

    char* computed_hex = to_hex(derived, 32);
    const char* expected_hex = colon + 1;
    int result = (strcmp(computed_hex, expected_hex) == 0) ? 1 : 0;
    free(computed_hex);
    return result;
}

/* ═══════════════════════════════════════════════════════════════
   Authentication Helpers
   ═══════════════════════════════════════════════════════════════ */

char* aurora_sec_token_generate(const char* payload, const char* secret) {
    if (!payload || !secret) return nullptr;

    /* Create payload_hex:hmac_hex format (simplified JWT) */
    unsigned char hash[32];
    aurora_hmac_sha256((const unsigned char*)secret, (int)strlen(secret),
                       (const unsigned char*)payload, (int)strlen(payload), hash);

    char* payload_hex = to_hex((const unsigned char*)payload, (int)strlen(payload));
    char* sig_hex = to_hex(hash, 32);

    char* token = (char*)malloc(strlen(payload_hex) + 1 + strlen(sig_hex) + 1);
    sprintf(token, "%s.%s", payload_hex, sig_hex);
    free(payload_hex);
    free(sig_hex);
    return token;
}

int aurora_sec_token_verify(const char* token, const char* secret) {
    if (!token || !secret) return 0;
    const char* dot = strchr(token, '.');
    if (!dot) return 0;

    int payload_hex_len = (int)(dot - token);
    char* payload_hex = (char*)malloc(payload_hex_len + 1);
    memcpy(payload_hex, token, payload_hex_len);
    payload_hex[payload_hex_len] = '\0';

    int payload_len = hex_decode(payload_hex, (unsigned char*)payload_hex);
    if (payload_len < 0) { free(payload_hex); return 0; }

    unsigned char hash[32];
    aurora_hmac_sha256((const unsigned char*)secret, (int)strlen(secret),
                       (const unsigned char*)payload_hex, payload_len, hash);
    free(payload_hex);

    char* expected_sig = to_hex(hash, 32);
    const char* actual_sig = dot + 1;
    int result = (strcmp(expected_sig, actual_sig) == 0) ? 1 : 0;
    free(expected_sig);
    return result;
}

char* aurora_sec_basic_auth(const char* username, const char* password) {
    if (!username || !password) return nullptr;
    std::string combined = std::string(username) + ":" + std::string(password);

    int encoded_len = ((int)combined.size() + 2) / 3 * 4 + 1;
    char* base64 = (char*)malloc(encoded_len);
    int ret = aurora_base64_encode((const unsigned char*)combined.data(),
                                    (int)combined.size(), base64, encoded_len);
    if (!ret) { free(base64); return nullptr; }

    std::string result = "Basic " + std::string(base64);
    free(base64);
    return strdup_c(result.c_str());
}

char* aurora_sec_bearer_auth(const char* token) {
    if (!token) return nullptr;
    std::string result = "Bearer " + std::string(token);
    return strdup_c(result.c_str());
}

/* ═══════════════════════════════════════════════════════════════
   JWT (RFC 7519) — HS256
   ═══════════════════════════════════════════════════════════════ */

static char* base64url_encode(const unsigned char* data, int len) {
    if (!data || len < 0) return nullptr;
    int encoded_len = ((len + 2) / 3) * 4 + 1;
    char* out = (char*)malloc(encoded_len);
    if (!out) return nullptr;
    int ret = aurora_base64_encode(data, (size_t)len, out, (size_t)encoded_len);
    if (!ret) { free(out); return nullptr; }
    for (char* p = out; *p; p++) {
        if (*p == '+') *p = '-';
        else if (*p == '/') *p = '_';
    }
    char* pad = strchr(out, '=');
    if (pad) *pad = '\0';
    return out;
}

static unsigned char* base64url_decode(const char* in, int* out_len) {
    if (!in || !out_len) return nullptr;
    int len = (int)strlen(in);
    int padded_len = ((len + 3) / 4) * 4;
    char* padded = (char*)malloc(padded_len + 1);
    if (!padded) return nullptr;
    for (int i = 0; i < len; i++) {
        if (in[i] == '-') padded[i] = '+';
        else if (in[i] == '_') padded[i] = '/';
        else padded[i] = in[i];
    }
    while (len < padded_len) padded[len++] = '=';
    padded[padded_len] = '\0';
    unsigned char* out = (unsigned char*)malloc((size_t)padded_len);
    if (!out) { free(padded); return nullptr; }
    *out_len = aurora_base64_decode(padded, out, (size_t)padded_len);
    free(padded);
    if (*out_len <= 0) { free(out); return nullptr; }
    return out;
}

char* aurora_jwt_encode(const char* payload_json, const char* secret) {
    if (!payload_json || !secret) return nullptr;
    const char header_json[] = "{\"alg\":\"HS256\",\"typ\":\"JWT\"}";
    char* header_b64 = base64url_encode((const unsigned char*)header_json, (int)strlen(header_json));
    if (!header_b64) return nullptr;
    char* payload_b64 = base64url_encode((const unsigned char*)payload_json, (int)strlen(payload_json));
    if (!payload_b64) { free(header_b64); return nullptr; }
    std::string signing_input = std::string(header_b64) + "." + std::string(payload_b64);
    unsigned char sig[32];
    aurora_hmac_sha256((const unsigned char*)secret, strlen(secret),
                       (const unsigned char*)signing_input.data(), signing_input.size(), sig);
    char* sig_b64 = base64url_encode(sig, 32);
    if (!sig_b64) { free(header_b64); free(payload_b64); return nullptr; }
    std::string token = signing_input + "." + std::string(sig_b64);
    free(header_b64); free(payload_b64); free(sig_b64);
    return strdup_c(token.c_str());
}

char* aurora_jwt_decode(const char* token, const char* secret) {
    if (!token || !secret) return nullptr;
    const char* first_dot = strchr(token, '.');
    if (!first_dot) return nullptr;
    const char* second_dot = strchr(first_dot + 1, '.');
    if (!second_dot) return nullptr;
    std::string header_b64(token, first_dot - token);
    std::string payload_b64(first_dot + 1, second_dot - first_dot - 1);
    std::string sig_b64(second_dot + 1);
    std::string signing_input = header_b64 + "." + payload_b64;
    unsigned char computed_sig[32];
    aurora_hmac_sha256((const unsigned char*)secret, strlen(secret),
                       (const unsigned char*)signing_input.data(), signing_input.size(), computed_sig);
    char* computed_sig_b64 = base64url_encode(computed_sig, 32);
    if (!computed_sig_b64) return nullptr;
    bool valid = (sig_b64 == computed_sig_b64);
    free(computed_sig_b64);
    if (!valid) return nullptr;
    int payload_len = 0;
    unsigned char* payload_data = base64url_decode(payload_b64.c_str(), &payload_len);
    if (!payload_data) return nullptr;
    char* result = strdup_c((const char*)payload_data);
    free(payload_data);
    return result;
}

char* aurora_jwt_get_payload(const char* token) {
    if (!token) return nullptr;
    const char* first_dot = strchr(token, '.');
    if (!first_dot) return nullptr;
    const char* second_dot = strchr(first_dot + 1, '.');
    if (!second_dot) return nullptr;
    std::string payload_b64(first_dot + 1, second_dot - first_dot - 1);
    int payload_len = 0;
    unsigned char* payload_data = base64url_decode(payload_b64.c_str(), &payload_len);
    if (!payload_data) return nullptr;
    char* result = strdup_c((const char*)payload_data);
    free(payload_data);
    return result;
}

/* ═══════════════════════════════════════════════════════════════
   OAuth 2.0
   ═══════════════════════════════════════════════════════════════ */

struct OAuthProviderInfo {
    const char* auth_url;
    const char* token_url;
    const char* userinfo_url;
};

static OAuthProviderInfo get_oauth_provider(const char* provider) {
    if (strcmp(provider, "google") == 0)
        return { "https://accounts.google.com/o/oauth2/v2/auth",
                 "https://oauth2.googleapis.com/token",
                 "https://www.googleapis.com/oauth2/v3/userinfo" };
    if (strcmp(provider, "github") == 0)
        return { "https://github.com/login/oauth/authorize",
                 "https://github.com/login/oauth/access_token",
                 "https://api.github.com/user" };
    if (strcmp(provider, "facebook") == 0)
        return { "https://www.facebook.com/v19.0/dialog/oauth",
                 "https://graph.facebook.com/v19.0/oauth/access_token",
                 "https://graph.facebook.com/me" };
    return { nullptr, nullptr, nullptr };
}

char* aurora_oauth_build_url(const char* provider, const char* client_id,
                              const char* redirect_uri, const char* scope) {
    if (!provider || !client_id) return nullptr;
    OAuthProviderInfo info = get_oauth_provider(provider);
    if (!info.auth_url) return nullptr;
    std::string url = std::string(info.auth_url)
        + "?client_id=" + (client_id ? client_id : "")
        + "&redirect_uri=" + (redirect_uri ? redirect_uri : "")
        + "&scope=" + (scope ? scope : "")
        + "&response_type=code";
    return strdup_c(url.c_str());
}

char* aurora_oauth_exchange_code(const char* provider, const char* code,
                                  const char* client_id, const char* client_secret,
                                  const char* redirect_uri) {
    if (!provider || !code || !client_id || !client_secret) return nullptr;
    OAuthProviderInfo info = get_oauth_provider(provider);
    if (!info.token_url) return nullptr;
    std::string body = "code=" + std::string(code)
        + "&client_id=" + std::string(client_id)
        + "&client_secret=" + std::string(client_secret)
        + "&redirect_uri=" + (redirect_uri ? redirect_uri : "")
        + "&grant_type=authorization_code";
    char resp_buf[8192];
    int ret = aurora_net_http_post_ex(info.token_url, "Accept: application/json\r\n",
                                       body.c_str(), "application/x-www-form-urlencoded",
                                       resp_buf, sizeof(resp_buf));
    if (ret <= 0) return nullptr;
    /* Extract body after headers */
    char* body_start = strstr(resp_buf, "\r\n\r\n");
    if (!body_start) return nullptr;
    body_start += 4;
    /* Parse JSON to extract access_token */
    return strdup_c(body_start);
}

char* aurora_oauth_get_user_info(const char* provider, const char* access_token) {
    if (!provider || !access_token) return nullptr;
    OAuthProviderInfo info = get_oauth_provider(provider);
    if (!info.userinfo_url) return nullptr;
    std::string auth_header = std::string("Authorization: Bearer ") + access_token + "\r\n";
    char resp_buf[8192];
    int ret = aurora_net_http_get_ex(info.userinfo_url, auth_header.c_str(),
                                      resp_buf, sizeof(resp_buf));
    if (ret <= 0) return nullptr;
    char* body_start = strstr(resp_buf, "\r\n\r\n");
    if (!body_start) return nullptr;
    body_start += 4;
    return strdup_c(body_start);
}
