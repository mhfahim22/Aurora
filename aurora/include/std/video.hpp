#pragma once
#include "common/platform.hpp"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Opaque type ── */
typedef struct AuroraVideoPlayer AuroraVideoPlayer;

/* ════════════════════════════════════════════════════════════
   Lifecycle
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT int  aurora_video_init(void);
AURORA_EXPORT void aurora_video_shutdown(void);

/* ════════════════════════════════════════════════════════════
   Player — open, play, pause, stop, free
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraVideoPlayer* aurora_video_player_new(void);
AURORA_EXPORT int                aurora_video_player_open(AuroraVideoPlayer* p, const char* path);
AURORA_EXPORT void               aurora_video_player_play(AuroraVideoPlayer* p);
AURORA_EXPORT void               aurora_video_player_pause(AuroraVideoPlayer* p);
AURORA_EXPORT void               aurora_video_player_resume(AuroraVideoPlayer* p);
AURORA_EXPORT void               aurora_video_player_stop(AuroraVideoPlayer* p);
AURORA_EXPORT int                aurora_video_player_is_playing(AuroraVideoPlayer* p);
AURORA_EXPORT int                aurora_video_player_has_ended(AuroraVideoPlayer* p);
AURORA_EXPORT void               aurora_video_player_free(AuroraVideoPlayer* p);

/* ════════════════════════════════════════════════════════════
   Seeking / Time / Duration
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT void   aurora_video_player_set_time(AuroraVideoPlayer* p, double sec);
AURORA_EXPORT double aurora_video_player_get_time(AuroraVideoPlayer* p);
AURORA_EXPORT double aurora_video_player_get_duration(AuroraVideoPlayer* p);

/* ════════════════════════════════════════════════════════════
   Properties
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT int    aurora_video_player_get_width(AuroraVideoPlayer* p);
AURORA_EXPORT int    aurora_video_player_get_height(AuroraVideoPlayer* p);
AURORA_EXPORT double aurora_video_player_get_fps(AuroraVideoPlayer* p);

/* ════════════════════════════════════════════════════════════
   Volume / Loop
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT void  aurora_video_player_set_volume(AuroraVideoPlayer* p, double vol);
AURORA_EXPORT double aurora_video_player_get_volume(AuroraVideoPlayer* p);
AURORA_EXPORT void  aurora_video_player_set_looping(AuroraVideoPlayer* p, int loop);
AURORA_EXPORT int   aurora_video_player_get_looping(AuroraVideoPlayer* p);

/* ════════════════════════════════════════════════════════════
   Subtitle
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT int          aurora_video_player_subtitle_load(AuroraVideoPlayer* p, const char* srt_path);
AURORA_EXPORT const char*  aurora_video_player_subtitle_get_text(AuroraVideoPlayer* p);

/* ════════════════════════════════════════════════════════════
   Frame — get current decoded RGBA frame
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT int     aurora_video_player_read_frame(AuroraVideoPlayer* p, void* rgba_buf, int buf_size);
AURORA_EXPORT void    aurora_video_player_decode(AuroraVideoPlayer* p, double dt);

#ifdef __cplusplus
}
#endif
