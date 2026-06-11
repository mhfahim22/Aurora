#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/* HTTP */
int aurora_net_http_get(const char* url, char* buffer, int buffer_size);
int aurora_net_http_post(const char* url, const char* body, const char* content_type,
                          char* buffer, int buffer_size);

/* DNS */
int aurora_net_dns_lookup(const char* hostname, char* buffer, int buffer_size);

/* TCP Socket (simple blocking) */
int64_t aurora_net_tcp_connect(const char* host, int port);
int     aurora_net_tcp_send(int64_t sock, const char* data, int len);
int     aurora_net_tcp_recv(int64_t sock, char* buffer, int buffer_size);
void    aurora_net_tcp_close(int64_t sock);

/* IP address resolution (returns first IPv4 as string) */
int aurora_net_resolve_host(const char* hostname, char* buffer, int buffer_size);

/* Send all bytes (retries partial writes, returns total sent or -1) */
int aurora_send_all(int64_t sock, const char* data, int len);

#ifdef __cplusplus
}
#endif
