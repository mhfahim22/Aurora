#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Scene ── */
void aurora_scene_init(void);
void aurora_scene_shutdown(void);

/* ── Entity ── */
typedef struct AuroraEntity {
    int64_t   id;
    int64_t   type;
    double    x, y, z;
    double    rot_x, rot_y, rot_z;
    double    scale_x, scale_y, scale_z;
    int       active;
    void*     user_data;
} AuroraEntity;

AuroraEntity* aurora_entity_create(int64_t type);
void          aurora_entity_destroy(AuroraEntity* e);
void          aurora_entity_set_pos(AuroraEntity* e, double x, double y, double z);
void          aurora_entity_get_pos(AuroraEntity* e, double* x, double* y, double* z);

/* ── Sprite ── */
typedef struct AuroraSprite {
    AuroraEntity* entity;
    char*         texture_path;
    double        width, height;
} AuroraSprite;

AuroraSprite* aurora_sprite_create(AuroraEntity* entity, double w, double h);

/* ── Camera ── */
typedef struct AuroraCamera {
    double x, y, z;
    double look_x, look_y, look_z;
    double fov;
} AuroraCamera;

AuroraCamera* aurora_camera_create(double x, double y, double z);

/* ── Physics ── */
void aurora_physics_init(void);
void aurora_physics_step(double dt);

/* ── Collision ── */
int  aurora_collision_check(AuroraEntity* a, AuroraEntity* b);

/* ── Audio ── */
void aurora_audio_play(const char* clip_path);
void aurora_audio_stop_all(void);

/* ── Frame timing / Render / Input ── */
void   aurora_engine_frame_start(void);
void   aurora_engine_frame_end(void);
void   aurora_engine_render(void);
int    aurora_engine_is_key_down(int key_code);
double aurora_engine_delta_time(void);

/* ── Animation ── */
typedef struct AuroraAnimation {
    double* keyframes;
    double* values;
    int     num_frames;
    double  duration;
    double  elapsed;
    int     looping;
    void*   target;
    void  (*apply_fn)(void* target, double value);
} AuroraAnimation;

AuroraAnimation* aurora_animation_create(double* keyframes, double* values,
                                          int num_frames, double duration);
void aurora_animation_play(const char* name, void* frame_cb,
                           int64_t duration_ms);
void aurora_animation_update(AuroraAnimation* anim, double dt);

#ifdef __cplusplus
}
#endif
