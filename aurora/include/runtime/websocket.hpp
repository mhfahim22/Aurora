#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

#define WS_OPCODE_CONT  0x0
#define WS_OPCODE_TEXT  0x1
#define WS_OPCODE_BIN   0x2
#define WS_OPCODE_CLOSE 0x8
#define WS_OPCODE_PING  0x9
#define WS_OPCODE_PONG  0xA

int  aurora_ws_is_upgrade(const char* raw_request);
const char* aurora_ws_get_key(const char* raw_request);

/* I/O functions — tls_handle = 0 means raw socket, >0 means TLS */
int  aurora_ws_upgrade(const char* key, int64_t sock, int64_t tls_handle);
int  aurora_ws_read_frame(int64_t sock, int64_t tls_handle, uint8_t** out_payload, int* opcode);
int  aurora_ws_write_frame(int64_t sock, int64_t tls_handle, int opcode, const uint8_t* data, int len);
void aurora_ws_close(int64_t sock, int64_t tls_handle);

#ifdef __cplusplus
}
#endif
