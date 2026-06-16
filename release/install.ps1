#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Aurora Language Installer — full installation with stdlib and runtime.
.DESCRIPTION
    Downloads the latest Aurora release package (compiler, stdlib, runtime lib)
    from GitHub and adds it to PATH.
    Requires: Windows 10+, PowerShell 5+
.LINK
    https://github.com/mhfahim22/Aurora
#>

$Repo = "mhfahim22/Aurora"
$AppName = "Aurora"
$InstallDir = "$env:LOCALAPPDATA\Aurora"
$BinDir = "$InstallDir"

# ── Ensure install directory ──
if (-not (Test-Path $InstallDir)) { New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null }

# ── Fetch latest release from GitHub API ──
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
        Write-Error "No releases found. The project may not have any releases yet."
        exit 1
    }
    $Version = $Tags[0].name -replace '^v', ''
    $Release = $null
}

# ── Download ──
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

# ── Extract ──
Write-Host "Extracting to $InstallDir ..." -ForegroundColor Cyan
try {
    # Remove old installation first
    if (Test-Path "$InstallDir\aurorac.exe") {
        Write-Host "  Removing previous installation..." -ForegroundColor Yellow
        Remove-Item "$InstallDir\*" -Recurse -Force -ErrorAction SilentlyContinue
    }
    Expand-Archive -Path $ZipPath -DestinationPath $InstallDir -Force
} catch {
    Write-Error "Extraction failed: $($_.Exception.Message)"
    exit 1
}

# ── Cleanup zip ──
Remove-Item $ZipPath -Force -ErrorAction SilentlyContinue

# ── Verify ──
$Exe = "$InstallDir\aurorac.exe"
if (-not (Test-Path $Exe)) {
    Write-Error "Installation corrupted: aurorac.exe not found"
    exit 1
}

# ── Verify libc/ ──
$LibcCount = @(Get-ChildItem "$InstallDir\libc\*.auf" -ErrorAction SilentlyContinue).Count
if ($LibcCount -eq 0) {
    Write-Warning "libc/ standard library not found. Some imports will not work."
} else {
    Write-Host "  [ok]   libc/ ($LibcCount .auf files)" -ForegroundColor Green
}

# ── Verify runtime lib ──
if (Test-Path "$InstallDir\lib\aurora_runtime.lib") {
    Write-Host "  [ok]   aurora_runtime.lib (for AOT compilation)" -ForegroundColor Green
}

# ── Add to PATH ──
$UserPath = [Environment]::GetEnvironmentVariable("PATH", "User")
if ($UserPath -notlike "*$InstallDir*") {
    $NewPath = if ($UserPath) { "$UserPath;$InstallDir" } else { $InstallDir }
    [Environment]::SetEnvironmentVariable("PATH", $NewPath, "User")
    $env:PATH = "$env:PATH;$InstallDir"
    Write-Host "  [ok]   Added $InstallDir to PATH (user-level)" -ForegroundColor Green
}

# ── Print summary ──
Write-Host ""
Write-Host "╔═══════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║  Aurora Language v$Version installed!             ║" -ForegroundColor Green
Write-Host "╠═══════════════════════════════════════════════╣" -ForegroundColor Green
Write-Host "║  Compiler:    aurorac.exe                     ║" -ForegroundColor Green
Write-Host "║  Package Mgr: voss.exe                        ║" -ForegroundColor Green
Write-Host "║  Std Library: libc/ ($LibcCount files)         ║" -ForegroundColor Green
Write-Host "║  Location:    $InstallDir" -ForegroundColor Green
Write-Host "╚═══════════════════════════════════════════════╝" -ForegroundColor Green
Write-Host ""
Write-Host "Quick start:" -ForegroundColor White
Write-Host "  Create a file hello.aura:" -ForegroundColor Cyan
Write-Host '    output("Hello, Aurora!")' -ForegroundColor White
Write-Host "  Run it:" -ForegroundColor Cyan
Write-Host "    aurorac hello.aura" -ForegroundColor White
Write-Host ""
Write-Host "Project workflow:" -ForegroundColor White
Write-Host "  voss init my-project" -ForegroundColor Cyan
Write-Host "  cd my-project" -ForegroundColor Cyan
Write-Host "  voss run" -ForegroundColor Cyan
Write-Host ""
Write-Host "Cross-ecosystem bridges (requires network):" -ForegroundColor White
Write-Host "  voss bridge pypi requests    # Python packages" -ForegroundColor Cyan
Write-Host "  voss bridge npm lodash       # Node.js packages" -ForegroundColor Cyan
Write-Host "  voss bridge cargo serde      # Rust crates" -ForegroundColor Cyan
Write-Host ""
Write-Host "Documentation: aurora/docs/language.md" -ForegroundColor White
