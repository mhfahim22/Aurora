#include "runtime/tls.hpp"
#include "runtime/memory.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <chrono>

#ifdef _WIN32
#define SECURITY_WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <windows.h>
#include <wincrypt.h>
#include <sspi.h>
#include <schannel.h>
#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "crypt32.lib")

/* ── TLS context (holds server credential + CA chain) ── */
struct AuroraTLSContext {
    CredHandle hCred;
    bool hasCred;
    PCCERT_CONTEXT certCtx;
    HCERTSTORE caStore;      /* additional CA certificates for chain building */
    char* caPath;            /* path to CA chain PEM file (for lazy loading) */
};

/* ── Per-connection TLS state ── */
struct TLSConn {
    CredHandle* hCred;    /* from context */
    CtxtHandle hCtxt;
    int64_t sock;
    bool handshakeDone;
    SecPkgContext_StreamSizes sizes;
    /* recv buffers for decryption */
    uint8_t* recvBuf;
    int recvBufCap;
    int recvOffset;
    int recvDataLen;
};

/* ── Encode a CN name as CERT_NAME_BLOB for self-signed cert ── */
static DATA_BLOB encode_cn_blob(const char* cn) {
    DATA_BLOB result = {0, NULL};
    if (!cn) cn = "localhost";
    CERT_RDN_ATTR attr;
    memset(&attr, 0, sizeof(attr));
    attr.pszObjId = const_cast<LPSTR>(szOID_COMMON_NAME);
    attr.dwValueType = CERT_RDN_PRINTABLE_STRING;
    attr.Value.cbData = (DWORD)strlen(cn);
    attr.Value.pbData = (BYTE*)cn;
    CERT_RDN rdn = {1, &attr};
    CERT_NAME_INFO nameInfo = {1, &rdn};
    DWORD cb = 0;
    if (!CryptEncodeObjectEx(X509_ASN_ENCODING, X509_NAME,
        &nameInfo, CRYPT_ENCODE_ALLOC_FLAG, NULL, &result.pbData, &cb))
        return result;
    result.cbData = cb;
    return result;
}

/* ── Create a self-signed certificate for localhost ── */
static PCCERT_CONTEXT create_self_signed() {
    DATA_BLOB nameBlob = encode_cn_blob("localhost");
    if (!nameBlob.pbData) return nullptr;
    SYSTEMTIME startTime, endTime;
    GetSystemTime(&startTime);
    endTime = startTime;
    endTime.wYear += 10; /* 10-year validity */
    PCCERT_CONTEXT cert = CertCreateSelfSignCertificate(
        NULL, &nameBlob, 0, NULL, NULL, &startTime, &endTime, NULL);
    LocalFree(nameBlob.pbData);
    return cert;
}

