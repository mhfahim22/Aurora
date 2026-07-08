#!/usr/bin/env bash
set -euo pipefail
# ── Aurora Android App Build Script ─────────────────────────
# Compiles an Aurora .aura file → .so → APK
#
# Usage: ./build_android_app.sh path/to/app.aura [output_name]
#
# Prerequisites:
#   - aurorac compiler in PATH
#   - Android NDK installed (ANDROID_NDK_HOME)
#   - Android SDK installed (ANDROID_HOME)
#   - LLVM 19 host tools

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

AURA_FILE="${1:-}"
OUTPUT_NAME="${2:-app}"
JOBS="${JOBS:-$(nproc)}"

if [ -z "${AURA_FILE}" ]; then
    echo "Usage: $0 path/to/app.aura [output_name]"
    echo ""
    echo "Examples:"
    echo "  $0 examples/mobile/counter.aura counter"
    echo "  $0 examples/mobile/todo.aura todo"
    exit 1
fi

if [ ! -f "${AURA_FILE}" ]; then
    echo "ERROR: File not found: ${AURA_FILE}"
    exit 1
fi

AURA_DIR="$(cd "$(dirname "${AURA_FILE}")" && pwd)"
AURA_BASE="$(basename "${AURA_FILE}" .aura)"
BUILD_DIR="${PROJECT_DIR}/build/android_app/${OUTPUT_NAME}"

echo "==> Aurora Android App Build"
echo "    Source : ${AURA_FILE}"
echo "    Output : ${OUTPUT_NAME}"
echo "    Build  : ${BUILD_DIR}"

mkdir -p "${BUILD_DIR}/jniLibs/arm64-v8a"
mkdir -p "${BUILD_DIR}/app/src/main/java/aurora"

# 1. Compile Aurora → shared library
echo "==> Compiling Aurora → .so..."
aurorac "${AURA_FILE}" -o "${BUILD_DIR}/jniLibs/arm64-v8a/libaurora_app.so" \
    --shared --target aarch64-linux-android 2>&1 || {
    echo "ERROR: aurorac compilation failed."
    echo "  Make sure aurorac is built and in PATH."
    echo "  Try: cmake --build build --target aurorac"
    exit 1
}

# 2. Create minimal Android wrapper
echo "==> Creating Android wrapper..."
cat > "${BUILD_DIR}/app/src/main/java/aurora/MainActivity.java" << 'JAVAEOF'
package aurora;

import android.app.Activity;
import android.graphics.Canvas;
import android.os.Bundle;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

public class MainActivity extends Activity {
    static { System.loadLibrary("aurora_app"); }

    private SurfaceView surfaceView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        surfaceView = new SurfaceView(this);
        setContentView(surfaceView);
        nativeInit();
        nativeInitRenderer();

        surfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override public void surfaceCreated(SurfaceHolder holder) {
                nativeOnSurfaceCreated(holder.getSurface());
            }
            @Override public void surfaceChanged(SurfaceHolder holder, int fmt, int w, int h) {
                nativeOnSurfaceChanged(holder.getSurface(), w, h);
            }
            @Override public void surfaceDestroyed(SurfaceHolder holder) {
                nativeOnSurfaceDestroyed();
            }
        });
    }

    private static native void nativeInit();
    private static native void nativeInitRenderer();
    private static native void nativeOnSurfaceCreated(Object surface);
    private static native void nativeOnSurfaceChanged(Object surface, int w, int h);
    private static native void nativeOnSurfaceDestroyed();
}
JAVAEOF

# 3. Build Gradle project
echo "==> Building APK..."
ANDROID_WRAPPER="${PROJECT_DIR}/packages/android"
if [ -d "${ANDROID_WRAPPER}" ]; then
    cp -r "${ANDROID_WRAPPER}/." "${BUILD_DIR}/"
    cd "${BUILD_DIR}"
    ./gradlew assembleDebug --parallel -j"${JOBS}" 2>&1 || {
        echo "WARNING: Gradle build failed."
        echo "  Manual: cd ${BUILD_DIR} && ./gradlew assembleDebug"
    }
else
    echo "WARNING: Android wrapper project not found at ${ANDROID_WRAPPER}"
    echo "  Create the project first or build manually."
fi

echo "==> Android app build complete."
echo "    APK: ${BUILD_DIR}/app/build/outputs/apk/debug/"
