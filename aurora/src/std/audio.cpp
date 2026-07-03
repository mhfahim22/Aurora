/* ════════════════════════════════════════════════════════════
   audio.cpp — Audio System (Phase 11)
   Playback, recording, effects, volume, pitch, loop
   Uses miniaudio (single-header C library)
   ════════════════════════════════════════════════════════════ */

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENGINE
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_GENERATION

#include "../../third_party/miniaudio.h"
#include "../../include/std/audio.hpp"

#include <cstring>
#include <cstdlib>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* ════════════════════════════════════════════════════════════
   Internal helpers
   ════════════════════════════════════════════════════════════ */

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* ── Simple biquad filter (lowpass / highpass) ── */
struct Biquad {
    float b0, b1, b2, a1, a2;
    float x1, x2, y1, y2;
    int active;

    void init_lowpass(float cutoff, float sample_rate) {
        if (cutoff <= 0 || cutoff >= sample_rate * 0.5f) { active = 0; return; }
        float w0 = 2 * M_PI * cutoff / sample_rate;
        float alpha = sinf(w0) / (2 * 0.7071f);
        float cos_w0 = cosf(w0);
        float a0 = 1 + alpha;
        b0 = (1 - cos_w0) / (2 * a0);
        b1 = (1 - cos_w0) / a0;
        b2 = (1 - cos_w0) / (2 * a0);
        a1 = (-2 * cos_w0) / a0;
        a2 = (1 - alpha) / a0;
        active = 1;
    }

    void init_highpass(float cutoff, float sample_rate) {
        if (cutoff <= 0 || cutoff >= sample_rate * 0.5f) { active = 0; return; }
        float w0 = 2 * M_PI * cutoff / sample_rate;
        float alpha = sinf(w0) / (2 * 0.7071f);
        float cos_w0 = cosf(w0);
        float a0 = 1 + alpha;
        b0 = (1 + cos_w0) / (2 * a0);
        b1 = -(1 + cos_w0) / a0;
        b2 = (1 + cos_w0) / (2 * a0);
        a1 = (-2 * cos_w0) / a0;
        a2 = (1 - alpha) / a0;
        active = 1;
    }

    float process(float in) {
        if (!active) return in;
        float out = b0 * in + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = in;
        y2 = y1; y1 = out;
        return out;
    }

    void reset() { x1 = x2 = y1 = y2 = 0; active = 0; }
};

/* ── Simple delay line (echo) ── */
struct DelayLine {
    float* buf;
    int capacity;
    int write_pos;
    int active;

    void init(int max_delay_samples) {
        buf = (float*)calloc((size_t)max_delay_samples, sizeof(float));
        capacity = max_delay_samples;
        write_pos = 0;
        active = 0;
    }

    float process(float in, int delay_samples, float decay) {
        if (!active || delay_samples <= 0 || delay_samples >= capacity) return 0;
        float delayed = buf[(write_pos - delay_samples + capacity) % capacity];
        buf[write_pos] = in + delayed * decay;
        write_pos = (write_pos + 1) % capacity;
        return delayed;
    }

    void free_buf() { free(buf); buf = nullptr; capacity = 0; active = 0; }
};

/* ════════════════════════════════════════════════════════════
   AuroraAudioSrc
   ════════════════════════════════════════════════════════════ */

struct AuroraAudioSrc {
    float* pcm;          /* decoded PCM data (float) */
    ma_uint64 num_frames;
    ma_uint32 channels;
    ma_uint32 sample_rate;

    float read_pos;      /* sub-frame read position for pitch resampling */
    int playing;
    int paused;
    int loop;
    float volume;
    float pitch;

    Biquad lowpass;
    Biquad highpass;
    DelayLine echo;
    float echo_delay_ms;
    float echo_decay;
    float reverb_mix;
    float reverb_decay;
    DelayLine reverb_line;
    int reverb_delay_samples;
};

