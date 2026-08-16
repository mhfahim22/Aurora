# ── Aurora WASM / Browser Build Script (Phase 40) ─────────────
# Usage: .\scripts\build_wasm.ps1 [-App <app.aura>] [-Out <dir>]
# Prerequisites:
#   clang + wasm-ld (LLVM 18+, e.g. C:\LLVM\bin)
#   aurorac built (cmake --build build --target aurorac)
#
# Produces in <out> (default: out/wasm):
#   app.wasm     WebAssembly module
#   app.js       JS glue (aurora/src/runtime/wasm/wasm_glue.js)
#   index.html   demo page loading app.wasm

param(
    [string]$App = "",
    [string]$Out = "",
    [string]$Clang = "",
    [string]$WasmLd = "",
    [string]$Aurorac = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

if (-not $App) { $App = Join-Path $root "examples\wasm\todo_spa.aura" }
if (-not $Out) { $Out = Join-Path $root "out\wasm" }
$appName = "app"

if (-not $Aurorac) {
    $cand = Join-Path $root "build\aurorac.exe"
    if (Test-Path $cand) { $Aurorac = $cand }
    else { $Aurorac = Get-Command aurorac -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source }
}
if (-not $Clang)     { $Clang = Get-Command clang -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source }
if (-not $WasmLd)    { $WasmLd = Get-Command wasm-ld -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source }
if (-not $Clang)     { throw "clang not found — pass -Clang <path>" }
if (-not $WasmLd)    { throw "wasm-ld not found — pass -WasmLd <path>" }
if (-not $Aurorac)   { throw "aurorac not found — build it first or pass -Aurorac <path>" }
if (-not (Test-Path $App)) { throw "source file not found: $App" }

New-Item -ItemType Directory -Force -Path $Out | Out-Null
Write-Host "==> Aurora WASM Build"
Write-Host "    App : $App"
Write-Host "    Out : $Out"

# 1. Compile the Aurora source to a wasm32 object
& $Aurorac $App --emit-obj --target wasm32-unknown-unknown -o (Join-Path $Out "$appName.o")
if ($LASTEXITCODE -ne 0) { throw "aurorac failed" }

# 2. Compile the WASM runtime (allocator + strings + output) and DOM bindings
& $Clang --target=wasm32-unknown-unknown -nostdlib -O2 -Wno-incompatible-library-redeclaration `
    -c (Join-Path $root "aurora\src\runtime\wasm\wasm_rt.c") -o (Join-Path $Out "wasm_rt.o")
if ($LASTEXITCODE -ne 0) { throw "clang wasm_rt failed" }

& $Clang --target=wasm32-unknown-unknown -std=c++17 -nostdlib -O2 -Wno-incompatible-library-redeclaration `
    -I (Join-Path $root "aurora\include") `
    -c (Join-Path $root "aurora\src\runtime\wasm\dom.cpp") -o (Join-Path $Out "dom.o")
if ($LASTEXITCODE -ne 0) { throw "clang dom failed" }

# 3. Link into a browser-native .wasm module
& $WasmLd --no-entry --export=main --export-table --export-memory --allow-undefined `
    --export=aurora_wasm_alloc_str --export=aurora_wasm_import_buf --export=aurora_wasm_import_setlen `
    --export=aurora_wasm_str_buf --export=aurora_wasm_str_len --export=aurora_wasm_str_set_len `
    --export=aurora_wasm_set_cb --export=aurora_wasm_get_cb `
    -o (Join-Path $Out "$appName.wasm") `
    (Join-Path $Out "$appName.o") (Join-Path $Out "wasm_rt.o") (Join-Path $Out "dom.o")
if ($LASTEXITCODE -ne 0) { throw "wasm-ld failed" }

# 4. Copy JS glue
Copy-Item (Join-Path $root "aurora\src\runtime\wasm\wasm_glue.js") (Join-Path $Out "$appName.js")

# 5. Write index.html demo page
$html = @"
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Aurora WASM App</title>
</head>
<body>
<script src="$appName.js"></script>
</body>
</html>
"@
Set-Content -Path (Join-Path $Out "index.html") -Value $html -Encoding UTF8

Write-Host "==> Done"
Write-Host "    Open $(Join-Path $Out 'index.html') in a browser (serve over http://)"
Get-Item (Join-Path $Out "$appName.wasm") | Select-Object FullName, Length