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

    /* Test 1: MSZIP without RAW, then strip header */
    {
        COMPRESSOR_HANDLE comp = NULL;
        CreateCompressor(COMPRESS_ALGORITHM_MSZIP, NULL, &comp);
        SIZE_T needed = 0;
        Compress(comp, (LPCVOID)big.data(), (SIZE_T)len, NULL, 0, &needed);
        std::vector<unsigned char> buf(needed);
        SIZE_T actual = needed;
        Compress(comp, (LPCVOID)big.data(), (SIZE_T)len, buf.data(), needed, &actual);
        CloseCompressor(comp);

        printf("MSZIP buffer mode: %zu -> %zu bytes\n", len, (size_t)actual);
        printf("  First 16: ");
        for (size_t j = 0; j < 16 && j < actual; j++) printf("%02X ", buf[j]);
        printf("\n");

        /* The 0x0A header is: 0A [flags/size/type...] */
        /* After 0x0A header, we might have the MSZIP signature or raw DEFLATE */
        /* Let's try stripping 4 bytes (the 0x0A header) and see if the rest is valid */
        size_t offset = 6; /* 0x0A + 5 bytes of header */
        size_t payload = actual - offset;
        printf("  Trying to wrap offset=%zu, payload=%zu as gzip\n", offset, payload);

        /* Check if payload starts with 0x43 0x4D MSZIP sig */
        if (payload >= 2 && buf[offset] == 0x43 && buf[offset+1] == 0x4D) {
            printf("  Found MSZIP sig at offset %zu\n", offset);
            offset += 2;
            payload -= 2;
        } else if (payload >= 2) {
            printf("  No MSZIP sig, bytes: %02X %02X\n", buf[offset], buf[offset+1]);
        }

        /* Build gzip */
        std::vector<unsigned char> gz(10 + payload + 8);
        gz[0]=0x1F; gz[1]=0x8B; gz[2]=0x08; gz[3]=0;
        gz[4]=gz[5]=gz[6]=gz[7]=gz[8]=0; gz[9]=0xFF;
        memcpy(gz.data()+10, buf.data()+offset, payload);

        /* CRC32 */
        uint32_t crc = 0xFFFFFFFF;
        static uint32_t crc_table[256];
        static int crc_ready = 0;
        if (!crc_ready) {
            for (uint32_t i = 0; i < 256; i++) {
                uint32_t c = i;
                for (int j = 0; j < 8; j++)
                    c = (c >> 1) ^ (c & 1 ? 0xEDB88320 : 0);
                crc_table[i] = c;
            }
            crc_ready = 1;
        }
        for (size_t i = 0; i < len; i++)
            crc = crc_table[(crc ^ (unsigned char)big[i]) & 0xFF] ^ (crc >> 8);
        crc ^= 0xFFFFFFFF;
        size_t off = 10 + payload;
        gz[off++] = (unsigned char)crc;
        gz[off++] = (unsigned char)(crc>>8);
        gz[off++] = (unsigned char)(crc>>16);
        gz[off++] = (unsigned char)(crc>>24);
        uint32_t isize = (uint32_t)len;
        gz[off++] = (unsigned char)isize;
        gz[off++] = (unsigned char)(isize>>8);
        gz[off++] = (unsigned char)(isize>>16);
        gz[off++] = (unsigned char)(isize>>24);

        FILE* f = fopen("test_gz_stripped.gz", "wb");
        fwrite(gz.data(), 1, gz.size(), f);
        fclose(f);
        printf("  Wrote test_gz_stripped.gz (%zu bytes)\n", gz.size());
    }

    /* Test 2: Decompress MSZIP (no RAW) correctly */
    {
        COMPRESSOR_HANDLE comp = NULL;
        CreateCompressor(COMPRESS_ALGORITHM_MSZIP, NULL, &comp);
        SIZE_T needed = 0;
        Compress(comp, (LPCVOID)big.data(), (SIZE_T)len, NULL, 0, &needed);
        std::vector<unsigned char> buf(needed);
        SIZE_T actual = needed;
        Compress(comp, (LPCVOID)big.data(), (SIZE_T)len, buf.data(), needed, &actual);
        CloseCompressor(comp);

        /* Decompress with MSZIP (no RAW) - should work */
        DECOMPRESSOR_HANDLE dec = NULL;
        if (!CreateDecompressor(COMPRESS_ALGORITHM_MSZIP, NULL, &dec)) {
            printf("ERROR: CreateDecompressor(MSZIP) failed: %lu\n", GetLastError());
        } else {
            SIZE_T dec_needed = 0;
            Decompress(dec, buf.data(), actual, NULL, 0, &dec_needed);
            std::vector<unsigned char> dec_out(dec_needed);
            SIZE_T dec_actual = dec_needed;
            if (Decompress(dec, buf.data(), actual, dec_out.data(), dec_needed, &dec_actual)) {
                std::string result((char*)dec_out.data(), dec_actual);
                printf("MSZIP decompress: %zu bytes, match=%s\n", (size_t)dec_actual,
                       result == big ? "YES" : "NO");
            } else {
                printf("MSZIP decompress failed: %lu\n", GetLastError());
            }
            CloseDecompressor(dec);
        }
    }

    return 0;
}
