#!/bin/bash
# Aurora Language Installer — Linux/macOS
# Usage: curl -fsSL https://raw.githubusercontent.com/mhfahim22/Aurora/main/release/install.sh | bash
set -e

REPO="mhfahim22/Aurora"
APP_NAME="Aurora"
INSTALL_DIR="${HOME}/.aurora"

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
    *)
        echo "ERROR: Unsupported OS: $OS" >&2
        echo "  Supported: Linux x86_64, macOS x86_64/aarch64" >&2
        exit 1
        ;;
esac
case "$ARCH" in
    x86_64|amd64) ARCH="x86_64" ;;
    aarch64|arm64) ARCH="aarch64" ;;
    *)
        echo "ERROR: Unsupported architecture: $ARCH" >&2
        echo "  Supported: x86_64, aarch64" >&2
        exit 1
        ;;
esac

if [ "$PLATFORM" = "macos" ] && [ "$ARCH" = "x86_64" ]; then
    ASSET="Aurora-$Version-macos-x86_64.tar.gz"
elif [ "$PLATFORM" = "macos" ] && [ "$ARCH" = "aarch64" ]; then
    ASSET="Aurora-$Version-macos-aarch64.tar.gz"
else
    ASSET="Aurora-$Version-$PLATFORM-$ARCH.tar.gz"
fi

URL="https://github.com/$REPO/releases/download/v$Version/$ASSET"
TEMP_DIR=$(mktemp -d)

echo "==> Downloading $ASSET ..."
curl -fL "$URL" -o "$TEMP_DIR/$ASSET" || {
    echo "ERROR: Download failed. The release may not have a $PLATFORM/$ARCH build yet." >&2
    echo "  Try manually: $URL" >&2
    rm -rf "$TEMP_DIR"
    exit 1
}

# Remove old installation
if [ -f "$INSTALL_DIR/aurorac" ]; then
    echo "  Removing previous installation..."
    rm -rf "$INSTALL_DIR"
fi

mkdir -p "$INSTALL_DIR"

echo "==> Extracting..."
tar -xzf "$TEMP_DIR/$ASSET" -C "$INSTALL_DIR" 2>/dev/null || {
    echo "ERROR: Extraction failed." >&2
    rm -rf "$TEMP_DIR"
    exit 1
}
rm -rf "$TEMP_DIR"

# Make binaries executable
chmod +x "$INSTALL_DIR/aurorac" "$INSTALL_DIR/voss" "$INSTALL_DIR/aurora_lsp" 2>/dev/null || true

# Verify
if [ ! -f "$INSTALL_DIR/aurorac" ]; then
    echo "ERROR: Installation corrupted: aurorac not found" >&2
    exit 1
fi

LIBC_COUNT=$(find "$INSTALL_DIR/libc" -name '*.auf' 2>/dev/null | wc -l)
if [ "$LIBC_COUNT" -eq 0 ]; then
    echo "  [warn] libc/ not found. Standard library imports will not work." >&2
fi

# Add to PATH via shell config
SHELL_CONFIG=""
if [[ ":$PATH:" != *":$INSTALL_DIR:"* ]]; then
    if [ -f "$HOME/.zshrc" ]; then
        SHELL_CONFIG="$HOME/.zshrc"
    elif [ -f "$HOME/.bashrc" ]; then
        SHELL_CONFIG="$HOME/.bashrc"
    elif [ -f "$HOME/.config/fish/config.fish" ]; then
        SHELL_CONFIG="$HOME/.config/fish/config.fish"
    fi

    if [ -n "$SHELL_CONFIG" ]; then
        echo "export PATH=\"\$PATH:$INSTALL_DIR\"" >> "$SHELL_CONFIG"
        echo "  Added $INSTALL_DIR to PATH ($SHELL_CONFIG)"
    else
        echo "  [warn] Could not find shell config. Add this to your shell rc file:"
        echo "    export PATH=\"\$PATH:$INSTALL_DIR\""
    fi
    export PATH="$PATH:$INSTALL_DIR"
fi

echo ""
echo "╔══════════════════════════════════════════════════╗"
echo "║  Aurora Language v$VERSION installed!                ║"
echo "╠══════════════════════════════════════════════════╣"
echo "║  Compiler:    aurorac                            ║"
echo "║  Package Mgr: voss                               ║"
echo "║  Std Library: libc/ (${LIBC_COUNT:-0} files)      ║"
echo "║  Location:    $INSTALL_DIR                       ║"
echo "╚══════════════════════════════════════════════════╝"
echo ""
echo "Quick start:"
echo "  Create hello.aura:"
echo '    echo '\''output("Hello, Aurora!")'\'' > hello.aura'
echo "  Run it:"
echo "    aurorac hello.aura"
echo ""
echo "Project workflow:"
echo "  voss init my-project"
echo "  cd my-project"
echo "  voss run"
echo ""
echo "Cross-ecosystem bridges (requires network):"
echo "  voss bridge pypi requests    # Python packages"
echo "  voss bridge npm lodash       # Node.js packages"
echo "  voss bridge cargo serde      # Rust crates"
echo ""
echo "Documentation: aurora/docs/language.md"