/* ════════════════════════════════════════════════════════════
   Global playback device
   ════════════════════════════════════════════════════════════ */

static ma_device g_playback_device;
static int g_audio_initialized = 0;
static std::vector<AuroraAudioSrc*> g_sources;
static ma_mutex g_src_mutex;

static void playback_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    (void)pInput;
    float* out = (float*)pOutput;
    ma_uint32 channels = pDevice->playback.channels;
    memset(out, 0, (size_t)frameCount * channels * sizeof(float));

    ma_mutex_lock(&g_src_mutex);
    for (auto* src : g_sources) {
        if (!src->playing || src->paused) continue;

        for (ma_uint32 f = 0; f < frameCount; ++f) {
            float left = 0, right = 0;

            if (src->read_pos >= (float)src->num_frames) {
                if (src->loop) {
                    src->read_pos = 0;
                } else {
                    src->playing = 0;
                    break;
                }
            }

            int pos = (int)src->read_pos;
            float frac = src->read_pos - pos;
            ma_uint64 next = (pos + 1 < (int)src->num_frames) ? pos + 1 : pos;

            if (src->channels == 1) {
                float s0 = src->pcm[pos];
                float s1 = src->pcm[next];
                left = right = s0 + (s1 - s0) * frac;
            } else {
                float s0_l = src->pcm[pos * 2];
                float s0_r = src->pcm[pos * 2 + 1];
                float s1_l = src->pcm[next * 2];
                float s1_r = src->pcm[next * 2 + 1];
                left  = s0_l + (s1_l - s0_l) * frac;
                right = s0_r + (s1_r - s0_r) * frac;
            }

            /* Effects */
            left  = src->lowpass.process(left);
            right = src->highpass.process(right);

            /* Echo */
            float echo_l = src->echo.process(left, (int)(src->echo_delay_ms * src->sample_rate / 1000), src->echo_decay);
            float echo_r = src->echo.process(right, (int)(src->echo_delay_ms * src->sample_rate / 1000), src->echo_decay);

            /* Reverb (simple delay-based) */
            float rev_l = src->reverb_line.process(left, src->reverb_delay_samples, src->reverb_decay);
            float rev_r = src->reverb_line.process(right, src->reverb_delay_samples, src->reverb_decay);

            left  = left  * src->volume + echo_l * 0.3f + rev_l * src->reverb_mix;
            right = right * src->volume + echo_r * 0.3f + rev_r * src->reverb_mix;

            if (channels == 1) {
                out[f] += (left + right) * 0.5f;
            } else {
                out[f * 2]     += left;
                out[f * 2 + 1] += right;
            }

            src->read_pos += src->pitch;
        }
    }
    ma_mutex_unlock(&g_src_mutex);
}

/* ════════════════════════════════════════════════════════════
   Lifecycle
   ════════════════════════════════════════════════════════════ */

int aurora_audio_init(void) {
    if (g_audio_initialized) return 1;

    ma_result result;
    result = ma_mutex_init(&g_src_mutex);
    if (result != MA_SUCCESS) return 0;

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate        = 44100;
    config.dataCallback      = playback_callback;
    config.pUserData         = nullptr;

    result = ma_device_init(NULL, &config, &g_playback_device);
    if (result != MA_SUCCESS) {
        ma_mutex_uninit(&g_src_mutex);
        return 0;
    }

    g_audio_initialized = 1;
    return 1;
}

void aurora_audio_shutdown(void) {
    if (!g_audio_initialized) return;

    ma_device_stop(&g_playback_device);

    ma_mutex_lock(&g_src_mutex);
    for (auto* src : g_sources) {
        free(src->pcm);
        src->echo.free_buf();
        src->reverb_line.free_buf();
        free(src);
    }
    g_sources.clear();
    ma_mutex_unlock(&g_src_mutex);

    ma_device_uninit(&g_playback_device);
    ma_mutex_uninit(&g_src_mutex);
    g_audio_initialized = 0;
}

