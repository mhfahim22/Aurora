#include <windows.h>
#include <compressapi.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#pragma comment(lib, "cabinet")

int main() {
    const char* data = "Hello World This is a test string that should be compressible";
    size_t len = strlen(data);

    printf("Testing CreateCompressor with different algorithms...\n");

    DWORD algorithms[] = {
        COMPRESS_ALGORITHM_NULL,
        COMPRESS_ALGORITHM_MSZIP,
        COMPRESS_ALGORITHM_XPRESS,
        COMPRESS_ALGORITHM_XPRESS_HUFF,
        COMPRESS_ALGORITHM_LZMS
    };
    const char* names[] = {"NULL", "MSZIP", "XPRESS", "XPRESS_HUFF", "LZMS"};

    for (int i = 0; i < 5; i++) {
        COMPRESSOR_HANDLE comp = NULL;
        BOOL ok = CreateCompressor(algorithms[i], NULL, &comp);
        if (ok) {
            printf("  %s: OK\n", names[i]);

            SIZE_T comp_size = 0;
            Compress(comp, data, len, NULL, 0, &comp_size);
            if (comp_size > 0) {
                std::vector<char> buf(comp_size);
                if (Compress(comp, data, len, buf.data(), comp_size, &comp_size)) {
                    printf("    compressed %zu -> %zu bytes\n", len, (size_t)comp_size);
                    printf("    first 4 bytes: %02X %02X %02X %02X\n",
                           (unsigned char)buf[0], (unsigned char)buf[1],
                           (unsigned char)buf[2], (unsigned char)buf[3]);
                }
            }
            CloseCompressor(comp);
        } else {
            printf("  %s: FAILED err=%lu\n", names[i], GetLastError());
        }
    }

    return 0;
}
