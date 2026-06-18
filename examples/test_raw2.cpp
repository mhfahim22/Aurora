#include <windows.h>
#include <compressapi.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#pragma comment(lib, "cabinet")

int main() {
    const char* data = "Hello World This is a test string that should be compressible. ";
    std::string big;
    for (int i = 0; i < 20; i++) big += data;
    size_t len = big.size();

    /* Compress with MSZIP|RAW */
    COMPRESSOR_HANDLE comp = NULL;
    if (!CreateCompressor(COMPRESS_ALGORITHM_MSZIP | COMPRESS_RAW, NULL, &comp)) {
        printf("CreateCompressor(MSZIP|RAW) failed: %lu\n", GetLastError());
        return 1;
    }
    SIZE_T needed = 0;
    Compress(comp, (LPCVOID)big.data(), (SIZE_T)len, NULL, 0, &needed);
    std::vector<unsigned char> compressed(needed);
    SIZE_T actual = needed;
    if (!Compress(comp, (LPCVOID)big.data(), (SIZE_T)len, compressed.data(), needed, &actual)) {
        printf("Compress failed: %lu\n", GetLastError());
        CloseCompressor(comp);
        return 1;
    }
    CloseCompressor(comp);
    printf("Compressed: %zu -> %zu bytes\n", len, (size_t)actual);
    printf("First 16: ");
    for (size_t j = 0; j < 16 && j < actual; j++) printf("%02X ", compressed[j]);
    printf("\n");

    /* Decompress with MSZIP|RAW */
    DECOMPRESSOR_HANDLE dec = NULL;
    if (!CreateDecompressor(COMPRESS_ALGORITHM_MSZIP | COMPRESS_RAW, NULL, &dec)) {
        printf("CreateDecompressor(MSZIP|RAW) failed: %lu\n", GetLastError());
    } else {
        SIZE_T dec_needed = 0;
        if (!Decompress(dec, (LPCVOID)compressed.data(), (SIZE_T)actual, NULL, 0, &dec_needed)) {
            DWORD err = GetLastError();
            printf("Decompress query needed: %lu (err=%lu)\n", (DWORD)dec_needed, err);
        }
        std::vector<unsigned char> decompressed(dec_needed);
        SIZE_T dec_actual = dec_needed;
        if (Decompress(dec, (LPCVOID)compressed.data(), (SIZE_T)actual, decompressed.data(), dec_needed, &dec_actual)) {
            std::string result((char*)decompressed.data(), dec_actual);
            printf("Decompressed OK: %zu bytes\n", (size_t)dec_actual);
            printf("Content: \"%s\"\n", result.c_str());
            printf("Match: %s\n", result == big ? "YES" : "NO");
        } else {
            printf("Decompress failed: %lu\n", GetLastError());
        }
        CloseDecompressor(dec);
    }

    /* Now try: wrap the compressed output in gzip and test */
    /* Build simple gzip */
    size_t gz_len = 10 + actual + 8;
    std::vector<unsigned char> gz(gz_len);
    gz[0] = 0x1F; gz[1] = 0x8B;
    gz[2] = 0x08;  /* CM = deflate */
    gz[3] = 0;
    gz[4] = gz[5] = gz[6] = gz[7] = 0;
    gz[8] = 0; gz[9] = 0xFF;
    memcpy(gz.data() + 10, compressed.data(), actual);
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = big[i];
        /* CRC table inline */
    }
    /* Simplified: just write the file and test outside */
    FILE* f = fopen("test_raw_gz.gz", "wb");
    fwrite(gz.data(), 1, gz_len, f);
    fclose(f);
    printf("\nWrote test_raw_gz.gz (%zu bytes)\n", gz_len);

    return 0;
}
