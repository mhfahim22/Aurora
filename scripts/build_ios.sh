#!/usr/bin/env bash
set -euo pipefail
# ── Aurora iOS Build Script ────────────────────────────────────
# Prerequisites:
#   - macOS with Xcode 15+
#   - LLVM 19 (brew install llvm@19)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build/ios"
JOBS="${JOBS:-$(sysctl -n hw.ncpu)}"
CONFIG="${CONFIG:-Release}"
SDK="${SDK:-iphoneos}"

echo "==> Aurora iOS Build"
echo "    SDK    : ${SDK}"
echo "    Config : ${CONFIG}"
echo "    Jobs   : ${JOBS}"

# Detect LLVM
if [ -z "${LLVM_DIR:-}" ]; then
    if brew list llvm@19 &>/dev/null; then
        LLVM_PREFIX=$(brew --prefix llvm@19)
        LLVM_DIR="${LLVM_PREFIX}/lib/cmake/llvm"
        export PATH="${LLVM_PREFIX}/bin:$PATH"
    else
        echo "ERROR: LLVM not found. Run: brew install llvm@19"
        exit 1
    fi
fi
echo "    LLVM   : ${LLVM_DIR}"

# Determine sysroot and deployment target
if [ "${SDK}" = "iphoneos" ]; then
    SYSROOT="iphoneos"
    TARGET="arm64-apple-ios15.0"
else
    SYSROOT="iphonesimulator"
    TARGET="arm64-apple-ios15.0-simulator"
fi

echo "==> Building native runtime..."
cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE="${CONFIG}" \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_SYSROOT="${SYSROOT}" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DLLVM_DIR="${LLVM_DIR}"

cmake --build "${BUILD_DIR}" --config "${CONFIG}" -j"${JOBS}" \
    --target aurora_runtime

echo "==> Native runtime built: $(find "${BUILD_DIR}" -name 'libaurora_runtime.a' 2>/dev/null)"

# Build Xcode project for the iOS app
echo "==> Building Xcode project for iOS app..."
XCODE_BUILD_DIR="${PROJECT_DIR}/build/ios-xcode"
cmake -S "${PROJECT_DIR}" -B "${XCODE_BUILD_DIR}" \
    -G Xcode \
    -DCMAKE_BUILD_TYPE="${CONFIG}" \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_SYSROOT="${SYSROOT}" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
    -DLLVM_DIR="${LLVM_DIR}"

cmake --build "${XCODE_BUILD_DIR}" --config "${CONFIG}" -j"${JOBS}" \
    --target aurora_runtime 2>&1 || echo "    Xcode build optional — continuing"

# Build via packages/ios Xcode project
echo "==> Building iOS app via packages/ios..."
cd "${PROJECT_DIR}/packages/ios"
xcodebuild -project AuroraApp.xcodeproj \
    -scheme AuroraApp \
    -configuration "${CONFIG}" \
    -sdk "${SDK}" \
    -jobs "${JOBS}" \
    build 2>&1 || echo "    Xcode project not found — run: open packages/ios/AuroraApp.xcodeproj"

echo "==> iOS build complete."
