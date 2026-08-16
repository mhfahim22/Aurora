#!/usr/bin/env bash
set -euo pipefail
# ── Aurora WASM / Browser Build Script (Phase 40) ─────────────
# Usage: scripts/build_wasm.sh <app.aura> [output_dir]
# Prerequisites:
#   clang + wasm-ld (LLVM 18+)  e.g. apt install clang lld
#   aurorac built (see build_linux.sh / build_macos.sh)
#
# Produces in <output_dir> (default: out/wasm):
#   app.wasm     WebAssembly module
#   app.js       JS glue (aurora/src/runtime/wasm/wasm_glue.js)
#   index.html   demo page loading app.wasm

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

APP="${1:-${PROJECT_DIR}/examples/wasm/todo_spa.aura}"
OUT="${2:-${PROJECT_DIR}/out/wasm}"
APP_NAME="app"

if [ ! -f "${APP}" ]; then
    echo "error: source file not found: ${APP}" >&2
    exit 1
fi

AURORAC="${AURORAC:-${PROJECT_DIR}/build/aurorac}"
CLANG="${CLANG:-clang}"
WASM_LD="${WASM_LD:-wasm-ld}"

for tool in "${AURORAC}" "${CLANG}" "${WASM_LD}"; do
    command -v "${tool}" >/dev/null 2>&1 || { echo "error: tool not found: ${tool}" >&2; exit 1; }
done

mkdir -p "${OUT}"
echo "==> Aurora WASM Build"
echo "    App   : ${APP}"
echo "    Out   : ${OUT}"

# 1. Compile the Aurora source to a wasm32 object
"${AURORAC}" "${APP}" --emit-obj --target wasm32-unknown-unknown \
    -o "${OUT}/${APP_NAME}.o"

# 2. Compile the WASM runtime (allocator + strings + output) and DOM bindings
"${CLANG}" --target=wasm32-unknown-unknown -nostdlib -O2 \
    -Wno-incompatible-library-redeclaration \
    -c "${PROJECT_DIR}/aurora/src/runtime/wasm/wasm_rt.c" \
    -o "${OUT}/wasm_rt.o"
"${CLANG}" --target=wasm32-unknown-unknown -std=c++17 -nostdlib -O2 \
    -Wno-incompatible-library-redeclaration \
    -I "${PROJECT_DIR}/aurora/include" \
    -c "${PROJECT_DIR}/aurora/src/runtime/wasm/dom.cpp" \
    -o "${OUT}/dom.o"

# 3. Link into a browser-native .wasm module
"${WASM_LD}" --no-entry --export=main --export-table --export-memory \
    --allow-undefined \
    --export=aurora_wasm_alloc_str --export=aurora_wasm_import_buf --export=aurora_wasm_import_setlen \
    --export=aurora_wasm_str_buf --export=aurora_wasm_str_len --export=aurora_wasm_str_set_len \
    --export=aurora_wasm_set_cb --export=aurora_wasm_get_cb \
    -o "${OUT}/${APP_NAME}.wasm" \
    "${OUT}/${APP_NAME}.o" "${OUT}/wasm_rt.o" "${OUT}/dom.o"

# 4. Copy JS glue
cp "${PROJECT_DIR}/aurora/src/runtime/wasm/wasm_glue.js" "${OUT}/${APP_NAME}.js"

# 5. Write index.html demo page
cat > "${OUT}/index.html" <<HTML
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Aurora WASM App</title>
</head>
<body>
<script src="${APP_NAME}.js"></script>
</body>
</html>
HTML

echo "==> Done"
echo "    Open ${OUT}/index.html in a browser (serve over http://, e.g. python3 -m http.server)"
ls -la "${OUT}/${APP_NAME}.wasm"