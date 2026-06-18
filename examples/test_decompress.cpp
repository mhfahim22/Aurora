#include <windows.h>
#include <compressapi.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#pragma comment(lib, "cabinet")

int main() {
    /* Read the gzip file */
    FILE* f = fopen("C:\\Users\\user\\AppData\\Local\\Temp\\raw_gzip.bin", "rb");
    if (!f) { printf("Cannot open file\n"); return 1; }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<unsigned char> data(fsize);
    fread(data.data(), 1, fsize, f);
    fclose(f);

    printf("File size: %ld bytes\n", fsize);

    /* Skip 10-byte gzip header */
    unsigned char* deflate_data = data.data() + 10;
    size_t deflate_size = fsize - 10 - 8;  /* minus gzip header and footer */
    printf("Deflate data offset=10, size=%zu\n", deflate_size);

    /* Try to decompress with Windows Compress API */
    DECOMPRESSOR_HANDLE decomp = NULL;
    if (!CreateDecompressor(COMPRESS_ALGORITHM_MSZIP, NULL, &decomp)) {
        printf("CreateDecompressor(MSZIP) failed: %lu\n", GetLastError());
    } else {
        SIZE_T uncomp_size = 0;
        Decompress(decomp, deflate_data, deflate_size, NULL, 0, &uncomp_size);
        printf("MSZIP query returned size: %zu (err=%lu)\n", uncomp_size, GetLastError());
        if (uncomp_size > 0) {
            std::vector<unsigned char> buf(uncomp_size);
            if (Decompress(decomp, deflate_data, deflate_size, buf.data(), uncomp_size, &uncomp_size)) {
                printf("Decompressed %zu -> %zu bytes\n", deflate_size, uncomp_size);
                printf("Content: %.*s\n", (int)uncomp_size, buf.data());
            } else {
                printf("Decompress failed: %lu\n", GetLastError());
            }
        }
        CloseDecompressor(decomp);
    }

    /* Try XPRESS */
    if (!CreateDecompressor(COMPRESS_ALGORITHM_XPRESS, NULL, &decomp)) {
        printf("CreateDecompressor(XPRESS) failed: %lu\n", GetLastError());
    } else {
        SIZE_T uncomp_size = 0;
        Decompress(decomp, deflate_data, deflate_size, NULL, 0, &uncomp_size);
        printf("XPRESS query returned size: %zu (err=%lu)\n", uncomp_size, GetLastError());
        CloseDecompressor(decomp);
    }

    /* Try LZMS */
    if (!CreateDecompressor(COMPRESS_ALGORITHM_LZMS, NULL, &decomp)) {
        printf("CreateDecompressor(LZMS) failed: %lu\n", GetLastError());
    } else {
        SIZE_T uncomp_size = 0;
        Decompress(decomp, deflate_data, deflate_size, NULL, 0, &uncomp_size);
        printf("LZMS query returned size: %zu (err=%lu)\n", uncomp_size, GetLastError());
        CloseDecompressor(decomp);
    }

    /* Try NULL */
    if (!CreateDecompressor(COMPRESS_ALGORITHM_NULL, NULL, &decomp)) {
        printf("CreateDecompressor(NULL) failed: %lu\n", GetLastError());
    } else {
        SIZE_T uncomp_size = 0;
        Decompress(decomp, deflate_data, deflate_size, NULL, 0, &uncomp_size);
        printf("NULL query returned size: %zu (err=%lu)\n", uncomp_size, GetLastError());
        CloseDecompressor(decomp);
    }

    /* Also try raw deflate: just the first bytes */
    printf("First 4 bytes of deflate data: %02X %02X %02X %02X\n",
           deflate_data[0], deflate_data[1], deflate_data[2], deflate_data[3]);
    printf("is 0A 51 = deflate block header\n");
    return 0;
}
