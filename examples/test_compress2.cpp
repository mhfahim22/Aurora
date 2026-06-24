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

    DWORD algos[] = {COMPRESS_ALGORITHM_NULL, COMPRESS_ALGORITHM_MSZIP, 
                     COMPRESS_ALGORITHM_XPRESS, COMPRESS_ALGORITHM_XPRESS_HUFF, COMPRESS_ALGORITHM_LZMS, 6};
    const char* names[] = {"NULL", "MSZIP", "XPRESS", "XPRESS_HUFF", "LZMS", "DEFLATE(6)"};

    for (int i = 0; i < 6; i++) {
        COMPRESSOR_HANDLE comp = NULL;
        if (!CreateCompressor(algos[i], NULL, &comp)) {
            printf("%s: CreateCompressor failed %lu\n", names[i], GetLastError());
            continue;
        }
        SIZE_T needed = 0;
        if (!Compress(comp, (LPCVOID)big.data(), (SIZE_T)len, NULL, 0, &needed) && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            printf("%s: Query failed %lu\n", names[i], GetLastError());
            CloseCompressor(comp);
            continue;
        }
        printf("%s: query needed=%zu\n", names[i], (size_t)needed);
        if (needed > 0) {
            std::vector<unsigned char> buf(needed);
            SIZE_T actual = needed;
            if (Compress(comp, (LPCVOID)big.data(), (SIZE_T)len, buf.data(), needed, &actual)) {
                printf("  compressed %zu -> %zu bytes\n", len, (size_t)actual);
                printf("  first 8 hex: ");
                for (size_t j = 0; j < 8 && j < actual; j++)
                    printf("%02X ", buf[j]);
                printf("\n");
            } else {
                printf("  Compress failed: %lu\n", GetLastError());
            }
        }
        CloseCompressor(comp);
    }
    return 0;
}