extern "C" {

AuroraTLSContext* aurora_tls_server_ctx_new(const char* cert_path, const char* key_path) {
    (void)key_path; /* unused — cert is self-contained in self-signed */
    AuroraTLSContext* ctx = (AuroraTLSContext*)calloc(1, sizeof(AuroraTLSContext));
    if (!ctx) return nullptr;

    /* Try to load PEM cert file, or fall back to self-signed */
    ctx->certCtx = nullptr;
    if (cert_path) {
        FILE* f = fopen(cert_path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            char* pem = (char*)malloc((size_t)sz + 1);
            if (pem) {
                fread(pem, 1, (size_t)sz, f);
                pem[sz] = '\0';
                DATA_BLOB derBlob = {0, NULL};
                if (CryptStringToBinaryA(pem, 0, CRYPT_STRING_BASE64HEADER,
                    NULL, &derBlob.cbData, NULL, NULL)) {
                    derBlob.pbData = (BYTE*)malloc(derBlob.cbData);
                    if (derBlob.pbData && CryptStringToBinaryA(pem, 0, CRYPT_STRING_BASE64HEADER,
                        derBlob.pbData, &derBlob.cbData, NULL, NULL)) {
                        ctx->certCtx = CertCreateCertificateContext(
                            X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                            derBlob.pbData, derBlob.cbData);
                    }
                    if (derBlob.pbData) free(derBlob.pbData);
                }
                free(pem);
            }
            fclose(f);
        }
    }
    if (!ctx->certCtx) {
        printf("[tls] no cert file, generating self-signed certificate\n");
        ctx->certCtx = create_self_signed();
    }
    if (!ctx->certCtx) {
        printf("[tls] failed to obtain certificate\n");
        free(ctx);
        return nullptr;
    }

    /* Acquire server credential handle with revocation checking */
    SCHANNEL_CRED schCred = {0};
    schCred.dwVersion = SCHANNEL_CRED_VERSION;
    schCred.cCreds = 1;
    schCred.paCred = &ctx->certCtx;
    schCred.grbitEnabledProtocols = SP_PROT_TLS1_2_SERVER | SP_PROT_TLS1_3_SERVER;
    schCred.dwFlags = SCH_CRED_NO_SYSTEM_MAPPER | SCH_CRED_NO_SERVERNAME_CHECK
                      | SCH_CRED_REVOCATION_CHECK_CHAIN;
    schCred.dwCredFormat = 0;

    TimeStamp tsExpiry;
    SECURITY_STATUS status = AcquireCredentialsHandleA(
        NULL, (LPSTR)UNISP_NAME_A, SECPKG_CRED_INBOUND,
        NULL, &schCred, NULL, NULL, &ctx->hCred, &tsExpiry);
    if (status != SEC_E_OK) {
        printf("[tls] AcquireCredentialsHandle failed: 0x%08x\n", (unsigned)status);
        CertFreeCertificateContext(ctx->certCtx);
        free(ctx);
        return nullptr;
    }
    ctx->hasCred = true;
    printf("[tls] server context created (self-signed cert)\n");
    return ctx;
}

int aurora_tls_set_ca_chain(AuroraTLSContext* ctx, const char* ca_pem_path) {
    if (!ctx || !ca_pem_path) return -1;
    /* Load CA certs from PEM into a memory certificate store */
    HANDLE hFile = CreateFileA(ca_pem_path, GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return -1;
    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == INVALID_FILE_SIZE || fileSize == 0) { CloseHandle(hFile); return -1; }
    char* pem = (char*)malloc((size_t)fileSize + 1);
    if (!pem) { CloseHandle(hFile); return -1; }
    DWORD read = 0;
    if (!ReadFile(hFile, pem, fileSize, &read, NULL)) { free(pem); CloseHandle(hFile); return -1; }
    pem[read] = '\0';
    CloseHandle(hFile);
    /* Create a memory store and add certs from this PEM blob */
    HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, NULL, 0, NULL);
    if (!store) { free(pem); return -1; }
    /* Decode PKCS7 / PEM blob into the store */
    CRYPT_DATA_BLOB derBlob = {0, NULL};
    char* pos = pem;
    int certCount = 0;
    while (*pos) {
        char* certStart = strstr(pos, "-----BEGIN CERTIFICATE-----");
        if (!certStart) break;
        char* certEnd = strstr(certStart, "-----END CERTIFICATE-----");
        if (!certEnd) break;
        certEnd += 25;
        /* Temporarily null-terminate at certEnd for decoding */
        char saved = *certEnd;
        *certEnd = '\0';
        if (CryptStringToBinaryA(certStart, 0, CRYPT_STRING_BASE64HEADER,
                                 NULL, &derBlob.cbData, NULL, NULL)) {
            derBlob.pbData = (BYTE*)malloc(derBlob.cbData);
            if (derBlob.pbData && CryptStringToBinaryA(certStart, 0, CRYPT_STRING_BASE64HEADER,
                    derBlob.pbData, &derBlob.cbData, NULL, NULL)) {
                PCCERT_CONTEXT certCtx = CertCreateCertificateContext(
                    X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                    derBlob.pbData, derBlob.cbData);
                if (certCtx) {
                    CertAddCertificateContextToStore(store, certCtx,
                        CERT_STORE_ADD_ALWAYS, NULL);
                    CertFreeCertificateContext(certCtx);
                    certCount++;
                }
            }
            if (derBlob.pbData) free(derBlob.pbData);
        }
        *certEnd = saved;
        pos = certEnd;
    }
    free(pem);
    if (certCount > 0) {
        if (ctx->caStore) CertCloseStore(ctx->caStore, 0);
        ctx->caStore = store;
        /* Store the path for future use */
        if (ctx->caPath) free(ctx->caPath);
        ctx->caPath = _strdup(ca_pem_path);
        printf("[tls] loaded %d CA cert(s) from %s\n", certCount, ca_pem_path);
        return 0;
    }
    CertCloseStore(store, 0);
    return -1;
}

