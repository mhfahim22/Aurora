#!/usr/bin/env bash
set -euo pipefail
# ── Aurora Android Build Script ────────────────────────────────
# Prerequisites:
#   - Android NDK installed (set ANDROID_NDK_HOME)
#   - LLVM 19 host tools
#   - Gradle (bundled or system)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build/android"
JOBS="${JOBS:-$(nproc)}"
CONFIG="${CONFIG:-Release}"

if [ -z "${ANDROID_NDK_HOME:-}" ]; then
    echo "ERROR: ANDROID_NDK_HOME not set."
    echo "  export ANDROID_NDK_HOME=/path/to/android-ndk"
    exit 1
fi
echo "==> Aurora Android Build"
echo "    NDK    : ${ANDROID_NDK_HOME}"
echo "    Config : ${CONFIG}"
echo "    Jobs   : ${JOBS}"

# Detect host LLVM
if [ -z "${LLVM_DIR:-}" ]; then
    for ver in 19 18 17; do
        dir="/usr/lib/llvm-${ver}/lib/cmake/llvm"
        if [ -f "${dir}/LLVMConfig.cmake" ]; then
            LLVM_DIR="${dir}"
            break
        fi
    done
    if [ -z "${LLVM_DIR:-}" ]; then
        echo "ERROR: LLVM not found. Install llvm-19-dev."
        exit 1
    fi
fi
echo "    LLVM   : ${LLVM_DIR}"

# Build native runtime via CMake + NDK
echo "==> Building native runtime..."
cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE="${CONFIG}" \
    -DCMAKE_TOOLCHAIN_FILE="${ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=24 \
    -DANDROID_STL=c++_shared \
    -DLLVM_DIR="${LLVM_DIR}"

cmake --build "${BUILD_DIR}" --config "${CONFIG}" -j"${JOBS}" \
    --target aurora_runtime

echo "==> Native runtime built: $(find "${BUILD_DIR}" -name 'libaurora_runtime.a' 2>/dev/null)"

# Build Gradle APK
echo "==> Building APK via Gradle..."
cd "${PROJECT_DIR}/packages/android"
./gradlew assembleDebug --parallel -j"${JOBS}" 2>&1 || {
    echo "WARNING: Gradle build failed (may need Android SDK)."
    echo "  Manual: cd packages/android && ./gradlew assembleDebug"
}

echo "==> Android build complete."
echo "    APK: packages/android/app/build/outputs/apk/debug/"
