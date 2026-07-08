#!/bin/bash
# submit_to_appstore.sh - Validate and upload iOS IPA to App Store
set -euo pipefail

usage() {
    echo "Usage: $0 --ipa <path> [--apple-id <email>] [--password <app-specific>] [--team-id <id>]"
    echo "  --ipa         Path to .ipa file"
    echo "  --apple-id    Apple ID email (or set APPLE_ID env)"
    echo "  --password    App-specific password (or set APPLE_PASSWORD env)"
    echo "  --team-id     Team ID (or set APPLE_TEAM_ID env)"
    exit 1
}

IPA=""
APPLE_ID="${APPLE_ID:-}"
APPLE_PASSWORD="${APPLE_PASSWORD:-}"
TEAM_ID="${APPLE_TEAM_ID:-}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --ipa) IPA="$2"; shift 2 ;;
        --apple-id) APPLE_ID="$2"; shift 2 ;;
        --password) APPLE_PASSWORD="$2"; shift 2 ;;
        --team-id) TEAM_ID="$2"; shift 2 ;;
        *) echo "Unknown option: $1"; usage ;;
    esac
done

if [[ -z "$IPA" ]]; then
    echo "Error: --ipa is required"
    usage
fi

if [[ ! -f "$IPA" ]]; then
    echo "Error: IPA file not found: $IPA"
    exit 1
fi

echo "Validating $IPA..."

# Validate with altool
xcrun altool --validate-app -f "$IPA" \
    -t ios \
    -u "${APPLE_ID}" \
    -p "${APPLE_PASSWORD}" \
    --output-format xml

echo "Uploading to App Store Connect..."
xcrun altool --upload-app -f "$IPA" \
    -t ios \
    -u "${APPLE_ID}" \
    -p "${APPLE_PASSWORD}" \
    --output-format xml

echo "Submission complete! Check App Store Connect for status."
