#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Opaque HTTP/2 connection ── */
typedef struct AuroraH2Connection AuroraH2Connection;

/* ── HTTP/2 frame types ── */
#define H2_FRAME_DATA         0x00
#define H2_FRAME_HEADERS      0x01
#define H2_FRAME_PRIORITY     0x02
#define H2_FRAME_RST_STREAM   0x03
#define H2_FRAME_SETTINGS     0x04
#define H2_FRAME_PUSH_PROMISE 0x05
#define H2_FRAME_PING         0x06
#define H2_FRAME_GOAWAY       0x07
#define H2_FRAME_WINDOW_UPDATE 0x08
#define H2_FRAME_CONTINUATION 0x09

/* ── HTTP/2 error codes ── */
#define H2_NO_ERROR           0x00
#define H2_PROTOCOL_ERROR     0x01
#define H2_INTERNAL_ERROR     0x02
#define H2_FLOW_CONTROL_ERROR 0x03
#define H2_SETTINGS_TIMEOUT   0x04
#define H2_STREAM_CLOSED      0x05
#define H2_FRAME_SIZE_ERROR   0x06
#define H2_REFUSED_STREAM     0x07
#define H2_CANCEL             0x08
#define H2_COMPRESSION_ERROR  0x09
#define H2_CONNECT_ERROR      0x0A
#define H2_ENHANCE_YOUR_CALM  0x0B
#define H2_INADEQUATE_SECURITY 0x0C
#define H2_HTTP_1_1_REQUIRED  0x0D

/* ── HTTP/2 settings ── */
#define H2_SETTINGS_HEADER_TABLE_SIZE     0x01
#define H2_SETTINGS_ENABLE_PUSH          0x02
#define H2_SETTINGS_MAX_CONCURRENT_STREAMS 0x03
#define H2_SETTINGS_INITIAL_WINDOW_SIZE  0x04
#define H2_SETTINGS_MAX_FRAME_SIZE       0x05
#define H2_SETTINGS_MAX_HEADER_LIST_SIZE 0x06

/* ── Default settings values ── */
#define H2_DEFAULT_MAX_CONCURRENT_STREAMS 100
#define H2_DEFAULT_INITIAL_WINDOW_SIZE    65535
#define H2_DEFAULT_MAX_FRAME_SIZE         16384

/* ── HTTP/2 connection API ── */

/* Create a new H2 connection for a given socket/TLS handle.
   `server_mode` = 1 for server, 0 for client. */
AuroraH2Connection* aurora_h2_new(int64_t sock, int64_t tls_handle, int server_mode);

/* Free the H2 connection (closes all streams) */
void aurora_h2_free(AuroraH2Connection* h2);

/* Read and process the connection preface (24 bytes) */
int aurora_h2_read_preface(AuroraH2Connection* h2);

/* Send the server connection preface (SETTINGS frame) */
int aurora_h2_send_preface(AuroraH2Connection* h2);

/* Process incoming frames (returns 0 on success, -1 on error/close) */
int aurora_h2_process_frames(AuroraH2Connection* h2);

/* ── Stream data callbacks ── */
typedef void (*AuroraH2RequestHandler)(int32_t stream_id, const char** headers,
                                        int header_count, const char* body, int body_len,
                                        void* user_data);

/* Set the request handler callback */
void aurora_h2_set_handler(AuroraH2Connection* h2, AuroraH2RequestHandler handler, void* user_data);

/* Send a response on a stream */
int aurora_h2_send_response(AuroraH2Connection* h2, int32_t stream_id,
                             int status_code, const char** headers, int header_count,
                             const char* body, int body_len);

/* Send headers on a stream */
int aurora_h2_send_headers(AuroraH2Connection* h2, int32_t stream_id,
                            const char** headers, int header_count, int end_stream);

/* Send data on a stream */
int aurora_h2_send_data(AuroraH2Connection* h2, int32_t stream_id,
                         const char* data, int len, int end_stream);

/* Send RST_STREAM */
int aurora_h2_send_rst_stream(AuroraH2Connection* h2, int32_t stream_id, uint32_t error_code);

/* Send GOAWAY */
int aurora_h2_send_goaway(AuroraH2Connection* h2, uint32_t error_code);

/* Check if connection is still open */
int aurora_h2_is_open(AuroraH2Connection* h2);

/* ── HPACK helper ── */
/* Simple header encoding: encode a single header name+value */
int aurora_h2_encode_header(const char* name, const char* value, unsigned char* out, int out_size);

/* Simple header decoding: parse a single header from a block */
int aurora_h2_decode_header(const unsigned char* in, int in_len, int* consumed,
                             char* name_out, int name_size, char* value_out, int value_size);

#ifdef __cplusplus
}
#endif
