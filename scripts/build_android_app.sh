#!/usr/bin/env bash
set -euo pipefail
# ── Aurora Android App Build Script (Phase 37) ─────────────────
# Compiles an Aurora .aura file → .so → installable APK
#
# Usage: ./build_android_app.sh path/to/app.aura [output_name] [--release]
#
# Prerequisites:
#   - aurorac compiler in PATH
#   - Android NDK installed (ANDROID_NDK_HOME)
#   - Android SDK installed (ANDROID_HOME)
#   - LLVM 19 host tools
#   - Java 17+ (for Gradle)
#
# Phase 37.1 deliverables:
#   - Multi-ABI (arm64-v8a, armeabi-v7a, x86_64) support
#   - Automatic Gradle wrapper generation
#   - Release signing via keystore or debug signing fallback
#   - Produces installable APK at build/android_app/<name>/app/build/outputs/apk/

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

AURA_FILE="${1:-}"
OUTPUT_NAME="${2:-app}"
BUILD_MODE="${3:-debug}"
JOBS="${JOBS:-$(nproc)}"
ANDROID_HOME="${ANDROID_HOME:-$ANDROID_SDK_ROOT}"
ANDROID_NDK_HOME="${ANDROID_NDK_HOME:-${ANDROID_HOME}/ndk/$(ls -1 "${ANDROID_HOME}"/ndk 2>/dev/null | sort -V | tail -n1)}"

if [ -z "${AURA_FILE}" ]; then
    echo "Usage: $0 path/to/app.aura [output_name] [--release]"
    echo ""
    echo "Examples:"
    echo "  $0 examples/mobile/counter.aura counter"
    echo "  $0 examples/mobile/todo.aura todo"
    echo "  $0 examples/mobile/todo.aura todo --release"
    exit 1
fi

if [ ! -f "${AURA_FILE}" ]; then
    echo "ERROR: File not found: ${AURA_FILE}"
    exit 1
fi

if [ "${BUILD_MODE}" = "--release" ]; then
    BUILD_MODE="release"
fi

AURA_DIR="$(cd "$(dirname "${AURA_FILE}")" && pwd)"
AURA_BASE="$(basename "${AURA_FILE}" .aura)"
BUILD_DIR="${PROJECT_DIR}/build/android_app/${OUTPUT_NAME}"

echo "==> Aurora Android App Build (Phase 37)"
echo "    Source : ${AURA_FILE}"
echo "    Output : ${OUTPUT_NAME}"
echo "    Mode   : ${BUILD_MODE}"
echo "    Build  : ${BUILD_DIR}"
echo "    SDK    : ${ANDROID_HOME:-<not set>}"
echo "    NDK    : ${ANDROID_NDK_HOME:-<not set>}"

for abi in arm64-v8a armeabi-v7a x86_64; do
    mkdir -p "${BUILD_DIR}/app/src/main/jniLibs/${abi}"
done
mkdir -p "${BUILD_DIR}/app/src/main/java/aurora"
mkdir -p "${BUILD_DIR}/app/src/main/res/values"

# 1. Compile Aurora → shared libraries (one per ABI)
echo "==> Compiling Aurora → .so..."
compile_abi() {
    local abi="$1"
    local target="$2"
    echo "    Compiling ${abi}..."
    aurorac "${AURA_FILE}" -o "${BUILD_DIR}/app/src/main/jniLibs/${abi}/libaurora_app.so" \
        --shared --target "${target}" 2>&1 || {
        echo "    WARNING: ${abi} compilation failed — skipping (host LLVM may lack the target)"
        rm -f "${BUILD_DIR}/app/src/main/jniLibs/${abi}/libaurora_app.so"
        return 0
    }
}
compile_abi arm64-v8a   aarch64-linux-android
compile_abi armeabi-v7a  armv7-linux-androideabi
compile_abi x86_64      x86_64-linux-android

# 2. Copy the Android wrapper project
echo "==> Staging Android wrapper project..."
ANDROID_WRAPPER="${PROJECT_DIR}/packages/android"
if [ ! -d "${ANDROID_WRAPPER}" ]; then
    echo "ERROR: Android wrapper project not found at ${ANDROID_WRAPPER}"
    exit 1
fi
cp -r "${ANDROID_WRAPPER}/." "${BUILD_DIR}/"

# 3. Generate native activity wrapper for the Aurora app
cat > "${BUILD_DIR}/app/src/main/java/aurora/MainActivity.java" << 'JAVAEOF'
package aurora;

import android.app.NativeActivity;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.content.pm.PackageManager;
import android.util.Log;

public class MainActivity extends NativeActivity {
    private static final String TAG = "AuroraApp";

    static { System.loadLibrary("aurora_app"); }

