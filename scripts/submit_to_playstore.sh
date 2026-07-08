#!/bin/bash
# submit_to_playstore.sh - Upload Android AAB/APK to Google Play
set -euo pipefail

usage() {
    echo "Usage: $0 --aab <path> [--service-account <json>] [--package-name <name>]"
    echo "  --aab               Path to .aab (preferred) or .apk file"
    echo "  --service-account   Path to service account JSON key (or set GOOGLE_SERVICE_ACCOUNT env)"
    echo "  --package-name      Android package name (or set ANDROID_PACKAGE_NAME env)"
    exit 1
}

AAB=""
SERVICE_ACCOUNT="${GOOGLE_SERVICE_ACCOUNT:-}"
PACKAGE_NAME="${ANDROID_PACKAGE_NAME:-}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --aab) AAB="$2"; shift 2 ;;
        --service-account) SERVICE_ACCOUNT="$2"; shift 2 ;;
        --package-name) PACKAGE_NAME="$2"; shift 2 ;;
        *) echo "Unknown option: $1"; usage ;;
    esac
done

if [[ -z "$AAB" ]]; then
    echo "Error: --aab is required"
    usage
fi
if [[ -z "$SERVICE_ACCOUNT" ]]; then
    echo "Error: --service-account is required (or set GOOGLE_SERVICE_ACCOUNT)"
    usage
fi
if [[ -z "$PACKAGE_NAME" ]]; then
    echo "Error: --package-name is required (or set ANDROID_PACKAGE_NAME)"
    usage
fi
if [[ ! -f "$AAB" ]]; then
    echo "Error: File not found: $AAB"
    exit 1
fi
if [[ ! -f "$SERVICE_ACCOUNT" ]]; then
    echo "Error: Service account file not found: $SERVICE_ACCOUNT"
    exit 1
fi

echo "Uploading $AAB to Google Play (package: $PACKAGE_NAME)..."

# Use google-play-publisher if available, else java -jar
if command -v bundletool &>/dev/null; then
    echo "Using bundletool..."
    java -jar "$(which bundletool)" build-bundle --module="$AAB" --output="$AAB"
fi

if command -v google-play-publisher &>/dev/null; then
    google-play-publisher \
        --service-account "$SERVICE_ACCOUNT" \
        --package-name "$PACKAGE_NAME" \
        --aab "$AAB" \
        --track production
    echo "Uploaded via google-play-publisher"
else
    echo "google-play-publisher not found."
    echo "Install: pip install google-play-publisher"
    echo "Or manually upload via Google Play Console."
    echo ""
    echo "File: $AAB"
    echo "Package: $PACKAGE_NAME"
    exit 1
fi

echo "Submission complete! Check Google Play Console for status."
