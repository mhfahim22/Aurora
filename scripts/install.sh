#!/bin/bash
# Aurora Language Installer — Linux/macOS
# Usage: curl -fsSL https://raw.githubusercontent.com/mhfahim22/Aurora/main/scripts/install.sh | bash

set -e
REPO="mhfahim22/Aurora"
BIN_DIR="$HOME/.aurora/bin"
APP_NAME="Aurora"

echo "==> Fetching latest Aurora release..."
RELEASE_URL="https://api.github.com/repos/$REPO/releases/latest"
VERSION=$(curl -sf "$RELEASE_URL" | grep '"tag_name"' | sed 's/.*"v\(.*\)".*/\1/')

if [ -z "$VERSION" ]; then
    echo "ERROR: Could not fetch latest release." >&2
    exit 1
fi
echo "  Found version: $VERSION"

# Detect platform
OS="$(uname -s)"
ARCH="$(uname -m)"
case "$OS" in
    Linux)  PLATFORM="linux" ;;
    Darwin) PLATFORM="macos" ;;
    *)      echo "ERROR: Unsupported OS: $OS" >&2; exit 1 ;;
esac
case "$ARCH" in
    x86_64|amd64) ARCH="x86_64" ;;
    aarch64|arm64) ARCH="aarch64" ;;
    *)          echo "ERROR: Unsupported arch: $ARCH" >&2; exit 1 ;;
esac

ASSET="Aurora-$VERSION-$PLATFORM-$ARCH.tar.gz"
URL="https://github.com/$REPO/releases/download/v$VERSION/$ASSET"

mkdir -p "$BIN_DIR"

echo "==> Downloading $ASSET ..."
curl -fL "$URL" -o "/tmp/$ASSET" || {
    echo "ERROR: Download failed. Try manually: $URL" >&2
    exit 1
}

echo "==> Extracting..."
tar -xzf "/tmp/$ASSET" -C "$BIN_DIR" 2>/dev/null || {
    echo "ERROR: Extraction failed. gzip/tar may not be available." >&2
    exit 1
}
rm -f "/tmp/$ASSET"

if [ ! -f "$BIN_DIR/voss" ]; then
    echo "ERROR: Installation corrupted" >&2
    exit 1
fi

# Add to PATH
chmod +x "$BIN_DIR/voss" "$BIN_DIR/aurorac" 2>/dev/null || true

if [[ ":$PATH:" != *":$BIN_DIR:"* ]]; then
    SHELL_CONFIG="$HOME/.bashrc"
    if [ -f "$HOME/.zshrc" ]; then
        SHELL_CONFIG="$HOME/.zshrc"
    elif [ -f "$HOME/.config/fish/config.fish" ]; then
        SHELL_CONFIG="$HOME/.config/fish/config.fish"
    fi
    echo "export PATH=\"\$PATH:$BIN_DIR\"" >> "$SHELL_CONFIG"
    export PATH="$PATH:$BIN_DIR"
    echo "  Added $BIN_DIR to PATH ($SHELL_CONFIG)"
fi

echo ""
echo "╔═══════════════════════════════════════════╗"
echo "║  Aurora Language v$VERSION installed!       ║"
echo "╠═══════════════════════════════════════════╣"
echo "║  Compiler: aurorac                        ║"
echo "║  Package Manager: voss                    ║"
echo "║  Location: $BIN_DIR                       ║"
echo "╚═══════════════════════════════════════════╝"
echo ""
echo "Quick start:"
echo "  voss init my-project"
echo "  cd my-project"
echo "  voss run"
echo ""
