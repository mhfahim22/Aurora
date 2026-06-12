#include "runtime/game.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/time.h>
#endif

extern "C" {

/* ── Static entity ID counter ── */
static int64_t next_entity_id = 1;

/* ── Scene state ── */
static int scene_active = 0;

/* ── Sprite registry for console renderer ── */
#define MAX_SPRITES 256
static AuroraSprite* sprite_registry[MAX_SPRITES];
static int sprite_registry_count = 0;

/* ── Frame timing ── */
#ifdef _WIN32
static LARGE_INTEGER frame_freq = { 0 };
static LARGE_INTEGER frame_start_time = { 0 };
#else
static struct timeval frame_start_time;
#endif
static double frame_delta_time = 0.016;

/* ── Gravity ── */
static double gravity_x = 0.0;
static double gravity_y = -9.81; /* default downward gravity */
static double gravity_z = 0.0;

/* ── Input state (256 key slots) ── */
#define MAX_KEYS 256
static int key_state[MAX_KEYS];
static int key_prev_state[MAX_KEYS];

void aurora_scene_init() {
    if (scene_active) return;
    scene_active = 1;
    next_entity_id = 1;
    sprite_registry_count = 0;
    memset(key_state, 0, sizeof(key_state));
    memset(key_prev_state, 0, sizeof(key_prev_state));
    printf("[scene] initialized\n");
}

void aurora_scene_shutdown() {
    if (!scene_active) return;
    scene_active = 0;
    printf("[scene] shutdown\n");
}

/* ── Entity ── */
AuroraEntity* aurora_entity_create(int64_t type) {
    AuroraEntity* e = (AuroraEntity*)calloc(1, sizeof(AuroraEntity));
    e->id = next_entity_id++;
    e->type = type;
    e->scale_x = e->scale_y = e->scale_z = 1.0;
    e->mass = 1.0;
    e->active = 1;
    return e;
}

void aurora_entity_destroy(AuroraEntity* e) {
    if (!e) return;
    free(e->user_data);
    free(e);
}

void aurora_entity_set_pos(AuroraEntity* e, double x, double y, double z) {
    if (!e) return;
    e->x = x; e->y = y; e->z = z;
}

void aurora_entity_get_pos(AuroraEntity* e, double* x, double* y, double* z) {
    if (!e) return;
    if (x) *x = e->x;
    if (y) *y = e->y;
    if (z) *z = e->z;
}

void aurora_entity_set_velocity(AuroraEntity* e, double vx, double vy, double vz) {
    if (!e) return;
    e->vx = vx; e->vy = vy; e->vz = vz;
}

void aurora_entity_get_velocity(AuroraEntity* e, double* vx, double* vy, double* vz) {
    if (!e) return;
    if (vx) *vx = e->vx;
    if (vy) *vy = e->vy;
    if (vz) *vz = e->vz;
}

/* ── Sprite ── */
AuroraSprite* aurora_sprite_create(AuroraEntity* entity, double w, double h) {
    AuroraSprite* s = (AuroraSprite*)calloc(1, sizeof(AuroraSprite));
    s->entity = entity;
    s->width = w;
    s->height = h;
    s->texture_path = nullptr;
    if (sprite_registry_count < MAX_SPRITES) {
        sprite_registry[sprite_registry_count++] = s;
    }
    return s;
}

/* ── Camera ── */
AuroraCamera* aurora_camera_create(double x, double y, double z) {
    AuroraCamera* c = (AuroraCamera*)calloc(1, sizeof(AuroraCamera));
    c->x = x; c->y = y; c->z = z;
    c->look_x = c->look_y = c->look_z = 0.0;
    c->fov = 60.0;
    return c;
}

/* ── Physics ── */
static int physics_initialized = 0;

void aurora_physics_set_gravity(double gx, double gy, double gz) {
    gravity_x = gx;
    gravity_y = gy;
    gravity_z = gz;
    printf("[physics] gravity set to (%.2f, %.2f, %.2f)\n", gx, gy, gz);
}

void aurora_physics_init() {
    if (physics_initialized) return;
    physics_initialized = 1;
    printf("[physics] initialized (gravity: %.2f, %.2f, %.2f)\n",
           gravity_x, gravity_y, gravity_z);
}

void aurora_physics_step(double dt) {
    if (!physics_initialized) return;
    if (dt <= 0.0) dt = 0.016;
    if (dt > 0.1) dt = 0.1; /* clamp */

    /* Apply gravity and integrate positions using semi-implicit Euler */
    for (int i = 0; i < sprite_registry_count; i++) {
        AuroraSprite* s = sprite_registry[i];
        if (!s || !s->entity || !s->entity->active) continue;

        AuroraEntity* e = s->entity;

        /* Apply gravity to velocity */
        e->vx += gravity_x * dt;
        e->vy += gravity_y * dt;
        e->vz += gravity_z * dt;

        /* Apply damping (air resistance) */
        double damping = 0.98;
        e->vx *= damping;
        e->vy *= damping;
        e->vz *= damping;

        /* Integrate position */
        e->x += e->vx * dt;
        e->y += e->vy * dt;
        e->z += e->vz * dt;

        /* Ground plane collision (y >= 0) */
        if (e->y < 0.0) {
            e->y = 0.0;
            e->vy = -e->vy * 0.5; /* bounce with energy loss */
        }
    }
    printf("[physics] step (dt=%.4f)\n", dt);
}

/* ── Collision ── */
int aurora_collision_check(AuroraEntity* a, AuroraEntity* b) {
    if (!a || !b || !a->active || !b->active) return 0;
    /* Simple AABB collision check using sprite width/height */
    double threshold = 1.0;
    /* Try to find sprite dimensions for more accurate collision */
    for (int i = 0; i < sprite_registry_count; i++) {
        AuroraSprite* s = sprite_registry[i];
        if (!s) continue;
        if (s->entity == a) {
            double hw = s->width * 0.5;
            double hh = s->height * 0.5;
            double dx = fabs(a->x - b->x);
            double dy = fabs(a->y - b->y);
            if (dx < hw + 0.5 && dy < hh + 0.5) return 1;
            return 0;
        }
    }
    /* Fallback: sphere collision */
    double dx = fabs(a->x - b->x);
    double dy = fabs(a->y - b->y);
    double dz = fabs(a->z - b->z);
    return (dx < threshold && dy < threshold && dz < threshold) ? 1 : 0;
}

/* ── Audio ── */
void aurora_audio_play(const char* clip_path) {
    if (!clip_path) return;
    printf("[audio] playing: %s\n", clip_path);
#ifdef _WIN32
    /* Play a simple beep as audio feedback */
    Beep(440, 200);
#endif
}

void aurora_audio_play_tone(int frequency, int duration_ms) {
    if (frequency < 37 || duration_ms < 1) return;
    printf("[audio] tone: %d Hz for %d ms\n", frequency, duration_ms);
#ifdef _WIN32
    Beep((DWORD)frequency, (DWORD)duration_ms);
#else
    /* On Linux/macOS, use terminal bell sequence */
    printf("\a");
    fflush(stdout);
#endif
}

void aurora_audio_stop_all() {
    printf("[audio] all stopped\n");
}

/* ── Animation ── */
AuroraAnimation* aurora_animation_create(double* keyframes, double* values,
                                          int num_frames, double duration) {
    if (!keyframes || !values || num_frames < 2) return nullptr;
    AuroraAnimation* a = (AuroraAnimation*)calloc(1, sizeof(AuroraAnimation));
    a->keyframes = (double*)malloc((size_t)num_frames * sizeof(double));
    a->values = (double*)malloc((size_t)num_frames * sizeof(double));
    memcpy(a->keyframes, keyframes, (size_t)num_frames * sizeof(double));
    memcpy(a->values, values, (size_t)num_frames * sizeof(double));
    a->num_frames = num_frames;
    a->duration = duration;
    a->elapsed = 0.0;
    a->looping = 0;
    a->target = nullptr;
    a->apply_fn = nullptr;
    return a;
}

void aurora_animation_play(const char* name, void* frame_cb,
                            int64_t duration_ms) {
    if (!name || !frame_cb) return;
    printf("[animation] playing: %s (duration: %lld ms)\n", name, (long long)duration_ms);
    /* Step through animation at ~60fps */
    double total_time = duration_ms / 1000.0;
    double step = 0.016;
    double elapsed = 0.0;
    while (elapsed < total_time) {
        double t = (total_time > 0.0) ? (elapsed / total_time) : 0.0;
        if (t > 1.0) t = 1.0;
        typedef void (*FrameFn)(void*, double);
        ((FrameFn)frame_cb)(NULL, t);
        elapsed += step;
    }
    /* Final frame */
    typedef void (*FrameFn)(void*, double);
    ((FrameFn)frame_cb)(NULL, 1.0);
}

void aurora_animation_update(AuroraAnimation* anim, double dt) {
    if (!anim || !anim->apply_fn || !anim->target) return;
    anim->elapsed += dt;
    if (anim->elapsed >= anim->duration) {
        if (anim->looping)
            anim->elapsed = fmod(anim->elapsed, anim->duration);
        else
            anim->elapsed = anim->duration;
    }
    /* Linear interpolation between keyframes */
    double t = (anim->duration > 0.0) ? (anim->elapsed / anim->duration) : 0.0;
    int idx = 0;
    for (int i = 0; i < anim->num_frames - 1; i++) {
        if (t >= anim->keyframes[i] && t <= anim->keyframes[i + 1]) {
            idx = i;
            break;
        }
    }
    double t0 = anim->keyframes[idx];
    double t1 = anim->keyframes[idx + 1];
    double frac = (t1 > t0) ? (t - t0) / (t1 - t0) : 0.0;
    double value = anim->values[idx] + frac * (anim->values[idx + 1] - anim->values[idx]);
    anim->apply_fn(anim->target, value);
}

/* ── Frame timing ── */
void aurora_engine_frame_start() {
#ifdef _WIN32
    if (frame_freq.QuadPart == 0) {
        QueryPerformanceFrequency(&frame_freq);
    }
    QueryPerformanceCounter(&frame_start_time);
#else
    gettimeofday(&frame_start_time, nullptr);
#endif
}

void aurora_engine_frame_end() {
#ifdef _WIN32
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    frame_delta_time = (double)(now.QuadPart - frame_start_time.QuadPart) / (double)frame_freq.QuadPart;
    if (frame_delta_time > 0.1) frame_delta_time = 0.1;
#else
    struct timeval now;
    gettimeofday(&now, nullptr);
    double sec = (double)(now.tv_sec - frame_start_time.tv_sec);
    double usec = (double)(now.tv_usec - frame_start_time.tv_usec);
    frame_delta_time = sec + usec / 1000000.0;
    if (frame_delta_time > 0.1) frame_delta_time = 0.1;
#endif
}

double aurora_engine_delta_time() {
    return frame_delta_time;
}

/* ── Input polling ── */
void aurora_engine_poll_input() {
    memcpy(key_prev_state, key_state, sizeof(key_state));
#ifdef _WIN32
    for (int i = 0; i < MAX_KEYS; i++) {
        key_state[i] = (GetAsyncKeyState(i) & 0x8000) ? 1 : 0;
    }
#else
    /* On non-Windows, read from stdin if available */
    static int stdin_initialized = 0;
    if (!stdin_initialized) {
#ifndef _WIN32
        struct termios oldt, newt;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
#endif
        stdin_initialized = 1;
    }
    /* Read available key presses */
    char c;
    while (read(STDIN_FILENO, &c, 1) > 0) {
        key_state[(int)c] = 1;
    }
#endif
}

int aurora_engine_is_key_down(int key_code) {
    if (key_code < 0 || key_code >= MAX_KEYS) return 0;
    return key_state[key_code];
}

/* ── Console renderer ── */
static char get_char_for_type(int64_t type) {
    switch (type) {
        case 1:  return '@';  /* player */
        case 2:  return '#';  /* wall */
        case 3:  return '*';  /* enemy */
        case 4:  return 'o';  /* projectile */
        case 5:  return '~';  /* water */
        case 6:  return '&';  /* item */
        default: return '+';  /* generic */
    }
}

void aurora_engine_render() {
    /* Clear screen */
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info;
    GetConsoleScreenBufferInfo(hConsole, &info);
    DWORD wrote;
    COORD origin = { 0, 0 };
    FillConsoleOutputCharacterA(hConsole, ' ', info.dwSize.X * info.dwSize.Y, origin, &wrote);
    SetConsoleCursorPosition(hConsole, origin);
#else
    printf("\033[2J\033[H");
#endif

    /* Draw each sprite as ASCII art at its entity position */
    for (int i = 0; i < sprite_registry_count; i++) {
        AuroraSprite* s = sprite_registry[i];
        if (!s || !s->entity || !s->entity->active) continue;

        int sx = (int)s->entity->x;
        int sy = (int)s->entity->y;
        int w = (int)s->width;
        int spr_h = (int)s->height;
        if (w < 1) w = 1;
        if (spr_h < 1) spr_h = 1;

        char ch = get_char_for_type(s->entity->type);

        /* Draw a filled rectangle of the type character */
        for (int row = 0; row < spr_h && sy + row >= 0; row++) {
#ifdef _WIN32
            COORD pos = { (SHORT)sx, (SHORT)(sy + row) };
            SetConsoleCursorPosition(hConsole, pos);
            for (int col = 0; col < w; col++) {
                putchar(ch);
            }
#else
            printf("\033[%d;%dH", sy + row + 1, sx + 1);
            for (int col = 0; col < w; col++) {
                putchar(ch);
            }
#endif
        }
    }
    fflush(stdout);
}

}