void aurora_tls_ctx_free(AuroraTLSContext* ctx) {
    if (!ctx) return;
    if (ctx->hasCred)
        FreeCredentialsHandle(&ctx->hCred);
    if (ctx->certCtx)
        CertFreeCertificateContext(ctx->certCtx);
    if (ctx->caStore)
        CertCloseStore(ctx->caStore, 0);
    if (ctx->caPath)
        free(ctx->caPath);
    free(ctx);
}

/* ── Perform TLS handshake (server side) ── */
static int do_server_handshake(TLSConn* conn) {
    SecBufferDesc outBuffDesc, inBuffDesc;
    SecBuffer outSecBuf, inSecBuf[2];
    DWORD cbIoBuffer, cbIoBufferMax = 16384;
    BYTE* ioBuffer = (BYTE*)malloc(cbIoBufferMax);
    if (!ioBuffer) return -1;

    SECURITY_STATUS status;
    DWORD dwFlags = ASC_REQ_EXTENDED_ERROR | ASC_REQ_STREAM |
                    ASC_REQ_SEQUENCE_DETECT | ASC_REQ_REPLAY_DETECT |
                    ASC_REQ_CONFIDENTIALITY | ASC_REQ_ALLOCATE_MEMORY;
    DWORD dwAttr;

    /* Read client hello */
    cbIoBuffer = 0;
    int first = 1;
    do {
        if (first) {
            /* Read initial ClientHello */
            int n;
#ifdef _WIN32
            n = recv((SOCKET)(intptr_t)conn->sock, (char*)ioBuffer + cbIoBuffer,
                     (int)(cbIoBufferMax - cbIoBuffer), 0);
#else
            n = (int)recv((int)conn->sock, (char*)ioBuffer + cbIoBuffer,
                         cbIoBufferMax - cbIoBuffer, 0);
#endif
            if (n <= 0) { free(ioBuffer); return -1; }
            cbIoBuffer += n;
            first = 0;
        }

        inSecBuf[0].cbBuffer = cbIoBuffer;
        inSecBuf[0].BufferType = SECBUFFER_TOKEN;
        inSecBuf[0].pvBuffer = ioBuffer;
        inSecBuf[1].cbBuffer = 0;
        inSecBuf[1].BufferType = SECBUFFER_EMPTY;
        inSecBuf[1].pvBuffer = NULL;

        inBuffDesc.ulVersion = SECBUFFER_VERSION;
        inBuffDesc.cBuffers = 2;
        inBuffDesc.pBuffers = inSecBuf;

        outSecBuf.cbBuffer = 0;
        outSecBuf.BufferType = SECBUFFER_TOKEN;
        outSecBuf.pvBuffer = NULL;

        outBuffDesc.ulVersion = SECBUFFER_VERSION;
        outBuffDesc.cBuffers = 1;
        outBuffDesc.pBuffers = &outSecBuf;

        status = AcceptSecurityContext(
            conn->hCred, &conn->hCtxt,
            &inBuffDesc, dwFlags,
            SECURITY_NATIVE_DREP, &conn->hCtxt,
            &outBuffDesc, &dwAttr, NULL);

        if (outSecBuf.cbBuffer > 0 && outSecBuf.pvBuffer) {
            /* Send handshake token to client */
            const char* p = (const char*)outSecBuf.pvBuffer;
            int remaining = (int)outSecBuf.cbBuffer;
            while (remaining > 0) {
#ifdef _WIN32
                int n = (int)send((SOCKET)(intptr_t)conn->sock, p, remaining, 0);
#else
                int n = (int)send((int)conn->sock, p, (size_t)remaining, MSG_NOSIGNAL);
#endif
                if (n <= 0) break;
                p += n;
                remaining -= n;
            }
            FreeContextBuffer(outSecBuf.pvBuffer);
        }

        if (status == SEC_E_INCOMPLETE_MESSAGE) {
            /* Need more data */
            int n;
#ifdef _WIN32
            n = recv((SOCKET)(intptr_t)conn->sock, (char*)ioBuffer + cbIoBuffer,
                     (int)(cbIoBufferMax - cbIoBuffer), 0);
#else
            n = (int)recv((int)conn->sock, (char*)ioBuffer + cbIoBuffer,
                         cbIoBufferMax - cbIoBuffer, 0);
#endif
            if (n <= 0) { free(ioBuffer); return -1; }
            cbIoBuffer += n;
        } else if (status == SEC_E_OK) {
            conn->handshakeDone = true;
            /* Query stream sizes */
            QueryContextAttributesA(&conn->hCtxt, SECPKG_ATTR_STREAM_SIZES, &conn->sizes);
            printf("[tls] handshake complete (stream sizes: hdr=%lu, tra=%lu, max=%lu, buf=%lu)\n",
                   conn->sizes.cbHeader, conn->sizes.cbTrailer,
                   conn->sizes.cbMaximumMessage, conn->sizes.cBuffers);
        } else if (status == SEC_I_CONTINUE_NEEDED) {
            /* More rounds needed, read next token */
            cbIoBuffer = 0;
        } else {
            printf("[tls] AcceptSecurityContext failed: 0x%08x\n", (unsigned)status);
            free(ioBuffer);
            return -1;
        }
    } while (!conn->handshakeDone && status != SEC_E_OK);

    free(ioBuffer);
    return conn->handshakeDone ? 0 : -1;
}

