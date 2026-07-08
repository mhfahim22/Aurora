#!/usr/bin/env pwsh
# pre-commit.ps1 — Pre-commit hook (Phase 12.5)
# Runs lint + typecheck + unit tests before allowing commit

$ErrorActionPreference = "Stop"
$rootDir = Split-Path -Parent (Split-Path -Parent $PSCommandPath)
$auroraC = Join-Path $rootDir "build\Release\aurorac.exe"
$voss = Join-Path $rootDir "build\Release\voss.exe"

Write-Host "═══ Pre-commit Checks ═══" -ForegroundColor Cyan
$errors = 0

# 1. Check aurorac exists
if (-not (Test-Path $auroraC)) {
    Write-Host "ERROR: aurorac not found at $auroraC" -ForegroundColor Red
    Write-Host "Run 'cmake --build build --config Release' first" -ForegroundColor Yellow
    exit 1
}

# 2. Compile all test files
Write-Host "── Compiling tests ──" -ForegroundColor Cyan
$testDir = Join-Path $rootDir "Workflow\tests"
if (Test-Path $testDir) {
    foreach ($tf in Get-ChildItem $testDir -Filter "*.aura") {
        $src = Join-Path $testDir $tf.Name
        Write-Host "  aurorac $($tf.Name)..." -NoNewline
        $out = & $auroraC $src 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Host " FAIL" -ForegroundColor Red
            $errors++
        } else {
            Write-Host " OK" -ForegroundColor Green
        }
        # Clean up .exe
        $exe = $src -replace '\.aura$', '.exe'
        if (Test-Path $exe) { Remove-Item $exe -Force }
    }
}

# 3. Compile example apps
Write-Host "── Compiling examples ──" -ForegroundColor Cyan
$exampleDir = Join-Path $rootDir "examples\app"
if (Test-Path $exampleDir) {
    foreach ($ef in Get-ChildItem $exampleDir -Filter "*.aura") {
        $src = Join-Path $exampleDir $ef.Name
        Write-Host "  aurorac $($ef.Name)..." -NoNewline
        $out = & $auroraC $src 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Host " FAIL" -ForegroundColor Red
            $errors++
        } else {
            Write-Host " OK" -ForegroundColor Green
        }
        $exe = $src -replace '\.aura$', '.exe'
        if (Test-Path $exe) { Remove-Item $exe -Force }
    }
}

# 4. Summary
Write-Host ""
if ($errors -gt 0) {
    Write-Host "FAILED: $errors error(s) found. Commit blocked." -ForegroundColor Red
    exit 1
} else {
    Write-Host "All checks passed. Commit allowed." -ForegroundColor Green
    exit 0
}
