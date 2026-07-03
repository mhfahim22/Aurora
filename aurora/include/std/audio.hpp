#pragma once
#include "common/platform.hpp"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Opaque types ── */
typedef struct AuroraAudioSrc AuroraAudioSrc;
typedef struct AuroraAudioRec AuroraAudioRec;

/* ════════════════════════════════════════════════════════════
   Lifecycle
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT int  aurora_audio_init(void);
AURORA_EXPORT void aurora_audio_shutdown(void);

/* ════════════════════════════════════════════════════════════
   Source — load, play, stop, pause, resume
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraAudioSrc* aurora_audio_src_new(void);
AURORA_EXPORT int             aurora_audio_src_load_file(AuroraAudioSrc* src, const char* path);
AURORA_EXPORT int             aurora_audio_src_load_mem(AuroraAudioSrc* src, const void* data, int len);
AURORA_EXPORT void            aurora_audio_src_play(AuroraAudioSrc* src);
AURORA_EXPORT void            aurora_audio_src_stop(AuroraAudioSrc* src);
AURORA_EXPORT void            aurora_audio_src_pause(AuroraAudioSrc* src);
AURORA_EXPORT void            aurora_audio_src_resume(AuroraAudioSrc* src);
AURORA_EXPORT int             aurora_audio_src_is_playing(AuroraAudioSrc* src);
AURORA_EXPORT void            aurora_audio_src_free(AuroraAudioSrc* src);

/* ════════════════════════════════════════════════════════════
   Volume / Pitch / Loop / Time / Duration
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT void   aurora_audio_src_set_volume(AuroraAudioSrc* src, double vol);
AURORA_EXPORT double aurora_audio_src_get_volume(AuroraAudioSrc* src);
AURORA_EXPORT void   aurora_audio_src_set_pitch(AuroraAudioSrc* src, double pitch);
AURORA_EXPORT double aurora_audio_src_get_pitch(AuroraAudioSrc* src);
AURORA_EXPORT void   aurora_audio_src_set_looping(AuroraAudioSrc* src, int loop);
AURORA_EXPORT int    aurora_audio_src_get_looping(AuroraAudioSrc* src);
AURORA_EXPORT void   aurora_audio_src_set_time(AuroraAudioSrc* src, double sec);
AURORA_EXPORT double aurora_audio_src_get_time(AuroraAudioSrc* src);
AURORA_EXPORT double aurora_audio_src_get_duration(AuroraAudioSrc* src);

/* ════════════════════════════════════════════════════════════
   Effects — reverb, echo, lowpass, highpass
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT void aurora_audio_src_set_reverb(AuroraAudioSrc* src, double mix, double decay);
AURORA_EXPORT void aurora_audio_src_set_echo(AuroraAudioSrc* src, double delay_ms, double decay);
AURORA_EXPORT void aurora_audio_src_set_lowpass(AuroraAudioSrc* src, double cutoff_hz);
AURORA_EXPORT void aurora_audio_src_set_highpass(AuroraAudioSrc* src, double cutoff_hz);
AURORA_EXPORT void aurora_audio_src_clear_effects(AuroraAudioSrc* src);

/* ════════════════════════════════════════════════════════════
   Recording
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraAudioRec* aurora_audio_rec_new(int sample_rate, int channels);
AURORA_EXPORT void            aurora_audio_rec_start(AuroraAudioRec* rec);
AURORA_EXPORT int             aurora_audio_rec_read(AuroraAudioRec* rec, void* buf, int frames);
AURORA_EXPORT void            aurora_audio_rec_stop(AuroraAudioRec* rec);
AURORA_EXPORT void            aurora_audio_rec_free(AuroraAudioRec* rec);

/* ════════════════════════════════════════════════════════════
   Legacy compat (engine.cpp)
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT int  aurora_audio_play_file(const char* path);
AURORA_EXPORT void aurora_audio_play(const char* clip_path);
AURORA_EXPORT void aurora_audio_play_tone(int frequency, int duration_ms);
AURORA_EXPORT void aurora_audio_stop_all(void);

#ifdef __cplusplus
}
#endif