int64_t aurora_tls_accept(int64_t sock, AuroraTLSContext* ctx) {
    if (!ctx || !ctx->hasCred) return -1;
    TLSConn* conn = (TLSConn*)calloc(1, sizeof(TLSConn));
    if (!conn) return -1;
    conn->hCred = &ctx->hCred;
    conn->sock = sock;
    conn->handshakeDone = false;
    conn->recvBuf = nullptr;
    conn->recvBufCap = 0;
    conn->recvOffset = 0;
    conn->recvDataLen = 0;

    /* Initialize security context */
    memset(&conn->hCtxt, 0, sizeof(conn->hCtxt));

    if (do_server_handshake(conn) != 0) {
        free(conn);
        return -1;
    }

    /* Allocate decrypt buffer */
    int bufSize = conn->sizes.cbHeader + conn->sizes.cbMaximumMessage + conn->sizes.cbTrailer;
    conn->recvBuf = (uint8_t*)malloc((size_t)bufSize);
    conn->recvBufCap = bufSize;
    conn->recvOffset = 0;
    conn->recvDataLen = 0;

    return (int64_t)(intptr_t)conn;
}

/* ── Decrypt data in-place, return plaintext bytes ── */
/* Updates conn->recvOffset/recvDataLen */
static int decrypt_data(TLSConn* conn) {
    if (conn->recvDataLen <= 0) return 0;
    SecBufferDesc msgDesc;
    SecBuffer msgBuf[4];
    msgDesc.ulVersion = SECBUFFER_VERSION;
    msgDesc.cBuffers = 4;
    msgDesc.pBuffers = msgBuf;
    msgBuf[0].cbBuffer = (ULONG)conn->recvDataLen;
    msgBuf[0].BufferType = SECBUFFER_DATA;
    msgBuf[0].pvBuffer = conn->recvBuf + conn->recvOffset;
    msgBuf[1].BufferType = SECBUFFER_EMPTY;
    msgBuf[2].BufferType = SECBUFFER_EMPTY;
    msgBuf[3].BufferType = SECBUFFER_EMPTY;

    SECURITY_STATUS status = DecryptMessage(&conn->hCtxt, &msgDesc, 0, NULL);
    if (status == SEC_E_INCOMPLETE_MESSAGE) {
        return 0; /* need more data */
    }
    if (status != SEC_E_OK && status != SEC_I_RENEGOTIATE) {
        return -1;
    }

    int dataIdx = -1;
    for (int i = 0; i < 4; i++) {
        if (msgBuf[i].BufferType == SECBUFFER_DATA) {
            dataIdx = i;
            break;
        }
    }
    if (dataIdx < 0) return 0;

    int decLen = (int)msgBuf[dataIdx].cbBuffer;
    if (decLen > 0)
        memmove(conn->recvBuf, msgBuf[dataIdx].pvBuffer, (size_t)decLen);
    conn->recvOffset = 0;
    conn->recvDataLen = decLen;
    return conn->recvDataLen;
}

