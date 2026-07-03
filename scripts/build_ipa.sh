#!/bin/bash
# ════════════════════════════════════════════════════════════
# build_ipa.sh — Aurora iOS IPA build script
# ════════════════════════════════════════════════════════════
# Prerequisites:
#   - macOS with Xcode 15+
#   - CocoaPods (pod --version)
#   - Apple Developer account (for signing)
# ════════════════════════════════════════════════════════════

set -e

AURORA_DIR="$(cd "$(dirname "$0")/.." && pwd)"
IOS_PROJECT="$AURORA_DIR/packages/ios"
BUILD_DIR="$AURORA_DIR/build/ios"

echo "[ios] Building Aurora runtime for iOS..."

# Step 1: Build aurora_runtime for iOS using Xcode
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
    -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY="Apple Development" \
    "$AURORA_DIR"

cmake --build . --target aurora_runtime --config Release \
    -- -sdk iphoneos ARCHS="arm64"

# Step 2: Copy libraries to Xcode project
cp "$BUILD_DIR/Release-iphoneos/libaurora_runtime.a" "$IOS_PROJECT/"

# Step 3: Install CocoaPods
cd "$IOS_PROJECT"
pod install

# Step 4: Build and archive
xcodebuild \
    -workspace AuroraApp.xcworkspace \
    -scheme AuroraApp \
    -sdk iphoneos \
    -configuration Release \
    -archivePath "$BUILD_DIR/AuroraApp.xcarchive" \
    archive

# Step 5: Export IPA
xcodebuild \
    -exportArchive \
    -archivePath "$BUILD_DIR/AuroraApp.xcarchive" \
    -exportPath "$BUILD_DIR/AuroraApp.ipa" \
    -exportOptionsPlist "$IOS_PROJECT/exportOptions.plist"

echo "[ios] IPA built: $BUILD_DIR/AuroraApp.ipa"