/* ════════════════════════════════════════════════════════════
   Source creation / loading
   ════════════════════════════════════════════════════════════ */

AuroraAudioSrc* aurora_audio_src_new(void) {
    if (!g_audio_initialized) return nullptr;
    AuroraAudioSrc* src = (AuroraAudioSrc*)calloc(1, sizeof(AuroraAudioSrc));
    if (!src) return nullptr;
    src->volume = 1.0f;
    src->pitch = 1.0f;
    src->playing = 0;
    src->paused = 0;
    src->loop = 0;
    src->read_pos = 0;
    src->echo_delay_ms = 200;
    src->echo_decay = 0.5f;
    src->reverb_mix = 0;
    src->reverb_decay = 0.5f;
    src->reverb_delay_samples = 0;
    src->echo.buf = nullptr;
    src->echo.capacity = 0;
    src->echo.active = 0;
    src->reverb_line.buf = nullptr;
    src->reverb_line.capacity = 0;
    src->reverb_line.active = 0;
    return src;
}

int aurora_audio_src_load_file(AuroraAudioSrc* src, const char* path) {
    if (!src || !path) return 0;

    ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 0, 0);
    ma_decoder decoder;
    ma_result result = ma_decoder_init_file(path, &cfg, &decoder);
    if (result != MA_SUCCESS) return 0;

    ma_uint64 total_frames;
    result = ma_decoder_get_length_in_pcm_frames(&decoder, &total_frames);
    if (result != MA_SUCCESS) { ma_decoder_uninit(&decoder); return 0; }

    src->channels    = decoder.outputChannels;
    src->sample_rate = decoder.outputSampleRate;

    free(src->pcm);
    src->pcm = (float*)malloc((size_t)(total_frames * src->channels * sizeof(float)));
    if (!src->pcm) { ma_decoder_uninit(&decoder); return 0; }

    ma_uint64 frames_read;
    result = ma_decoder_read_pcm_frames(&decoder, src->pcm, total_frames, &frames_read);
    ma_decoder_uninit(&decoder);
    if (result != MA_SUCCESS) { free(src->pcm); src->pcm = nullptr; return 0; }

    src->num_frames = frames_read;
    src->read_pos = 0;

    /* Init echo/reverb delay lines */
    int max_delay = (int)(src->sample_rate * 2);
    src->echo.free_buf();
    src->echo.init(max_delay);
    src->reverb_line.free_buf();
    src->reverb_line.init(max_delay);

    return 1;
}

int aurora_audio_src_load_mem(AuroraAudioSrc* src, const void* data, int len) {
    if (!src || !data || len <= 0) return 0;

    ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 0, 0);
    ma_decoder decoder;
    ma_result result = ma_decoder_init_memory(data, (size_t)len, &cfg, &decoder);
    if (result != MA_SUCCESS) return 0;

    ma_uint64 total_frames;
    result = ma_decoder_get_length_in_pcm_frames(&decoder, &total_frames);
    if (result != MA_SUCCESS) { ma_decoder_uninit(&decoder); return 0; }

    src->channels    = decoder.outputChannels;
    src->sample_rate = decoder.outputSampleRate;

    free(src->pcm);
    src->pcm = (float*)malloc((size_t)(total_frames * src->channels * sizeof(float)));
    if (!src->pcm) { ma_decoder_uninit(&decoder); return 0; }

    ma_uint64 frames_read;
    result = ma_decoder_read_pcm_frames(&decoder, src->pcm, total_frames, &frames_read);
    ma_decoder_uninit(&decoder);
    if (result != MA_SUCCESS) { free(src->pcm); src->pcm = nullptr; return 0; }

    src->num_frames = frames_read;
    src->read_pos = 0;

    int max_delay = (int)(src->sample_rate * 2);
    src->echo.free_buf();
    src->echo.init(max_delay);
    src->reverb_line.free_buf();
    src->reverb_line.init(max_delay);

    return 1;
}

