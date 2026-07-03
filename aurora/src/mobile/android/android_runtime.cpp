/* ════════════════════════════════════════════════════════════
   android_runtime.cpp — Android JNI bridge & Activity lifecycle
   Compile for Android with NDK (arm64-v8a / armeabi-v7a / x86_64).
   ════════════════════════════════════════════════════════════ */

#include "mobile/android.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>

/* ── JNI helpers ── */
/* These use the JNI C API directly from Android NDK. */
#include <jni.h>

/* ── Global state ── */
static JavaVM*     g_jvm = nullptr;
static jobject     g_activity = nullptr;
static JNIEnv*     g_env = nullptr;
static int         g_screen_w = 0;
static int         g_screen_h = 0;
static int         g_orientation = 2; /* auto */
static std::string g_ime_text;

/* Touch event ring buffer */
static std::vector<AuroraTouchEvent> g_touch_events;
static std::map<int, bool> g_key_state;

/* Sensor data */
static float g_accel[3] = {0, 0, 0};
static float g_gyro[3]  = {0, 0, 0};
static float g_mag[3]   = {0, 0, 0};
static float g_light     = 0;
static float g_proximity = 0;
static int   g_sensor_mask = 0;

/* Permission callbacks */
struct PermissionRequest {
    std::string permission;
    AuroraPermissionCallback cb;
};
static std::vector<PermissionRequest> g_pending_permissions;

/* Lifecycle state */
static int g_lifecycle_state = 0; /* 0=created, 1=started, 2=resumed, 3=paused, 4=stopped */

/* Saved instance state */
static std::map<std::string, std::string> g_saved_state;

/* ── JNI helper functions ── */

static JNIEnv* get_env() {
    if (g_jvm) {
        g_jvm->GetEnv((void**)&g_env, JNI_VERSION_1_6);
    }
    return g_env;
}

static jclass find_class(const char* name) {
    JNIEnv* env = get_env();
    if (!env) return nullptr;
    return env->FindClass(name);
}

/* ════════════════════════════════════════════════════════════
   Public API
   ════════════════════════════════════════════════════════════ */

