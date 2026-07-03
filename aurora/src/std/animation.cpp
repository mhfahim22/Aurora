/* ════════════════════════════════════════════════════════════
   animation.cpp — Animation System (Phase 10)
   Tweens, easing curves, sequences, controllers, keyframes
   ════════════════════════════════════════════════════════════ */

#include "../../include/std/animation.hpp"
#include <cstdlib>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* ════════════════════════════════════════════════════════════
   Easing helpers
   ════════════════════════════════════════════════════════════ */

static float clamp01(float t) {
    if (t < 0) return 0;
    if (t > 1) return 1;
    return t;
}

float aurora_anim_ease_linear(float t) { return clamp01(t); }

float aurora_anim_ease_in_quad(float t)  { t = clamp01(t); return t * t; }
float aurora_anim_ease_out_quad(float t) { t = clamp01(t); return t * (2 - t); }
float aurora_anim_ease_in_out_quad(float t) {
    t = clamp01(t);
    return t < 0.5f ? 2 * t * t : -1 + (4 - 2 * t) * t;
}

float aurora_anim_ease_in_cubic(float t)  { t = clamp01(t); return t * t * t; }
float aurora_anim_ease_out_cubic(float t) { t = clamp01(t); float u = t - 1; return u * u * u + 1; }
float aurora_anim_ease_in_out_cubic(float t) {
    t = clamp01(t);
    return t < 0.5f ? 4 * t * t * t : (t - 1) * (2 * t - 2) * (2 * t - 2) + 1;
}

float aurora_anim_ease_in_quart(float t)  { t = clamp01(t); return t * t * t * t; }
float aurora_anim_ease_out_quart(float t) { t = clamp01(t); float u = t - 1; return 1 - u * u * u * u; }
float aurora_anim_ease_in_out_quart(float t) {
    t = clamp01(t);
    return t < 0.5f ? 8 * t * t * t * t : 1 - 8 * (t - 1) * (t - 1) * (t - 1) * (t - 1);
}

float aurora_anim_ease_in_elastic(float t) {
    t = clamp01(t);
    if (t == 0 || t == 1) return t;
    return -powf(2, 10 * (t - 1)) * sinf((t - 1.075f) * (2 * M_PI) / 0.3f);
}

float aurora_anim_ease_out_elastic(float t) {
    t = clamp01(t);
    if (t == 0 || t == 1) return t;
    return powf(2, -10 * t) * sinf((t - 0.075f) * (2 * M_PI) / 0.3f) + 1;
}

float aurora_anim_ease_in_out_elastic(float t) {
    t = clamp01(t);
    if (t == 0 || t == 1) return t;
    t *= 2;
    if (t < 1) return -0.5f * powf(2, 10 * (t - 1)) * sinf((t - 1.1f) * (2 * M_PI) / 0.45f);
    return 0.5f * powf(2, -10 * (t - 1)) * sinf((t - 1.1f) * (2 * M_PI) / 0.45f) + 1;
}

float aurora_anim_ease_in_bounce(float t) {
    t = clamp01(t);
    return 1 - aurora_anim_ease_out_bounce(1 - t);
}

float aurora_anim_ease_out_bounce(float t) {
    t = clamp01(t);
    if (t < 1 / 2.75f) return 7.5625f * t * t;
    if (t < 2 / 2.75f) { t -= 1.5f / 2.75f; return 7.5625f * t * t + 0.75f; }
    if (t < 2.5f / 2.75f) { t -= 2.25f / 2.75f; return 7.5625f * t * t + 0.9375f; }
    t -= 2.625f / 2.75f; return 7.5625f * t * t + 0.984375f;
}

float aurora_anim_ease_in_out_bounce(float t) {
    t = clamp01(t);
    return t < 0.5f ? (1 - aurora_anim_ease_out_bounce(1 - 2 * t)) * 0.5f
                    : (1 + aurora_anim_ease_out_bounce(2 * t - 1)) * 0.5f;
}

float aurora_anim_ease_in_back(float t) {
    t = clamp01(t);
    return t * t * (2.70158f * t - 1.70158f);
}

float aurora_anim_ease_out_back(float t) {
    t = clamp01(t);
    float u = t - 1;
    return u * u * (2.70158f * u + 1.70158f) + 1;
}

float aurora_anim_ease_in_out_back(float t) {
    t = clamp01(t);
    t *= 2;
    if (t < 1) return 0.5f * t * t * (3.5949095f * t - 2.5949095f);
    t -= 2;
    return 0.5f * (t * t * (3.5949095f * t + 2.5949095f) + 2);
}