/* ════════════════════════════════════════════════════════════
   Play / Stop / Pause / Resume
   ════════════════════════════════════════════════════════════ */

void aurora_audio_src_play(AuroraAudioSrc* src) {
    if (!src || !src->pcm) return;
    ma_mutex_lock(&g_src_mutex);
    src->playing = 1;
    src->paused = 0;
    src->read_pos = 0;

    src->lowpass.reset();
    src->highpass.reset();

    /* Check if already in list */
    int found = 0;
    for (auto* s : g_sources) { if (s == src) { found = 1; break; } }
    if (!found) g_sources.push_back(src);

    ma_mutex_unlock(&g_src_mutex);
    ma_device_start(&g_playback_device);
}

void aurora_audio_src_stop(AuroraAudioSrc* src) {
    if (!src) return;
    ma_mutex_lock(&g_src_mutex);
    src->playing = 0;
    src->paused = 0;
    src->read_pos = 0;
    ma_mutex_unlock(&g_src_mutex);
}

void aurora_audio_src_pause(AuroraAudioSrc* src) {
    if (!src) return;
    ma_mutex_lock(&g_src_mutex);
    src->paused = 1;
    ma_mutex_unlock(&g_src_mutex);
}

void aurora_audio_src_resume(AuroraAudioSrc* src) {
    if (!src) return;
    ma_mutex_lock(&g_src_mutex);
    if (src->paused) src->paused = 0;
    ma_mutex_unlock(&g_src_mutex);
    ma_device_start(&g_playback_device);
}

int aurora_audio_src_is_playing(AuroraAudioSrc* src) {
    return src ? src->playing : 0;
}

void aurora_audio_src_free(AuroraAudioSrc* src) {
    if (!src) return;
    ma_mutex_lock(&g_src_mutex);
    src->playing = 0;
    /* Remove from global list */
    for (auto it = g_sources.begin(); it != g_sources.end(); ++it) {
        if (*it == src) { g_sources.erase(it); break; }
    }
    ma_mutex_unlock(&g_src_mutex);

    free(src->pcm);
    src->echo.free_buf();
    src->reverb_line.free_buf();
    free(src);
}

/* ════════════════════════════════════════════════════════════
   Volume / Pitch / Loop / Time / Duration
   ════════════════════════════════════════════════════════════ */

void aurora_audio_src_set_volume(AuroraAudioSrc* src, double vol) {
    if (src) src->volume = (float)clampf((float)vol, 0, 4);
}
double aurora_audio_src_get_volume(AuroraAudioSrc* src) {
    return src ? (double)src->volume : 0;
}

void aurora_audio_src_set_pitch(AuroraAudioSrc* src, double pitch) {
    if (src) src->pitch = (float)clampf((float)pitch, 0.0625f, 16);
}
double aurora_audio_src_get_pitch(AuroraAudioSrc* src) {
    return src ? (double)src->pitch : 1;
}

void aurora_audio_src_set_looping(AuroraAudioSrc* src, int loop) {
    if (src) src->loop = loop ? 1 : 0;
}
int aurora_audio_src_get_looping(AuroraAudioSrc* src) {
    return src ? src->loop : 0;
}

void aurora_audio_src_set_time(AuroraAudioSrc* src, double sec) {
    if (src && src->sample_rate > 0) {
        src->read_pos = clampf((float)(sec * src->sample_rate), 0, (float)src->num_frames - 1);
    }
}
double aurora_audio_src_get_time(AuroraAudioSrc* src) {
    if (!src || src->sample_rate <= 0) return 0;
    return (double)(src->read_pos / src->sample_rate);
}
double aurora_audio_src_get_duration(AuroraAudioSrc* src) {
    if (!src || src->sample_rate <= 0) return 0;
    return (double)src->num_frames / src->sample_rate;
}

