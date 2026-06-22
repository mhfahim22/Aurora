/* ── Audio helper: wraps Win32 PlaySound for Aurora FFI ── */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>

#define SND_ASYNC    0x0001
#define SND_FILENAME 0x00020000
#define SND_MEMORY   0x0004

typedef int(__stdcall* PlaySoundA_t)(const char* pszSound, void* hmod, int fdwSound);

static HMODULE g_winmm = NULL;
static PlaySoundA_t g_PlaySoundA = NULL;

#pragma pack(push, 1)
typedef struct {
    char           chunk_id[4];
    unsigned       chunk_size;
    char           format[4];
    char           subchunk1_id[4];
    unsigned       subchunk1_size;
    unsigned short audio_format;
    unsigned short num_channels;
    unsigned       sample_rate;
    unsigned       byte_rate;
    unsigned short block_align;
    unsigned short bits_per_sample;
    char           subchunk2_id[4];
    unsigned       subchunk2_size;
} WavHeader;
#pragma pack(pop)

extern "C" {

int aurora_audio_init() {
    g_winmm = LoadLibraryA("winmm.dll");
    if (!g_winmm) return 0;
    g_PlaySoundA = (PlaySoundA_t)GetProcAddress(g_winmm, "PlaySoundA");
    if (!g_PlaySoundA) { FreeLibrary(g_winmm); g_winmm = NULL; return 0; }
    return 1;
}

int aurora_audio_play_file(const char* path) {
    if (!g_PlaySoundA) return 0;
    return g_PlaySoundA(path, NULL, SND_ASYNC | SND_FILENAME) != 0 ? 1 : 0;
}

void aurora_audio_shutdown() {
    if (g_PlaySoundA) g_PlaySoundA(NULL, 0, 0);
    if (g_winmm) { FreeLibrary(g_winmm); g_winmm = NULL; }
    g_PlaySoundA = NULL;
}

} /* extern "C" */
#endif /* _WIN32 */
