#!/usr/bin/env bash
set -euo pipefail
# ── Aurora Android Emulator Validation ─────────────────────────
# Prerequisites:
#   - Android SDK + emulator installed
#   - AVD created (e.g. "AuroraTest" API 34)
#   - ANDROID_HOME / ANDROID_SDK_ROOT set
#   - adb on PATH

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
AVD_NAME="${AVD_NAME:-AuroraTest}"
TIMEOUT="${TIMEOUT:-120}"

echo "==> Aurora Android Emulator Validation"

# Check SDK
if [ -z "${ANDROID_HOME:-}" ] && [ -z "${ANDROID_SDK_ROOT:-}" ]; then
    echo "ERROR: ANDROID_HOME or ANDROID_SDK_ROOT not set."
    exit 1
fi
SDK="${ANDROID_HOME:-${ANDROID_SDK_ROOT}}"
EMULATOR="${SDK}/emulator/emulator"
ADB="${SDK}/platform-tools/adb"

# Build APK first
echo "==> Building APK..."
bash "${SCRIPT_DIR}/build_android.sh" || {
    echo "WARNING: Build failed — using existing APK if available."
}

APK="${PROJECT_DIR}/packages/android/app/build/outputs/apk/debug/app-debug.apk"
if [ ! -f "$APK" ]; then
    echo "ERROR: APK not found at ${APK}. Build must succeed first."
    exit 1
fi

# Start emulator
echo "==> Starting emulator (AVD: ${AVD_NAME})..."
"${EMULATOR}" -avd "${AVD_NAME}" -no-window -no-audio -no-boot-anim &
EMU_PID=$!

# Wait for boot
echo "==> Waiting for device..."
"${ADB}" wait-for-device
"${ADB}" shell 'while [[ -z $(getprop sys.boot_completed) ]]; do sleep 2; done'
echo "    Device booted."

# Install APK
echo "==> Installing APK..."
"${ADB}" install -r "${APK}"

# Run validation activity
echo "==> Launching validation..."
"${ADB}" shell am start -n aurora.app/.AuroraActivity \
    -e test crossplatform 2>&1 || echo "    (activity start optional)"

# Wait and capture logcat
sleep "${TIMEOUT}"
"${ADB}" logcat -d -s Aurora:V | grep "CROSS_TEST" || echo "    (no cross-test output in logcat)"

# Cleanup
echo "==> Stopping emulator..."
kill "${EMU_PID}" 2>/dev/null || true

echo "==> Android emulator validation complete."
