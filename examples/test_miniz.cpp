/* Test if we can write a minimal DEFLATE encoder */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Minimal gzip/deflate output for testing */

static uint32_t crc32_table[256];
static int crc32_init = 0;

static void make_crc32_table(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
            c = (c >> 1) ^ (c & 1 ? 0xEDB88320 : 0);
        crc32_table[i] = c;
    }
}

static uint32_t gzip_crc32(const void* data, size_t len) {
    if (!crc32_init) { make_crc32_table(); crc32_init = 1; }
    uint32_t c = 0xFFFFFFFF;
    const unsigned char* p = (const unsigned char*)data;
    for (size_t i = 0; i < len; i++)
        c = crc32_table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFF;
}

/* Store block (no compression) - valid RFC 1951 */
size_t deflate_store_block(unsigned char* out, const unsigned char* in, size_t len, int final) {
    size_t pos = 0;
    /* BFINAL + BTYPE=0 (stored) */
    out[pos++] = (unsigned char)(final ? 1 : 0);
    /* LEN (16-bit, little-endian) */
    out[pos++] = (unsigned char)(len);
    out[pos++] = (unsigned char)(len >> 8);
    /* NLEN = ones-complement of LEN (16-bit) */
    uint16_t nlen = (uint16_t)~len;
    out[pos++] = (unsigned char)(nlen);
    out[pos++] = (unsigned char)(nlen >> 8);
    /* Copy data */
    memcpy(out + pos, in, len);
    pos += len;
    return pos;
}

size_t gzip_compress(const void* input, size_t input_len, void** output) {
    /* Output buffer: gzip header (10) + max DEFLATE overhead (5 + input_len) + trailer (8) */
    size_t max_out = 10 + input_len + 5 + 8;
    unsigned char* out = (unsigned char*)malloc(max_out);
    size_t pos = 0;

    /* GZIP header */
    out[pos++] = 0x1F; out[pos++] = 0x8B;  /* ID */
    out[pos++] = 0x08;                       /* CM = deflate */
    out[pos++] = 0;                           /* FLG */
    out[pos++] = 0; out[pos++] = 0;           /* MTIME (zero) */
    out[pos++] = 0; out[pos++] = 0;
    out[pos++] = 0;                           /* XFL */
    out[pos++] = 0xFF;                        /* OS = unknown */

    /* DEFLATE stored block wrapping input */
    pos += deflate_store_block(out + pos, (const unsigned char*)input, input_len, 1);

    /* CRC32 */
    uint32_t crc = gzip_crc32(input, input_len);
    out[pos++] = (unsigned char)(crc);
    out[pos++] = (unsigned char)(crc >> 8);
    out[pos++] = (unsigned char)(crc >> 16);
    out[pos++] = (unsigned char)(crc >> 24);

    /* ISIZE (input length mod 2^32) */
    uint32_t isize = (uint32_t)input_len;
    out[pos++] = (unsigned char)(isize);
    out[pos++] = (unsigned char)(isize >> 8);
    out[pos++] = (unsigned char)(isize >> 16);
    out[pos++] = (unsigned char)(isize >> 24);

    *output = out;
    return pos;
}

int main() {
    const char* data = "Hello World This is a test of gzip. Hello World This is a test of gzip. ";
    std::string big;
    for (int i = 0; i < 10; i++) big += data;
    size_t len = big.size();
    printf("Input: %zu bytes\n", len);

    void* compressed = NULL;
    size_t clen = gzip_compress(big.data(), len, &compressed);
    printf("Compressed: %zu bytes\n", clen);

    unsigned char* c = (unsigned char*)compressed;
    printf("First 16 hex: ");
    for (size_t i = 0; i < 16 && i < clen; i++)
        printf("%02X ", c[i]);
    printf("\n");

    /* Try to decompress with .NET */
    FILE* f = fopen("test_miniz_out.gz", "wb");
    fwrite(compressed, 1, clen, f);
    fclose(f);
    printf("Wrote test_miniz_out.gz\n");

    free(compressed);
    return 0;
}
