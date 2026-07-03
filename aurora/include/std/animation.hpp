#pragma once
#include <cstdint>
#include "common/platform.hpp"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Easing type enum ── */
#define AURORA_EASE_LINEAR         0
#define AURORA_EASE_IN_QUAD        1
#define AURORA_EASE_OUT_QUAD       2
#define AURORA_EASE_IN_OUT_QUAD    3
#define AURORA_EASE_IN_CUBIC       4
#define AURORA_EASE_OUT_CUBIC      5
#define AURORA_EASE_IN_OUT_CUBIC   6
#define AURORA_EASE_IN_QUART       7
#define AURORA_EASE_OUT_QUART      8
#define AURORA_EASE_IN_OUT_QUART   9
#define AURORA_EASE_IN_ELASTIC    10
#define AURORA_EASE_OUT_ELASTIC   11
#define AURORA_EASE_IN_OUT_ELASTIC 12
#define AURORA_EASE_IN_BOUNCE     13
#define AURORA_EASE_OUT_BOUNCE    14
#define AURORA_EASE_IN_OUT_BOUNCE 15
#define AURORA_EASE_IN_BACK       16
#define AURORA_EASE_OUT_BACK      17
#define AURORA_EASE_IN_OUT_BACK   18

/* ── Opaque types ── */
typedef struct AuroraTween   AuroraTween;
typedef struct AuroraAnimSeq AuroraAnimSeq;
typedef struct AuroraAnimCtrl AuroraAnimCtrl;
typedef struct AuroraAnimKf   AuroraAnimKf;

/* ════════════════════════════════════════════════════════════
   Easing functions
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT float aurora_anim_ease_linear(float t);
AURORA_EXPORT float aurora_anim_ease_in_quad(float t);
AURORA_EXPORT float aurora_anim_ease_out_quad(float t);
AURORA_EXPORT float aurora_anim_ease_in_out_quad(float t);
AURORA_EXPORT float aurora_anim_ease_in_cubic(float t);
AURORA_EXPORT float aurora_anim_ease_out_cubic(float t);
AURORA_EXPORT float aurora_anim_ease_in_out_cubic(float t);
AURORA_EXPORT float aurora_anim_ease_in_quart(float t);
AURORA_EXPORT float aurora_anim_ease_out_quart(float t);
AURORA_EXPORT float aurora_anim_ease_in_out_quart(float t);
AURORA_EXPORT float aurora_anim_ease_in_elastic(float t);
AURORA_EXPORT float aurora_anim_ease_out_elastic(float t);
AURORA_EXPORT float aurora_anim_ease_in_out_elastic(float t);
AURORA_EXPORT float aurora_anim_ease_in_bounce(float t);
AURORA_EXPORT float aurora_anim_ease_out_bounce(float t);
AURORA_EXPORT float aurora_anim_ease_in_out_bounce(float t);
AURORA_EXPORT float aurora_anim_ease_in_back(float t);
AURORA_EXPORT float aurora_anim_ease_out_back(float t);
AURORA_EXPORT float aurora_anim_ease_in_out_back(float t);
AURORA_EXPORT float aurora_anim_ease_apply(int easing, float t);

/* ════════════════════════════════════════════════════════════
   Tween — animate a single float property
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraTween* aurora_anim_tween(float from, float to, float duration, int easing);
AURORA_EXPORT void         aurora_anim_tween_on_update(AuroraTween* t, void (*cb)(void*, float), void* userdata);
AURORA_EXPORT void         aurora_anim_tween_on_done(AuroraTween* t, void (*cb)(void*), void* userdata);
AURORA_EXPORT void         aurora_anim_tween_update(AuroraTween* t, float dt);
AURORA_EXPORT int          aurora_anim_tween_is_done(AuroraTween* t);
AURORA_EXPORT float        aurora_anim_tween_value(AuroraTween* t);
AURORA_EXPORT void         aurora_anim_tween_free(AuroraTween* t);

/* ════════════════════════════════════════════════════════════
   Sequence — run tweens in series
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraAnimSeq* aurora_anim_seq_new();
AURORA_EXPORT void           aurora_anim_seq_add(AuroraAnimSeq* seq, float from, float to, float duration, int easing);
AURORA_EXPORT void           aurora_anim_seq_on_update(AuroraAnimSeq* seq, void (*cb)(void*, float), void* userdata);
AURORA_EXPORT void           aurora_anim_seq_on_done(AuroraAnimSeq* seq, void (*cb)(void*), void* userdata);
AURORA_EXPORT void           aurora_anim_seq_update(AuroraAnimSeq* seq, float dt);
AURORA_EXPORT int            aurora_anim_seq_is_done(AuroraAnimSeq* seq);
AURORA_EXPORT void           aurora_anim_seq_free(AuroraAnimSeq* seq);

/* ════════════════════════════════════════════════════════════
   Controller — run multiple tweens in parallel
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraAnimCtrl* aurora_anim_ctrl_new();
AURORA_EXPORT void            aurora_anim_ctrl_add(AuroraAnimCtrl* ctrl, AuroraTween* tween);
AURORA_EXPORT void            aurora_anim_ctrl_update(AuroraAnimCtrl* ctrl, float dt);
AURORA_EXPORT int             aurora_anim_ctrl_is_done(AuroraAnimCtrl* ctrl);
AURORA_EXPORT void            aurora_anim_ctrl_free(AuroraAnimCtrl* ctrl);

/* ════════════════════════════════════════════════════════════
   Keyframe — evaluate keyframe curves
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraAnimKf* aurora_anim_kf_new(int num_frames);
AURORA_EXPORT void          aurora_anim_kf_set_key(AuroraAnimKf* kf, int index, float time, float value);
AURORA_EXPORT float         aurora_anim_kf_evaluate(AuroraAnimKf* kf, float t);
AURORA_EXPORT void          aurora_anim_kf_free(AuroraAnimKf* kf);

#ifdef __cplusplus
}
#endif
