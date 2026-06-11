#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Aurora Language Installer — installs the Aurora compiler and package manager.
.DESCRIPTION
    Downloads the latest Aurora release from GitHub and adds it to PATH.
    Requires: Windows 10+, PowerShell 5+
.LINK
    https://github.com/mhfahim22/Aurora
#>

$Repo = "mhfahim22/Aurora"
$AppName = "Aurora"
$BinDir = "$env:LOCALAPPDATA\Aurora\bin"

# ── Ensure bin directory ──
if (-not (Test-Path $BinDir)) { New-Item -ItemType Directory -Path $BinDir -Force | Out-Null }

# ── Fetch latest release from GitHub API ──
Write-Host "Fetching latest Aurora release..." -ForegroundColor Cyan
$ReleaseUrl = "https://api.github.com/repos/$Repo/releases/latest"
try {
    $Release = Invoke-RestMethod -Uri $ReleaseUrl -UseBasicParsing
    $Version = $Release.tag_name -replace '^v', ''
    Write-Host "Found version: $Version" -ForegroundColor Green
} catch {
    Write-Warning "Could not fetch release info. Falling back to latest tag."
    $TagsUrl = "https://api.github.com/repos/$Repo/tags"
    $Tags = Invoke-RestMethod -Uri $TagsUrl -UseBasicParsing
    if (-not $Tags -or $Tags.Count -eq 0) {
        Write-Error "No releases found. The project may not have any releases yet."
        exit 1
    }
    $Version = $Tags[0].name -replace '^v', ''
    $Release = $null
}

# ── File to download ──
$AssetName = "Aurora-$Version-windows-x64.zip"
$DownloadUrl = "https://github.com/$Repo/releases/download/v$Version/$AssetName"
$ZipPath = "$env:TEMP\Aurora-$Version.zip"

# ── Download ──
Write-Host "Downloading $AssetName ..." -ForegroundColor Cyan
try {
    Invoke-WebRequest -Uri $DownloadUrl -OutFile $ZipPath -UseBasicParsing -TimeoutSec 60
} catch {
    Write-Error "Download failed: $($_.Exception.Message)"
    Write-Host "  Try manually: $DownloadUrl"
    exit 1
}

# ── Extract ──
Write-Host "Extracting..." -ForegroundColor Cyan
try {
    Expand-Archive -Path $ZipPath -DestinationPath $BinDir -Force
} catch {
    Write-Error "Extraction failed: $($_.Exception.Message)"
    exit 1
}

# ── Cleanup zip ──
Remove-Item $ZipPath -Force -ErrorAction SilentlyContinue

# ── Verify ──
$VossExe = "$BinDir\voss.exe"
$AuroracExe = "$BinDir\aurorac.exe"
if (-not (Test-Path $VossExe)) {
    Write-Error "Installation corrupted: voss.exe not found"
    exit 1
}

# ── Add to PATH ──
$UserPath = [Environment]::GetEnvironmentVariable("PATH", "User")
if ($UserPath -notlike "*$BinDir*") {
    $NewPath = if ($UserPath) { "$UserPath;$BinDir" } else { $BinDir }
    [Environment]::SetEnvironmentVariable("PATH", $NewPath, "User")
    # Also update current session
    $env:PATH = "$env:PATH;$BinDir"
    Write-Host "Added $BinDir to PATH (user-level)" -ForegroundColor Green
}

Write-Host ""
Write-Host "╔═══════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║  Aurora Language v$Version installed!       ║" -ForegroundColor Green
Write-Host "╠═══════════════════════════════════════════╣" -ForegroundColor Green
Write-Host "║  Compiler: aurorac.exe                    ║" -ForegroundColor Green
Write-Host "║  Package Manager: voss.exe                ║" -ForegroundColor Green
Write-Host "║  Location: $BinDir" -ForegroundColor Green
Write-Host "╚═══════════════════════════════════════════╝" -ForegroundColor Green
Write-Host ""
Write-Host "Quick start:" -ForegroundColor Cyan
Write-Host "  voss init my-project"
Write-Host "  cd my-project"
Write-Host "  voss run"
Write-Host ""
Write-Host "Cross-ecosystem bridges:" -ForegroundColor Cyan
Write-Host "  voss bridge pypi requests"
Write-Host "  voss bridge npm  lodash"
Write-Host "  voss bridge cargo serde"
Write-Host ""
