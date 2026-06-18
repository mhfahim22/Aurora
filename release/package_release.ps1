#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Creates the Aurora release package (ZIP) with all runtime dependencies.
.DESCRIPTION
    Packages aurorac.exe, voss.exe, all tools, DLLs, libc/*.auf,
    headers, docs, and examples into a single ZIP file.
#>

$RepoRoot = Split-Path -Parent $PSScriptRoot
$BuildDir  = Join-Path $RepoRoot "build\Release"
$ReleaseDir = $PSScriptRoot

$Version = "0.2.0"
$ZipName = "Aurora-$Version-windows-x64.zip"
$ZipPath = Join-Path $ReleaseDir $ZipName

Write-Host "==> Creating release package: $ZipName" -ForegroundColor Cyan

# ── Create staging directory ──
$Stage = Join-Path $env:TEMP "aurora_package_stage"
if (Test-Path $Stage) { Remove-Item -Path $Stage -Recurse -Force }
New-Item -ItemType Directory -Path $Stage -Force | Out-Null

# ── Copy executables ──
Write-Host "  Copying tools..." -ForegroundColor Cyan
@(
    "aurorac.exe"
    "voss.exe"
    "aurora_lsp.exe"
    "aurora_bindgen.exe"
    "aurora_cppwrap.exe"
) | ForEach-Object {
    $src = Join-Path $BuildDir $_
    if (Test-Path $src) { Copy-Item -Path $src -Destination $Stage }
    else { Write-Warning "  NOT FOUND: $_" }
}

# ── Copy DLLs ──
Write-Host "  Copying DLLs..." -ForegroundColor Cyan
@(
    "glfw3.dll"
) | ForEach-Object {
    $src = Join-Path $BuildDir $_
    if (Test-Path $src) { Copy-Item -Path $src -Destination $Stage }
    else { Write-Warning "  NOT FOUND: $_" }
}

# ── Copy standard library ──
Write-Host "  Copying libc/ ..." -ForegroundColor Cyan
$LibcStage = Join-Path $Stage "libc"
New-Item -ItemType Directory -Path $LibcStage -Force | Out-Null
Get-ChildItem -Path (Join-Path $RepoRoot "libc") -Filter "*.auf" | Copy-Item -Destination $LibcStage

# ── Copy static library ──
$LibDir = Join-Path $Stage "lib"
New-Item -ItemType Directory -Path $LibDir -Force | Out-Null
$RuntimeLib = Join-Path $BuildDir "aurora_runtime.lib"
if (Test-Path $RuntimeLib) { Copy-Item -Path $RuntimeLib -Destination $LibDir }

# ── Copy headers ──
Write-Host "  Copying headers..." -ForegroundColor Cyan
$IncludeDir = Join-Path $Stage "include"
New-Item -ItemType Directory -Path $IncludeDir -Force | Out-Null

# runtime headers
$RunInc = Join-Path $IncludeDir "runtime"
New-Item -ItemType Directory -Path $RunInc -Force | Out-Null
Get-ChildItem -Path (Join-Path $RepoRoot "aurora\include\runtime") -Filter "*.hpp" | Copy-Item -Destination $RunInc
Get-ChildItem -Path (Join-Path $RepoRoot "aurora\include\runtime") -Filter "*.h" | Copy-Item -Destination $RunInc

# interop headers
$InteropDir = Join-Path $RunInc "interop"
New-Item -ItemType Directory -Path $InteropDir -Force | Out-Null
Get-ChildItem -Path (Join-Path $RepoRoot "aurora\include\runtime\interop") -Filter "*.hpp" | Copy-Item -Destination $InteropDir

# ui headers
$UiDir = Join-Path $RunInc "ui"
New-Item -ItemType Directory -Path $UiDir -Force | Out-Null
Get-ChildItem -Path (Join-Path $RepoRoot "aurora\include\runtime\ui") -Filter "*.h" | Copy-Item -Destination $UiDir

# compiler headers
$CompInc = Join-Path $IncludeDir "compiler"
New-Item -ItemType Directory -Path $CompInc -Force | Out-Null
Get-ChildItem -Path (Join-Path $RepoRoot "aurora\include\compiler") -Filter "*.hpp" | Copy-Item -Destination $CompInc

# ── Copy stb_image ──
Write-Host "  Copying deps..." -ForegroundColor Cyan
$DepsDir = Join-Path $Stage "deps"
New-Item -ItemType Directory -Path $DepsDir -Force | Out-Null
Copy-Item -Path (Join-Path $RepoRoot "deps\stb_image.h") -Destination $DepsDir

# ── Copy docs ──
Write-Host "  Copying docs..." -ForegroundColor Cyan
$DocsDir = Join-Path $Stage "docs"
New-Item -ItemType Directory -Path $DocsDir -Force | Out-Null
Get-ChildItem -Path (Join-Path $RepoRoot "aurora\docs") -Filter "*.md" | Copy-Item -Destination $DocsDir
Copy-Item -Path (Join-Path $RepoRoot "README.md") -Destination $Stage
Copy-Item -Path (Join-Path $RepoRoot "LICENSE") -Destination $Stage

# ── Create ZIP ──
Write-Host "  Creating ZIP..." -ForegroundColor Cyan
if (Test-Path $ZipPath) { Remove-Item -Path $ZipPath -Force }
Compress-Archive -Path "$Stage\*" -DestinationPath $ZipPath -CompressionLevel Optimal

# ── Cleanup ──
Remove-Item -Path $Stage -Recurse -Force

Write-Host ""
Write-Host "SUCCESS: $ZipPath" -ForegroundColor Green
Write-Host "  Size: $([math]::Round((Get-Item $ZipPath).Length / 1MB, 1)) MB"
