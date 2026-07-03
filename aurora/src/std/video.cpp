/* ════════════════════════════════════════════════════════════
   video.cpp — Video System (Phase 12)
   MPEG-1 video playback via pl_mpeg, subtitle support
   ════════════════════════════════════════════════════════════ */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <string>

#define PL_MPEG_IMPLEMENTATION
#include "../../third_party/pl_mpeg.h"
#include "../../include/std/video.hpp"

/* ════════════════════════════════════════════════════════════
   Subtitle entry (SRT format)
   ════════════════════════════════════════════════════════════ */

struct SubEntry {
    double start, end;
    std::string text;
};

/* ════════════════════════════════════════════════════════════
   AuroraVideoPlayer
   ════════════════════════════════════════════════════════════ */

struct AuroraVideoPlayer {
    plm_t* plm;
    int playing;
    int paused;
    double volume;
    double current_time;

    /* Frame data */
    int width;
    int height;
    double fps;
    double duration;
    uint8_t* rgba;   /* last decoded frame in RGBA */
    int rgba_size;

    /* Subtitles */
    std::vector<SubEntry> subs;
    int sub_loaded;
    std::string current_sub;
};

/* ════════════════════════════════════════════════════════════
   Lifecycle
   ════════════════════════════════════════════════════════════ */

static int g_video_initialized = 0;

int aurora_video_init(void) {
    if (g_video_initialized) return 1;
    g_video_initialized = 1;
    return 1;
}

void aurora_video_shutdown(void) {
    g_video_initialized = 0;
}

/* ════════════════════════════════════════════════════════════
   Player
   ════════════════════════════════════════════════════════════ */

AuroraVideoPlayer* aurora_video_player_new(void) {
    if (!g_video_initialized) return nullptr;
    AuroraVideoPlayer* p = (AuroraVideoPlayer*)calloc(1, sizeof(AuroraVideoPlayer));
    if (!p) return nullptr;
    p->volume = 1.0;
    p->playing = 0;
    p->paused = 0;
    p->plm = nullptr;
    p->rgba = nullptr;
    p->rgba_size = 0;
    p->sub_loaded = 0;
    return p;
}

int aurora_video_player_open(AuroraVideoPlayer* p, const char* path) {
    if (!p || !path) return 0;

    /* Close previous */
    if (p->plm) {
        plm_destroy(p->plm);
        p->plm = nullptr;
    }
    free(p->rgba);
    p->rgba = nullptr;
    p->rgba_size = 0;

    p->plm = plm_create_with_filename(path);
    if (!p->plm) return 0;

    /* Wait for headers */
    while (!plm_has_headers(p->plm)) {
        plm_decode(p->plm, 0.1);
    }

    p->width    = plm_get_width(p->plm);
    p->height   = plm_get_height(p->plm);
    p->fps      = plm_get_framerate(p->plm);
    p->duration = plm_get_duration(p->plm);

    /* Allocate RGBA buffer */
    int stride = p->width * 4;
    p->rgba_size = stride * p->height;
    p->rgba = (uint8_t*)malloc((size_t)p->rgba_size);
    if (!p->rgba) { plm_destroy(p->plm); p->plm = nullptr; return 0; }
    memset(p->rgba, 0, (size_t)p->rgba_size);

    /* Disable audio decoding (we handle audio separately via the audio system) */
    plm_set_audio_enabled(p->plm, 0);

    /* Rewind to start */
    plm_rewind(p->plm);
    p->current_time = 0;
    p->playing = 0;
    p->paused = 0;

    return 1;
}

void aurora_video_player_play(AuroraVideoPlayer* p) {
    if (!p || !p->plm) return;
    p->playing = 1;
    p->paused = 0;
    plm_rewind(p->plm);
    p->current_time = 0;
}

void aurora_video_player_pause(AuroraVideoPlayer* p) {
    if (!p) return;
    p->paused = 1;
}

void aurora_video_player_resume(AuroraVideoPlayer* p) {
    if (!p) return;
    p->paused = 0;
    p->playing = 1;
}

void aurora_video_player_stop(AuroraVideoPlayer* p) {
    if (!p) return;
    p->playing = 0;
    p->paused = 0;
    p->current_time = 0;
    if (p->plm) plm_rewind(p->plm);
}

int aurora_video_player_is_playing(AuroraVideoPlayer* p) {
    return p ? (p->playing && !p->paused) : 0;
}

int aurora_video_player_has_ended(AuroraVideoPlayer* p) {
    if (!p || !p->plm) return 1;
    return plm_has_ended(p->plm) ? 1 : 0;
}

void aurora_video_player_free(AuroraVideoPlayer* p) {
    if (!p) return;
    if (p->plm) plm_destroy(p->plm);
    free(p->rgba);
    free(p);
}

/* ════════════════════════════════════════════════════════════
   Seeking / Time / Duration
   ════════════════════════════════════════════════════════════ */