int aurora_tls_read(int64_t tls_conn, char* buf, int size) {
    TLSConn* conn = (TLSConn*)(intptr_t)tls_conn;
    if (!conn || !conn->handshakeDone) return -1;

    /* If we have buffered decrypted data, return it */
    if (conn->recvDataLen > 0) {
        int toGive = (size < conn->recvDataLen) ? size : conn->recvDataLen;
        memcpy(buf, conn->recvBuf + conn->recvOffset, (size_t)toGive);
        conn->recvOffset += toGive;
        conn->recvDataLen -= toGive;
        if (conn->recvDataLen == 0) conn->recvOffset = 0;
        return toGive;
    }

    /* Read encrypted data from socket */
    conn->recvOffset = 0;
    int n;
#ifdef _WIN32
    n = recv((SOCKET)(intptr_t)conn->sock, (char*)conn->recvBuf,
             conn->recvBufCap, 0);
#else
    n = (int)recv((int)conn->sock, (char*)conn->recvBuf,
                 (size_t)conn->recvBufCap, 0);
#endif
    if (n <= 0) return -1;
    conn->recvDataLen = n;

    /* Decrypt */
    if (decrypt_data(conn) < 0) return -1;
    if (conn->recvDataLen <= 0) return 0;

    int toGive = (size < conn->recvDataLen) ? size : conn->recvDataLen;
    memcpy(buf, conn->recvBuf, (size_t)toGive);
    conn->recvOffset = toGive;
    conn->recvDataLen -= toGive;
    if (conn->recvDataLen == 0) conn->recvOffset = 0;
    return toGive;
}

int aurora_tls_write(int64_t tls_conn, const char* data, int len) {
    TLSConn* conn = (TLSConn*)(intptr_t)tls_conn;
    if (!conn || !conn->handshakeDone || !data) return -1;

    if (len == 0) return 0;
    if (len > (int)conn->sizes.cbMaximumMessage)
        len = (int)conn->sizes.cbMaximumMessage;

    /* Build message buffers: header + data + trailer */
    SecBuffer msgBuf[4];
    SecBufferDesc msgDesc;
    msgDesc.ulVersion = SECBUFFER_VERSION;
    msgDesc.cBuffers = 4;
    msgDesc.pBuffers = msgBuf;

    /* Allocate a contiguous buffer for header+data+trailer */
    int totalSize = conn->sizes.cbHeader + len + conn->sizes.cbTrailer;
    uint8_t* msg = (uint8_t*)malloc((size_t)totalSize);
    if (!msg) return -1;

    msgBuf[0].cbBuffer = conn->sizes.cbHeader;
    msgBuf[0].BufferType = SECBUFFER_STREAM_HEADER;
    msgBuf[0].pvBuffer = msg;

    msgBuf[1].cbBuffer = (ULONG)len;
    msgBuf[1].BufferType = SECBUFFER_DATA;
    msgBuf[1].pvBuffer = msg + conn->sizes.cbHeader;
    memcpy(msgBuf[1].pvBuffer, data, (size_t)len);

    msgBuf[2].cbBuffer = conn->sizes.cbTrailer;
    msgBuf[2].BufferType = SECBUFFER_STREAM_TRAILER;
    msgBuf[2].pvBuffer = msg + conn->sizes.cbHeader + len;

    msgBuf[3].cbBuffer = 0;
    msgBuf[3].BufferType = SECBUFFER_EMPTY;

    SECURITY_STATUS status = EncryptMessage(&conn->hCtxt, 0, &msgDesc, 0);
    if (status != SEC_E_OK) {
        free(msg);
        return -1;
    }

    /* Send the encrypted result */
    int totalEnc = (int)(msgBuf[0].cbBuffer + msgBuf[1].cbBuffer + msgBuf[2].cbBuffer);
    const char* p = (const char*)msg;
    int remaining = totalEnc;
    while (remaining > 0) {
#ifdef _WIN32
        int n = (int)send((SOCKET)(intptr_t)conn->sock, p, remaining, 0);
#else
        int n = (int)send((int)conn->sock, p, (size_t)remaining, MSG_NOSIGNAL);
#endif
        if (n <= 0) break;
        p += n;
        remaining -= n;
    }
    free(msg);
    return (remaining == 0) ? len : -1;
}

