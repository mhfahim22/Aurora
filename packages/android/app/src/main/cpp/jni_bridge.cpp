/* ════════════════════════════════════════════════════════════
   jni_bridge.cpp — JNI bridge between Java AuroraActivity and C++ runtime
   ════════════════════════════════════════════════════════════ */

#include <jni.h>
#include <cstdio>

#include "mobile/android.hpp"

/* ── Forward declarations of internal functions ── */
extern "C" {
void _aurora_android_push_touch(int action, int id, float x, float y, float pressure, float size);
void _aurora_android_set_key(int key_code, int pressed);
void _aurora_android_set_ime_text(const char* text);
void _aurora_android_on_sensor(int type, float x, float y, float z);
}

/* ── JNI_OnLoad ── */

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    (void)reserved;
    printf("[jni] JNI_OnLoad called\n");
    aurora_android_init(vm, nullptr);
    return JNI_VERSION_1_6;
}

/* ── Native method implementations ── */

extern "C" {

JNIEXPORT void JNICALL Java_aurora_AuroraActivity_nativeInit(JNIEnv* env, jclass cls) {
    (void)env; (void)cls;
    aurora_android_on_create();
    aurora_android_on_start();
    aurora_android_on_resume();
}

JNIEXPORT void JNICALL Java_aurora_AuroraActivity_nativeSetActivity(JNIEnv* env, jclass cls, jobject activity) {
    (void)env; (void)cls;
    aurora_android_set_activity(activity);
}

JNIEXPORT void JNICALL Java_aurora_AuroraActivity_nativeOnTouch(
    JNIEnv* env, jclass cls, jint action, jint id, jfloat x, jfloat y, jfloat pressure, jfloat size) {
    (void)env; (void)cls;
    _aurora_android_push_touch(action, id, x, y, pressure, size);
}

JNIEXPORT void JNICALL Java_aurora_AuroraActivity_nativeOnKey(
    JNIEnv* env, jclass cls, jint keyCode, jint pressed) {
    (void)env; (void)cls;
    _aurora_android_set_key(keyCode, pressed);
}

JNIEXPORT void JNICALL Java_aurora_AuroraActivity_nativeOnImeText(
    JNIEnv* env, jclass cls, jstring text) {
    (void)cls;
    const char* utf = env->GetStringUTFChars(text, nullptr);
    if (utf) {
        _aurora_android_set_ime_text(utf);
        env->ReleaseStringUTFChars(text, utf);
    }
}

JNIEXPORT void JNICALL Java_aurora_AuroraActivity_nativeOnSensor(
    JNIEnv* env, jclass cls, jint type, jfloat x, jfloat y, jfloat z) {
    (void)env; (void)cls;
    _aurora_android_on_sensor(type, x, y, z);
}

JNIEXPORT void JNICALL Java_aurora_AuroraActivity_nativeOnPermissionResult(
    JNIEnv* env, jclass cls, jstring permission, jboolean granted) {
    (void)cls;
    const char* utf = env->GetStringUTFChars(permission, nullptr);
    if (utf) {
        aurora_android_on_permission_result(utf, granted ? 1 : 0);
        env->ReleaseStringUTFChars(permission, utf);
    }
}

JNIEXPORT void JNICALL Java_aurora_AuroraActivity_nativeRenderWidgets(
    JNIEnv* env, jclass cls, jobject canvas, jlong rootWidgetPtr) {
    (void)cls;
    aurora_android_render_mw(env, canvas, (void*)rootWidgetPtr);
}

JNIEXPORT void JNICALL Java_aurora_AuroraActivity_nativeInitRenderer(
    JNIEnv* env, jclass cls) {
    aurora_android_renderer_init(env);
}

} /* extern "C" */