/* ════════════════════════════════════════════════════════════
   Effects
   ════════════════════════════════════════════════════════════ */

void aurora_audio_src_set_reverb(AuroraAudioSrc* src, double mix, double decay) {
    if (!src) return;
    src->reverb_mix = clampf((float)mix, 0, 1);
    src->reverb_decay = clampf((float)decay, 0, 0.99f);
    src->reverb_delay_samples = src->sample_rate > 0 ? (int)(0.03f * src->sample_rate) : 0;
    if (src->reverb_mix > 0) src->reverb_line.active = 1;
}

void aurora_audio_src_set_echo(AuroraAudioSrc* src, double delay_ms, double decay) {
    if (!src) return;
    src->echo_delay_ms = clampf((float)delay_ms, 10, 2000);
    src->echo_decay = clampf((float)decay, 0, 0.99f);
    if (src->echo_delay_ms > 0) src->echo.active = 1;
}

void aurora_audio_src_set_lowpass(AuroraAudioSrc* src, double cutoff_hz) {
    if (!src || src->sample_rate <= 0) return;
    src->lowpass.init_lowpass((float)cutoff_hz, (float)src->sample_rate);
}

void aurora_audio_src_set_highpass(AuroraAudioSrc* src, double cutoff_hz) {
    if (!src || src->sample_rate <= 0) return;
    src->highpass.init_highpass((float)cutoff_hz, (float)src->sample_rate);
}

void aurora_audio_src_clear_effects(AuroraAudioSrc* src) {
    if (!src) return;
    src->lowpass.reset();
    src->highpass.reset();
    src->echo.active = 0;
    src->reverb_line.active = 0;
    src->reverb_mix = 0;
    src->echo_delay_ms = 200;
    src->echo_decay = 0.5f;
}

/* ════════════════════════════════════════════════════════════
   Recording
   ════════════════════════════════════════════════════════════ */

struct AuroraAudioRec {
    ma_device device;
    ma_device_config config;
    float* ring;
    int ring_capacity;
    int ring_write;
    int ring_read;
    int channels;
    int sample_rate;
    int running;
};

static void capture_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    (void)pOutput;
    auto* rec = (AuroraAudioRec*)pDevice->pUserData;
    if (!rec || !rec->running) return;
    const float* in = (const float*)pInput;

    ma_mutex_lock(&g_src_mutex);
    for (ma_uint32 f = 0; f < frameCount; ++f) {
        for (ma_uint32 c = 0; c < rec->channels; ++c) {
            int idx = rec->ring_write * rec->channels + c;
            rec->ring[idx] = in[f * rec->channels + c];
        }
        rec->ring_write = (rec->ring_write + 1) % rec->ring_capacity;
        if (rec->ring_write == rec->ring_read) {
            rec->ring_read = (rec->ring_read + 1) % rec->ring_capacity;
        }
    }
    ma_mutex_unlock(&g_src_mutex);
}

AuroraAudioRec* aurora_audio_rec_new(int sample_rate, int channels) {
    if (!g_audio_initialized) return nullptr;
    AuroraAudioRec* rec = (AuroraAudioRec*)calloc(1, sizeof(AuroraAudioRec));
    if (!rec) return nullptr;

    rec->sample_rate = (sample_rate > 0) ? sample_rate : 44100;
    rec->channels = (channels > 0) ? channels : 1;
    rec->ring_capacity = rec->sample_rate * 2; /* 2 seconds */
    rec->ring = (float*)calloc((size_t)(rec->ring_capacity * rec->channels), sizeof(float));
    if (!rec->ring) { free(rec); return nullptr; }
    rec->ring_write = 0;
    rec->ring_read = 0;
    rec->running = 0;

    rec->config = ma_device_config_init(ma_device_type_capture);
    rec->config.capture.format   = ma_format_f32;
    rec->config.capture.channels = (ma_uint32)rec->channels;
    rec->config.sampleRate       = (ma_uint32)rec->sample_rate;
    rec->config.dataCallback     = capture_callback;
    rec->config.pUserData        = rec;

    ma_result result = ma_device_init(NULL, &rec->config, &rec->device);
    if (result != MA_SUCCESS) {
        free(rec->ring);
        free(rec);
        return nullptr;
    }

    return rec;
}

