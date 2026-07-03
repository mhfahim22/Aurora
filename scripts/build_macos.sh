#!/usr/bin/env bash
set -euo pipefail
# ── Aurora macOS Desktop Build Script ──────────────────────────
# Prerequisites:
#   brew install cmake ninja llvm@19 curl

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
JOBS="${JOBS:-$(sysctl -n hw.ncpu)}"
CONFIG="${CONFIG:-Release}"
ARCH="${ARCH:-$(uname -m)}"

echo "==> Aurora macOS Desktop Build"
echo "    Config : ${CONFIG}"
echo "    Arch   : ${ARCH}"
echo "    Jobs   : ${JOBS}"

# Detect LLVM (brew)
if [ -z "${LLVM_DIR:-}" ]; then
    if brew list llvm@19 &>/dev/null; then
        LLVM_PREFIX=$(brew --prefix llvm@19)
        LLVM_DIR="${LLVM_PREFIX}/lib/cmake/llvm"
        export PATH="${LLVM_PREFIX}/bin:$PATH"
    elif brew list llvm &>/dev/null; then
        LLVM_PREFIX=$(brew --prefix llvm)
        LLVM_DIR="${LLVM_PREFIX}/lib/cmake/llvm"
        export PATH="${LLVM_PREFIX}/bin:$PATH"
    else
        echo "ERROR: LLVM not found. Run: brew install llvm@19"
        exit 1
    fi
    echo "    LLVM   : ${LLVM_DIR}"
fi

# Choose generator
if command -v ninja &>/dev/null; then
    GENERATOR="Ninja"
else
    GENERATOR="Unix Makefiles"
fi

# Configure
cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" \
    -G "${GENERATOR}" \
    -DCMAKE_BUILD_TYPE="${CONFIG}" \
    -DLLVM_DIR="${LLVM_DIR}"

# Build core targets
cmake --build "${BUILD_DIR}" --config "${CONFIG}" -j"${JOBS}" \
    --target aurorac \
    --target aurora_runtime \
    --target voss \
    --target aurora_lsp

# Build optional test targets
echo "==> Building test targets..."
cmake --build "${BUILD_DIR}" --config "${CONFIG}" -j"${JOBS}" \
    --target test_fiber --target test_autograd --target test_server \
    2>&1 || echo "    (test targets optional — continuing)"

# Run regression
echo "==> Running regression tests..."
pwsh -File "${PROJECT_DIR}/scripts/regression.ps1" 2>&1 || echo "    (regression warnings — continuing)"

echo "==> Build complete. Artifacts in ${BUILD_DIR}"
ls -la "${BUILD_DIR}/aurorac" "${BUILD_DIR}/voss" "${BUILD_DIR}/aurora_lsp" 2>/dev/null || \
    ls -la "${BUILD_DIR}/Release/aurorac" "${BUILD_DIR}/Release/voss" "${BUILD_DIR}/Release/aurora_lsp" 2>/dev/null || true
