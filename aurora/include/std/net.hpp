#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/* ── HTTP (supports http:// and https:// on Windows via WinHTTP) ── */
int aurora_net_http_get(const char* url, char* buffer, int buffer_size);
int aurora_net_http_post(const char* url, const char* body, const char* content_type,
                          char* buffer, int buffer_size);
int aurora_net_http_put(const char* url, const char* body, const char* content_type,
                         char* buffer, int buffer_size);
int aurora_net_http_delete(const char* url, char* buffer, int buffer_size);
int aurora_net_http_patch(const char* url, const char* body, const char* content_type,
                           char* buffer, int buffer_size);
int aurora_net_http_head(const char* url, char* buffer, int buffer_size);

/* ── HTTP with custom headers ── */
int aurora_net_http_get_ex(const char* url, const char* headers,
                            char* buffer, int buffer_size);
int aurora_net_http_post_ex(const char* url, const char* headers,
                             const char* body, const char* content_type,
                             char* buffer, int buffer_size);

/* ── HTTP status code extraction ── */
int aurora_net_http_status(const char* response);

/* ── URL helpers ── */
int aurora_net_url_encode(const char* input, char* out, int out_size);
int aurora_net_url_decode(const char* input, char* out, int out_size);

/* ── DNS ── */
int aurora_net_dns_lookup(const char* hostname, char* buffer, int buffer_size);

/* ── TCP Socket (simple blocking) ── */
int64_t aurora_net_tcp_connect(const char* host, int port);
int     aurora_net_tcp_send(int64_t sock, const char* data, int len);
int     aurora_net_tcp_recv(int64_t sock, char* buffer, int buffer_size);
void    aurora_net_tcp_close(int64_t sock);

/* ── UDP Socket ── */
int64_t aurora_net_udp_socket(void);
int     aurora_net_udp_sendto(int64_t sock, const char* data, int len,
                               const char* host, int port);
int     aurora_net_udp_recvfrom(int64_t sock, char* buffer, int buffer_size,
                                 char* src_host, int src_host_size, int* src_port);
void    aurora_net_udp_close(int64_t sock);

/* ── WebSocket Client ── */
int64_t aurora_net_ws_connect(const char* url, char* response, int response_size);
int     aurora_net_ws_send(int64_t ws, const char* data, int len, int is_text);
int     aurora_net_ws_recv(int64_t ws, char* buffer, int buffer_size, int* is_text);
void    aurora_net_ws_close(int64_t ws);

/* ── Multipart/form-data builder ── */
char* aurora_net_multipart_begin(const char* boundary);
char* aurora_net_multipart_add_field(char* form, const char* boundary,
                                      const char* name, const char* value);
char* aurora_net_multipart_add_file(char* form, const char* boundary,
                                     const char* name, const char* filename,
                                     const char* data, int data_len);
char* aurora_net_multipart_end(char* form, const char* boundary, int* out_len);

/* ── Download helper (HTTP GET to file, returns bytes written or -1) ── */
int aurora_net_download(const char* url, const char* filepath);

/* ── Authentication helpers ── */
void aurora_net_auth_basic(const char* username, const char* password,
                            char* out, int out_size);
void aurora_net_auth_bearer(const char* token, char* out, int out_size);

/* ── Send all bytes (retries partial writes) ── */
int aurora_send_all(int64_t sock, const char* data, int len);

/* ── IP address resolution (returns first IPv4 as string) ── */
int aurora_net_resolve_host(const char* hostname, char* buffer, int buffer_size);

/* ── Connection Pool ── */
typedef struct AuroraConnPool AuroraConnPool;

AuroraConnPool* aurora_net_conn_pool_new(int max_per_host, int idle_timeout_sec);
void            aurora_net_conn_pool_free(AuroraConnPool* pool);
int64_t         aurora_net_conn_pool_get(AuroraConnPool* pool, const char* host, int port);
void            aurora_net_conn_pool_put(AuroraConnPool* pool, int64_t sock, const char* host, int port);
void            aurora_net_conn_pool_clear_host(AuroraConnPool* pool, const char* host, int port);
int             aurora_net_conn_pool_idle_count(AuroraConnPool* pool);

#ifdef __cplusplus
}
#endif
