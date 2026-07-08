#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Android JNI / Runtime ── */

/* Initialize the Aurora runtime for Android. Called from JNI_OnLoad. */
/* Returns 0 on success, -1 on error. */
int aurora_android_init(void* jvm, void* activity);

/* Set the Android activity reference (JNI jobject). */
void aurora_android_set_activity(void* activity);

/* Get the current Android activity reference (JNI jobject). */
void* aurora_android_get_activity(void);

/* Process an Android input event (AInputEvent*). Returns 1 if handled. */
int aurora_android_process_input(void* input_event);

/* Get screen dimensions in pixels. */
int aurora_android_screen_width(void);
int aurora_android_screen_height(void);

/* Get/set screen orientation. 0=portrait, 1=landscape, 2=auto. */
int aurora_android_orientation(void);
void aurora_android_set_orientation(int orient);

/* ── Touch / Input ── */

/* Touch event data */
typedef struct {
    int action;       /* 0=down, 1=up, 2=move, 3=cancel */
    int pointer_id;
    float x, y;
    float pressure;
    float size;
} AuroraTouchEvent;

/* Get number of touch events since last call */
int aurora_android_touch_count(void);

/* Get touch event by index (0-based). Returns 0 on success. */
int aurora_android_touch_get(int index, AuroraTouchEvent* out);

/* Clear touch event buffer */
void aurora_android_touch_clear(void);

/* Check if a key is currently pressed (Android key code). */
int aurora_android_key_pressed(int key_code);

/* Get the text from the last IME input (returns temporary string). */
const char* aurora_android_ime_text(void);

/* ── Sensors ── */

/* Sensor type constants */
#define AURORA_SENSOR_ACCELEROMETER 1
#define AURORA_SENSOR_GYROSCOPE     2
#define AURORA_SENSOR_MAGNETOMETER  4
#define AURORA_SENSOR_LIGHT         8
#define AURORA_SENSOR_PROXIMITY     16

/* Enable/disable sensors (bitmask of AURORA_SENSOR_*). */
void aurora_android_sensors_enable(int mask);
void aurora_android_sensors_disable(int mask);

/* Get latest sensor values. Returns 0 if data is available. */
/* x, y, z are sensor-specific (e.g., m/s^2 for accel, rad/s for gyro). */
int aurora_android_sensor_data(int type, float* x, float* y, float* z);

/* ── Permissions ── */

/* Request a runtime permission. callback(permission, granted) is called when done. */
typedef void (*AuroraPermissionCallback)(const char* permission, int granted);
void aurora_android_request_permission(const char* permission, AuroraPermissionCallback cb);

/* Check if a permission is granted. Returns 1 if granted, 0 if not, -1 if unknown. */
int aurora_android_check_permission(const char* permission);

/* Handle permission result from Activity.onRequestPermissionsResult. */
void aurora_android_on_permission_result(const char* permission, int granted);

/* ── Lifecycle ── */

/* Called from Activity lifecycle methods. */
void aurora_android_on_create(void);
void aurora_android_on_start(void);
void aurora_android_on_resume(void);
void aurora_android_on_pause(void);
void aurora_android_on_stop(void);
void aurora_android_on_destroy(void);

/* Called from Activity.onSaveInstanceState / onRestoreInstanceState. */
void aurora_android_on_save_state(const char* key, const char* value);
const char* aurora_android_on_restore_state(const char* key);

/* ── Surface / Rendering ── */

/* Initialize OpenGL ES rendering surface. */
/* Returns 0 on success. */
int aurora_android_surface_init(void* native_window);

/* Resize the rendering surface. */
void aurora_android_surface_resize(int width, int height);

/* Render a frame. Returns 0 on success. */
int aurora_android_surface_render(void);

/* Destroy the rendering surface. */
void aurora_android_surface_destroy(void);

/* ── Widget Renderer ── */

/* Initialize JNI method IDs for Canvas rendering (call once from JNI_OnLoad). */
void aurora_android_renderer_init(void* env);

/* Render the widget tree to an Android Canvas. */
/* root_widget: pointer to root MwWidget. */
void aurora_android_render_mw(void* env, void* canvas, void* root_widget);

/* ── Utility ── */

/* Show a toast message. */
void aurora_android_toast(const char* message, int short_duration);

/* Get device info. Returns temporary strings. */
const char* aurora_android_device_model(void);
const char* aurora_android_device_manufacturer(void);
const char* aurora_android_os_version(void);

/* Get density-independent pixel conversion. */
float aurora_android_density(void);
int aurora_android_dp_to_px(int dp);
int aurora_android_px_to_dp(int px);

#ifdef __cplusplus
}
#endif
