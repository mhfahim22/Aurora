#!/bin/bash
# submit_to_windows_store.sh - Submit MSIX to Microsoft Store
set -euo pipefail

usage() {
    echo "Usage: $0 --msix <path> [--app-id <id>] [--tenant-id <id>]"
    echo "  --msix        Path to .msix package"
    echo "  --app-id      Microsoft Store app ID (or set MS_STORE_APP_ID env)"
    echo "  --tenant-id   Azure AD tenant ID (or set MS_TENANT_ID env)"
    exit 1
}

MSIX=""
APP_ID="${MS_STORE_APP_ID:-}"
TENANT_ID="${MS_TENANT_ID:-}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --msix) MSIX="$2"; shift 2 ;;
        --app-id) APP_ID="$2"; shift 2 ;;
        --tenant-id) TENANT_ID="$2"; shift 2 ;;
        *) echo "Unknown option: $1"; usage ;;
    esac
done

if [[ -z "$MSIX" ]]; then
    echo "Error: --msix is required"
    usage
fi
if [[ ! -f "$MSIX" ]]; then
    echo "Error: MSIX file not found: $MSIX"
    exit 1
fi

echo "Submitting $MSIX to Microsoft Store (App ID: ${APP_ID:-not set})..."

# Use the Windows Store Broker API or MSIX Packaging Tool
if command -v msisubmit &>/dev/null; then
    msisubmit --app-id "$APP_ID" --package "$MSIX"
elif command -v az &>/dev/null; then
    # Fallback: Azure CLI with store extension
    az extension add --name store-submission 2>/dev/null || true
    az store submission create \
        --app-id "$APP_ID" \
        --package-path "$MSIX" \
        --tenant-id "$TENANT_ID"
    echo "Submitted via Azure CLI"
else
    echo "Warning: No submission tool found."
    echo "Manual steps:"
    echo "  1. Go to https://partner.microsoft.com/dashboard"
    echo "  2. Navigate to your app > Submissions > New submission"
    echo "  3. Upload $MSIX"
    echo ""
    echo "Install submission tools:"
    echo "  Windows: winget install msistore"
    echo "  Cross-platform: pip install ms-store-submission"
    exit 1
fi

echo "Submission complete! Check Microsoft Partner Center for status."
