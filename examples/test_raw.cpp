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
    printf("Input: %zu bytes\n", len);

    /* Test MSZIP without RAW */
    COMPRESSOR_HANDLE comp = NULL;
    if (!CreateCompressor(COMPRESS_ALGORITHM_MSZIP, NULL, &comp)) {
        printf("CreateCompressor(MSZIP) failed: %lu\n", GetLastError());
        return 1;
    }
    SIZE_T needed = 0;
    Compress(comp, (LPCVOID)big.data(), (SIZE_T)len, NULL, 0, &needed);
    printf("MSZIP (buffer mode): needed=%zu\n", (size_t)needed);
    if (needed > 0) {
        std::vector<unsigned char> buf(needed);
        SIZE_T actual = needed;
        if (Compress(comp, (LPCVOID)big.data(), (SIZE_T)len, buf.data(), needed, &actual)) {
            printf("  compressed %zu -> %zu bytes\n", len, (size_t)actual);
            printf("  first 16 hex: ");
            for (size_t j = 0; j < 16 && j < actual; j++)
                printf("%02X ", buf[j]);
            printf("\n");
        }
    }
    CloseCompressor(comp);

    /* Test MSZIP with RAW */
    comp = NULL;
    if (!CreateCompressor(COMPRESS_ALGORITHM_MSZIP | COMPRESS_RAW, NULL, &comp)) {
        printf("CreateCompressor(MSZIP|RAW) failed: %lu\n", GetLastError());
    } else {
        needed = 0;
        Compress(comp, (LPCVOID)big.data(), (SIZE_T)len, NULL, 0, &needed);
        printf("MSZIP|RAW (block mode): needed=%zu\n", (size_t)needed);
        if (needed > 0) {
            std::vector<unsigned char> buf(needed);
            SIZE_T actual = needed;
            if (Compress(comp, (LPCVOID)big.data(), (SIZE_T)len, buf.data(), needed, &actual)) {
                printf("  compressed %zu -> %zu bytes\n", len, (size_t)actual);
                printf("  first 16 hex: ");
                for (size_t j = 0; j < 16 && j < actual; j++)
                    printf("%02X ", buf[j]);
                printf("\n");
            }
        }
        CloseCompressor(comp);
    }
    return 0;
}
