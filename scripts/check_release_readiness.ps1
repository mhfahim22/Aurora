#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Aurora Release Readiness Checker
.DESCRIPTION
    Validates build, tests, docs, and versions before tagging a release.
    Returns exit code 0 if all checks pass.
#>

$Root = Split-Path -Parent $PSScriptRoot
$VersionFile = "$Root\VERSION"
$Changelog   = "$Root\CHANGELOG.md"
$ReleaseMd   = "$Root\RELEASE.md"

$errors = @()
$warnings = @()

Write-Host "╔══════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║   Aurora Release Readiness Checker              ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# ── 1. Version file ──
Write-Host "Checking VERSION file..." -ForegroundColor Cyan
if (-not (Test-Path $VersionFile)) {
    $errors += "VERSION file not found at $VersionFile"
} else {
    $version = (Get-Content $VersionFile -Raw).Trim()
    Write-Host "  Version: $version" -ForegroundColor Green
    if ($version -match '-') {
        $warnings += "Version '$version' contains pre-release suffix — ensure this is intentional"
    }
}

# ── 2. CHANGELOG.md ──
Write-Host "Checking CHANGELOG.md..." -ForegroundColor Cyan
if (-not (Test-Path $Changelog)) {
    $errors += "CHANGELOG.md not found"
}
else {
    $content = Get-Content $Changelog -Raw
    if ($content -notmatch "## v$([regex]::Escape($version))") {
        $warnings += "CHANGELOG.md has no section for v$version"
    } else {
        Write-Host "  Section for v$version found" -ForegroundColor Green
    }
}

# ── 3. RELEASE.md ──
Write-Host "Checking RELEASE.md..." -ForegroundColor Cyan
if (-not (Test-Path $ReleaseMd)) {
    $errors += "RELEASE.md not found"
} else {
    Write-Host "  RELEASE.md exists" -ForegroundColor Green
}

# ── 4. Build artifacts ──
Write-Host "Checking build artifacts..." -ForegroundColor Cyan
$BuildDir = "$Root\build\Release"
if (-not (Test-Path $BuildDir)) {
    $BuildDir = "$Root\build\Debug"
}
if (-not (Test-Path $BuildDir)) {
    $warnings += "No build directory found (build/Release or build/Debug)"
} else {
    $required = @("aurorac.exe", "voss.exe", "aurora_lsp.exe")
    foreach ($bin in $required) {
        if (Test-Path "$BuildDir\$bin") {
            Write-Host "  OK $bin" -ForegroundColor Green
        } else {
            $warnings += "Missing build artifact: $bin"
        }
    }
}

# ── 5. CTest (build and run crossplatform test) ──
Write-Host "Running cross-platform validation tests..." -ForegroundColor Cyan
try {
    $result = & ctest -R test_crossplatform -C Debug --output-on-failure 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  test_crossplatform: PASS" -ForegroundColor Green
    } else {
        $warnings += "test_crossplatform failed (exit code: $LASTEXITCODE)"
    }
} catch {
    $warnings += "Could not run test_crossplatform: $_"
}

# ── 6. Standard library ──
Write-Host "Checking standard library..." -ForegroundColor Cyan
$LibcDir = "$Root\libc"
if (Test-Path $LibcDir) {
    $count = (Get-ChildItem "$LibcDir\*.auf").Count
    Write-Host "  $count .auf files in libc/" -ForegroundColor Green
    if ($count -eq 0) {
        $errors += "No .auf files in libc/"
    }
} else {
    $errors += "libc/ directory not found"
}

# ── 7. Version consistency ──
Write-Host "Checking version consistency..." -ForegroundColor Cyan
$VersionHpp = "$Root\aurora\include\common\aurora_version.hpp"
if (Test-Path $VersionHpp) {
    $hppContent = Get-Content $VersionHpp -Raw
    if ($hppContent -match 'AURORA_VERSION_STRING "([^"]+)"') {
        $hppVersion = $Matches[1]
        if ($hppVersion -eq $version) {
            Write-Host "  aurora_version.hpp: $hppVersion (matches)" -ForegroundColor Green
        } else {
            $errors += "Version mismatch: VERSION says '$version' but aurora_version.hpp says '$hppVersion'"
        }
    }
}

$SetupIss = "$Root\release\setup.iss"
if (Test-Path $SetupIss) {
    $issContent = Get-Content $SetupIss -Raw
    if ($issContent -match '#define MyAppVersion "([^"]+)"') {
        $issVersion = $Matches[1]
        if ($issVersion -eq $version) {
            Write-Host "  setup.iss: $issVersion (matches)" -ForegroundColor Green
        } else {
            $errors += "Version mismatch: VERSION says '$version' but setup.iss says '$issVersion'"
        }
    }
}

# ── 8. Docs ──
Write-Host "Checking documentation..." -ForegroundColor Cyan
$docs = @(
    "$Root\README.md",
    "$Root\LICENSE",
    "$Root\CONTRIBUTING.md",
    "$Root\SECURITY.md",
    "$Root\aurora\docs\language.md"
)
foreach ($doc in $docs) {
    if (Test-Path $doc) {
        Write-Host "  OK $(Split-Path $doc -Leaf)" -ForegroundColor Green
    } else {
        $warnings += "Missing document: $doc"
    }
}

# ── 9. Git status (warn if dirty) ──
Write-Host "Checking git status..." -ForegroundColor Cyan
try {
    $gitStatus = & git status --porcelain 2>&1
    if ($gitStatus) {
        $warnings += "Git working tree has uncommitted changes"
        Write-Host "  Uncommitted changes:" -ForegroundColor Yellow
        $gitStatus | ForEach-Object { Write-Host "    $_" -ForegroundColor Yellow }
    } else {
        Write-Host "  Working tree clean" -ForegroundColor Green
    }
} catch {
    $warnings += "git not available — skipping"
}

# ── Summary ──
Write-Host ""
Write-Host "═══════════════════════════════════════════════════" -ForegroundColor Cyan
if ($errors.Count -eq 0 -and $warnings.Count -eq 0) {
    Write-Host "  ✅ RELEASE READY — All checks passed" -ForegroundColor Green
    exit 0
}
if ($errors.Count -gt 0) {
    Write-Host "  ❌ ERRORS ($($errors.Count)):" -ForegroundColor Red
    foreach ($e in $errors) { Write-Host "    - $e" -ForegroundColor Red }
}
if ($warnings.Count -gt 0) {
    Write-Host "  ⚠️  WARNINGS ($($warnings.Count)):" -ForegroundColor Yellow
    foreach ($w in $warnings) { Write-Host "    - $w" -ForegroundColor Yellow }
}
Write-Host ""
if ($errors.Count -gt 0) {
    Write-Host "  Release blocked — fix errors first." -ForegroundColor Red
} else {
    Write-Host "  Release ready (with warnings)." -ForegroundColor Yellow
}
exit ($errors.Count -gt 0 ? 1 : 0)
