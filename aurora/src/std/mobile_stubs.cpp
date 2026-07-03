/* ════════════════════════════════════════════════════════════
   mobile_stubs.cpp — Desktop stubs for mobile runtime functions.
   These allow the compiler to link on desktop builds.
   Real implementations are in aurora/src/mobile/android/ and
   aurora/src/mobile/ios/ for actual mobile builds.
   ════════════════════════════════════════════════════════════ */

#include "common/platform.hpp"

#if !AURORA_PLATFORM_ANDROID && !AURORA_PLATFORM_IOS

#include "mobile/android.hpp"
#include "mobile/ios.hpp"
#include <cstdlib>
#include <cstring>

/* ── Android stubs ── */

extern "C" {

int aurora_android_screen_width() { return 0; }
int aurora_android_screen_height() { return 0; }
int aurora_android_orientation() { return 0; }
void aurora_android_set_orientation(int) {}
int aurora_android_touch_count() { return 0; }
int aurora_android_touch_get(int, AuroraTouchEvent*) { return -1; }
void aurora_android_touch_clear() {}
int aurora_android_key_pressed(int) { return 0; }
const char* aurora_android_ime_text() { return ""; }
void aurora_android_sensors_enable(int) {}
void aurora_android_sensors_disable(int) {}
int aurora_android_sensor_data(int, float*, float*, float*) { return -1; }
int aurora_android_check_permission(const char*) { return -1; }
void aurora_android_toast(const char*, int) {}
const char* aurora_android_device_model() { return "Desktop"; }
const char* aurora_android_device_manufacturer() { return "Desktop"; }
const char* aurora_android_os_version() { return "0.0"; }
float aurora_android_density() { return 1.0f; }
int aurora_android_dp_to_px(int dp) { return dp; }
int aurora_android_px_to_dp(int px) { return px; }

/* ── iOS stubs ── */

float aurora_ios_screen_width() { return 0; }
float aurora_ios_screen_height() { return 0; }
float aurora_ios_screen_scale() { return 1.0f; }
int aurora_ios_orientation() { return 0; }
void aurora_ios_set_orientation(int) {}
int aurora_ios_touch_count() { return 0; }
int aurora_ios_touch_get(int, AuroraIOSTouch*) { return -1; }
const char* aurora_ios_path_for_resource(const char* n, const char* t) { (void)n; (void)t; return ""; }
const char* aurora_ios_documents_path() { return ""; }
const char* aurora_ios_cache_path() { return ""; }
const char* aurora_ios_device_model() { return "Desktop"; }
const char* aurora_ios_os_version() { return "0.0"; }
int aurora_ios_is_ipad() { return 0; }
void aurora_ios_haptic(int) {}

} /* extern "C" */

#endif /* !ANDROID && !IOS */