    private static native void nativeInit();
    private static native void nativeInitRenderer();
    private static native void nativeOnTouch(int action, int id, float x, float y, float pressure, float size);
    private static native void nativeOnKey(int keyCode, int pressed);
    private static native void nativeOnImeText(String text);
    private static native void nativeOnPermissionResult(String permission, boolean granted);
    private static native void nativeOnSafeArea(float top, float bottom, float left, float right);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        nativeInit();
        nativeInitRenderer();
        getWindow().getDecorView().post(this::reportSafeArea);
    }

    /* 37.3 — read real device insets (status bar / notch, navigation bar
       / home indicator) and push them to the native widget tree in dp. */
    private void reportSafeArea() {
        try {
            View decor = getWindow().getDecorView();
            if (decor == null) return;
            android.graphics.Rect frame = new android.graphics.Rect();
            decor.getWindowVisibleDisplayFrame(frame);
            android.view.WindowInsets insets = decor.getRootWindowInsets();
            if (insets == null) return;
            float density = getResources().getDisplayMetrics().density;
            if (density <= 0.0f) density = 1.0f;
            float top = insets.getSystemWindowInsetTop() / density;
            float bottom = insets.getSystemWindowInsetBottom() / density;
            float left = insets.getSystemWindowInsetLeft() / density;
            float right = insets.getSystemWindowInsetRight() / density;
            nativeOnSafeArea(top, bottom, left, right);
        } catch (Exception e) {
            Log.w(TAG, "reportSafeArea failed: " + e.getMessage());
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) reportSafeArea();
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        int action = event.getActionMasked();
        int pointerIndex = event.getActionIndex();
        int id = event.getPointerId(pointerIndex);
        switch (action) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN:
                nativeOnTouch(0, id, event.getX(pointerIndex), event.getY(pointerIndex),
                        event.getPressure(pointerIndex), event.getSize(pointerIndex));
                return true;
            case MotionEvent.ACTION_MOVE:
                for (int i = 0; i < event.getPointerCount(); i++) {
                    nativeOnTouch(2, event.getPointerId(i), event.getX(i), event.getY(i),
                            event.getPressure(i), event.getSize(i));
                }
                return true;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
                nativeOnTouch(1, id, event.getX(pointerIndex), event.getY(pointerIndex),
                        event.getPressure(pointerIndex), event.getSize(pointerIndex));
                return true;
            case MotionEvent.ACTION_CANCEL:
                nativeOnTouch(3, id, 0, 0, 0, 0);
                return true;
        }
        return super.onTouchEvent(event);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        nativeOnKey(event.getKeyCode(), 1);
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        nativeOnKey(event.getKeyCode(), 0);
        return super.onKeyUp(keyCode, event);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        for (int i = 0; i < permissions.length && i < grantResults.length; i++) {
            nativeOnPermissionResult(permissions[i], grantResults[i] == PackageManager.PERMISSION_GRANTED);
        }
    }
}
JAVAEOF

# 4. Write gradle.properties with SDK locations
cat > "${BUILD_DIR}/gradle.properties" << PROPEOF
org.gradle.jvmargs=-Xmx2048m -Dfile.encoding=UTF-8
android.useAndroidX=true
android.enableJetifier=true
PROPEOF

# 5. Generate Gradle wrapper if missing
if [ ! -x "${BUILD_DIR}/gradlew" ]; then
    echo "==> Generating Gradle wrapper..."
    if command -v gradle >/dev/null 2>&1; then
        (cd "${BUILD_DIR}" && gradle wrapper --gradle-version 8.5) 2>/dev/null || \
            echo "    WARNING: 'gradle' not found. Install Gradle or copy gradlew manually."
    else
        echo "    WARNING: 'gradle' not found. Install Gradle or copy gradlew manually."
    fi
fi

# 6. Build the APK
echo "==> Building ${BUILD_MODE} APK..."
cd "${BUILD_DIR}"
if [ "${BUILD_MODE}" = "release" ]; then
    ./gradlew assembleRelease --parallel 2>&1 || {
        echo "WARNING: Release build failed (likely missing signing keystore)."
        echo "  Falling back to debug APK..."
        ./gradlew assembleDebug --parallel 2>&1 || {
            echo "ERROR: Gradle build failed."
            echo "  Manual: cd ${BUILD_DIR} && ./gradlew assembleDebug"
            exit 1
        }
    }
else
    ./gradlew assembleDebug --parallel 2>&1 || {
        echo "ERROR: Gradle build failed."
        echo "  Manual: cd ${BUILD_DIR} && ./gradlew assembleDebug"
        exit 1
    }
fi

echo ""
echo "==> Android app build complete."
APK_DIR="${BUILD_DIR}/app/build/outputs/apk/${BUILD_MODE}"
if [ -d "${APK_DIR}" ]; then
    echo "    APK: ${APK_DIR}/"
    ls -la "${APK_DIR}"
else
    echo "    Find APK under: ${BUILD_DIR}/app/build/outputs/apk/"
fi
echo ""
echo "  Install: adb install -r ${APK_DIR}/app-${BUILD_MODE}.apk"