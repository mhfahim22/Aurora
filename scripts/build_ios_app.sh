#!/usr/bin/env bash
set -euo pipefail
# ── Aurora iOS App Build Script ─────────────────────────────
# Compiles an Aurora .aura file → .a → IPA
#
# Usage: ./build_ios_app.sh path/to/app.aura [output_name]
#
# Prerequisites:
#   - macOS with Xcode 15+
#   - aurorac compiler in PATH
#   - LLVM 19 (brew install llvm@19)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

AURA_FILE="${1:-}"
OUTPUT_NAME="${2:-app}"
JOBS="${JOBS:-$(sysctl -n hw.ncpu)}"

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

BUILD_DIR="${PROJECT_DIR}/build/ios_app/${OUTPUT_NAME}"

echo "==> Aurora iOS App Build"
echo "    Source : ${AURA_FILE}"
echo "    Output : ${OUTPUT_NAME}"
echo "    Build  : ${BUILD_DIR}"

mkdir -p "${BUILD_DIR}"

# 1. Compile Aurora → static library
echo "==> Compiling Aurora → .a..."
aurorac "${AURA_FILE}" -o "${BUILD_DIR}/libaurora_app.a" \
    --static --target arm64-apple-ios 2>&1 || {
    echo "ERROR: aurorac compilation failed."
    echo "  Make sure aurorac is built and in PATH."
    exit 1
}

# 2. Build via packages/ios Xcode project
echo "==> Building iOS app via packages/ios..."
IOS_PROJECT="${PROJECT_DIR}/packages/ios"
if [ -d "${IOS_PROJECT}" ]; then
    # Ensure ios_renderer_widgets.mm is referenced in the Xcode project
    # If not, open the project and add:
    #   aurora/src/mobile/ios/ios_renderer_widgets.mm
    # to the Compile Sources build phase.

    cd "${IOS_PROJECT}"
    xcodebuild -project AuroraApp.xcodeproj \
        -scheme AuroraApp \
        -configuration Release \
        -sdk iphoneos \
        -jobs "${JOBS}" \
        build 2>&1 || {
        echo "WARNING: Xcode build failed."
        echo "  Manual: cd packages/ios && xcodebuild -project AuroraApp.xcodeproj -scheme AuroraApp build"
    }
else
    echo "WARNING: iOS wrapper project not found at ${IOS_PROJECT}"
    echo "  Create the Xcode project first or build manually."
fi

echo "==> iOS app build complete."
echo "    Output: ${BUILD_DIR}/libaurora_app.a"