float aurora_anim_ease_apply(int easing, float t) {
    switch (easing) {
        default:
        case  0: return aurora_anim_ease_linear(t);
        case  1: return aurora_anim_ease_in_quad(t);
        case  2: return aurora_anim_ease_out_quad(t);
        case  3: return aurora_anim_ease_in_out_quad(t);
        case  4: return aurora_anim_ease_in_cubic(t);
        case  5: return aurora_anim_ease_out_cubic(t);
        case  6: return aurora_anim_ease_in_out_cubic(t);
        case  7: return aurora_anim_ease_in_quart(t);
        case  8: return aurora_anim_ease_out_quart(t);
        case  9: return aurora_anim_ease_in_out_quart(t);
        case 10: return aurora_anim_ease_in_elastic(t);
        case 11: return aurora_anim_ease_out_elastic(t);
        case 12: return aurora_anim_ease_in_out_elastic(t);
        case 13: return aurora_anim_ease_in_bounce(t);
        case 14: return aurora_anim_ease_out_bounce(t);
        case 15: return aurora_anim_ease_in_out_bounce(t);
        case 16: return aurora_anim_ease_in_back(t);
        case 17: return aurora_anim_ease_out_back(t);
        case 18: return aurora_anim_ease_in_out_back(t);
    }
}

/* ════════════════════════════════════════════════════════════
   Tween
   ════════════════════════════════════════════════════════════ */

struct AuroraTween {
    float from, to;
    float duration, elapsed;
    int easing;
    int done;
    float current;
    void (*update_cb)(void*, float);
    void* update_user;
    void (*done_cb)(void*);
    void* done_user;
};

AuroraTween* aurora_anim_tween(float from, float to, float duration, int easing) {
    if (duration <= 0) return nullptr;
    AuroraTween* t = (AuroraTween*)calloc(1, sizeof(AuroraTween));
    if (!t) return nullptr;
    t->from = from; t->to = to;
    t->duration = duration; t->elapsed = 0;
    t->easing = easing; t->done = 0;
    t->current = from;
    t->update_cb = nullptr; t->update_user = nullptr;
    t->done_cb = nullptr; t->done_user = nullptr;
    return t;
}

void aurora_anim_tween_on_update(AuroraTween* t, void (*cb)(void*, float), void* userdata) {
    if (t) { t->update_cb = cb; t->update_user = userdata; }
}

void aurora_anim_tween_on_done(AuroraTween* t, void (*cb)(void*), void* userdata) {
    if (t) { t->done_cb = cb; t->done_user = userdata; }
}

void aurora_anim_tween_update(AuroraTween* t, float dt) {
    if (!t || t->done) return;
    t->elapsed += dt;
    if (t->elapsed >= t->duration) {
        t->elapsed = t->duration;
        t->done = 1;
    }
    float p = t->duration > 0 ? t->elapsed / t->duration : 1;
    float e = aurora_anim_ease_apply(t->easing, p);
    t->current = t->from + (t->to - t->from) * e;
    if (t->update_cb) t->update_cb(t->update_user, t->current);
    if (t->done && t->done_cb) { t->done_cb(t->done_user); t->done_cb = nullptr; }
}

int aurora_anim_tween_is_done(AuroraTween* t) { return t ? t->done : 1; }
float aurora_anim_tween_value(AuroraTween* t) { return t ? t->current : 0; }

void aurora_anim_tween_free(AuroraTween* t) { free(t); }

/* ════════════════════════════════════════════════════════════
   Sequence
   ════════════════════════════════════════════════════════════ */

struct SeqStep {
    float from, to, duration;
    int easing;
};

struct AuroraAnimSeq {
    std::vector<SeqStep> steps;
    int current_step;
    float elapsed;
    int done;
    float current;
    void (*update_cb)(void*, float);
    void* update_user;
    void (*done_cb)(void*);
    void* done_user;
};

AuroraAnimSeq* aurora_anim_seq_new() {
    AuroraAnimSeq* s = (AuroraAnimSeq*)calloc(1, sizeof(AuroraAnimSeq));
    if (s) { s->current_step = 0; s->elapsed = 0; s->done = 0; }
    return s;
}

void aurora_anim_seq_add(AuroraAnimSeq* seq, float from, float to, float duration, int easing) {
    if (!seq) return;
    SeqStep step; step.from = from; step.to = to;
    step.duration = duration; step.easing = easing;
    seq->steps.push_back(step);
}

void aurora_anim_seq_on_update(AuroraAnimSeq* seq, void (*cb)(void*, float), void* userdata) {
    if (seq) { seq->update_cb = cb; seq->update_user = userdata; }
}

