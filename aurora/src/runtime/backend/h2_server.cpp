#include "runtime/h2_server.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

/* ── Helper ── */

static char* strdup_c(const char* s) {
    if (!s) return nullptr;
    size_t n = strlen(s) + 1;
    char* p = (char*)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static uint32_t read24(const unsigned char* p) {
    return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
}

static void write24(unsigned char* p, uint32_t v) {
    p[0] = (unsigned char)((v >> 16) & 0xff);
    p[1] = (unsigned char)((v >> 8) & 0xff);
    p[2] = (unsigned char)(v & 0xff);
}

/* ── External network helpers ── */
extern "C" {
    int aurora_send_all(int64_t sock, const char* data, int len);
    int aurora_net_tcp_recv(int64_t sock, char* buffer, int buffer_size);
}

/* ═══════════════════════════════════════════════════════════════
   HPACK (simplified — static table only)
   ═══════════════════════════════════════════════════════════════ */

/* HPACK static table (RFC 7541 Appendix A) — first 61 entries */
static const char* h2_static_table_names[] = {
    ":authority", ":method", ":method", ":path", ":path", ":scheme", ":scheme",
    ":status", ":status", ":status", ":status", ":status", ":status",
    "accept-charset", "accept-encoding", "accept-language", "accept-ranges",
    "accept", "access-control-allow-origin", "age", "allow", "authorization",
    "cache-control", "content-disposition", "content-encoding", "content-language",
    "content-length", "content-location", "content-range", "content-type",
    "cookie", "date", "etag", "expect", "expires", "from", "host",
    "if-match", "if-modified-since", "if-none-match", "if-range",
    "if-unmodified-since", "last-modified", "link", "location",
    "max-forwards", "proxy-authenticate", "proxy-authorization", "range",
    "referer", "refresh", "retry-after", "server", "set-cookie",
    "strict-transport-security", "transfer-encoding", "user-agent",
    "vary", "via", "www-authenticate"
};

static const char* h2_static_table_values[] = {
    "", "GET", "POST", "/", "/index.html", "http", "https",
    "200", "204", "206", "304", "400", "404", "500",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", ""
};

/* Simplified HPACK encoding — literal with indexing, no Huffman */
static int hpack_encode_literal(unsigned char* out, int out_size, const char* name, const char* value) {
    int pos = 0;
    /* Check static table for common headers */
    int table_idx = -1;
    for (int i = 0; i < 61; i++) {
        if (strcmp(h2_static_table_names[i], name) == 0) {
            table_idx = i + 1;
            break;
        }
    }
    if (table_idx > 0) {
        /* Indexed header field (7-bit prefix, 0x80) */
        /* Use indexed name + literal value: 01xxxxxx */
        if (pos + 1 > out_size) return -1;
        out[pos] = (unsigned char)(0x40 | (table_idx & 0x3f)); /* 01 + 6-bit index (handle >63 separately) */
        pos++;
    } else {
        /* Literal with incremental indexing — new name (0x40, no index) */
        if (pos + 1 > out_size) return -1;
        out[pos++] = 0x40; /* 01 000000 = literal, incremental indexing, new name */
        /* Encode name length (no Huffman) */
        int name_len = (int)strlen(name);
        if (pos + 1 > out_size) return -1;
        if (name_len < 127) {
            out[pos++] = (unsigned char)name_len;
        } else {
            out[pos++] = 0x7f;
            /* integer encoding with prefix 0x7f would need more bytes; skip for simplicity */
        }
        if (pos + name_len > out_size) return -1;
        memcpy(out + pos, name, (size_t)name_len);
        pos += name_len;
    }
    /* Encode value length */
    int value_len = (int)strlen(value);
    if (pos + 1 > out_size) return -1;
    if (value_len < 127) {
        out[pos++] = (unsigned char)value_len;
    } else {
        out[pos++] = 0x7f;
        /* Would need continuation byte(s) for >127 — skip for simplicity */
    }
    if (pos + value_len > out_size) return -1;
    memcpy(out + pos, value, (size_t)value_len);
    pos += value_len;
    return pos;
}

/* Simplified HPACK decoding */
static int hpack_decode_one(const unsigned char* in, int in_len, int* consumed,
                             char* name, int name_size, char* value, int value_size) {
    if (in_len < 1) return -1;
    *consumed = 0;
    int idx = 0;
    unsigned char b = in[0];
    if ((b & 0x80) == 0x80) {
        /* Indexed header field */
        idx = b & 0x7f;
        (*consumed)++;
        /* Would handle extended index here */
    } else if ((b & 0xc0) == 0x40) {
        /* Literal with incremental indexing */
        idx = b & 0x3f;
        (*consumed)++;
        if (idx == 0x3f) { /* extended index, skip for simplicity */
            if (*consumed >= in_len) return -1;
            idx += in[(*consumed)++];
        }
        /* Decode name */
        if (idx > 0 && idx <= 61) {
            strncpy(name, h2_static_table_names[idx - 1], (size_t)name_size - 1);
            name[name_size - 1] = '\0';
        } else {
            /* Literal name — read length then bytes */
            if (*consumed >= in_len) return -1;
            int name_len = in[(*consumed)++];
            if ((name_len & 0x80) == 0x80) {
                /* Huffman — skip for now, treat as no-match */
                name[0] = '\0';
                return -1;
            }
            if (*consumed + name_len > in_len) return -1;
            int cp = (name_len < name_size - 1) ? name_len : (name_size - 1);
            memcpy(name, in + *consumed, (size_t)cp);
            name[cp] = '\0';
            *consumed += name_len;
        }
        /* Decode value length */
        if (*consumed >= in_len) return -1;
        int vlen = in[(*consumed)++];
        if ((vlen & 0x80) == 0x80) {
            /* Huffman — skip */
            int actual = vlen & 0x7f;
            if (*consumed + actual > in_len) return -1;
            *consumed += actual;
            name[0] = '\0';
            return -1;
        }
        if (*consumed + vlen > in_len) return -1;
        int cp = (vlen < value_size - 1) ? vlen : (value_size - 1);
        memcpy(value, in + *consumed, (size_t)cp);
        value[cp] = '\0';
        *consumed += vlen;
        return 0;
    } else if ((b & 0xe0) == 0x20) {
        /* Dynamic table size update */
        (*consumed)++;
        name[0] = '\0';
        return -1;
    } else {
        /* Literal without indexing or never-indexed */
        (*consumed)++;
        name[0] = '\0';
        return -1;
    }
    /* If indexed, look up in static table */
    if (idx > 0 && idx <= 61) {
        strncpy(name, h2_static_table_names[idx - 1], (size_t)name_size - 1);
        strncpy(value, h2_static_table_values[idx - 1], (size_t)value_size - 1);
        return 0;
    }
    return -1;
}

/* ═══════════════════════════════════════════════════════════════
   HTTP/2 Connection
   ═══════════════════════════════════════════════════════════════ */

struct H2Stream {
    int32_t id;
    int state; /* 0=idle, 1=open, 2=half-closed-local, 3=half-closed-remote, 4=closed */
    std::string header_block;  /* accumulated HEADERS payload */
    std::vector<std::string> req_headers; /* interleaved name/value pairs */
    std::string body;
    int end_stream : 1;
    int32_t window_size;
};

struct AuroraH2Connection {
    int64_t sock;
    int64_t tls_handle;
    int server_mode;
    int is_open;

    /* Settings */
    uint32_t settings_max_concurrent;
    uint32_t settings_initial_window;
    uint32_t settings_max_frame_size;

    /* Remote settings (what peer advertised) */
    uint32_t remote_window;

    /* Streams */
    std::unordered_map<int32_t, H2Stream*> streams;
    int32_t last_stream_id;
    int32_t goaway_last_stream;

    /* Pending input buffer */
    std::string read_buf;

    /* Callback */
    AuroraH2RequestHandler request_handler;
    void* handler_user_data;

    /* HPACK dynamic table (simplified — fixed size ring buffer) */
    struct DynEntry { std::string name; std::string value; };
    std::vector<DynEntry> dynamic_table;
    int dynamic_table_size;
    int dynamic_table_max;
};

AuroraH2Connection* aurora_h2_new(int64_t sock, int64_t tls_handle, int server_mode) {
    AuroraH2Connection* h2 = new AuroraH2Connection();
    if (!h2) return nullptr;
    h2->sock = sock;
    h2->tls_handle = tls_handle;
    h2->server_mode = server_mode;
    h2->is_open = 1;
    h2->settings_max_concurrent = H2_DEFAULT_MAX_CONCURRENT_STREAMS;
    h2->settings_initial_window = H2_DEFAULT_INITIAL_WINDOW_SIZE;
    h2->settings_max_frame_size = H2_DEFAULT_MAX_FRAME_SIZE;
    h2->remote_window = H2_DEFAULT_INITIAL_WINDOW_SIZE;
    h2->last_stream_id = 0;
    h2->goaway_last_stream = 0x7fffffff;
    h2->request_handler = nullptr;
    h2->handler_user_data = nullptr;
    h2->dynamic_table_max = 4096;
    h2->dynamic_table_size = 0;
    return h2;
}

void aurora_h2_free(AuroraH2Connection* h2) {
    if (!h2) return;
    for (auto& pair : h2->streams) delete pair.second;
    h2->streams.clear();
    delete h2;
}

/* ── Raw frame I/O ── */

static int h2_read_raw(AuroraH2Connection* h2, unsigned char* buf, int len) {
    int total = 0;
    while (total < len) {
        /* Check read buffer first */
        if (!h2->read_buf.empty()) {
            int avail = (int)h2->read_buf.size();
            int take = (avail < len - total) ? avail : (len - total);
            memcpy(buf + total, h2->read_buf.data(), (size_t)take);
            h2->read_buf.erase(0, (size_t)take);
            total += take;
            continue;
        }
        char tmp[4096];
        int n = aurora_net_tcp_recv(h2->sock, tmp, sizeof(tmp));
        if (n <= 0) return -1;
        if (total + n <= len) {
            memcpy(buf + total, tmp, (size_t)n);
            total += n;
        } else {
            int needed = len - total;
            memcpy(buf + total, tmp, (size_t)needed);
            h2->read_buf.append(tmp + needed, (size_t)(n - needed));
            total = len;
            break;
        }
    }
    return total;
}

static int h2_write_raw(AuroraH2Connection* h2, const unsigned char* data, int len) {
    int sent = aurora_send_all(h2->sock, (const char*)data, len);
    return sent;
}

/* ── Frame writing ── */

static int h2_write_frame(AuroraH2Connection* h2, uint8_t type, uint8_t flags,
                           uint32_t stream_id, const unsigned char* payload, int payload_len) {
    unsigned char hdr[9];
    write24(hdr, (uint32_t)payload_len);
    hdr[3] = type;
    hdr[4] = flags;
    hdr[5] = (unsigned char)((stream_id >> 24) & 0x7f);
    hdr[6] = (unsigned char)((stream_id >> 16) & 0xff);
    hdr[7] = (unsigned char)((stream_id >> 8) & 0xff);
    hdr[8] = (unsigned char)(stream_id & 0xff);
    if (h2_write_raw(h2, hdr, 9) != 9) return -1;
    if (payload_len > 0 && payload) {
        if (h2_write_raw(h2, payload, payload_len) != payload_len) return -1;
    }
    return 0;
}

/* ── Frame reading ── */

static int h2_read_frame(AuroraH2Connection* h2, uint8_t* type, uint8_t* flags,
                          uint32_t* stream_id, unsigned char* payload, int* payload_len, int max_payload) {
    unsigned char hdr[9];
    if (h2_read_raw(h2, hdr, 9) != 9) return -1;
    *payload_len = (int)read24(hdr);
    *type = hdr[3];
    *flags = hdr[4];
    *stream_id = ((uint32_t)hdr[5] << 24) | ((uint32_t)hdr[6] << 16) |
                  ((uint32_t)hdr[7] << 8) | hdr[8];
    *stream_id &= 0x7fffffff;
    if (*payload_len > max_payload) return -1;
    if (*payload_len > 0) {
        if (h2_read_raw(h2, payload, *payload_len) != *payload_len) return -1;
    }
    return 0;
}

/* ── Connection preface ── */

int aurora_h2_read_preface(AuroraH2Connection* h2) {
    if (!h2) return -1;
    char preface[24];
    if (h2_read_raw(h2, (unsigned char*)preface, 24) != 24) return -1;
    /* Verify: "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n" */
    if (memcmp(preface, "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n", 24) != 0) return -1;
    return 0;
}

int aurora_h2_send_preface(AuroraH2Connection* h2) {
    if (!h2) return -1;
    if (!h2->server_mode) {
        /* Client sends connection preface */
        const char* preface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
        if (h2_write_raw(h2, (const unsigned char*)preface, 24) != 24) return -1;
    }
    /* Send initial SETTINGS frame */
    unsigned char settings_payload[] = {
        0x00, 0x03, 0x00, 0x00, 0x00, 0x64, /* SETTINGS_MAX_CONCURRENT_STREAMS = 100 */
        0x00, 0x04, 0x00, 0x00, 0xff, 0xff, /* SETTINGS_INITIAL_WINDOW_SIZE = 65535 */
    };
    return h2_write_frame(h2, H2_FRAME_SETTINGS, 0x00, 0, settings_payload, sizeof(settings_payload));
}

/* ── Process a single frame ── */

static void h2_process_goaway(AuroraH2Connection* h2, uint32_t last_stream, uint32_t error) {
    h2->goaway_last_stream = last_stream;
    h2->is_open = 0;
}

static void h2_process_settings(AuroraH2Connection* h2, const unsigned char* payload, int len, int is_ack) {
    if (is_ack) return;
    for (int i = 0; i + 5 < len; i += 6) {
        uint16_t id = (uint16_t)((payload[i] << 8) | payload[i + 1]);
        uint32_t val = (uint32_t)(payload[i + 2] << 24) | (payload[i + 3] << 16) |
                       (payload[i + 4] << 8) | payload[i + 5];
        switch (id) {
            case H2_SETTINGS_MAX_CONCURRENT_STREAMS:
                h2->settings_max_concurrent = val; break;
            case H2_SETTINGS_INITIAL_WINDOW_SIZE:
                h2->settings_initial_window = val; break;
            case H2_SETTINGS_MAX_FRAME_SIZE:
                if (val >= 16384 && val <= 16777215)
                    h2->settings_max_frame_size = val;
                break;
        }
    }
    /* Send ACK */
    h2_write_frame(h2, H2_FRAME_SETTINGS, 0x01, 0, nullptr, 0);
}

static void h2_process_window_update(AuroraH2Connection* h2, uint32_t stream_id, uint32_t increment) {
    if (stream_id == 0) {
        h2->remote_window += increment;
    } else {
        auto it = h2->streams.find((int32_t)stream_id);
        if (it != h2->streams.end())
            it->second->window_size += (int32_t)increment;
    }
}

static int h2_process_headers(AuroraH2Connection* h2, const unsigned char* payload, int len,
                               uint8_t flags, uint32_t stream_id) {
    /* Create or find stream */
    auto it = h2->streams.find((int32_t)stream_id);
    H2Stream* stream;
    if (it == h2->streams.end()) {
        stream = new H2Stream();
        stream->id = (int32_t)stream_id;
        stream->state = 1; /* open */
        stream->end_stream = 0;
        stream->window_size = (int32_t)h2->settings_initial_window;
        h2->streams[(int32_t)stream_id] = stream;
        if ((int32_t)stream_id > h2->last_stream_id)
            h2->last_stream_id = (int32_t)stream_id;
    } else {
        stream = it->second;
    }
    if (stream->state == 4) return 0; /* closed stream */

    /* Find the payload start (skip priority if present) */
    int offset = 0;
    if (flags & 0x20) offset += 5; /* PRIORITY flag */

    /* Accumulate header block fragments */
    stream->header_block.append((const char*)payload + offset, (size_t)(len - offset));

    if (flags & 0x01) stream->end_stream = 1;

    /* If END_HEADERS, parse the header block */
    if (flags & 0x04) {
        /* Parse HPACK block */
        const unsigned char* hp = (const unsigned char*)stream->header_block.data();
        int hlen = (int)stream->header_block.size();
        int pos = 0;
        while (pos < hlen) {
            int consumed = 0;
            char n[256], v[256];
            int ret = hpack_decode_one(hp + pos, hlen - pos, &consumed, n, sizeof(n), v, sizeof(v));
            if (ret != 0 || consumed <= 0) { pos++; continue; }
            if (n[0]) {
                stream->req_headers.push_back(strdup_c(n));
                stream->req_headers.push_back(strdup_c(v));
            }
            pos += consumed;
        }
        /* If END_STREAM, invoke handler */
        if (stream->end_stream && h2->request_handler) {
            int hc = (int)stream->req_headers.size() / 2;
            const char** hptr = (const char**)malloc((size_t)hc * 2 * sizeof(char*));
            for (int i = 0; i < hc * 2; i++)
                hptr[i] = stream->req_headers[i].c_str();
            /* Create array of pointers */
            const char** hdr_arr = (const char**)malloc((size_t)(hc * 2 + 1) * sizeof(char*));
            for (int i = 0; i < hc; i++) {
                hdr_arr[i * 2] = stream->req_headers[i * 2].c_str();
                hdr_arr[i * 2 + 1] = stream->req_headers[i * 2 + 1].c_str();
            }
            h2->request_handler(stream->id, hdr_arr, hc,
                                 stream->body.empty() ? nullptr : stream->body.c_str(),
                                 (int)stream->body.size(), h2->handler_user_data);
            free(hptr);
            free(hdr_arr);
        }
    }
    return 0;
}

static int h2_process_data(AuroraH2Connection* h2, const unsigned char* payload, int len,
                            uint8_t flags, uint32_t stream_id) {
    auto it = h2->streams.find((int32_t)stream_id);
    if (it == h2->streams.end()) return 0;
    H2Stream* stream = it->second;
    stream->body.append((const char*)payload, (size_t)len);

    /* Send WINDOW_UPDATE */
    unsigned char wu[4];
    uint32_t inc = (uint32_t)len;
    wu[0] = (unsigned char)((inc >> 24) & 0xff);
    wu[1] = (unsigned char)((inc >> 16) & 0xff);
    wu[2] = (unsigned char)((inc >> 8) & 0xff);
    wu[3] = (unsigned char)(inc & 0xff);
    h2_write_frame(h2, H2_FRAME_WINDOW_UPDATE, 0, 0, wu, 4); /* connection-level */
    h2_write_frame(h2, H2_FRAME_WINDOW_UPDATE, 0, stream_id, wu, 4); /* stream-level */

    if (flags & 0x01) {
        stream->end_stream = 1;
        stream->state = 3; /* half-closed-remote */
    }
    return 0;
}

int aurora_h2_process_frames(AuroraH2Connection* h2) {
    if (!h2 || !h2->is_open) return -1;
    int frame_count = 0;
    while (h2->is_open && frame_count < 1000) {
        uint8_t type, flags;
        uint32_t stream_id;
        unsigned char payload[H2_DEFAULT_MAX_FRAME_SIZE];
        int payload_len;
        int ret = h2_read_frame(h2, &type, &flags, &stream_id, payload, &payload_len, sizeof(payload));
        if (ret < 0) { h2->is_open = 0; return -1; }
        frame_count++;

        switch (type) {
            case H2_FRAME_SETTINGS:
                h2_process_settings(h2, payload, payload_len, flags & 0x01);
                break;
            case H2_FRAME_GOAWAY:
                if (payload_len >= 8) {
                    uint32_t last = ((uint32_t)payload[0] << 24) | (payload[1] << 16) |
                                     (payload[2] << 8) | payload[3];
                    uint32_t err = ((uint32_t)payload[4] << 24) | (payload[5] << 16) |
                                    (payload[6] << 8) | payload[7];
                    h2_process_goaway(h2, last, err);
                }
                break;
            case H2_FRAME_WINDOW_UPDATE:
                if (payload_len >= 4) {
                    uint32_t inc = ((uint32_t)payload[0] << 24) | (payload[1] << 16) |
                                    (payload[2] << 8) | payload[3];
                    h2_process_window_update(h2, stream_id, inc);
                }
                break;
            case H2_FRAME_HEADERS:
            case H2_FRAME_CONTINUATION:
                h2_process_headers(h2, payload, payload_len, flags, stream_id);
                break;
            case H2_FRAME_DATA:
                h2_process_data(h2, payload, payload_len, flags, stream_id);
                break;
            case H2_FRAME_PING:
                if (flags & 0x01) break; /* ignore ACK */
                h2_write_frame(h2, H2_FRAME_PING, 0x01, stream_id, payload, payload_len);
                break;
            case H2_FRAME_RST_STREAM: {
                auto it = h2->streams.find((int32_t)stream_id);
                if (it != h2->streams.end()) {
                    it->second->state = 4;
                    delete it->second;
                    h2->streams.erase(it);
                }
                break;
            }
            case H2_FRAME_PRIORITY:
                break; /* ignore */
            default:
                break;
        }
    }
    return 0;
}

/* ── Response sending ── */

int aurora_h2_send_headers(AuroraH2Connection* h2, int32_t stream_id,
                            const char** headers, int header_count, int end_stream) {
    if (!h2 || !h2->is_open) return -1;
    /* Encode HPACK block */
    unsigned char hpack_buf[8192];
    int hpack_len = 0;
    /* Add :status pseudo-header */
    if (header_count > 0) {
        int n = hpack_encode_literal(hpack_buf + hpack_len, (int)sizeof(hpack_buf) - hpack_len,
                                      ":status", headers[0]);
        if (n > 0) hpack_len += n;
    }
    for (int i = 1; i < header_count; i++) {
        int n = hpack_encode_literal(hpack_buf + hpack_len, (int)sizeof(hpack_buf) - hpack_len,
                                      headers[i * 2], (i * 2 + 1 < header_count * 2) ? headers[i * 2 + 1] : "");
        if (n > 0) hpack_len += n;
    }
    uint8_t flags = 0x04; /* END_HEADERS */
    if (end_stream) flags |= 0x01;
    return h2_write_frame(h2, H2_FRAME_HEADERS, flags, (uint32_t)stream_id, hpack_buf, hpack_len);
}

int aurora_h2_send_data(AuroraH2Connection* h2, int32_t stream_id,
                         const char* data, int len, int end_stream) {
    if (!h2 || !h2->is_open) return -1;
    uint8_t flags = end_stream ? 0x01 : 0x00;
    return h2_write_frame(h2, H2_FRAME_DATA, flags, (uint32_t)stream_id,
                           (const unsigned char*)data, len);
}

int aurora_h2_send_response(AuroraH2Connection* h2, int32_t stream_id,
                             int status_code, const char** headers, int header_count,
                             const char* body, int body_len) {
    /* Build pseudo-header for status */
    char status_str[16];
    snprintf(status_str, sizeof(status_str), "%d", status_code);
    const char* all_headers[64];
    int hc = 0;
    all_headers[hc++] = status_str; /* :status value — encoding handles ":status" key */
    for (int i = 0; i < header_count && hc + 2 < 64; i++) {
        all_headers[hc++] = headers[i * 2];
        all_headers[hc++] = headers[i * 2 + 1];
    }
    if (aurora_h2_send_headers(h2, stream_id, all_headers, hc, body == nullptr || body_len == 0) < 0)
        return -1;
    if (body && body_len > 0) {
        int max_chunk = 16384;
        int offset = 0;
        while (offset < body_len) {
            int chunk = body_len - offset;
            if (chunk > max_chunk) chunk = max_chunk;
            int last = (offset + chunk >= body_len) ? 1 : 0;
            if (aurora_h2_send_data(h2, stream_id, body + offset, chunk, last) < 0)
                return -1;
            offset += chunk;
        }
    }
    return 0;
}

int aurora_h2_send_rst_stream(AuroraH2Connection* h2, int32_t stream_id, uint32_t error_code) {
    unsigned char payload[4];
    payload[0] = (unsigned char)((error_code >> 24) & 0xff);
    payload[1] = (unsigned char)((error_code >> 16) & 0xff);
    payload[2] = (unsigned char)((error_code >> 8) & 0xff);
    payload[3] = (unsigned char)(error_code & 0xff);
    return h2_write_frame(h2, H2_FRAME_RST_STREAM, 0, (uint32_t)stream_id, payload, 4);
}

int aurora_h2_send_goaway(AuroraH2Connection* h2, uint32_t error_code) {
    unsigned char payload[8];
    uint32_t last = h2->last_stream_id > 0 ? (uint32_t)h2->last_stream_id : 0;
    payload[0] = (unsigned char)((last >> 24) & 0xff);
    payload[1] = (unsigned char)((last >> 16) & 0xff);
    payload[2] = (unsigned char)((last >> 8) & 0xff);
    payload[3] = (unsigned char)(last & 0xff);
    payload[4] = (unsigned char)((error_code >> 24) & 0xff);
    payload[5] = (unsigned char)((error_code >> 16) & 0xff);
    payload[6] = (unsigned char)((error_code >> 8) & 0xff);
    payload[7] = (unsigned char)(error_code & 0xff);
    return h2_write_frame(h2, H2_FRAME_GOAWAY, 0, 0, payload, 8);
}

int aurora_h2_is_open(AuroraH2Connection* h2) {
    return h2 ? h2->is_open : 0;
}

void aurora_h2_set_handler(AuroraH2Connection* h2, AuroraH2RequestHandler handler, void* user_data) {
    if (!h2) return;
    h2->request_handler = handler;
    h2->handler_user_data = user_data;
}

/* ── HPACK helper (exposed for testing) ── */

int aurora_h2_encode_header(const char* name, const char* value,
                             unsigned char* out, int out_size) {
    return hpack_encode_literal(out, out_size, name, value);
}

int aurora_h2_decode_header(const unsigned char* in, int in_len, int* consumed,
                             char* name_out, int name_size, char* value_out, int value_size) {
    return hpack_decode_one(in, in_len, consumed, name_out, name_size, value_out, value_size);
}