extern "C" {

int aurora_android_init(void* jvm, void* activity) {
    if (!jvm) return -1;
    g_jvm = (JavaVM*)jvm;
    g_activity = activity ? g_env->NewGlobalRef((jobject)activity) : nullptr;

    JNIEnv* env = get_env();
    if (!env) return -1;

    /* Get screen dimensions from activity resources */
    jclass activity_class = env->GetObjectClass((jobject)g_activity);
    if (activity_class) {
        jmethodID get_res = env->GetMethodID(activity_class, "getResources", "()Landroid/content/res/Resources;");
        if (get_res) {
            jobject res = env->CallObjectMethod((jobject)g_activity, get_res);
            if (res) {
                jclass res_class = env->GetObjectClass(res);
                jmethodID get_display = env->GetMethodID(res_class, "getDisplayMetrics", "()Landroid/util/DisplayMetrics;");
                if (get_display) {
                    jobject dm = env->CallObjectMethod(res, get_display);
                    if (dm) {
                        jclass dm_class = env->GetObjectClass(dm);
                        jfieldID w = env->GetFieldID(dm_class, "widthPixels", "I");
                        jfieldID h = env->GetFieldID(dm_class, "heightPixels", "I");
                        if (w && h) {
                            g_screen_w = env->GetIntField(dm, w);
                            g_screen_h = env->GetIntField(dm, h);
                        }
                    }
                }
            }
        }
    }

    printf("[android] runtime initialized: %dx%d\n", g_screen_w, g_screen_h);
    return 0;
}

void aurora_android_set_activity(void* activity) {
    JNIEnv* env = get_env();
    if (g_activity && env) {
        env->DeleteGlobalRef(g_activity);
    }
    g_activity = activity ? env->NewGlobalRef((jobject)activity) : nullptr;
}

void* aurora_android_get_activity() {
    return (void*)g_activity;
}

int aurora_android_process_input(void* input_event) {
    /* AInputEvent* processing — this is called from the native activity's
       input queue callback. We extract motion events and key events. */
    (void)input_event;
    /* In a full implementation, use AInputEvent_getType() etc. from android/input.h */
    return 0;
}

int aurora_android_screen_width() { return g_screen_w; }
int aurora_android_screen_height() { return g_screen_h; }

int aurora_android_orientation() { return g_orientation; }

void aurora_android_set_orientation(int orient) {
    g_orientation = orient;
    JNIEnv* env = get_env();
    if (!env || !g_activity) return;

    jclass activity_class = env->GetObjectClass((jobject)g_activity);
    if (!activity_class) return;

    jmethodID set_req = env->GetMethodID(activity_class, "setRequestedOrientation", "(I)V");
    if (!set_req) return;

    int android_orient = 0; /* SCREEN_ORIENTATION_UNSPECIFIED */
    if (orient == 0) android_orient = 1;  /* PORTRAIT */
    else if (orient == 1) android_orient = 0; /* LANDSCAPE */
    else android_orient = 4;  /* SENSOR */

    env->CallVoidMethod((jobject)g_activity, set_req, android_orient);
}

/* ════════════════════════════════════════════════════════════
   Touch / Input
   ════════════════════════════════════════════════════════════ */

int aurora_android_touch_count() {
    return (int)g_touch_events.size();
}

int aurora_android_touch_get(int index, AuroraTouchEvent* out) {
    if (index < 0 || index >= (int)g_touch_events.size() || !out)
        return -1;
    *out = g_touch_events[index];
    return 0;
}

void aurora_android_touch_clear() {
    g_touch_events.clear();
}

int aurora_android_key_pressed(int key_code) {
    auto it = g_key_state.find(key_code);
    return (it != g_key_state.end() && it->second) ? 1 : 0;
}

const char* aurora_android_ime_text() {
    return g_ime_text.c_str();
}

/* Internal: called from JNI callback when touch events arrive */
void _aurora_android_push_touch(int action, int id, float x, float y, float pressure, float size) {
    AuroraTouchEvent ev;
    ev.action = action;
    ev.pointer_id = id;
    ev.x = x;
    ev.y = y;
    ev.pressure = pressure;
    ev.size = size;
    g_touch_events.push_back(ev);
}

/* Internal: called from JNI callback for key events */
void _aurora_android_set_key(int key_code, int pressed) {
    g_key_state[key_code] = (pressed != 0);
}

/* Internal: called from JNI callback for IME text */
void _aurora_android_set_ime_text(const char* text) {
    g_ime_text = text ? text : "";
}

/* ════════════════════════════════════════════════════════════
   Sensors
   ════════════════════════════════════════════════════════════ */

void aurora_android_sensors_enable(int mask) {
    g_sensor_mask |= mask;
    /* In full implementation: register sensor listeners via JNI */
}

void aurora_android_sensors_disable(int mask) {
    g_sensor_mask &= ~mask;
    /* In full implementation: unregister sensor listeners via JNI */
}

int aurora_android_sensor_data(int type, float* x, float* y, float* z) {
    if (!x || !y || !z) return -1;
    switch (type) {
        case AURORA_SENSOR_ACCELEROMETER:
            *x = g_accel[0]; *y = g_accel[1]; *z = g_accel[2];
            return 0;
        case AURORA_SENSOR_GYROSCOPE:
            *x = g_gyro[0]; *y = g_gyro[1]; *z = g_gyro[2];
            return 0;
        case AURORA_SENSOR_MAGNETOMETER:
            *x = g_mag[0]; *y = g_mag[1]; *z = g_mag[2];
            return 0;
        case AURORA_SENSOR_LIGHT:
            *x = g_light; *y = 0; *z = 0;
            return 0;
        case AURORA_SENSOR_PROXIMITY:
            *x = g_proximity; *y = 0; *z = 0;
            return 0;
        default:
            return -1;
    }
}

/* Internal: called from JNI sensor callback */
void _aurora_android_on_sensor(int type, float x, float y, float z) {
    switch (type) {
        case AURORA_SENSOR_ACCELEROMETER:
            g_accel[0] = x; g_accel[1] = y; g_accel[2] = z;
            break;
        case AURORA_SENSOR_GYROSCOPE:
            g_gyro[0] = x; g_gyro[1] = y; g_gyro[2] = z;
            break;
        case AURORA_SENSOR_MAGNETOMETER:
            g_mag[0] = x; g_mag[1] = y; g_mag[2] = z;
            break;
        case AURORA_SENSOR_LIGHT:
            g_light = x;
            break;
        case AURORA_SENSOR_PROXIMITY:
            g_proximity = x;
            break;
    }
}

/* ════════════════════════════════════════════════════════════
   Permissions
   ════════════════════════════════════════════════════════════ */

void aurora_android_request_permission(const char* permission, AuroraPermissionCallback cb) {
    if (!permission) return;
    PermissionRequest req;
    req.permission = permission;
    req.cb = cb;
    g_pending_permissions.push_back(req);

    JNIEnv* env = get_env();
    if (!env || !g_activity) return;

    jclass activity_class = env->GetObjectClass((jobject)g_activity);
    if (!activity_class) return;

    jmethodID req_perm = env->GetMethodID(activity_class, "requestPermissions",
        "([Ljava/lang/String;I)V");
    if (!req_perm) return;

    jstring perm_str = env->NewStringUTF(permission);
    jobjectArray arr = env->NewObjectArray(1, env->FindClass("java/lang/String"), perm_str);
    env->CallVoidMethod((jobject)g_activity, req_perm, arr, 1001);
}

int aurora_android_check_permission(const char* permission) {
    JNIEnv* env = get_env();
    if (!env || !g_activity) return -1;

    jclass activity_class = env->GetObjectClass((jobject)g_activity);
    if (!activity_class) return -1;

    jmethodID check = env->GetMethodID(activity_class, "checkSelfPermission",
        "(Ljava/lang/String;)I");
    if (!check) return -1;

    jstring perm_str = env->NewStringUTF(permission);
    int result = env->CallIntMethod((jobject)g_activity, check, perm_str);
    return (result == 0) ? 1 : 0;
}

void aurora_android_on_permission_result(const char* permission, int granted) {
    for (auto it = g_pending_permissions.begin(); it != g_pending_permissions.end();) {
        if (it->permission == permission) {
            if (it->cb) it->cb(permission, granted);
            it = g_pending_permissions.erase(it);
        } else {
            ++it;
        }
    }
}

/* ════════════════════════════════════════════════════════════
   Lifecycle
   ════════════════════════════════════════════════════════════ */

void aurora_android_on_create() {
    g_lifecycle_state = 0;
    printf("[android] lifecycle: onCreate\n");
}

void aurora_android_on_start() {
    g_lifecycle_state = 1;
    printf("[android] lifecycle: onStart\n");
}

void aurora_android_on_resume() {
    g_lifecycle_state = 2;
    printf("[android] lifecycle: onResume\n");
}

void aurora_android_on_pause() {
    g_lifecycle_state = 3;
    printf("[android] lifecycle: onPause\n");
}

void aurora_android_on_stop() {
    g_lifecycle_state = 4;
    printf("[android] lifecycle: onStop\n");
}

void aurora_android_on_destroy() {
    g_lifecycle_state = 0;
    g_touch_events.clear();
    g_key_state.clear();
    g_pending_permissions.clear();
    printf("[android] lifecycle: onDestroy\n");
}

void aurora_android_on_save_state(const char* key, const char* value) {
    if (key && value) g_saved_state[key] = value;
}

const char* aurora_android_on_restore_state(const char* key) {
    auto it = g_saved_state.find(key ? key : "");
    return (it != g_saved_state.end()) ? it->second.c_str() : "";
}

/* ════════════════════════════════════════════════════════════
   Surface / Rendering
   ════════════════════════════════════════════════════════════ */

/* Simple OpenGL ES 2.0 renderer state */
static struct {
    void* native_window;
    int width;
    int height;
    int egl_display;
    int egl_surface;
    int egl_context;
} g_render = {0};

int aurora_android_surface_init(void* native_window) {
    if (!native_window) return -1;
    g_render.native_window = native_window;
    printf("[android] surface init: %p\n", native_window);
    /* Full implementation would create EGL display, surface, context here.
       Requires linking against EGL and GLESv2 libraries. */
    return 0;
}

void aurora_android_surface_resize(int width, int height) {
    g_render.width = width;
    g_render.height = height;
    /* Update EGL surface dimensions / viewport */
}

int aurora_android_surface_render() {
    /* Clear and swap buffers */
    /* glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); */
    /* eglSwapBuffers(egl_display, egl_surface); */
    return 0;
}

void aurora_android_surface_destroy() {
    /* eglDestroySurface, eglDestroyContext, eglTerminate */
    g_render.native_window = nullptr;
}

/* ════════════════════════════════════════════════════════════
   Utility
   ════════════════════════════════════════════════════════════ */

void aurora_android_toast(const char* message, int short_duration) {
    JNIEnv* env = get_env();
    if (!env || !g_activity) return;

    jclass activity_class = env->GetObjectClass((jobject)g_activity);
    if (!activity_class) return;

    jmethodID make_text = env->GetStaticMethodID(
        env->FindClass("android/widget/Toast"),
        "makeText",
        "(Landroid/content/Context;Ljava/lang/CharSequence;I)Landroid/widget/Toast;");
    if (!make_text) return;

    jmethodID show = env->GetMethodID(
        env->FindClass("android/widget/Toast"),
        "show", "()V");

    jstring msg = env->NewStringUTF(message ? message : "");
    jobject toast = env->CallStaticObjectMethod(
        env->FindClass("android/widget/Toast"),
        make_text, g_activity, msg, short_duration ? 0 : 1);
    if (toast && show) {
        env->CallVoidMethod(toast, show);
    }
}

const char* aurora_android_device_model() {
    static std::string model;
    JNIEnv* env = get_env();
    if (env) {
        jclass build_class = env->FindClass("android/os/Build");
        if (build_class) {
            jfieldID model_fid = env->GetStaticFieldID(build_class, "MODEL", "Ljava/lang/String;");
            if (model_fid) {
                jstring model_str = (jstring)env->GetStaticObjectField(build_class, model_fid);
                if (model_str) {
                    const char* utf = env->GetStringUTFChars(model_str, nullptr);
                    if (utf) {
                        model = utf;
                        env->ReleaseStringUTFChars(model_str, utf);
                    }
                }
            }
        }
    }
    return model.c_str();
}

const char* aurora_android_device_manufacturer() {
    static std::string manufacturer;
    JNIEnv* env = get_env();
    if (env) {
        jclass build_class = env->FindClass("android/os/Build");
        if (build_class) {
            jfieldID mfid = env->GetStaticFieldID(build_class, "MANUFACTURER", "Ljava/lang/String;");
            if (mfid) {
                jstring ms = (jstring)env->GetStaticObjectField(build_class, mfid);
                if (ms) {
                    const char* utf = env->GetStringUTFChars(ms, nullptr);
                    if (utf) { manufacturer = utf; env->ReleaseStringUTFChars(ms, utf); }
                }
            }
        }
    }
    return manufacturer.c_str();
}

const char* aurora_android_os_version() {
    static std::string version;
    JNIEnv* env = get_env();
    if (env) {
        jclass build_class = env->FindClass("android/os/Build$VERSION");
        if (build_class) {
            jfieldID rel = env->GetStaticFieldID(build_class, "RELEASE", "Ljava/lang/String;");
            if (rel) {
                jstring rs = (jstring)env->GetStaticObjectField(build_class, rel);
                if (rs) {
                    const char* utf = env->GetStringUTFChars(rs, nullptr);
                    if (utf) { version = utf; env->ReleaseStringUTFChars(rs, utf); }
                }
            }
        }
    }
    return version.c_str();
}

float aurora_android_density() {
    static float density = 0;
    if (density == 0) {
        JNIEnv* env = get_env();
        if (env && g_activity) {
            jclass activity_class = env->GetObjectClass((jobject)g_activity);
            jmethodID get_res = env->GetMethodID(activity_class, "getResources", "()Landroid/content/res/Resources;");
            if (get_res) {
                jobject res = env->CallObjectMethod((jobject)g_activity, get_res);
                if (res) {
                    jclass res_class = env->GetObjectClass(res);
                    jmethodID get_dm = env->GetMethodID(res_class, "getDisplayMetrics", "()Landroid/util/DisplayMetrics;");
                    if (get_dm) {
                        jobject dm = env->CallObjectMethod(res, get_dm);
                        if (dm) {
                            jclass dm_class = env->GetObjectClass(dm);
                            jfieldID d = env->GetFieldID(dm_class, "density", "F");
                            if (d) density = env->GetFloatField(dm, d);
                        }
                    }
                }
            }
        }
    }
    return density;
}

int aurora_android_dp_to_px(int dp) {
    return (int)(dp * aurora_android_density() + 0.5f);
}

int aurora_android_px_to_dp(int px) {
    float d = aurora_android_density();
    return d > 0 ? (int)(px / d + 0.5f) : px;
}

} /* extern "C" */