void aurora_anim_seq_on_done(AuroraAnimSeq* seq, void (*cb)(void*), void* userdata) {
    if (seq) { seq->done_cb = cb; seq->done_user = userdata; }
}

void aurora_anim_seq_update(AuroraAnimSeq* seq, float dt) {
    if (!seq || seq->done || seq->steps.empty()) return;
    seq->elapsed += dt;
    float acc = 0;
    for (size_t i = 0; i < seq->steps.size(); ++i) {
        float dur = seq->steps[i].duration;
        if (seq->elapsed <= acc + dur) {
            float p = dur > 0 ? (seq->elapsed - acc) / dur : 1;
            float e = aurora_anim_ease_apply(seq->steps[i].easing, p > 1 ? 1 : p);
            seq->current = seq->steps[i].from + (seq->steps[i].to - seq->steps[i].from) * e;
            seq->current_step = (int)i;
            if (seq->update_cb) seq->update_cb(seq->update_user, seq->current);
            return;
        }
        acc += dur;
    }
    /* All steps finished */
    seq->done = 1;
    seq->current = seq->steps.back().to;
    if (seq->update_cb) seq->update_cb(seq->update_user, seq->current);
    if (seq->done_cb) { seq->done_cb(seq->done_user); seq->done_cb = nullptr; }
}

int aurora_anim_seq_is_done(AuroraAnimSeq* seq) { return seq ? seq->done : 1; }

void aurora_anim_seq_free(AuroraAnimSeq* seq) { delete seq; }

/* ════════════════════════════════════════════════════════════
   Controller
   ════════════════════════════════════════════════════════════ */

struct AuroraAnimCtrl {
    std::vector<AuroraTween*> tweens;
};

AuroraAnimCtrl* aurora_anim_ctrl_new() {
    AuroraAnimCtrl* c = (AuroraAnimCtrl*)calloc(1, sizeof(AuroraAnimCtrl));
    return c;
}

void aurora_anim_ctrl_add(AuroraAnimCtrl* ctrl, AuroraTween* tween) {
    if (ctrl && tween) ctrl->tweens.push_back(tween);
}

void aurora_anim_ctrl_update(AuroraAnimCtrl* ctrl, float dt) {
    if (!ctrl) return;
    for (auto* t : ctrl->tweens) aurora_anim_tween_update(t, dt);
}

int aurora_anim_ctrl_is_done(AuroraAnimCtrl* ctrl) {
    if (!ctrl) return 1;
    for (auto* t : ctrl->tweens) if (!t->done) return 0;
    return 1;
}

void aurora_anim_ctrl_free(AuroraAnimCtrl* ctrl) {
    if (!ctrl) return;
    for (auto* t : ctrl->tweens) aurora_anim_tween_free(t);
    ctrl->tweens.clear();
    free(ctrl);
}

/* ════════════════════════════════════════════════════════════
   Keyframe
   ════════════════════════════════════════════════════════════ */

struct AuroraAnimKf {
    int count;
    float* times;
    float* values;
};

AuroraAnimKf* aurora_anim_kf_new(int num_frames) {
    if (num_frames < 2) return nullptr;
    AuroraAnimKf* kf = (AuroraAnimKf*)calloc(1, sizeof(AuroraAnimKf));
    if (!kf) return nullptr;
    kf->count = num_frames;
    kf->times = (float*)calloc((size_t)num_frames, sizeof(float));
    kf->values = (float*)calloc((size_t)num_frames, sizeof(float));
    if (!kf->times || !kf->values) {
        free(kf->times); free(kf->values); free(kf);
        return nullptr;
    }
    return kf;
}

void aurora_anim_kf_set_key(AuroraAnimKf* kf, int index, float time, float value) {
    if (!kf || index < 0 || index >= kf->count) return;
    kf->times[index] = time;
    kf->values[index] = value;
}

float aurora_anim_kf_evaluate(AuroraAnimKf* kf, float t) {
    if (!kf || kf->count < 2) return 0;
    if (t <= kf->times[0]) return kf->values[0];
    if (t >= kf->times[kf->count - 1]) return kf->values[kf->count - 1];
    for (int i = 0; i < kf->count - 1; ++i) {
        if (t >= kf->times[i] && t <= kf->times[i + 1]) {
            float span = kf->times[i + 1] - kf->times[i];
            float p = span > 0 ? (t - kf->times[i]) / span : 0;
            return kf->values[i] + p * (kf->values[i + 1] - kf->values[i]);
        }
    }
    return kf->values[kf->count - 1];
}

void aurora_anim_kf_free(AuroraAnimKf* kf) {
    if (!kf) return;
    free(kf->times);
    free(kf->values);
    free(kf);
}
