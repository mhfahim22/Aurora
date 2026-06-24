#!/usr/bin/env pwsh
# Aurora — Auto-build all bridge DLLs
# Run from repository root: .\build_bridges.ps1
# Requires: MSVC C compiler (cl.exe), Python dev headers, Rust, Node.js

$RepoRoot = $PSScriptRoot
$VossExe = Join-Path $RepoRoot "_build\Release\voss.exe"
$BridgeDir = Join-Path $RepoRoot "packages\bridges"

if (-not (Test-Path -LiteralPath $VossExe)) {
    Write-Host "❌ voss not found. Build it first: cmake --build _build --target voss" -ForegroundColor Red
    exit 1
}

# ── npm bridges (via QuickJS C wrapper) ──
Write-Host "`n[1/3] npm bridges..." -ForegroundColor Yellow
$npmPkgs = @("moment", "lodash", "chalk", "execa", "got", "left-pad", "mobx", "uuid")
foreach ($pkg in $npmPkgs) {
    $dllPath = Join-Path $BridgeDir "npm\${pkg}_npm\${pkg}_npm.dll"
    if (Test-Path -LiteralPath $dllPath) {
        Write-Host "  ✅ $pkg — already exists" -ForegroundColor Green
        continue
    }
    Write-Host "  Generating $pkg bridge..." -NoNewline
    & $VossExe bridge npm $pkg 2>&1 | Out-Null
    if ($LASTEXITCODE -eq 0) { Write-Host " ✅" -ForegroundColor Green }
    else { Write-Host " ❌ failed" -ForegroundColor Red }
}

# ── Cargo bridges ──
Write-Host "`n[2/3] Cargo bridges..." -ForegroundColor Yellow
$cargoPkgs = @("hello_rust", "serde", "rand", "regex", "tokio", "uuid", "bitflags",
                "crossbeam", "either", "indexmap", "itertools", "log", "once_cell",
                "parking_lot", "smallvec", "spin", "strsim", "thread_local")
foreach ($pkg in $cargoPkgs) {
    $dllPath = Join-Path $BridgeDir "cargo\${pkg}_cargo\${pkg}_cargo.dll"
    if (Test-Path -LiteralPath $dllPath) {
        Write-Host "  ✅ $pkg — already exists" -ForegroundColor Green
        continue
    }
    Write-Host "  Generating $pkg bridge..." -NoNewline
    & $VossExe bridge cargo $pkg 2>&1 | Out-Null
    if ($LASTEXITCODE -eq 0) { Write-Host " ✅" -ForegroundColor Green }
    else { Write-Host " ❌ failed" -ForegroundColor Red }
}

# ── PyPI bridges (requires Python C API) ──
Write-Host "`n[3/3] PyPI bridges..." -ForegroundColor Yellow
$pypiPkgs = @("markdown", "numpy", "requests", "Pillow", "flask")
foreach ($pkg in $pypiPkgs) {
    $dllPath = Join-Path $RepoRoot "${pkg}_pypi\${pkg}_pypi.dll"
    if (Test-Path -LiteralPath $dllPath) {
        Write-Host "  ✅ $pkg — already exists" -ForegroundColor Green
        continue
    }
    Write-Host "  Generating $pkg bridge..." -NoNewline
    & $VossExe bridge pypi $pkg 2>&1 | Out-Null
    if ($LASTEXITCODE -eq 0) { Write-Host " ✅" -ForegroundColor Green }
    else { Write-Host " ❌ failed" -ForegroundColor Red }
}

Write-Host "`nDone! Run bridge tests: _build\Release\test_bridge_e2e.exe" -ForegroundColor Cyan
