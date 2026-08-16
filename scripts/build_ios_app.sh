#!/usr/bin/env bash
set -euo pipefail
# ── Aurora iOS App Build Script ─────────────────────────────
# Compiles an Aurora .aura file → .a → .app → .ipa
#
# Usage:
#   ./build_ios_app.sh path/to/app.aura [output_name] [mode] [--export-method method]
#
# Modes:
#   build        Build .app for device (default)
#   archive      Build + archive (requires signing team)
#   export-ipa   Export archived .xcarchive → .ipa
#   all          Build + archive + export IPA (full pipeline)
#
# Export methods:
#   app-store     App Store / TestFlight
#   development   Development provisioning
#
# Prerequisites:
#   - macOS with Xcode 15+
#   - aurorac compiler in PATH
#   - LLVM 19 (brew install llvm@19)
#   - Signing: set DEVELOPMENT_TEAM, or pass via env:
#       DEVELOPMENT_TEAM=ABCDE12345 CODE_SIGN_IDENTITY="Apple Development" \
#       ./build_ios_app.sh app.aura app all --export-method development
#
# Examples:
#   ./build_ios_app.sh examples/mobile/counter.aura counter build
#   ./build_ios_app.sh examples/mobile/todo.aura todo archive
#   ./build_ios_app.sh examples/mobile/todo.aura todo all --export-method app-store
# ════════════════════════════════════════════════════════════

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

AURA_FILE="${1:-}"
OUTPUT_NAME="${2:-app}"
MODE="${3:-build}"
EXPORT_METHOD="${EXPORT_METHOD:-app-store}"
TEAM_ID="${DEVELOPMENT_TEAM:-}"

# Parse --export-method from remaining args
for arg in "$@"; do
    case "${arg}" in
        --export-method=*) EXPORT_METHOD="${arg#*=}" ;;
        --export-method) shift ;;
    esac
done

JOBS="${JOBS:-$(sysctl -n hw.ncpu)}"

if [ -z "${AURA_FILE}" ]; then
    echo "Usage: $0 path/to/app.aura [output_name] [mode] [--export-method method]"
    echo ""
    echo "Modes: build | archive | export-ipa | all"
    echo "Export methods: app-store | development"
    echo ""
    echo "Examples:"
    echo "  $0 examples/mobile/counter.aura counter build"
    echo "  $0 examples/mobile/todo.aura todo all --export-method app-store"
    exit 1
fi

if [ ! -f "${AURA_FILE}" ]; then
    echo "ERROR: File not found: ${AURA_FILE}"
    exit 1
fi

case "${EXPORT_METHOD}" in
    app-store|development) ;;
    *) echo "ERROR: Invalid export method '${EXPORT_METHOD}'. Use app-store or development."; exit 1 ;;
esac

BUILD_DIR="${PROJECT_DIR}/build/ios_app/${OUTPUT_NAME}"
ARCHIVE_DIR="${BUILD_DIR}/Archive"
APP_NAME="${OUTPUT_NAME}"
EXPORT_DIR="${BUILD_DIR}/export"
IPA_FILE="${EXPORT_DIR}/${APP_NAME}.ipa"

echo "==> Aurora iOS App Build"
echo "    Source       : ${AURA_FILE}"
echo "    Output       : ${OUTPUT_NAME}"
echo "    Mode         : ${MODE}"
echo "    Export method: ${EXPORT_METHOD}"
echo "    Build dir    : ${BUILD_DIR}"

mkdir -p "${BUILD_DIR}" "${ARCHIVE_DIR}" "${EXPORT_DIR}"

# 1. Compile Aurora → static library
compile_aurora() {
    echo "==> Compiling Aurora → .a..."
    aurorac "${AURA_FILE}" -o "${BUILD_DIR}/libaurora_app.a" \
        --static --target arm64-apple-ios 2>&1 || {
        echo "ERROR: aurorac compilation failed."
        echo "  Make sure aurorac is built and in PATH."
        exit 1
    }
}

# 2. Build .app via packages/ios Xcode project
build_app() {
    echo "==> Building iOS app via packages/ios..."
    local IOS_PROJECT="${PROJECT_DIR}/packages/ios"
    if [ ! -d "${IOS_PROJECT}" ]; then
        echo "ERROR: iOS wrapper project not found at ${IOS_PROJECT}"
        exit 1
    fi

    # Place compiled static library where Xcode project expects it
    mkdir -p "${IOS_PROJECT}/AuroraApp/libs"
    cp "${BUILD_DIR}/libaurora_app.a" "${IOS_PROJECT}/AuroraApp/libs/"

    local xcode_args=(
        -project "${IOS_PROJECT}/AuroraApp.xcodeproj"
        -scheme AuroraApp
        -configuration Release
        -sdk iphoneos
        -jobs "${JOBS}"
        CONFIGURATION_BUILD_DIR="${BUILD_DIR}/build"
        CODE_SIGNING_ALLOWED=YES
    )
    if [ -n "${TEAM_ID}" ]; then
        xcode_args+=( DEVELOPMENT_TEAM="${TEAM_ID}" )
    fi

    xcodebuild "${xcode_args[@]}" build 2>&1 || {
        echo "ERROR: Xcode build failed."
        echo "  Manual: cd packages/ios && xcodebuild -project AuroraApp.xcodeproj -scheme AuroraApp build"
        exit 1
    }
    echo "    .app built at ${BUILD_DIR}/build/AuroraApp.app"
}

