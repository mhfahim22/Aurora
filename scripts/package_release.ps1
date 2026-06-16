#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Creates a full Aurora release package with all required files.
.DESCRIPTION
    Packages aurorac, voss, libc/ standard library, runtime libs,
    and optional bridge DLLs into platform-specific archives.
#>

param(
    [string]$Version,
    [switch]$SkipExisting
)

$Root = Split-Path -Parent $PSScriptRoot
$BuildDir = "$Root\build\Release"
$OutDir = "$Root\dist"

if (-not $Version) {
    $Version = if (Test-Path "$Root\VERSION") { (Get-Content "$Root\VERSION" -Raw).Trim() } else { "0.2.0" }
}

Write-Host "Packaging Aurora v$Version" -ForegroundColor Cyan
Write-Host "  Build dir: $BuildDir" -ForegroundColor Cyan
Write-Host "  Output:    $OutDir" -ForegroundColor Cyan
Write-Host ""

if (-not (Test-Path $BuildDir\aurorac.exe)) {
    Write-Error "No built binaries found. Run 'cmake --build build --config Release' first."
    exit 1
}

# ── Create temp staging directory ──
$TempDir = "$env:TEMP\AuroraRelease_$Version"
if (Test-Path $TempDir) { Remove-Item $TempDir -Recurse -Force }
New-Item -ItemType Directory -Path $TempDir -Force | Out-Null

# ── Copy binaries ──
$Binaries = @(
    "aurorac.exe",
    "voss.exe",
    "aurora_lsp.exe"
)
# Optional binaries (may not exist)
$OptionalBinaries = @(
    "aurora_bindgen.exe",
    "aurora_cppwrap.exe"
)

foreach ($f in $Binaries) {
    Copy-Item "$BuildDir\$f" "$TempDir\" -Force -ErrorAction Stop
}
foreach ($f in $OptionalBinaries) {
    if (Test-Path "$BuildDir\$f") { Copy-Item "$BuildDir\$f" "$TempDir\" -Force }
    else { Write-Host "  [skip] $f (not found)" -ForegroundColor Yellow }
}

# ── Copy runtime library (for AOT linking) ──
if (Test-Path "$BuildDir\aurora_runtime.lib") {
    New-Item -ItemType Directory -Path "$TempDir\lib" -Force | Out-Null
    Copy-Item "$BuildDir\aurora_runtime.lib" "$TempDir\lib\" -Force
    Write-Host "  [ok]   aurora_runtime.lib" -ForegroundColor Green
} else {
    Write-Host "  [skip] aurora_runtime.lib (not found)" -ForegroundColor Yellow
}

if (Test-Path "$BuildDir\aurora_parser.lib") {
    Copy-Item "$BuildDir\aurora_parser.lib" "$TempDir\lib\" -Force
    Write-Host "  [ok]   aurora_parser.lib" -ForegroundColor Green
}

# ── Copy libc/ (standard library .auf files) ──
$LibcDir = "$Root\libc"
if (Test-Path $LibcDir) {
    $DestLibc = "$TempDir\libc"
    New-Item -ItemType Directory -Path $DestLibc -Force | Out-Null
    Copy-Item "$LibcDir\*.auf" $DestLibc -Force
    Write-Host "  [ok]   libc/ ($((Get-ChildItem $DestLibc -Filter *.auf).Count) .auf files)" -ForegroundColor Green
} else {
    Write-Host "  [warn] libc/ directory not found at $LibcDir" -ForegroundColor Yellow
}

# ── Copy glfw3.dll (if present) ──
if (Test-Path "$BuildDir\glfw3.dll") {
    Copy-Item "$BuildDir\glfw3.dll" "$TempDir\" -Force
    Write-Host "  [ok]   glfw3.dll" -ForegroundColor Green
}

# ── Copy any bridge DLLs ──
$BridgeDlls = Get-ChildItem "$BuildDir" -Filter "*bridge*.dll" | Where-Object { $_.Name -ne "cpp_interop_test.dll" }
if ($BridgeDlls) {
    New-Item -ItemType Directory -Path "$TempDir\bridges" -Force | Out-Null
    foreach ($dll in $BridgeDlls) {
        Copy-Item $dll.FullName "$TempDir\bridges\" -Force
        Write-Host "  [ok]   bridges/$($dll.Name)" -ForegroundColor Green
    }
} else {
    Write-Host "  [skip] bridge DLLs (none built)" -ForegroundColor Yellow
}

# ── Copy VERSION ──
Copy-Item "$Root\VERSION" "$TempDir\" -Force

# ── Create Windows .zip ──
$ZipName = "Aurora-$Version-windows-x64.zip"
$ZipPath = "$OutDir\$ZipName"
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }
if (Test-Path $ZipPath) { Remove-Item $ZipPath -Force }

Write-Host ""
Write-Host "Creating $ZipName ..." -ForegroundColor Cyan
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory($TempDir, $ZipPath)
$Size = [math]::Round((Get-Item $ZipPath).Length / 1MB, 1)
Write-Host "  Created: $ZipPath ($Size MB)" -ForegroundColor Green

# ── Also create .tar.gz for Linux/macOS (same content, no .exe rename needed) ──
if (Get-Command tar -ErrorAction SilentlyContinue) {
    $TarName = "Aurora-$Version-linux-x86_64.tar.gz"
    $TarPath = "$OutDir\$TarName"
    Push-Location $TempDir
    tar -czf $TarPath *
    Pop-Location
    $TarSize = [math]::Round((Get-Item $TarPath).Length / 1MB, 1)
    Write-Host "  Created: $TarPath ($TarSize MB)" -ForegroundColor Green
} else {
    Write-Host "  [skip] tar.gz (tar not available on this system)" -ForegroundColor Yellow
}

# ── Cleanup ──
Remove-Item $TempDir -Recurse -Force

Write-Host ""
Write-Host "Package ready for GitHub release: $OutDir" -ForegroundColor Green
Write-Host ""
Write-Host "To create a GitHub release, run:" -ForegroundColor White
Write-Host "  gh release create v$Version $OutDir\* --title ""Aurora v$Version"" --notes ""See CHANGELOG.md for details""" -ForegroundColor Cyan
