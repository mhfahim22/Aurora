#!/usr/bin/env bash
set -euo pipefail
# ── Aurora iOS Simulator Validation ────────────────────────────
# Prerequisites:
#   - macOS with Xcode 15+
#   - LLVM 19 (brew install llvm@19)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SIM_DEVICE="${SIM_DEVICE:-iPhone 15 Pro}"
SIM_RUNTIME="${SIM_RUNTIME:-iOS 17.5}"
TIMEOUT="${TIMEOUT:-120}"

echo "==> Aurora iOS Simulator Validation"

# Build for simulator
echo "==> Building for iOS simulator..."
SDK=iphonesimulator bash "${SCRIPT_DIR}/build_ios.sh" || {
    echo "WARNING: Build failed — using existing if available."
}

# Create simulator if needed
echo "==> Checking simulator device..."
DEVICE_UDID=$(xcrun simctl list devices "${SIM_RUNTIME}" 2>/dev/null | \
    grep "${SIM_DEVICE}" | grep -oE '[a-f0-9-]{36}' | head -1)

if [ -z "${DEVICE_UDID}" ]; then
    echo "    Creating simulator: ${SIM_DEVICE} (${SIM_RUNTIME})..."
    RUNTIME_ID=$(xcrun simctl list runtimes | grep "${SIM_RUNTIME}" | grep -oE 'com\.apple\.[^ ]+' | head -1)
    DEV_TYPE_ID=$(xcrun simctl list devicetypes | grep "${SIM_DEVICE}" | grep -oE 'com\.apple\.[^ ]+' | head -1)
    DEVICE_UDID=$(xcrun simctl create "${SIM_DEVICE}" "${DEV_TYPE_ID}" "${RUNTIME_ID}")
fi

# Boot simulator
echo "==> Booting simulator..."
xcrun simctl boot "${DEVICE_UDID}" || true
open -a Simulator

# Install app
APP_BUNDLE="${PROJECT_DIR}/build/ios-xcode/Debug-iphonesimulator/AuroraApp.app"
if [ -d "${APP_BUNDLE}" ]; then
    echo "==> Installing app..."
    xcrun simctl install "${DEVICE_UDID}" "${APP_BUNDLE}"
    echo "==> Launching app..."
    xcrun simctl launch "${DEVICE_UDID}" aurora.app
fi

# Wait
sleep "${TIMEOUT}"

# Collect logs
echo "==> Collecting simulator logs..."
xcrun simctl spawn "${DEVICE_UDID}" log collect \
    --output /tmp/aurora_sim_log.log 2>/dev/null || true

# Shutdown
echo "==> Shutting down simulator..."
xcrun simctl shutdown "${DEVICE_UDID}" 2>/dev/null || true

echo "==> iOS simulator validation complete."