# 3. Archive the .app
archive_app() {
    echo "==> Archiving iOS app..."
    local IOS_PROJECT="${PROJECT_DIR}/packages/ios"
    local xcode_args=(
        -project "${IOS_PROJECT}/AuroraApp.xcodeproj"
        -scheme AuroraApp
        -configuration Release
        -sdk iphoneos
        -jobs "${JOBS}"
        -archivePath "${ARCHIVE_DIR}/${APP_NAME}.xcarchive"
        CONFIGURATION_BUILD_DIR="${BUILD_DIR}/build"
        CODE_SIGNING_ALLOWED=YES
    )
    if [ -n "${TEAM_ID}" ]; then
        xcode_args+=( DEVELOPMENT_TEAM="${TEAM_ID}" )
    fi

    xcodebuild "${xcode_args[@]}" archive 2>&1 || {
        echo "ERROR: Archive failed. Check signing configuration."
        echo "  Set DEVELOPMENT_TEAM=XXXXXX and ensure provisioning profiles exist."
        exit 1
    }
    echo "    Archive: ${ARCHIVE_DIR}/${APP_NAME}.xcarchive"
}

# 4. Export .xcarchive → .ipa via ExportOptions.plist
export_ipa() {
    echo "==> Exporting IPA (${EXPORT_METHOD})..."
    local archive_path="${ARCHIVE_DIR}/${APP_NAME}.xcarchive"
    if [ ! -d "${archive_path}" ]; then
        echo "ERROR: Archive not found — run archive mode first."
        exit 1
    fi

    # Generate ExportOptions.plist — use committed template when available
    local export_options="${EXPORT_DIR}/ExportOptions.plist"
    local template="${PROJECT_DIR}/packages/ios/ExportOptions/${EXPORT_METHOD}.plist"
    if [ -f "${template}" ]; then
        cp "${template}" "${export_options}"
        if [ -n "${TEAM_ID}" ]; then
            # Inject teamID into the committed template for signing
            sed -i '' "s|</dict>|<key>teamID</key><string>${TEAM_ID}</string></dict>|" "${export_options}" 2>/dev/null || \
            sed -i "s|</dict>|<key>teamID</key><string>${TEAM_ID}</string></dict>|" "${export_options}"
        fi
    else
        cat > "${export_options}" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>method</key>
    <string>${EXPORT_METHOD}</string>
    <key>compileBitcode</key>
    <false/>
    <key>stripSwiftSymbols</key>
    <true/>
    <key>signingStyle</key>
    <string>automatic</string>
PLIST
        if [ -n "${TEAM_ID}" ]; then
            cat >> "${export_options}" <<PLIST
    <key>teamID</key>
    <string>${TEAM_ID}</string>
PLIST
        fi
        cat >> "${export_options}" <<PLIST
</dict>
</plist>
PLIST
    fi

    xcodebuild -exportArchive \
        -archivePath "${archive_path}" \
        -exportOptionsPlist "${export_options}" \
        -exportPath "${EXPORT_DIR}" 2>&1 || {
        echo "ERROR: IPA export failed."
        echo "  Check code signing identity and provisioning profiles."
        exit 1
    }
    echo "    IPA: ${IPA_FILE}"
}

# ── Run selected mode ──
compile_aurora
case "${MODE}" in
    build)
        build_app
        ;;
    archive)
        build_app
        archive_app
        ;;
    export-ipa)
        build_app
        archive_app
        export_ipa
        ;;
    all)
        build_app
        archive_app
        export_ipa
        ;;
    *)
        echo "ERROR: Unknown mode '${MODE}'. Use build | archive | export-ipa | all."
        exit 1
        ;;
esac

echo ""
echo "==> iOS build complete."
echo "    Static lib : ${BUILD_DIR}/libaurora_app.a"
if [ -f "${IPA_FILE}" ]; then
    echo "    IPA        : ${IPA_FILE}"
elif [ -d "${ARCHIVE_DIR}/${APP_NAME}.xcarchive" ]; then
    echo "    Archive    : ${ARCHIVE_DIR}/${APP_NAME}.xcarchive"
fi