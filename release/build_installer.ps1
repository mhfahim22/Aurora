#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Builds the Aurora Windows release package + installer.
.DESCRIPTION
    1. Builds all executables in Release mode
    2. Creates the release ZIP package
    3. If Inno Setup (ISCC.exe) is available, compiles setup.iss
    4. Output: release/Aurora-<version>-windows-x64-setup.exe
#>

$RepoRoot = Split-Path -Parent $PSScriptRoot
$BuildDir  = Join-Path $RepoRoot "build"
$ReleaseDir = $PSScriptRoot

# ── Step 1: Build Release ──
Write-Host "==> Building Aurora Release..." -ForegroundColor Cyan
Push-Location $BuildDir
try {
    cmake --build . --config Release 2>&1 | Out-Host
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Build failed!"
        exit 1
    }
} finally {
    Pop-Location
}

# ── Step 2: Create release ZIP ──
Write-Host ""
Write-Host "==> Creating release ZIP package..." -ForegroundColor Cyan
& (Join-Path $ReleaseDir "package_release.ps1")

# ── Step 3: Find Inno Setup ──
$IsccPaths = @(
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
    "${env:ProgramFiles(x86)}\Inno Setup 5\ISCC.exe"
    "${env:ProgramFiles}\Inno Setup 6\ISCC.exe"
    "${env:ProgramFiles}\Inno Setup 5\ISCC.exe"
)

$Iscc = $null
foreach ($p in $IsccPaths) {
    if (Test-Path $p) { $Iscc = $p; break }
}

if (-not $Iscc) {
    $Iscc = Get-Command "ISCC.exe" -ErrorAction SilentlyContinue
}

if (-not $Iscc) {
    Write-Warning "Inno Setup (ISCC.exe) not found."
    Write-Host ""
    Write-Host "  To compile the .exe installer:" -ForegroundColor Yellow
    Write-Host "  1. Download Inno Setup from https://jrsoftware.org/isdl.php" -ForegroundColor Yellow
    Write-Host "  2. Install it (default location is fine)" -ForegroundColor Yellow
    Write-Host "  3. Run this script again" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "  The ZIP package is ready at release/ - users can use install.ps1" -ForegroundColor Green
    exit 0
}

# ── Step 4: Compile installer ──
Write-Host ""
Write-Host "==> Compiling installer with Inno Setup..." -ForegroundColor Cyan
Push-Location $ReleaseDir
try {
    & $Iscc "setup.iss" 2>&1 | Out-Host
    if ($LASTEXITCODE -eq 0) {
        Write-Host ""
        Write-Host "SUCCESS:" -ForegroundColor Green
        Get-ChildItem -Path $ReleaseDir -Filter "Aurora-*-setup.exe" | ForEach-Object {
            Write-Host "  $($_.Name) ($([math]::Round($_.Length / 1MB, 1)) MB)" -ForegroundColor Green
        }
    } else {
        Write-Error "Inno Setup compilation failed (exit code: $LASTEXITCODE)"
    }
} finally {
    Pop-Location
}
