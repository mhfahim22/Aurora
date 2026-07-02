#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Opaque TLS context (holds server cert/keys) ── */
typedef struct AuroraTLSContext AuroraTLSContext;

/* ── Create TLS server context from PEM cert + key files ── */
AuroraTLSContext* aurora_tls_server_ctx_new(const char* cert_pem_path, const char* key_pem_path);

/* ── Set CA certificate chain for verifying peer certificates ── */
/* Returns 0 on success, -1 on failure */
int aurora_tls_set_ca_chain(AuroraTLSContext* ctx, const char* ca_pem_path);

/* ── Free TLS context ── */
void aurora_tls_ctx_free(AuroraTLSContext* ctx);

/* ── Accept TLS handshake on a connected socket ── */
/* Returns a TLS connection handle (>0), or -1 on failure */
int64_t aurora_tls_accept(int64_t sock, AuroraTLSContext* ctx);

/* ── Read from TLS connection ── */
int aurora_tls_read(int64_t tls_conn, char* buf, int size);

/* ── Write to TLS connection ── */
int aurora_tls_write(int64_t tls_conn, const char* data, int len);

/* ── Close TLS connection ── */
void aurora_tls_close(int64_t tls_conn);

#ifdef __cplusplus
}
#endif
