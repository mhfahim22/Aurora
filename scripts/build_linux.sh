#!/usr/bin/env bash
set -euo pipefail
# ── Aurora Linux Desktop Build Script ──────────────────────────
# Prerequisites:
#   sudo apt install cmake g++ ninja-build libcurl4-openssl-dev \
#     libgl1-mesa-dev liblld-19-dev
#   (LLVM 19 installed via apt.llvm.org)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
JOBS="${JOBS:-$(nproc)}"
CONFIG="${CONFIG:-Release}"

echo "==> Aurora Linux Desktop Build"
echo "    Config : ${CONFIG}"
echo "    Jobs   : ${JOBS}"
echo "    Build  : ${BUILD_DIR}"

# Detect LLVM
if [ -z "${LLVM_DIR:-}" ]; then
    for ver in 19 18 17; do
        dir="/usr/lib/llvm-${ver}/lib/cmake/llvm"
        if [ -f "${dir}/LLVMConfig.cmake" ]; then
            echo "    LLVM   : ${dir}"
            LLVM_DIR="${dir}"
            break
        fi
    done
    if [ -z "${LLVM_DIR:-}" ]; then
        echo "ERROR: LLVM not found. Install llvm-19-dev."
        exit 1
    fi
fi

# Configure
cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE="${CONFIG}" \
    -DLLVM_DIR="${LLVM_DIR}"

# Build core targets
cmake --build "${BUILD_DIR}" --config "${CONFIG}" -j"${JOBS}" \
    --target aurorac \
    --target aurora_runtime \
    --target voss \
    --target aurora_lsp

# Build optional targets
echo "==> Building test targets..."
cmake --build "${BUILD_DIR}" --config "${CONFIG}" -j"${JOBS}" \
    --target test_fiber --target test_autograd --target test_server \
    2>&1 || echo "    (test targets optional — continuing)"

# Run regression
echo "==> Running regression tests..."
pwsh -File "${PROJECT_DIR}/scripts/regression.ps1" 2>&1 || echo "    (regression warnings — continuing)"

echo "==> Build complete. Artifacts in ${BUILD_DIR}"
echo "    $(find "${BUILD_DIR}" -maxdepth 1 -type f -name 'aurorac' -o -name 'voss' -o -name 'aurora_lsp' -o -name 'libaurora_runtime.a' 2>/dev/null | wc -l) target(s) built."
