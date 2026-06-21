#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Aurora Language Installer — one-command setup for Windows.
.DESCRIPTION
    Downloads and installs the latest Aurora release.
    First tries the .exe setup (Inno Setup), falls back to .zip extraction.
    Adds Aurora to PATH and sets AURORA_PATH/AURORA_LIB env vars.
.LINK
    https://github.com/mhfahim22/Aurora
#>

$Repo = "mhfahim22/Aurora"
$AppName = "Aurora"
$InstallDir = "$env:LOCALAPPDATA\Aurora"

# ── Helper: ensure directory exists ──
if (-not (Test-Path $InstallDir)) { New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null }

# ── Fetch latest release ──
Write-Host "Fetching latest Aurora release..." -ForegroundColor Cyan
$ReleaseUrl = "https://api.github.com/repos/$Repo/releases/latest"
try {
    $Release = Invoke-RestMethod -Uri $ReleaseUrl -UseBasicParsing
    $Version = $Release.tag_name -replace '^v', ''
    Write-Host "  Found version: $Version" -ForegroundColor Green
} catch {
    Write-Warning "Could not fetch release info. Falling back to latest tag."
    $TagsUrl = "https://api.github.com/repos/$Repo/tags"
    $Tags = Invoke-RestMethod -Uri $TagsUrl -UseBasicParsing
    if (-not $Tags -or $Tags.Count -eq 0) {
        Write-Error "No releases found."
        exit 1
    }
    $Version = $Tags[0].name -replace '^v', ''
    $Release = $null
}

# ── Try .exe installer first (Inno Setup) ──
$ExeName = "Aurora-$Version-windows-x64-setup.exe"
$ExeUrl = "https://github.com/$Repo/releases/download/v$Version/$ExeName"
$ExePath = "$env:TEMP\$ExeName"

Write-Host "Downloading $ExeName ..." -ForegroundColor Cyan
try {
    Invoke-WebRequest -Uri $ExeUrl -OutFile $ExePath -UseBasicParsing -TimeoutSec 120
    Write-Host "  Downloaded successfully!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Running installer..." -ForegroundColor Cyan
    Start-Process -FilePath $ExePath -Wait
    Remove-Item $ExePath -Force -ErrorAction SilentlyContinue

    # Add Aurora to PATH if not already present
    $UserPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if ($UserPath -notlike "*$InstallDir*") {
        $NewPath = "$InstallDir;$UserPath"
        [Environment]::SetEnvironmentVariable("Path", $NewPath, "User")
        Write-Host "  Added $InstallDir to PATH" -ForegroundColor Green
    }
    # Refresh PATH from registry for current session
    $env:Path = [Environment]::GetEnvironmentVariable('Path','User')
    
    Write-Host ""
    Write-Host "Aurora $Version installed successfully!" -ForegroundColor Green
    Write-Host "  Location: (choose during setup)"
    Write-Host "  Run: aurorac --repl"
    exit 0
} catch {
    Write-Warning "Setup installer not available (no release build yet). Falling back to ZIP."
}

# ── Fallback: download and extract ZIP ──
$AssetName = "Aurora-$Version-windows-x64.zip"
$DownloadUrl = "https://github.com/$Repo/releases/download/v$Version/$AssetName"
$ZipPath = "$env:TEMP\Aurora-$Version.zip"

Write-Host "Downloading $AssetName ..." -ForegroundColor Cyan
try {
    Invoke-WebRequest -Uri $DownloadUrl -OutFile $ZipPath -UseBasicParsing -TimeoutSec 120
} catch {
    Write-Error "Download failed: $($_.Exception.Message)"
    Write-Host "  Try manually: $DownloadUrl" -ForegroundColor Yellow
    exit 1
}

Write-Host "  Extracting..." -ForegroundColor Cyan

# ── Backup existing libc if present ──
$LibcDir = Join-Path $InstallDir "libc"
if (Test-Path $LibcDir) {
    $BackupDir = "$env:TEMP\aurora_libc_backup_$(Get-Date -Format 'yyyyMMddHHmmss')"
    Move-Item -Path $LibcDir -Destination $BackupDir -Force
    Write-Host "  Backed up existing libc to $BackupDir"
}

# ── Extract to temp then copy ──
$ExtractDir = "$env:TEMP\AuroraExtract"
if (Test-Path $ExtractDir) { Remove-Item -Path $ExtractDir -Recurse -Force }
Expand-Archive -Path $ZipPath -DestinationPath $ExtractDir -Force

# ── Copy files ──
Get-ChildItem -Path $ExtractDir | Copy-Item -Destination $InstallDir -Recurse -Force
Remove-Item -Path $ExtractDir -Recurse -Force
Remove-Item -Path $ZipPath -Force

# ── Add to PATH if not already present ──
$UserPath = [Environment]::GetEnvironmentVariable("Path", "User")
if ($UserPath -notlike "*$InstallDir*") {
    $NewPath = "$InstallDir;$UserPath"
    [Environment]::SetEnvironmentVariable("Path", $NewPath, "User")
    Write-Host "  Added $InstallDir to PATH" -ForegroundColor Green
}

# ── Set AURORA_PATH and AURORA_LIB ──
[Environment]::SetEnvironmentVariable("AURORA_PATH", "$InstallDir\libc", "User")
[Environment]::SetEnvironmentVariable("AURORA_LIB", "$InstallDir\libc", "User")
Write-Host "  Set AURORA_PATH = $InstallDir\libc" -ForegroundColor Green

# ── Update current session PATH ──
$env:Path = "$InstallDir;$env:Path"
$env:AURORA_PATH = "$InstallDir\libc"
$env:AURORA_LIB = "$InstallDir\libc"

Write-Host ""
Write-Host "Aurora $Version installed successfully!" -ForegroundColor Green
Write-Host "  Location: $InstallDir"
Write-Host "  libc:     $InstallDir\libc"
Write-Host ""
Write-Host "Quick test:" -ForegroundColor Cyan
Write-Host "  aurorac --repl" -ForegroundColor White
Write-Host "  aurorac --run examples/2d/flappy_bird.aura" -ForegroundColor White
Write-Host ""
Write-Host "Close and reopen your terminal for PATH changes to take effect." -ForegroundColor Yellow