void aurora_tls_close(int64_t tls_conn) {
    TLSConn* conn = (TLSConn*)(intptr_t)tls_conn;
    if (!conn) return;
    /* Send close_notify */
    if (conn->handshakeDone) {
        SecBufferDesc shutdownDesc;
        SecBuffer shutdownBuf[1];
        DWORD shutdownToken = SCHANNEL_SHUTDOWN;
        shutdownBuf[0].cbBuffer = sizeof(DWORD);
        shutdownBuf[0].BufferType = SECBUFFER_TOKEN;
        shutdownBuf[0].pvBuffer = &shutdownToken;
        shutdownDesc.ulVersion = SECBUFFER_VERSION;
        shutdownDesc.cBuffers = 1;
        shutdownDesc.pBuffers = shutdownBuf;
        ApplyControlToken(&conn->hCtxt, &shutdownDesc);

        /* Encrypt empty shutdown message */
        SecBufferDesc msgDesc;
        SecBuffer msgBuf[1];
        msgBuf[0].cbBuffer = 0;
        msgBuf[0].BufferType = SECBUFFER_TOKEN;
        msgBuf[0].pvBuffer = NULL;
        msgDesc.ulVersion = SECBUFFER_VERSION;
        msgDesc.cBuffers = 1;
        msgDesc.pBuffers = msgBuf;
        SECURITY_STATUS status = EncryptMessage(&conn->hCtxt, 0, &msgDesc, 0);
        if (status == SEC_E_OK && msgBuf[0].cbBuffer > 0 && msgBuf[0].pvBuffer) {
            const char* p = (const char*)msgBuf[0].pvBuffer;
            int remaining = (int)msgBuf[0].cbBuffer;
            while (remaining > 0) {
                int n = (int)send((SOCKET)(intptr_t)conn->sock, p, remaining, 0);
                if (n <= 0) break;
                p += n;
                remaining -= n;
            }
            free(msgBuf[0].pvBuffer);
        }
    }
    DeleteSecurityContext(&conn->hCtxt);
    if (conn->recvBuf) free(conn->recvBuf);
#ifdef _WIN32
    closesocket((SOCKET)(intptr_t)conn->sock);
#else
    close((int)conn->sock);
#endif
    free(conn);
}

#else
/* ── POSIX: OpenSSL-based TLS ── */
#include <dlfcn.h>
#include <sys/socket.h>

/* Dynamic loading for OpenSSL */
struct OpenSSLFuncs {
    void* libssl;
    void* libcrypto;
    void* (*SSL_new)(void*);
    void  (*SSL_free)(void*);
    int   (*SSL_set_fd)(void*, int);
    int   (*SSL_accept)(void*);
    int   (*SSL_read)(void*, void*, int);
    int   (*SSL_write)(void*, const void*, int);
    int   (*SSL_shutdown)(void*);
    void* (*SSL_CTX_new)(void*);
    void  (*SSL_CTX_free)(void*);
    int   (*SSL_CTX_use_certificate_file)(void*, const char*, int);
    int   (*SSL_CTX_use_PrivateKey_file)(void*, const char*, int);
    void* (*SSL_CTX_get0_param)(void*);
    int   (*X509_VERIFY_PARAM_set_flags)(void*, unsigned long);
};

struct AuroraTLSContext {
    void* ssl_ctx;
    OpenSSLFuncs ssl;
};

