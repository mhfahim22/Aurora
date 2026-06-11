#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Creates a GitHub release package: zips up built binaries and runtime DLLs.
.DESCRIPTION
    Collects voss.exe, aurorac.exe, aurora_runtime.dll, and related files
    into a release zip for distribution.
#>

$Root = Split-Path -Parent $PSScriptRoot
$BuildDir = "$Root\build\Release"
$Version = if (Test-Path "$Root\VERSION") { Get-Content "$Root\VERSION" -Raw | ForEach-Object { $_.Trim() } } else { "0.3.0" }
$OutDir = "$Root\dist"

# ── Files to include ──
$Files = @(
    "voss.exe",
    "aurorac.exe",
    "aurora_runtime.dll",
    "aurora_bindgen.exe",
    "aurora_cppwrap.exe",
    "aurora_lsp.exe"
)

# ── Create dist directory ──
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }

# ── Create Windows zip ──
$ZipName = "Aurora-$Version-windows-x64.zip"
$ZipPath = "$OutDir\$ZipName"
$TempDir = "$env:TEMP\AuroraRelease"
if (Test-Path $TempDir) { Remove-Item $TempDir -Recurse -Force }
New-Item -ItemType Directory -Path $TempDir -Force | Out-Null

Write-Host "Packaging $ZipName ..." -ForegroundColor Cyan
$Missing = @()
foreach ($f in $Files) {
    $src = "$BuildDir\$f"
    if (Test-Path $src) {
        Copy-Item $src "$TempDir\" -Force
    } else {
        $Missing += $f
    }
}
if ($Missing.Count -eq $Files.Count) {
    Write-Error "No built binaries found. Run 'cmake --build build --config Release' first."
    exit 1
}

# ── Copy quickjs DLL if exists ──
if (Test-Path "$BuildDir\quickjs.dll") { Copy-Item "$BuildDir\quickjs.dll" "$TempDir\" }

# ── Create .zip ──
if (Test-Path $ZipPath) { Remove-Item $ZipPath -Force }
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory($TempDir, $ZipPath)

# ── Cleanup ──
Remove-Item $TempDir -Recurse -Force

$Size = [math]::Round((Get-Item $ZipPath).Length / 1MB, 1)
Write-Host "  Created: $ZipPath ($Size MB)" -ForegroundColor Green

# ── Also create tar.gz for Linux/macOS ──
Write-Host ""
Write-Host "For Linux/macOS releases, build on respective platforms or use cross-compilation." -ForegroundColor Yellow
Write-Host "Package ready for GitHub release: $OutDir" -ForegroundColor Green