void aurora_audio_rec_start(AuroraAudioRec* rec) {
    if (!rec || rec->running) return;
    rec->running = 1;
    rec->ring_write = 0;
    rec->ring_read = 0;
    ma_device_start(&rec->device);
}

int aurora_audio_rec_read(AuroraAudioRec* rec, void* buf, int frames) {
    if (!rec || !buf || frames <= 0) return 0;
    float* out = (float*)buf;
    int total = 0;

    ma_mutex_lock(&g_src_mutex);
    while (total < frames && rec->ring_read != rec->ring_write) {
        for (int c = 0; c < rec->channels; ++c) {
            out[total * rec->channels + c] = rec->ring[rec->ring_read * rec->channels + c];
        }
        rec->ring_read = (rec->ring_read + 1) % rec->ring_capacity;
        total++;
    }
    ma_mutex_unlock(&g_src_mutex);
    return total;
}

void aurora_audio_rec_stop(AuroraAudioRec* rec) {
    if (!rec) return;
    rec->running = 0;
    ma_device_stop(&rec->device);
}

void aurora_audio_rec_free(AuroraAudioRec* rec) {
    if (!rec) return;
    rec->running = 0;
    ma_device_stop(&rec->device);
    ma_device_uninit(&rec->device);
    free(rec->ring);
    free(rec);
}

/* ════════════════════════════════════════════════════════════
   Legacy compat
   ════════════════════════════════════════════════════════════ */

int aurora_audio_play_file(const char* path) {
    if (!path) return 0;
    /* Create a temporary source, play it, free when done */
    AuroraAudioSrc* src = aurora_audio_src_new();
    if (!src) return 0;
    if (!aurora_audio_src_load_file(src, path)) {
        aurora_audio_src_free(src);
        return 0;
    }
    aurora_audio_src_play(src);
    return 1;
}

void aurora_audio_play(const char* clip_path) {
    aurora_audio_play_file(clip_path);
}

void aurora_audio_play_tone(int frequency, int duration_ms) {
    if (frequency < 37 || duration_ms < 1) return;
    /* Generate a sine wave tone in memory */
    int sample_rate = 44100;
    int channels = 1;
    int total_samples = (int)(sample_rate * duration_ms / 1000.0f);
    if (total_samples < 1) return;

    float* pcm = (float*)malloc((size_t)total_samples * sizeof(float));
    if (!pcm) return;
    for (int i = 0; i < total_samples; ++i) {
        float t = (float)i / sample_rate;
        float env = 1.0f;
        if (i < 100) env = (float)i / 100.0f;
        if (i > total_samples - 200) env = (float)(total_samples - i) / 200.0f;
        pcm[i] = sinf(2 * M_PI * frequency * t) * env;
    }

    AuroraAudioSrc* src = aurora_audio_src_new();
    if (!src) { free(pcm); return; }
    src->pcm = pcm;
    src->num_frames = total_samples;
    src->channels = 1;
    src->sample_rate = sample_rate;

    int max_delay = sample_rate * 2;
    src->echo.init(max_delay);
    src->reverb_line.init(max_delay);

    aurora_audio_src_play(src);
}

void aurora_audio_stop_all(void) {
    ma_mutex_lock(&g_src_mutex);
    for (auto* src : g_sources) {
        src->playing = 0;
        src->paused = 0;
        src->read_pos = 0;
    }
    ma_mutex_unlock(&g_src_mutex);
}