struct TLSConn {
    void* ssl;
    int64_t sock;
};

extern "C" {

AuroraTLSContext* aurora_tls_server_ctx_new(const char* cert_path, const char* key_path) {
    AuroraTLSContext* ctx = (AuroraTLSContext*)calloc(1, sizeof(AuroraTLSContext));
    if (!ctx) return nullptr;

    memset(&ctx->ssl, 0, sizeof(ctx->ssl));
    ctx->ssl.libssl = dlopen("libssl.so.3", RTLD_LAZY | RTLD_LOCAL);
    if (!ctx->ssl.libssl)
        ctx->ssl.libssl = dlopen("libssl.so.1.1", RTLD_LAZY | RTLD_LOCAL);
    if (!ctx->ssl.libssl)
        ctx->ssl.libssl = dlopen("libssl.so", RTLD_LAZY | RTLD_LOCAL);
    if (!ctx->ssl.libssl) {
        printf("[tls] failed to load OpenSSL\n");
        free(ctx);
        return nullptr;
    }
    ctx->ssl.libcrypto = dlopen("libcrypto.so.3", RTLD_LAZY | RTLD_LOCAL);
    if (!ctx->ssl.libcrypto)
        ctx->ssl.libcrypto = dlopen("libcrypto.so.1.1", RTLD_LAZY | RTLD_LOCAL);
    if (!ctx->ssl.libcrypto)
        ctx->ssl.libcrypto = dlopen("libcrypto.so", RTLD_LAZY | RTLD_LOCAL);

#define LOAD(name) do { \
    ctx->ssl.SSL_ ## name = (decltype(ctx->ssl.SSL_ ## name))dlsym(ctx->ssl.libssl, "SSL_" #name); \
    if (!ctx->ssl.SSL_ ## name) { \
        printf("[tls] missing OpenSSL symbol: %s\n", "SSL_" #name); \
        dlclose(ctx->ssl.libssl); \
        free(ctx); \
        return nullptr; \
    } \
} while(0)

    LOAD(new);
    LOAD(free);
    LOAD(set_fd);
    LOAD(accept);
    LOAD(read);
    LOAD(write);
    LOAD(shutdown);
    LOAD(CTX_new);
    LOAD(CTX_free);
    LOAD(CTX_use_certificate_file);
    LOAD(CTX_use_PrivateKey_file);
    LOAD(CTX_get0_param);
#undef LOAD

    /* Load X509_VERIFY_PARAM_set_flags from libcrypto */
    ctx->ssl.X509_VERIFY_PARAM_set_flags =
        (decltype(ctx->ssl.X509_VERIFY_PARAM_set_flags))dlsym(
            ctx->ssl.libcrypto ? ctx->ssl.libcrypto : ctx->ssl.libssl,
            "X509_VERIFY_PARAM_set_flags");
    if (!ctx->ssl.X509_VERIFY_PARAM_set_flags) {
        printf("[tls] warning: X509_VERIFY_PARAM_set_flags not found, CRL checking disabled\n");
    }

    ctx->ssl_ctx = ctx->ssl.SSL_CTX_new(NULL);
    if (!ctx->ssl_ctx) {
        dlclose(ctx->ssl.libssl);
        free(ctx);
        return nullptr;
    }

    /* Use method TLS_server_method() — loaded separately */
    typedef void* (*tls_method_t)();
    tls_method_t method_fn = (tls_method_t)dlsym(ctx->ssl.libssl, "TLS_server_method");
    if (method_fn) {
        ctx->ssl_ctx = ctx->ssl.SSL_CTX_new(method_fn());
    }

    /* Enable CRL checking if X509_VERIFY_PARAM_set_flags is available */
    if (ctx->ssl.X509_VERIFY_PARAM_set_flags && ctx->ssl.SSL_CTX_get0_param) {
        void* param = ctx->ssl.SSL_CTX_get0_param(ctx->ssl_ctx);
        if (param) {
            /* X509_V_FLAG_CRL_CHECK = 0x4, X509_V_FLAG_CRL_CHECK_ALL = 0x8 */
            ctx->ssl.X509_VERIFY_PARAM_set_flags(param, 0x4 | 0x8);
            printf("[tls] CRL checking enabled\n");
        }
    }

    if (cert_path && key_path) {
        if (ctx->ssl.SSL_CTX_use_certificate_file(ctx->ssl_ctx, cert_path, 1) <= 0)
            printf("[tls] warning: failed to load cert %s\n", cert_path);
        if (ctx->ssl.SSL_CTX_use_PrivateKey_file(ctx->ssl_ctx, key_path, 1) <= 0)
            printf("[tls] warning: failed to load key %s\n", key_path);
    } else {
        printf("[tls] warning: no cert/key provided, connections may fail\n");
    }
    return ctx;
}

int aurora_tls_set_ca_chain(AuroraTLSContext* ctx, const char* ca_pem_path) {
    if (!ctx || !ctx->ssl_ctx || !ca_pem_path) return -1;
    /* Load the CA file for peer certificate verification */
    typedef int (*load_verify_fn)(void*, const char*, const char*);
    load_verify_fn load_verify = (load_verify_fn)dlsym(ctx->ssl.libssl, "SSL_CTX_load_verify_locations");
    if (!load_verify) return -1;
    int result = load_verify(ctx->ssl_ctx, ca_pem_path, NULL);
    if (result <= 0) {
        printf("[tls] warning: failed to load CA chain %s\n", ca_pem_path);
        return -1;
    }
    /* Enable peer certificate verification (mutual TLS) */
    typedef void (*set_verify_fn)(void*, int, void*);
    set_verify_fn set_verify = (set_verify_fn)dlsym(ctx->ssl.libssl, "SSL_CTX_set_verify");
    if (set_verify) {
        set_verify(ctx->ssl_ctx, 1, NULL); /* SSL_VERIFY_PEER */
    }
    printf("[tls] CA chain loaded from %s\n", ca_pem_path);
    return 0;
}

void aurora_tls_ctx_free(AuroraTLSContext* ctx) {
    if (!ctx) return;
    if (ctx->ssl_ctx) ctx->ssl.SSL_CTX_free(ctx->ssl_ctx);
    if (ctx->ssl.libssl) dlclose(ctx->ssl.libssl);
    if (ctx->ssl.libcrypto) dlclose(ctx->ssl.libcrypto);
    free(ctx);
}

int64_t aurora_tls_accept(int64_t sock, AuroraTLSContext* ctx) {
    if (!ctx || !ctx->ssl_ctx) return -1;
    TLSConn* conn = (TLSConn*)calloc(1, sizeof(TLSConn));
    if (!conn) return -1;
    conn->sock = sock;
    conn->ssl = ctx->ssl.SSL_new(ctx->ssl_ctx);
    if (!conn->ssl) { free(conn); return -1; }
    ctx->ssl.SSL_set_fd(conn->ssl, (int)sock);
    if (ctx->ssl.SSL_accept(conn->ssl) <= 0) {
        ctx->ssl.SSL_free(conn->ssl);
        free(conn);
        return -1;
    }
    return (int64_t)(intptr_t)conn;
}

int aurora_tls_read(int64_t tls_conn, char* buf, int size) {
    TLSConn* conn = (TLSConn*)(intptr_t)tls_conn;
    if (!conn || !conn->ssl) return -1;
    return conn->ssl.SSL_read(conn->ssl, buf, size);
}

int aurora_tls_write(int64_t tls_conn, const char* data, int len) {
    TLSConn* conn = (TLSConn*)(intptr_t)tls_conn;
    if (!conn || !conn->ssl || !data) return -1;
    return conn->ssl.SSL_write(conn->ssl, data, len);
}

void aurora_tls_close(int64_t tls_conn) {
    TLSConn* conn = (TLSConn*)(intptr_t)tls_conn;
    if (!conn) return;
    if (conn->ssl) {
        conn->ssl.SSL_shutdown(conn->ssl);
        conn->ssl.SSL_free(conn->ssl);
    }
#ifdef _WIN32
    closesocket((SOCKET)(intptr_t)conn->sock);
#else
    close((int)conn->sock);
#endif
    free(conn);
}

#endif /* _WIN32 */

} /* extern "C" */
