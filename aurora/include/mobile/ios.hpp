#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* ── iOS UIKit / Runtime ── */

/* Initialize the Aurora runtime for iOS. Called from UIApplicationMain. */
/* Returns 0 on success, -1 on error. */
int aurora_ios_init(void);

/* Get the current UIViewController reference (as void* for cross-language use). */
void* aurora_ios_get_view_controller(void);

/* Set the current UIViewController (called from app delegate). */
void aurora_ios_set_view_controller(void* vc);

/* Get screen dimensions in points (not pixels). */
float aurora_ios_screen_width(void);
float aurora_ios_screen_height(void);

/* Get screen scale (1.0 for standard, 2.0/3.0 for Retina). */
float aurora_ios_screen_scale(void);

/* Get/set interface orientation. 0=portrait, 1=landscape, 2=auto. */
int aurora_ios_orientation(void);
void aurora_ios_set_orientation(int orient);

/* ── Touch / Input ── */

/* Touch event data */
typedef struct {
    int phase;        /* 0=began, 1=moved, 2=stationary, 3=ended, 4=cancelled */
    int tap_count;
    float x, y;
    float prev_x, prev_y;
    double timestamp;
} AuroraIOSTouch;

/* Get number of active touches */
int aurora_ios_touch_count(void);

/* Get touch by index (0-based). Returns 0 on success. */
int aurora_ios_touch_get(int index, AuroraIOSTouch* out);

/* ── Rendering ── */

/* Initialize Metal renderer. Returns 0 on success. */
int aurora_ios_metal_init(void* metal_layer);

/* Initialize OpenGL ES renderer (fallback). Returns 0 on success. */
int aurora_ios_gles_init(void* gl_layer);

/* Resize the render surface. */
void aurora_ios_render_resize(float width, float height, float scale);

/* Render a frame. Returns 0 on success. */
int aurora_ios_render_frame(void);

/* ── Lifecycle ── */

/* Called from AppDelegate lifecycle methods. */
void aurora_ios_on_did_finish_launching(void);
void aurora_ios_on_will_resign_active(void);
void aurora_ios_on_did_enter_background(void);
void aurora_ios_on_will_enter_foreground(void);
void aurora_ios_on_did_become_active(void);
void aurora_ios_on_will_terminate(void);

/* Handle memory warning. */
void aurora_ios_on_memory_warning(void);

/* ── Bundle / Resources ── */

/* Get path to app bundle resource. Returns temporary string. */
const char* aurora_ios_path_for_resource(const char* name, const char* type);

/* Get path to Documents directory. Returns temporary string. */
const char* aurora_ios_documents_path(void);

/* Get path to Cache directory. Returns temporary string. */
const char* aurora_ios_cache_path(void);

/* ── Device Info ── */

/* Get device model name (e.g., "iPhone14,3"). */
const char* aurora_ios_device_model(void);

/* Get OS version (e.g., "17.0"). */
const char* aurora_ios_os_version(void);

/* Check if running on iPad. */
int aurora_ios_is_ipad(void);

/* ── Haptics ── */

/* Trigger a haptic feedback. type: 0=light, 1=medium, 2=heavy, 3=selection. */
void aurora_ios_haptic(int type);

#ifdef __cplusplus
}
#endif
