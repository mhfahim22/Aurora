#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <windows.h>
#include <compressapi.h>
#include <vector>
#include <time.h>

#pragma comment(lib, "cabinet")

static unsigned int gzip_crc32(const unsigned char* buf, size_t len) {
    static unsigned int table[256];
    static int init = 0;
    if (!init) {
        for (unsigned int i = 0; i < 256; i++) {
            unsigned int crc = i;
            for (int j = 0; j < 8; j++)
                crc = (crc >> 1) ^ (crc & 1 ? 0xEDB88320 : 0);
            table[i] = crc;
        }
        init = 1;
    }
    unsigned int crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++)
        crc = table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFF;
}

static unsigned char* gzip_compress(const unsigned char* input, size_t input_len, size_t* out_len) {
    *out_len = 0;
    if (!input || input_len == 0) return nullptr;
    COMPRESSOR_HANDLE compressor = NULL;
    if (!CreateCompressor(COMPRESS_ALGORITHM_DEFLATE, NULL, &compressor)) {
        fprintf(stderr, "CreateCompressor failed: %lu\n", GetLastError());
        return nullptr;
    }
    SIZE_T compressed_size = 0;
    Compress(compressor, (PVOID)input, input_len, NULL, 0, &compressed_size);
    if (compressed_size == 0) { CloseCompressor(compressor); return nullptr; }
    std::vector<unsigned char> deflate_buf(compressed_size);
    if (!Compress(compressor, (PVOID)input, input_len, deflate_buf.data(), compressed_size, &compressed_size)) {
        fprintf(stderr, "Compress failed: %lu\n", GetLastError());
        CloseCompressor(compressor); return nullptr;
    }
    CloseCompressor(compressor);
    *out_len = 10 + compressed_size + 8;
    unsigned char* gz = (unsigned char*)malloc(*out_len);
    if (!gz) return nullptr;
    gz[0] = 0x1F; gz[1] = 0x8B;
    gz[2] = 0x08;
    gz[3] = 0;
    uint32_t mtime = (uint32_t)time(NULL);
    gz[4] = (unsigned char)(mtime); gz[5] = (unsigned char)(mtime >> 8);
    gz[6] = (unsigned char)(mtime >> 16); gz[7] = (unsigned char)(mtime >> 24);
    gz[8] = 0;
    gz[9] = 0xFF;
    memcpy(gz + 10, deflate_buf.data(), compressed_size);
    uint32_t crc = gzip_crc32(input, input_len);
    size_t off = 10 + compressed_size;
    gz[off]     = (unsigned char)(crc);
    gz[off + 1] = (unsigned char)(crc >> 8);
    gz[off + 2] = (unsigned char)(crc >> 16);
    gz[off + 3] = (unsigned char)(crc >> 24);
    uint32_t isize = (uint32_t)input_len;
    gz[off + 4] = (unsigned char)(isize);
    gz[off + 5] = (unsigned char)(isize >> 8);
    gz[off + 6] = (unsigned char)(isize >> 16);
    gz[off + 7] = (unsigned char)(isize >> 24);
    return gz;
}

int main() {
    const char* test = "This is a test string that is long enough to test gzip compression. ";
    std::string big;
    for (int i = 0; i < 20; i++) big += test;
    size_t in_len = big.size();
    printf("Input: %zu bytes\n", in_len);

    size_t out_len = 0;
    unsigned char* out = gzip_compress((const unsigned char*)big.c_str(), in_len, &out_len);
    if (out) {
        printf("Compressed: %zu bytes (%.1f%%)\n", out_len, 100.0 * out_len / in_len);
        printf("First 2 bytes: 0x%02X 0x%02X\n", out[0], out[1]);
        if (out_len >= 2 && out[0] == 0x1F && out[1] == 0x8B)
            printf("GZIP HEADER VALID!\n");
        else
            printf("INVALID GZIP HEADER!\n");
        free(out);
    } else {
        printf("Compression FAILED!\n");
        return 1;
    }
    return 0;
}