void aurora_video_player_set_time(AuroraVideoPlayer* p, double sec) {
    if (!p || !p->plm) return;
    if (sec < 0) sec = 0;
    if (sec > p->duration) sec = p->duration;
    plm_seek(p->plm, sec, 1);
    p->current_time = sec;
    /* Decode one frame at new position */
    plm_frame_t* frame = plm_decode_video(p->plm);
    if (frame) {
        plm_frame_to_rgba(frame, p->rgba, p->width * 4);
        p->current_time = frame->time;
    }
}

double aurora_video_player_get_time(AuroraVideoPlayer* p) {
    return p ? p->current_time : 0;
}

double aurora_video_player_get_duration(AuroraVideoPlayer* p) {
    return p ? p->duration : 0;
}

/* ════════════════════════════════════════════════════════════
   Properties
   ════════════════════════════════════════════════════════════ */

int aurora_video_player_get_width(AuroraVideoPlayer* p) {
    return p ? p->width : 0;
}

int aurora_video_player_get_height(AuroraVideoPlayer* p) {
    return p ? p->height : 0;
}

double aurora_video_player_get_fps(AuroraVideoPlayer* p) {
    return p ? p->fps : 0;
}

/* ════════════════════════════════════════════════════════════
   Volume / Loop
   ════════════════════════════════════════════════════════════ */

void aurora_video_player_set_volume(AuroraVideoPlayer* p, double vol) {
    if (p) p->volume = vol < 0 ? 0 : (vol > 4 ? 4 : vol);
}

double aurora_video_player_get_volume(AuroraVideoPlayer* p) {
    return p ? p->volume : 0;
}

void aurora_video_player_set_looping(AuroraVideoPlayer* p, int loop) {
    if (p && p->plm) plm_set_loop(p->plm, loop ? 1 : 0);
}

int aurora_video_player_get_looping(AuroraVideoPlayer* p) {
    return (p && p->plm) ? plm_get_loop(p->plm) : 0;
}

/* ════════════════════════════════════════════════════════════
   Subtitle (SRT parser)
   ════════════════════════════════════════════════════════════ */

static double srt_time_to_sec(const char* s) {
    int h, m, sec, ms;
    if (sscanf(s, "%d:%d:%d,%d", &h, &m, &sec, &ms) >= 3)
        return h * 3600.0 + m * 60.0 + sec + ms / 1000.0;
    return 0;
}

int aurora_video_player_subtitle_load(AuroraVideoPlayer* p, const char* srt_path) {
    if (!p || !srt_path) return 0;
    p->subs.clear();
    p->sub_loaded = 0;

    FILE* f = fopen(srt_path, "r");
    if (!f) return 0;

    char line[1024];
    int state = 0; /* 0 = index, 1 = time, 2+ = text */
    SubEntry cur;
    cur.start = 0; cur.end = 0;

    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[--len] = 0;
        if (len > 0 && line[len - 1] == '\r') line[--len] = 0;

        if (state == 0) {
            if (strchr(line, '-') && strchr(line, '>')) {
                /* Time line */
                char start[64] = {0}, end[64] = {0};
                if (sscanf(line, "%63s --> %63s", start, end) == 2) {
                    cur.start = srt_time_to_sec(start);
                    cur.end = srt_time_to_sec(end);
                    cur.text.clear();
                    state = 2;
                }
            }
        } else if (state == 2) {
            if (line[0] == 0) {
                if (!cur.text.empty()) {
                    p->subs.push_back(cur);
                }
                state = 0;
            } else {
                if (!cur.text.empty()) cur.text += "\n";
                cur.text += line;
            }
        }
    }
    if (!cur.text.empty()) p->subs.push_back(cur);

    fclose(f);
    p->sub_loaded = 1;
    return 1;
}

const char* aurora_video_player_subtitle_get_text(AuroraVideoPlayer* p) {
    if (!p || !p->sub_loaded) return nullptr;
    p->current_sub.clear();
    for (const auto& s : p->subs) {
        if (p->current_time >= s.start && p->current_time <= s.end) {
            p->current_sub = s.text;
            return p->current_sub.c_str();
        }
    }
    return nullptr;
}

/* ════════════════════════════════════════════════════════════
   Frame decoding
   ════════════════════════════════════════════════════════════ */

void aurora_video_player_decode(AuroraVideoPlayer* p, double dt) {
    if (!p || !p->plm || !p->playing || p->paused) return;

    p->current_time += dt;

    plm_frame_t* frame = plm_decode_video(p->plm);
    if (frame) {
        plm_frame_to_rgba(frame, p->rgba, p->width * 4);
        p->current_time = frame->time;
    }

    if (plm_has_ended(p->plm)) {
        p->playing = 0;
    }
}

int aurora_video_player_read_frame(AuroraVideoPlayer* p, void* rgba_buf, int buf_size) {
    if (!p || !rgba_buf || buf_size < p->rgba_size) return 0;
    memcpy(rgba_buf, p->rgba, (size_t)p->rgba_size);
    return 1;
}
